#ifndef _SE_VULKAN_RENDER_PASS_H_
#define _SE_VULKAN_RENDER_PASS_H_

#include "se_vulkan_base.hpp"

#define se_vk_render_pass_num_subpasses(passPtr) ((passPtr)->info.numSubpasses)
#define se_vk_render_pass_num_attachments(passPtr) ((passPtr)->info.numColorAttachments + ((passPtr)->info.hasDepthStencilAttachment ? 1 : 0))

struct SeVkRenderPassAttachment
{
    VkFormat                format;
    VkAttachmentLoadOp      loadOp;
    VkAttachmentStoreOp     storeOp;
    VkSampleCountFlagBits   sampling;
    VkClearValue            clearValue;
};

struct SeVkResolveRef
{
    uint32_t from;
    uint32_t to;
};

struct SeVkRenderPassSubpass
{
    SeVkGeneralBitmask  colorRefs;                                  // each value references colorAttachments array
    SeVkGeneralBitmask  inputRefs;                                  // each value references colorAttachments array
    SeVkResolveRef      resolveRefs[SE_VK_GENERAL_BITMASK_WIDTH];   // each value references colorAttachments array
    uint32_t            numResolveRefs;
    bool                depthRead;
    bool                depthWrite;
};

struct SeVkRenderPassInfo
{
    SeVkDevice*                 device;
    SeVkRenderPassSubpass       subpasses[SE_VK_GENERAL_BITMASK_WIDTH];
    uint32_t                    numSubpasses;
    SeVkRenderPassAttachment    colorAttachments[SE_VK_GENERAL_BITMASK_WIDTH];
    uint32_t                    numColorAttachments;
    SeVkRenderPassAttachment    depthStencilAttachment;
    bool                        hasDepthStencilAttachment;
};

struct SeVkRenderPassAttachmentLayoutInfo
{
    VkImageLayout initialLayout;
    VkImageLayout finalLayout;
};

struct SeVkRenderPass
{
    SeVkObject                          object;
    SeVkRenderPassInfo                  info;
    VkRenderPass                        handle;
    SeVkRenderPassAttachmentLayoutInfo  attachmentLayoutInfos[SE_VK_GENERAL_BITMASK_WIDTH];
    VkClearValue                        clearValues[SE_VK_GENERAL_BITMASK_WIDTH];
};

void se_vk_render_pass_construct(SeVkRenderPass* pass, SeVkRenderPassInfo* info);
void se_vk_render_pass_destroy(SeVkRenderPass* pass);

void                    se_vk_render_pass_validate_fragment_program_setup(SeVkRenderPass* pass, SeVkProgram* program, size_t subpassIndex);
uint32_t                se_vk_render_pass_get_num_color_attachments(SeVkRenderPass* pass, size_t subpassIndex);
void                    se_vk_render_pass_validate_framebuffer_textures(SeVkRenderPass* pass, SeVkTexture** textures, size_t numTextures);
VkRenderPassBeginInfo   se_vk_render_pass_get_begin_info(SeVkRenderPass* pass, VkFramebuffer framebuffer, VkRect2D renderArea);

#define se_vk_render_pass_get_hash_input(passPtr) (SeHashInput{ passPtr, sizeof(SeVkObject) })

template<>
void se_vk_destroy<SeVkRenderPass>(SeVkRenderPass* res)
{
    se_vk_render_pass_destroy(res);
}

template<>
void se_hash_value_builder_absorb<SeVkRenderPassInfo>(SeHashValueBuilder& builder, const SeVkRenderPassInfo& value)
{
    se_hash_value_builder_absorb_raw(builder, { (void*)&value, sizeof(value) });
}

template<>
void se_hash_value_builder_absorb<SeVkRenderPass>(SeHashValueBuilder& builder, const SeVkRenderPass& value)
{
    se_hash_value_builder_absorb_raw(builder, { (void*)&value.object, sizeof(value.object) });
}

template<>
SeHashValue se_hash_value_generate<SeVkRenderPassInfo>(const SeVkRenderPassInfo& value)
{
    return se_hash_value_generate_raw({ (void*)&value, sizeof(value) });
}

template<>
SeHashValue se_hash_value_generate<SeVkRenderPass>(const SeVkRenderPass& value)
{
    return se_hash_value_generate_raw({ (void*)&value.object, sizeof(value.object) });
}

#endif
