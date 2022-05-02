#ifndef _SE_POOL_ALLOCATOR_SUBSYSTEM_H_
#define _SE_POOL_ALLOCATOR_SUBSYSTEM_H_

#include <inttypes.h>
#include "engine/allocator_bindings.hpp"

#define SE_POOL_ALLOCATOR_SUBSYSTEM_MAX_BUCKETS 8

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
    SePoolMemoryBucket buckets[SE_POOL_ALLOCATOR_SUBSYSTEM_MAX_BUCKETS];
    size_t numBuckets;
};

struct SePoolMemoryBucketConfig
{
    size_t blockSize;
};

struct SePoolAllocatorCreateInfo
{
    SePoolMemoryBucketConfig buckets[SE_POOL_ALLOCATOR_SUBSYSTEM_MAX_BUCKETS];
};

struct SePoolAllocatorSubsystemInterface
{
    static constexpr const char* NAME = "SePoolAllocatorSubsystemInterface";

    void                (*construct)            (SePoolAllocator& allocator, const SePoolAllocatorCreateInfo& createInfo);
    void                (*destroy)              (SePoolAllocator& allocator);
    SeAllocatorBindings (*to_allocator_bindings)(SePoolAllocator& allocator);
};

struct SePoolAllocatorSubsystem
{
    using Interface = SePoolAllocatorSubsystemInterface;
    static constexpr const char* NAME = "se_pool_allocator_subsystem";
};

#define SE_POOL_ALLOCATOR_SUBSYSTEM_GLOBAL_NAME g_poolAllocatorIface
const SePoolAllocatorSubsystemInterface* SE_POOL_ALLOCATOR_SUBSYSTEM_GLOBAL_NAME;

namespace pool_allocator
{
    inline void construct(SePoolAllocator& allocator, const SePoolAllocatorCreateInfo& createInfo)
    {
        SE_POOL_ALLOCATOR_SUBSYSTEM_GLOBAL_NAME->construct(allocator, createInfo);
    }

    inline SePoolAllocator create(const SePoolAllocatorCreateInfo& createInfo)
    {
        SePoolAllocator allocator;
        SE_POOL_ALLOCATOR_SUBSYSTEM_GLOBAL_NAME->construct(allocator, createInfo);
        return allocator;
    }

    inline void destroy(SePoolAllocator& allocator)
    {
        SE_POOL_ALLOCATOR_SUBSYSTEM_GLOBAL_NAME->destroy(allocator);
    }

    inline SeAllocatorBindings to_bindings(SePoolAllocator& allocator)
    {
        return SE_POOL_ALLOCATOR_SUBSYSTEM_GLOBAL_NAME->to_allocator_bindings(allocator);
    }
}

#endif
