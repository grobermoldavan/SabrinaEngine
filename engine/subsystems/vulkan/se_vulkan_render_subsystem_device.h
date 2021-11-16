#ifndef _SE_VULKAN_RENDER_SUSBSYSTEM_DEVICE_H_
#define _SE_VULKAN_RENDER_SUSBSYSTEM_DEVICE_H_

#include "se_vulkan_render_subsystem_base.h"

typedef enum SeVkCommandQueueFlagBits
{
    SE_VK_CMD_QUEUE_GRAPHICS    = 0x00000001,
    SE_VK_CMD_QUEUE_PRESENT     = 0x00000002,
    SE_VK_CMD_QUEUE_TRANSFER    = 0x00000004,
} SeVkCommandQueueFlagBits;

typedef uint32_t SeVkCommandQueueFlags;

struct SeRenderObject;

struct SeRenderObject*  se_vk_device_create(struct SeRenderDeviceCreateInfo* createInfo);
void                    se_vk_device_destroy(struct SeRenderObject* device);
void                    se_vk_device_wait(struct SeRenderObject* device);
size_t                  se_vk_device_get_swap_chain_textures_num(struct SeRenderObject* device);
struct SeRenderObject*  se_vk_device_get_swap_chain_texture(struct SeRenderObject* device, size_t index);

struct SeVkMemoryManager*   se_vk_device_get_memory_manager(struct SeRenderObject* device);
VkCommandPool               se_vk_device_get_command_pool(struct SeRenderObject* device, SeVkCommandQueueFlags flags);
VkQueue                     se_vk_device_get_command_queue(struct SeRenderObject* device, SeVkCommandQueueFlags flags);
VkDevice                    se_vk_device_get_logical_handle(struct SeRenderObject* device);
struct SeRenderObject*      se_vk_device_get_last_command_buffer(struct SeRenderObject* device);
void                        se_vk_device_submit_command_buffer(struct SeRenderObject* device, VkSubmitInfo* submitInfo, struct SeRenderObject* buffer, VkQueue queue);
bool                        se_vk_device_is_stencil_supported(struct SeRenderObject* device);
VkSampleCountFlags          se_vk_device_get_supported_framebuffer_multisample_types(struct SeRenderObject* device);
VkSampleCountFlags          se_vk_device_get_supported_image_multisample_types(struct SeRenderObject* device);
VkFormat                    se_vk_device_get_depth_stencil_format(struct SeRenderObject* device);
VkFormat                    se_vk_device_get_swap_chain_format(struct SeRenderObject* device);

#endif
