
#include <string.h>
#include "se_vulkan_render_subsystem_memory.hpp"
#include "se_vulkan_render_subsystem_device.hpp"
#include "se_vulkan_render_subsystem_frame_manager.hpp"
#include "se_vulkan_render_subsystem_utils.hpp"
#include "se_vulkan_render_subsystem_program.hpp"
#include "se_vulkan_render_subsystem_texture.hpp"
#include "se_vulkan_render_subsystem_render_pass.hpp"
#include "se_vulkan_render_subsystem_framebuffer.hpp"
#include "se_vulkan_render_subsystem_pipeline.hpp"
#include "se_vulkan_render_subsystem_memory_buffer.hpp"
#include "se_vulkan_render_subsystem_sampler.hpp"
#include "se_vulkan_render_subsystem_command_buffer.hpp"
#include "engine/allocator_bindings.hpp"
#include "engine/common_includes.hpp"

#define SE_VK_GPU_MEMORY_BLOCK_SIZE_BYTES   64ull
#define SE_VK_GPU_DEFAULT_CHUNK_SIZE_BYTES  (32ull * 1024ull * 1024ull)

static size_t g_memoryManagerPoolIndex = 0;

template<typename T>
static size_t se_vk_memory_manager_get_pool_index()
{
    static size_t index = g_memoryManagerPoolIndex++;
    return index;
}

static void* se_vk_memory_manager_alloc(void* pUserData, size_t size, size_t alignment, VkSystemAllocationScope allocationScope)
{
    SeVkMemoryManager* manager = (SeVkMemoryManager*)pUserData;
    SeAllocatorBindings* allocator = manager->cpu_persistentAllocator;
    void* result = allocator->alloc(allocator->allocator, size, alignment, se_alloc_tag);
    dynamic_array::push(manager->cpu_allocations, { result, size });
    return result;
}

static void* se_vk_memory_manager_realloc(void* pUserData, void* pOriginal, size_t size, size_t alignment, VkSystemAllocationScope allocationScope)
{
    SeVkMemoryManager* manager = (SeVkMemoryManager*)pUserData;
    SeAllocatorBindings* allocator = manager->cpu_persistentAllocator;
    const size_t numAllocations = dynamic_array::size(manager->cpu_allocations);
    void* result = nullptr;
    for (size_t it = 0; it < numAllocations; it++)
    {
        if (manager->cpu_allocations[it].ptr == pOriginal)
        {
            result = allocator->alloc(allocator->allocator, size, alignment, se_alloc_tag);
            memcpy(result, pOriginal, manager->cpu_allocations[it].size);
            allocator->dealloc(allocator->allocator, manager->cpu_allocations[it].ptr, manager->cpu_allocations[it].size);
            manager->cpu_allocations[it] = { result, size, };
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
    const size_t numAllocations = dynamic_array::size(manager->cpu_allocations);
    for (size_t it = 0; it < numAllocations; it++)
    {
        if (manager->cpu_allocations[it].ptr == pMemory)
        {
            allocator->dealloc(allocator->allocator, manager->cpu_allocations[it].ptr, manager->cpu_allocations[it].size);
            dynamic_array::remove(manager->cpu_allocations, it);
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
    chunk->used += numBlocks * SE_VK_GPU_MEMORY_BLOCK_SIZE_BYTES;
    se_assert(chunk->used <= chunk->memorySize);
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
    se_assert(numBlocks * SE_VK_GPU_MEMORY_BLOCK_SIZE_BYTES <= chunk->used);
    chunk->used -= numBlocks * SE_VK_GPU_MEMORY_BLOCK_SIZE_BYTES;
    se_assert(chunk->used <= chunk->memorySize);
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

void se_vk_memory_manager_construct(SeVkMemoryManager* manager, SeVkMemoryManagerCreateInfo* createInfo)
{
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
        .cpu_allocations            = dynamic_array::create<SeVkCpuAllocation>(*createInfo->persistentAllocator, 4096),
        .cpu_persistentAllocator    = createInfo->persistentAllocator,
        .cpu_frameAllocator         = createInfo->frameAllocator,
        .platform                   = createInfo->platform,
        .cpu_pools                  = dynamic_array::create<TypelessObjectPool>(*createInfo->persistentAllocator, 32),
        .device                     = nullptr,
        .gpu_chunks                 = dynamic_array::create<SeVkGpuMemoryChunk>(*createInfo->persistentAllocator, 64),
        .memoryProperties           = nullptr,
    };
}

void se_vk_memory_manager_free_gpu_memory(SeVkMemoryManager* manager)
{
    VkDevice logicalHandle = se_vk_device_get_logical_handle(manager->device);
    const size_t numChunks = dynamic_array::size(manager->gpu_chunks);
    for (size_t it = 0; it < numChunks; it++)
    {
        SeVkGpuMemoryChunk* chunk = &manager->gpu_chunks[it];
        vkFreeMemory(logicalHandle, manager->gpu_chunks[it].memory, se_vk_memory_manager_get_callbacks(manager));
        manager->cpu_persistentAllocator->dealloc(manager->cpu_persistentAllocator->allocator, chunk->ledger, chunk->ledgerSize);
    }
    dynamic_array::destroy(manager->gpu_chunks);
}

void se_vk_memory_manager_free_cpu_memory(SeVkMemoryManager* manager)
{
    SeAllocatorBindings* allocator = manager->cpu_persistentAllocator;

    const size_t numAllocations = dynamic_array::size(manager->cpu_allocations);
    for (size_t it = 0; it < numAllocations; it++)
        allocator->dealloc(allocator->allocator, manager->cpu_allocations[it].ptr, manager->cpu_allocations[it].size);
    dynamic_array::destroy(manager->cpu_allocations);

    for (auto it : manager->cpu_pools)
        object_pool::destroy(iter::value(it));
}

void se_vk_memory_manager_set_device(SeVkMemoryManager* manager, SeVkDevice* device)
{
    manager->device = device;
    manager->memoryProperties = se_vk_device_get_memory_properties(device);
}

bool se_vk_memory_manager_is_valid_memory(const SeVkMemory memory)
{
    return memory.memory != VK_NULL_HANDLE;
}

SeVkMemory se_vk_memory_manager_allocate(SeVkMemoryManager* manager, const SeVkGpuAllocationRequest request)
{
    const size_t requiredNumberOfBlocks = 1 + ((request.size - 1) / SE_VK_GPU_MEMORY_BLOCK_SIZE_BYTES);
    uint32_t memoryTypeIndex = 0;
    if (!se_vk_utils_get_memory_type_index(manager->memoryProperties, request.memoryTypeBits, request.properties, &memoryTypeIndex))
        se_assert_msg(false, "Unable to find memory type index");
    //
    // 1. Try to find memory in available chunks
    //
    const size_t numChunks = dynamic_array::size(manager->gpu_chunks);
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
            .offset         = inChunkOffset * SE_VK_GPU_MEMORY_BLOCK_SIZE_BYTES,
            .size           = requiredNumberOfBlocks * SE_VK_GPU_MEMORY_BLOCK_SIZE_BYTES,
            .mappedMemory   = chunk->mappedMemory ? ((char*)chunk->mappedMemory) + inChunkOffset * SE_VK_GPU_MEMORY_BLOCK_SIZE_BYTES : nullptr,
        };
    }
    //
    // 2. Try to allocate new chunk
    //
    {
        SeVkGpuMemoryChunk newChunk = { };
        VkDevice logicalHandle = se_vk_device_get_logical_handle(manager->device);
        // chunkNumberOfBlocks is aligned to 8 because each ledger byte can store info about 8 blocks
        const size_t chunkNumberOfBlocks = request.size > SE_VK_GPU_DEFAULT_CHUNK_SIZE_BYTES
            ? requiredNumberOfBlocks + (8 - (requiredNumberOfBlocks % 8))
            : SE_VK_GPU_DEFAULT_CHUNK_SIZE_BYTES / SE_VK_GPU_MEMORY_BLOCK_SIZE_BYTES;
        const size_t chunkMemorySize = request.size > SE_VK_GPU_DEFAULT_CHUNK_SIZE_BYTES
            ? chunkNumberOfBlocks * SE_VK_GPU_MEMORY_BLOCK_SIZE_BYTES
            : SE_VK_GPU_DEFAULT_CHUNK_SIZE_BYTES;
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
        newChunk.ledger = (uint8_t*)manager->cpu_persistentAllocator->alloc(manager->cpu_persistentAllocator->allocator, newChunk.ledgerSize, se_default_alignment, se_alloc_tag);
        memset(newChunk.ledger, 0, newChunk.ledgerSize);
        newChunk.memoryTypeIndex = memoryTypeIndex;
        dynamic_array::push(manager->gpu_chunks, newChunk);
    }
    //
    // 3. Allocate memory in new chunk
    //
    {
        SeVkGpuMemoryChunk* chunk = &manager->gpu_chunks[dynamic_array::size(manager->gpu_chunks) - 1];
        size_t inChunkOffset = se_vk_memory_chunk_find_aligned_free_space(chunk, requiredNumberOfBlocks, request.alignment);
        se_assert(inChunkOffset != chunk->memorySize);
        se_vk_memory_manager_set_in_use(chunk, requiredNumberOfBlocks, inChunkOffset);
        return
        {
            .memory         = chunk->memory,
            .offset         = inChunkOffset * SE_VK_GPU_MEMORY_BLOCK_SIZE_BYTES,
            .size           = requiredNumberOfBlocks * SE_VK_GPU_MEMORY_BLOCK_SIZE_BYTES,
            .mappedMemory   = chunk->mappedMemory ? ((char*)chunk->mappedMemory) + inChunkOffset * SE_VK_GPU_MEMORY_BLOCK_SIZE_BYTES : nullptr,
        };
    }
}

void se_vk_memory_manager_deallocate(SeVkMemoryManager* manager, const SeVkMemory allocation)
{
    const size_t numChunks = dynamic_array::size(manager->gpu_chunks);
    for (size_t it = 0; it < numChunks; it++)
    {
        SeVkGpuMemoryChunk* chunk = &manager->gpu_chunks[it];
        if (chunk->memory != allocation.memory) continue;
        const size_t startBlock = allocation.offset / SE_VK_GPU_MEMORY_BLOCK_SIZE_BYTES;
        const size_t numBlocks = allocation.size / SE_VK_GPU_MEMORY_BLOCK_SIZE_BYTES;
        se_vk_memory_manager_set_free(chunk, numBlocks, startBlock);
        if (chunk->used == 0)
        {
            // Chunk is empty, so we free it
            VkDevice logicalHandle = se_vk_device_get_logical_handle(manager->device);
            vkFreeMemory(logicalHandle, chunk->memory, se_vk_memory_manager_get_callbacks(manager));
            manager->cpu_persistentAllocator->dealloc(manager->cpu_persistentAllocator->allocator, chunk->ledger, chunk->ledgerSize);
            dynamic_array::remove(manager->gpu_chunks, chunk);
        }
        break;
    }
}

template<typename T>
ObjectPool<T>& se_vk_memory_manager_create_pool(SeVkMemoryManager* manager)
{
    const size_t index = se_vk_memory_manager_get_pool_index<T>();
    if (index == dynamic_array::size(manager->cpu_pools))
    {
        dynamic_array::push(manager->cpu_pools, object_pool::create_typeless(manager->platform));
    }
    else
    {
        se_assert(index < dynamic_array::size(manager->cpu_pools));
    }
    return object_pool::from_typeless<T>(manager->cpu_pools[index]);
}

template<typename T>
ObjectPool<T>& se_vk_memory_manager_get_pool(SeVkMemoryManager* manager)
{
    const size_t index = se_vk_memory_manager_get_pool_index<T>();
    se_assert(index < dynamic_array::size(manager->cpu_pools));
    return object_pool::from_typeless<T>(manager->cpu_pools[index]);
}

VkAllocationCallbacks* se_vk_memory_manager_get_callbacks(SeVkMemoryManager* manager)
{
    return &manager->cpu_allocationCallbacks;
}