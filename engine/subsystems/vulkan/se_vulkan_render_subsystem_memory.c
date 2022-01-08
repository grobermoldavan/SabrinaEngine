
#include <string.h>
#include "se_vulkan_render_subsystem_memory.h"
#include "se_vulkan_render_subsystem_device.h"
#include "se_vulkan_render_subsystem_texture.h"
#include "se_vulkan_render_subsystem_sampler.h"
#include "se_vulkan_render_subsystem_render_pass.h"
#include "se_vulkan_render_subsystem_render_pipeline.h"
#include "se_vulkan_render_subsystem_framebuffer.h"
#include "se_vulkan_render_subsystem_render_program.h"
#include "se_vulkan_render_subsystem_memory_buffer.h"
#include "se_vulkan_render_subsystem_command_buffer.h"
#include "se_vulkan_render_subsystem_resource_set.h"
#include "se_vulkan_render_subsystem_in_flight_manager.h"
#include "engine/allocator_bindings.h"
#include "engine/common_includes.h"

#define SE_VK_GPU_MEMORY_BLOCK_SIZE_BYTES   64ull
#define SE_VK_GPU_DEFAULT_CHUNK_SIZE_BYTES  (64ull * 1024ull * 1024ull)

static void* se_vk_memory_manager_alloc(void* pUserData, size_t size, size_t alignment, VkSystemAllocationScope allocationScope)
{
    SeVkMemoryManager* manager = (SeVkMemoryManager*)pUserData;
    SeAllocatorBindings* allocator = manager->cpu_persistentAllocator;
    void* result = allocator->alloc(allocator->allocator, size, alignment, se_alloc_tag);
    SeVkCpuAllocation allocation = (SeVkCpuAllocation) { result, size, };
    se_sbuffer_push(manager->cpu_allocations, allocation);
    return result;
}

static void* se_vk_memory_manager_realloc(void* pUserData, void* pOriginal, size_t size, size_t alignment, VkSystemAllocationScope allocationScope)
{
    SeVkMemoryManager* manager = (SeVkMemoryManager*)pUserData;
    SeAllocatorBindings* allocator = manager->cpu_persistentAllocator;
    const size_t numAllocations = se_sbuffer_size(manager->cpu_allocations);
    void* result = NULL;
    for (size_t it = 0; it < numAllocations; it++)
    {
        if (manager->cpu_allocations[it].ptr == pOriginal)
        {
            result = allocator->alloc(allocator->allocator, size, alignment, se_alloc_tag);
            memcpy(result, pOriginal, manager->cpu_allocations[it].size);
            allocator->dealloc(allocator->allocator, manager->cpu_allocations[it].ptr, manager->cpu_allocations[it].size);
            manager->cpu_allocations[it] = (SeVkCpuAllocation) { result, size, };
            break;
        }
    }
    se_assert(result);
    return result;
}

static void se_vk_memory_manager_dealloc(void* pUserData, void* pMemory)
{
    SeVkMemoryManager* manager = (SeVkMemoryManager*)pUserData;
    SeAllocatorBindings* allocator = manager->cpu_persistentAllocator;
    const size_t numAllocations = se_sbuffer_size(manager->cpu_allocations);
    for (size_t it = 0; it < numAllocations; it++)
    {
        if (manager->cpu_allocations[it].ptr == pMemory)
        {
            allocator->dealloc(allocator->allocator, manager->cpu_allocations[it].ptr, manager->cpu_allocations[it].size);
            se_sbuffer_remove_idx(manager->cpu_allocations, it);
            break;
        }
    }
}

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
    memoryManager->cpu_allocationCallbacks = (VkAllocationCallbacks)
    {
        .pUserData               = memoryManager,
        .pfnAllocation           = se_vk_memory_manager_alloc,
        .pfnReallocation         = se_vk_memory_manager_realloc,
        .pfnFree                 = se_vk_memory_manager_dealloc,
        .pfnInternalAllocation   = NULL,
        .pfnInternalFree         = NULL,
    };
    se_sbuffer_construct(memoryManager->cpu_allocations, 4096, memoryManager->cpu_persistentAllocator);
    se_object_pool_construct(&memoryManager->cpu_texturePool        , &((SeObjectPoolCreateInfo){ createInfo->platform, sizeof(SeVkTexture) }));
    se_object_pool_construct(&memoryManager->cpu_samplerPool        , &((SeObjectPoolCreateInfo){ createInfo->platform, sizeof(SeVkSampler) }));
    se_object_pool_construct(&memoryManager->cpu_renderPassPool     , &((SeObjectPoolCreateInfo){ createInfo->platform, sizeof(SeVkRenderPass) }));
    se_object_pool_construct(&memoryManager->cpu_renderPipelinePool , &((SeObjectPoolCreateInfo){ createInfo->platform, sizeof(SeVkRenderPipeline) }));
    se_object_pool_construct(&memoryManager->cpu_framebufferPool    , &((SeObjectPoolCreateInfo){ createInfo->platform, sizeof(SeVkFramebuffer) }));
    se_object_pool_construct(&memoryManager->cpu_renderProgramPool  , &((SeObjectPoolCreateInfo){ createInfo->platform, sizeof(SeVkRenderProgram) }));
    se_object_pool_construct(&memoryManager->cpu_memoryBufferPool   , &((SeObjectPoolCreateInfo){ createInfo->platform, sizeof(SeVkMemoryBuffer) }));
    se_sbuffer_construct(memoryManager->cpu_commandBufferPools, createInfo->numImagesInFlight, memoryManager->cpu_persistentAllocator);
    se_sbuffer_set_size(memoryManager->cpu_commandBufferPools, createInfo->numImagesInFlight);
    for (size_t it = 0; it < createInfo->numImagesInFlight; it++)
        se_object_pool_construct(&memoryManager->cpu_commandBufferPools[it], &((SeObjectPoolCreateInfo){ createInfo->platform, sizeof(SeVkCommandBuffer) }));
    se_sbuffer_construct(memoryManager->cpu_resourceSetPools, createInfo->numImagesInFlight, memoryManager->cpu_persistentAllocator);
    se_sbuffer_set_size(memoryManager->cpu_resourceSetPools, createInfo->numImagesInFlight);
    for (size_t it = 0; it < createInfo->numImagesInFlight; it++)
        se_object_pool_construct(&memoryManager->cpu_resourceSetPools[it], &((SeObjectPoolCreateInfo){ createInfo->platform, sizeof(SeVkResourceSet) }));
    se_sbuffer_construct(memoryManager->gpu_chunks, 64, memoryManager->cpu_persistentAllocator);
}

void se_vk_memory_manager_free_gpu_memory(SeVkMemoryManager* memoryManager)
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

void se_vk_memory_manager_free_cpu_memory(SeVkMemoryManager* memoryManager)
{
    SeAllocatorBindings* allocator = memoryManager->cpu_persistentAllocator;
    const size_t numAllocations = se_sbuffer_size(memoryManager->cpu_allocations);
    for (size_t it = 0; it < numAllocations; it++)
        allocator->dealloc(allocator->allocator, memoryManager->cpu_allocations[it].ptr, memoryManager->cpu_allocations[it].size);
    se_sbuffer_destroy(memoryManager->cpu_allocations);
    se_object_pool_destroy(&memoryManager->cpu_texturePool);
    se_object_pool_destroy(&memoryManager->cpu_samplerPool);
    se_object_pool_destroy(&memoryManager->cpu_renderPassPool);
    se_object_pool_destroy(&memoryManager->cpu_renderPipelinePool);
    se_object_pool_destroy(&memoryManager->cpu_framebufferPool);
    se_object_pool_destroy(&memoryManager->cpu_renderProgramPool);
    se_object_pool_destroy(&memoryManager->cpu_memoryBufferPool);
    const size_t numCommandBufferPools = se_sbuffer_size(memoryManager->cpu_commandBufferPools);
    for (size_t it = 0; it < numCommandBufferPools; it++)
        se_object_pool_destroy(&memoryManager->cpu_commandBufferPools[it]);
    se_sbuffer_destroy(memoryManager->cpu_commandBufferPools);
    const size_t numResourceSetPools = se_sbuffer_size(memoryManager->cpu_resourceSetPools);
    for (size_t it = 0; it < numResourceSetPools; it++)
        se_object_pool_destroy(&memoryManager->cpu_resourceSetPools[it]);
    se_sbuffer_destroy(memoryManager->cpu_resourceSetPools);
}

void se_vk_memory_manager_set_device(SeVkMemoryManager* memoryManager, SeRenderObject* device)
{
    se_vk_expect_handle(device, SE_RENDER_HANDLE_TYPE_DEVICE, "Can't set device to memory manager");
    memoryManager->device = device;
    memoryManager->memoryProperties = se_vk_device_get_memory_properties(device);
}

SeObjectPool* se_vk_memory_manager_get_pool(SeVkMemoryManager* memoryManager, SeRenderHandleType type)
{
    SeVkInFlightManager* inFlightManager = se_vk_device_get_in_flight_manager(memoryManager->device);
    switch (type)
    {
        case SE_RENDER_HANDLE_TYPE_PROGRAM:         return &memoryManager->cpu_renderProgramPool;
        case SE_RENDER_HANDLE_TYPE_TEXTURE:         return &memoryManager->cpu_texturePool;
        case SE_RENDER_HANDLE_TYPE_SAMPLER:         return &memoryManager->cpu_samplerPool;
        case SE_RENDER_HANDLE_TYPE_PASS:            return &memoryManager->cpu_renderPassPool;
        case SE_RENDER_HANDLE_TYPE_PIPELINE:        return &memoryManager->cpu_renderPipelinePool;
        case SE_RENDER_HANDLE_TYPE_FRAMEBUFFER:     return &memoryManager->cpu_framebufferPool;
        case SE_RENDER_HANDLE_TYPE_RESOURCE_SET:    return &memoryManager->cpu_resourceSetPools[inFlightManager->currentImageInFlight];
        case SE_RENDER_HANDLE_TYPE_MEMORY_BUFFER:   return &memoryManager->cpu_memoryBufferPool;
        case SE_RENDER_HANDLE_TYPE_COMMAND_BUFFER:  return &memoryManager->cpu_commandBufferPools[inFlightManager->currentImageInFlight];
        default: se_assert(false);
    }
    return NULL;
}

SeObjectPool* se_vk_memory_manager_get_pool_manually(SeVkMemoryManager* memoryManager, SeRenderHandleType type, size_t poolIndex)
{
    switch (type)
    {
        case SE_RENDER_HANDLE_TYPE_RESOURCE_SET:    se_assert(poolIndex < se_sbuffer_size(memoryManager->cpu_resourceSetPools)); return &memoryManager->cpu_resourceSetPools[poolIndex];
        case SE_RENDER_HANDLE_TYPE_COMMAND_BUFFER:  se_assert(poolIndex < se_sbuffer_size(memoryManager->cpu_commandBufferPools)); return &memoryManager->cpu_commandBufferPools[poolIndex];
        default: se_assert(false);
    }
    return NULL;
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
    // 1. Try to find memory in available chunks
    //
    const size_t numChunks = se_sbuffer_size(memoryManager->gpu_chunks);
    for (size_t it = 0; it < numChunks; it++)
    {
        SeVkGpuMemoryChunk* chunk = &memoryManager->gpu_chunks[it];
        se_assert(chunk->memory != VK_NULL_HANDLE);
        if (chunk->memoryTypeIndex != memoryTypeIndex) continue;
        if ((chunk->memorySize - chunk->usedMemoryBytes) < request.sizeBytes) continue;
        const size_t inChunkOffset = se_vk_memory_chunk_find_aligned_free_space(chunk, requiredNumberOfBlocks, request.alignment);
        if (inChunkOffset == chunk->ledgerSize) continue;
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
    // 2. Try to allocate new chunk
    //
    {
        SeVkGpuMemoryChunk newChunk = {0};
        VkDevice logicalHandle = se_vk_device_get_logical_handle(memoryManager->device);
        // chunkNumberOfBlocks is aligned to 8 because each ledger byte can store info about 8 blocks
        const size_t chunkNumberOfBlocks = requiredNumberOfBlocks + (8 - (requiredNumberOfBlocks % 8));
        const size_t chunkMemorySize = request.sizeBytes > SE_VK_GPU_DEFAULT_CHUNK_SIZE_BYTES
            ? chunkNumberOfBlocks * SE_VK_GPU_MEMORY_BLOCK_SIZE_BYTES
            : SE_VK_GPU_DEFAULT_CHUNK_SIZE_BYTES;
        VkMemoryAllocateInfo memoryAllocateInfo = (VkMemoryAllocateInfo)
        {
            .sType              = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext              = NULL,
            .allocationSize     = chunkMemorySize,
            .memoryTypeIndex    = memoryTypeIndex,
        };
        se_vk_check(vkAllocateMemory(logicalHandle, &memoryAllocateInfo, se_vk_memory_manager_get_callbacks(memoryManager), &newChunk.memory));
        newChunk.memorySize = chunkMemorySize;
        if (request.properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
            vkMapMemory(logicalHandle, newChunk.memory, 0, newChunk.memorySize, 0, &newChunk.mappedMemory);
        newChunk.ledgerSize = chunkNumberOfBlocks / 8;
        newChunk.ledger = memoryManager->cpu_persistentAllocator->alloc(memoryManager->cpu_persistentAllocator->allocator, newChunk.ledgerSize, se_default_alignment, se_alloc_tag);
        memset(newChunk.ledger, 0, newChunk.ledgerSize);
        newChunk.memoryTypeIndex = memoryTypeIndex;
        se_sbuffer_push(memoryManager->gpu_chunks, newChunk);
    }
    //
    // 3. Allocate memory in new chunk
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
    return &memoryManager->cpu_allocationCallbacks;
}
