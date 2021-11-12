
#include <string.h>

#include "se_vulkan_render_subsystem_utils.h"

#define SE_MATH_IMPL
#include "engine/se_math.h"

#define SE_CONTAINERS_IMPL
#include "engine/containers.h"

#define SE_DEBUG_IMPL
#include "engine/debug.h"

const char** se_get_required_validation_layers(size_t* outNum)
{
    static const char* VALIDATION_LAYERS[] = 
    {
        "VK_LAYER_KHRONOS_validation",
    };
    *outNum = se_array_size(VALIDATION_LAYERS);
    return VALIDATION_LAYERS;
}

const char** se_get_required_instance_extensions(size_t* outNum)
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

const char** se_get_required_device_extensions(size_t* outNum)
{
    static const char* DEVICE_EXTENSIONS[] = 
    {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };
    *outNum = se_array_size(DEVICE_EXTENSIONS);
    return DEVICE_EXTENSIONS;
}

se_sbuffer(VkLayerProperties) se_get_available_validation_layers(struct SeAllocatorBindings* allocator)
{
    uint32_t count;
    se_sbuffer(VkLayerProperties) result = {0};
    vkEnumerateInstanceLayerProperties(&count, NULL);
    se_sbuffer_construct(result, count, allocator);
    vkEnumerateInstanceLayerProperties(&count, result);
    se_sbuffer_set_size(result, count);
    return result;
}

se_sbuffer(VkExtensionProperties) se_get_available_instance_extensions(struct SeAllocatorBindings* allocator)
{
    uint32_t count;
    se_sbuffer(VkExtensionProperties) result = {0};
    vkEnumerateInstanceExtensionProperties(NULL, &count, NULL);
    se_sbuffer_construct(result, count, allocator);
    vkEnumerateInstanceExtensionProperties(NULL, &count, result);
    se_sbuffer_set_size(result, count);
    return result;
}

VkDebugUtilsMessengerCreateInfoEXT se_get_debug_messenger_create_info(PFN_vkDebugUtilsMessengerCallbackEXT callback, void* userData)
{
    return (VkDebugUtilsMessengerCreateInfoEXT)
    {
        .sType              = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .pNext              = NULL,
        .flags              = 0,
        .messageSeverity    = /*VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |*/ VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType        = /*VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |*/ VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback    = callback,
        .pUserData          = userData,
    };
}

VkDebugUtilsMessengerEXT se_create_debug_messenger(VkDebugUtilsMessengerCreateInfoEXT* createInfo, VkInstance instance, VkAllocationCallbacks* callbacks)
{
    VkDebugUtilsMessengerEXT messenger;
    PFN_vkCreateDebugUtilsMessengerEXT CreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    se_assert(CreateDebugUtilsMessengerEXT);
    se_vk_check(CreateDebugUtilsMessengerEXT(instance, createInfo, callbacks, &messenger));
    return messenger;
}

void se_destroy_debug_messenger(VkInstance instance, VkDebugUtilsMessengerEXT messenger, VkAllocationCallbacks* callbacks)
{
    PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    se_assert(vkDestroyDebugUtilsMessengerEXT);
    vkDestroyDebugUtilsMessengerEXT(instance, messenger, callbacks);
}

VkCommandPool se_create_command_pool(VkDevice device, uint32_t queueFamilyIndex, VkAllocationCallbacks* callbacks, VkCommandPoolCreateFlags flags)
{
    VkCommandPool pool;
    VkCommandPoolCreateInfo poolInfo = (VkCommandPoolCreateInfo)
    {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext              = NULL,
        .flags              = flags,
        .queueFamilyIndex   = queueFamilyIndex,
    };
    se_vk_check(vkCreateCommandPool(device, &poolInfo, callbacks, &pool));
    return pool;
}

void se_destroy_command_pool(VkCommandPool pool, VkDevice device, VkAllocationCallbacks* callbacks)
{
    vkDestroyCommandPool(device, pool, callbacks);
}

SeVkSwapChainSupportDetails se_create_swap_chain_support_details(VkSurfaceKHR surface, VkPhysicalDevice device, struct SeAllocatorBindings* allocator)
{
    SeVkSwapChainSupportDetails result = {0};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &result.capabilities);
    uint32_t count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &count, NULL);
    if (count != 0)
    {
        se_sbuffer_construct(result.formats, count, allocator);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &count, result.formats);
        se_sbuffer_set_size(result.formats, count);
    }
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &count, NULL);
    if (count != 0)
    {
        se_sbuffer_construct(result.presentModes, count, allocator);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &count, result.presentModes);
        se_sbuffer_set_size(result.presentModes, count);
    }
    return result;
}

void se_destroy_swap_chain_support_details(SeVkSwapChainSupportDetails* details)
{
    se_sbuffer_destroy(details->formats);
    se_sbuffer_destroy(details->presentModes);
}

VkSurfaceFormatKHR se_choose_swap_chain_surface_format(se_sbuffer(VkSurfaceFormatKHR) available)
{
    for (size_t it = 0; it < se_sbuffer_size(available); it++)
    {
        if (available[it].format == /*VK_FORMAT_B8G8R8A8_SRGB*/ VK_FORMAT_R8G8B8A8_SRGB && available[it].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            return available[it];
        }
    }
    return available[0];
}

VkPresentModeKHR se_choose_swap_chain_surface_present_mode(se_sbuffer(VkPresentModeKHR) available)
{
    for (size_t it = 0; it < se_sbuffer_size(available); it++)
    {
        if (available[it] == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            return available[it];
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR; // Guarateed to be available
}

VkExtent2D se_choose_swap_chain_extent(uint32_t windowWidth, uint32_t windowHeight, VkSurfaceCapabilitiesKHR* capabilities)
{
    if (capabilities->currentExtent.width != UINT32_MAX)
    {
        return capabilities->currentExtent;
    }
    else
    {
        return (VkExtent2D)
        {
            se_clamp(windowWidth, capabilities->minImageExtent.width , capabilities->maxImageExtent.width),
            se_clamp(windowHeight, capabilities->minImageExtent.height, capabilities->maxImageExtent.height)
        };
    }
}

uint32_t se_pick_graphics_queue(se_sbuffer(VkQueueFamilyProperties) familyProperties)
{
    for (size_t it = 0; it < se_sbuffer_size(familyProperties); it++)
    {
        if (familyProperties[it].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            return it;
        }
    }
    return SE_VK_INVALID_QUEUE;
}

uint32_t se_pick_present_queue(se_sbuffer(VkQueueFamilyProperties) familyProperties, VkPhysicalDevice device, VkSurfaceKHR surface)
{
    for (size_t it = 0; it < se_sbuffer_size(familyProperties); it++)
    {
        VkBool32 isSupported;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, it, surface, &isSupported);
        if (isSupported)
        {
            return it;
        }
    }
    return SE_VK_INVALID_QUEUE;
}

uint32_t se_pick_transfer_queue(se_sbuffer(VkQueueFamilyProperties) familyProperties)
{
    for (size_t it = 0; it < se_sbuffer_size(familyProperties); it++)
    {
        if (familyProperties[it].queueFlags & VK_QUEUE_TRANSFER_BIT)
        {
            return it;
        }
    }
    return SE_VK_INVALID_QUEUE;
}

se_sbuffer(VkDeviceQueueCreateInfo) se_get_queue_create_infos(uint32_t* queues, size_t numQueues, struct SeAllocatorBindings* allocator)
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
    se_sbuffer(VkDeviceQueueCreateInfo) result = {0};
    se_sbuffer_construct(result, numUniqueIndices, allocator);
    for (size_t it = 0; it < numUniqueIndices; it++)
    {
        result[it] = (VkDeviceQueueCreateInfo)
        {
            .sType              = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext              = NULL,
            .flags              = 0,
            .queueFamilyIndex   = indicesArray[it],
            .queueCount         = 1,
            .pQueuePriorities   = &QUEUE_DEFAULT_PRIORITY,
        };
    }
    se_sbuffer_set_size(result, numUniqueIndices);
    return result;
}

VkCommandPoolCreateInfo se_command_pool_create_info(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags)
{
    return (VkCommandPoolCreateInfo)
    {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext              = NULL,
        .flags              = flags,
        .queueFamilyIndex   = queueFamilyIndex,
    };
}

bool se_pick_depth_stencil_format(VkPhysicalDevice physicalDevice, VkFormat* result)
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

se_sbuffer(VkPhysicalDevice) se_get_available_physical_devices(VkInstance instance, struct SeAllocatorBindings* allocator)
{
    uint32_t count;
    se_sbuffer(VkPhysicalDevice) result = {0};
    vkEnumeratePhysicalDevices(instance, &count, NULL);
    se_sbuffer_construct(result, count, allocator);
    vkEnumeratePhysicalDevices(instance, &count, result);
    se_sbuffer_set_size(result, count);
    return result;
};

se_sbuffer(VkQueueFamilyProperties) se_get_physical_device_queue_family_properties(VkPhysicalDevice physicalDevice, struct SeAllocatorBindings* allocator)
{
    uint32_t count;
    se_sbuffer(VkQueueFamilyProperties) familyProperties;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &count, NULL);
    se_sbuffer_construct(familyProperties, count, allocator);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &count, familyProperties);
    se_sbuffer_set_size(familyProperties, count);
    return familyProperties;
}

bool se_does_physical_device_supports_required_extensions(VkPhysicalDevice device, const char** extensions, size_t numExtensions, struct SeAllocatorBindings* allocator)
{
    uint32_t count;
    VkPhysicalDeviceFeatures feat = {0};
    vkGetPhysicalDeviceFeatures(device, &feat);
    vkEnumerateDeviceExtensionProperties(device, NULL, &count, NULL);
    se_sbuffer(VkExtensionProperties) availableExtensions = {0};
    se_sbuffer_construct(availableExtensions, count, allocator);
    se_sbuffer_set_size(availableExtensions, count);
    vkEnumerateDeviceExtensionProperties(device, NULL, &count, availableExtensions);
    bool isRequiredExtensionAvailable;
    bool result = true;
    for (size_t requiredIt = 0; requiredIt < numExtensions; requiredIt++)
    {
        isRequiredExtensionAvailable = false;
        for (size_t availableIt = 0; availableIt < se_sbuffer_size(availableExtensions); availableIt++)
        {
            if (strcmp(availableExtensions[availableIt].extensionName, extensions[requiredIt]) == 0)
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
    se_sbuffer_destroy(availableExtensions);
    return result;
};

bool se_does_physical_device_supports_required_features(VkPhysicalDevice device, VkPhysicalDeviceFeatures* requiredFeatures)
{
    // VkPhysicalDeviceFeatures is just a collection of VkBool32 values, so we can iterate over it like an array
    VkPhysicalDeviceFeatures supportedFeatures = {0};
    vkGetPhysicalDeviceFeatures(device, &supportedFeatures);
    VkBool32* requiredArray = (VkBool32*)requiredFeatures;
    VkBool32* supportedArray = (VkBool32*)&supportedFeatures;
    for (size_t it = 0; it < sizeof(VkPhysicalDeviceFeatures) / sizeof(VkBool32); it++)
    {
        if (!(!requiredArray[it] || supportedArray[it]))
        {
            return false;
        }
    }
    return true;
}

VkImageType se_pick_image_type(VkExtent3D imageExtent)
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

VkCommandBuffer se_create_command_buffer(VkDevice device, VkCommandPool pool, VkCommandBufferLevel level)
{
    VkCommandBuffer buffer;
    VkCommandBufferAllocateInfo allocInfo = (VkCommandBufferAllocateInfo)
    {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext              = NULL,
        .commandPool        = pool,
        .level              = level,
        .commandBufferCount = 1,
    };
    se_vk_check(vkAllocateCommandBuffers(device, &allocInfo, &buffer));
    return buffer;
};

VkShaderModule se_create_shader_module(VkDevice device, uint32_t* bytecode, size_t bytecodeSIze, VkAllocationCallbacks* allocationCb)
{
    VkShaderModuleCreateInfo createInfo = (VkShaderModuleCreateInfo)
    {
        .sType      = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext      = NULL,
        .flags      = 0,
        .codeSize   = bytecodeSIze,
        .pCode      = bytecode,
    };
    VkShaderModule shaderModule;
    se_vk_check(vkCreateShaderModule(device, &createInfo, allocationCb, &shaderModule));
    return shaderModule;
}

void se_destroy_shader_module(VkDevice device, VkShaderModule module, VkAllocationCallbacks* allocationCb)
{
    vkDestroyShaderModule(device, module, allocationCb);
}

bool se_get_memory_type_index(VkPhysicalDeviceMemoryProperties* props, uint32_t typeBits, VkMemoryPropertyFlags properties, uint32_t* result)
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

SeTextureFormat se_to_texture_format(VkFormat vkFormat)
{
    switch (vkFormat)
    {
        case VK_FORMAT_R8G8B8A8_SRGB: return SE_RGBA_8;
        case VK_FORMAT_R32G32B32A32_SFLOAT: return SE_RGBA_32F;
    }
    se_assert(!"Unsupported VkFormat");
    return (SeTextureFormat)0;
}

VkFormat se_to_vk_format(SeTextureFormat format)
{
    switch (format)
    {
        case SE_RGBA_8: return VK_FORMAT_R8G8B8A8_SRGB;
        case SE_RGBA_32F: return VK_FORMAT_R32G32B32A32_SFLOAT;
    }
    se_assert(!"Unsupported TextureFormat");
    return (VkFormat)0;
}

VkAttachmentLoadOp se_to_vk_load_op(SeAttachmentLoadOp loadOp)
{
    switch (loadOp)
    {
        case SE_LOAD_CLEAR: return VK_ATTACHMENT_LOAD_OP_CLEAR;
        case SE_LOAD_LOAD: return VK_ATTACHMENT_LOAD_OP_LOAD;
        case SE_LOAD_NOTHING: return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    }
    se_assert(!"Unsupported SeAttachmentLoadOp");
    return (VkAttachmentLoadOp)0;
}

VkAttachmentStoreOp se_to_vk_store_op(SeAttachmentStoreOp storeOp)
{
    switch (storeOp)
    {
        case SE_STORE_STORE: return VK_ATTACHMENT_STORE_OP_STORE;
        case SE_STORE_NOTHING: return VK_ATTACHMENT_STORE_OP_DONT_CARE;
    }
    se_assert(!"Unsupported SeAttachmentStoreOp");
    return (VkAttachmentStoreOp)0;
}

VkPolygonMode se_to_vk_polygon_mode(SePipelinePoligonMode mode)
{
    switch (mode)
    {
        case SE_POLIGON_FILL: return VK_POLYGON_MODE_FILL;
        case SE_POLIGON_LINE: return VK_POLYGON_MODE_LINE;
        case SE_POLIGON_POINT: return VK_POLYGON_MODE_POINT;
    }
    se_assert(!"Unsupported PipelinePoligonMode");
    return (VkPolygonMode)0;
}

VkCullModeFlags se_to_vk_cull_mode(SePipelineCullMode mode)
{
    switch (mode)
    {
        case SE_CULL_NONE: return VK_CULL_MODE_NONE;
        case SE_CULL_FRONT: return VK_CULL_MODE_FRONT_BIT;
        case SE_CULL_BACK: return VK_CULL_MODE_BACK_BIT;
        case SE_CULL_FRONT_BACK: return VK_CULL_MODE_FRONT_AND_BACK;
    }
    se_assert(!"Unsupported PipelineCullMode");
    return (VkCullModeFlags)0;
}

VkFrontFace se_to_vk_front_face(SePipelineFrontFace frontFace)
{
    switch (frontFace)
    {
        case SE_CLOCKWISE: return VK_FRONT_FACE_CLOCKWISE;
        case SE_COUNTER_CLOCKWISE: return VK_FRONT_FACE_COUNTER_CLOCKWISE;
    }
    se_assert(!"Unsupported SePipelineFrontFace");
    return (VkFrontFace)0;
}

VkSampleCountFlagBits se_to_vk_sample_count(SeMultisamplingType multisample)
{
    switch (multisample)
    {
        case SE_SAMPLE_1: return VK_SAMPLE_COUNT_1_BIT;
        case SE_SAMPLE_2: return VK_SAMPLE_COUNT_2_BIT;
        case SE_SAMPLE_4: return VK_SAMPLE_COUNT_4_BIT;
        case SE_SAMPLE_8: return VK_SAMPLE_COUNT_8_BIT;
    }
    se_assert(!"Unsupported SeMultisamplingType");
    return 0;
}

VkSampleCountFlagBits se_pick_sample_count(VkSampleCountFlags desired, VkSampleCountFlags supported)
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

VkStencilOp se_to_vk_stencil_op(SeStencilOp op)
{
    switch (op)
    {
        case SE_STENCIL_KEEP:                   return VK_STENCIL_OP_KEEP;
        case SE_STENCIL_ZERO:                   return VK_STENCIL_OP_ZERO;
        case SE_STENCIL_REPLACE:                return VK_STENCIL_OP_REPLACE;
        case SE_STENCIL_INCREMENT_AND_CLAMP:    return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
        case SE_STENCIL_DECREMENT_AND_CLAMP:    return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
        case SE_STENCIL_INVERT:                 return VK_STENCIL_OP_INVERT;
        case SE_STENCIL_INCREMENT_AND_WRAP:     return VK_STENCIL_OP_INCREMENT_AND_WRAP;
        case SE_STENCIL_DECREMENT_AND_WRAP:     return VK_STENCIL_OP_DECREMENT_AND_WRAP;
    }
    se_assert(!"Unsupported SeStencilOp");
    return (VkStencilOp)0;
}

VkCompareOp se_to_vk_compare_op(SeCompareOp op)
{
    switch (op)
    {
        case SE_COMPARE_NEVER:              return VK_COMPARE_OP_NEVER;
        case SE_COMPARE_LESS:               return VK_COMPARE_OP_LESS;
        case SE_COMPARE_EQUAL:              return VK_COMPARE_OP_EQUAL;
        case SE_COMPARE_LESS_OR_EQUAL:      return VK_COMPARE_OP_LESS_OR_EQUAL;
        case SE_COMPARE_GREATER:            return VK_COMPARE_OP_GREATER;
        case SE_COMPARE_NOT_EQUAL:          return VK_COMPARE_OP_NOT_EQUAL;
        case SE_COMPARE_GREATER_OR_EQUAL:   return VK_COMPARE_OP_GREATER_OR_EQUAL;
        case SE_COMPARE_ALWAYS:             return VK_COMPARE_OP_ALWAYS;
    }
    se_assert(!"Unsupported CompareOp");
    return (VkCompareOp)0;
}

VkBool32 se_to_vk_bool(bool value)
{
    return value ? VK_TRUE : VK_FALSE;
}

VkShaderStageFlags se_to_vk_stage_flags(SeProgramStageFlags stages)
{
    return
        (VkShaderStageFlags)(stages & (uint32_t)SE_STAGE_VERTEX     ? VK_SHADER_STAGE_VERTEX_BIT    : 0) |
        (VkShaderStageFlags)(stages & (uint32_t)SE_STAGE_FRAGMENT   ? VK_SHADER_STAGE_FRAGMENT_BIT  : 0) |
        (VkShaderStageFlags)(stages & (uint32_t)SE_STAGE_COMPUTE    ? VK_SHADER_STAGE_COMPUTE_BIT   : 0);
}

VkPipelineShaderStageCreateInfo se_shader_stage_create_info(VkShaderStageFlagBits stage, VkShaderModule module, const char* pName)
{
    return (VkPipelineShaderStageCreateInfo)
    {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext                  = NULL,
        .flags                  = 0,
        .stage                  = stage,
        .module                 = module,
        .pName                  = pName,
        .pSpecializationInfo    = NULL,
    };
}

VkPipelineVertexInputStateCreateInfo se_vertex_input_state_create_info(uint32_t bindingsCount, const VkVertexInputBindingDescription* bindingDescs, uint32_t attrCount, const VkVertexInputAttributeDescription* attrDescs)
{
    return (VkPipelineVertexInputStateCreateInfo)
    {
        .sType                              = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext                              = NULL,
        .flags                              = 0,
        .vertexBindingDescriptionCount      = bindingsCount,
        .pVertexBindingDescriptions         = bindingDescs,
        .vertexAttributeDescriptionCount    = attrCount,
        .pVertexAttributeDescriptions       = attrDescs,
    };
}

VkPipelineInputAssemblyStateCreateInfo se_input_assembly_state_create_info(VkPrimitiveTopology topology, VkBool32 primitiveRestartEnable)
{
    return (VkPipelineInputAssemblyStateCreateInfo)
    {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .pNext                  = NULL,
        .flags                  = 0,
        .topology               = topology,
        .primitiveRestartEnable = primitiveRestartEnable,
    };
}

SeVkViewportScissor se_default_viewport_scissor(uint32_t width, uint32_t height)
{
    return (SeVkViewportScissor)
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
            .offset = (VkOffset2D){ 0, 0 },
            .extent = (VkExtent2D){ width, height },
        }
    };
}

VkPipelineViewportStateCreateInfo se_viewport_state_create_info(VkViewport* viewport, VkRect2D* scissor)
{
    return (VkPipelineViewportStateCreateInfo)
    {
        .sType          = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext          = NULL,
        .flags          = 0,
        .viewportCount  = 1,
        .pViewports     = viewport,
        .scissorCount   = 1,
        .pScissors      = scissor,
    };
}

VkPipelineRasterizationStateCreateInfo se_rasterization_state_create_info(VkPolygonMode polygonMode, VkCullModeFlags cullMode, VkFrontFace frontFace)
{
    return (VkPipelineRasterizationStateCreateInfo)
    {
        .sType                      = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .pNext                      = NULL,
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

VkPipelineMultisampleStateCreateInfo se_multisample_state_create_info(VkSampleCountFlagBits resterizationSamples)
{
    return (VkPipelineMultisampleStateCreateInfo)
    {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .pNext                  = NULL,
        .flags                  = 0,
        .rasterizationSamples   = resterizationSamples,
        .sampleShadingEnable    = VK_FALSE,
        .minSampleShading       = 1.0f,
        .pSampleMask            = NULL,
        .alphaToCoverageEnable  = VK_FALSE,
        .alphaToOneEnable       = VK_FALSE,
    };
}

VkPipelineColorBlendStateCreateInfo se_color_blending_create_info(VkPipelineColorBlendAttachmentState* colorBlendAttachmentStates, uint32_t numStates)
{
    return (VkPipelineColorBlendStateCreateInfo)
    {
        .sType              = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext              = NULL,
        .flags              = 0,
        .logicOpEnable      = VK_FALSE,
        .logicOp            = VK_LOGIC_OP_COPY, // optional if logicOpEnable == VK_FALSE
        .attachmentCount    = numStates,
        .pAttachments       = colorBlendAttachmentStates,
        .blendConstants     = { 0.0f, 0.0f, 0.0f, 0.0f }, // optional, because color blending is disabled in colorBlendAttachments
    };
}

VkPipelineDynamicStateCreateInfo se_dynamic_state_default_create_info()
{
    static VkDynamicState dynamicStates[] =
    {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    return (VkPipelineDynamicStateCreateInfo)
    {
        .sType              = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pNext              = NULL,
        .flags              = 0,
        .dynamicStateCount  = se_array_size(dynamicStates),
        .pDynamicStates     = dynamicStates,
    };
}

VkAccessFlags se_image_layout_to_access_flags(VkImageLayout layout)
{
    switch (layout)
    {
        case VK_IMAGE_LAYOUT_UNDEFINED:                         return (VkAccessFlags)0;
        case VK_IMAGE_LAYOUT_GENERAL:                           return VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:          return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:  return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:   return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:          return VK_ACCESS_SHADER_READ_BIT;
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:              return VK_ACCESS_TRANSFER_READ_BIT;
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:              return VK_ACCESS_TRANSFER_WRITE_BIT;
        case VK_IMAGE_LAYOUT_PREINITIALIZED:                    return VK_ACCESS_MEMORY_WRITE_BIT;
        case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:                   return VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
        default: se_assert(!"Unsupported VkImageLayout");
    }
    return (VkAccessFlags)0;
}

VkPipelineStageFlags se_image_layout_to_pipeline_stage_flags(VkImageLayout layout)
{
    switch (layout)
    {
        case VK_IMAGE_LAYOUT_UNDEFINED:                         return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:              return VK_PIPELINE_STAGE_TRANSFER_BIT;
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:          return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:          return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:  return VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL:          return VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:                   return VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT; // is this correct ?
        default: se_assert(!"Unsupported VkImageLayout to VkPipelineStageFlags conversion");
    }
    return (VkPipelineStageFlags)0;
}
