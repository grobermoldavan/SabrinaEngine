
#include "se_vulkan_render_subsystem_device.hpp"
#include "se_vulkan_render_subsystem_utils.hpp"
#include "engine/subsystems/se_application_allocators_subsystem.hpp"
#include "engine/platform.hpp"
#include "engine/allocator_bindings.hpp"
#include "engine/debug.hpp"

#define SE_VK_INVALID_DEVICE_RATING -1.0f

extern SeWindowSubsystemInterface*                  g_windowIface;
extern SeApplicationAllocatorsSubsystemInterface*   g_allocatorsIface;
extern SePlatformInterface*                         g_platformIface;

static size_t g_deviceIndex = 0;

static void se_vk_gpu_fill_required_physical_deivce_features(VkPhysicalDeviceFeatures* features)
{
    features->samplerAnisotropy = VK_TRUE;
}

static float se_vk_gpu_get_device_rating(VkPhysicalDevice device, VkSurfaceKHR surface, SeAllocatorBindings& bindings, VkPhysicalDeviceFeatures* featuresToEnable)
{
    //
    // Get queues
    //
    DynamicArray<VkQueueFamilyProperties> familyProperties = se_vk_utils_get_physical_device_queue_family_properties(device, bindings);
    const uint32_t graphicsQueue = se_vk_utils_pick_graphics_queue(familyProperties);
    const uint32_t presentQueue = se_vk_utils_pick_present_queue(familyProperties, device, surface);
    const uint32_t transferQueue = se_vk_utils_pick_transfer_queue(familyProperties);
    dynamic_array::destroy(familyProperties);
    if ((graphicsQueue == SE_VK_INVALID_QUEUE) || (presentQueue == SE_VK_INVALID_QUEUE) || (transferQueue == SE_VK_INVALID_QUEUE))
    {
        return SE_VK_INVALID_DEVICE_RATING;
    }
    //
    // Check extensions support
    //
    size_t numExtensions;
    const char** extensions = se_vk_utils_get_required_device_extensions(&numExtensions);
    const bool isRequiredExtensionsSupported = se_vk_utils_does_physical_device_supports_required_extensions(device, extensions, numExtensions, bindings);
    if (!isRequiredExtensionsSupported)
    {
        return SE_VK_INVALID_DEVICE_RATING;
    }
    //
    // Check swap chain support
    //
    SeVkSwapChainSupportDetails supportDetails = se_vk_utils_create_swap_chain_support_details(surface, device, bindings);
    const bool isSwapChainSuppoted = dynamic_array::size(supportDetails.formats) && dynamic_array::size(supportDetails.presentModes);
    se_vk_utils_destroy_swap_chain_support_details(supportDetails);
    if (!isSwapChainSuppoted)
    {
        return SE_VK_INVALID_DEVICE_RATING;
    }
    //
    // Get device feature rating
    //
    VkPhysicalDeviceFeatures requiredFeatures{0};
    VkPhysicalDeviceFeatures supportedFeatures{0};
    se_vk_gpu_fill_required_physical_deivce_features(&requiredFeatures);
    vkGetPhysicalDeviceFeatures(device, &supportedFeatures);
    const VkBool32* requiredArray = (VkBool32*)&requiredFeatures;
    const VkBool32* supportedArray = (VkBool32*)&supportedFeatures;
    VkBool32* featuresToEnableArray = (VkBool32*)featuresToEnable;
    const size_t featuresArraySize = sizeof(VkPhysicalDeviceFeatures) / sizeof(VkBool32);
    size_t numRequiredFeatures = 0;
    size_t numSupportedFeatures = 0;
    for (size_t it = 0; it < featuresArraySize; it++)
    {
        if (requiredArray[it]) numRequiredFeatures += 1;
        if (supportedArray[it]) numSupportedFeatures += 1;
        if (requiredArray[it] && supportedArray[it]) featuresToEnableArray[it] = VK_TRUE;
    }
    return numRequiredFeatures == 0 ? 1.0f : ((float)numSupportedFeatures) / ((float)numRequiredFeatures);
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
    se_assert(!"Unsupported command queue requested");
    return nullptr;
}

static VkPhysicalDevice se_vk_gpu_pick_physical_device(VkInstance instance, VkSurfaceKHR surface, SeAllocatorBindings& bindings, VkPhysicalDeviceFeatures* featuresToEnable)
{
    DynamicArray<VkPhysicalDevice> available = se_vk_utils_get_available_physical_devices(instance, bindings);
    const size_t numAvailableDevices = dynamic_array::size(available);
    DynamicArray<float> ratings;
    DynamicArray<VkPhysicalDeviceFeatures> features;
    dynamic_array::construct(ratings, bindings, numAvailableDevices);
    dynamic_array::construct(features, bindings, numAvailableDevices);
    for (size_t it = 0; it < numAvailableDevices; it++)
    {
        dynamic_array::push(features, { });
        const float rating = se_vk_gpu_get_device_rating(available[it], surface, bindings, &features[it]);
        dynamic_array::push(ratings, rating);
    }
    float bestRating = SE_VK_INVALID_DEVICE_RATING;
    size_t bestDeviceIndex = 0;
    for (size_t it = 0; it < numAvailableDevices; it++)
    {
        if (ratings[it] > bestRating)
        {
            bestRating = ratings[it];
            bestDeviceIndex = it;
        }
    }
    se_assert_msg(bestRating != SE_VK_INVALID_DEVICE_RATING, "Unable to pick physical device");
    VkPhysicalDevice device = available[bestDeviceIndex];
    *featuresToEnable = features[bestDeviceIndex];
    dynamic_array::destroy(features);
    dynamic_array::destroy(ratings);
    dynamic_array::destroy(available);
    return device;
}

static void se_vk_device_swap_chain_create(SeVkDevice* device, SeWindowHandle window, bool allocateNewTextures)
{
    SeVkMemoryManager* memoryManager = &device->memoryManager;
    VkAllocationCallbacks* callbacks = se_vk_memory_manager_get_callbacks(memoryManager);
    {
        const uint32_t windowWidth = g_windowIface->get_width(window);
        const uint32_t windowHeight = g_windowIface->get_height(window);;
        SeVkSwapChainSupportDetails supportDetails = se_vk_utils_create_swap_chain_support_details(device->surface, device->gpu.physicalHandle, *memoryManager->cpu_frameAllocator);
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
        VkSwapchainCreateInfoKHR swapChainCreateInfo
        {
            .sType                  = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .pNext                  = nullptr,
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
        se_vk_device_fill_sharing_mode
        (
            device,
            SE_VK_CMD_QUEUE_GRAPHICS | SE_VK_CMD_QUEUE_PRESENT,
            &swapChainCreateInfo.queueFamilyIndexCount,
            queueFamiliesIndices,
            &swapChainCreateInfo.imageSharingMode
        );
        se_vk_check(vkCreateSwapchainKHR(device->gpu.logicalHandle, &swapChainCreateInfo, callbacks, &device->swapChain.handle));
        device->swapChain.surfaceFormat = surfaceFormat;
        device->swapChain.extent = extent;
        se_vk_utils_destroy_swap_chain_support_details(supportDetails);
    }
    {
        uint32_t swapChainImageCount;
        se_vk_check(vkGetSwapchainImagesKHR(device->gpu.logicalHandle, device->swapChain.handle, &swapChainImageCount, nullptr));
        se_assert(swapChainImageCount < SE_VK_MAX_SWAP_CHAIN_IMAGES);
        se_assert(allocateNewTextures || swapChainImageCount == device->swapChain.numTextures);
        DynamicArray<VkImage> swapChainImageHandles;
        dynamic_array::construct(swapChainImageHandles, *memoryManager->cpu_frameAllocator, swapChainImageCount);
        dynamic_array::force_set_size(swapChainImageHandles, swapChainImageCount);
        // It seems like this vkGetSwapchainImagesKHR call leaks memory
        se_vk_check(vkGetSwapchainImagesKHR(device->gpu.logicalHandle, device->swapChain.handle, &swapChainImageCount, dynamic_array::raw(swapChainImageHandles)));
        for (uint32_t it = 0; it < swapChainImageCount; it++)
        {
            VkImageView view = VK_NULL_HANDLE;
            VkImageViewCreateInfo imageViewCreateInfo
            {
                .sType              = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .pNext              = nullptr,
                .flags              = 0,
                .image              = swapChainImageHandles[it],
                .viewType           = VK_IMAGE_VIEW_TYPE_2D,
                .format             = device->swapChain.surfaceFormat.format,
                .components         =
                {
                    .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .a = VK_COMPONENT_SWIZZLE_IDENTITY
                },
                .subresourceRange   =
                {
                    .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel   = 0,
                    .levelCount     = 1,
                    .baseArrayLayer = 0,
                    .layerCount     = 1
                }
            };
            se_vk_check(vkCreateImageView(device->gpu.logicalHandle, &imageViewCreateInfo, callbacks, &view));
            device->swapChain.images[it] =
            {
                .handle = swapChainImageHandles[it],
                .view = view,
            };
        }
        device->swapChain.numTextures = swapChainImageCount;
        dynamic_array::destroy(swapChainImageHandles);
    }
    {
        for (size_t it = 0; it < device->swapChain.numTextures; it++)
        {
            SeVkSwapChainImage* image = &device->swapChain.images[it];
            if (allocateNewTextures)
            {
                device->swapChain.textures[it] = object_pool::take(se_vk_memory_manager_get_pool<SeVkTexture>(memoryManager));
                se_vk_texture_construct_from_swap_chain(device->swapChain.textures[it], device, &device->swapChain.extent, image->handle, image->view, device->swapChain.surfaceFormat.format);
            }
            else
            {
                se_vk_texture_destroy(device->swapChain.textures[it]);
                se_vk_texture_construct_from_swap_chain(device->swapChain.textures[it], device, &device->swapChain.extent, image->handle, image->view, device->swapChain.surfaceFormat.format);
            }
        }
    }
}

static void se_vk_device_swap_chain_destroy(SeVkDevice* device, bool deallocateTextures)
{
    for (size_t it = 0; it < device->swapChain.numTextures; it++)
    {
        if (deallocateTextures)
        {
            se_vk_texture_destroy(device->swapChain.textures[it]);
            object_pool::release(se_vk_memory_manager_get_pool<SeVkTexture>(&device->memoryManager), device->swapChain.textures[it]);
        }
        else
        {
            se_vk_texture_destroy(device->swapChain.textures[it]);
        }
    }
    VkAllocationCallbacks* callbacks = se_vk_memory_manager_get_callbacks(&device->memoryManager);
    for (size_t it = 0; it < device->swapChain.numTextures; it++)
        vkDestroyImageView(device->gpu.logicalHandle, device->swapChain.images[it].view, callbacks);
    vkDestroySwapchainKHR(device->gpu.logicalHandle, device->swapChain.handle, callbacks);
}

static void se_vk_device_handle_window_resized(SeWindowHandle window, void* userData)
{
    // SeVkDevice* device = (SeVkDevice*)userData;
    // SeVkMemoryManager* memoryManager = &device->memoryManager;

    se_assert(!"todo : is this really needed?");
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

SeDeviceHandle se_vk_device_create(SeDeviceInfo* deviceInfo)
{
    SeAllocatorBindings* allocator = g_allocatorsIface->persistentAllocator;
    SeVkDevice* device = (SeVkDevice*)allocator->alloc(allocator->allocator, sizeof(SeVkDevice), se_default_alignment, se_alloc_tag);
    device->object = { SE_VK_TYPE_DEVICE, g_deviceIndex++ };
    const size_t NUM_FRAMES = 3;
    //
    // Memory manager
    //
    {
        SeVkMemoryManagerCreateInfo createInfo =
        {
            .persistentAllocator    = g_allocatorsIface->persistentAllocator,
            .frameAllocator         = g_allocatorsIface->frameAllocator,
            .platform               = g_platformIface,
            .numFrames              = NUM_FRAMES,
        };
        se_vk_memory_manager_construct(&device->memoryManager, &createInfo);
        se_vk_memory_manager_create_pool<SeVkCommandBuffer>(&device->memoryManager);
        se_vk_memory_manager_create_pool<SeVkFramebuffer>(&device->memoryManager);
        se_vk_memory_manager_create_pool<SeVkMemoryBuffer>(&device->memoryManager);
        se_vk_memory_manager_create_pool<SeVkPipeline>(&device->memoryManager);
        se_vk_memory_manager_create_pool<SeVkProgram>(&device->memoryManager);
        se_vk_memory_manager_create_pool<SeVkRenderPass>(&device->memoryManager);
        se_vk_memory_manager_create_pool<SeVkSampler>(&device->memoryManager);
        se_vk_memory_manager_create_pool<SeVkTexture>(&device->memoryManager);

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
        DynamicArray<VkLayerProperties> availableValidationLayers = se_vk_utils_get_available_validation_layers(*device->memoryManager.cpu_frameAllocator);
        for (size_t requiredIt = 0; requiredIt < numValidationLayers; requiredIt++)
        {
            const char* requiredLayerName = validationLayers[requiredIt];
            bool isFound = false;
            for (auto it : availableValidationLayers)
            {
                const char* availableLayerName = iter::value(it).layerName;
                if (strcmp(availableLayerName, requiredLayerName) == 0)
                {
                    isFound = true;
                    break;
                }
            }
            se_assert(isFound && "Required validation layer is not supported");
        }
        dynamic_array::destroy(availableValidationLayers);
#endif
        //
        // Check that all instance extensions are supported
        //
        size_t numInstanceExtensions = 0;
        const char** instanceExtensions = se_vk_utils_get_required_instance_extensions(&numInstanceExtensions);
        DynamicArray<VkExtensionProperties> availableInstanceExtensions = se_vk_utils_get_available_instance_extensions(*device->memoryManager.cpu_frameAllocator);
        for (size_t requiredIt = 0; requiredIt < numInstanceExtensions; requiredIt++)
        {
            const char* requiredExtensionName = instanceExtensions[requiredIt];
            bool isFound = false;
            for (auto it : availableInstanceExtensions)
            {
                const char* availableExtensionName = iter::value(it).extensionName;
                if (strcmp(availableExtensionName, requiredExtensionName) == 0)
                {
                    isFound = true;
                    break;
                }
            }
            se_assert(isFound && "Required instance extension is not supported");
        }
        dynamic_array::destroy(availableInstanceExtensions);
        VkApplicationInfo applicationInfo =
        {
            .sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pNext              = nullptr,
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
        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = se_vk_utils_get_debug_messenger_create_info(se_vk_debug_callback, nullptr);
        VkInstanceCreateInfo instanceCreateInfo =
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
        VkInstanceCreateInfo instanceCreateInfo =
        {
            .sType                      = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pNext                      = nullptr,
            .flags                      = 0,
            .pApplicationInfo           = &applicationInfo,
            .enabledLayerCount          = 0,
            .ppEnabledLayerNames        = nullptr,
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
        HWND windowHandle = (HWND)g_windowIface->get_native_handle(*deviceInfo->window);
        VkWin32SurfaceCreateInfoKHR surfaceCreateInfo =
        {
            .sType      = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
            .pNext      = nullptr,
            .flags      = 0,
            .hinstance  = GetModuleHandle(nullptr),
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
        VkPhysicalDeviceFeatures featuresToEnable = {0};
        device->gpu.physicalHandle = se_vk_gpu_pick_physical_device(device->instance, device->surface, *g_allocatorsIface->frameAllocator, &featuresToEnable);
        device->gpu.enabledFeatures = featuresToEnable;
        //
        // Get command queue infos
        //
        DynamicArray<VkQueueFamilyProperties> familyProperties = se_vk_utils_get_physical_device_queue_family_properties(device->gpu.physicalHandle, *g_allocatorsIface->frameAllocator);
        uint32_t queuesFamilyIndices[] =
        {
            se_vk_utils_pick_graphics_queue(familyProperties),
            se_vk_utils_pick_present_queue(familyProperties, device->gpu.physicalHandle, device->surface),
            se_vk_utils_pick_transfer_queue(familyProperties),
            se_vk_utils_pick_compute_queue(familyProperties),
        };
        DynamicArray<VkDeviceQueueCreateInfo> queueCreateInfos = se_vk_utils_get_queue_create_infos(queuesFamilyIndices, se_array_size(queuesFamilyIndices), *g_allocatorsIface->frameAllocator);
        //
        // Create logical device
        //
        size_t numValidationLayers;
        const char** requiredValidationLayers = se_vk_utils_get_required_validation_layers(&numValidationLayers);
        size_t numDeviceExtensions;
        const char** requiredDeviceExtensions = se_vk_utils_get_required_device_extensions(&numDeviceExtensions);
        VkDeviceCreateInfo logicalDeviceCreateInfo =
        {
            .sType                      = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext                      = nullptr,
            .flags                      = 0,
            .queueCreateInfoCount       = (uint32_t)dynamic_array::size(queueCreateInfos),
            .pQueueCreateInfos          = dynamic_array::raw(queueCreateInfos),
            .enabledLayerCount          = (uint32_t)numValidationLayers,
            .ppEnabledLayerNames        = requiredValidationLayers,
            .enabledExtensionCount      = (uint32_t)numDeviceExtensions,
            .ppEnabledExtensionNames    = requiredDeviceExtensions,
            .pEnabledFeatures           = &featuresToEnable
        };
        se_vk_check(vkCreateDevice(device->gpu.physicalHandle, &logicalDeviceCreateInfo, callbacks, &device->gpu.logicalHandle));
        //
        // Create queues
        //
        for (auto it : queueCreateInfos)
        {
            SeVkCommandQueue* queue = &device->gpu.commandQueues[iter::index(it)];
            queue->queueFamilyIndex = iter::value(it).queueFamilyIndex;
            if (queuesFamilyIndices[0] == queue->queueFamilyIndex) queue->flags |= SE_VK_CMD_QUEUE_GRAPHICS;
            if (queuesFamilyIndices[1] == queue->queueFamilyIndex) queue->flags |= SE_VK_CMD_QUEUE_PRESENT;
            if (queuesFamilyIndices[2] == queue->queueFamilyIndex) queue->flags |= SE_VK_CMD_QUEUE_TRANSFER;
            if (queuesFamilyIndices[3] == queue->queueFamilyIndex) queue->flags |= SE_VK_CMD_QUEUE_COMPUTE;
            vkGetDeviceQueue(device->gpu.logicalHandle, queue->queueFamilyIndex, 0, &queue->handle);
            VkCommandPoolCreateInfo poolCreateInfo = se_vk_utils_command_pool_create_info(queue->queueFamilyIndex, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);
            se_vk_check(vkCreateCommandPool(device->gpu.logicalHandle, &poolCreateInfo, callbacks, &queue->commandPoolHandle));
        }
        dynamic_array::destroy(familyProperties);
        dynamic_array::destroy(queueCreateInfos);
        //
        // Get device info
        //
        bool hasStencil = se_vk_utils_pick_depth_stencil_format(device->gpu.physicalHandle, &device->gpu.depthStencilFormat);
        if (hasStencil) device->gpu.flags |= SE_VK_GPU_HAS_STENCIL;
        vkGetPhysicalDeviceMemoryProperties(device->gpu.physicalHandle, &device->gpu.memoryProperties);
        VkPhysicalDeviceVulkan12Properties deviceProperties_12 =
        {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES,
            .pNext = nullptr
        };
        VkPhysicalDeviceVulkan11Properties deviceProperties_11 =
        {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES,
            .pNext = &deviceProperties_12
        };
        VkPhysicalDeviceProperties2 physcialDeviceProperties2 =
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
    se_vk_memory_manager_set_device(&device->memoryManager, device);
    //
    // Swap chain
    //
    se_vk_device_swap_chain_create(device, *deviceInfo->window, true);
    //
    // Frame manager
    //
    {
        SeVkFrameManagerCreateInfo frameManagerCreateInfo =
        {
            .device = device,
            .numFrames = NUM_FRAMES,
        };
        se_vk_frame_manager_construct(&device->frameManager, &frameManagerCreateInfo);
    }
    //
    // Graph
    //
    {
        SeVkGraphInfo graphCreateInfo =
        {
            .device     = device,
            .numFrames  = NUM_FRAMES,
        };
        se_vk_graph_construct(&device->graph, &graphCreateInfo);
    }
    return (SeDeviceHandle)device;
}

void se_vk_device_destroy(SeDeviceHandle _device)
{
    SeVkDevice* device = (SeVkDevice*)_device;
    VkAllocationCallbacks* callbacks = se_vk_memory_manager_get_callbacks(&device->memoryManager);
    vkDeviceWaitIdle(device->gpu.logicalHandle);
    //
    // Destroy all resources
    //
    object_pool::for_each(se_vk_memory_manager_get_pool<SeVkSampler>(&device->memoryManager),
        (object_pool::ForEachCb<SeVkSampler>)[](SeVkSampler* obj) { se_vk_sampler_destroy(obj); });

    object_pool::for_each(se_vk_memory_manager_get_pool<SeVkMemoryBuffer>(&device->memoryManager),
        (object_pool::ForEachCb<SeVkMemoryBuffer>)[](SeVkMemoryBuffer* obj) { se_vk_memory_buffer_destroy(obj); });

    object_pool::for_each(se_vk_memory_manager_get_pool<SeVkFramebuffer>(&device->memoryManager),
        (object_pool::ForEachCb<SeVkFramebuffer>)[](SeVkFramebuffer* obj) { se_vk_framebuffer_destroy(obj); });

    object_pool::for_each(se_vk_memory_manager_get_pool<SeVkTexture>(&device->memoryManager),
        (object_pool::ForEachCb<SeVkTexture>)[](SeVkTexture* obj) { se_vk_texture_destroy(obj); });

    object_pool::for_each(se_vk_memory_manager_get_pool<SeVkPipeline>(&device->memoryManager),
        (object_pool::ForEachCb<SeVkPipeline>)[](SeVkPipeline* obj) { se_vk_pipeline_destroy(obj); });

    object_pool::for_each(se_vk_memory_manager_get_pool<SeVkRenderPass>(&device->memoryManager),
        (object_pool::ForEachCb<SeVkRenderPass>)[](SeVkRenderPass* obj) { se_vk_render_pass_destroy(obj); });

    object_pool::for_each(se_vk_memory_manager_get_pool<SeVkProgram>(&device->memoryManager),
        (object_pool::ForEachCb<SeVkProgram>)[](SeVkProgram* obj) { se_vk_program_destroy(obj); });
        
    object_pool::for_each(se_vk_memory_manager_get_pool<SeVkCommandBuffer>(&device->memoryManager),
        (object_pool::ForEachCb<SeVkCommandBuffer>)[](SeVkCommandBuffer* obj) { se_vk_command_buffer_destroy(obj); });
    //
    // Graph
    //
    se_vk_graph_destroy(&device->graph);
    //
    // Frame manager
    //
    se_vk_frame_manager_destroy(&device->frameManager);
    //
    // Swap Chain
    //
    se_vk_device_swap_chain_destroy(device, true);
    //
    // Gpu
    //
    for (size_t it = 0; it < SE_VK_MAX_UNIQUE_COMMAND_QUEUES; it++)
    {
        if (device->gpu.commandQueues[it].commandPoolHandle == VK_NULL_HANDLE) break;
        vkDestroyCommandPool(device->gpu.logicalHandle, device->gpu.commandQueues[it].commandPoolHandle, callbacks);
    }
    se_vk_memory_manager_free_gpu_memory(&device->memoryManager);
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
    se_vk_memory_manager_free_cpu_memory(&device->memoryManager);
    device->memoryManager.cpu_persistentAllocator->dealloc(device->memoryManager.cpu_persistentAllocator->allocator, device, sizeof(SeVkDevice));
}

void se_vk_device_begin_frame(SeDeviceHandle _device)
{
    SeVkDevice* device = (SeVkDevice*)_device;
    se_vk_frame_manager_advance(&device->frameManager);
    se_vk_graph_begin_frame(&device->graph);
}

void se_vk_device_end_frame(SeDeviceHandle _device)
{
    SeVkDevice* device = (SeVkDevice*)_device;
    se_vk_graph_end_frame(&device->graph);
}

SeVkFlags se_vk_device_get_supported_sampling_types(SeVkDevice* device)
{
    VkSampleCountFlags vkSampleFlags;
    if (device->gpu.flags & SE_VK_GPU_HAS_STENCIL)
        vkSampleFlags = device->gpu.deviceProperties_10.limits.sampledImageColorSampleCounts &
                        device->gpu.deviceProperties_10.limits.sampledImageIntegerSampleCounts &
                        device->gpu.deviceProperties_10.limits.sampledImageDepthSampleCounts &
                        device->gpu.deviceProperties_10.limits.sampledImageStencilSampleCounts;
    else
        vkSampleFlags = device->gpu.deviceProperties_10.limits.sampledImageColorSampleCounts &
                        device->gpu.deviceProperties_10.limits.sampledImageIntegerSampleCounts &
                        device->gpu.deviceProperties_10.limits.sampledImageDepthSampleCounts;
    return (SeVkFlags)vkSampleFlags;
}

VkSampleCountFlags se_vk_device_get_supported_framebuffer_multisample_types(SeVkDevice* device)
{
    // Is this even correct ?
    return  device->gpu.deviceProperties_10.limits.framebufferColorSampleCounts &
            device->gpu.deviceProperties_10.limits.framebufferDepthSampleCounts &
            device->gpu.deviceProperties_10.limits.framebufferStencilSampleCounts &
            device->gpu.deviceProperties_10.limits.framebufferNoAttachmentsSampleCounts &
            device->gpu.deviceProperties_12.framebufferIntegerColorSampleCounts;
}

VkSampleCountFlags se_vk_device_get_supported_image_multisample_types(SeVkDevice* device)
{
    // Is this even correct ?
    return  device->gpu.deviceProperties_10.limits.sampledImageColorSampleCounts &
            device->gpu.deviceProperties_10.limits.sampledImageIntegerSampleCounts &
            device->gpu.deviceProperties_10.limits.sampledImageDepthSampleCounts &
            device->gpu.deviceProperties_10.limits.sampledImageStencilSampleCounts &
            device->gpu.deviceProperties_10.limits.storageImageSampleCounts;
}

void se_vk_device_fill_sharing_mode(SeVkDevice* device, SeVkCommandQueueFlags flags, uint32_t* numQueues, uint32_t* queueFamilyIndices, VkSharingMode* sharingMode)
{
    const uint32_t queueIndices[] =
    {
        se_vk_gpu_get_command_queue(&device->gpu, SE_VK_CMD_QUEUE_GRAPHICS)->queueFamilyIndex,
        se_vk_gpu_get_command_queue(&device->gpu, SE_VK_CMD_QUEUE_PRESENT)->queueFamilyIndex,
        se_vk_gpu_get_command_queue(&device->gpu, SE_VK_CMD_QUEUE_TRANSFER)->queueFamilyIndex,
        se_vk_gpu_get_command_queue(&device->gpu, SE_VK_CMD_QUEUE_COMPUTE)->queueFamilyIndex,
    };
    se_assert_msg(se_array_size(queueIndices) == SE_VK_MAX_UNIQUE_COMMAND_QUEUES, "Don't forget to support new queues here");
    uint32_t uniqueQueues[SE_VK_MAX_UNIQUE_COMMAND_QUEUES];
    uint32_t numUniqueQueues = 0;
    for (uint32_t it = 0; it < SE_VK_MAX_UNIQUE_COMMAND_QUEUES; it++)
    {
        if (!(flags & it)) continue;
        bool isFound = false;
        for (uint32_t uniqueIt = 0; uniqueIt < numUniqueQueues; uniqueIt++)
        {
            if (uniqueQueues[uniqueIt] == queueIndices[it])
            {
                isFound = true;
                break;
            }
        }
        if (!isFound)
            uniqueQueues[numUniqueQueues++] = queueIndices[it];
    }
    if (numUniqueQueues == 1)
    {
        *sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        *numQueues = 0;
    }
    else
    {
        *sharingMode = VK_SHARING_MODE_CONCURRENT;
        *numQueues = numUniqueQueues;
        memcpy(queueFamilyIndices, uniqueQueues, sizeof(uint32_t) * numUniqueQueues);
    }
}
