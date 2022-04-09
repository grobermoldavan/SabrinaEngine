#ifndef _SE_VULKAN_RENDER_SUBSYSTEM_MEMORY_H_
#define _SE_VULKAN_RENDER_SUBSYSTEM_MEMORY_H_

#include <inttypes.h>
#include "se_vulkan_render_subsystem_base.hpp"
#include "engine/containers.hpp"

struct SeVkMemoryManagerCreateInfo
{
    struct SeAllocatorBindings* persistentAllocator;
    struct SeAllocatorBindings* frameAllocator;
    struct SePlatformSubsystemInterface* platform;
    size_t                      numFrames;
};

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
    VkAllocationCallbacks                   cpu_allocationCallbacks;
    DynamicArray<SeVkCpuAllocation>         cpu_allocations;
    struct SeAllocatorBindings*             cpu_persistentAllocator;
    struct SeAllocatorBindings*             cpu_frameAllocator;
    struct SePlatformSubsystemInterface*    platform;
    DynamicArray<TypelessObjectPool>        cpu_pools;

    struct SeVkDevice*                      device;
    DynamicArray<SeVkGpuMemoryChunk>        gpu_chunks;
    VkPhysicalDeviceMemoryProperties*       memoryProperties;
};

void            se_vk_memory_manager_construct(SeVkMemoryManager* manager, SeVkMemoryManagerCreateInfo* createInfo);
void            se_vk_memory_manager_free_gpu_memory(SeVkMemoryManager* manager);
void            se_vk_memory_manager_free_cpu_memory(SeVkMemoryManager* manager);

void            se_vk_memory_manager_set_device(SeVkMemoryManager* manager, struct SeVkDevice* device);
bool            se_vk_memory_manager_is_valid_memory(const SeVkMemory memory);
SeVkMemory      se_vk_memory_manager_allocate(SeVkMemoryManager* manager, const SeVkGpuAllocationRequest request);
void            se_vk_memory_manager_deallocate(SeVkMemoryManager* manager, const SeVkMemory allocation);

template<typename T> ObjectPool<T>& se_vk_memory_manager_create_pool(SeVkMemoryManager* manager);
template<typename T> ObjectPool<T>& se_vk_memory_manager_get_pool(SeVkMemoryManager* manager);

VkAllocationCallbacks* se_vk_memory_manager_get_callbacks(SeVkMemoryManager* manager);

#endif
