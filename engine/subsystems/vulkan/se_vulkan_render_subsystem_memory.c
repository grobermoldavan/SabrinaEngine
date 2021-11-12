
#include <string.h>
#include <stdbool.h>

#include "se_vulkan_render_subsystem_memory.h"
#include "engine/allocator_bindings.h"
#include "engine/common_includes.h"

// static void* se_vk_memory_manager_alloc(void* pUserData, size_t size, size_t alignment, VkSystemAllocationScope allocationScope)
// {
//     SeVkMemoryManager* manager = (SeVkMemoryManager*)pUserData;
//     SeAllocatorBindings* allocator = manager->cpu_allocator;
//     void* result = allocator->alloc(allocator, size, alignment);
//     se_assert(manager->cpu_numAllocations < SE_VK_MAX_CPU_ALLOCATIONS);
//     manager->cpu_allocations[manager->cpu_numAllocations++] = (SeVkCpuAllocation)
//     {
//         .ptr = result,
//         .size = size,
//     };
//     return result;
// }

// static void* se_vk_memory_manager_realloc(void* pUserData, void* pOriginal, size_t size, size_t alignment, VkSystemAllocationScope allocationScope)
// {
//     SeVkMemoryManager* manager = (SeVkMemoryManager*)pUserData;
//     SeAllocatorBindings* allocator = manager->cpu_allocator;
//     void* result = NULL;
//     for (size_t it = 0; it < manager->cpu_numAllocations; it++)
//     {
//         if (manager->cpu_allocations[it].ptr == pOriginal)
//         {
//             allocator->dealloc(allocator, manager->cpu_allocations[it].ptr, manager->cpu_allocations[it].size);
//             result = allocator->alloc(allocator, size, alignment);
//             manager->cpu_allocations[it] = (SeVkCpuAllocation)
//             {
//                 .ptr = result,
//                 .size = size,
//             };
//             break;
//         }
//     }
//     return result;
// }

// static void se_vk_memory_manager_dealloc(void* pUserData, void* pMemory)
// {
//     SeVkMemoryManager* manager = (SeVkMemoryManager*)pUserData;
//     SeAllocatorBindings* allocator = manager->cpu_allocator;
//     for (size_t it = 0; it < manager->cpu_currentNumberOfAllocations; it++)
//     {
//         if (manager->cpu_allocations[it].ptr == pMemory)
//         {
//             allocator->dealloc(allocator, manager->cpu_allocations[it].ptr, manager->cpu_allocations[it].size);
//             manager->cpu_allocations[it] = manager->cpu_allocations[manager->cpu_currentNumberOfAllocations - 1];
//             manager->cpu_currentNumberOfAllocations -= 1;
//             break;
//         }
//     }
// },

static void se_vk_memory_manager_set_in_use(SeVkGpuMemoryChunk* chunk, size_t numBlocks, size_t startBlock)
{
    for (size_t it = 0; it < numBlocks; it++)
    {
        size_t currentByte = (startBlock + it) / 8;
        size_t currentBit = (startBlock + it) % 8;
        uint8_t* byte = &chunk->ledger[currentByte];
        *byte |= (1 << currentBit);
    }
    chunk->usedMemoryBytes += numBlocks * SE_VK_GPU_MEMORY_BLOCK_SIZE_BYTES;
    se_assert(chunk->usedMemoryBytes <= SE_VK_GPU_CHUNK_SIZE_BYTES);
}

static void se_vk_memory_manager_set_free(SeVkGpuMemoryChunk* chunk, size_t numBlocks, size_t startBlock)
{
    for (size_t it = 0; it < numBlocks; it++)
    {
        size_t currentByte = (startBlock + it) / 8;
        size_t currentBit = (startBlock + it) % 8;
        uint8_t* byte = &chunk->ledger[currentByte];
        *byte &= ~(1 << currentBit);
    }
    chunk->usedMemoryBytes -= numBlocks * SE_VK_GPU_MEMORY_BLOCK_SIZE_BYTES;
    se_assert(chunk->usedMemoryBytes <= SE_VK_GPU_CHUNK_SIZE_BYTES);
}

static size_t se_vk_memory_chunk_find_aligned_free_space(SeVkGpuMemoryChunk* chunk, size_t requiredNumberOfBlocks, size_t alignment)
{
    size_t freeCount = 0;
    size_t startBlock = 0;
    size_t currentBlock = 0;
    for (size_t ledgerIt = 0; ledgerIt < SE_VK_GPU_CHUNK_LEDGER_SIZE_BYTES; ledgerIt++)
    {
        uint8_t byte = chunk->ledger[ledgerIt];
        if (byte == 255)
        {
            freeCount = 0;
            currentBlock += 8;
            continue;
        }
        for (size_t byteIt = 0; byteIt < 8; byteIt++)
        {
            if (byte & (1 << byteIt))
            {
                freeCount = 0;
            }
            else
            {
                if (freeCount == 0)
                {
                    size_t currentBlockOffsetBytes = (ledgerIt * 8 + byteIt) * SE_VK_GPU_MEMORY_BLOCK_SIZE_BYTES;
                    bool isAlignedCorrectly = (currentBlockOffsetBytes % alignment) == 0;
                    if (isAlignedCorrectly)
                    {
                        startBlock = currentBlock;
                        freeCount += 1;
                    }
                }
                else
                {
                    freeCount += 1;
                }
            }
            currentBlock += 1;
            if (freeCount == requiredNumberOfBlocks)
            {
                goto block_found;
            }
        }
    }
    startBlock = SE_VK_GPU_CHUNK_LEDGER_SIZE_BYTES;
    block_found:
    return startBlock;
}

void se_vk_memory_manager_construct(SeVkMemoryManager* memoryManager, SeVkMemoryManagerCreateInfo* createInfo)
{
    SeAllocatorBindings* allocator = createInfo->allocator;
    memoryManager->cpu_allocator = createInfo->allocator;
    memoryManager->gpu_chunks = allocator->alloc(allocator->allocator, SE_VK_GPU_MAX_CHUNKS * sizeof(SeVkGpuMemoryChunk), se_default_alignment);
    memoryManager->gpu_ledgers = allocator->alloc(allocator->allocator, SE_VK_GPU_CHUNK_LEDGER_SIZE_BYTES * SE_VK_GPU_MAX_CHUNKS, se_default_alignment);
    for (size_t it = 0; it < SE_VK_GPU_MAX_CHUNKS; it++)
    {
        SeVkGpuMemoryChunk* chunk = &memoryManager->gpu_chunks[it];
        chunk->ledger = &memoryManager->gpu_ledgers[it * SE_VK_GPU_CHUNK_LEDGER_SIZE_BYTES];
    }
}

void se_vk_memory_manager_destroy(SeVkMemoryManager* memoryManager)
{
    for (size_t it = 0; it < SE_VK_GPU_MAX_CHUNKS; it++)
    {
        SeVkGpuMemoryChunk* chunk = &memoryManager->gpu_chunks[it];
        if (chunk->memory)
        {
            vkFreeMemory(memoryManager->device, chunk->memory, NULL /*&memoryManager->cpu_allocationCallbacks*/);
        }
    }
    SeAllocatorBindings* allocator = memoryManager->cpu_allocator;
    allocator->dealloc(allocator->allocator, memoryManager->gpu_chunks, SE_VK_GPU_MAX_CHUNKS * sizeof(SeVkGpuMemoryChunk));
    allocator->dealloc(allocator->allocator, memoryManager->gpu_ledgers, SE_VK_GPU_CHUNK_LEDGER_SIZE_BYTES * SE_VK_GPU_MAX_CHUNKS);
}

void se_vk_memory_manager_set_device(SeVkMemoryManager* memoryManager, VkDevice device)
{
    memoryManager->device = device;
}

bool se_vk_gpu_is_valid_memory(SeVkMemory memory)
{
    return memory.memory != VK_NULL_HANDLE;
}

SeVkMemory se_vk_gpu_allocate(SeVkMemoryManager* memoryManager, SeVkGpuAllocationRequest request)
{
    se_assert(request.sizeBytes <= SE_VK_GPU_CHUNK_SIZE_BYTES);
    size_t requiredNumberOfBlocks = 1 + ((request.sizeBytes - 1) / SE_VK_GPU_MEMORY_BLOCK_SIZE_BYTES);
    // 1. Try to find memory in available chunks
    for (size_t it = 0; it < SE_VK_GPU_MAX_CHUNKS; it++)
    {
        SeVkGpuMemoryChunk* chunk = &memoryManager->gpu_chunks[it];
        if (!chunk->memory)
        {
            continue;
        }
        if (chunk->memoryTypeIndex != request.memoryTypeIndex)
        {
            continue;
        }
        if ((SE_VK_GPU_CHUNK_SIZE_BYTES - chunk->usedMemoryBytes) < request.sizeBytes)
        {
            continue;
        }
        size_t inChunkOffset = se_vk_memory_chunk_find_aligned_free_space(chunk, requiredNumberOfBlocks, request.alignment);
        if (inChunkOffset == SE_VK_GPU_CHUNK_LEDGER_SIZE_BYTES)
        {
            continue;
        }
        se_vk_memory_manager_set_in_use(chunk, requiredNumberOfBlocks, inChunkOffset);
        return (SeVkMemory)
        {
            .memory = chunk->memory,
            .offsetBytes = inChunkOffset * SE_VK_GPU_MEMORY_BLOCK_SIZE_BYTES,
            .sizeBytes = requiredNumberOfBlocks * SE_VK_GPU_MEMORY_BLOCK_SIZE_BYTES,
        };
    }
    // 2. Try to allocate new chunk
    for (size_t it = 0; it < SE_VK_GPU_MAX_CHUNKS; it++)
    {
        SeVkGpuMemoryChunk* chunk = &memoryManager->gpu_chunks[it];
        if (chunk->memory)
        {
            continue;
        }
        // Allocating new chunk
        {
            VkMemoryAllocateInfo memoryAllocateInfo = (VkMemoryAllocateInfo)
            {
                .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                .pNext = NULL,
                .allocationSize = SE_VK_GPU_CHUNK_SIZE_BYTES,
                .memoryTypeIndex = request.memoryTypeIndex,
            };
            se_vk_check(vkAllocateMemory(memoryManager->device, &memoryAllocateInfo, NULL /*&memoryManager->cpu_allocationCallbacks*/, &chunk->memory));
            chunk->memoryTypeIndex = request.memoryTypeIndex;
            memset(chunk->ledger, 0, SE_VK_GPU_CHUNK_LEDGER_SIZE_BYTES);
        }
        // Allocating memory in new chunk
        size_t inChunkOffset = se_vk_memory_chunk_find_aligned_free_space(chunk, requiredNumberOfBlocks, request.alignment);
        se_assert(inChunkOffset != SE_VK_GPU_CHUNK_LEDGER_SIZE_BYTES);
        se_vk_memory_manager_set_in_use(chunk, requiredNumberOfBlocks, inChunkOffset);
        return (SeVkMemory)
        {
            .memory = chunk->memory,
            .offsetBytes = inChunkOffset * SE_VK_GPU_MEMORY_BLOCK_SIZE_BYTES,
            .sizeBytes = requiredNumberOfBlocks * SE_VK_GPU_MEMORY_BLOCK_SIZE_BYTES,
        };
    }
    se_assert(!"Out of memory");
    return (SeVkMemory){0};
}

void se_vk_gpu_deallocate(SeVkMemoryManager* memoryManager, SeVkMemory allocation)
{
    for (size_t it = 0; it < SE_VK_GPU_MAX_CHUNKS; it++)
    {
        SeVkGpuMemoryChunk* chunk = &memoryManager->gpu_chunks[it];
        if (chunk->memory != allocation.memory)
        {
            continue;
        }
        size_t startBlock = allocation.offsetBytes / SE_VK_GPU_MEMORY_BLOCK_SIZE_BYTES;
        size_t numBlocks = allocation.sizeBytes / SE_VK_GPU_MEMORY_BLOCK_SIZE_BYTES;
        se_vk_memory_manager_set_free(chunk, numBlocks, startBlock);
        if (chunk->usedMemoryBytes == 0)
        {
            // Chunk is empty, so we free it
            vkFreeMemory(memoryManager->device, chunk->memory, NULL /*&memoryManager->cpu_allocationCallbacks*/);
            chunk->memory = VK_NULL_HANDLE;
        }
        break;
    }
}

VkAllocationCallbacks* se_vk_memory_manager_get_callbacks(SeVkMemoryManager* memoryManager)
{
    return NULL;
}
