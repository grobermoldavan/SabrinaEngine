#ifndef _SE_VULKAN_RENDER_SUBSYSTEM_RENDER_PASS_H_
#define _SE_VULKAN_RENDER_SUBSYSTEM_RENDER_PASS_H_

#include "se_vulkan_render_subsystem_base.h"

typedef struct SeVkSubpassInfo
{
    uint32_t inputAttachmentRefsBitmask;
    uint32_t colorAttachmentRefsBitmask;
} SeVkSubpassInfo;

typedef struct SeVkRenderPassAttachmentInfo
{
    VkImageLayout initialLayout;
    VkImageLayout finalLayout;
    VkFormat format;
} SeVkRenderPassAttachmentInfo;

typedef struct SeVkRenderPass
{
    //
    // Constant info (filled once at creation)
    //
    SeVkRenderObject                object;
    SeRenderPassCreateInfo          createInfo;
    //
    // Non-constant info (filled every time at re-creation)
    //
    VkRenderPass                    handle;
    SeVkSubpassInfo                 subpassInfos[SE_VK_GENERAL_BITMASK_WIDTH];
    SeVkRenderPassAttachmentInfo    attachmentInfos[SE_VK_GENERAL_BITMASK_WIDTH];
    VkClearValue                    clearValues[SE_VK_GENERAL_BITMASK_WIDTH];
    size_t                          numSubpasses;
    size_t                          numAttachments;
    uint64_t                        flags;
} SeVkRenderPass;

#define __SE_VK_RENDER_PASS_RECREATE_ZEROING_SIZE (sizeof(SeVkRenderPass) - offsetof(SeVkRenderPass, handle))
#define __SE_VK_RENDER_PASS_RECREATE_ZEROING_OFFSET(passPtr) &(passPtr)->handle

SeRenderObject*         se_vk_render_pass_create(SeRenderPassCreateInfo* createInfo);
void                    se_vk_render_pass_submit_for_deffered_destruction(SeRenderObject* pass);
void                    se_vk_render_pass_destroy(SeRenderObject* pass);
void                    se_vk_render_pass_destroy_inplace(SeRenderObject* pass);
void                    se_vk_render_pass_recreate_inplace(SeRenderObject* pass);

size_t                  se_vk_render_pass_get_num_subpasses(SeRenderObject* pass);
void                    se_vk_render_pass_validate_fragment_program_setup(SeRenderObject* pass, SeRenderObject* fragmentShader, size_t subpassIndex);
uint32_t                se_vk_render_pass_get_num_color_attachments(SeRenderObject* pass, size_t subpassIndex);
VkRenderPass            se_vk_render_pass_get_handle(SeRenderObject* pass);
void                    se_vk_render_pass_validate_framebuffer_textures(SeRenderObject* pass, SeRenderObject** textures, size_t numTextures);
VkRenderPassBeginInfo   se_vk_render_pass_get_begin_info(SeRenderObject* pass, SeRenderObject* framebuffer, VkRect2D renderArea);
VkImageLayout           se_vk_render_pass_get_initial_attachment_layout(SeRenderObject* pass, size_t attachmentIndex);
bool                    se_vk_render_pass_is_dependent_on_swap_chain(SeRenderObject* pass);

#endif
