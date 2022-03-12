#ifndef _SE_VULKAN_RENDER_SUBSYSTEM_RENDER_PASS_H_
#define _SE_VULKAN_RENDER_SUBSYSTEM_RENDER_PASS_H_

#include "se_vulkan_render_subsystem_base.h"

#define se_vk_render_pass_num_subpasses(passPtr) ((passPtr)->info.numSubpasses)
#define se_vk_render_pass_num_attachments(passPtr) ((passPtr)->info.numColorAttachments + ((passPtr)->info.hasDepthStencilAttachment ? 1 : 0))

typedef struct SeVkRenderPassAttachment
{
    VkFormat                format;
    VkAttachmentLoadOp      loadOp;
    VkAttachmentStoreOp     storeOp;
    VkSampleCountFlagBits   sampling;
    VkClearValue            clearValue;
} SeVkRenderPassAttachment;

typedef struct SeVkResolveRef
{
    uint32_t from;
    uint32_t to;
} SeVkResolveRef;

typedef struct SeVkRenderPassSubpass
{
    SeVkGeneralBitmask  colorRefs;                                  // each value references colorAttachments array
    SeVkGeneralBitmask  inputRefs;                                  // each value references colorAttachments array
    SeVkResolveRef      resolveRefs[SE_VK_GENERAL_BITMASK_WIDTH];   // each value references colorAttachments array
    uint32_t            numResolveRefs;
    bool                depthRead;
    bool                depthWrite;
} SeVkRenderPassSubpass;

typedef struct SeVkRenderPassInfo
{
    struct SeVkDevice*          device;
    SeVkRenderPassSubpass       subpasses[SE_VK_GENERAL_BITMASK_WIDTH];
    uint32_t                    numSubpasses;
    SeVkRenderPassAttachment    colorAttachments[SE_VK_GENERAL_BITMASK_WIDTH];
    uint32_t                    numColorAttachments;
    SeVkRenderPassAttachment    depthStencilAttachment;
    bool                        hasDepthStencilAttachment;
} SeVkRenderPassInfo;

typedef struct SeVkRenderPassAttachmentLayoutInfo
{
    VkImageLayout initialLayout;
    VkImageLayout finalLayout;
} SeVkRenderPassAttachmentLayoutInfo;

typedef struct SeVkRenderPass
{
    SeVkObject                          object;
    SeVkRenderPassInfo                  info;
    VkRenderPass                        handle;
    SeVkRenderPassAttachmentLayoutInfo  attachmentLayoutInfos[SE_VK_GENERAL_BITMASK_WIDTH];
    VkClearValue                        clearValues[SE_VK_GENERAL_BITMASK_WIDTH];
} SeVkRenderPass;

void se_vk_render_pass_construct(SeVkRenderPass* pass, SeVkRenderPassInfo* info);
void se_vk_render_pass_destroy(SeVkRenderPass* pass);

void                    se_vk_render_pass_validate_fragment_program_setup(SeVkRenderPass* pass, struct SeVkProgram* program, size_t subpassIndex);
uint32_t                se_vk_render_pass_get_num_color_attachments(SeVkRenderPass* pass, size_t subpassIndex);
void                    se_vk_render_pass_validate_framebuffer_textures(SeVkRenderPass* pass, struct SeVkTexture** textures, size_t numTextures);
VkRenderPassBeginInfo   se_vk_render_pass_get_begin_info(SeVkRenderPass* pass, VkFramebuffer framebuffer, VkRect2D renderArea);

#define se_vk_render_pass_get_hash_input(passPtr) ((SeHashInput) { passPtr, sizeof(SeVkObject) })

#endif
