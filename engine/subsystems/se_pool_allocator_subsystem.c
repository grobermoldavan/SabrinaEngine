
#include "se_pool_allocator_subsystem.h"
#include "engine/common_includes.h"
#include "engine/allocator_bindings.h"
#include "engine/platform.h"
#include "engine/debug.h"

typedef struct SePoolMemoryBucketCompareInfo
{
    size_t bucketId;
    size_t blocksUsed;
    size_t memoryWasted;
} SePoolMemoryBucketCompareInfo;

void se_pool_allocator_construct(SePoolAllocator* allocator, SePoolAllocatorCreateInfo* createInfo);
void se_pool_allocator_destroy(SePoolAllocator* allocator);
void se_pool_allocator_to_allocator_bindings(SePoolAllocator* allocator, SeAllocatorBindings* bindings);

void* se_pool_allocator_alloc(SePoolAllocator* allocator, size_t allocationSize, size_t alignment, const char* allocTag);
void se_pool_allocator_dealloc(SePoolAllocator* allocator, void* ptr, size_t size);

static SePoolAllocatorSubsystemInterface g_iface;

SE_DLL_EXPORT void se_load(struct SabrinaEngine* engine)
{
    g_iface = (SePoolAllocatorSubsystemInterface)
    {
        .construct              = se_pool_allocator_construct,
        .destroy                = se_pool_allocator_destroy,
        .to_allocator_bindings  = se_pool_allocator_to_allocator_bindings,
    };
}

SE_DLL_EXPORT void* se_get_interface(struct SabrinaEngine* engine)
{
    return &g_iface;
}

void se_pool_allocator_memory_bucket_source_construct(SePoolMemoryBucketSource* source, SePlatformInterface* platformIface)
{
    const size_t RESERVE_SIZE = se_gigabytes(32);
    source->base = platformIface->mem_reserve(RESERVE_SIZE);
    source->reserved = RESERVE_SIZE;
    source->commited = 0;
}

void se_pool_allocator_memory_bucket_source_destroy(SePoolMemoryBucketSource* source, SePlatformInterface* platformIface)
{
    platformIface->mem_release(source->base, source->reserved);
}

void se_pool_allocator_construct(SePoolAllocator* allocator, SePoolAllocatorCreateInfo* createInfo)
{
    *allocator = (SePoolAllocator){0};
    allocator->platformIface = createInfo->platformIface;
    const size_t memPageSize = allocator->platformIface->get_mem_page_size();
    for (size_t it = 0; it < SE_POOL_ALLOCATOR_SUBSYSTEM_MAX_BUCKETS; it++)
    {
        const SePoolMemoryBucketConfig* config = &createInfo->buckets[it];
        if (config->blockSize == 0) break;
        SePoolMemoryBucket* bucket = &allocator->buckets[allocator->numBuckets++];
        bucket->blockSize = config->blockSize;
        se_pool_allocator_memory_bucket_source_construct(&bucket->memory, allocator->platformIface);
        se_pool_allocator_memory_bucket_source_construct(&bucket->ledger, allocator->platformIface);
    }
}

void se_pool_allocator_destroy(SePoolAllocator* allocator)
{
    for (size_t it = 0; it < allocator->numBuckets; it++)
    {
        SePoolMemoryBucket* bucket = &allocator->buckets[it];
        se_pool_allocator_memory_bucket_source_destroy(&bucket->memory, allocator->platformIface);
        se_pool_allocator_memory_bucket_source_destroy(&bucket->ledger, allocator->platformIface);
    }
    *allocator = (SePoolAllocator){0};
}

void se_pool_allocator_to_allocator_bindings(SePoolAllocator* allocator, SeAllocatorBindings* bindings)
{
    *bindings = (SeAllocatorBindings)
    {
        .allocator = allocator,
        .alloc = se_pool_allocator_alloc,
        .dealloc = se_pool_allocator_dealloc,
    };
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
        return NULL;
    }
    se_assert(((alignment - 1) & alignment) == 0 && "Alignment must be a power of two");
    //
    // Find best bucket
    //
    SePoolMemoryBucketCompareInfo compareInfos[SE_POOL_ALLOCATOR_SUBSYSTEM_MAX_BUCKETS] = {0};
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
            for (size_t it = 0; it < bucket->ledger.commited; it++)
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
            firstValidLedgerBit = bucket->ledger.commited * 8;
        }
        //
        // Commit new memory if there is not enough space for allocation
        //
        block_found:
        if (firstValidLedgerBit == (bucket->ledger.commited * 8))
        {
            const size_t memPageSize = allocator->platformIface->get_mem_page_size();
            size_t ledgerCommitSize = 0;
            {   // Commit ledger bucket memory
                //
                // Ledger memory gets commited with granularity of LEDGER_COMMIT_GRANULARITY
                // This is done, so we don't overcommit bucket main memory later
                //
                const size_t LEDGER_COMMIT_GRANULARITY_BYTES = 64;
                const size_t numLedgerBytes = 1 + ((numOccupiedLedgerBits - 1) / 8);
                const size_t numGranules = 1 + ((numLedgerBytes - 1) / LEDGER_COMMIT_GRANULARITY_BYTES);
                void* memCommitBase = ((uint8_t*)bucket->ledger.base) + bucket->ledger.commited;
                ledgerCommitSize = numGranules * LEDGER_COMMIT_GRANULARITY_BYTES;
                se_assert(bucket->ledger.reserved - bucket->ledger.commited >= ledgerCommitSize);
                allocator->platformIface->mem_commit(memCommitBase, ledgerCommitSize);
                bucket->ledger.commited += ledgerCommitSize;
            }
            se_assert(ledgerCommitSize != 0);
            {   // Commit main bucket memory
                //
                // Main memory commit size depends on the size of ledger commit
                //
                const size_t numPages = 1 + (((ledgerCommitSize * 8 * bucket->blockSize) - 1) / memPageSize);
                const size_t commitSize = numPages * memPageSize;
                void* memCommitBase = ((uint8_t*)bucket->memory.base) + bucket->memory.commited;
                se_assert(bucket->memory.reserved - bucket->memory.commited >= commitSize);
                allocator->platformIface->mem_commit(memCommitBase, commitSize);
                bucket->memory.commited += commitSize;
            }
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
        const intptr_t end = base + bucket->memory.commited;
        const intptr_t candidate = (intptr_t)ptr;
        if (candidate >= base && candidate < end)
        {
            const size_t numOccupiedLedgerBits = 1 + ((size - 1) / bucket->blockSize);
            const size_t firstBlock = (candidate - base) / bucket->blockSize;
            for (size_t it = 0; it < numOccupiedLedgerBits; it++)
            {
                const size_t currentByte = (firstBlock + it) / 8;
                const size_t currentBit = (firstBlock + it) % 8;
                uint8_t* byte = ((uint8_t*)(bucket->ledger.base)) + currentByte;
                se_assert(*byte & (1 << currentBit));
                *byte = *byte & ~(1 << currentBit);
            }
            break;
        }
    }
}
