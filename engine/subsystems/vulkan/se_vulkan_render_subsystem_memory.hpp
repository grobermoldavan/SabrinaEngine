#ifndef _SE_VULKAN_RENDER_SUBSYSTEM_MEMORY_H_
#define _SE_VULKAN_RENDER_SUBSYSTEM_MEMORY_H_

#include <inttypes.h>
#include "se_vulkan_render_subsystem_base.hpp"
#include "engine/allocator_bindings.hpp"
#include "engine/containers.hpp"

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

struct SeVkMemoryManager
{
    VkAllocationCallbacks               cpu_allocationCallbacks;
    DynamicArray<SeVkCpuAllocation>     cpu_allocations;
    struct SeVkMemoryObjectPools*       cpu_objectPools;
    
    struct SeVkDevice*                  device;
    DynamicArray<SeVkGpuMemoryChunk>    gpu_chunks;
    VkPhysicalDeviceMemoryProperties*   memoryProperties;

    struct SeVkMemoryBuffer* stagingBuffer;
};

void            se_vk_memory_manager_construct(SeVkMemoryManager* manager);
void            se_vk_memory_manager_free_gpu_memory(SeVkMemoryManager* manager);
void            se_vk_memory_manager_free_cpu_memory(SeVkMemoryManager* manager);

void            se_vk_memory_manager_set_device(SeVkMemoryManager* manager, struct SeVkDevice* device);
bool            se_vk_memory_manager_is_valid_memory(SeVkMemory memory);
SeVkMemory      se_vk_memory_manager_allocate(SeVkMemoryManager* manager, SeVkGpuAllocationRequest request);
void            se_vk_memory_manager_deallocate(SeVkMemoryManager* manager, SeVkMemory allocation);

template<typename T> ObjectPool<T>& se_vk_memory_manager_get_pool(SeVkMemoryManager* manager);

VkAllocationCallbacks* se_vk_memory_manager_get_callbacks(SeVkMemoryManager* manager);

struct SeVkMemoryBuffer* se_vk_memory_manager_get_staging_buffer(SeVkMemoryManager* manager);

#endif
