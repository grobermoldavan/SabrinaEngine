#ifndef _SE_VULKAN_RENDER_SUBSYSTEM_FRAME_MANAGER_H_
#define _SE_VULKAN_RENDER_SUBSYSTEM_FRAME_MANAGER_H_

#include "se_vulkan_render_subsystem_base.hpp"
#include "se_vulkan_render_subsystem_memory_buffer.hpp"

#define SE_VK_FRAME_MANAGER_MAX_NUM_FRAMES 6
#define SE_VK_FRAME_MANAGER_INVALID_FRAME 0xDEAD

#define se_vk_frame_manager_get_active_frame(manager) (&(manager)->frames[(manager)->frameNumber % (manager)->numFrames])
#define se_vk_frame_manager_get_active_frame_index(manager) ((manager)->frameNumber % (manager)->numFrames)

struct SeVkFrame
{
    SeVkMemoryBuffer*           scratchBuffer;
    size_t                      scratchBufferTop;
    VkSemaphore                 imageAvailableSemaphore;
    struct SeVkCommandBuffer*   lastBuffer;
};

struct SeVkFrameManager
{
    struct SeVkDevice*  device;
    SeVkFrame           frames[SE_VK_FRAME_MANAGER_MAX_NUM_FRAMES];
    size_t              imageToFrame[SE_VK_FRAME_MANAGER_MAX_NUM_FRAMES];
    size_t              numFrames;
    size_t              frameNumber;
    size_t              scratchBufferAlignment;
};

struct SeVkFrameManagerCreateInfo
{
    struct SeVkDevice* device;
    size_t numFrames;
};

void se_vk_frame_manager_construct(SeVkFrameManager* manager, SeVkFrameManagerCreateInfo* createInfo);
void se_vk_frame_manager_destroy(SeVkFrameManager* manager);
void se_vk_frame_manager_advance(SeVkFrameManager* manager);

SeVkMemoryBufferView se_vk_frame_manager_alloc_scratch_buffer(SeVkFrameManager* manager, size_t size);

#endif
