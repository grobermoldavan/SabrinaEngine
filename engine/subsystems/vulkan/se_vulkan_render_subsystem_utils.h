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

const char**                            se_get_required_validation_layers(size_t* outNum);
const char**                            se_get_required_instance_extensions(size_t* outNum);
const char**                            se_get_required_device_extensions(size_t* outNum);
se_sbuffer(VkLayerProperties)           se_get_available_validation_layers(struct SeAllocatorBindings* allocator);
se_sbuffer(VkExtensionProperties)       se_get_available_instance_extensions(struct SeAllocatorBindings* allocator);
VkDebugUtilsMessengerCreateInfoEXT      se_get_debug_messenger_create_info(PFN_vkDebugUtilsMessengerCallbackEXT callback, void* userData);
VkDebugUtilsMessengerEXT                se_create_debug_messenger(VkDebugUtilsMessengerCreateInfoEXT* createInfo, VkInstance instance, VkAllocationCallbacks* callbacks);
void                                    se_destroy_debug_messenger(VkInstance instance, VkDebugUtilsMessengerEXT messenger, VkAllocationCallbacks* callbacks);
VkCommandPool                           se_create_command_pool(VkDevice device, uint32_t queueFamilyIndex, VkAllocationCallbacks* callbacks, VkCommandPoolCreateFlags flags);
void                                    se_destroy_command_pool(VkCommandPool pool, VkDevice device, VkAllocationCallbacks* callbacks);
SeVkSwapChainSupportDetails             se_create_swap_chain_support_details(VkSurfaceKHR surface, VkPhysicalDevice device, struct SeAllocatorBindings* allocator);
void                                    se_destroy_swap_chain_support_details(SeVkSwapChainSupportDetails* details);
VkSurfaceFormatKHR                      se_choose_swap_chain_surface_format(se_sbuffer(VkSurfaceFormatKHR) available);
VkPresentModeKHR                        se_choose_swap_chain_surface_present_mode(se_sbuffer(VkPresentModeKHR) available);
VkExtent2D                              se_choose_swap_chain_extent(uint32_t windowWidth, uint32_t windowHeight, VkSurfaceCapabilitiesKHR* capabilities);
uint32_t                                se_pick_graphics_queue(se_sbuffer(VkQueueFamilyProperties) familyProperties);
uint32_t                                se_pick_present_queue(se_sbuffer(VkQueueFamilyProperties) familyProperties, VkPhysicalDevice device, VkSurfaceKHR surface);
uint32_t                                se_pick_transfer_queue(se_sbuffer(VkQueueFamilyProperties) familyProperties);
se_sbuffer(VkDeviceQueueCreateInfo)     se_get_queue_create_infos(uint32_t* queues, size_t numQueues, struct SeAllocatorBindings* allocator);
VkCommandPoolCreateInfo                 se_command_pool_create_info(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags);
bool                                    se_pick_depth_stencil_format(VkPhysicalDevice physicalDevice, VkFormat* result);
se_sbuffer(VkPhysicalDevice)            se_get_available_physical_devices(VkInstance instance, struct SeAllocatorBindings* allocator);
se_sbuffer(VkQueueFamilyProperties)     se_get_physical_device_queue_family_properties(VkPhysicalDevice physicalDevice, struct SeAllocatorBindings* allocator);
bool                                    se_does_physical_device_supports_required_extensions(VkPhysicalDevice device, const char** extensions, size_t numExtensions, struct SeAllocatorBindings* allocator);
bool                                    se_does_physical_device_supports_required_features(VkPhysicalDevice device, VkPhysicalDeviceFeatures* requiredFeatures);
VkImageType                             se_pick_image_type(VkExtent3D imageExtent);
VkCommandBuffer                         se_create_command_buffer(VkDevice device, VkCommandPool pool, VkCommandBufferLevel level);
VkShaderModule                          se_create_shader_module(VkDevice device, uint32_t* bytecode, size_t bytecodeSIze, VkAllocationCallbacks* allocationCb);
void                                    se_destroy_shader_module(VkDevice device, VkShaderModule module, VkAllocationCallbacks* allocationCb);
bool                                    se_get_memory_type_index(VkPhysicalDeviceMemoryProperties* props, uint32_t typeBits, VkMemoryPropertyFlags properties, uint32_t* result);
enum SeTextureFormat                    se_to_texture_format(enum VkFormat vkFormat);
VkFormat                                se_to_vk_format(enum SeTextureFormat format);
VkAttachmentLoadOp                      se_to_vk_load_op(enum SeAttachmentLoadOp loadOp);
VkAttachmentStoreOp                     se_to_vk_store_op(enum SeAttachmentStoreOp storeOp);
VkPolygonMode                           se_to_vk_polygon_mode(enum SePipelinePoligonMode mode);
VkCullModeFlags                         se_to_vk_cull_mode(enum SePipelineCullMode mode);
VkFrontFace                             se_to_vk_front_face(enum SePipelineFrontFace frontFace);
VkSampleCountFlagBits                   se_to_vk_sample_count(enum SeMultisamplingType multisample);
VkSampleCountFlagBits                   se_pick_sample_count(VkSampleCountFlags desired, VkSampleCountFlags supported);
VkStencilOp                             se_to_vk_stencil_op(enum SeStencilOp op);
VkCompareOp                             se_to_vk_compare_op(enum SeCompareOp op);
VkBool32                                se_to_vk_bool(bool value);
VkShaderStageFlags                      se_to_vk_stage_flags(enum SeProgramStageFlags stages);
VkPipelineShaderStageCreateInfo         se_shader_stage_create_info(VkShaderStageFlagBits stage, VkShaderModule module, const char* pName);
VkPipelineVertexInputStateCreateInfo    se_vertex_input_state_create_info(uint32_t bindingsCount, const VkVertexInputBindingDescription* bindingDescs, uint32_t attrCount, const VkVertexInputAttributeDescription* attrDescs);
VkPipelineInputAssemblyStateCreateInfo  se_input_assembly_state_create_info(VkPrimitiveTopology topology, VkBool32 primitiveRestartEnable);
SeVkViewportScissor                     se_default_viewport_scissor(uint32_t width, uint32_t height);
VkPipelineViewportStateCreateInfo       se_viewport_state_create_info(VkViewport* viewport, VkRect2D* scissor);
VkPipelineRasterizationStateCreateInfo  se_rasterization_state_create_info(VkPolygonMode polygonMode, VkCullModeFlags cullMode, VkFrontFace frontFace);
VkPipelineMultisampleStateCreateInfo    se_multisample_state_create_info(VkSampleCountFlagBits resterizationSamples);
VkPipelineColorBlendStateCreateInfo     se_color_blending_create_info(VkPipelineColorBlendAttachmentState* colorBlendAttachmentStates, uint32_t numStates);
VkPipelineDynamicStateCreateInfo        se_dynamic_state_default_create_info();
VkAccessFlags                           se_image_layout_to_access_flags(VkImageLayout layout);
VkPipelineStageFlags                    se_image_layout_to_pipeline_stage_flags(VkImageLayout layout);

#endif
