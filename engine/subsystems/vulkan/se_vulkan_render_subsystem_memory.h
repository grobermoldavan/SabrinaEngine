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
    size_t numImagesInFlight;
} SeVkMemoryManagerCreateInfo;

typedef struct SeVkGpuAllocationRequest
{
    size_t sizeBytes;
    size_t alignment;
    uint32_t memoryTypeBits;
    VkMemoryPropertyFlags properties;
} SeVkGpuAllocationRequest;

typedef struct SeVkMemory
{
    VkDeviceMemory memory;
    VkDeviceSize offsetBytes;
    VkDeviceSize sizeBytes;
    void* mappedMemory;
} SeVkMemory;

typedef struct SeVkGpuMemoryChunk
{
    size_t memorySize;
    VkDeviceMemory memory;
    void* mappedMemory;
    size_t ledgerSize;
    uint8_t* ledger;
    uint32_t memoryTypeIndex;
    uint32_t usedMemoryBytes;
} SeVkGpuMemoryChunk;

typedef struct SeVkCpuAllocation
{
    void* ptr;
    size_t size;
} SeVkCpuAllocation;

typedef struct SeVkMemoryManager
{
    VkAllocationCallbacks           cpu_allocationCallbacks;
    se_sbuffer(SeVkCpuAllocation)   cpu_allocations;
    struct SeAllocatorBindings*     cpu_persistentAllocator;
    struct SeAllocatorBindings*     cpu_frameAllocator;
    SeObjectPool                    cpu_texturePool;
    SeObjectPool                    cpu_samplerPool;
    SeObjectPool                    cpu_renderPassPool;
    SeObjectPool                    cpu_renderPipelinePool;
    SeObjectPool                    cpu_framebufferPool;
    SeObjectPool                    cpu_renderProgramPool;
    SeObjectPool                    cpu_memoryBufferPool;
    se_sbuffer(SeObjectPool)        cpu_commandBufferPools;
    se_sbuffer(SeObjectPool)        cpu_resourceSetPools;

    SeRenderObject*                     device;
    se_sbuffer(SeVkGpuMemoryChunk)      gpu_chunks;
    VkPhysicalDeviceMemoryProperties*   memoryProperties;
} SeVkMemoryManager;

void        se_vk_memory_manager_construct(SeVkMemoryManager* memoryManager, SeVkMemoryManagerCreateInfo* createInfo);
void        se_vk_memory_manager_free_gpu_memory(SeVkMemoryManager* memoryManager);
void        se_vk_memory_manager_free_cpu_memory(SeVkMemoryManager* memoryManager);

void            se_vk_memory_manager_set_device(SeVkMemoryManager* memoryManager, SeRenderObject* device);
SeObjectPool*   se_vk_memory_manager_get_pool(SeVkMemoryManager* memoryManager, SeRenderHandleType type);
SeObjectPool*   se_vk_memory_manager_get_pool_manually(SeVkMemoryManager* memoryManager, SeRenderHandleType type, size_t poolIndex);
bool            se_vk_gpu_is_valid_memory(SeVkMemory memory);
SeVkMemory      se_vk_gpu_allocate(SeVkMemoryManager* memoryManager, SeVkGpuAllocationRequest request);
void            se_vk_gpu_deallocate(SeVkMemoryManager* memoryManager, SeVkMemory allocation);

VkAllocationCallbacks* se_vk_memory_manager_get_callbacks(SeVkMemoryManager* memoryManager);

#endif
