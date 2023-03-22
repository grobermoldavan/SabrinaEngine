#ifndef _SE_VULKAN_FRAMEBUFFER_H_
#define _SE_VULKAN_FRAMEBUFFER_H_

#include "se_vulkan_base.hpp"
#include "se_vulkan_render_pass.hpp"
#include "se_vulkan_texture.hpp"

struct SeVkFramebufferInfo
{
    SeVkDevice*                         device;
    SeObjectPoolEntryRef<SeVkRenderPass>  pass;
    SeObjectPoolEntryRef<SeVkTexture>     textures[SeVkConfig::FRAMEBUFFER_MAX_TEXTURES];
    uint32_t                            numTextures;
};

struct SeVkFramebuffer
{
    SeVkObject                          object;
    SeVkDevice*                         device;
    SeObjectPoolEntryRef<SeVkRenderPass>  pass;
    SeObjectPoolEntryRef<SeVkTexture>     textures[SeVkConfig::FRAMEBUFFER_MAX_TEXTURES];
    uint32_t                            numTextures;
    VkFramebuffer                       handle;
    VkExtent2D                          extent;
};

void se_vk_framebuffer_construct(SeVkFramebuffer* framebuffer, SeVkFramebufferInfo* info);
void se_vk_framebuffer_destroy(SeVkFramebuffer* framebuffer);

template<>
void se_vk_destroy<SeVkFramebuffer>(SeVkFramebuffer* res)
{
    se_vk_framebuffer_destroy(res);
}

template<>
void se_hash_value_builder_absorb<SeVkFramebufferInfo>(SeHashValueBuilder& builder, const SeVkFramebufferInfo& value)
{
    se_hash_value_builder_absorb(builder, value.pass);
    se_hash_value_builder_absorb(builder, value.numTextures);
    for (size_t it = 0; it < value.numTextures; it++) se_hash_value_builder_absorb(builder, value.textures[it]);
}

template<>
SeHashValue se_hash_value_generate<SeVkFramebufferInfo>(const SeVkFramebufferInfo& value)
{
    SeHashValueBuilder builder = se_hash_value_builder_begin();
    se_hash_value_builder_absorb(builder, value);
    return se_hash_value_builder_end(builder);
}

template<>
SeString se_string_cast<SeVkFramebufferInfo>(const SeVkFramebufferInfo& value, SeStringLifetime lifetime)
{
    SeStringBuilder builder = se_string_builder_begin("[Framebuffer info. ", lifetime);
    se_string_builder_append(builder, "Pass id: [");
    se_string_builder_append(builder, se_string_cast(value.pass.generation));
    se_string_builder_append(builder, ", ");
    se_string_builder_append(builder, se_string_cast(value.pass.index));
    se_string_builder_append(builder, "]. Textures: [num: ");
    se_string_builder_append(builder, se_string_cast(value.numTextures));
    se_string_builder_append(builder, ", ");
    for (size_t it = 0; it < value.numTextures; it++)
    {
        se_string_builder_append(builder, "[");
        se_string_builder_append(builder, se_string_cast(value.textures[it].generation));
        se_string_builder_append(builder, ", ");
        se_string_builder_append(builder, se_string_cast(value.textures[it].index));
        se_string_builder_append(builder, "], ");
    }
    se_string_builder_append(builder, "]]");
    return se_string_builder_end(builder);
}

template<>
SeString se_string_cast<SeVkFramebuffer>(const SeVkFramebuffer& value, SeStringLifetime lifetime)
{
    SeStringBuilder builder = se_string_builder_begin("[Framebuffer. ", lifetime);
    se_string_builder_append(builder, "id: [");
    se_string_builder_append(builder, se_string_cast(value.object.uniqueIndex));
    se_string_builder_append(builder, "]");
    return se_string_builder_end(builder);
}

#endif
