#ifndef _SE_VULKAN_RENDER_SUBSYSTEM_IN_FLIGHT_MANAGER_H_
#define _SE_VULKAN_RENDER_SUBSYSTEM_IN_FLIGHT_MANAGER_H_

#include <inttypes.h>
#include <stdbool.h>

#include "se_vulkan_render_subsystem_base.h"
#include "engine/containers.h"

/*
    This struct is a thin wrap over a VkDescriptorPool.
    It is meant to allocate only a single type of descriptor sets.
*/
typedef struct SeVkDescriptorSetPool
{
    VkDescriptorPool handle;
    size_t numAllocations;
    bool isLastAllocationSuccessful;
} SeVkDescriptorSetPool;

/*
    This is a collection of pools for a single render pipeline.
    Each entry of the pools array is an array of SeVkDescriptorSetPool
    structs. Each array contains pools used for allocating same
    descriptor sets.
    setPools[0] contains all pools for set 0.
    setPools[1] contains all pools for set 1.
    And so on.
    New pool is added to SeVkDescriptorSetPool array if there is no
    place for a new descriptor set allocation (isLastAllocationSuccessful
    value of every SeVkDescriptorSetPool is equals to false).
*/
typedef struct SeVkRenderPipelinePools
{
    se_sbuffer(se_sbuffer(SeVkDescriptorSetPool)) setPools;
} SeVkRenderPipelinePools;

/*
    Vulkan backend can render a few frames in advance, so cpu doesn't
    have to wait for gpu to finish rendering every frame.
    SeVkInFlightData contains all the data required on a per image-in-flight
    basis.
*/
typedef struct SeVkInFlightData
{
    se_sbuffer(struct SeRenderObject*) submittedCommandBuffers;
    se_sbuffer(SeVkRenderPipelinePools) renderPipelinePools;
    VkSemaphore imageAvailableSemaphore;
} SeVkInFlightData;

/*
    SeVkInFlightManager contains all data required for correct "images in flight"
    logic handling.
    inFlightDatas contanis all data for each in flight image.
        se_sbuffer_size(inFlightDatas) == number of images in flight
    registeredPipelines is an array of all registered pipelines.
        This array is needed for a corrent resource set (descriptor set) allocation.
        Allocation logic is as follows:
            - user requests resource set of some render pipeline and set
            - system retrieves pipeline's index from registeredPipelines array
            - system gets current SeVkInFlightData object from inFlightDatas
            - using index from registeredPipelines array, system gets corrent 
              SeVkRenderPipelinePools object from renderPipelinePools array
            - system gets correct pool and allocates new descriptor set
    swapChainImageToInFlightFrameMap maps swap chain image index to the index
    of last image in flight, that used this swap chain image.
*/
typedef struct SeVkInFlightManager
{
    struct SeRenderObject* device;
    se_sbuffer(SeVkInFlightData) inFlightDatas;
    se_sbuffer(struct SeRenderObject*) registeredPipelines;
    se_sbuffer(uint32_t) swapChainImageToInFlightFrameMap;
    size_t currentImageInFlight;
    uint32_t currentSwapChainImageIndex;
} SeVkInFlightManager;

typedef struct SeVkInFlightManagerCreateInfo
{
    struct SeRenderObject* device;
    size_t numSwapChainImages;
    size_t numImagesInFlight;
} SeVkInFlightManagerCreateInfo;

void                    se_vk_in_flight_manager_construct(SeVkInFlightManager* manager, SeVkInFlightManagerCreateInfo* createInfo);
void                    se_vk_in_flight_manager_destroy(SeVkInFlightManager* manager);
void                    se_vk_in_flight_manager_advance_frame(SeVkInFlightManager* manager);
void                    se_vk_in_flight_manager_register_pipeline(SeVkInFlightManager* manager, struct SeRenderObject* pipeline);
void                    se_vk_in_flight_manager_unregister_pipeline(SeVkInFlightManager* manager, struct SeRenderObject* pipeline);

void                    se_vk_in_flight_manager_submit_command_buffer(SeVkInFlightManager* manager, struct SeRenderObject* commandBuffer);
struct SeRenderObject*  se_vk_in_flight_manager_get_last_submitted_command_buffer(SeVkInFlightManager* manager);
VkSemaphore             se_vk_in_flight_manager_get_image_available_semaphore(SeVkInFlightManager* manager);
uint32_t                se_vk_in_flight_manager_get_current_swap_chain_image_index(SeVkInFlightManager* manager);
VkDescriptorSet         se_vk_in_flight_manager_create_descriptor_set(SeVkInFlightManager* manager, struct SeRenderObject* pipeline, size_t set);

#endif
