#ifndef _SE_VULKAN_RENDER_SUBSYSTEM_RENDER_PROGRAM_H_
#define _SE_VULKAN_RENDER_SUBSYSTEM_RENDER_PROGRAM_H_

#include "se_vulkan_render_subsystem_base.h"
#include "engine/libs/ssr/simple_spirv_reflection.h"

typedef struct SeVkRenderProgram
{
    SeVkRenderObject        object;
    SeRenderObject*         device;
    VkShaderModule          handle;
    SimpleSpirvReflection   reflection;
} SeVkRenderProgram;

SeRenderObject*                     se_vk_render_program_create(SeRenderProgramCreateInfo* createInfo);
void                                se_vk_render_program_submit_for_deffered_destruction(SeRenderObject* program);
void                                se_vk_render_program_destroy(SeRenderObject* program);
SeRenderProgramComputeWorkGroupSize se_vk_render_program_get_compute_work_group_size(SeRenderObject* program);

SimpleSpirvReflection*          se_vk_render_program_get_reflection(SeRenderObject* program);
VkPipelineShaderStageCreateInfo se_vk_render_program_get_shader_stage_create_info(SeProgramWithConstants* pipelineProgram, SeAllocatorBindings* allocator);

#endif
