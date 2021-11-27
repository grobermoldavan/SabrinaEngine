
#include "se_vulkan_render_subsystem_device.h"
#include "se_vulkan_render_subsystem_base.h"
#include "se_vulkan_render_subsystem_memory.h"
#include "se_vulkan_render_subsystem_utils.h"
#include "se_vulkan_render_subsystem_texture.h"
#include "se_vulkan_render_subsystem_command_buffer.h"
#include "engine/subsystems/se_window_subsystem.h"
#include "engine/platform.h"
#include "engine/render_abstraction_interface.h"
#include "engine/allocator_bindings.h"

#define SE_DEBUG_IMPL
#include "engine/debug.h"

extern SeWindowSubsystemInterface* g_windowIface;

#define SE_VK_MAX_UNIQUE_COMMAND_QUEUES 3
#define SE_VK_MAX_SWAP_CHAIN_IMAGES     16
#define SE_VK_UNUSED_IN_FLIGHT_DATA_REF ~((uint32_t)0)
#define SE_VK_NUM_IMAGES_IN_FLIGHT      3

typedef enum SeVkGpuFlags
{
    SE_VK_GPU_HAS_STENCIL = 0x00000001,
} SeVkGpuFlags;

typedef struct SeVkCommandQueue
{
    VkQueue handle;
    uint32_t queueFamilyIndex;
    SeVkCommandQueueFlags flags;
    VkCommandPool commandPoolHandle;
} SeVkCommandQueue;

typedef struct SeVkGpu
{
    VkPhysicalDeviceProperties deviceProperties_10;
    VkPhysicalDeviceVulkan11Properties deviceProperties_11;
    VkPhysicalDeviceVulkan12Properties deviceProperties_12;
    VkPhysicalDeviceMemoryProperties memoryProperties;
    SeVkCommandQueue commandQueues[SE_VK_MAX_UNIQUE_COMMAND_QUEUES];
    VkPhysicalDevice physicalHandle;
    VkDevice logicalHandle;
    VkFormat depthStencilFormat;
    uint32_t flags;
} SeVkGpu;

typedef enum SeVkRenderDeviceFlags
{
    SE_VK_DEVICE_IS_IN_RENDER_FRAME      = 0x00000001,
    SE_VK_DEVICE_HAS_SUBMITTED_BUFFERS   = 0x00000002,
} SeVkRenderDeviceFlags;

typedef struct SeVkSwapChainImage
{
    VkImage handle;
    VkImageView view;
} SeVkSwapChainImage;

typedef struct SeVkSwapChain
{
    VkSwapchainKHR handle;
    VkSurfaceFormatKHR surfaceFormat;
    VkExtent2D extent;
    SeVkSwapChainImage images[SE_VK_MAX_SWAP_CHAIN_IMAGES];
    SeRenderObject* textures[SE_VK_MAX_SWAP_CHAIN_IMAGES];
    size_t numTextures;
} SeVkSwapChain;

typedef struct SeVkPerImageInFlightData
{
    se_sbuffer(SeRenderObject*) commandBuffers;
    VkSemaphore imageAvailableSemaphore;
} SeVkPerImageInFlightData;

typedef struct SeVkInFlightData
{
    SeVkPerImageInFlightData inFlightData[SE_VK_NUM_IMAGES_IN_FLIGHT];
    uint32_t swapChainImageToInFlightFrameMap[SE_VK_MAX_SWAP_CHAIN_IMAGES];
    uint32_t activeFrameInFlightIndex;
    uint32_t activeSwapChainImageIndex;
} SeVkInFlightData;

typedef struct SeVkRenderDevice
{
    SeRenderObject handle;
    SeVkMemoryManager memoryManager;
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;
    VkSurfaceKHR surface;
    SeVkGpu gpu;
    SeVkSwapChain swapChain;
    SeVkInFlightData inFlightData;
    uint64_t flags;
} SeVkRenderDevice;

static void se_vk_gpu_fill_required_physical_deivce_features(VkPhysicalDeviceFeatures* features)
{
    // nothing
}

static bool se_vk_gpu_is_device_suitable(VkPhysicalDevice device, VkSurfaceKHR surface, SeAllocatorBindings* bindings)
{
    se_sbuffer(VkQueueFamilyProperties) familyProperties = se_vk_utils_get_physical_device_queue_family_properties(device, bindings);
    uint32_t graphicsQueue = se_vk_utils_pick_graphics_queue(familyProperties);
    uint32_t presentQueue = se_vk_utils_pick_present_queue(familyProperties, device, surface);
    uint32_t transferQueue = se_vk_utils_pick_transfer_queue(familyProperties);
    se_sbuffer_destroy(familyProperties);
    size_t numExtensions;
    const char** extensions = se_vk_utils_get_required_device_extensions(&numExtensions);
    bool isRequiredExtensionsSupported = se_vk_utils_does_physical_device_supports_required_extensions(device, extensions, numExtensions, bindings);
    bool isSwapChainSuppoted = false;
    if (isRequiredExtensionsSupported)
    {
        SeVkSwapChainSupportDetails supportDetails = se_vk_utils_create_swap_chain_support_details(surface, device, bindings);
        isSwapChainSuppoted = se_sbuffer_size(supportDetails.formats) && se_sbuffer_size(supportDetails.presentModes);
        se_vk_utils_destroy_swap_chain_support_details(&supportDetails);
    }
    VkPhysicalDeviceFeatures requiredFeatures = {0};
    se_vk_gpu_fill_required_physical_deivce_features(&requiredFeatures);
    bool isRequiredFeaturesSupported = se_vk_utils_does_physical_device_supports_required_features(device, &requiredFeatures);
    return
        (graphicsQueue != SE_VK_INVALID_QUEUE) && (presentQueue != SE_VK_INVALID_QUEUE) && (transferQueue != SE_VK_INVALID_QUEUE) &&
        isRequiredExtensionsSupported && isSwapChainSuppoted && isRequiredFeaturesSupported;
};

static SeVkCommandQueue* se_vk_gpu_get_command_queue(SeVkGpu* gpu, SeVkCommandQueueFlags flags)
{
    for (size_t it = 0; it < SE_VK_MAX_UNIQUE_COMMAND_QUEUES; it++)
    {
        SeVkCommandQueue* queue = &gpu->commandQueues[it];
        if (queue->flags & flags)
        {
            return queue;
        }
    }
    return NULL;
}

static VkPhysicalDevice se_vk_gpu_pick_physical_device(VkInstance instance, VkSurfaceKHR surface, SeAllocatorBindings* bindings)
{
    se_sbuffer(VkPhysicalDevice) available = se_vk_utils_get_available_physical_devices(instance, bindings);
    for (size_t it = 0; it < se_sbuffer_size(available); it++)
    {
        if (se_vk_gpu_is_device_suitable(available[it], surface, bindings))
        {
            // @TODO :  log device name and shit
            // VkPhysicalDeviceProperties deviceProperties;
            // vkGetPhysicalDeviceProperties(chosenPhysicalDevice, &deviceProperties);
            se_sbuffer_destroy(available);
            return available[it];
        }
    }
    se_sbuffer_destroy(available);
    se_assert(!"Unable to puck physical device");
    return VK_NULL_HANDLE;
}

VKAPI_ATTR VkBool32 VKAPI_CALL se_vk_debug_callback(
                                            VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                            VkDebugUtilsMessageTypeFlagsEXT messageType,
                                            const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                            void* pUserData) 
{
    printf("Debug callback : %s\n", pCallbackData->pMessage);
    return VK_FALSE;
}

SeRenderObject* se_vk_device_create(SeRenderDeviceCreateInfo* deviceCreateInfo)
{
    SeAllocatorBindings* allocator = deviceCreateInfo->persistentAllocator;
    SeVkRenderDevice* device = allocator->alloc(allocator->allocator, sizeof(SeVkRenderDevice), se_default_alignment, se_alloc_tag);
    device->handle.handleType = SE_RENDER_DEVICE;
    device->handle.destroy = se_vk_device_destroy;
    //
    // Memory manager
    //
    {
        SeVkMemoryManagerCreateInfo createInfo = (SeVkMemoryManagerCreateInfo)
        {
            .persistentAllocator = deviceCreateInfo->persistentAllocator,
            .frameAllocator = deviceCreateInfo->frameAllocator,
        };
        se_vk_memory_manager_construct(&device->memoryManager, &createInfo);
    }
    VkAllocationCallbacks* callbacks = se_vk_memory_manager_get_callbacks(&device->memoryManager);
    //
    // Instance and debug messenger
    //
    {
#if defined(SE_DEBUG)
        //
        // Check that all validation layers are supported
        //
        size_t numValidationLayers = 0;
        const char** validationLayers = se_vk_utils_get_required_validation_layers(&numValidationLayers);
        se_sbuffer(VkLayerProperties) availableValidationLayers = se_vk_utils_get_available_validation_layers(device->memoryManager.cpu_frameAllocator);
        size_t size = se_sbuffer_size(availableValidationLayers);
        for (size_t requiredIt = 0; requiredIt < numValidationLayers; requiredIt++)
        {
            const char* requiredLayerName = validationLayers[requiredIt];
            bool isFound = false;
            for (size_t availableIt = 0; availableIt < se_sbuffer_size(availableValidationLayers); availableIt++)
            {
                const char* availableLayerName = availableValidationLayers[availableIt].layerName;
                if (strcmp(availableLayerName, requiredLayerName) == 0)
                {
                    isFound = true;
                    break;
                }
            }
            se_assert(isFound && "Required validation layer is not supported");
        }
        se_sbuffer_destroy(availableValidationLayers);
#endif
        //
        // Check that all instance extensions are supported
        //
        size_t numInstanceExtensions = 0;
        const char** instanceExtensions = se_vk_utils_get_required_instance_extensions(&numInstanceExtensions);
        se_sbuffer(VkExtensionProperties) availableInstanceExtensions = se_vk_utils_get_available_instance_extensions(device->memoryManager.cpu_frameAllocator);
        for (size_t requiredIt = 0; requiredIt < numInstanceExtensions; requiredIt++)
        {
            const char* requiredExtensionName = instanceExtensions[requiredIt];
            bool isFound = false;
            for (size_t availableIt = 0; availableIt < se_sbuffer_size(availableInstanceExtensions); availableIt++)
            {
                const char* availableExtensionName = availableInstanceExtensions[availableIt].extensionName;
                if (strcmp(availableExtensionName, requiredExtensionName) == 0)
                {
                    isFound = true;
                    break;
                }
            }
            se_assert(isFound && "Required instance extension is not supported");
        }
        se_sbuffer_destroy(availableInstanceExtensions);
        VkApplicationInfo applicationInfo = (VkApplicationInfo)
        {
            .sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pNext              = NULL,
            .pApplicationName   = "Sabrina Engine App",
            .applicationVersion = VK_MAKE_VERSION(0, 1, 0),
            .pEngineName        = "Sabrina Engine",
            .engineVersion      = VK_MAKE_VERSION(0, 1, 0),
            .apiVersion         = VK_API_VERSION_1_2
        };
#if defined(SE_DEBUG)
        // @NOTE :  although we setuping debug messenger in the end of this function,
        //          created debug messenger will not be able to recieve information about
        //          vkCreateInstance and vkDestroyInstance calls. So we have to pass additional
        //          VkDebugUtilsMessengerCreateInfoEXT struct pointer to VkInstanceCreateInfo::pNext
        //          to enable debug of these functions
        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = se_vk_utils_get_debug_messenger_create_info(se_vk_debug_callback, NULL);
        VkInstanceCreateInfo instanceCreateInfo = (VkInstanceCreateInfo)
        {
            .sType                      = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pNext                      = &debugCreateInfo,
            .flags                      = 0,
            .pApplicationInfo           = &applicationInfo,
            .enabledLayerCount          = (uint32_t)numValidationLayers,
            .ppEnabledLayerNames        = validationLayers,
            .enabledExtensionCount      = (uint32_t)numInstanceExtensions,
            .ppEnabledExtensionNames    = instanceExtensions,
        };
#else
        VkInstanceCreateInfo instanceCreateInfo = (VkInstanceCreateInfo)
        {
            .sType                      = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pNext                      = NULL,
            .flags                      = 0,
            .pApplicationInfo           = &applicationInfo,
            .enabledLayerCount          = 0,
            .ppEnabledLayerNames        = NULL,
            .enabledExtensionCount      = (uint32_t)numInstanceExtensions,
            .ppEnabledExtensionNames    = instanceExtensions,
        };
#endif
        se_vk_check(vkCreateInstance(&instanceCreateInfo, callbacks, &device->instance));
#if defined(SE_DEBUG)
        device->debugMessenger = se_vk_utils_create_debug_messenger(&debugCreateInfo, device->instance, callbacks);
#endif
        volkLoadInstance(device->instance);
    }
    //
    // Surface
    //
    {
#ifdef _WIN32
        HWND windowHandle = (HWND)g_windowIface->get_native_handle(*deviceCreateInfo->window);
        VkWin32SurfaceCreateInfoKHR surfaceCreateInfo = (VkWin32SurfaceCreateInfoKHR)
        {
            .sType      = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
            .pNext      = NULL,
            .flags      = 0,
            .hinstance  = GetModuleHandle(NULL),
            .hwnd       = windowHandle,
        };
        se_vk_check(vkCreateWin32SurfaceKHR(device->instance, &surfaceCreateInfo, callbacks, &device->surface));
#else
#   error Unsupported platform
#endif
    }
    //
    // GPU
    //
    {
        device->gpu.physicalHandle = se_vk_gpu_pick_physical_device(device->instance, device->surface, deviceCreateInfo->frameAllocator);
        //
        // Get command queue infos
        //
        se_sbuffer(VkQueueFamilyProperties) familyProperties = se_vk_utils_get_physical_device_queue_family_properties(device->gpu.physicalHandle, deviceCreateInfo->frameAllocator);
		uint32_t queuesFamilyIndices[] =
		{
			se_vk_utils_pick_graphics_queue(familyProperties),
			se_vk_utils_pick_present_queue(familyProperties, device->gpu.physicalHandle, device->surface),
			se_vk_utils_pick_transfer_queue(familyProperties),
		};
        se_sbuffer(VkDeviceQueueCreateInfo) queueCreateInfos = se_vk_utils_get_queue_create_infos(queuesFamilyIndices, se_array_size(queuesFamilyIndices), deviceCreateInfo->frameAllocator);
        //
        // Create logical device
        //
        VkPhysicalDeviceFeatures deviceFeatures = {0};
        se_vk_gpu_fill_required_physical_deivce_features(&deviceFeatures);
        size_t numValidationLayers;
        const char** requiredValidationLayers = se_vk_utils_get_required_validation_layers(&numValidationLayers);
        size_t numDeviceExtensions;
        const char** requiredDeviceExtensions = se_vk_utils_get_required_device_extensions(&numDeviceExtensions);
        VkDeviceCreateInfo logicalDeviceCreateInfo = (VkDeviceCreateInfo)
        {
            .sType                      = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext                      = NULL,
            .flags                      = 0,
            .queueCreateInfoCount       = (uint32_t)se_sbuffer_size(queueCreateInfos),
            .pQueueCreateInfos          = queueCreateInfos,
            .enabledLayerCount          = (uint32_t)numValidationLayers,
            .ppEnabledLayerNames        = requiredValidationLayers,
            .enabledExtensionCount      = (uint32_t)numDeviceExtensions,
            .ppEnabledExtensionNames    = requiredDeviceExtensions,
            .pEnabledFeatures           = &deviceFeatures
        };
        se_vk_check(vkCreateDevice(device->gpu.physicalHandle, &logicalDeviceCreateInfo, callbacks, &device->gpu.logicalHandle));
        //
        // Create queues
        //
        for (size_t it = 0; it < se_sbuffer_size(queueCreateInfos); it++)
        {
            SeVkCommandQueue* queue = &device->gpu.commandQueues[it];
            queue->queueFamilyIndex = queueCreateInfos[it].queueFamilyIndex;
            if (queuesFamilyIndices[0] == queue->queueFamilyIndex) queue->flags |= SE_VK_CMD_QUEUE_GRAPHICS;
            if (queuesFamilyIndices[1] == queue->queueFamilyIndex) queue->flags |= SE_VK_CMD_QUEUE_PRESENT;
            if (queuesFamilyIndices[2] == queue->queueFamilyIndex) queue->flags |= SE_VK_CMD_QUEUE_TRANSFER;
            vkGetDeviceQueue(device->gpu.logicalHandle, queue->queueFamilyIndex, 0, &queue->handle);
            VkCommandPoolCreateInfo poolCreateInfo = se_vk_utils_command_pool_create_info(queue->queueFamilyIndex, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);
            se_vk_check(vkCreateCommandPool(device->gpu.logicalHandle, &poolCreateInfo, callbacks, &queue->commandPoolHandle));
        }
        se_sbuffer_destroy(familyProperties);
        se_sbuffer_destroy(queueCreateInfos);
        //
        // Get device info
        //
        bool hasStencil = se_vk_utils_pick_depth_stencil_format(device->gpu.physicalHandle, &device->gpu.depthStencilFormat);
        if (hasStencil) device->gpu.flags |= SE_VK_GPU_HAS_STENCIL;
        vkGetPhysicalDeviceMemoryProperties(device->gpu.physicalHandle, &device->gpu.memoryProperties);
        VkPhysicalDeviceVulkan12Properties deviceProperties_12 = (VkPhysicalDeviceVulkan12Properties)
        {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES,
            .pNext = NULL
        };
        VkPhysicalDeviceVulkan11Properties deviceProperties_11 = (VkPhysicalDeviceVulkan11Properties)
        {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES,
            .pNext = &deviceProperties_12
        };
        VkPhysicalDeviceProperties2 physcialDeviceProperties2 = (VkPhysicalDeviceProperties2)
        {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
            .pNext = &deviceProperties_11,
            .properties = {0},
        };
        vkGetPhysicalDeviceProperties2(device->gpu.physicalHandle, &physcialDeviceProperties2);
        device->gpu.deviceProperties_10 = physcialDeviceProperties2.properties;
        device->gpu.deviceProperties_11 = deviceProperties_11;
        device->gpu.deviceProperties_12 = deviceProperties_12;
    }
    se_vk_memory_manager_set_device(&device->memoryManager, device->gpu.logicalHandle);
    //
    // Swap chain
    //
    {
        {
            uint32_t windowWidth = g_windowIface->get_width(*deviceCreateInfo->window);
            uint32_t windowHeight = g_windowIface->get_height(*deviceCreateInfo->window);;
            SeVkSwapChainSupportDetails supportDetails = se_vk_utils_create_swap_chain_support_details(device->surface, device->gpu.physicalHandle, deviceCreateInfo->frameAllocator);
            VkSurfaceFormatKHR  surfaceFormat   = se_vk_utils_choose_swap_chain_surface_format(supportDetails.formats);
            VkPresentModeKHR    presentMode     = se_vk_utils_choose_swap_chain_surface_present_mode(supportDetails.presentModes);
            VkExtent2D          extent          = se_vk_utils_choose_swap_chain_extent(windowWidth, windowHeight, &supportDetails.capabilities);
            // @NOTE :  supportDetails.capabilities.maxImageCount == 0 means unlimited amount of images in swapchain
            uint32_t imageCount = supportDetails.capabilities.minImageCount + 1;
            if (supportDetails.capabilities.maxImageCount != 0 && imageCount > supportDetails.capabilities.maxImageCount)
            {
                imageCount = supportDetails.capabilities.maxImageCount;
            }
            uint32_t queueFamiliesIndices[SE_VK_MAX_UNIQUE_COMMAND_QUEUES] = {0};
            VkSwapchainCreateInfoKHR swapChainCreateInfo = (VkSwapchainCreateInfoKHR)
            {
                .sType                  = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
                .pNext                  = NULL,
                .flags                  = 0,
                .surface                = device->surface,
                .minImageCount          = imageCount,
                .imageFormat            = surfaceFormat.format,
                .imageColorSpace        = surfaceFormat.colorSpace,
                .imageExtent            = extent,
                .imageArrayLayers       = 1,
                .imageUsage             = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                .imageSharingMode       = VK_SHARING_MODE_EXCLUSIVE,
                .queueFamilyIndexCount  = 0,
                .pQueueFamilyIndices    = queueFamiliesIndices,
                .preTransform           = supportDetails.capabilities.currentTransform, // Allows to apply transform to images (rotate etc)
                .compositeAlpha         = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
                .presentMode            = presentMode,
                .clipped                = VK_TRUE,
                .oldSwapchain           = VK_NULL_HANDLE,
            };
            //
            // Fill sharing mode
            //
            {
                SeVkCommandQueue* graphicsQueue = se_vk_gpu_get_command_queue(&device->gpu, SE_VK_CMD_QUEUE_GRAPHICS);
                SeVkCommandQueue* presentQueue = se_vk_gpu_get_command_queue(&device->gpu, SE_VK_CMD_QUEUE_PRESENT);
                if (graphicsQueue->queueFamilyIndex == presentQueue->queueFamilyIndex)
                {
                    swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
                    swapChainCreateInfo.queueFamilyIndexCount = 0;
                }
                else
                {
                    swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
                    swapChainCreateInfo.queueFamilyIndexCount = 2;
                    queueFamiliesIndices[0] = graphicsQueue->queueFamilyIndex;
                    queueFamiliesIndices[1] = presentQueue->queueFamilyIndex;
                }
            }
            se_vk_check(vkCreateSwapchainKHR(device->gpu.logicalHandle, &swapChainCreateInfo, callbacks, &device->swapChain.handle));
            device->swapChain.surfaceFormat = surfaceFormat;
            device->swapChain.extent = extent;
            se_vk_utils_destroy_swap_chain_support_details(&supportDetails);
        }
        {
            uint32_t swapChainImageCount;
            se_vk_check(vkGetSwapchainImagesKHR(device->gpu.logicalHandle, device->swapChain.handle, &swapChainImageCount, NULL));
            se_assert(swapChainImageCount < SE_VK_MAX_SWAP_CHAIN_IMAGES);
            se_sbuffer(VkImage) swapChainImageHandles = {0};
            se_sbuffer_construct(swapChainImageHandles, swapChainImageCount, deviceCreateInfo->frameAllocator);
            se_sbuffer_set_size(swapChainImageHandles, swapChainImageCount);
            // It seems like this vkGetSwapchainImagesKHR call leaks memory
            se_vk_check(vkGetSwapchainImagesKHR(device->gpu.logicalHandle, device->swapChain.handle, &swapChainImageCount, swapChainImageHandles));
            for (uint32_t it = 0; it < swapChainImageCount; it++)
            {
                VkImageView view = VK_NULL_HANDLE;
                VkImageViewCreateInfo imageViewCreateInfo = (VkImageViewCreateInfo)
                {
                    .sType              = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                    .pNext              = NULL,
                    .flags              = 0,
                    .image              = swapChainImageHandles[it],
                    .viewType           = VK_IMAGE_VIEW_TYPE_2D,
                    .format             = device->swapChain.surfaceFormat.format,
                    .components         = (VkComponentMapping)
                    {
                        .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                        .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                        .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                        .a = VK_COMPONENT_SWIZZLE_IDENTITY
                    },
                    .subresourceRange   = (VkImageSubresourceRange)
                    {
                        .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                        .baseMipLevel   = 0,
                        .levelCount     = 1,
                        .baseArrayLayer = 0,
                        .layerCount     = 1
                    }
                };
                se_vk_check(vkCreateImageView(device->gpu.logicalHandle, &imageViewCreateInfo, callbacks, &view));
                device->swapChain.images[it] = (SeVkSwapChainImage)
                {
                    .handle = swapChainImageHandles[it],
                    .view = view,
                };
            }
            device->swapChain.numTextures = swapChainImageCount;
            se_sbuffer_destroy(swapChainImageHandles);
        }
        {
            for (size_t it = 0; it < device->swapChain.numTextures; it++)
            {
                if (!device->swapChain.images->handle) break;
                SeVkSwapChainImage* image = &device->swapChain.images[it];
                SeRenderObject* tex = se_vk_texture_create_from_external_resources((SeRenderObject*)device, &device->swapChain.extent, image->handle, image->view, device->swapChain.surfaceFormat.format);
                device->swapChain.textures[it] = tex;
            }
        }
    }
    //
    // In flight data
    //
    {
        VkSemaphoreCreateInfo semaphoreCreateInfo = (VkSemaphoreCreateInfo)
        {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
        };
        for (size_t it = 0; it < SE_VK_NUM_IMAGES_IN_FLIGHT; it++)
        {
            SeVkPerImageInFlightData* data = &device->inFlightData.inFlightData[it];
            se_vk_check(vkCreateSemaphore(device->gpu.logicalHandle, &semaphoreCreateInfo, callbacks, &data->imageAvailableSemaphore));
            se_sbuffer_construct(data->commandBuffers, 8, device->memoryManager.cpu_persistentAllocator);
        }
        for (size_t it = 0; it < SE_VK_MAX_SWAP_CHAIN_IMAGES; it++)
        {
            device->inFlightData.swapChainImageToInFlightFrameMap[it] = SE_VK_UNUSED_IN_FLIGHT_DATA_REF;
        }
        device->inFlightData.activeFrameInFlightIndex = 0;
        device->inFlightData.activeSwapChainImageIndex = 0;
    }
    return (SeRenderObject*)device;
}

void se_vk_device_destroy(SeRenderObject* _device)
{
    SeVkRenderDevice* device = (SeVkRenderDevice*)_device;
    VkAllocationCallbacks* callbacks = se_vk_memory_manager_get_callbacks(&device->memoryManager);
    //
    // In flight data
    //
    for (size_t it = 0; it < SE_VK_NUM_IMAGES_IN_FLIGHT; it++)
    {
        SeVkPerImageInFlightData* perImageData = &device->inFlightData.inFlightData[it];
        vkDestroySemaphore(device->gpu.logicalHandle, perImageData->imageAvailableSemaphore, callbacks);
        for (size_t cbIt = 0; cbIt < se_sbuffer_size(perImageData->commandBuffers); cbIt++)
        {
            se_vk_command_buffer_destroy(perImageData->commandBuffers[cbIt]);
        }
        se_sbuffer_destroy(perImageData->commandBuffers);
    }
    //
    // Swap Chain
    //
    for (size_t it = 0; it < device->swapChain.numTextures; it++)
    {
        SeVkSwapChainImage* image = &device->swapChain.images[it];
        if (!image->handle) break;
        se_vk_texture_destroy(device->swapChain.textures[it]);
        vkDestroyImageView(device->gpu.logicalHandle, image->view, callbacks);
    }
    vkDestroySwapchainKHR(device->gpu.logicalHandle, device->swapChain.handle, callbacks);
    //
    // Memory manager
    //
    se_vk_memory_manager_destroy(&device->memoryManager);
    //
    // Gpu
    //
    for (size_t it = 0; it < SE_VK_MAX_UNIQUE_COMMAND_QUEUES; it++)
    {
        if (device->gpu.commandQueues[it].commandPoolHandle == VK_NULL_HANDLE) break;
        vkDestroyCommandPool(device->gpu.logicalHandle, device->gpu.commandQueues[it].commandPoolHandle, callbacks);
    }
    vkDestroyDevice(device->gpu.logicalHandle, callbacks);
    //
    // Surface
    //
    vkDestroySurfaceKHR(device->instance, device->surface, callbacks);
    //
    // Instance
    //
#if defined(SE_DEBUG)
    se_vk_utils_destroy_debug_messenger(device->instance, device->debugMessenger, callbacks);
#endif
    vkDestroyInstance(device->instance, callbacks);
    device->memoryManager.cpu_persistentAllocator->dealloc(device->memoryManager.cpu_persistentAllocator->allocator, device, sizeof(SeVkRenderDevice));
}

void se_vk_device_wait(SeRenderObject* _device)
{
    SeVkRenderDevice* device = (SeVkRenderDevice*)_device;
    vkDeviceWaitIdle(device->gpu.logicalHandle);
}

void se_vk_device_begin_frame(SeRenderObject* _device)
{
    SeVkRenderDevice* device = (SeVkRenderDevice*)_device;
    //
    // Advance in flight frame
    //
    {
        /*
            1. Advance in flight index
            2. Wait until previous command buffers of this frame finish execution
            3. Acquire next image and set the image semaphore to wait for
            4. If this image is already used by another in flight frame, wait for it's execution fence too
            5. Save in flight frame reference to tha map
        */
        SeVkInFlightData* data = &device->inFlightData;
        data->activeFrameInFlightIndex = (data->activeFrameInFlightIndex + 1) % SE_VK_NUM_IMAGES_IN_FLIGHT;
        se_sbuffer(SeRenderObject*) activeFrameCommandBuffers = data->inFlightData[data->activeFrameInFlightIndex].commandBuffers;
        if (se_sbuffer_size(activeFrameCommandBuffers))
        {
            SeRenderObject* lastCommandBuffer = activeFrameCommandBuffers[se_sbuffer_size(activeFrameCommandBuffers) - 1];
            VkFence fence = se_vk_command_buffer_get_fence(lastCommandBuffer);
            vkWaitForFences(device->gpu.logicalHandle, 1, &fence, VK_TRUE, UINT64_MAX);
        }
        se_vk_check(vkAcquireNextImageKHR(device->gpu.logicalHandle, device->swapChain.handle, UINT64_MAX, data->inFlightData[data->activeFrameInFlightIndex].imageAvailableSemaphore, VK_NULL_HANDLE, &data->activeSwapChainImageIndex));
        const uint32_t inFlightFrameReferencedBySwapChainImage = data->swapChainImageToInFlightFrameMap[data->activeSwapChainImageIndex];
        if (inFlightFrameReferencedBySwapChainImage != SE_VK_UNUSED_IN_FLIGHT_DATA_REF)
        {
            se_sbuffer(SeRenderObject*) referencedCommandBuffers = data->inFlightData[inFlightFrameReferencedBySwapChainImage].commandBuffers;
            if (se_sbuffer_size(referencedCommandBuffers))
            {
                SeRenderObject* lastCommandBuffer = referencedCommandBuffers[se_sbuffer_size(referencedCommandBuffers) - 1];
                VkFence fence = se_vk_command_buffer_get_fence(lastCommandBuffer);
                vkWaitForFences(device->gpu.logicalHandle, 1, &fence, VK_TRUE, UINT64_MAX);
            }
        }
        data->swapChainImageToInFlightFrameMap[data->activeSwapChainImageIndex] = data->activeFrameInFlightIndex;
        for (size_t it = 0; it < se_sbuffer_size(activeFrameCommandBuffers); it++)
        {
            se_vk_command_buffer_destroy(activeFrameCommandBuffers[it]);
        }
        se_sbuffer_set_size(activeFrameCommandBuffers, 0);
    }
    //
    // Remove flag
    //
    device->flags &= ~((uint64_t)SE_VK_DEVICE_HAS_SUBMITTED_BUFFERS);
}

void se_vk_device_end_frame(SeRenderObject* _device)
{
    SeVkRenderDevice* device = (SeVkRenderDevice*)_device;
    SeVkPerImageInFlightData* currentInFlightData = &device->inFlightData.inFlightData[device->inFlightData.activeFrameInFlightIndex];
    se_assert(device->flags & SE_VK_DEVICE_HAS_SUBMITTED_BUFFERS && "You must submit some commands between being_frame and end_frame");
    
    SeCommandBufferRequestInfo requestInfo = (SeCommandBufferRequestInfo)
    {
        (SeRenderObject*)device,
        SE_USAGE_TRANSFER,
    };
    SeRenderObject* commandBuffer = se_vk_command_buffer_request(&requestInfo);
    SeRenderObject* swapChainTexture = device->swapChain.textures[device->inFlightData.activeSwapChainImageIndex];
    se_vk_command_buffer_transition_image_layout(commandBuffer, swapChainTexture, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    se_vk_command_buffer_submit(commandBuffer);

    VkSwapchainKHR swapChains[] = { device->swapChain.handle, };
    VkSemaphore presentWaitSemaphores[] =
    {
        se_vk_command_buffer_get_semaphore(currentInFlightData->commandBuffers[se_sbuffer_size(currentInFlightData->commandBuffers) - 1]),
        currentInFlightData->imageAvailableSemaphore,
    };
    VkPresentInfoKHR presentInfo = (VkPresentInfoKHR)
    {
        .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext              = NULL,
        .waitSemaphoreCount = se_array_size(presentWaitSemaphores),
        .pWaitSemaphores    = presentWaitSemaphores,
        .swapchainCount     = se_array_size(swapChains),
        .pSwapchains        = swapChains,
        .pImageIndices      = &device->inFlightData.activeSwapChainImageIndex,
        .pResults           = NULL,
    };
    se_vk_check(vkQueuePresentKHR(se_vk_device_get_command_queue((SeRenderObject*)device, SE_VK_CMD_QUEUE_PRESENT), &presentInfo));
}

size_t se_vk_device_get_swap_chain_textures_num(SeRenderObject* _device)
{
    SeVkRenderDevice* device = (SeVkRenderDevice*)_device;
    return device->swapChain.numTextures;
}

SeRenderObject* se_vk_device_get_swap_chain_texture(SeRenderObject* _device, size_t index)
{
    SeVkRenderDevice* device = (SeVkRenderDevice*)_device;
    return device->swapChain.textures[index];
}

size_t se_vk_device_get_active_swap_chain_texture_index(SeRenderObject* _device)
{
    SeVkRenderDevice* device = (SeVkRenderDevice*)_device;
    return device->inFlightData.activeSwapChainImageIndex;
}

SeVkMemoryManager* se_vk_device_get_memory_manager(SeRenderObject* _device)
{
    SeVkRenderDevice* device = (SeVkRenderDevice*)_device;
    return &device->memoryManager;
}

VkCommandPool se_vk_device_get_command_pool(SeRenderObject* _device, SeVkCommandQueueFlags flags)
{
    SeVkRenderDevice* device = (SeVkRenderDevice*)_device;
    SeVkCommandQueue* queue = se_vk_gpu_get_command_queue(&device->gpu, flags);
    return queue->commandPoolHandle;
}

VkQueue se_vk_device_get_command_queue(SeRenderObject* _device, SeVkCommandQueueFlags flags)
{
    SeVkRenderDevice* device = (SeVkRenderDevice*)_device;
    SeVkCommandQueue* queue = se_vk_gpu_get_command_queue(&device->gpu, flags);
    return queue->handle;
}

VkDevice se_vk_device_get_logical_handle(SeRenderObject* _device)
{
    SeVkRenderDevice* device = (SeVkRenderDevice*)_device;
    return device->gpu.logicalHandle;
}

SeRenderObject* se_vk_device_get_last_command_buffer(SeRenderObject* _device)
{
    SeVkRenderDevice* device = (SeVkRenderDevice*)_device;
    SeVkPerImageInFlightData* data = &device->inFlightData.inFlightData[device->inFlightData.activeFrameInFlightIndex];
    if (se_sbuffer_size(data->commandBuffers))
    {
        return data->commandBuffers[se_sbuffer_size(data->commandBuffers) - 1];
    }
    return NULL;
}

void se_vk_device_submit_command_buffer(SeRenderObject* _device, VkSubmitInfo* submitInfo, SeRenderObject* buffer, VkQueue queue)
{
    SeVkRenderDevice* device = (SeVkRenderDevice*)_device;
    SeVkPerImageInFlightData* data = &device->inFlightData.inFlightData[device->inFlightData.activeFrameInFlightIndex];
    se_vk_check(vkQueueSubmit(queue, 1, submitInfo, se_vk_command_buffer_get_fence(buffer)));
    se_sbuffer_push(data->commandBuffers, buffer);
    device->flags |= SE_VK_DEVICE_HAS_SUBMITTED_BUFFERS;
}

bool se_vk_device_is_stencil_supported(SeRenderObject* _device)
{
    SeVkRenderDevice* device = (SeVkRenderDevice*)_device;
    return device->gpu.flags & SE_VK_GPU_HAS_STENCIL;
}

VkSampleCountFlags se_vk_device_get_supported_framebuffer_multisample_types(SeRenderObject* _device)
{
    SeVkGpu* gpu = &((SeVkRenderDevice*)_device)->gpu;
    // Is this even correct ?
    return  gpu->deviceProperties_10.limits.framebufferColorSampleCounts &
            gpu->deviceProperties_10.limits.framebufferDepthSampleCounts &
            gpu->deviceProperties_10.limits.framebufferStencilSampleCounts &
            gpu->deviceProperties_10.limits.framebufferNoAttachmentsSampleCounts &
            gpu->deviceProperties_12.framebufferIntegerColorSampleCounts;
}

VkSampleCountFlags se_vk_device_get_supported_image_multisample_types(SeRenderObject* _device)
{
    SeVkGpu* gpu = &((SeVkRenderDevice*)_device)->gpu;
    // Is this even correct ?
    return  gpu->deviceProperties_10.limits.sampledImageColorSampleCounts &
            gpu->deviceProperties_10.limits.sampledImageIntegerSampleCounts &
            gpu->deviceProperties_10.limits.sampledImageDepthSampleCounts &
            gpu->deviceProperties_10.limits.sampledImageStencilSampleCounts &
            gpu->deviceProperties_10.limits.storageImageSampleCounts;
}

VkFormat se_vk_device_get_depth_stencil_format(SeRenderObject* _device)
{
    SeVkGpu* gpu = &((SeVkRenderDevice*)_device)->gpu;
    return gpu->depthStencilFormat;
}

VkFormat se_vk_device_get_swap_chain_format(SeRenderObject* _device)
{
    SeVkSwapChain* swapChain = &((SeVkRenderDevice*)_device)->swapChain;
    return swapChain->surfaceFormat.format;
}

VkExtent2D se_vk_device_get_swap_chain_extent(SeRenderObject* _device)
{
    SeVkSwapChain* swapChain = &((SeVkRenderDevice*)_device)->swapChain;
    return swapChain->extent;
}
