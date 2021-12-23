
#include <string.h>
#include <stdbool.h>
#include "se_vulkan_render_subsystem_memory.h"
#include "se_vulkan_render_subsystem_device.h"
#include "engine/render_abstraction_interface.h"
#include "engine/allocator_bindings.h"
#include "engine/common_includes.h"

#define SE_VK_GPU_MEMORY_BLOCK_SIZE_BYTES   64ull
#define SE_VK_GPU_DEFAULT_CHUNK_SIZE_BYTES  (64ull * 1024ull * 1024ull)

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
    se_assert(chunk->usedMemoryBytes <= chunk->memorySize);
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
    se_assert(numBlocks * SE_VK_GPU_MEMORY_BLOCK_SIZE_BYTES <= chunk->usedMemoryBytes);
    chunk->usedMemoryBytes -= numBlocks * SE_VK_GPU_MEMORY_BLOCK_SIZE_BYTES;
    se_assert(chunk->usedMemoryBytes <= chunk->memorySize);
}

static size_t se_vk_memory_chunk_find_aligned_free_space(SeVkGpuMemoryChunk* chunk, size_t requiredNumberOfBlocks, size_t alignment)
{
    size_t freeCount = 0;
    size_t startBlock = 0;
    size_t currentBlock = 0;
    for (size_t ledgerIt = 0; ledgerIt < chunk->ledgerSize; ledgerIt++)
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
    startBlock = chunk->ledgerSize;
    block_found:
    return startBlock;
}

void se_vk_memory_manager_construct(SeVkMemoryManager* memoryManager, SeVkMemoryManagerCreateInfo* createInfo)
{
    memoryManager->cpu_persistentAllocator = createInfo->persistentAllocator;
    memoryManager->cpu_frameAllocator = createInfo->frameAllocator;
    SeAllocatorBindings* allocator = memoryManager->cpu_persistentAllocator;
    se_sbuffer_construct(memoryManager->gpu_chunks, 64, memoryManager->cpu_persistentAllocator);
}

void se_vk_memory_manager_destroy(SeVkMemoryManager* memoryManager)
{
    VkDevice logicalHandle = se_vk_device_get_logical_handle(memoryManager->device);
    const size_t numChunks = se_sbuffer_size(memoryManager->gpu_chunks);
    for (size_t it = 0; it < numChunks; it++)
    {
        SeVkGpuMemoryChunk* chunk = &memoryManager->gpu_chunks[it];
        vkFreeMemory(logicalHandle, memoryManager->gpu_chunks[it].memory, se_vk_memory_manager_get_callbacks(memoryManager));
        memoryManager->cpu_persistentAllocator->dealloc(memoryManager->cpu_persistentAllocator->allocator, chunk->ledger, chunk->ledgerSize);
    }
    se_sbuffer_destroy(memoryManager->gpu_chunks);
}

void se_vk_memory_manager_set_device(SeVkMemoryManager* memoryManager, SeRenderObject* device)
{
    se_vk_expect_handle(device, SE_RENDER_HANDLE_TYPE_DEVICE, "Can't set device to memory manager");
    memoryManager->device = device;
    memoryManager->memoryProperties = se_vk_device_get_memory_properties(device);
}

bool se_vk_gpu_is_valid_memory(SeVkMemory memory)
{
    return memory.memory != VK_NULL_HANDLE;
}

SeVkMemory se_vk_gpu_allocate(SeVkMemoryManager* memoryManager, SeVkGpuAllocationRequest request)
{
    const size_t requiredNumberOfBlocks = 1 + ((request.sizeBytes - 1) / SE_VK_GPU_MEMORY_BLOCK_SIZE_BYTES);
    uint32_t memoryTypeIndex = 0;
    if (!se_vk_utils_get_memory_type_index(memoryManager->memoryProperties, request.memoryTypeBits, request.properties, &memoryTypeIndex))
        se_assert_msg(false, "Unable to find memory type index");
    //
    // 2. Try to find memory in available chunks
    //
    const size_t numChunks = se_sbuffer_size(memoryManager->gpu_chunks);
    for (size_t it = 0; it < numChunks; it++)
    {
        SeVkGpuMemoryChunk* chunk = &memoryManager->gpu_chunks[it];
        se_assert(chunk->memory != VK_NULL_HANDLE);
        if (chunk->memoryTypeIndex != memoryTypeIndex)                          continue;
        if ((chunk->memorySize - chunk->usedMemoryBytes) < request.sizeBytes)   continue;
        const size_t inChunkOffset = se_vk_memory_chunk_find_aligned_free_space(chunk, requiredNumberOfBlocks, request.alignment);
        if (inChunkOffset == chunk->ledgerSize)                                 continue;
        se_vk_memory_manager_set_in_use(chunk, requiredNumberOfBlocks, inChunkOffset);
        return (SeVkMemory)
        {
            .memory         = chunk->memory,
            .offsetBytes    = inChunkOffset * SE_VK_GPU_MEMORY_BLOCK_SIZE_BYTES,
            .sizeBytes      = requiredNumberOfBlocks * SE_VK_GPU_MEMORY_BLOCK_SIZE_BYTES,
            .mappedMemory   = chunk->mappedMemory ? ((char*)chunk->mappedMemory) + inChunkOffset * SE_VK_GPU_MEMORY_BLOCK_SIZE_BYTES : NULL,
        };
    }
    //
    // 3. Try to allocate new chunk
    //
    {
        SeVkGpuMemoryChunk newChunk = {0};
        VkDevice logicalHandle = se_vk_device_get_logical_handle(memoryManager->device);
        const size_t allocationSize = request.sizeBytes > SE_VK_GPU_DEFAULT_CHUNK_SIZE_BYTES ? request.sizeBytes : SE_VK_GPU_DEFAULT_CHUNK_SIZE_BYTES;
        VkMemoryAllocateInfo memoryAllocateInfo = (VkMemoryAllocateInfo)
        {
            .sType              = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext              = NULL,
            .allocationSize     = allocationSize,
            .memoryTypeIndex    = memoryTypeIndex,
        };
        se_vk_check(vkAllocateMemory(logicalHandle, &memoryAllocateInfo, se_vk_memory_manager_get_callbacks(memoryManager), &newChunk.memory));
        newChunk.memorySize = allocationSize;
        if (request.properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
            vkMapMemory(logicalHandle, newChunk.memory, 0, newChunk.memorySize, 0, &newChunk.mappedMemory);
        newChunk.ledgerSize = allocationSize / SE_VK_GPU_MEMORY_BLOCK_SIZE_BYTES / 8;
        newChunk.ledger = memoryManager->cpu_persistentAllocator->alloc(memoryManager->cpu_persistentAllocator->allocator, newChunk.ledgerSize, se_default_alignment, se_alloc_tag);
        memset(newChunk.ledger, 0, newChunk.ledgerSize);
        newChunk.memoryTypeIndex = memoryTypeIndex;
        se_sbuffer_push(memoryManager->gpu_chunks, newChunk);
    }
    //
    // 4. Allocate memory in new chunk
    //
    {
        SeVkGpuMemoryChunk* chunk = &memoryManager->gpu_chunks[se_sbuffer_size(memoryManager->gpu_chunks) - 1];
        size_t inChunkOffset = se_vk_memory_chunk_find_aligned_free_space(chunk, requiredNumberOfBlocks, request.alignment);
        se_assert(inChunkOffset != chunk->memorySize);
        se_vk_memory_manager_set_in_use(chunk, requiredNumberOfBlocks, inChunkOffset);
        return (SeVkMemory)
        {
            .memory         = chunk->memory,
            .offsetBytes    = inChunkOffset * SE_VK_GPU_MEMORY_BLOCK_SIZE_BYTES,
            .sizeBytes      = requiredNumberOfBlocks * SE_VK_GPU_MEMORY_BLOCK_SIZE_BYTES,
            .mappedMemory   = chunk->mappedMemory ? ((char*)chunk->mappedMemory) + inChunkOffset * SE_VK_GPU_MEMORY_BLOCK_SIZE_BYTES : NULL,
        };
    }
}

void se_vk_gpu_deallocate(SeVkMemoryManager* memoryManager, SeVkMemory allocation)
{
    const size_t numChunks = se_sbuffer_size(memoryManager->gpu_chunks);
    for (size_t it = 0; it < numChunks; it++)
    {
        SeVkGpuMemoryChunk* chunk = &memoryManager->gpu_chunks[it];
        if (chunk->memory != allocation.memory) continue;
        const size_t startBlock = allocation.offsetBytes / SE_VK_GPU_MEMORY_BLOCK_SIZE_BYTES;
        const size_t numBlocks = allocation.sizeBytes / SE_VK_GPU_MEMORY_BLOCK_SIZE_BYTES;
        se_vk_memory_manager_set_free(chunk, numBlocks, startBlock);
        if (chunk->usedMemoryBytes == 0)
        {
            // Chunk is empty, so we free it
            VkDevice logicalHandle = se_vk_device_get_logical_handle(memoryManager->device);
            vkFreeMemory(logicalHandle, chunk->memory, se_vk_memory_manager_get_callbacks(memoryManager));
            memoryManager->cpu_persistentAllocator->dealloc(memoryManager->cpu_persistentAllocator->allocator, chunk->ledger, chunk->ledgerSize);
            se_sbuffer_remove_val(memoryManager->gpu_chunks, chunk);
        }
        break;
    }
}

VkAllocationCallbacks* se_vk_memory_manager_get_callbacks(SeVkMemoryManager* memoryManager)
{
    return NULL;
}
