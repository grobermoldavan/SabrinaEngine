#ifndef _SE_VULKAN_RENDER_SUBSYSTEM_SAMPLER_H_
#define _SE_VULKAN_RENDER_SUBSYSTEM_SAMPLER_H_

#include "se_vulkan_render_subsystem_base.h"

typedef struct SeVkSampler
{
    SeVkRenderObject object;
    SeRenderObject* device;
    VkSampler handle;
} SeVkSampler;

SeRenderObject* se_vk_sampler_create(SeSamplerCreateInfo* createInfo);
void se_vk_sampler_submit_for_deffered_destruction(SeRenderObject* sampler);
void se_vk_sampler_destroy(SeRenderObject* sampler);

VkSampler se_vk_sampler_get_handle(SeRenderObject* sampler);

#endif