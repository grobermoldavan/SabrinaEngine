#ifndef _SE_VULKAN_RENDER_SUBSYSTEM_FRAMEBUFFER_H_
#define _SE_VULKAN_RENDER_SUBSYSTEM_FRAMEBUFFER_H_

#include "se_vulkan_render_subsystem_base.hpp"
#include "se_vulkan_render_subsystem_render_pass.hpp"
#include "se_vulkan_render_subsystem_texture.hpp"
#include "engine/containers.hpp"

#define SE_VK_FRAMEBUFFER_MAX_TEXTURES 8

struct SeVkFramebufferInfo
{
    struct SeVkDevice*                  device;
    ObjectPoolEntryRef<SeVkRenderPass>  pass;
    ObjectPoolEntryRef<SeVkTexture>     textures[SE_VK_FRAMEBUFFER_MAX_TEXTURES];
    uint32_t                            numTextures;
};

struct SeVkFramebuffer
{
    SeVkObject                          object;
    struct SeVkDevice*                  device;
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
        HashValueBuilder builder = hash_value::builder::create();
        hash_value::builder::absorb(builder, value);
        return hash_value::builder::end(builder);
    }
}

#endif
