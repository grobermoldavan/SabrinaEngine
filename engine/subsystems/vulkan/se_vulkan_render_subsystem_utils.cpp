
#include <string.h>

#include "se_vulkan_render_subsystem_utils.hpp"
#include "engine/render_abstraction_interface.hpp"
#include "engine/se_math.hpp"
#include "engine/containers.hpp"
#include "engine/debug.hpp"

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

DynamicArray<VkLayerProperties> se_vk_utils_get_available_validation_layers(SeAllocatorBindings& allocator)
{
    uint32_t count;
    DynamicArray<VkLayerProperties> result;
    vkEnumerateInstanceLayerProperties(&count, nullptr);
    dynamic_array::construct(result, allocator, count);
    vkEnumerateInstanceLayerProperties(&count, dynamic_array::raw(result));
    dynamic_array::force_set_size(result, count);
    return result;
}

DynamicArray<VkExtensionProperties> se_vk_utils_get_available_instance_extensions(SeAllocatorBindings& allocator)
{
    uint32_t count;
    DynamicArray<VkExtensionProperties> result;
    vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
    dynamic_array::construct(result, allocator, count);
    vkEnumerateInstanceExtensionProperties(nullptr, &count, dynamic_array::raw(result));
    dynamic_array::force_set_size(result, count);
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

VkDebugUtilsMessengerEXT se_vk_utils_create_debug_messenger(VkDebugUtilsMessengerCreateInfoEXT* createInfo, VkInstance instance, VkAllocationCallbacks* callbacks)
{
    VkDebugUtilsMessengerEXT messenger;
    PFN_vkCreateDebugUtilsMessengerEXT CreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    se_assert(CreateDebugUtilsMessengerEXT);
    se_vk_check(CreateDebugUtilsMessengerEXT(instance, createInfo, callbacks, &messenger));
    return messenger;
}

void se_vk_utils_destroy_debug_messenger(VkInstance instance, VkDebugUtilsMessengerEXT messenger, VkAllocationCallbacks* callbacks)
{
    vkDestroyDebugUtilsMessengerEXT(instance, messenger, callbacks);
}

VkCommandPool se_vk_utils_create_command_pool(VkDevice device, uint32_t queueFamilyIndex, VkAllocationCallbacks* callbacks, VkCommandPoolCreateFlags flags)
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

void se_vk_utils_destroy_command_pool(VkCommandPool pool, VkDevice device, VkAllocationCallbacks* callbacks)
{
    vkDestroyCommandPool(device, pool, callbacks);
}

SeVkSwapChainSupportDetails se_vk_utils_create_swap_chain_support_details(VkSurfaceKHR surface, VkPhysicalDevice device, SeAllocatorBindings& allocator)
{
    SeVkSwapChainSupportDetails result = {0};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &result.capabilities);
    uint32_t count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &count, nullptr);
    if (count != 0)
    {
        dynamic_array::construct(result.formats, allocator, count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &count, dynamic_array::raw(result.formats));
        dynamic_array::force_set_size(result.formats, count);
    }
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &count, nullptr);
    if (count != 0)
    {
        dynamic_array::construct(result.presentModes, allocator, count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &count, dynamic_array::raw(result.presentModes));
        dynamic_array::force_set_size(result.presentModes, count);
    }
    return result;
}

void se_vk_utils_destroy_swap_chain_support_details(SeVkSwapChainSupportDetails& details)
{
    dynamic_array::destroy(details.formats);
    dynamic_array::destroy(details.presentModes);
}

VkSurfaceFormatKHR se_vk_utils_choose_swap_chain_surface_format(const DynamicArray<VkSurfaceFormatKHR>& available)
{
    for (auto it : available)
    {
        const VkSurfaceFormatKHR& value = iter::value(it);
        if (value.format == /*VK_FORMAT_B8G8R8A8_SRGB*/ VK_FORMAT_R8G8B8A8_SRGB && value.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            return value;
        }
    }
    return available[0];
}

VkPresentModeKHR se_vk_utils_choose_swap_chain_surface_present_mode(const DynamicArray<VkPresentModeKHR>& available)
{
    for (auto it : available)
    {
        const VkPresentModeKHR& value = iter::value(it);
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

uint32_t se_vk_utils_pick_graphics_queue(const DynamicArray<VkQueueFamilyProperties>& familyProperties)
{
    for (auto it : familyProperties)
    {
        if (iter::value(it).queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            return (uint32_t)iter::index(it);
        }
    }
    return SE_VK_INVALID_QUEUE;
}

uint32_t se_vk_utils_pick_present_queue(const DynamicArray<VkQueueFamilyProperties>& familyProperties, VkPhysicalDevice device, VkSurfaceKHR surface)
{
    for (auto it : familyProperties)
    {
        const uint32_t index = (uint32_t)iter::index(it);
        VkBool32 isSupported;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, index, surface, &isSupported);
        if (isSupported)
        {
            return index;
        }
    }
    return SE_VK_INVALID_QUEUE;
}

uint32_t se_vk_utils_pick_transfer_queue(const DynamicArray<VkQueueFamilyProperties>& familyProperties)
{
    for (auto it : familyProperties)
    {
        if (iter::value(it).queueFlags & VK_QUEUE_TRANSFER_BIT)
        {
            return (uint32_t)iter::index(it);
        }
    }
    return SE_VK_INVALID_QUEUE;
}

uint32_t se_vk_utils_pick_compute_queue(const DynamicArray<VkQueueFamilyProperties>& familyProperties)
{
    for (auto it : familyProperties)
    {
        if (iter::value(it).queueFlags & VK_QUEUE_COMPUTE_BIT)
        {
            return (uint32_t)iter::index(it);
        }
    }
    return SE_VK_INVALID_QUEUE;
}

DynamicArray<VkDeviceQueueCreateInfo> se_vk_utils_get_queue_create_infos(uint32_t* queues, size_t numQueues, SeAllocatorBindings& allocator)
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
    DynamicArray<VkDeviceQueueCreateInfo> result;
    dynamic_array::construct(result, allocator, numUniqueIndices);
    for (size_t it = 0; it < numUniqueIndices; it++)
    {
        dynamic_array::push(result,
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

DynamicArray<VkPhysicalDevice> se_vk_utils_get_available_physical_devices(VkInstance instance, SeAllocatorBindings& allocator)
{
    uint32_t count;
    DynamicArray<VkPhysicalDevice> result;
    vkEnumeratePhysicalDevices(instance, &count, nullptr);
    dynamic_array::construct(result, allocator, count);
    vkEnumeratePhysicalDevices(instance, &count, dynamic_array::raw(result));
    dynamic_array::force_set_size(result, count);
    return result;
};

DynamicArray<VkQueueFamilyProperties> se_vk_utils_get_physical_device_queue_family_properties(VkPhysicalDevice physicalDevice, SeAllocatorBindings& allocator)
{
    uint32_t count;
    DynamicArray<VkQueueFamilyProperties> familyProperties;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &count, nullptr);
    dynamic_array::construct(familyProperties, allocator, count);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &count, dynamic_array::raw(familyProperties));
    dynamic_array::force_set_size(familyProperties, count);
    return familyProperties;
}

bool se_vk_utils_does_physical_device_supports_required_extensions(VkPhysicalDevice device, const char** extensions, size_t numExtensions, SeAllocatorBindings& allocator)
{
    uint32_t count;
    VkPhysicalDeviceFeatures feat = {0};
    vkGetPhysicalDeviceFeatures(device, &feat);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
    DynamicArray<VkExtensionProperties> availableExtensions;
    dynamic_array::construct(availableExtensions, allocator, count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, dynamic_array::raw(availableExtensions));
    dynamic_array::force_set_size(availableExtensions, count);
    bool isRequiredExtensionAvailable;
    bool result = true;
    for (size_t requiredIt = 0; requiredIt < numExtensions; requiredIt++)
    {
        isRequiredExtensionAvailable = false;
        for (auto availableIt : availableExtensions)
        {
            if (strcmp(iter::value(availableIt).extensionName, extensions[requiredIt]) == 0)
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
    dynamic_array::destroy(availableExtensions);
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

VkShaderModule se_vk_utils_create_shader_module(VkDevice device, const uint32_t* bytecode, size_t bytecodeSize, VkAllocationCallbacks* allocationCb)
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

void se_vk_utils_destroy_shader_module(VkDevice device, VkShaderModule module, VkAllocationCallbacks* allocationCb)
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

SeTextureFormat se_vk_utils_to_texture_format(VkFormat vkFormat)
{
    switch (vkFormat)
    {
        case VK_FORMAT_R8G8B8A8_SRGB: return SE_TEXTURE_FORMAT_RGBA_8;
        case VK_FORMAT_R32G32B32A32_SFLOAT: return SE_TEXTURE_FORMAT_RGBA_32F;
    }
    se_assert(!"Unsupported VkFormat");
    return (SeTextureFormat)0;
}

VkFormat se_vk_utils_to_vk_format(SeTextureFormat format)
{
    switch (format)
    {
        case SE_TEXTURE_FORMAT_RGBA_8: return VK_FORMAT_R8G8B8A8_SRGB;
        case SE_TEXTURE_FORMAT_RGBA_32F: return VK_FORMAT_R32G32B32A32_SFLOAT;
    }
    se_assert(!"Unsupported TextureFormat");
    return (VkFormat)0;
}

VkPolygonMode se_vk_utils_to_vk_polygon_mode(SePipelinePolygonMode mode)
{
    switch (mode)
    {
        case SE_PIPELINE_POLYGON_FILL_MODE_FILL: return VK_POLYGON_MODE_FILL;
        case SE_PIPELINE_POLYGON_FILL_MODE_LINE: return VK_POLYGON_MODE_LINE;
        case SE_PIPELINE_POLYGON_FILL_MODE_POINT: return VK_POLYGON_MODE_POINT;
    }
    se_assert(!"Unsupported SePipelinePolygonMode");
    return (VkPolygonMode)0;
}

VkCullModeFlags se_vk_utils_to_vk_cull_mode(SePipelineCullMode mode)
{
    switch (mode)
    {
        case SE_PIPELINE_CULL_MODE_NONE: return VK_CULL_MODE_NONE;
        case SE_PIPELINE_CULL_MODE_FRONT: return VK_CULL_MODE_FRONT_BIT;
        case SE_PIPELINE_CULL_MODE_BACK: return VK_CULL_MODE_BACK_BIT;
        case SE_PIPELINE_CULL_MODE_FRONT_BACK: return VK_CULL_MODE_FRONT_AND_BACK;
    }
    se_assert(!"Unsupported PipelineCullMode");
    return (VkCullModeFlags)0;
}

VkFrontFace se_vk_utils_to_vk_front_face(SePipelineFrontFace frontFace)
{
    switch (frontFace)
    {
        case SE_PIPELINE_FRONT_FACE_CLOCKWISE: return VK_FRONT_FACE_CLOCKWISE;
        case SE_PIPELINE_FRONT_FACE_COUNTER_CLOCKWISE: return VK_FRONT_FACE_COUNTER_CLOCKWISE;
    }
    se_assert(!"Unsupported SePipelineFrontFace");
    return (VkFrontFace)0;
}

VkSampleCountFlagBits se_vk_utils_to_vk_sample_count(SeSamplingType sampling)
{
    switch (sampling)
    {
        case SE_SAMPLING_1: return (VkSampleCountFlagBits)VK_SAMPLE_COUNT_1_BIT;
        case SE_SAMPLING_2: return (VkSampleCountFlagBits)VK_SAMPLE_COUNT_2_BIT;
        case SE_SAMPLING_4: return (VkSampleCountFlagBits)VK_SAMPLE_COUNT_4_BIT;
        case SE_SAMPLING_8: return (VkSampleCountFlagBits)VK_SAMPLE_COUNT_8_BIT;
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
        case SE_STENCIL_OP_KEEP:                return VK_STENCIL_OP_KEEP;
        case SE_STENCIL_OP_ZERO:                return VK_STENCIL_OP_ZERO;
        case SE_STENCIL_OP_REPLACE:             return VK_STENCIL_OP_REPLACE;
        case SE_STENCIL_OP_INCREMENT_AND_CLAMP: return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
        case SE_STENCIL_OP_DECREMENT_AND_CLAMP: return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
        case SE_STENCIL_OP_INVERT:              return VK_STENCIL_OP_INVERT;
        case SE_STENCIL_OP_INCREMENT_AND_WRAP:  return VK_STENCIL_OP_INCREMENT_AND_WRAP;
        case SE_STENCIL_OP_DECREMENT_AND_WRAP:  return VK_STENCIL_OP_DECREMENT_AND_WRAP;
    }
    se_assert(!"Unsupported SeStencilOp");
    return (VkStencilOp)0;
}

VkCompareOp se_vk_utils_to_vk_compare_op(SeCompareOp op)
{
    switch (op)
    {
        case SE_COMPARE_OP_NEVER:               return VK_COMPARE_OP_NEVER;
        case SE_COMPARE_OP_LESS:                return VK_COMPARE_OP_LESS;
        case SE_COMPARE_OP_EQUAL:               return VK_COMPARE_OP_EQUAL;
        case SE_COMPARE_OP_LESS_OR_EQUAL:       return VK_COMPARE_OP_LESS_OR_EQUAL;
        case SE_COMPARE_OP_GREATER:             return VK_COMPARE_OP_GREATER;
        case SE_COMPARE_OP_NOT_EQUAL:           return VK_COMPARE_OP_NOT_EQUAL;
        case SE_COMPARE_OP_GREATER_OR_EQUAL:    return VK_COMPARE_OP_GREATER_OR_EQUAL;
        case SE_COMPARE_OP_ALWAYS:              return VK_COMPARE_OP_ALWAYS;
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
        .blendConstants     = { 0.0f, 0.0f, 0.0f, 0.0f }, // optional, because color blending is disabled in colorBlendAttachments
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
        case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:                   return VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT; // is this correct ?
        default: se_assert(!"Unsupported VkImageLayout to VkPipelineStageFlags conversion");
    }
    return (VkPipelineStageFlags)0;
}
