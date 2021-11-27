#ifndef _SE_POOL_ALLOCATOR_SUBSYSTEM_H_
#define _SE_POOL_ALLOCATOR_SUBSYSTEM_H_

#include <inttypes.h>

#define SE_POOL_ALLOCATOR_SUBSYSTEM_NAME "se_pool_allocator_subsystem"
#define SE_POOL_ALLOCATOR_SUBSYSTEM_MAX_BUCKETS 8

typedef struct SePoolMemoryBucketSource
{
    void* base;
    size_t reserved;
    size_t commited;
} SePoolMemoryBucketSource;

typedef struct SePoolMemoryBucket
{
    SePoolMemoryBucketSource memory;
    SePoolMemoryBucketSource ledger;
    size_t blockSize;
} SePoolMemoryBucket;

typedef struct SePoolAllocator
{
    SePoolMemoryBucket buckets[SE_POOL_ALLOCATOR_SUBSYSTEM_MAX_BUCKETS];
    size_t numBuckets;
    struct SePlatformInterface* platformIface;
} SePoolAllocator;

typedef struct SePoolMemoryBucketConfig
{
    size_t blockSize;
} SePoolMemoryBucketConfig;

typedef struct SePoolAllocatorCreateInfo
{
    SePoolMemoryBucketConfig buckets[SE_POOL_ALLOCATOR_SUBSYSTEM_MAX_BUCKETS];
    struct SePlatformInterface* platformIface;
} SePoolAllocatorCreateInfo;

typedef struct SePoolAllocatorSubsystemInterface
{
    void (*construct)(SePoolAllocator* allocator, SePoolAllocatorCreateInfo* createInfo);
    void (*destroy)(SePoolAllocator* allocator);
    void (*to_allocator_bindings)(SePoolAllocator* allocator, struct SeAllocatorBindings* bindings);
} SePoolAllocatorSubsystemInterface;

#endif
