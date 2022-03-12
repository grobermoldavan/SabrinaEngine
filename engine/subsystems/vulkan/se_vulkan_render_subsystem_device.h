#ifndef _SE_VULKAN_RENDER_SUSBSYSTEM_DEVICE_H_
#define _SE_VULKAN_RENDER_SUSBSYSTEM_DEVICE_H_

#include "se_vulkan_render_subsystem_base.h"
#include "se_vulkan_render_subsystem_memory.h"
#include "se_vulkan_render_subsystem_frame_manager.h"
#include "se_vulkan_render_subsystem_graph.h"
#include "engine/subsystems/se_window_subsystem.h"

#define se_vk_device_get_logical_handle(device)                     ((device)->gpu.logicalHandle)
#define se_vk_device_is_stencil_supported(device)                   ((device)->gpu.flags & SE_VK_GPU_HAS_STENCIL)
#define se_vk_device_get_command_pool(device, flags)                (se_vk_gpu_get_command_queue(&(device)->gpu, flags)->commandPoolHandle)
#define se_vk_device_get_command_queue(device, flags)               (se_vk_gpu_get_command_queue(&(device)->gpu, flags)->handle)
#define se_vk_device_get_command_queue_family_index(device, flags)  (se_vk_gpu_get_command_queue(&(device)->gpu, flags)->queueFamilyIndex)
#define se_vk_device_get_depth_stencil_format(device)               ((device)->gpu.depthStencilFormat)
#define se_vk_device_get_swap_chain_format(device)                  ((device)->swapChain.surfaceFormat.format)
#define se_vk_device_get_swap_chain_extent(device)                  ((device)->swapChain.extent)
#define se_vk_device_get_memory_properties(device)                  (&(device)->gpu.memoryProperties)
#define se_vk_device_get_swap_chain_handle(device)                  ((device)->swapChain.handle)
#define se_vk_device_get_physical_device_features(device)           (&(device)->gpu.enabledFeatures)
#define se_vk_device_get_physical_device_properties(device)         (&(device)->gpu.deviceProperties_10)
#define se_vk_device_get_swap_chain_texture(device, index)          ((device)->swapChain.textures[index])

#define SE_VK_MAX_UNIQUE_COMMAND_QUEUES 4
#define SE_VK_MAX_SWAP_CHAIN_IMAGES     16

typedef enum SeVkCommandQueueFlagBits
{
    SE_VK_CMD_QUEUE_GRAPHICS    = 0x00000001,
    SE_VK_CMD_QUEUE_PRESENT     = 0x00000002,
    SE_VK_CMD_QUEUE_TRANSFER    = 0x00000004,
    SE_VK_CMD_QUEUE_COMPUTE     = 0x00000008,
} SeVkCommandQueueFlagBits;
typedef SeVkFlags SeVkCommandQueueFlags;

typedef enum SeVkGpuFlagBits
{
    SE_VK_GPU_HAS_STENCIL = 0x00000001,
} SeVkGpuFlagBits;
typedef uint32_t SeVkGpuFlags;

typedef enum SeVkDeviceFlagBits
{
    SE_VK_DEVICE_IN_FRAME = 0x00000001,
} SeVkDeviceFlagBits;
typedef uint32_t SeVkDeviceFlags;

typedef struct SeVkCommandQueue
{
    VkQueue                 handle;
    uint32_t                queueFamilyIndex;
    SeVkCommandQueueFlags   flags;
    VkCommandPool           commandPoolHandle;
} SeVkCommandQueue;

typedef struct SeVkGpu
{
    VkPhysicalDeviceFeatures            enabledFeatures;
    VkPhysicalDeviceProperties          deviceProperties_10;
    VkPhysicalDeviceVulkan11Properties  deviceProperties_11;
    VkPhysicalDeviceVulkan12Properties  deviceProperties_12;
    VkPhysicalDeviceMemoryProperties    memoryProperties;
    SeVkCommandQueue                    commandQueues[SE_VK_MAX_UNIQUE_COMMAND_QUEUES];
    VkPhysicalDevice                    physicalHandle;
    VkDevice                            logicalHandle;
    VkFormat                            depthStencilFormat;
    SeVkGpuFlags                        flags;
} SeVkGpu;

typedef struct SeVkSwapChainImage
{
    VkImage     handle;
    VkImageView view;
} SeVkSwapChainImage;

typedef struct SeVkSwapChain
{
    VkSwapchainKHR      handle;
    VkSurfaceFormatKHR  surfaceFormat;
    VkExtent2D          extent;
    SeVkSwapChainImage  images[SE_VK_MAX_SWAP_CHAIN_IMAGES];
    struct SeVkTexture* textures[SE_VK_MAX_SWAP_CHAIN_IMAGES];
    size_t              numTextures;
} SeVkSwapChain;

typedef struct SeVkDevice
{
    SeVkObject                      object;
    SeVkMemoryManager               memoryManager;
    VkInstance                      instance;
    VkDebugUtilsMessengerEXT        debugMessenger;
    VkSurfaceKHR                    surface;
    SeVkGpu                         gpu;
    SeVkSwapChain                   swapChain;
    SeVkDeviceFlags                 flags;
    SeWindowResizeCallbackHandle    resizeCallbackHandle;
    SeVkFrameManager                frameManager;
    SeVkGraph                       graph;
} SeVkDevice;

SeDeviceHandle                      se_vk_device_create(SeRenderDeviceCreateInfo* createInfo);
void                                se_vk_device_destroy(SeDeviceHandle device);
void                                se_vk_device_begin_frame(SeDeviceHandle device);
void                                se_vk_device_end_frame(SeDeviceHandle device);

SeVkFlags                           se_vk_device_get_supported_sampling_types(SeVkDevice* device);
VkSampleCountFlags                  se_vk_device_get_supported_framebuffer_multisample_types(SeVkDevice* device);
VkSampleCountFlags                  se_vk_device_get_supported_image_multisample_types(SeVkDevice* device);
void                                se_vk_device_fill_sharing_mode(SeVkDevice* device, SeVkCommandQueueFlags flags, uint32_t* numQueues, uint32_t* queueFamilyIndices, VkSharingMode* sharingMode);

#endif
