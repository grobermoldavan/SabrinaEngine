
#include "se_vulkan_device.hpp"
#include "se_vulkan_utils.hpp"

#define SE_VK_INVALID_DEVICE_RATING -1.0f

size_t g_deviceIndex = 0;

void se_vk_gpu_fill_required_physical_deivce_features(VkPhysicalDeviceFeatures* features)
{
    features->samplerAnisotropy = VK_TRUE;
}

float se_vk_gpu_get_device_rating(VkPhysicalDevice device, VkSurfaceKHR surface, const SeAllocatorBindings& bindings, VkPhysicalDeviceFeatures* featuresToEnable)
{
    //
    // Get queues
    //
    SeDynamicArray<VkQueueFamilyProperties> familyProperties = se_vk_utils_get_physical_device_queue_family_properties(device, bindings);
    const uint32_t graphicsQueue = se_vk_utils_pick_graphics_queue(familyProperties);
    const uint32_t presentQueue = se_vk_utils_pick_present_queue(familyProperties, device, surface);
    const uint32_t transferQueue = se_vk_utils_pick_transfer_queue(familyProperties);
    se_dynamic_array_destroy(familyProperties);
    if ((graphicsQueue == SE_VK_INVALID_QUEUE) || (presentQueue == SE_VK_INVALID_QUEUE) || (transferQueue == SE_VK_INVALID_QUEUE))
    {
        return SE_VK_INVALID_DEVICE_RATING;
    }
    //
    // Check extensions support
    //
    size_t numExtensions;
    const char** const extensions = se_vk_utils_get_required_device_extensions(&numExtensions);
    const bool isRequiredExtensionsSupported = se_vk_utils_does_physical_device_supports_required_extensions(device, extensions, numExtensions, bindings);
    if (!isRequiredExtensionsSupported)
    {
        return SE_VK_INVALID_DEVICE_RATING;
    }
    //
    // Check swap chain support
    //
    SeVkSwapChainSupportDetails supportDetails = se_vk_utils_create_swap_chain_support_details(surface, device, bindings);
    const bool isSwapChainSuppoted = se_dynamic_array_size(supportDetails.formats) && se_dynamic_array_size(supportDetails.presentModes);
    se_vk_utils_destroy_swap_chain_support_details(supportDetails);
    if (!isSwapChainSuppoted)
    {
        return SE_VK_INVALID_DEVICE_RATING;
    }
    //
    // Get device feature rating
    //
    VkPhysicalDeviceFeatures requiredFeatures = { };
    VkPhysicalDeviceFeatures supportedFeatures = { };
    se_vk_gpu_fill_required_physical_deivce_features(&requiredFeatures);
    vkGetPhysicalDeviceFeatures(device, &supportedFeatures);
    const VkBool32* const requiredArray = (VkBool32*)&requiredFeatures;
    const VkBool32* const supportedArray = (VkBool32*)&supportedFeatures;
    VkBool32* const featuresToEnableArray = (VkBool32*)featuresToEnable;
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

const SeVkCommandQueue* se_vk_gpu_get_command_queue(const SeVkGpu* gpu, SeVkCommandQueueFlags flags)
{
    for (size_t it = 0; it < SeVkConfig::MAX_UNIQUE_COMMAND_QUEUES; it++)
    {
        const SeVkCommandQueue* const queue = &gpu->commandQueues[it];
        if (queue->flags & flags)
        {
            return queue;
        }
    }
    se_assert(!"Unsupported command queue requested");
    return nullptr;
}

VkPhysicalDevice se_vk_gpu_pick_physical_device(VkInstance instance, VkSurfaceKHR surface, const SeAllocatorBindings& bindings, VkPhysicalDeviceFeatures* featuresToEnable)
{
    SeDynamicArray<VkPhysicalDevice> available = se_vk_utils_get_available_physical_devices(instance, bindings);
    const size_t numAvailableDevices = se_dynamic_array_size(available);
    SeDynamicArray<float> ratings;
    SeDynamicArray<VkPhysicalDeviceFeatures> features;
    se_dynamic_array_construct(ratings, bindings, numAvailableDevices);
    se_dynamic_array_construct(features, bindings, numAvailableDevices);
    for (size_t it = 0; it < numAvailableDevices; it++)
    {
        se_dynamic_array_push(features, { });
        const float rating = se_vk_gpu_get_device_rating(available[it], surface, bindings, &features[it]);
        se_dynamic_array_push(ratings, rating);
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
    se_dynamic_array_destroy(features);
    se_dynamic_array_destroy(ratings);
    se_dynamic_array_destroy(available);
    return device;
}

void se_vk_device_swap_chain_create(SeVkDevice* device, uint32_t width, uint32_t height)
{
    const SeAllocatorBindings frameAllocator = se_allocator_frame();
    SeVkMemoryManager* const memoryManager = &device->memoryManager;
    const VkAllocationCallbacks* const callbacks = se_vk_memory_manager_get_callbacks(memoryManager);
    {
        SeVkSwapChainSupportDetails supportDetails = se_vk_utils_create_swap_chain_support_details(device->surface, device->gpu.physicalHandle, frameAllocator);
        VkSurfaceFormatKHR  surfaceFormat   = se_vk_utils_choose_swap_chain_surface_format(supportDetails.formats);
        VkPresentModeKHR    presentMode     = se_vk_utils_choose_swap_chain_surface_present_mode(supportDetails.presentModes);
        VkExtent2D          extent          = se_vk_utils_choose_swap_chain_extent(width, height, &supportDetails.capabilities);
        // @NOTE :  supportDetails.capabilities.maxImageCount == 0 means unlimited amount of images in swapchain
        uint32_t imageCount = supportDetails.capabilities.minImageCount + 1;
        if (supportDetails.capabilities.maxImageCount != 0 && imageCount > supportDetails.capabilities.maxImageCount)
        {
            imageCount = supportDetails.capabilities.maxImageCount;
        }
        uint32_t queueFamiliesIndices[SeVkConfig::MAX_UNIQUE_COMMAND_QUEUES] = { };
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
        se_assert(swapChainImageCount < SeVkConfig::MAX_SWAP_CHAIN_IMAGES);
        SeDynamicArray<VkImage> swapChainImageHandles;
        se_dynamic_array_construct(swapChainImageHandles, frameAllocator, swapChainImageCount);
        se_dynamic_array_force_set_size(swapChainImageHandles, swapChainImageCount);
        // It seems like this vkGetSwapchainImagesKHR call leaks memory
        se_vk_check(vkGetSwapchainImagesKHR(device->gpu.logicalHandle, device->swapChain.handle, &swapChainImageCount, se_dynamic_array_raw(swapChainImageHandles)));
        for (uint32_t it = 0; it < swapChainImageCount; it++)
        {
            VkImageView view = VK_NULL_HANDLE;
            const VkImageViewCreateInfo imageViewCreateInfo
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
        se_dynamic_array_destroy(swapChainImageHandles);
    }
    {
        for (size_t it = 0; it < device->swapChain.numTextures; it++)
        {
            SeVkSwapChainImage* const image = &device->swapChain.images[it];
            SeObjectPool<SeVkTexture>& pool = se_vk_memory_manager_get_pool<SeVkTexture>(memoryManager);
            SeVkTexture* const texture = se_object_pool_take(pool);
            se_vk_texture_construct_from_swap_chain(texture, device, &device->swapChain.extent, image->handle, image->view, device->swapChain.surfaceFormat.format);
            device->swapChain.textures[it] = se_object_pool_to_ref(pool, texture);
        }
    }
}

void se_vk_device_swap_chain_destroy(SeVkDevice* device)
{
    for (size_t it = 0; it < device->swapChain.numTextures; it++)
    {
        SeVkTexture* const tex = *device->swapChain.textures[it];
        se_vk_texture_destroy(tex);
        se_object_pool_release(se_vk_memory_manager_get_pool<SeVkTexture>(&device->memoryManager), tex);
    }
    const VkAllocationCallbacks* const callbacks = se_vk_memory_manager_get_callbacks(&device->memoryManager);
    for (size_t it = 0; it < device->swapChain.numTextures; it++)
        vkDestroyImageView(device->gpu.logicalHandle, device->swapChain.images[it].view, callbacks);
    vkDestroySwapchainKHR(device->gpu.logicalHandle, device->swapChain.handle, callbacks);
}

VKAPI_ATTR VkBool32 VKAPI_CALL se_vk_debug_callback(
                                            VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                            VkDebugUtilsMessageTypeFlagsEXT messageType,
                                            const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                            void* pUserData)
{
    se_dbg_message("Debug callback : {}\n", pCallbackData->pMessage);
    se_assert(false);
    return VK_FALSE;
}

SeVkDevice* se_vk_device_create(const SeSettings& settings, void* nativeWindowHandle)
{
    const SeAllocatorBindings persistentAllocator = se_allocator_persistent();
    const SeAllocatorBindings frameAllocator = se_allocator_frame();

    SeVkDevice* const device = (SeVkDevice*)persistentAllocator.alloc(persistentAllocator.allocator, sizeof(SeVkDevice), se_default_alignment, se_alloc_tag);
    device->object = { SeVkObject::Type::DEVICE, 0, g_deviceIndex++ };
    //
    // Memory manager
    //
    se_vk_memory_manager_construct(&device->memoryManager);
    const VkAllocationCallbacks* const callbacks = se_vk_memory_manager_get_callbacks(&device->memoryManager);
    //
    // Instance and debug messenger
    //
    {
#if defined(SE_DEBUG)
        //
        // Check that all validation layers are supported
        //
        size_t numValidationLayers = 0;
        const char** const validationLayers = se_vk_utils_get_required_validation_layers(&numValidationLayers);
        SeDynamicArray<VkLayerProperties> availableValidationLayers = se_vk_utils_get_available_validation_layers(frameAllocator);
        for (size_t requiredIt = 0; requiredIt < numValidationLayers; requiredIt++)
        {
            const char* const requiredLayerName = validationLayers[requiredIt];
            bool isFound = false;
            for (auto it : availableValidationLayers)
            {
                const char* const availableLayerName = se_iterator_value(it).layerName;
                if (strcmp(availableLayerName, requiredLayerName) == 0)
                {
                    isFound = true;
                    break;
                }
            }
            se_assert_msg(isFound, "Required validation layer is not supported");
        }
        se_dynamic_array_destroy(availableValidationLayers);
#endif
        //
        // Check that all instance extensions are supported
        //
        size_t numInstanceExtensions = 0;
        const char** const instanceExtensions = se_vk_utils_get_required_instance_extensions(&numInstanceExtensions);
        SeDynamicArray<VkExtensionProperties> availableInstanceExtensions = se_vk_utils_get_available_instance_extensions(frameAllocator);
        for (size_t requiredIt = 0; requiredIt < numInstanceExtensions; requiredIt++)
        {
            const char* const requiredExtensionName = instanceExtensions[requiredIt];
            bool isFound = false;
            for (auto it : availableInstanceExtensions)
            {
                const char* const availableExtensionName = se_iterator_value(it).extensionName;
                if (strcmp(availableExtensionName, requiredExtensionName) == 0)
                {
                    isFound = true;
                    break;
                }
            }
            se_assert_msg(isFound, "Required instance extension is not supported");
        }
        se_dynamic_array_destroy(availableInstanceExtensions);
        const VkApplicationInfo applicationInfo =
        {
            .sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pNext              = nullptr,
            .pApplicationName   = settings.applicationName,
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
        const VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = se_vk_utils_get_debug_messenger_create_info(se_vk_debug_callback, nullptr);
        const VkInstanceCreateInfo instanceCreateInfo =
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
        const VkInstanceCreateInfo instanceCreateInfo =
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
        const HWND windowHandle = (HWND)nativeWindowHandle;
        const VkWin32SurfaceCreateInfoKHR surfaceCreateInfo =
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
        VkPhysicalDeviceFeatures featuresToEnable = { };
        device->gpu.physicalHandle = se_vk_gpu_pick_physical_device(device->instance, device->surface, frameAllocator, &featuresToEnable);
        device->gpu.enabledFeatures = featuresToEnable;
        //
        // Get command queue infos
        //
        SeDynamicArray<VkQueueFamilyProperties> familyProperties = se_vk_utils_get_physical_device_queue_family_properties(device->gpu.physicalHandle, frameAllocator);
        const uint32_t queuesFamilyIndices[] =
        {
            se_vk_utils_pick_graphics_queue(familyProperties),
            se_vk_utils_pick_present_queue(familyProperties, device->gpu.physicalHandle, device->surface),
            se_vk_utils_pick_transfer_queue(familyProperties),
            se_vk_utils_pick_compute_queue(familyProperties),
        };
        SeDynamicArray<VkDeviceQueueCreateInfo> queueCreateInfos = se_vk_utils_get_queue_create_infos(queuesFamilyIndices, se_array_size(queuesFamilyIndices), frameAllocator);
        //
        // Create logical device
        //
        size_t numValidationLayers;
        const char** const requiredValidationLayers = se_vk_utils_get_required_validation_layers(&numValidationLayers);
        size_t numDeviceExtensions;
        const char** const requiredDeviceExtensions = se_vk_utils_get_required_device_extensions(&numDeviceExtensions);
        const VkDeviceCreateInfo logicalDeviceCreateInfo =
        {
            .sType                      = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext                      = nullptr,
            .flags                      = 0,
            .queueCreateInfoCount       = (uint32_t)se_dynamic_array_size(queueCreateInfos),
            .pQueueCreateInfos          = se_dynamic_array_raw(queueCreateInfos),
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
            SeVkCommandQueue* const queue = &device->gpu.commandQueues[se_iterator_index(it)];
            queue->queueFamilyIndex = se_iterator_value(it).queueFamilyIndex;
            if (queuesFamilyIndices[0] == queue->queueFamilyIndex) queue->flags |= SE_VK_CMD_QUEUE_GRAPHICS;
            if (queuesFamilyIndices[1] == queue->queueFamilyIndex) queue->flags |= SE_VK_CMD_QUEUE_PRESENT;
            if (queuesFamilyIndices[2] == queue->queueFamilyIndex) queue->flags |= SE_VK_CMD_QUEUE_TRANSFER;
            if (queuesFamilyIndices[3] == queue->queueFamilyIndex) queue->flags |= SE_VK_CMD_QUEUE_COMPUTE;
            vkGetDeviceQueue(device->gpu.logicalHandle, queue->queueFamilyIndex, 0, &queue->handle);
            const VkCommandPoolCreateInfo poolCreateInfo = se_vk_utils_command_pool_create_info(queue->queueFamilyIndex, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);
            se_vk_check(vkCreateCommandPool(device->gpu.logicalHandle, &poolCreateInfo, callbacks, &queue->commandPoolHandle));
        }
        se_dynamic_array_destroy(familyProperties);
        se_dynamic_array_destroy(queueCreateInfos);
        //
        // Get device info
        //
        const bool hasStencil = se_vk_utils_pick_depth_stencil_format(device->gpu.physicalHandle, &device->gpu.depthStencilFormat);
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
            .properties = { },
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
    se_vk_device_swap_chain_create(device, se_win_get_width(), se_win_get_height());
    //
    // Frame manager
    //
    {
        const SeVkFrameManagerCreateInfo frameManagerCreateInfo =
        {
            .device = device,
        };
        se_vk_frame_manager_construct(&device->frameManager, &frameManagerCreateInfo);
    }
    //
    // Graph
    //
    {
        const SeVkGraphInfo graphCreateInfo =
        {
            .device = device,
        };
        se_vk_graph_construct(&device->graph, &graphCreateInfo);
    }
    //
    // Graveyard
    //
    se_dynamic_array_construct(device->graveyard.programs, se_allocator_persistent());
    se_dynamic_array_construct(device->graveyard.samplers, se_allocator_persistent());
    se_dynamic_array_construct(device->graveyard.buffers, se_allocator_persistent());
    se_dynamic_array_construct(device->graveyard.textures, se_allocator_persistent());
    return device;
}

void se_vk_device_destroy(SeVkDevice* device)
{
    const VkAllocationCallbacks* const callbacks = se_vk_memory_manager_get_callbacks(&device->memoryManager);
    vkDeviceWaitIdle(device->gpu.logicalHandle);
    //
    // Destroy all resources
    //
    for (auto it : se_vk_memory_manager_get_pool<SeVkSampler>(&device->memoryManager))       se_vk_destroy(&se_iterator_value(it));
    for (auto it : se_vk_memory_manager_get_pool<SeVkMemoryBuffer>(&device->memoryManager))  se_vk_destroy(&se_iterator_value(it));
    for (auto it : se_vk_memory_manager_get_pool<SeVkFramebuffer>(&device->memoryManager))   se_vk_destroy(&se_iterator_value(it));
    for (auto it : se_vk_memory_manager_get_pool<SeVkTexture>(&device->memoryManager))       se_vk_destroy(&se_iterator_value(it));
    for (auto it : se_vk_memory_manager_get_pool<SeVkPipeline>(&device->memoryManager))      se_vk_destroy(&se_iterator_value(it));
    for (auto it : se_vk_memory_manager_get_pool<SeVkRenderPass>(&device->memoryManager))    se_vk_destroy(&se_iterator_value(it));
    for (auto it : se_vk_memory_manager_get_pool<SeVkProgram>(&device->memoryManager))       se_vk_destroy(&se_iterator_value(it));
    for (auto it : se_vk_memory_manager_get_pool<SeVkCommandBuffer>(&device->memoryManager)) se_vk_destroy(&se_iterator_value(it));
    //
    // Graveyard
    //
    se_dynamic_array_destroy(device->graveyard.programs);
    se_dynamic_array_destroy(device->graveyard.samplers);
    se_dynamic_array_destroy(device->graveyard.buffers);
    se_dynamic_array_destroy(device->graveyard.textures);
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
    se_vk_device_swap_chain_destroy(device);
    //
    // Gpu
    //
    for (size_t it = 0; it < SeVkConfig::MAX_UNIQUE_COMMAND_QUEUES; it++)
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

    const SeAllocatorBindings allocator = se_allocator_persistent();
    allocator.dealloc(allocator.allocator, device, sizeof(SeVkDevice));
}

inline void se_vk_device_begin_frame(SeVkDevice* device, VkExtent2D extent)
{
    if (!se_compare(device->swapChain.extent, extent))
    {
        vkDeviceWaitIdle(device->gpu.logicalHandle);
        se_vk_device_swap_chain_destroy(device);
        se_vk_device_swap_chain_create(device, extent.width, extent.height);
    }
    se_vk_frame_manager_advance(&device->frameManager);
    se_vk_graph_begin_frame(&device->graph);
}

inline void se_vk_device_end_frame(SeVkDevice* device)
{
    se_vk_graph_end_frame(&device->graph);
}

template<typename Ref>
void se_vk_device_submit_to_graveyard(SeVkDevice* device, Ref ref)
{
    const size_t frameNumber = device->frameManager.frameNumber;
    if      constexpr (std::is_same_v<Ref, SeProgramRef>) se_dynamic_array_push(device->graveyard.programs, { ref, frameNumber });
    else if constexpr (std::is_same_v<Ref, SeSamplerRef>) se_dynamic_array_push(device->graveyard.samplers, { ref, frameNumber });
    else if constexpr (std::is_same_v<Ref, SeBufferRef>)  se_dynamic_array_push(device->graveyard.buffers,  { ref, frameNumber });
    else if constexpr (std::is_same_v<Ref, SeTextureRef>) se_dynamic_array_push(device->graveyard.textures, { ref, frameNumber });
    else static_assert(!"what");
    se_vk_unref(ref)->object.flags |= SeVkObject::Flags::IN_GRAVEYARD;
}

template<typename Ref>
void se_vk_device_update_graveyard_collection(SeVkDevice* device, SeDynamicArray<SeVkGraveyard::Entry<Ref>>& collection)
{
    using VulkanResourceT = SeVkRefToResource<Ref>::Res;
    SeObjectPool<VulkanResourceT>& objectPool = se_vk_memory_manager_get_pool<VulkanResourceT>(&device->memoryManager);
    const SeVkFrameManager* const frameManager = &device->frameManager;
    const VkDevice logicalHandle = device->gpu.logicalHandle;
    for (auto it : collection)
    {
        const auto& value = se_iterator_value(it);
        const SeVkFrame* const frame = ((frameManager->frameNumber - value.frameIndex) < SeVkConfig::NUM_FRAMES_IN_FLIGHT)
            ? se_vk_frame_manager_get_frame(frameManager, value.frameIndex)
            : nullptr;
        const VkFence fence = (*se_dynamic_array_last(frame->commandBuffers))->fence;
        const bool isFinished = !frame || vkGetFenceStatus(logicalHandle, fence) == VK_SUCCESS;
        if (isFinished)
        {
            auto* const object = se_vk_unref_graveyard(value.ref);
            se_vk_destroy(object);
            se_object_pool_release(objectPool, object);
            se_iterator_remove(it);
            se_dbg_message("Destroyed texture");
        }
    }
}

void se_vk_device_update_graveyard(SeVkDevice* device)
{
    se_vk_device_update_graveyard_collection(device, device->graveyard.programs);
    se_vk_device_update_graveyard_collection(device, device->graveyard.samplers);
    se_vk_device_update_graveyard_collection(device, device->graveyard.buffers);
    se_vk_device_update_graveyard_collection(device, device->graveyard.textures);
}

inline VkSampleCountFlags se_vk_device_get_supported_sampling_types(SeVkDevice* device)
{
    if (device->gpu.flags & SE_VK_GPU_HAS_STENCIL)
        return  device->gpu.deviceProperties_10.limits.sampledImageColorSampleCounts &
                device->gpu.deviceProperties_10.limits.sampledImageIntegerSampleCounts &
                device->gpu.deviceProperties_10.limits.sampledImageDepthSampleCounts &
                device->gpu.deviceProperties_10.limits.sampledImageStencilSampleCounts;
    else
        return  device->gpu.deviceProperties_10.limits.sampledImageColorSampleCounts &
                device->gpu.deviceProperties_10.limits.sampledImageIntegerSampleCounts &
                device->gpu.deviceProperties_10.limits.sampledImageDepthSampleCounts;
}

inline VkSampleCountFlags se_vk_device_get_supported_framebuffer_multisample_types(SeVkDevice* device)
{
    // Is this even correct ?
    return  device->gpu.deviceProperties_10.limits.framebufferColorSampleCounts &
            device->gpu.deviceProperties_10.limits.framebufferDepthSampleCounts &
            device->gpu.deviceProperties_10.limits.framebufferStencilSampleCounts &
            device->gpu.deviceProperties_10.limits.framebufferNoAttachmentsSampleCounts &
            device->gpu.deviceProperties_12.framebufferIntegerColorSampleCounts;
}

inline VkSampleCountFlags se_vk_device_get_supported_image_multisample_types(SeVkDevice* device)
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
    se_assert_msg(se_array_size(queueIndices) == SeVkConfig::MAX_UNIQUE_COMMAND_QUEUES, "Don't forget to support new queues here");
    uint32_t uniqueQueues[SeVkConfig::MAX_UNIQUE_COMMAND_QUEUES];
    uint32_t numUniqueQueues = 0;
    for (uint32_t it = 0; it < SeVkConfig::MAX_UNIQUE_COMMAND_QUEUES; it++)
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
