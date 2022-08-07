#ifndef _SE_VULKAN_RENDER_SUBSYSTEM_FRAMEBUFFER_H_
#define _SE_VULKAN_RENDER_SUBSYSTEM_FRAMEBUFFER_H_

#include "se_vulkan_render_subsystem_base.hpp"
#include "se_vulkan_render_subsystem_render_pass.hpp"
#include "se_vulkan_render_subsystem_texture.hpp"

#define SE_VK_FRAMEBUFFER_MAX_TEXTURES 8

struct SeVkFramebufferInfo
{
    SeVkDevice*                         device;
    ObjectPoolEntryRef<SeVkRenderPass>  pass;
    ObjectPoolEntryRef<SeVkTexture>     textures[SE_VK_FRAMEBUFFER_MAX_TEXTURES];
    uint32_t                            numTextures;
};

struct SeVkFramebuffer
{
    SeVkObject                          object;
    SeVkDevice*                         device;
    ObjectPoolEntryRef<SeVkRenderPass>  pass;
    ObjectPoolEntryRef<SeVkTexture>     textures[SE_VK_FRAMEBUFFER_MAX_TEXTURES];
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

namespace hash_value
{
    namespace builder
    {
        template<>
        void absorb<SeVkFramebufferInfo>(HashValueBuilder& builder, const SeVkFramebufferInfo& value)
        {
            hash_value::builder::absorb(builder, value.pass);
            hash_value::builder::absorb(builder, value.numTextures);
            for (size_t it = 0; it < value.numTextures; it++) hash_value::builder::absorb(builder, value.textures[it]);
        }
    }

    template<>
    HashValue generate<SeVkFramebufferInfo>(const SeVkFramebufferInfo& value)
    {
        HashValueBuilder builder = hash_value::builder::begin();
        hash_value::builder::absorb(builder, value);
        return hash_value::builder::end(builder);
    }
}

namespace string
{
    template<>
    SeString cast<SeVkFramebufferInfo>(const SeVkFramebufferInfo& value, SeStringLifetime lifetime)
    {
        SeStringBuilder builder = string_builder::begin(lifetime);
        string_builder::append(builder, "[Framebuffer info. ");
        string_builder::append(builder, "Pass id: [");
        string_builder::append(builder, string::cast(value.pass.generation));
        string_builder::append(builder, ", ");
        string_builder::append(builder, string::cast(value.pass.index));
        string_builder::append(builder, "]. Textures: [num: ");
        string_builder::append(builder, string::cast(value.numTextures));
        string_builder::append(builder, ", ");
        for (size_t it = 0; it < value.numTextures; it++)
        {
            string_builder::append(builder, "[");
            string_builder::append(builder, string::cast(value.textures[it].generation));
            string_builder::append(builder, ", ");
            string_builder::append(builder, string::cast(value.textures[it].index));
            string_builder::append(builder, "], ");
        }
        string_builder::append(builder, "]]");
        return string_builder::end(builder);
    }

    template<>
    SeString cast<SeVkFramebuffer>(const SeVkFramebuffer& value, SeStringLifetime lifetime)
    {
        SeStringBuilder builder = string_builder::begin(lifetime);
        string_builder::append(builder, "Framebuffer. ");
        string_builder::append(builder, "id: [");
        string_builder::append(builder, string::cast(value.object.uniqueIndex));
        string_builder::append(builder, "]");
        return string_builder::end(builder);
    }
}

#endif
