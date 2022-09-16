#ifndef _SE_VULKAN_MEMORY_H_
#define _SE_VULKAN_MEMORY_H_

#include "se_vulkan_base.hpp"

struct SeVkGpuAllocationRequest
{
    size_t                  size;
    size_t                  alignment;
    uint32_t                memoryTypeBits;
    VkMemoryPropertyFlags   properties;
};

struct SeVkMemory
{
    VkDeviceMemory  memory;
    VkDeviceSize    offset;
    VkDeviceSize    size;
    void*           mappedMemory;
};

struct SeVkGpuMemoryChunk
{
    size_t          memorySize;
    VkDeviceMemory  memory;
    void*           mappedMemory;
    size_t          ledgerSize;
    uint8_t*        ledger;
    uint32_t        memoryTypeIndex;
    size_t          used;
};

struct SeVkCpuAllocation
{
    void*   ptr;
    size_t  size;
};

struct SeVkMemoryObjectPools;

struct SeVkMemoryManager
{
    VkAllocationCallbacks               cpu_allocationCallbacks;
    DynamicArray<SeVkCpuAllocation>     cpu_allocations;
    SeVkMemoryObjectPools*              cpu_objectPools;
    
    SeVkDevice*                         device;
    DynamicArray<SeVkGpuMemoryChunk>    gpu_chunks;
    VkPhysicalDeviceMemoryProperties*   memoryProperties;

    SeVkMemoryBuffer*                   stagingBuffer;
};

void            se_vk_memory_manager_construct(SeVkMemoryManager* manager);
void            se_vk_memory_manager_free_gpu_memory(SeVkMemoryManager* manager);
void            se_vk_memory_manager_free_cpu_memory(SeVkMemoryManager* manager);

void            se_vk_memory_manager_set_device(SeVkMemoryManager* manager, SeVkDevice* device);
bool            se_vk_memory_manager_is_valid_memory(SeVkMemory memory);
SeVkMemory      se_vk_memory_manager_allocate(SeVkMemoryManager* manager, SeVkGpuAllocationRequest request);
void            se_vk_memory_manager_deallocate(SeVkMemoryManager* manager, SeVkMemory allocation);

template<typename T> ObjectPool<T>& se_vk_memory_manager_get_pool(SeVkMemoryManager* manager);
const VkAllocationCallbacks*        se_vk_memory_manager_get_callbacks(const SeVkMemoryManager* manager);
SeVkMemoryBuffer*                   se_vk_memory_manager_get_staging_buffer(SeVkMemoryManager* manager);

#endif
