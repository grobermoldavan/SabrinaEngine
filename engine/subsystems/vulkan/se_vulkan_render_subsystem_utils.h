#ifndef _SE_VULKAN_RENDER_SUBSYSTEM_UTILS_H_
#define _SE_VULKAN_RENDER_SUBSYSTEM_UTILS_H_

#include "se_vulkan_render_subsystem_base.h"
#include "engine/containers.h"

#define SE_VK_INVALID_QUEUE (~0u)

typedef struct SeVkSwapChainSupportDetails
{
    VkSurfaceCapabilitiesKHR        capabilities;
    se_sbuffer(VkSurfaceFormatKHR)  formats;
    se_sbuffer(VkPresentModeKHR)    presentModes;
} SeVkSwapChainSupportDetails;

typedef struct SeVkViewportScissor
{
    VkViewport viewport;
    VkRect2D scissor;
} SeVkViewportScissor;

const char**                            se_vk_utils_get_required_validation_layers(size_t* outNum);
const char**                            se_vk_utils_get_required_instance_extensions(size_t* outNum);
const char**                            se_vk_utils_get_required_device_extensions(size_t* outNum);
se_sbuffer(VkLayerProperties)           se_vk_utils_get_available_validation_layers(struct SeAllocatorBindings* allocator);
se_sbuffer(VkExtensionProperties)       se_vk_utils_get_available_instance_extensions(struct SeAllocatorBindings* allocator);
VkDebugUtilsMessengerCreateInfoEXT      se_vk_utils_get_debug_messenger_create_info(PFN_vkDebugUtilsMessengerCallbackEXT callback, void* userData);
VkDebugUtilsMessengerEXT                se_vk_utils_create_debug_messenger(VkDebugUtilsMessengerCreateInfoEXT* createInfo, VkInstance instance, VkAllocationCallbacks* callbacks);
void                                    se_vk_utils_destroy_debug_messenger(VkInstance instance, VkDebugUtilsMessengerEXT messenger, VkAllocationCallbacks* callbacks);
VkCommandPool                           se_vk_utils_create_command_pool(VkDevice device, uint32_t queueFamilyIndex, VkAllocationCallbacks* callbacks, VkCommandPoolCreateFlags flags);
void                                    se_vk_utils_destroy_command_pool(VkCommandPool pool, VkDevice device, VkAllocationCallbacks* callbacks);
SeVkSwapChainSupportDetails             se_vk_utils_create_swap_chain_support_details(VkSurfaceKHR surface, VkPhysicalDevice device, struct SeAllocatorBindings* allocator);
void                                    se_vk_utils_destroy_swap_chain_support_details(SeVkSwapChainSupportDetails* details);
VkSurfaceFormatKHR                      se_vk_utils_choose_swap_chain_surface_format(se_sbuffer(VkSurfaceFormatKHR) available);
VkPresentModeKHR                        se_vk_utils_choose_swap_chain_surface_present_mode(se_sbuffer(VkPresentModeKHR) available);
VkExtent2D                              se_vk_utils_choose_swap_chain_extent(uint32_t windowWidth, uint32_t windowHeight, VkSurfaceCapabilitiesKHR* capabilities);
uint32_t                                se_vk_utils_pick_graphics_queue(se_sbuffer(VkQueueFamilyProperties) familyProperties);
uint32_t                                se_vk_utils_pick_present_queue(se_sbuffer(VkQueueFamilyProperties) familyProperties, VkPhysicalDevice device, VkSurfaceKHR surface);
uint32_t                                se_vk_utils_pick_transfer_queue(se_sbuffer(VkQueueFamilyProperties) familyProperties);
uint32_t                                se_vk_utils_pick_compute_queue(se_sbuffer(VkQueueFamilyProperties) familyProperties);
se_sbuffer(VkDeviceQueueCreateInfo)     se_vk_utils_get_queue_create_infos(uint32_t* queues, size_t numQueues, struct SeAllocatorBindings* allocator);
VkCommandPoolCreateInfo                 se_vk_utils_command_pool_create_info(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags);
bool                                    se_vk_utils_pick_depth_stencil_format(VkPhysicalDevice physicalDevice, VkFormat* result);
se_sbuffer(VkPhysicalDevice)            se_vk_utils_get_available_physical_devices(VkInstance instance, struct SeAllocatorBindings* allocator);
se_sbuffer(VkQueueFamilyProperties)     se_vk_utils_get_physical_device_queue_family_properties(VkPhysicalDevice physicalDevice, struct SeAllocatorBindings* allocator);
bool                                    se_vk_utils_does_physical_device_supports_required_extensions(VkPhysicalDevice device, const char** extensions, size_t numExtensions, struct SeAllocatorBindings* allocator);
VkImageType                             se_vk_utils_pick_image_type(VkExtent3D imageExtent);
VkCommandBuffer                         se_vk_utils_create_command_buffer(VkDevice device, VkCommandPool pool, VkCommandBufferLevel level);
VkShaderModule                          se_vk_utils_create_shader_module(VkDevice device, const uint32_t* bytecode, size_t bytecodeSIze, VkAllocationCallbacks* allocationCb);
void                                    se_vk_utils_destroy_shader_module(VkDevice device, VkShaderModule module, VkAllocationCallbacks* allocationCb);
bool                                    se_vk_utils_get_memory_type_index(VkPhysicalDeviceMemoryProperties* props, uint32_t typeBits, VkMemoryPropertyFlags properties, uint32_t* result);
enum SeTextureFormat                    se_vk_utils_to_texture_format(enum VkFormat vkFormat);
VkFormat                                se_vk_utils_to_vk_format(enum SeTextureFormat format);
VkAttachmentLoadOp                      se_vk_utils_to_vk_load_op(enum SeAttachmentLoadOp loadOp);
VkAttachmentStoreOp                     se_vk_utils_to_vk_store_op(enum SeAttachmentStoreOp storeOp);
VkPolygonMode                           se_vk_utils_to_vk_polygon_mode(enum SePipelinePoligonMode mode);
VkCullModeFlags                         se_vk_utils_to_vk_cull_mode(enum SePipelineCullMode mode);
VkFrontFace                             se_vk_utils_to_vk_front_face(enum SePipelineFrontFace frontFace);
VkSampleCountFlagBits                   se_vk_utils_to_vk_sample_count(enum SeSamplingType sampling);
VkSampleCountFlagBits                   se_vk_utils_pick_sample_count(VkSampleCountFlags desired, VkSampleCountFlags supported);
VkStencilOp                             se_vk_utils_to_vk_stencil_op(enum SeStencilOp op);
VkCompareOp                             se_vk_utils_to_vk_compare_op(enum SeCompareOp op);
VkBool32                                se_vk_utils_to_vk_bool(bool value);
VkPipelineVertexInputStateCreateInfo    se_vk_utils_vertex_input_state_create_info(uint32_t bindingsCount, const VkVertexInputBindingDescription* bindingDescs, uint32_t attrCount, const VkVertexInputAttributeDescription* attrDescs);
VkPipelineInputAssemblyStateCreateInfo  se_vk_utils_input_assembly_state_create_info(VkPrimitiveTopology topology, VkBool32 primitiveRestartEnable);
SeVkViewportScissor                     se_vk_utils_default_viewport_scissor(uint32_t width, uint32_t height);
VkPipelineViewportStateCreateInfo       se_vk_utils_viewport_state_create_info(VkViewport* viewport, VkRect2D* scissor);
VkPipelineRasterizationStateCreateInfo  se_vk_utils_rasterization_state_create_info(VkPolygonMode polygonMode, VkCullModeFlags cullMode, VkFrontFace frontFace);
VkPipelineMultisampleStateCreateInfo    se_vk_utils_multisample_state_create_info(VkSampleCountFlagBits resterizationSamples);
VkPipelineColorBlendStateCreateInfo     se_vk_utils_color_blending_create_info(VkPipelineColorBlendAttachmentState* colorBlendAttachmentStates, uint32_t numStates);
VkPipelineDynamicStateCreateInfo        se_vk_utils_dynamic_state_default_create_info();
VkAccessFlags                           se_vk_utils_image_layout_to_access_flags(VkImageLayout layout);
VkPipelineStageFlags                    se_vk_utils_image_layout_to_pipeline_stage_flags(VkImageLayout layout);

#endif
