
#include "se_vulkan_render_subsystem_render_program.h"
#include "se_vulkan_render_subsystem_base.h"
#include "se_vulkan_render_subsystem_device.h"
#include "se_vulkan_render_subsystem_memory.h"
#include "se_vulkan_render_subsystem_utils.h"
#include "se_vulkan_render_subsystem_in_flight_manager.h"
#include "engine/allocator_bindings.h"

static void* se_vk_ssr_alloc(void* userData, size_t size)
{
    SeAllocatorBindings* allocator = (SeAllocatorBindings*)userData;
    return allocator->alloc(allocator->allocator, size, se_default_alignment, se_alloc_tag);
}

static void se_vk_ssr_free(void* userData, void* ptr, size_t size)
{
    SeAllocatorBindings* allocator = (SeAllocatorBindings*)userData;
    allocator->dealloc(allocator->allocator, ptr, size);
}

SeRenderObject* se_vk_render_program_create(SeRenderProgramCreateInfo* createInfo)
{
    se_vk_expect_handle(createInfo->device, SE_RENDER_HANDLE_TYPE_DEVICE, "Can't create program");
    SeVkMemoryManager* memoryManager = se_vk_device_get_memory_manager(createInfo->device);
    VkAllocationCallbacks* callbacks = se_vk_memory_manager_get_callbacks(memoryManager);
    VkDevice logicalHandle = se_vk_device_get_logical_handle(createInfo->device);
    //
    // Create program and reflection
    //
    SeVkRenderProgram* program = se_object_pool_take(SeVkRenderProgram, se_vk_memory_manager_get_pool(memoryManager, SE_RENDER_HANDLE_TYPE_PROGRAM));
    *program = (SeVkRenderProgram)
    {
        .object     = se_vk_render_object(SE_RENDER_HANDLE_TYPE_PROGRAM, se_vk_render_program_submit_for_deffered_destruction),
        .device     = createInfo->device,
        .handle     = se_vk_utils_create_shader_module(logicalHandle, createInfo->bytecode, createInfo->codeSizeBytes, callbacks),
        .reflection = {0},
    };
    SsrAllocator ssrPersistentAllocator = (SsrAllocator)
    {
        .userData   = memoryManager->cpu_persistentAllocator,
        .alloc      = se_vk_ssr_alloc,
        .free       = se_vk_ssr_free,
    };
    SsrAllocator ssrFrameAllocator = (SsrAllocator)
    {
        .userData   = memoryManager->cpu_frameAllocator,
        .alloc      = se_vk_ssr_alloc,
        .free       = se_vk_ssr_free,
    };
    SsrCreateInfo reflectionCreateInfo = (SsrCreateInfo)
    {
        .persistentAllocator    = &ssrPersistentAllocator,
        .nonPersistentAllocator = &ssrFrameAllocator,
        .bytecode               = (SsrSpirvWord*)createInfo->bytecode,
        .bytecodeNumWords       = createInfo->codeSizeBytes / 4,
    };
    ssr_construct(&program->reflection, &reflectionCreateInfo);
    //
    // Add external dependencies
    //
    se_vk_add_external_resource_dependency(program->device);
    //
    //
    //
    return (SeRenderObject*)program;
}

void se_vk_render_program_submit_for_deffered_destruction(SeRenderObject* _program)
{
    se_vk_expect_handle(_program, SE_RENDER_HANDLE_TYPE_PROGRAM, "Can't submit program for deffered destruction");
    SeVkInFlightManager* inFlightManager = se_vk_device_get_in_flight_manager(((SeVkRenderProgram*)_program)->device);
    se_vk_in_flight_manager_submit_deffered_destruction(inFlightManager, (SeVkDefferedDestruction) { _program, se_vk_render_program_destroy });
}

void se_vk_render_program_destroy(SeRenderObject* _program)
{
    se_vk_expect_handle(_program, SE_RENDER_HANDLE_TYPE_PROGRAM, "Can't destroy program");
    se_vk_check_external_resource_dependencies(_program);
    SeVkRenderProgram* program = (SeVkRenderProgram*)_program;
    SeVkMemoryManager* memoryManager = se_vk_device_get_memory_manager(program->device);
    VkAllocationCallbacks* callbacks = se_vk_memory_manager_get_callbacks(memoryManager);
    VkDevice logicalHandle = se_vk_device_get_logical_handle(program->device);
    //
    // Destroy reflection and vk handles
    //
    ssr_destroy(&program->reflection);
    se_vk_utils_destroy_shader_module(logicalHandle, program->handle, callbacks);
    //
    // Remove external dependencies
    //
    se_vk_remove_external_resource_dependency(program->device);
    //
    //
    //
    se_object_pool_return(SeVkRenderProgram, se_vk_memory_manager_get_pool(memoryManager, SE_RENDER_HANDLE_TYPE_PROGRAM), program);
}

SeRenderProgramComputeWorkGroupSize se_vk_render_program_get_compute_work_group_size(SeRenderObject* _program)
{
    se_vk_expect_handle(_program, SE_RENDER_HANDLE_TYPE_PROGRAM, "Can't get program compute workgroup size");
    SeVkRenderProgram* program = (SeVkRenderProgram*)_program;
    se_assert(program->reflection.shaderType == SSR_SHADER_TYPE_COMPUTE);
    return (SeRenderProgramComputeWorkGroupSize)
    {
        .x = program->reflection.computeWorkGroupSizeX,
        .y = program->reflection.computeWorkGroupSizeY,
        .z = program->reflection.computeWorkGroupSizeZ,
    };
}

SimpleSpirvReflection* se_vk_render_program_get_reflection(SeRenderObject* _program)
{
    se_vk_expect_handle(_program, SE_RENDER_HANDLE_TYPE_PROGRAM, "Can't get program reflection");
    SeVkRenderProgram* program = (SeVkRenderProgram*)_program;
    return &program->reflection;
}

VkPipelineShaderStageCreateInfo se_vk_render_program_get_shader_stage_create_info(SeProgramWithConstants* pipelineProgram, SeAllocatorBindings* allocator)
{
    se_vk_expect_handle(pipelineProgram->program, SE_RENDER_HANDLE_TYPE_PROGRAM, "Can't get program shader stage create info");
    const SeVkRenderProgram* program = (SeVkRenderProgram*)pipelineProgram->program;
    const SimpleSpirvReflection* reflection = &program->reflection;
    const VkShaderStageFlagBits stage =
        reflection->shaderType == SSR_SHADER_TYPE_VERTEX ? VK_SHADER_STAGE_VERTEX_BIT :
        reflection->shaderType == SSR_SHADER_TYPE_FRAGMENT ? VK_SHADER_STAGE_FRAGMENT_BIT :
        reflection->shaderType == SSR_SHADER_TYPE_COMPUTE ? VK_SHADER_STAGE_COMPUTE_BIT :
        0;
    //
    // Fill specialization constants info
    //
    VkSpecializationMapEntry* specializationEntries = (VkSpecializationMapEntry*)allocator->alloc
    (
        allocator->allocator,
        sizeof(VkSpecializationMapEntry) * pipelineProgram->numSpecializationConstants,
        se_default_alignment,
        se_alloc_tag
    );
    char* data = (char*)allocator->alloc
    (
        allocator->allocator,
        4 * pipelineProgram->numSpecializationConstants,
        se_default_alignment,
        se_alloc_tag
    );
    VkSpecializationInfo* specializationInfo = (VkSpecializationInfo*)allocator->alloc
    (
        allocator->allocator,
        sizeof(VkSpecializationInfo),
        se_default_alignment,
        se_alloc_tag
    );
    *specializationInfo = (VkSpecializationInfo)
    {
        .mapEntryCount  = (uint32_t)pipelineProgram->numSpecializationConstants, // @TODO : safe cast
        .pMapEntries    = specializationEntries,
        .dataSize       = (uint32_t)pipelineProgram->numSpecializationConstants * 4, // @TODO : safe cast
        .pData          = data,
    };
    for (uint32_t it = 0; it < pipelineProgram->numSpecializationConstants; it++)
    {
        const SeSpecializationConstant* constantDesc = &pipelineProgram->specializationConstants[it];
        SsrSpecializationConstant* reflectionConstant = NULL;
        for (size_t scIt = 0; scIt < reflection->numSpecializationConstants; scIt++)
        {
            SsrSpecializationConstant* candidate = &reflection->specializationConstants[scIt];
            if (candidate->constantId == constantDesc->constantId)
            {
                reflectionConstant = candidate;
                break;
            }
        }
        se_assert(reflectionConstant);
        specializationEntries[it] = (VkSpecializationMapEntry)
        {
            .constantID = constantDesc->constantId,
            .offset     = it * 4,
            .size       = ssr_is_bool(reflectionConstant->type) ? 1 : 4,
        };
        memcpy(&data[it * 4], &constantDesc->asInt, 4);
    }
    //
    //
    //
    return (VkPipelineShaderStageCreateInfo)
    {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext                  = NULL,
        .flags                  = 0,
        .stage                  = stage,
        .module                 = program->handle,
        .pName                  = program->reflection.entryPointName,
        .pSpecializationInfo    = specializationInfo,
    };
}
