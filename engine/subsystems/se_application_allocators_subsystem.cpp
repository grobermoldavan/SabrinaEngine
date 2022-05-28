
#include "se_application_allocators_subsystem.hpp"
#include "engine/engine.hpp"

//
// Stack allocator
//

struct SeStackAllocator
{
    intptr_t base;      // actual pointer
    size_t cur;         // offset form base
    size_t reservedMax; // offset form base
    size_t commitedMax; // offset form base
};

static void* se_stack_allocator_alloc(SeStackAllocator* allocator, size_t size, size_t alignment, const char* allocTag)
{
    se_assert(((alignment - 1) & alignment) == 0 && "Alignment must be a power of two");
    //
    // Align pointer
    //
    const intptr_t stackTopPtr = allocator->base + allocator->cur;
    size_t additionalAlignment = (size_t)(stackTopPtr & (alignment - 1));
    if (additionalAlignment != 0) additionalAlignment = alignment - additionalAlignment;
    const intptr_t alignedPtr = stackTopPtr + additionalAlignment;
    const size_t alignedAllocationSize = size + additionalAlignment;
    //
    // Try allocate
    //
    if ((allocator->cur + alignedAllocationSize) > allocator->reservedMax)
    {
        return nullptr;
    }
    if ((allocator->cur + alignedAllocationSize) > allocator->commitedMax)
    {
        const intptr_t allocationSize = alignedAllocationSize - (allocator->commitedMax - allocator->cur);
        const size_t pageSize = platform::get()->get_mem_page_size();
        const size_t numPages = allocationSize / pageSize + (allocationSize % pageSize == 0 ? 0 : 1);
        void* commitedTopPtr = (void*)(allocator->base + allocator->commitedMax);
        platform::get()->mem_commit(commitedTopPtr, numPages * pageSize);
        allocator->commitedMax += numPages * pageSize;
    }
    allocator->cur += alignedAllocationSize;
    return (void*)alignedPtr;
}

static void se_stack_allocator_dealloc(SeStackAllocator* allocator, void* ptr, size_t size)
{
    // nothing
}

static SeStackAllocator se_stack_allocator_create(size_t capacity)
{
    return
    {
        .base           = (intptr_t)platform::get()->mem_reserve(capacity),
        .cur            = 0,
        .reservedMax    = capacity,
        .commitedMax    = 0,
    };
}

static void se_stack_allocator_destroy(SeStackAllocator& allocator)
{
    platform::get()->mem_release((void*)allocator.base, allocator.reservedMax);
    memset(&allocator, 0, sizeof(SeStackAllocator));
}

//
// Pool allocator
//

static constexpr size_t POOL_ALLOCATOR_MAX_BUCKETS = 8;

struct SePoolMemoryBucketSource
{
    void* base;
    size_t reserved;
    size_t commited;
    size_t used;
};

struct SePoolMemoryBucket
{
    SePoolMemoryBucketSource memory;
    SePoolMemoryBucketSource ledger;
    size_t blockSize;
};

struct SePoolAllocator
{
    SePoolMemoryBucket buckets[POOL_ALLOCATOR_MAX_BUCKETS];
    size_t numBuckets;
};

struct SePoolMemoryBucketConfig
{
    size_t blockSize;
};

struct SePoolAllocatorCreateInfo
{
    SePoolMemoryBucketConfig buckets[POOL_ALLOCATOR_MAX_BUCKETS];
};

struct SePoolMemoryBucketCompareInfo
{
    size_t bucketId;
    size_t blocksUsed;
    size_t memoryWasted;
};

void se_pool_allocator_memory_bucket_source_construct(SePoolMemoryBucketSource* source)
{
    const size_t reserveSize = se_gigabytes(32);
    source->base = platform::get()->mem_reserve(reserveSize);
    source->reserved = reserveSize;
    source->commited = 0;
    source->used = 0;
}

void se_pool_allocator_memory_bucket_source_add_memory(SePoolMemoryBucketSource* source, size_t size)
{
    source->used += size;
    if (source->used > source->commited)
    {
        const size_t requiredCommit = source->used - source->commited;
        const size_t memPageSize = platform::get()->get_mem_page_size();
        const size_t numPagesToCommit = 1 + ((requiredCommit - 1) / memPageSize);
        const size_t actualCommitSize = numPagesToCommit * memPageSize;
        se_assert((source->commited + actualCommitSize) <= source->reserved);
        platform::get()->mem_commit(((char*)source->base) + source->commited, actualCommitSize);
        source->commited += actualCommitSize;
    }
}

void se_pool_allocator_memory_bucket_source_destroy(SePoolMemoryBucketSource* source)
{
    platform::get()->mem_release(source->base, source->reserved);
}

bool se_pool_allocator_compare_infos_is_greater(void* _infos, size_t oneIndex, size_t otherIndex)
{
    SePoolMemoryBucketCompareInfo* infos = (SePoolMemoryBucketCompareInfo*)_infos;
    SePoolMemoryBucketCompareInfo* one = &infos[oneIndex];
    SePoolMemoryBucketCompareInfo* other = &infos[otherIndex];
    return one->memoryWasted == other->memoryWasted ? one->blocksUsed > other->blocksUsed : one->memoryWasted > other->memoryWasted;
}

void se_pool_allocator_compare_infos_swap(void* _infos, size_t oneIndex, size_t otherIndex)
{
    SePoolMemoryBucketCompareInfo* infos = (SePoolMemoryBucketCompareInfo*)_infos;
    SePoolMemoryBucketCompareInfo val = infos[oneIndex];
    infos[oneIndex] = infos[otherIndex];
    infos[otherIndex] = val;
}

void* se_pool_allocator_alloc(SePoolAllocator* allocator, size_t allocationSize, size_t alignment, const char* allocTag /*unused*/)
{
    if (allocationSize == 0)
    {
        return nullptr;
    }
    se_assert(((alignment - 1) & alignment) == 0 && "Alignment must be a power of two");
    //
    // Find best bucket
    //
    SePoolMemoryBucketCompareInfo compareInfos[POOL_ALLOCATOR_MAX_BUCKETS] = {0};
    for (size_t it = 0; it < allocator->numBuckets; it++)
    {
        SePoolMemoryBucket* bucket = &allocator->buckets[it];
        SePoolMemoryBucketCompareInfo* compareInfo = &compareInfos[it];
        compareInfo->bucketId = it;
        if (bucket->blockSize >= allocationSize)
        {
            compareInfo->blocksUsed = 1;
            compareInfo->memoryWasted = bucket->blockSize - allocationSize;
        }
        else
        {
            compareInfo->blocksUsed = 1 + ((allocationSize - 1) / bucket->blockSize);
            compareInfo->memoryWasted = (compareInfo->blocksUsed * bucket->blockSize) - allocationSize;
        }
    }
    se_qsort(compareInfos, 0, allocator->numBuckets - 1, se_pool_allocator_compare_infos_is_greater, se_pool_allocator_compare_infos_swap);
    //
    // Allocate memory in the best bucket
    //
    SePoolMemoryBucket* bucket = &allocator->buckets[compareInfos[0].bucketId];
    const size_t numOccupiedLedgerBits = 1 + ((allocationSize - 1) / bucket->blockSize);
    size_t firstValidLedgerBit = 0;
    while (true)
    {
        //
        // Find contiguous blocks
        //
        {
            size_t contiguousBitsCount = 0; // Contains the number of contiguous free blocks
            size_t currentLedgerBit = 0;    // Contains id of current block
            for (size_t it = 0; it < bucket->ledger.used; it++)
            {
                const uint8_t ledgerByte = *((uint8_t*)(bucket->ledger.base) + it);
                if (ledgerByte == 255)
                {
                    // Small optimization. We don't need to check byte if is already full.
                    contiguousBitsCount = 0;
                    currentLedgerBit += 8;
                    continue;
                }
                for (size_t byteIt = 0; byteIt < 8; byteIt++)
                {
                    if (((ledgerByte >> byteIt) & 1) == 1)
                    {
                        contiguousBitsCount = 0;
                    }
                    else
                    {
                        if (contiguousBitsCount == 0)
                        {
                            const uintptr_t currentBlockAddress = ((uintptr_t)(bucket->memory.base)) + (it * 8 + byteIt) * bucket->blockSize;
                            if ((currentBlockAddress % alignment) == 0 /*is block satisfies our alignment requirements*/)
                            {
                                firstValidLedgerBit = currentLedgerBit;
                                contiguousBitsCount++;
                            }
                        }
                        else
                        {
                            contiguousBitsCount++;
                        }
                    }
                    currentLedgerBit++;
                    if (contiguousBitsCount == numOccupiedLedgerBits)
                    {
                        goto block_found;
                    }
                }
            }
            // If nothing was found set firstBlockId to blockCount - this indicates failure of method
            firstValidLedgerBit = bucket->ledger.used * 8;
        }
        //
        // Commit new memory if there is not enough space for allocation
        //
        block_found:
        if (firstValidLedgerBit == (bucket->ledger.used * 8))
        {
            const size_t numOccupiedLedgerBytes = numOccupiedLedgerBits / 8 + ((numOccupiedLedgerBits % 8) ? 1 : 0);
            se_pool_allocator_memory_bucket_source_add_memory(&bucket->ledger, numOccupiedLedgerBytes);
            se_pool_allocator_memory_bucket_source_add_memory(&bucket->memory, numOccupiedLedgerBytes * 8 * bucket->blockSize);
        }
        //
        // Set ledger blocks in use if requested memory if found
        //
        else
        {
            for (size_t it = 0; it < numOccupiedLedgerBits; it++)
            {
                size_t currentByte = (firstValidLedgerBit + it) / 8;
                size_t currentBit = (firstValidLedgerBit + it) % 8;
                uint8_t* byte = ((uint8_t*)(bucket->ledger.base)) + currentByte;
                se_assert(!(*byte & (1 << currentBit)));
                *byte = *byte | (1 << currentBit);
            }
            break;
        }
    } // while (true)
    return ((uint8_t*)bucket->memory.base) + firstValidLedgerBit * bucket->blockSize;
}

void se_pool_allocator_dealloc(SePoolAllocator* allocator, void* ptr, size_t size)
{
    for (size_t it = 0; it < allocator->numBuckets; it++)
    {
        SePoolMemoryBucket* bucket = &allocator->buckets[it];
        const intptr_t base = (intptr_t)bucket->memory.base;
        const intptr_t end = base + bucket->memory.used;
        const intptr_t candidate = (intptr_t)ptr;
        if (candidate >= base && candidate < end)
        {
            const size_t numOccupiedLedgerBits = 1 + ((size - 1) / bucket->blockSize);
            const size_t firstBlock = (candidate - base) / bucket->blockSize;
            for (size_t ledgerIt = 0; ledgerIt < numOccupiedLedgerBits; ledgerIt++)
            {
                const size_t currentByte = (firstBlock + ledgerIt) / 8;
                const size_t currentBit = (firstBlock + ledgerIt) % 8;
                uint8_t* byte = ((uint8_t*)(bucket->ledger.base)) + currentByte;
                se_assert(*byte & (1 << currentBit));
                *byte = *byte & ~(1 << currentBit);
            }
            break;
        }
    }
}

SePoolAllocator se_pool_allocator_create(const SePoolAllocatorCreateInfo& createInfo)
{
    SePoolAllocator allocator = { };
    for (size_t it = 0; it < POOL_ALLOCATOR_MAX_BUCKETS; it++)
    {
        const SePoolMemoryBucketConfig* config = &createInfo.buckets[it];
        if (config->blockSize == 0) break;
        SePoolMemoryBucket* bucket = &allocator.buckets[allocator.numBuckets++];
        bucket->blockSize = config->blockSize;
        se_pool_allocator_memory_bucket_source_construct(&bucket->memory);
        se_pool_allocator_memory_bucket_source_construct(&bucket->ledger);
    }
    return allocator;
}

void se_pool_allocator_destroy(SePoolAllocator& allocator)
{
    for (size_t it = 0; it < allocator.numBuckets; it++)
    {
        SePoolMemoryBucket* bucket = &allocator.buckets[it];
        se_pool_allocator_memory_bucket_source_destroy(&bucket->memory);
        se_pool_allocator_memory_bucket_source_destroy(&bucket->ledger);
    }
}

static SeStackAllocator g_frameAllocator;
static SePoolAllocator g_persistentAllocator;
static SeApplicationAllocatorsSubsystemInterface g_iface;

SE_DLL_EXPORT void se_init(SabrinaEngine* engine)
{
    se_init_global_subsystem_pointers(engine);
    g_frameAllocator = se_stack_allocator_create(se_gigabytes(64));
    g_persistentAllocator = se_pool_allocator_create({ .buckets = { { .blockSize = 8 }, { .blockSize = 32 }, { .blockSize = 256 } }, });
    g_iface =
    {
        .frameAllocator =
        {
            .allocator = &g_frameAllocator,
            .alloc = [](void* allocator, size_t size, size_t alignment, const char* tag)
            {
                return se_stack_allocator_alloc((SeStackAllocator*)allocator, size, alignment, tag);
            },
            .dealloc = [](void* allocator, void* ptr, size_t size)
            {
                se_stack_allocator_dealloc((SeStackAllocator*)allocator, ptr, size);
            },
        },
        .persistentAllocator =
        {
            .allocator = &g_persistentAllocator,
            .alloc = [](void* allocator, size_t size, size_t alignment, const char* tag)
            {
                return se_pool_allocator_alloc((SePoolAllocator*)allocator, size, alignment, tag);
            },
            .dealloc = [](void* allocator, void* ptr, size_t size)
            {
                se_pool_allocator_dealloc((SePoolAllocator*)allocator, ptr, size);
            },
        },
    };
}

SE_DLL_EXPORT void se_terminate(SabrinaEngine* engine)
{
    se_pool_allocator_destroy(g_persistentAllocator);
    se_stack_allocator_destroy(g_frameAllocator);
}

SE_DLL_EXPORT void se_update(SabrinaEngine* engine, const UpdateInfo* info)
{
    g_frameAllocator.cur = 0;
}

SE_DLL_EXPORT void* se_get_interface(SabrinaEngine* engine)
{
    return &g_iface;
}
