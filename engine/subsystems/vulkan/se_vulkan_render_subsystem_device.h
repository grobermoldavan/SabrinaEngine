#ifndef _SE_VULKAN_RENDER_SUSBSYSTEM_DEVICE_H_
#define _SE_VULKAN_RENDER_SUSBSYSTEM_DEVICE_H_

#include "se_vulkan_render_subsystem_base.h"

#define SE_VK_MAX_UNIQUE_COMMAND_QUEUES 3
#define SE_VK_NUM_IMAGES_IN_FLIGHT      3

typedef enum SeVkCommandQueueFlagBits
{
    SE_VK_CMD_QUEUE_GRAPHICS    = 0x00000001,
    SE_VK_CMD_QUEUE_PRESENT     = 0x00000002,
    SE_VK_CMD_QUEUE_TRANSFER    = 0x00000004,
} SeVkCommandQueueFlagBits;
typedef uint32_t SeVkCommandQueueFlags;

SeRenderObject*         se_vk_device_create(SeRenderDeviceCreateInfo* createInfo);
void                    se_vk_device_destroy(SeRenderObject* device);
void                    se_vk_device_wait(SeRenderObject* device);
void                    se_vk_device_begin_frame(SeRenderObject* device);
void                    se_vk_device_end_frame(SeRenderObject* device);
size_t                  se_vk_device_get_swap_chain_textures_num(SeRenderObject* device);
SeRenderObject*         se_vk_device_get_swap_chain_texture(SeRenderObject* device, size_t index);
size_t                  se_vk_device_get_active_swap_chain_texture_index(SeRenderObject* device);
SeSamplingFlags         se_vk_device_get_supported_sampling_types(SeRenderObject* device);

struct SeVkMemoryManager*           se_vk_device_get_memory_manager(SeRenderObject* device);
VkCommandPool                       se_vk_device_get_command_pool(SeRenderObject* device, SeVkCommandQueueFlags flags);
VkQueue                             se_vk_device_get_command_queue(SeRenderObject* device, SeVkCommandQueueFlags flags);
uint32_t                            se_vk_device_get_command_queue_family_index(SeRenderObject* device, SeVkCommandQueueFlags flags);
VkDevice                            se_vk_device_get_logical_handle(SeRenderObject* device);
bool                                se_vk_device_is_stencil_supported(SeRenderObject* device);
VkSampleCountFlags                  se_vk_device_get_supported_framebuffer_multisample_types(SeRenderObject* device);
VkSampleCountFlags                  se_vk_device_get_supported_image_multisample_types(SeRenderObject* device);
VkFormat                            se_vk_device_get_depth_stencil_format(SeRenderObject* device);
VkFormat                            se_vk_device_get_swap_chain_format(SeRenderObject* device);
VkExtent2D                          se_vk_device_get_swap_chain_extent(SeRenderObject* device);
VkPhysicalDeviceMemoryProperties*   se_vk_device_get_memory_properties(SeRenderObject* device);
VkSwapchainKHR                      se_vk_device_get_swap_chain_handle(SeRenderObject* device);
struct SeVkInFlightManager*         se_vk_device_get_in_flight_manager(SeRenderObject* device);

#endif
