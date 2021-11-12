#ifndef _SE_VULKAN_RENDER_SUBSYSTEM_MEMORY_H_
#define _SE_VULKAN_RENDER_SUBSYSTEM_MEMORY_H_

#include <inttypes.h>

#include "se_vulkan_render_subsystem_base.h"

#define SE_VK_MAX_CPU_ALLOCATIONS 8192

#define SE_VK_GPU_MEMORY_BLOCK_SIZE_BYTES   64ull
#define SE_VK_GPU_CHUNK_SIZE_BYTES          (16ull * 1024ull * 1024ull)
#define SE_VK_GPU_CHUNK_LEDGER_SIZE_BYTES   (SE_VK_GPU_CHUNK_SIZE_BYTES / SE_VK_GPU_MEMORY_BLOCK_SIZE_BYTES / 8ull)
#define SE_VK_GPU_MAX_CHUNKS                64ull

typedef struct SeVkMemoryManagerCreateInfo
{
    struct SeAllocatorBindings* allocator;
} SeVkMemoryManagerCreateInfo;

typedef struct SeVkGpuAllocationRequest
{
    size_t sizeBytes;
    size_t alignment;
    uint32_t memoryTypeIndex;
} SeVkGpuAllocationRequest;

typedef struct SeVkMemory
{
    VkDeviceMemory memory;
    VkDeviceSize offsetBytes;
    VkDeviceSize sizeBytes;
} SeVkMemory;

// typedef struct SeVkCpuAllocation
// {
//     void* ptr;
//     size_t size;
// } SeVkCpuAllocation;

typedef struct SeVkGpuMemoryChunk
{
    VkDeviceMemory memory;
    uint32_t memoryTypeIndex;
    uint32_t usedMemoryBytes;
    uint8_t* ledger;
} SeVkGpuMemoryChunk;

typedef struct SeVkMemoryManager
{
    //
    // CPU-side
    //
    struct SeAllocatorBindings* cpu_allocator;
    // VkAllocationCallbacks cpu_allocationCallbacks;
    // SeVkCpuAllocation* cpu_allocations;
    // size_t cpu_numAllocations;
    //
    // GPU-side
    //
    SeVkGpuMemoryChunk* gpu_chunks;
    uint8_t* gpu_ledgers;
    VkDevice device;
} SeVkMemoryManager;

void        se_vk_memory_manager_construct(SeVkMemoryManager* memoryManager, SeVkMemoryManagerCreateInfo* createInfo);
void        se_vk_memory_manager_destroy(SeVkMemoryManager* memoryManager);
void        se_vk_memory_manager_set_device(SeVkMemoryManager* memoryManager, VkDevice device);
bool        se_vk_gpu_is_valid_memory(SeVkMemory memory);
SeVkMemory  se_vk_gpu_allocate(SeVkMemoryManager* memoryManager, SeVkGpuAllocationRequest request);
void        se_vk_gpu_deallocate(SeVkMemoryManager* memoryManager, SeVkMemory allocation);

VkAllocationCallbacks* se_vk_memory_manager_get_callbacks(SeVkMemoryManager* memoryManager);

#endif
