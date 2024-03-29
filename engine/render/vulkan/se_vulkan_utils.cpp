
#include "se_vulkan_utils.hpp"

const char** se_vk_utils_get_required_validation_layers(size_t* outNum)
{
    static const char* VALIDATION_LAYERS[] =
    {
        "VK_LAYER_KHRONOS_validation",
    };
    *outNum = se_array_size(VALIDATION_LAYERS);
    return VALIDATION_LAYERS;
}

const char** se_vk_utils_get_required_instance_extensions(size_t* outNum)
{
    static const char* WINDOWS_INSTANCE_EXTENSIONS[] =
    {
        "VK_KHR_surface",
        "VK_KHR_win32_surface",
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
    };
    *outNum = se_array_size(WINDOWS_INSTANCE_EXTENSIONS);
    return WINDOWS_INSTANCE_EXTENSIONS;
}

const char** se_vk_utils_get_required_device_extensions(size_t* outNum)
{
    static const char* DEVICE_EXTENSIONS[] =
    {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };
    *outNum = se_array_size(DEVICE_EXTENSIONS);
    return DEVICE_EXTENSIONS;
}

SeDynamicArray<VkLayerProperties> se_vk_utils_get_available_validation_layers(const SeAllocatorBindings& allocator)
{
    uint32_t count;
    SeDynamicArray<VkLayerProperties> result;
    vkEnumerateInstanceLayerProperties(&count, nullptr);
    se_dynamic_array_construct(result, allocator, count);
    vkEnumerateInstanceLayerProperties(&count, se_dynamic_array_raw(result));
    se_dynamic_array_force_set_size(result, count);
    return result;
}

SeDynamicArray<VkExtensionProperties> se_vk_utils_get_available_instance_extensions(const SeAllocatorBindings& allocator)
{
    uint32_t count;
    SeDynamicArray<VkExtensionProperties> result;
    vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
    se_dynamic_array_construct(result, allocator, count);
    vkEnumerateInstanceExtensionProperties(nullptr, &count, se_dynamic_array_raw(result));
    se_dynamic_array_force_set_size(result, count);
    return result;
}

VkDebugUtilsMessengerCreateInfoEXT se_vk_utils_get_debug_messenger_create_info(PFN_vkDebugUtilsMessengerCallbackEXT callback, void* userData)
{
    return
    {
        .sType              = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .pNext              = nullptr,
        .flags              = 0,
        .messageSeverity    = /*VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |*/ VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType        = /*VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |*/ VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback    = callback,
        .pUserData          = userData,
    };
}

VkDebugUtilsMessengerEXT se_vk_utils_create_debug_messenger(const VkDebugUtilsMessengerCreateInfoEXT* createInfo, VkInstance instance, const VkAllocationCallbacks* callbacks)
{
    VkDebugUtilsMessengerEXT messenger;
    PFN_vkCreateDebugUtilsMessengerEXT CreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    se_assert(CreateDebugUtilsMessengerEXT);
    se_vk_check(CreateDebugUtilsMessengerEXT(instance, createInfo, callbacks, &messenger));
    return messenger;
}

void se_vk_utils_destroy_debug_messenger(VkInstance instance, VkDebugUtilsMessengerEXT messenger, const VkAllocationCallbacks* callbacks)
{
    vkDestroyDebugUtilsMessengerEXT(instance, messenger, callbacks);
}

VkCommandPool se_vk_utils_create_command_pool(VkDevice device, uint32_t queueFamilyIndex, const VkAllocationCallbacks* callbacks, VkCommandPoolCreateFlags flags)
{
    VkCommandPool pool;
    VkCommandPoolCreateInfo poolInfo
    {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext              = nullptr,
        .flags              = flags,
        .queueFamilyIndex   = queueFamilyIndex,
    };
    se_vk_check(vkCreateCommandPool(device, &poolInfo, callbacks, &pool));
    return pool;
}

void se_vk_utils_destroy_command_pool(VkCommandPool pool, VkDevice device, const VkAllocationCallbacks* callbacks)
{
    vkDestroyCommandPool(device, pool, callbacks);
}

SeVkSwapChainSupportDetails se_vk_utils_create_swap_chain_support_details(VkSurfaceKHR surface, VkPhysicalDevice device, const SeAllocatorBindings& allocator)
{
    SeVkSwapChainSupportDetails result = {0};
    se_vk_check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &result.capabilities));
    uint32_t count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &count, nullptr);
    if (count != 0)
    {
        se_dynamic_array_construct(result.formats, allocator, count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &count, se_dynamic_array_raw(result.formats));
        se_dynamic_array_force_set_size(result.formats, count);
    }
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &count, nullptr);
    if (count != 0)
    {
        se_dynamic_array_construct(result.presentModes, allocator, count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &count, se_dynamic_array_raw(result.presentModes));
        se_dynamic_array_force_set_size(result.presentModes, count);
    }
    return result;
}

void se_vk_utils_destroy_swap_chain_support_details(SeVkSwapChainSupportDetails& details)
{
    se_dynamic_array_destroy(details.formats);
    se_dynamic_array_destroy(details.presentModes);
}

VkSurfaceFormatKHR se_vk_utils_choose_swap_chain_surface_format(const SeDynamicArray<VkSurfaceFormatKHR>& available)
{
    for (auto it : available)
    {
        const VkSurfaceFormatKHR& value = se_iterator_value(it);
        if (value.format == VK_FORMAT_B8G8R8A8_SRGB && value.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            return value;
        }
    }
    return available[0];
}

VkPresentModeKHR se_vk_utils_choose_swap_chain_surface_present_mode(const SeDynamicArray<VkPresentModeKHR>& available)
{
    for (auto it : available)
    {
        const VkPresentModeKHR& value = se_iterator_value(it);
        if (value == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            return value;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR; // Guarateed to be available
}

VkExtent2D se_vk_utils_choose_swap_chain_extent(uint32_t windowWidth, uint32_t windowHeight, VkSurfaceCapabilitiesKHR* capabilities)
{
    if (capabilities->currentExtent.width != UINT32_MAX)
    {
        return capabilities->currentExtent;
    }
    else
    {
        return
        {
            se_clamp(windowWidth, capabilities->minImageExtent.width , capabilities->maxImageExtent.width),
            se_clamp(windowHeight, capabilities->minImageExtent.height, capabilities->maxImageExtent.height)
        };
    }
}

uint32_t se_vk_utils_pick_graphics_queue(const SeDynamicArray<VkQueueFamilyProperties>& familyProperties)
{
    for (auto it : familyProperties)
    {
        if (se_iterator_value(it).queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            return (uint32_t)se_iterator_index(it);
        }
    }
    return SE_VK_INVALID_QUEUE;
}

uint32_t se_vk_utils_pick_present_queue(const SeDynamicArray<VkQueueFamilyProperties>& familyProperties, VkPhysicalDevice device, VkSurfaceKHR surface)
{
    for (auto it : familyProperties)
    {
        const uint32_t index = (uint32_t)se_iterator_index(it);
        VkBool32 isSupported;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, index, surface, &isSupported);
        if (isSupported)
        {
            return index;
        }
    }
    return SE_VK_INVALID_QUEUE;
}

uint32_t se_vk_utils_pick_transfer_queue(const SeDynamicArray<VkQueueFamilyProperties>& familyProperties)
{
    for (auto it : familyProperties)
    {
        if (se_iterator_value(it).queueFlags & VK_QUEUE_TRANSFER_BIT)
        {
            return (uint32_t)se_iterator_index(it);
        }
    }
    return SE_VK_INVALID_QUEUE;
}

uint32_t se_vk_utils_pick_compute_queue(const SeDynamicArray<VkQueueFamilyProperties>& familyProperties)
{
    for (auto it : familyProperties)
    {
        if (se_iterator_value(it).queueFlags & VK_QUEUE_COMPUTE_BIT)
        {
            return (uint32_t)se_iterator_index(it);
        }
    }
    return SE_VK_INVALID_QUEUE;
}

SeDynamicArray<VkDeviceQueueCreateInfo> se_vk_utils_get_queue_create_infos(const uint32_t* queues, size_t numQueues, const SeAllocatorBindings& allocator)
{
    // @NOTE :  this is possible that queue family might support more than one of the required features,
    //          so we have to remove duplicates from queueFamiliesInfo and create VkDeviceQueueCreateInfos
    //          only for unique indexes
    static const float QUEUE_DEFAULT_PRIORITY = 1.0f;
    uint32_t indicesArray[64];
    size_t numUniqueIndices = 0;
    for (size_t it = 0; it < numQueues; it++)
    {
        bool isFound = false;
        for (size_t uniqueIt = 0; uniqueIt < numUniqueIndices; uniqueIt++)
        {
            if (indicesArray[uniqueIt] == queues[it])
            {
                isFound = true;
                break;
            }
        }
        if (!isFound)
        {
            indicesArray[numUniqueIndices++] = queues[it];
        }
    }
    SeDynamicArray<VkDeviceQueueCreateInfo> result;
    se_dynamic_array_construct(result, allocator, numUniqueIndices);
    for (size_t it = 0; it < numUniqueIndices; it++)
    {
        se_dynamic_array_push(result,
        {
            .sType              = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext              = nullptr,
            .flags              = 0,
            .queueFamilyIndex   = indicesArray[it],
            .queueCount         = 1,
            .pQueuePriorities   = &QUEUE_DEFAULT_PRIORITY,
        });
    }
    return result;
}

VkCommandPoolCreateInfo se_vk_utils_command_pool_create_info(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags)
{
    return
    {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext              = nullptr,
        .flags              = flags,
        .queueFamilyIndex   = queueFamilyIndex,
    };
}

bool se_vk_utils_pick_depth_stencil_format(VkPhysicalDevice physicalDevice, VkFormat* result)
{
    // @NOTE :  taken from https://github.com/SaschaWillems/Vulkan/blob/master/base/VulkanTools.cpp
    // Since all depth formats may be optional, we need to find a suitable depth format to use
    // Start with the highest precision packed format
    VkFormat depthStecilFormats[] =
    {
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D16_UNORM_S8_UINT,
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D16_UNORM,
    };
    for (size_t it = 0; it < se_array_size(depthStecilFormats); it++)
    {
        VkFormat format = depthStecilFormats[it];
        VkFormatProperties formatProps;
        vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &formatProps);
        // Format must support depth stencil attachment for optimal tiling
        if (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
        {
            *result = format;
            return it < 3;
        }
    }
    return false;
}

SeDynamicArray<VkPhysicalDevice> se_vk_utils_get_available_physical_devices(VkInstance instance, const SeAllocatorBindings& allocator)
{
    uint32_t count;
    SeDynamicArray<VkPhysicalDevice> result;
    vkEnumeratePhysicalDevices(instance, &count, nullptr);
    se_dynamic_array_construct(result, allocator, count);
    vkEnumeratePhysicalDevices(instance, &count, se_dynamic_array_raw(result));
    se_dynamic_array_force_set_size(result, count);
    return result;
};

SeDynamicArray<VkQueueFamilyProperties> se_vk_utils_get_physical_device_queue_family_properties(VkPhysicalDevice physicalDevice, const SeAllocatorBindings& allocator)
{
    uint32_t count;
    SeDynamicArray<VkQueueFamilyProperties> familyProperties;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &count, nullptr);
    se_dynamic_array_construct(familyProperties, allocator, count);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &count, se_dynamic_array_raw(familyProperties));
    se_dynamic_array_force_set_size(familyProperties, count);
    return familyProperties;
}

bool se_vk_utils_does_physical_device_supports_required_extensions(VkPhysicalDevice device, const char** extensions, size_t numExtensions, const SeAllocatorBindings& allocator)
{
    uint32_t count;
    VkPhysicalDeviceFeatures feat = {0};
    vkGetPhysicalDeviceFeatures(device, &feat);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
    SeDynamicArray<VkExtensionProperties> availableExtensions;
    se_dynamic_array_construct(availableExtensions, allocator, count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, se_dynamic_array_raw(availableExtensions));
    se_dynamic_array_force_set_size(availableExtensions, count);
    bool isRequiredExtensionAvailable;
    bool result = true;
    for (size_t requiredIt = 0; requiredIt < numExtensions; requiredIt++)
    {
        isRequiredExtensionAvailable = false;
        for (auto availableIt : availableExtensions)
        {
            if (strcmp(se_iterator_value(availableIt).extensionName, extensions[requiredIt]) == 0)
            {
                isRequiredExtensionAvailable = true;
                break;
            }
        }
        if (!isRequiredExtensionAvailable)
        {
            result = false;
            break;
        }
    }
    se_dynamic_array_destroy(availableExtensions);
    return result;
};

VkImageType se_vk_utils_pick_image_type(VkExtent3D imageExtent)
{
    // https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/VkImageCreateInfo.html
    // If imageType is VK_IMAGE_TYPE_1D, both extent.height and extent.depth must be 1
    if (imageExtent.height == 1 && imageExtent.depth == 1)
    {
        return VK_IMAGE_TYPE_1D;
    }
    // If imageType is VK_IMAGE_TYPE_2D, extent.depth must be 1
    if (imageExtent.depth == 1)
    {
        return VK_IMAGE_TYPE_2D;
    }
    return VK_IMAGE_TYPE_3D;
}

VkCommandBuffer se_vk_utils_create_command_buffer(VkDevice device, VkCommandPool pool, VkCommandBufferLevel level)
{
    VkCommandBuffer buffer;
    VkCommandBufferAllocateInfo allocInfo
    {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext              = nullptr,
        .commandPool        = pool,
        .level              = level,
        .commandBufferCount = 1,
    };
    se_vk_check(vkAllocateCommandBuffers(device, &allocInfo, &buffer));
    return buffer;
};

VkShaderModule se_vk_utils_create_shader_module(VkDevice device, const uint32_t* bytecode, size_t bytecodeSize, const VkAllocationCallbacks* allocationCb)
{
    VkShaderModuleCreateInfo createInfo
    {
        .sType      = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext      = nullptr,
        .flags      = 0,
        .codeSize   = bytecodeSize,
        .pCode      = bytecode,
    };
    VkShaderModule shaderModule;
    se_vk_check(vkCreateShaderModule(device, &createInfo, allocationCb, &shaderModule));
    return shaderModule;
}

void se_vk_utils_destroy_shader_module(VkDevice device, VkShaderModule module, const VkAllocationCallbacks* allocationCb)
{
    vkDestroyShaderModule(device, module, allocationCb);
}

bool se_vk_utils_get_memory_type_index(VkPhysicalDeviceMemoryProperties* props, uint32_t typeBits, VkMemoryPropertyFlags properties, uint32_t* result)
{
    for (uint32_t it = 0; it < props->memoryTypeCount; it++)
    {
        if (typeBits & 1)
        {
            if ((props->memoryTypes[it].propertyFlags & properties) == properties)
            {
                *result = it;
                return true;
            }
        }
        typeBits >>= 1;
    }
    return false;
}

SeTextureFormat se_vk_utils_to_se_texture_format(VkFormat vkFormat)
{
    switch (vkFormat)
    {
        case VK_FORMAT_R8_UNORM: return SeTextureFormat::R_8_UNORM;
        case VK_FORMAT_R8_SRGB: return SeTextureFormat::R_8_SRGB;
        case VK_FORMAT_R8G8B8A8_UNORM: return SeTextureFormat::RGBA_8_UNORM;
        case VK_FORMAT_R8G8B8A8_SRGB: return SeTextureFormat::RGBA_8_SRGB;
    }
    se_assert(!"Unsupported VkFormat");
    return (SeTextureFormat)0;
}

VkFormat se_vk_utils_to_vk_texture_format(SeTextureFormat format)
{
    switch (format)
    {
        case SeTextureFormat::R_8_UNORM: return VK_FORMAT_R8_UNORM;
        case SeTextureFormat::R_8_SRGB: return VK_FORMAT_R8_SRGB;
        case SeTextureFormat::RGBA_8_UNORM: return VK_FORMAT_R8G8B8A8_UNORM;
        case SeTextureFormat::RGBA_8_SRGB: return VK_FORMAT_R8G8B8A8_SRGB;
    }
    se_assert(!"Unsupported TextureFormat");
    return (VkFormat)0;
}

VkPolygonMode se_vk_utils_to_vk_polygon_mode(SePipelinePolygonMode mode)
{
    switch (mode)
    {
        case SePipelinePolygonMode::FILL: return VK_POLYGON_MODE_FILL;
        case SePipelinePolygonMode::LINE: return VK_POLYGON_MODE_LINE;
        case SePipelinePolygonMode::POINT: return VK_POLYGON_MODE_POINT;
    }
    se_assert(!"Unsupported SePipelinePolygonMode");
    return (VkPolygonMode)0;
}

VkCullModeFlags se_vk_utils_to_vk_cull_mode(SePipelineCullMode mode)
{
    switch (mode)
    {
        case SePipelineCullMode::NONE: return VK_CULL_MODE_NONE;
        case SePipelineCullMode::FRONT: return VK_CULL_MODE_FRONT_BIT;
        case SePipelineCullMode::BACK: return VK_CULL_MODE_BACK_BIT;
        case SePipelineCullMode::FRONT_BACK: return VK_CULL_MODE_FRONT_AND_BACK;
    }
    se_assert(!"Unsupported PipelineCullMode");
    return (VkCullModeFlags)0;
}

VkFrontFace se_vk_utils_to_vk_front_face(SePipelineFrontFace frontFace)
{
    switch (frontFace)
    {
        case SePipelineFrontFace::CLOCKWISE: return VK_FRONT_FACE_CLOCKWISE;
        case SePipelineFrontFace::COUNTER_CLOCKWISE: return VK_FRONT_FACE_COUNTER_CLOCKWISE;
    }
    se_assert(!"Unsupported SePipelineFrontFace");
    return (VkFrontFace)0;
}

VkSampleCountFlagBits se_vk_utils_to_vk_sample_count(SeSamplingType sampling)
{
    switch (sampling)
    {
        case SeSamplingType::_1: return (VkSampleCountFlagBits)VK_SAMPLE_COUNT_1_BIT;
        case SeSamplingType::_2: return (VkSampleCountFlagBits)VK_SAMPLE_COUNT_2_BIT;
        case SeSamplingType::_4: return (VkSampleCountFlagBits)VK_SAMPLE_COUNT_4_BIT;
        case SeSamplingType::_8: return (VkSampleCountFlagBits)VK_SAMPLE_COUNT_8_BIT;
    }
    se_assert(!"Unsupported SeSamplingType");
    return (VkSampleCountFlagBits)0;
}

VkSampleCountFlagBits se_vk_utils_pick_sample_count(VkSampleCountFlags desired, VkSampleCountFlags supported)
{
    if (supported & desired) return (VkSampleCountFlagBits)desired;
    for (VkSampleCountFlags it = sizeof(VkSampleCountFlags) * 8 - 1; it >= 0; it++)
    {
        if (((supported >> it) & 1) && it < desired)
        {
            return (VkSampleCountFlagBits)it;
        }
    }
    se_assert(!"Unable to pick sample count");
    return (VkSampleCountFlagBits)0;
}

VkStencilOp se_vk_utils_to_vk_stencil_op(SeStencilOp op)
{
    switch (op)
    {
        case SeStencilOp::KEEP:                 return VK_STENCIL_OP_KEEP;
        case SeStencilOp::ZERO:                 return VK_STENCIL_OP_ZERO;
        case SeStencilOp::REPLACE:              return VK_STENCIL_OP_REPLACE;
        case SeStencilOp::INCREMENT_AND_CLAMP:  return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
        case SeStencilOp::DECREMENT_AND_CLAMP:  return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
        case SeStencilOp::INVERT:               return VK_STENCIL_OP_INVERT;
        case SeStencilOp::INCREMENT_AND_WRAP:   return VK_STENCIL_OP_INCREMENT_AND_WRAP;
        case SeStencilOp::DECREMENT_AND_WRAP:   return VK_STENCIL_OP_DECREMENT_AND_WRAP;
    }
    se_assert(!"Unsupported SeStencilOp");
    return (VkStencilOp)0;
}

VkCompareOp se_vk_utils_to_vk_compare_op(SeCompareOp op)
{
    switch (op)
    {
        case SeCompareOp::NEVER:            return VK_COMPARE_OP_NEVER;
        case SeCompareOp::LESS:             return VK_COMPARE_OP_LESS;
        case SeCompareOp::EQUAL:            return VK_COMPARE_OP_EQUAL;
        case SeCompareOp::LESS_OR_EQUAL:    return VK_COMPARE_OP_LESS_OR_EQUAL;
        case SeCompareOp::GREATER:          return VK_COMPARE_OP_GREATER;
        case SeCompareOp::NOT_EQUAL:        return VK_COMPARE_OP_NOT_EQUAL;
        case SeCompareOp::GREATER_OR_EQUAL: return VK_COMPARE_OP_GREATER_OR_EQUAL;
        case SeCompareOp::ALWAYS:           return VK_COMPARE_OP_ALWAYS;
    }
    se_assert(!"Unsupported CompareOp");
    return (VkCompareOp)0;
}

VkBool32 se_vk_utils_to_vk_bool(bool value)
{
    return value ? VK_TRUE : VK_FALSE;
}

VkPipelineVertexInputStateCreateInfo se_vk_utils_vertex_input_state_create_info(uint32_t bindingsCount, const VkVertexInputBindingDescription* bindingDescs, uint32_t attrCount, const VkVertexInputAttributeDescription* attrDescs)
{
    return
    {
        .sType                              = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext                              = nullptr,
        .flags                              = 0,
        .vertexBindingDescriptionCount      = bindingsCount,
        .pVertexBindingDescriptions         = bindingDescs,
        .vertexAttributeDescriptionCount    = attrCount,
        .pVertexAttributeDescriptions       = attrDescs,
    };
}

VkPipelineInputAssemblyStateCreateInfo se_vk_utils_input_assembly_state_create_info(VkPrimitiveTopology topology, VkBool32 primitiveRestartEnable)
{
    return
    {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .pNext                  = nullptr,
        .flags                  = 0,
        .topology               = topology,
        .primitiveRestartEnable = primitiveRestartEnable,
    };
}

SeVkViewportScissor se_vk_utils_default_viewport_scissor(uint32_t width, uint32_t height)
{
    return
    {
        {
            .x          = 0.0f,
            .y          = 0.0f,
            .width      = (float)width,
            .height     = (float)height,
            .minDepth   = 0.0f,
            .maxDepth   = 1.0f,
        },
        {
            .offset = { 0, 0 },
            .extent = { width, height },
        }
    };
}

VkPipelineViewportStateCreateInfo se_vk_utils_viewport_state_create_info(const VkViewport* viewport, const VkRect2D* scissor)
{
    return
    {
        .sType          = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext          = nullptr,
        .flags          = 0,
        .viewportCount  = 1,
        .pViewports     = viewport,
        .scissorCount   = 1,
        .pScissors      = scissor,
    };
}

VkPipelineRasterizationStateCreateInfo se_vk_utils_rasterization_state_create_info(VkPolygonMode polygonMode, VkCullModeFlags cullMode, VkFrontFace frontFace)
{
    return
    {
        .sType                      = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .pNext                      = nullptr,
        .flags                      = 0,
        .depthClampEnable           = VK_FALSE,
        .rasterizerDiscardEnable    = VK_FALSE,
        .polygonMode                = polygonMode,
        .cullMode                   = cullMode,
        .frontFace                  = frontFace,
        .depthBiasEnable            = VK_FALSE,
        .depthBiasConstantFactor    = 0.0f,
        .depthBiasClamp             = 0.0f,
        .depthBiasSlopeFactor       = 0.0f,
        .lineWidth                  = 1.0f,
    };
}

VkPipelineMultisampleStateCreateInfo se_vk_utils_multisample_state_create_info(VkSampleCountFlagBits resterizationSamples)
{
    return
    {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .pNext                  = nullptr,
        .flags                  = 0,
        .rasterizationSamples   = resterizationSamples,
        .sampleShadingEnable    = VK_FALSE,
        .minSampleShading       = 1.0f,
        .pSampleMask            = nullptr,
        .alphaToCoverageEnable  = VK_FALSE,
        .alphaToOneEnable       = VK_FALSE,
    };
}

VkPipelineColorBlendStateCreateInfo se_vk_utils_color_blending_create_info(VkPipelineColorBlendAttachmentState* colorBlendAttachmentStates, uint32_t numStates)
{
    return
    {
        .sType              = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext              = nullptr,
        .flags              = 0,
        .logicOpEnable      = VK_FALSE,
        .logicOp            = VK_LOGIC_OP_COPY, // optional if logicOpEnable == VK_FALSE
        .attachmentCount    = numStates,
        .pAttachments       = colorBlendAttachmentStates,
        .blendConstants     = { 1.0f, 1.0f, 1.0f, 1.0f }, // optional, because color blending is disabled in colorBlendAttachments
    };
}

VkPipelineDynamicStateCreateInfo se_vk_utils_dynamic_state_default_create_info()
{
    static VkDynamicState dynamicStates[] =
    {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    return
    {
        .sType              = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pNext              = nullptr,
        .flags              = 0,
        .dynamicStateCount  = se_array_size(dynamicStates),
        .pDynamicStates     = dynamicStates,
    };
}

VkAccessFlags se_vk_utils_image_layout_to_access_flags(VkImageLayout layout)
{
    switch (layout)
    {
        case VK_IMAGE_LAYOUT_UNDEFINED:                         return VkAccessFlags(0);
        case VK_IMAGE_LAYOUT_GENERAL:                           return VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:          return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:  return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:   return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:          return VK_ACCESS_SHADER_READ_BIT;
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:              return VK_ACCESS_TRANSFER_READ_BIT;
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:              return VK_ACCESS_TRANSFER_WRITE_BIT;
        case VK_IMAGE_LAYOUT_PREINITIALIZED:                    return VK_ACCESS_MEMORY_WRITE_BIT;
        // @NOTE :  https://themaister.net/blog/2019/08/14/yet-another-blog-explaining-vulkan-synchronization/
        //          Having dstStageMask = BOTTOM_OF_PIPE and access mask being 0 is perfectly fine.
        //          We don’t care about making this memory visible to any stage beyond this point.
        //          We will use semaphores to synchronize with the presentation engine anyways.
        case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:                   return VkAccessFlags(0);
        default: se_assert(!"Unsupported VkImageLayout");
    }
    return VkAccessFlags(0);
}

VkPipelineStageFlags se_vk_utils_image_layout_to_pipeline_stage_flags(VkImageLayout layout)
{
    switch (layout)
    {
        case VK_IMAGE_LAYOUT_UNDEFINED:                         return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:              return VK_PIPELINE_STAGE_TRANSFER_BIT;
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:          return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:          return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:  return VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL:          return VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:                   return VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        default: se_assert(!"Unsupported VkImageLayout to VkPipelineStageFlags conversion");
    }
    return VkPipelineStageFlags(0);
}

VkAttachmentLoadOp se_vk_utils_to_vk_load_op(SeRenderTargetLoadOp loadOp)
{
    return VkAttachmentLoadOp(int(loadOp) - 1);
}

// https://registry.khronos.org/vulkan/specs/1.1/html/vkspec.html#_identification_of_formats
// https://stackoverflow.com/questions/59628956/what-is-the-difference-between-normalized-scaled-and-integer-vkformats
SeVkFormatInfo se_vk_utils_get_format_info(VkFormat format)
{
    switch (format)
    {
        case VK_FORMAT_R8_SNORM:                    return { 1, 8, SeVkFormatInfo::Type::INT,    SeVkFormatInfo::Type::FLOAT };
        case VK_FORMAT_R8_UNORM:                    return { 1, 8, SeVkFormatInfo::Type::UINT,   SeVkFormatInfo::Type::FLOAT };
        case VK_FORMAT_R8_USCALED:                  return { 1, 8, SeVkFormatInfo::Type::UINT,   SeVkFormatInfo::Type::FLOAT };
        case VK_FORMAT_R8_SSCALED:                  return { 1, 8, SeVkFormatInfo::Type::INT,    SeVkFormatInfo::Type::FLOAT };
        case VK_FORMAT_R8_UINT:                     return { 1, 8, SeVkFormatInfo::Type::UINT,   SeVkFormatInfo::Type::UINT };
        case VK_FORMAT_R8_SINT:                     return { 1, 8, SeVkFormatInfo::Type::INT,    SeVkFormatInfo::Type::INT };
        case VK_FORMAT_R8_SRGB:                     return { 1, 8, SeVkFormatInfo::Type::UINT,   SeVkFormatInfo::Type::FLOAT };
        case VK_FORMAT_R8G8_UNORM:                  return { 2, 8, SeVkFormatInfo::Type::UINT,   SeVkFormatInfo::Type::FLOAT };
        case VK_FORMAT_R8G8_SNORM:                  return { 2, 8, SeVkFormatInfo::Type::INT,    SeVkFormatInfo::Type::FLOAT };
        case VK_FORMAT_R8G8_USCALED:                return { 2, 8, SeVkFormatInfo::Type::UINT,   SeVkFormatInfo::Type::FLOAT };
        case VK_FORMAT_R8G8_SSCALED:                return { 2, 8, SeVkFormatInfo::Type::INT,    SeVkFormatInfo::Type::FLOAT };
        case VK_FORMAT_R8G8_UINT:                   return { 2, 8, SeVkFormatInfo::Type::UINT,   SeVkFormatInfo::Type::UINT };
        case VK_FORMAT_R8G8_SINT:                   return { 2, 8, SeVkFormatInfo::Type::INT,    SeVkFormatInfo::Type::INT };
        case VK_FORMAT_R8G8_SRGB:                   return { 2, 8, SeVkFormatInfo::Type::UINT,   SeVkFormatInfo::Type::FLOAT };
        case VK_FORMAT_R8G8B8_UNORM:                return { 3, 8, SeVkFormatInfo::Type::UINT,   SeVkFormatInfo::Type::FLOAT };
        case VK_FORMAT_R8G8B8_SNORM:                return { 3, 8, SeVkFormatInfo::Type::INT,    SeVkFormatInfo::Type::FLOAT };
        case VK_FORMAT_R8G8B8_USCALED:              return { 3, 8, SeVkFormatInfo::Type::UINT,   SeVkFormatInfo::Type::FLOAT };
        case VK_FORMAT_R8G8B8_SSCALED:              return { 3, 8, SeVkFormatInfo::Type::INT,    SeVkFormatInfo::Type::FLOAT };
        case VK_FORMAT_R8G8B8_UINT:                 return { 3, 8, SeVkFormatInfo::Type::UINT,   SeVkFormatInfo::Type::UINT };
        case VK_FORMAT_R8G8B8_SINT:                 return { 3, 8, SeVkFormatInfo::Type::INT,    SeVkFormatInfo::Type::INT };
        case VK_FORMAT_R8G8B8_SRGB:                 return { 3, 8, SeVkFormatInfo::Type::UINT,   SeVkFormatInfo::Type::FLOAT };
        case VK_FORMAT_B8G8R8_UNORM:                return { 3, 8, SeVkFormatInfo::Type::UINT,   SeVkFormatInfo::Type::FLOAT };
        case VK_FORMAT_B8G8R8_SNORM:                return { 3, 8, SeVkFormatInfo::Type::INT,    SeVkFormatInfo::Type::FLOAT };
        case VK_FORMAT_B8G8R8_USCALED:              return { 3, 8, SeVkFormatInfo::Type::UINT,   SeVkFormatInfo::Type::FLOAT };
        case VK_FORMAT_B8G8R8_SSCALED:              return { 3, 8, SeVkFormatInfo::Type::INT,    SeVkFormatInfo::Type::FLOAT };
        case VK_FORMAT_B8G8R8_UINT:                 return { 3, 8, SeVkFormatInfo::Type::UINT,   SeVkFormatInfo::Type::UINT };
        case VK_FORMAT_B8G8R8_SINT:                 return { 3, 8, SeVkFormatInfo::Type::INT,    SeVkFormatInfo::Type::INT };
        case VK_FORMAT_B8G8R8_SRGB:                 return { 3, 8, SeVkFormatInfo::Type::UINT,   SeVkFormatInfo::Type::FLOAT };
        case VK_FORMAT_R8G8B8A8_UNORM:              return { 4, 8, SeVkFormatInfo::Type::UINT,   SeVkFormatInfo::Type::FLOAT };
        case VK_FORMAT_R8G8B8A8_SNORM:              return { 4, 8, SeVkFormatInfo::Type::INT,    SeVkFormatInfo::Type::FLOAT };
        case VK_FORMAT_R8G8B8A8_USCALED:            return { 4, 8, SeVkFormatInfo::Type::UINT,   SeVkFormatInfo::Type::FLOAT };
        case VK_FORMAT_R8G8B8A8_SSCALED:            return { 4, 8, SeVkFormatInfo::Type::INT,    SeVkFormatInfo::Type::FLOAT };
        case VK_FORMAT_R8G8B8A8_UINT:               return { 4, 8, SeVkFormatInfo::Type::UINT,   SeVkFormatInfo::Type::UINT };
        case VK_FORMAT_R8G8B8A8_SINT:               return { 4, 8, SeVkFormatInfo::Type::INT,    SeVkFormatInfo::Type::INT };
        case VK_FORMAT_R8G8B8A8_SRGB:               return { 4, 8, SeVkFormatInfo::Type::UINT,   SeVkFormatInfo::Type::FLOAT };
        case VK_FORMAT_B8G8R8A8_UNORM:              return { 4, 8, SeVkFormatInfo::Type::UINT,   SeVkFormatInfo::Type::FLOAT };
        case VK_FORMAT_B8G8R8A8_SNORM:              return { 4, 8, SeVkFormatInfo::Type::INT,    SeVkFormatInfo::Type::FLOAT };
        case VK_FORMAT_B8G8R8A8_USCALED:            return { 4, 8, SeVkFormatInfo::Type::UINT,   SeVkFormatInfo::Type::FLOAT };
        case VK_FORMAT_B8G8R8A8_SSCALED:            return { 4, 8, SeVkFormatInfo::Type::INT,    SeVkFormatInfo::Type::FLOAT };
        case VK_FORMAT_B8G8R8A8_UINT:               return { 4, 8, SeVkFormatInfo::Type::UINT,   SeVkFormatInfo::Type::UINT };
        case VK_FORMAT_B8G8R8A8_SINT:               return { 4, 8, SeVkFormatInfo::Type::INT,    SeVkFormatInfo::Type::INT };
        case VK_FORMAT_B8G8R8A8_SRGB:               return { 4, 8, SeVkFormatInfo::Type::UINT,   SeVkFormatInfo::Type::FLOAT };
        case VK_FORMAT_A8B8G8R8_UNORM_PACK32:       return { 4, 8, SeVkFormatInfo::Type::UINT,   SeVkFormatInfo::Type::FLOAT };
        case VK_FORMAT_A8B8G8R8_SNORM_PACK32:       return { 4, 8, SeVkFormatInfo::Type::INT,    SeVkFormatInfo::Type::FLOAT };
        case VK_FORMAT_A8B8G8R8_USCALED_PACK32:     return { 4, 8, SeVkFormatInfo::Type::UINT,   SeVkFormatInfo::Type::FLOAT };
        case VK_FORMAT_A8B8G8R8_SSCALED_PACK32:     return { 4, 8, SeVkFormatInfo::Type::INT,    SeVkFormatInfo::Type::FLOAT };
        case VK_FORMAT_A8B8G8R8_UINT_PACK32:        return { 4, 8, SeVkFormatInfo::Type::UINT,   SeVkFormatInfo::Type::UINT };
        case VK_FORMAT_A8B8G8R8_SINT_PACK32:        return { 4, 8, SeVkFormatInfo::Type::INT,    SeVkFormatInfo::Type::INT };
        case VK_FORMAT_A8B8G8R8_SRGB_PACK32:        return { 4, 8, SeVkFormatInfo::Type::UINT,   SeVkFormatInfo::Type::FLOAT };
        case VK_FORMAT_R16_UNORM:                   return { 1, 16, SeVkFormatInfo::Type::UINT,  SeVkFormatInfo::Type::FLOAT };
        case VK_FORMAT_R16_SNORM:                   return { 1, 16, SeVkFormatInfo::Type::INT,   SeVkFormatInfo::Type::FLOAT };
        case VK_FORMAT_R16_USCALED:                 return { 1, 16, SeVkFormatInfo::Type::UINT,  SeVkFormatInfo::Type::FLOAT };
        case VK_FORMAT_R16_SSCALED:                 return { 1, 16, SeVkFormatInfo::Type::INT,   SeVkFormatInfo::Type::FLOAT };
        case VK_FORMAT_R16_UINT:                    return { 1, 16, SeVkFormatInfo::Type::UINT,  SeVkFormatInfo::Type::UINT };
        case VK_FORMAT_R16_SINT:                    return { 1, 16, SeVkFormatInfo::Type::INT,   SeVkFormatInfo::Type::INT };
        case VK_FORMAT_R16_SFLOAT:                  return { 1, 16, SeVkFormatInfo::Type::FLOAT, SeVkFormatInfo::Type::FLOAT  };
        case VK_FORMAT_R16G16_UNORM:                return { 2, 16, SeVkFormatInfo::Type::UINT,  SeVkFormatInfo::Type::FLOAT };
        case VK_FORMAT_R16G16_SNORM:                return { 2, 16, SeVkFormatInfo::Type::INT,   SeVkFormatInfo::Type::FLOAT };
        case VK_FORMAT_R16G16_USCALED:              return { 2, 16, SeVkFormatInfo::Type::UINT,  SeVkFormatInfo::Type::FLOAT };
        case VK_FORMAT_R16G16_SSCALED:              return { 2, 16, SeVkFormatInfo::Type::INT,   SeVkFormatInfo::Type::FLOAT };
        case VK_FORMAT_R16G16_UINT:                 return { 2, 16, SeVkFormatInfo::Type::UINT,  SeVkFormatInfo::Type::UINT };
        case VK_FORMAT_R16G16_SINT:                 return { 2, 16, SeVkFormatInfo::Type::INT,   SeVkFormatInfo::Type::INT };
        case VK_FORMAT_R16G16_SFLOAT:               return { 2, 16, SeVkFormatInfo::Type::FLOAT, SeVkFormatInfo::Type::FLOAT  };
        case VK_FORMAT_R16G16B16_UNORM:             return { 3, 16, SeVkFormatInfo::Type::UINT,  SeVkFormatInfo::Type::FLOAT };
        case VK_FORMAT_R16G16B16_SNORM:             return { 3, 16, SeVkFormatInfo::Type::INT,   SeVkFormatInfo::Type::FLOAT };
        case VK_FORMAT_R16G16B16_USCALED:           return { 3, 16, SeVkFormatInfo::Type::UINT,  SeVkFormatInfo::Type::FLOAT };
        case VK_FORMAT_R16G16B16_SSCALED:           return { 3, 16, SeVkFormatInfo::Type::INT,   SeVkFormatInfo::Type::FLOAT };
        case VK_FORMAT_R16G16B16_UINT:              return { 3, 16, SeVkFormatInfo::Type::UINT,  SeVkFormatInfo::Type::UINT };
        case VK_FORMAT_R16G16B16_SINT:              return { 3, 16, SeVkFormatInfo::Type::INT,   SeVkFormatInfo::Type::INT };
        case VK_FORMAT_R16G16B16_SFLOAT:            return { 3, 16, SeVkFormatInfo::Type::FLOAT, SeVkFormatInfo::Type::FLOAT  };
        case VK_FORMAT_R16G16B16A16_UNORM:          return { 4, 16, SeVkFormatInfo::Type::UINT,  SeVkFormatInfo::Type::FLOAT };
        case VK_FORMAT_R16G16B16A16_SNORM:          return { 4, 16, SeVkFormatInfo::Type::INT,   SeVkFormatInfo::Type::FLOAT };
        case VK_FORMAT_R16G16B16A16_USCALED:        return { 4, 16, SeVkFormatInfo::Type::UINT,  SeVkFormatInfo::Type::FLOAT };
        case VK_FORMAT_R16G16B16A16_SSCALED:        return { 4, 16, SeVkFormatInfo::Type::INT,   SeVkFormatInfo::Type::FLOAT };
        case VK_FORMAT_R16G16B16A16_UINT:           return { 4, 16, SeVkFormatInfo::Type::UINT,  SeVkFormatInfo::Type::UINT };
        case VK_FORMAT_R16G16B16A16_SINT:           return { 4, 16, SeVkFormatInfo::Type::INT,   SeVkFormatInfo::Type::INT };
        case VK_FORMAT_R16G16B16A16_SFLOAT:         return { 4, 16, SeVkFormatInfo::Type::FLOAT, SeVkFormatInfo::Type::FLOAT  };
        case VK_FORMAT_R32_UINT:                    return { 1, 32, SeVkFormatInfo::Type::UINT,  SeVkFormatInfo::Type::UINT };
        case VK_FORMAT_R32_SINT:                    return { 1, 32, SeVkFormatInfo::Type::INT,   SeVkFormatInfo::Type::INT };
        case VK_FORMAT_R32_SFLOAT:                  return { 1, 32, SeVkFormatInfo::Type::FLOAT, SeVkFormatInfo::Type::FLOAT  };
        case VK_FORMAT_R32G32_UINT:                 return { 2, 32, SeVkFormatInfo::Type::UINT,  SeVkFormatInfo::Type::UINT };
        case VK_FORMAT_R32G32_SINT:                 return { 2, 32, SeVkFormatInfo::Type::INT,   SeVkFormatInfo::Type::INT };
        case VK_FORMAT_R32G32_SFLOAT:               return { 2, 32, SeVkFormatInfo::Type::FLOAT, SeVkFormatInfo::Type::FLOAT  };
        case VK_FORMAT_R32G32B32_UINT:              return { 3, 32, SeVkFormatInfo::Type::UINT,  SeVkFormatInfo::Type::UINT };
        case VK_FORMAT_R32G32B32_SINT:              return { 3, 32, SeVkFormatInfo::Type::INT,   SeVkFormatInfo::Type::INT };
        case VK_FORMAT_R32G32B32_SFLOAT:            return { 3, 32, SeVkFormatInfo::Type::FLOAT, SeVkFormatInfo::Type::FLOAT  };
        case VK_FORMAT_R32G32B32A32_UINT:           return { 4, 32, SeVkFormatInfo::Type::UINT,  SeVkFormatInfo::Type::UINT };
        case VK_FORMAT_R32G32B32A32_SINT:           return { 4, 32, SeVkFormatInfo::Type::INT,   SeVkFormatInfo::Type::INT };
        case VK_FORMAT_R32G32B32A32_SFLOAT:         return { 4, 32, SeVkFormatInfo::Type::FLOAT, SeVkFormatInfo::Type::FLOAT  };
        case VK_FORMAT_R64_UINT:                    return { 1, 64, SeVkFormatInfo::Type::UINT,  SeVkFormatInfo::Type::UINT };
        case VK_FORMAT_R64_SINT:                    return { 1, 64, SeVkFormatInfo::Type::INT,   SeVkFormatInfo::Type::INT };
        case VK_FORMAT_R64_SFLOAT:                  return { 1, 64, SeVkFormatInfo::Type::FLOAT, SeVkFormatInfo::Type::FLOAT  };
        case VK_FORMAT_R64G64_UINT:                 return { 2, 64, SeVkFormatInfo::Type::UINT,  SeVkFormatInfo::Type::UINT };
        case VK_FORMAT_R64G64_SINT:                 return { 2, 64, SeVkFormatInfo::Type::INT,   SeVkFormatInfo::Type::INT };
        case VK_FORMAT_R64G64_SFLOAT:               return { 2, 64, SeVkFormatInfo::Type::FLOAT, SeVkFormatInfo::Type::FLOAT  };
        case VK_FORMAT_R64G64B64_UINT:              return { 3, 64, SeVkFormatInfo::Type::UINT,  SeVkFormatInfo::Type::UINT };
        case VK_FORMAT_R64G64B64_SINT:              return { 3, 64, SeVkFormatInfo::Type::INT,   SeVkFormatInfo::Type::INT };
        case VK_FORMAT_R64G64B64_SFLOAT:            return { 3, 64, SeVkFormatInfo::Type::FLOAT, SeVkFormatInfo::Type::FLOAT  };
        case VK_FORMAT_R64G64B64A64_UINT:           return { 4, 64, SeVkFormatInfo::Type::UINT,  SeVkFormatInfo::Type::UINT };
        case VK_FORMAT_R64G64B64A64_SINT:           return { 4, 64, SeVkFormatInfo::Type::INT,   SeVkFormatInfo::Type::INT };
        case VK_FORMAT_R64G64B64A64_SFLOAT:         return { 4, 64, SeVkFormatInfo::Type::FLOAT, SeVkFormatInfo::Type::FLOAT  };
        case VK_FORMAT_D16_UNORM:                   return { 1, 16, SeVkFormatInfo::Type::UINT,  SeVkFormatInfo::Type::FLOAT };
        case VK_FORMAT_D32_SFLOAT:                  return { 1, 32, SeVkFormatInfo::Type::FLOAT, SeVkFormatInfo::Type::FLOAT  };
        case VK_FORMAT_S8_UINT:                     return { 1, 8, SeVkFormatInfo::Type::UINT,   SeVkFormatInfo::Type::UINT };
        default:                                    { se_assert(false); }
    }
    return { };
}
