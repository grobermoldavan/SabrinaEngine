#ifndef _SE_VULKAN_RENDER_SUBSYSTEM_RENDER_PROGRAM_H_
#define _SE_VULKAN_RENDER_SUBSYSTEM_RENDER_PROGRAM_H_

#include "se_vulkan_render_subsystem_base.h"
#include "engine/libs/ssr/simple_spirv_reflection.h"

typedef struct SeVkProgram
{
    SeVkObject              object;
    struct SeVkDevice*      device;
    VkShaderModule          handle;
    SimpleSpirvReflection   reflection;
} SeVkProgram;

typedef struct SeVkProgramInfo
{
    struct SeVkDevice*  device;
    const uint32_t*     bytecode;
    size_t              bytecodeSize;
} SeVkProgramInfo;

typedef struct SeVkProgramWithConstants
{
    SeVkProgram*                program;
    SeSpecializationConstant    constants[SE_MAX_SPECIALIZATION_CONSTANTS];
    size_t                      numSpecializationConstants;
} SeVkProgramWithConstants;

void se_vk_program_construct(SeVkProgram* program, SeVkProgramInfo* info);
void se_vk_program_destroy(SeVkProgram* program);

VkPipelineShaderStageCreateInfo se_vk_program_get_shader_stage_create_info(struct SeVkDevice* device, SeVkProgramWithConstants* pipelineProgram, SeAllocatorBindings* allocator);

#define se_vk_program_get_hash_input(programPtr) ((SeHashInput) { programPtr, sizeof(SeVkObject) })

#endif
