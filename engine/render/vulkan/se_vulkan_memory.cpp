
#include "se_vulkan_memory.hpp"
#include "se_vulkan_device.hpp"
#include "se_vulkan_frame_manager.hpp"
#include "se_vulkan_utils.hpp"
#include "se_vulkan_program.hpp"
#include "se_vulkan_texture.hpp"
#include "se_vulkan_render_pass.hpp"
#include "se_vulkan_framebuffer.hpp"
#include "se_vulkan_pipeline.hpp"
#include "se_vulkan_memory_buffer.hpp"
#include "se_vulkan_sampler.hpp"
#include "se_vulkan_command_buffer.hpp"

struct SeVkMemoryObjectPools
{
    SeObjectPool<SeVkCommandBuffer>   commandBufferPool;
    SeObjectPool<SeVkFramebuffer>     framebufferPool;
    SeObjectPool<SeVkMemoryBuffer>    memoryBufferPool;
    SeObjectPool<SeVkPipeline>        pipelinePool;
    SeObjectPool<SeVkProgram>         propgramPool;
    SeObjectPool<SeVkRenderPass>      renderPassPool;
    SeObjectPool<SeVkSampler>         samplerPool;
    SeObjectPool<SeVkTexture>         texturePool;
};

constexpr size_t MEMORY_BLOCK_SIZE_BYTES     = 64ull;
constexpr size_t DEFAULT_CHUNK_SIZE_BYTES    = 32ull * 1024ull * 1024ull;
constexpr size_t STAGING_BUFFER_SIZE         = se_megabytes(16);

void* se_vk_memory_manager_alloc(void* pUserData, size_t size, size_t alignment, VkSystemAllocationScope allocationScope)
{
    SeAllocatorBindings allocator = se_allocator_persistent();
    SeVkMemoryManager* manager = (SeVkMemoryManager*)pUserData;
    void* result = allocator.alloc(allocator.allocator, size, alignment, se_alloc_tag);
    se_dynamic_array_push(manager->cpu_allocations, { result, size });
    return result;
}

void* se_vk_memory_manager_realloc(void* pUserData, void* pOriginal, size_t size, size_t alignment, VkSystemAllocationScope allocationScope)
{
    SeAllocatorBindings allocator = se_allocator_persistent();
    SeVkMemoryManager* manager = (SeVkMemoryManager*)pUserData;
    const size_t numAllocations = se_dynamic_array_size(manager->cpu_allocations);
    void* result = nullptr;
    for (size_t it = 0; it < numAllocations; it++)
    {
        if (manager->cpu_allocations[it].ptr == pOriginal)
        {
            result = allocator.alloc(allocator.allocator, size, alignment, se_alloc_tag);
            memcpy(result, pOriginal, manager->cpu_allocations[it].size);
            allocator.dealloc(allocator.allocator, manager->cpu_allocations[it].ptr, manager->cpu_allocations[it].size);
            manager->cpu_allocations[it] = { result, size, };
            break;
        }
    }
    se_assert(result);
    return result;
}

void se_vk_memory_manager_dealloc(void* pUserData, void* pMemory)
{
    SeAllocatorBindings allocator = se_allocator_persistent();
    SeVkMemoryManager* manager = (SeVkMemoryManager*)pUserData;
    const size_t numAllocations = se_dynamic_array_size(manager->cpu_allocations);
    for (size_t it = 0; it < numAllocations; it++)
    {
        if (manager->cpu_allocations[it].ptr == pMemory)
        {
            allocator.dealloc(allocator.allocator, manager->cpu_allocations[it].ptr, manager->cpu_allocations[it].size);
            se_dynamic_array_remove_idx(manager->cpu_allocations, it);
            break;
        }
    }
}

void se_vk_memory_manager_set_in_use(SeVkGpuMemoryChunk* chunk, size_t numBlocks, size_t startBlock)
{
    for (size_t it = 0; it < numBlocks; it++)
    {
        size_t currentByte = (startBlock + it) / 8;
        size_t currentBit = (startBlock + it) % 8;
        uint8_t* byte = &chunk->ledger[currentByte];
        *byte |= (1 << currentBit);
    }
    chunk->used += numBlocks * MEMORY_BLOCK_SIZE_BYTES;
    se_assert(chunk->used <= chunk->memorySize);
}

void se_vk_memory_manager_set_free(SeVkGpuMemoryChunk* chunk, size_t numBlocks, size_t startBlock)
{
    for (size_t it = 0; it < numBlocks; it++)
    {
        size_t currentByte = (startBlock + it) / 8;
        size_t currentBit = (startBlock + it) % 8;
        uint8_t* byte = &chunk->ledger[currentByte];
        *byte &= ~(1 << currentBit);
    }
    se_assert(numBlocks * MEMORY_BLOCK_SIZE_BYTES <= chunk->used);
    chunk->used -= numBlocks * MEMORY_BLOCK_SIZE_BYTES;
    se_assert(chunk->used <= chunk->memorySize);
}

size_t se_vk_memory_chunk_find_aligned_free_space(SeVkGpuMemoryChunk* chunk, size_t requiredNumberOfBlocks, size_t alignment)
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
                    size_t currentBlockOffsetBytes = (ledgerIt * 8 + byteIt) * MEMORY_BLOCK_SIZE_BYTES;
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

void se_vk_memory_manager_construct(SeVkMemoryManager* manager)
{
    SeAllocatorBindings allocator = se_allocator_persistent();
    *manager = SeVkMemoryManager
    {
        .cpu_allocationCallbacks    =
        {
            .pUserData              = manager,
            .pfnAllocation          = se_vk_memory_manager_alloc,
            .pfnReallocation        = se_vk_memory_manager_realloc,
            .pfnFree                = se_vk_memory_manager_dealloc,
            .pfnInternalAllocation  = nullptr,
            .pfnInternalFree        = nullptr,
        },
        .cpu_allocations            = se_dynamic_array_create<SeVkCpuAllocation>(allocator, 4096),
        .cpu_objectPools            = (SeVkMemoryObjectPools*)allocator.alloc(allocator.allocator, sizeof(SeVkMemoryObjectPools), se_default_alignment, se_alloc_tag),
        .device                     = nullptr,
        .gpu_chunks                 = se_dynamic_array_create<SeVkGpuMemoryChunk>(allocator, 64),
        .memoryProperties           = nullptr,
        .stagingBuffer              = nullptr,
    };
    se_object_pool_construct(manager->cpu_objectPools->commandBufferPool);
    se_object_pool_construct(manager->cpu_objectPools->framebufferPool);
    se_object_pool_construct(manager->cpu_objectPools->memoryBufferPool);
    se_object_pool_construct(manager->cpu_objectPools->pipelinePool);
    se_object_pool_construct(manager->cpu_objectPools->propgramPool);
    se_object_pool_construct(manager->cpu_objectPools->renderPassPool);
    se_object_pool_construct(manager->cpu_objectPools->samplerPool);
    se_object_pool_construct(manager->cpu_objectPools->texturePool);
}

void se_vk_memory_manager_free_gpu_memory(SeVkMemoryManager* manager)
{
    SeAllocatorBindings allocator = se_allocator_persistent();
    VkDevice logicalHandle = se_vk_device_get_logical_handle(manager->device);
    const size_t numChunks = se_dynamic_array_size(manager->gpu_chunks);
    for (size_t it = 0; it < numChunks; it++)
    {
        SeVkGpuMemoryChunk* chunk = &manager->gpu_chunks[it];
        vkFreeMemory(logicalHandle, manager->gpu_chunks[it].memory, se_vk_memory_manager_get_callbacks(manager));
        allocator.dealloc(allocator.allocator, chunk->ledger, chunk->ledgerSize);
    }
    se_dynamic_array_destroy(manager->gpu_chunks);
}

void se_vk_memory_manager_free_cpu_memory(SeVkMemoryManager* manager)
{
    SeAllocatorBindings allocator = se_allocator_persistent();

    const size_t numAllocations = se_dynamic_array_size(manager->cpu_allocations);
    for (size_t it = 0; it < numAllocations; it++)
        allocator.dealloc(allocator.allocator, manager->cpu_allocations[it].ptr, manager->cpu_allocations[it].size);
    se_dynamic_array_destroy(manager->cpu_allocations);

    se_object_pool_destroy(manager->cpu_objectPools->commandBufferPool);
    se_object_pool_destroy(manager->cpu_objectPools->framebufferPool);
    se_object_pool_destroy(manager->cpu_objectPools->memoryBufferPool);
    se_object_pool_destroy(manager->cpu_objectPools->pipelinePool);
    se_object_pool_destroy(manager->cpu_objectPools->propgramPool);
    se_object_pool_destroy(manager->cpu_objectPools->renderPassPool);
    se_object_pool_destroy(manager->cpu_objectPools->samplerPool);
    se_object_pool_destroy(manager->cpu_objectPools->texturePool);
}

void se_vk_memory_manager_set_device(SeVkMemoryManager* manager, SeVkDevice* device)
{
    manager->device = device;
    manager->memoryProperties = se_vk_device_get_memory_properties(device);

    manager->stagingBuffer = se_object_pool_take(manager->cpu_objectPools->memoryBufferPool);
    SeVkMemoryBufferInfo stagingBufferInfo
    {
        .device     = manager->device,
        .size       = STAGING_BUFFER_SIZE,
        .usage      = VK_BUFFER_USAGE_TRANSFER_DST_BIT   | 
                      VK_BUFFER_USAGE_TRANSFER_SRC_BIT   ,
        .visibility = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
    };
    se_vk_memory_buffer_construct(manager->stagingBuffer, &stagingBufferInfo);
}

bool se_vk_memory_manager_is_valid_memory(SeVkMemory memory)
{
    return memory.memory != VK_NULL_HANDLE;
}

SeVkMemory se_vk_memory_manager_allocate(SeVkMemoryManager* manager, SeVkGpuAllocationRequest request)
{
    SeAllocatorBindings allocator = se_allocator_persistent();

    const size_t requiredNumberOfBlocks = 1 + ((request.size - 1) / MEMORY_BLOCK_SIZE_BYTES);
    uint32_t memoryTypeIndex = 0;
    if (!se_vk_utils_get_memory_type_index(manager->memoryProperties, request.memoryTypeBits, request.properties, &memoryTypeIndex))
    {
        se_assert_msg(false, "Unable to find memory type index");
    }
    //
    // 1. Try to find memory in available chunks
    //
    const size_t numChunks = se_dynamic_array_size(manager->gpu_chunks);
    for (size_t it = 0; it < numChunks; it++)
    {
        SeVkGpuMemoryChunk* chunk = &manager->gpu_chunks[it];
        se_assert(chunk->memory != VK_NULL_HANDLE);
        if (chunk->memoryTypeIndex != memoryTypeIndex) continue;
        if ((chunk->memorySize - chunk->used) < request.size) continue;
        const size_t inChunkOffset = se_vk_memory_chunk_find_aligned_free_space(chunk, requiredNumberOfBlocks, request.alignment);
        if (inChunkOffset == chunk->ledgerSize) continue;
        se_vk_memory_manager_set_in_use(chunk, requiredNumberOfBlocks, inChunkOffset);
        return
        {
            .memory         = chunk->memory,
            .offset         = inChunkOffset * MEMORY_BLOCK_SIZE_BYTES,
            .size           = requiredNumberOfBlocks * MEMORY_BLOCK_SIZE_BYTES,
            .mappedMemory   = chunk->mappedMemory ? ((char*)chunk->mappedMemory) + inChunkOffset * MEMORY_BLOCK_SIZE_BYTES : nullptr,
        };
    }
    //
    // 2. Try to allocate new chunk
    //
    {
        SeVkGpuMemoryChunk newChunk = { };
        VkDevice logicalHandle = se_vk_device_get_logical_handle(manager->device);
        // chunkNumberOfBlocks is aligned to 8 because each ledger byte can store info about 8 blocks
        const size_t chunkNumberOfBlocks = request.size > DEFAULT_CHUNK_SIZE_BYTES
            ? requiredNumberOfBlocks + (8 - (requiredNumberOfBlocks % 8))
            : DEFAULT_CHUNK_SIZE_BYTES / MEMORY_BLOCK_SIZE_BYTES;
        const size_t chunkMemorySize = request.size > DEFAULT_CHUNK_SIZE_BYTES
            ? chunkNumberOfBlocks * MEMORY_BLOCK_SIZE_BYTES
            : DEFAULT_CHUNK_SIZE_BYTES;
        VkMemoryAllocateInfo memoryAllocateInfo
        {
            .sType              = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext              = nullptr,
            .allocationSize     = chunkMemorySize,
            .memoryTypeIndex    = memoryTypeIndex,
        };
        se_vk_check(vkAllocateMemory(logicalHandle, &memoryAllocateInfo, se_vk_memory_manager_get_callbacks(manager), &newChunk.memory));
        newChunk.memorySize = chunkMemorySize;
        if (request.properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
            vkMapMemory(logicalHandle, newChunk.memory, 0, newChunk.memorySize, 0, &newChunk.mappedMemory);
        newChunk.ledgerSize = chunkNumberOfBlocks / 8;
        newChunk.ledger = (uint8_t*)allocator.alloc(allocator.allocator, newChunk.ledgerSize, se_default_alignment, se_alloc_tag);
        memset(newChunk.ledger, 0, newChunk.ledgerSize);
        newChunk.memoryTypeIndex = memoryTypeIndex;
        se_dynamic_array_push(manager->gpu_chunks, newChunk);
    }
    //
    // 3. Allocate memory in new chunk
    //
    {
        SeVkGpuMemoryChunk* const chunk = &manager->gpu_chunks[se_dynamic_array_size(manager->gpu_chunks) - 1];
        const size_t inChunkOffset = se_vk_memory_chunk_find_aligned_free_space(chunk, requiredNumberOfBlocks, request.alignment);
        se_assert(inChunkOffset != chunk->memorySize);
        se_vk_memory_manager_set_in_use(chunk, requiredNumberOfBlocks, inChunkOffset);
        return
        {
            .memory         = chunk->memory,
            .offset         = inChunkOffset * MEMORY_BLOCK_SIZE_BYTES,
            .size           = requiredNumberOfBlocks * MEMORY_BLOCK_SIZE_BYTES,
            .mappedMemory   = chunk->mappedMemory ? ((char*)chunk->mappedMemory) + inChunkOffset * MEMORY_BLOCK_SIZE_BYTES : nullptr,
        };
    }
}

void se_vk_memory_manager_deallocate(SeVkMemoryManager* manager, SeVkMemory allocation)
{
    SeAllocatorBindings allocator = se_allocator_persistent();

    const size_t numChunks = se_dynamic_array_size(manager->gpu_chunks);
    for (size_t it = 0; it < numChunks; it++)
    {
        SeVkGpuMemoryChunk* chunk = &manager->gpu_chunks[it];
        if (chunk->memory != allocation.memory) continue;
        const size_t startBlock = allocation.offset / MEMORY_BLOCK_SIZE_BYTES;
        const size_t numBlocks = allocation.size / MEMORY_BLOCK_SIZE_BYTES;
        se_vk_memory_manager_set_free(chunk, numBlocks, startBlock);
        if (chunk->used == 0)
        {
            // Chunk is empty, so we free it
            VkDevice logicalHandle = se_vk_device_get_logical_handle(manager->device);
            vkFreeMemory(logicalHandle, chunk->memory, se_vk_memory_manager_get_callbacks(manager));
            allocator.dealloc(allocator.allocator, chunk->ledger, chunk->ledgerSize);
            se_dynamic_array_remove(manager->gpu_chunks, chunk);
        }
        break;
    }
}

template<typename T>
SeObjectPool<T>& se_vk_memory_manager_get_pool(SeVkMemoryManager* manager)
{
    if      constexpr (std::is_same<SeVkCommandBuffer, T>::value)   return manager->cpu_objectPools->commandBufferPool;
    else if constexpr (std::is_same<SeVkFramebuffer, T>::value)     return manager->cpu_objectPools->framebufferPool;
    else if constexpr (std::is_same<SeVkMemoryBuffer, T>::value)    return manager->cpu_objectPools->memoryBufferPool;
    else if constexpr (std::is_same<SeVkPipeline, T>::value)        return manager->cpu_objectPools->pipelinePool;
    else if constexpr (std::is_same<SeVkProgram, T>::value)         return manager->cpu_objectPools->propgramPool;
    else if constexpr (std::is_same<SeVkRenderPass, T>::value)      return manager->cpu_objectPools->renderPassPool;
    else if constexpr (std::is_same<SeVkSampler, T>::value)         return manager->cpu_objectPools->samplerPool;
    else if constexpr (std::is_same<SeVkTexture, T>::value)         return manager->cpu_objectPools->texturePool;
    else
    {
        static_assert(false, "Invalid code path");
        return nullptr;
    }
}

const VkAllocationCallbacks* se_vk_memory_manager_get_callbacks(const SeVkMemoryManager* manager)
{
    return &manager->cpu_allocationCallbacks;
}

SeVkMemoryBuffer* se_vk_memory_manager_get_staging_buffer(SeVkMemoryManager* manager)
{
    return manager->stagingBuffer;
}
