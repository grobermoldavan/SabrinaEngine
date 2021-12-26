#ifndef _SE_VULKAN_RENDER_SUSBSYSTEM_DEVICE_H_
#define _SE_VULKAN_RENDER_SUSBSYSTEM_DEVICE_H_

#include "se_vulkan_render_subsystem_base.h"
#include "engine/render_abstraction_interface.h"

#define SE_VK_MAX_UNIQUE_COMMAND_QUEUES 3
#define SE_VK_NUM_IMAGES_IN_FLIGHT      3

typedef enum SeVkCommandQueueFlagBits
{
    SE_VK_CMD_QUEUE_GRAPHICS    = 0x00000001,
    SE_VK_CMD_QUEUE_PRESENT     = 0x00000002,
    SE_VK_CMD_QUEUE_TRANSFER    = 0x00000004,
} SeVkCommandQueueFlagBits;
typedef uint32_t SeVkCommandQueueFlags;

struct SeRenderObject*  se_vk_device_create(struct SeRenderDeviceCreateInfo* createInfo);
void                    se_vk_device_destroy(struct SeRenderObject* device);
void                    se_vk_device_wait(struct SeRenderObject* device);
void                    se_vk_device_begin_frame(struct SeRenderObject* device);
void                    se_vk_device_end_frame(struct SeRenderObject* device);
size_t                  se_vk_device_get_swap_chain_textures_num(struct SeRenderObject* device);
struct SeRenderObject*  se_vk_device_get_swap_chain_texture(struct SeRenderObject* device, size_t index);
size_t                  se_vk_device_get_active_swap_chain_texture_index(struct SeRenderObject* device);
SeSamplingFlags         se_vk_device_get_supported_sampling_types(struct SeRenderObject* device);

struct SeVkMemoryManager*           se_vk_device_get_memory_manager(struct SeRenderObject* device);
VkCommandPool                       se_vk_device_get_command_pool(struct SeRenderObject* device, SeVkCommandQueueFlags flags);
VkQueue                             se_vk_device_get_command_queue(struct SeRenderObject* device, SeVkCommandQueueFlags flags);
uint32_t                            se_vk_device_get_command_queue_family_index(struct SeRenderObject* device, SeVkCommandQueueFlags flags);
VkDevice                            se_vk_device_get_logical_handle(struct SeRenderObject* device);
bool                                se_vk_device_is_stencil_supported(struct SeRenderObject* device);
VkSampleCountFlags                  se_vk_device_get_supported_framebuffer_multisample_types(struct SeRenderObject* device);
VkSampleCountFlags                  se_vk_device_get_supported_image_multisample_types(struct SeRenderObject* device);
VkFormat                            se_vk_device_get_depth_stencil_format(struct SeRenderObject* device);
VkFormat                            se_vk_device_get_swap_chain_format(struct SeRenderObject* device);
VkExtent2D                          se_vk_device_get_swap_chain_extent(struct SeRenderObject* device);
VkPhysicalDeviceMemoryProperties*   se_vk_device_get_memory_properties(struct SeRenderObject* device);
VkSwapchainKHR                      se_vk_device_get_swap_chain_handle(struct SeRenderObject* device);
struct SeVkInFlightManager*         se_vk_device_get_in_flight_manager(struct SeRenderObject* device);

#endif
