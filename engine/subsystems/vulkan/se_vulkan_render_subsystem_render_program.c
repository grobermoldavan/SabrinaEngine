
#include "se_vulkan_render_subsystem_render_program.h"
#include "se_vulkan_render_subsystem_base.h"
#include "se_vulkan_render_subsystem_device.h"
#include "se_vulkan_render_subsystem_memory.h"
#include "se_vulkan_render_subsystem_utils.h"
#include "se_vulkan_render_subsystem_in_flight_manager.h"
#include "engine/allocator_bindings.h"
#include "engine/render_abstraction_interface.h"
#include "engine/libs/ssr/simple_spirv_reflection.h"

typedef struct
{
    SeRenderObject object;
    SeRenderObject* device;
    VkShaderModule handle;
    SimpleSpirvReflection reflection;
} SeVkRenderProgram;

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
    SeAllocatorBindings* allocator = memoryManager->cpu_persistentAllocator;
    VkDevice logicalHandle = se_vk_device_get_logical_handle(createInfo->device);
    //
    //
    //
    SeVkRenderProgram* program = allocator->alloc(allocator->allocator, sizeof(SeVkRenderProgram), se_default_alignment, se_alloc_tag);
    program->object.handleType = SE_RENDER_HANDLE_TYPE_PROGRAM;
    program->object.destroy = se_vk_render_program_submit_for_deffered_destruction;
    program->device = createInfo->device;
    program->handle = se_vk_utils_create_shader_module(logicalHandle, createInfo->bytecode, createInfo->codeSizeBytes, callbacks);
    SsrAllocator ssrAllocator = (SsrAllocator)
    {
        .userData   = allocator,
        .alloc      = se_vk_ssr_alloc,
        .free       = se_vk_ssr_free,
    };
    SsrCreateInfo reflectionCreateInfo = (SsrCreateInfo)
    {
        .persistentAllocator    = &ssrAllocator,
        .nonPersistentAllocator = &ssrAllocator, // @TODO : use frame allocator
        .bytecode               = (SsrSpirvWord*)createInfo->bytecode,
        .bytecodeNumWords       = createInfo->codeSizeBytes / 4,
    };
    ssr_construct(&program->reflection, &reflectionCreateInfo);
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
    SeVkRenderProgram* program = (SeVkRenderProgram*)_program;
    SeVkMemoryManager* memoryManager = se_vk_device_get_memory_manager(program->device);
    VkAllocationCallbacks* callbacks = se_vk_memory_manager_get_callbacks(memoryManager);
    VkDevice logicalHandle = se_vk_device_get_logical_handle(program->device);
    ssr_destroy(&program->reflection);
    se_vk_utils_destroy_shader_module(logicalHandle, program->handle, callbacks);
    memoryManager->cpu_persistentAllocator->dealloc(memoryManager->cpu_persistentAllocator->allocator, program, sizeof(SeVkRenderProgram));
}

SimpleSpirvReflection* se_vk_render_program_get_reflection(SeRenderObject* _program)
{
    se_vk_expect_handle(_program, SE_RENDER_HANDLE_TYPE_PROGRAM, "Can't get program reflection");
    SeVkRenderProgram* program = (SeVkRenderProgram*)_program;
    return &program->reflection;
}

VkPipelineShaderStageCreateInfo se_vk_render_program_get_shader_stage_create_info(SeRenderObject* _program)
{
    se_vk_expect_handle(_program, SE_RENDER_HANDLE_TYPE_PROGRAM, "Can't get program shader stage create info");
    SeVkRenderProgram* program = (SeVkRenderProgram*)_program;
    const VkShaderStageFlagBits stage =
        program->reflection.type == SSR_SHADER_VERTEX ? VK_SHADER_STAGE_VERTEX_BIT :
        program->reflection.type == SSR_SHADER_FRAGMENT ? VK_SHADER_STAGE_FRAGMENT_BIT :
        0;
    return (VkPipelineShaderStageCreateInfo)
    {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext                  = NULL,
        .flags                  = 0,
        .stage                  = stage,
        .module                 = program->handle,
        .pName                  = program->reflection.entryPointName,
        .pSpecializationInfo    = NULL,
    };
}
