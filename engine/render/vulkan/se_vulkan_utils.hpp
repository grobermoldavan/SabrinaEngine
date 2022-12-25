#ifndef _SE_VULKAN_UTILS_H_
#define _SE_VULKAN_UTILS_H_

#include "se_vulkan_base.hpp"

#define SE_VK_INVALID_QUEUE (~0u)

#define se_vk_utils_is_depth_format(format)                 \
            ((format) == VK_FORMAT_D16_UNORM            ||  \
            (format) == VK_FORMAT_X8_D24_UNORM_PACK32   ||  \
            (format) == VK_FORMAT_D32_SFLOAT            ||  \
            (format) == VK_FORMAT_D16_UNORM_S8_UINT     ||  \
            (format) == VK_FORMAT_D24_UNORM_S8_UINT     ||  \
            (format) == VK_FORMAT_D32_SFLOAT_S8_UINT    )   
            
#define se_vk_utils_is_stencil_format(format)               \
            ((format) == VK_FORMAT_S8_UINT              ||  \
            (format) == VK_FORMAT_D16_UNORM_S8_UINT     ||  \
            (format) == VK_FORMAT_D24_UNORM_S8_UINT     ||  \
            (format) == VK_FORMAT_D32_SFLOAT_S8_UINT    )

#define se_vk_utils_is_depth_stencil_format(format)         \
            ((format) == VK_FORMAT_S8_UINT              ||  \
            (format) == VK_FORMAT_D16_UNORM             ||  \
            (format) == VK_FORMAT_X8_D24_UNORM_PACK32   ||  \
            (format) == VK_FORMAT_D32_SFLOAT            ||  \
            (format) == VK_FORMAT_D16_UNORM_S8_UINT     ||  \
            (format) == VK_FORMAT_D24_UNORM_S8_UINT     ||  \
            (format) == VK_FORMAT_D32_SFLOAT_S8_UINT    )      

struct SeVkSwapChainSupportDetails
{
    VkSurfaceCapabilitiesKHR            capabilities;
    DynamicArray<VkSurfaceFormatKHR>    formats;
    DynamicArray<VkPresentModeKHR>      presentModes;
};

struct SeVkViewportScissor
{
    VkViewport viewport;
    VkRect2D scissor;
};

struct SeVkFormatInfo
{
    enum struct Type : uint8_t
    {
        FLOAT,
        UINT,
        INT,
    };
    uint8_t numComponents;
    uint8_t componentsSizeBits;
    Type componentType;
    Type sampledType;
};

const char**                            se_vk_utils_get_required_validation_layers(size_t* outNum);
const char**                            se_vk_utils_get_required_instance_extensions(size_t* outNum);
const char**                            se_vk_utils_get_required_device_extensions(size_t* outNum);
DynamicArray<VkLayerProperties>         se_vk_utils_get_available_validation_layers(const AllocatorBindings& allocator);
DynamicArray<VkExtensionProperties>     se_vk_utils_get_available_instance_extensions(const AllocatorBindings& allocator);
VkDebugUtilsMessengerCreateInfoEXT      se_vk_utils_get_debug_messenger_create_info(PFN_vkDebugUtilsMessengerCallbackEXT callback, void* userData);
VkDebugUtilsMessengerEXT                se_vk_utils_create_debug_messenger(const VkDebugUtilsMessengerCreateInfoEXT* createInfo, VkInstance instance, const VkAllocationCallbacks* callbacks);
void                                    se_vk_utils_destroy_debug_messenger(VkInstance instance, VkDebugUtilsMessengerEXT messenger, const VkAllocationCallbacks* callbacks);
VkCommandPool                           se_vk_utils_create_command_pool(VkDevice device, uint32_t queueFamilyIndex, const VkAllocationCallbacks* callbacks, VkCommandPoolCreateFlags flags);
void                                    se_vk_utils_destroy_command_pool(VkCommandPool pool, VkDevice device, const VkAllocationCallbacks* callbacks);
SeVkSwapChainSupportDetails             se_vk_utils_create_swap_chain_support_details(VkSurfaceKHR surface, VkPhysicalDevice device, const AllocatorBindings& allocator);
void                                    se_vk_utils_destroy_swap_chain_support_details(SeVkSwapChainSupportDetails& details);
VkSurfaceFormatKHR                      se_vk_utils_choose_swap_chain_surface_format(const DynamicArray<VkSurfaceFormatKHR>& available);
VkPresentModeKHR                        se_vk_utils_choose_swap_chain_surface_present_mode(const DynamicArray<VkPresentModeKHR>& available);
VkExtent2D                              se_vk_utils_choose_swap_chain_extent(uint32_t windowWidth, uint32_t windowHeight, VkSurfaceCapabilitiesKHR* capabilities);
uint32_t                                se_vk_utils_pick_graphics_queue(const DynamicArray<VkQueueFamilyProperties>& familyProperties);
uint32_t                                se_vk_utils_pick_present_queue(const DynamicArray<VkQueueFamilyProperties>& familyProperties, VkPhysicalDevice device, VkSurfaceKHR surface);
uint32_t                                se_vk_utils_pick_transfer_queue(const DynamicArray<VkQueueFamilyProperties>& familyProperties);
uint32_t                                se_vk_utils_pick_compute_queue(const DynamicArray<VkQueueFamilyProperties>& familyProperties);
DynamicArray<VkDeviceQueueCreateInfo>   se_vk_utils_get_queue_create_infos(const uint32_t* queues, size_t numQueues, const AllocatorBindings& allocator);
VkCommandPoolCreateInfo                 se_vk_utils_command_pool_create_info(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags);
bool                                    se_vk_utils_pick_depth_stencil_format(VkPhysicalDevice physicalDevice, VkFormat* result);
DynamicArray<VkPhysicalDevice>          se_vk_utils_get_available_physical_devices(VkInstance instance, const AllocatorBindings& allocator);
DynamicArray<VkQueueFamilyProperties>   se_vk_utils_get_physical_device_queue_family_properties(VkPhysicalDevice physicalDevice, const AllocatorBindings& allocator);
bool                                    se_vk_utils_does_physical_device_supports_required_extensions(VkPhysicalDevice device, const char** extensions, size_t numExtensions, const struct AllocatorBindings& allocator);
VkImageType                             se_vk_utils_pick_image_type(VkExtent3D imageExtent);
VkCommandBuffer                         se_vk_utils_create_command_buffer(VkDevice device, VkCommandPool pool, VkCommandBufferLevel level);
VkShaderModule                          se_vk_utils_create_shader_module(VkDevice device, const uint32_t* bytecode, size_t bytecodeSIze, const VkAllocationCallbacks* allocationCb);
void                                    se_vk_utils_destroy_shader_module(VkDevice device, VkShaderModule module, const VkAllocationCallbacks* allocationCb);
bool                                    se_vk_utils_get_memory_type_index(VkPhysicalDeviceMemoryProperties* props, uint32_t typeBits, VkMemoryPropertyFlags properties, uint32_t* result);
SeTextureFormat                         se_vk_utils_to_se_texture_format(VkFormat vkFormat);
VkFormat                                se_vk_utils_to_vk_texture_format(SeTextureFormat format);
VkPolygonMode                           se_vk_utils_to_vk_polygon_mode(SePipelinePolygonMode mode);
VkCullModeFlags                         se_vk_utils_to_vk_cull_mode(SePipelineCullMode mode);
VkFrontFace                             se_vk_utils_to_vk_front_face(SePipelineFrontFace frontFace);
VkSampleCountFlagBits                   se_vk_utils_to_vk_sample_count(SeSamplingType sampling);
VkSampleCountFlagBits                   se_vk_utils_pick_sample_count(VkSampleCountFlags desired, VkSampleCountFlags supported);
VkStencilOp                             se_vk_utils_to_vk_stencil_op(SeStencilOp op);
VkCompareOp                             se_vk_utils_to_vk_compare_op(SeCompareOp op);
VkBool32                                se_vk_utils_to_vk_bool(bool value);
VkPipelineVertexInputStateCreateInfo    se_vk_utils_vertex_input_state_create_info(uint32_t bindingsCount, const VkVertexInputBindingDescription* bindingDescs, uint32_t attrCount, const VkVertexInputAttributeDescription* attrDescs);
VkPipelineInputAssemblyStateCreateInfo  se_vk_utils_input_assembly_state_create_info(VkPrimitiveTopology topology, VkBool32 primitiveRestartEnable);
SeVkViewportScissor                     se_vk_utils_default_viewport_scissor(uint32_t width, uint32_t height);
VkPipelineViewportStateCreateInfo       se_vk_utils_viewport_state_create_info(const VkViewport* viewport, const VkRect2D* scissor);
VkPipelineRasterizationStateCreateInfo  se_vk_utils_rasterization_state_create_info(VkPolygonMode polygonMode, VkCullModeFlags cullMode, VkFrontFace frontFace);
VkPipelineMultisampleStateCreateInfo    se_vk_utils_multisample_state_create_info(VkSampleCountFlagBits resterizationSamples);
VkPipelineColorBlendStateCreateInfo     se_vk_utils_color_blending_create_info(VkPipelineColorBlendAttachmentState* colorBlendAttachmentStates, uint32_t numStates);
VkPipelineDynamicStateCreateInfo        se_vk_utils_dynamic_state_default_create_info();
VkAccessFlags                           se_vk_utils_image_layout_to_access_flags(VkImageLayout layout);
VkPipelineStageFlags                    se_vk_utils_image_layout_to_pipeline_stage_flags(VkImageLayout layout);
VkAttachmentLoadOp                      se_vk_utils_to_vk_load_op(SeRenderTargetLoadOp loadOp);
SeVkFormatInfo                          se_vk_utils_get_format_info(VkFormat format);

#endif
