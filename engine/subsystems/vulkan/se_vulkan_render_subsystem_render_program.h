#ifndef _SE_VULKAN_RENDER_SUBSYSTEM_RENDER_PROGRAM_H_
#define _SE_VULKAN_RENDER_SUBSYSTEM_RENDER_PROGRAM_H_

#include "se_vulkan_render_subsystem_base.h"

struct SeRenderObject*  se_vk_render_program_create(struct SeRenderProgramCreateInfo* createInfo);
void                    se_vk_render_program_submit_for_deffered_destruction(struct SeRenderObject* program);
void                    se_vk_render_program_destroy(struct SeRenderObject* program);

struct SimpleSpirvReflection*   se_vk_render_program_get_reflection(struct SeRenderObject* program);
VkPipelineShaderStageCreateInfo se_vk_render_program_get_shader_stage_create_info(struct SeRenderObject* program);

#endif
