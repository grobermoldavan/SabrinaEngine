
#include "se_vulkan_render_subsystem_program.hpp"
#include "se_vulkan_render_subsystem_base.hpp"
#include "se_vulkan_render_subsystem_device.hpp"
#include "se_vulkan_render_subsystem_memory.hpp"
#include "se_vulkan_render_subsystem_utils.hpp"

size_t g_programIndex = 0;

void* se_vk_ssr_alloc_persistent(void* userData, size_t size)
{
    AllocatorBindings allocator = app_allocators::persistent();
    return allocator.alloc(allocator.allocator, size, se_default_alignment, se_alloc_tag);
}

void se_vk_ssr_free_persistent(void* userData, void* ptr, size_t size)
{
    AllocatorBindings allocator = app_allocators::persistent();
    allocator.dealloc(allocator.allocator, ptr, size);
}

void* se_vk_ssr_alloc_frame(void* userData, size_t size)
{
    AllocatorBindings allocator = app_allocators::frame();
    return allocator.alloc(allocator.allocator, size, se_default_alignment, se_alloc_tag);
}

void se_vk_ssr_free_frame(void* userData, void* ptr, size_t size)
{
    AllocatorBindings allocator = app_allocators::frame();
    allocator.dealloc(allocator.allocator, ptr, size);
}

void se_vk_program_construct(SeVkProgram* program, SeVkProgramInfo* info)
{
    SeVkDevice* device = info->device;
    SeVkMemoryManager* memoryManager = &device->memoryManager;
    VkAllocationCallbacks* callbacks = se_vk_memory_manager_get_callbacks(memoryManager);
    VkDevice logicalHandle = se_vk_device_get_logical_handle(info->device);
    
    auto [bytecode, bytecodeSize] = data_provider::get(info->data);
    *program =
    {
        .object     = { SE_VK_TYPE_PROGRAM, g_programIndex++ },
        .device     = info->device,
        .handle     = se_vk_utils_create_shader_module(logicalHandle, (const uint32_t*)bytecode, bytecodeSize, callbacks),
        .reflection = { },
    };
    SsrAllocator ssrPersistentAllocator
    {
        .userData   = nullptr,
        .alloc      = se_vk_ssr_alloc_persistent,
        .free       = se_vk_ssr_free_persistent,
    };
    SsrAllocator ssrFrameAllocator
    {
        .userData   = nullptr,
        .alloc      = se_vk_ssr_alloc_frame,
        .free       = se_vk_ssr_free_frame,
    };
    SsrCreateInfo reflectionCreateInfo
    {
        .persistentAllocator    = &ssrPersistentAllocator,
        .nonPersistentAllocator = &ssrFrameAllocator,
        .bytecode               = (SsrSpirvWord*)bytecode,
        .bytecodeNumWords       = bytecodeSize / 4,
    };
    ssr_construct(&program->reflection, &reflectionCreateInfo);
}

void se_vk_program_destroy(SeVkProgram* program)
{
    SeVkMemoryManager* memoryManager = &program->device->memoryManager;
    VkAllocationCallbacks* callbacks = se_vk_memory_manager_get_callbacks(memoryManager);
    VkDevice logicalHandle = se_vk_device_get_logical_handle(program->device);
    
    ssr_destroy(&program->reflection);
    se_vk_utils_destroy_shader_module(logicalHandle, program->handle, callbacks);
}

VkPipelineShaderStageCreateInfo se_vk_program_get_shader_stage_create_info(SeVkDevice* device, SeVkProgramWithConstants* pipelineProgram, AllocatorBindings& allocator)
{
    const SeVkProgram* program = pipelineProgram->program;
    const SimpleSpirvReflection* reflection = &program->reflection;
    const VkShaderStageFlagBits stage =
        reflection->shaderType == SSR_SHADER_TYPE_VERTEX    ? (VkShaderStageFlagBits)VK_SHADER_STAGE_VERTEX_BIT :
        reflection->shaderType == SSR_SHADER_TYPE_FRAGMENT  ? (VkShaderStageFlagBits)VK_SHADER_STAGE_FRAGMENT_BIT :
        reflection->shaderType == SSR_SHADER_TYPE_COMPUTE   ? (VkShaderStageFlagBits)VK_SHADER_STAGE_COMPUTE_BIT :
        (VkShaderStageFlagBits)0;
    
    VkSpecializationMapEntry* specializationEntries = (VkSpecializationMapEntry*)allocator.alloc
    (
        allocator.allocator,
        sizeof(VkSpecializationMapEntry) * pipelineProgram->numSpecializationConstants,
        se_default_alignment,
        se_alloc_tag
    );
    char* data = (char*)allocator.alloc
    (
        allocator.allocator,
        4 * pipelineProgram->numSpecializationConstants,
        se_default_alignment,
        se_alloc_tag
    );
    VkSpecializationInfo* specializationInfo = (VkSpecializationInfo*)allocator.alloc
    (
        allocator.allocator,
        sizeof(VkSpecializationInfo),
        se_default_alignment,
        se_alloc_tag
    );
    *specializationInfo =
    {
        .mapEntryCount  = (uint32_t)pipelineProgram->numSpecializationConstants, // @TODO : safe cast
        .pMapEntries    = specializationEntries,
        .dataSize       = (uint32_t)pipelineProgram->numSpecializationConstants * 4, // @TODO : safe cast
        .pData          = data,
    };
    for (uint32_t it = 0; it < pipelineProgram->numSpecializationConstants; it++)
    {
        const SeSpecializationConstant* constantDesc = &pipelineProgram->constants[it];
        SsrSpecializationConstant* reflectionConstant = nullptr;
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
        specializationEntries[it] =
        {
            .constantID = constantDesc->constantId,
            .offset     = it * 4,
            .size       = ssr_is_bool(reflectionConstant->type) ? 1u : 4u,
        };
        memcpy(&data[it * 4], &constantDesc->asInt, 4);
    }
    
    return
    {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext                  = nullptr,
        .flags                  = 0,
        .stage                  = stage,
        .module                 = program->handle,
        .pName                  = program->reflection.entryPointName,
        .pSpecializationInfo    = specializationInfo,
    };
}
