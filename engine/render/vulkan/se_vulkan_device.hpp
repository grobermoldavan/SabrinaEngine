#ifndef _SE_VULKAN_RENDER_SUSBSYSTEM_DEVICE_H_
#define _SE_VULKAN_RENDER_SUSBSYSTEM_DEVICE_H_

#include "se_vulkan_base.hpp"
#include "se_vulkan_memory.hpp"
#include "se_vulkan_frame_manager.hpp"
#include "se_vulkan_graph.hpp"

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

enum SeVkCommandQueueFlagBits
{
    SE_VK_CMD_QUEUE_GRAPHICS    = 0x00000001,
    SE_VK_CMD_QUEUE_PRESENT     = 0x00000002,
    SE_VK_CMD_QUEUE_TRANSFER    = 0x00000004,
    SE_VK_CMD_QUEUE_COMPUTE     = 0x00000008,
};
using SeVkCommandQueueFlags = SeVkFlags;

enum SeVkGpuFlagBits
{
    SE_VK_GPU_HAS_STENCIL = 0x00000001,
};
using SeVkGpuFlags = SeVkFlags;

enum SeVkDeviceFlagBits
{
    SE_VK_DEVICE_IN_FRAME = 0x00000001,
};
using SeVkDeviceFlags = SeVkFlags;

struct SeVkCommandQueue
{
    VkQueue                 handle;
    uint32_t                queueFamilyIndex;
    SeVkCommandQueueFlags   flags;
    VkCommandPool           commandPoolHandle;
};

struct SeVkGpu
{
    VkPhysicalDeviceFeatures            enabledFeatures;
    VkPhysicalDeviceProperties          deviceProperties_10;
    VkPhysicalDeviceVulkan11Properties  deviceProperties_11;
    VkPhysicalDeviceVulkan12Properties  deviceProperties_12;
    VkPhysicalDeviceMemoryProperties    memoryProperties;
    SeVkCommandQueue                    commandQueues[SeVkConfig::MAX_UNIQUE_COMMAND_QUEUES];
    VkPhysicalDevice                    physicalHandle;
    VkDevice                            logicalHandle;
    VkFormat                            depthStencilFormat;
    SeVkGpuFlags                        flags;
};

struct SeVkSwapChainImage
{
    VkImage     handle;
    VkImageView view;
};

struct SeVkSwapChain
{
    VkSwapchainKHR                  handle;
    VkSurfaceFormatKHR              surfaceFormat;
    VkExtent2D                      extent;
    SeVkSwapChainImage              images[SeVkConfig::MAX_SWAP_CHAIN_IMAGES];
    SeObjectPoolEntryRef<SeVkTexture> textures[SeVkConfig::MAX_SWAP_CHAIN_IMAGES];
    size_t                          numTextures;
};

struct SeVkGraveyard
{
    template<typename Ref>
    struct Entry
    {
        Ref ref;
        size_t frameIndex;
    };
    SeDynamicArray<Entry<SeProgramRef>>   programs;
    SeDynamicArray<Entry<SeSamplerRef>>   samplers;
    SeDynamicArray<Entry<SeBufferRef>>    buffers;
    SeDynamicArray<Entry<SeTextureRef>>   textures;
};

struct SeVkDevice
{
    SeVkObject                      object;
    SeVkMemoryManager               memoryManager;
    VkInstance                      instance;
    VkDebugUtilsMessengerEXT        debugMessenger;
    VkSurfaceKHR                    surface;
    SeVkGpu                         gpu;
    SeVkSwapChain                   swapChain;
    SeVkDeviceFlags                 flags;
    SeVkFrameManager                frameManager;
    SeVkGraph                       graph;
    SeVkGraveyard                   graveyard;
};

SeVkDevice*                         se_vk_device_create(const SeSettings& settings, void* nativeWindowHandle);
void                                se_vk_device_destroy(SeVkDevice* device);
void                                se_vk_device_begin_frame(SeVkDevice* device, VkExtent2D extent);
void                                se_vk_device_end_frame(SeVkDevice* device);

template<typename Ref> void         se_vk_device_submit_to_graveyard(SeVkDevice* device, Ref ref);
template<typename Ref> void         se_vk_device_update_graveyard_collection(SeVkDevice* device, SeDynamicArray<SeVkGraveyard::Entry<Ref>>& collection);
void                                se_vk_device_update_graveyard(SeVkDevice* device);

SeVkFlags                           se_vk_device_get_supported_sampling_types(SeVkDevice* device);
VkSampleCountFlags                  se_vk_device_get_supported_framebuffer_multisample_types(SeVkDevice* device);
VkSampleCountFlags                  se_vk_device_get_supported_image_multisample_types(SeVkDevice* device);
void                                se_vk_device_fill_sharing_mode(SeVkDevice* device, SeVkCommandQueueFlags flags, uint32_t* numQueues, uint32_t* queueFamilyIndices, VkSharingMode* sharingMode);

#endif
