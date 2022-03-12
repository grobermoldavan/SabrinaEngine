#ifndef _SE_VULKAN_RENDER_SUBSYSTEM_MEMORY_H_
#define _SE_VULKAN_RENDER_SUBSYSTEM_MEMORY_H_

#include <inttypes.h>
#include "se_vulkan_render_subsystem_base.h"
#include "engine/containers.h"

typedef struct SeVkMemoryManagerCreateInfo
{
    struct SeAllocatorBindings* persistentAllocator;
    struct SeAllocatorBindings* frameAllocator;
    struct SePlatformInterface* platform;
    size_t                      numFrames;
} SeVkMemoryManagerCreateInfo;

typedef struct SeVkGpuAllocationRequest
{
    size_t                  size;
    size_t                  alignment;
    uint32_t                memoryTypeBits;
    VkMemoryPropertyFlags   properties;
} SeVkGpuAllocationRequest;

typedef struct SeVkMemory
{
    VkDeviceMemory  memory;
    VkDeviceSize    offset;
    VkDeviceSize    size;
    void*           mappedMemory;
} SeVkMemory;

typedef struct SeVkGpuMemoryChunk
{
    size_t          memorySize;
    VkDeviceMemory  memory;
    void*           mappedMemory;
    size_t          ledgerSize;
    uint8_t*        ledger;
    uint32_t        memoryTypeIndex;
    uint32_t        used;
} SeVkGpuMemoryChunk;

typedef struct SeVkCpuAllocation
{
    void*   ptr;
    size_t  size;
} SeVkCpuAllocation;

typedef struct SeVkMemoryManager
{
    VkAllocationCallbacks               cpu_allocationCallbacks;
    se_sbuffer(SeVkCpuAllocation)       cpu_allocations;
    struct SeAllocatorBindings*         cpu_persistentAllocator;
    struct SeAllocatorBindings*         cpu_frameAllocator;

    SeObjectPool                        programPool;
    SeObjectPool                        texturePool;
    SeObjectPool                        renderPassPool;
    SeObjectPool                        framebufferPool;
    SeObjectPool                        pipelinePool;
    SeObjectPool                        memoryBufferPool;
    SeObjectPool                        samplerPool;
    se_sbuffer(SeObjectPool)            commandBufferPools;

    struct SeVkDevice*                  device;
    se_sbuffer(SeVkGpuMemoryChunk)      gpu_chunks;
    VkPhysicalDeviceMemoryProperties*   memoryProperties;
} SeVkMemoryManager;

void            se_vk_memory_manager_construct(SeVkMemoryManager* manager, SeVkMemoryManagerCreateInfo* createInfo);
void            se_vk_memory_manager_free_gpu_memory(SeVkMemoryManager* manager);
void            se_vk_memory_manager_free_cpu_memory(SeVkMemoryManager* manager);

void            se_vk_memory_manager_set_device(SeVkMemoryManager* manager, struct SeVkDevice* device);
bool            se_vk_memory_manager_is_valid_memory(const SeVkMemory memory);
SeVkMemory      se_vk_memory_manager_allocate(SeVkMemoryManager* manager, const SeVkGpuAllocationRequest request);
void            se_vk_memory_manager_deallocate(SeVkMemoryManager* manager, const SeVkMemory allocation);

VkAllocationCallbacks* se_vk_memory_manager_get_callbacks(SeVkMemoryManager* manager);

#endif
