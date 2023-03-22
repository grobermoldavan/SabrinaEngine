#ifndef _SE_VULKAN_FRAME_MANAGER_H_
#define _SE_VULKAN_FRAME_MANAGER_H_

#include "se_vulkan_base.hpp"
#include "se_vulkan_memory_buffer.hpp"

#define SE_VK_FRAME_MANAGER_INVALID_FRAME 0xDEAD

#define se_vk_frame_manager_get_active_frame(manager) (&(manager)->frames[(manager)->frameNumber % SeVkConfig::NUM_FRAMES_IN_FLIGHT])
#define se_vk_frame_manager_get_active_frame_index(manager) ((manager)->frameNumber % SeVkConfig::NUM_FRAMES_IN_FLIGHT)
#define se_vk_frame_manager_get_frame(manager, index) (&(manager)->frames[(index) % SeVkConfig::NUM_FRAMES_IN_FLIGHT])

struct SeVkFrame
{
    struct ScratchBufferView
    {
        size_t offset;
        size_t size;
    };

    VkSemaphore                         imageAvailableSemaphore;
    SeDynamicArray<SeVkCommandBuffer*>    commandBuffers;
    SeVkMemoryBuffer*                   scratchBuffer;
    SeDynamicArray<ScratchBufferView>     scratchBufferViews;
    size_t                              scratchBufferTop;
};

struct SeVkFrameManager
{
    SeVkDevice* device;
    SeVkFrame   frames[SeVkConfig::NUM_FRAMES_IN_FLIGHT];
    size_t      imageToFrame[SeVkConfig::MAX_SWAP_CHAIN_IMAGES];
    size_t      frameNumber;
    size_t      scratchBufferAlignment;
};

struct SeVkFrameManagerCreateInfo
{
    SeVkDevice* device;
};

struct SeVkCommandBufferInfo;

void se_vk_frame_manager_construct(SeVkFrameManager* manager, const SeVkFrameManagerCreateInfo* createInfo);
void se_vk_frame_manager_destroy(SeVkFrameManager* manager);
void se_vk_frame_manager_advance(SeVkFrameManager* manager);

SeVkCommandBuffer* se_vk_frame_manager_get_cmd(SeVkFrameManager* manager, SeVkCommandBufferInfo* info);
uint32_t se_vk_frame_manager_alloc_scratch_buffer(SeVkFrameManager* manager, SeDataProvider data);

#endif
