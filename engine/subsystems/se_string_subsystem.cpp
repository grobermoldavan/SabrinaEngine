
#include "se_string_subsystem.hpp"
#include "engine/engine.hpp"

#include <string.h>
#include <stdio.h>

struct SePersistentAllocation
{
    char* memory;
    size_t capacity;
};

SeStringSubsystemInterface g_iface;

ObjectPool<SePersistentAllocation> g_persistentstrings;

char* se_string_allocate_new_buffer(const AllocatorBindings& allocator, size_t capacity)
{
    char* memory = (char*)allocator.alloc(allocator.allocator, capacity + 1, se_default_alignment, se_alloc_tag);
    memset(memory, 0, capacity + 1);
    return memory;
}

char* se_string_allocate_new_buffer(const AllocatorBindings& allocator, size_t capacity, size_t sourceLength, const char* source)
{
    char* memory = (char*)allocator.alloc(allocator.allocator, capacity + 1, se_default_alignment, se_alloc_tag);

    const size_t copySize = se_min(capacity, sourceLength);
    const size_t zeroSize = capacity - copySize + 1;
    if (copySize) memcpy(memory, source, copySize);
    memset(memory + copySize, 0, zeroSize);
    
    return memory;
}

void se_string_deallocate_buffer(const AllocatorBindings& allocator, const char* buffer, size_t length)
{
    allocator.dealloc(allocator.allocator, (void*)buffer, length + 1);
}

SeString se_string_create(bool isTmp, size_t length, const char* source)
{
    const AllocatorBindings allocator = isTmp ? app_allocators::frame() : app_allocators::persistent();
    char* const memory = source
        ? se_string_allocate_new_buffer(allocator, length, strlen(source), source)
        : se_string_allocate_new_buffer(allocator, length);
    if (isTmp)
    {
        return { memory, length, nullptr };
    }
    else
    {
        SePersistentAllocation* const allocation = object_pool::take(g_persistentstrings);
        *allocation =
        {
            .memory = memory,
            .capacity = length,
        };
        return { memory, length, allocation };
    }
    
}

SeString se_string_create_from_source(bool isTmp, const char* source)
{
    se_assert(source);
    return se_string_create(isTmp, strlen(source), source);
}

void se_string_persistent_allocation_destroy(const SePersistentAllocation* data)
{
    AllocatorBindings allocator = app_allocators::persistent();
    se_string_deallocate_buffer(allocator, data->memory, data->capacity);
}

void se_string_destroy(const SeString& string)
{
    if (!string.internaldData) return;

    const SePersistentAllocation* const allocation = (SePersistentAllocation*)string.internaldData;
    se_string_persistent_allocation_destroy(allocation);
    object_pool::release(g_persistentstrings, allocation);
}

SeStringBuilder se_string_builder_begin(bool isTmp, size_t capacity, const char* source)
{
    AllocatorBindings allocator = isTmp ? app_allocators::frame() : app_allocators::persistent();
    const size_t sourceLength = source ? strlen(source) : 0;
    char* memory = source
        ? se_string_allocate_new_buffer(allocator, capacity, sourceLength, source)
        : se_string_allocate_new_buffer(allocator, capacity);
    return { memory, sourceLength, capacity, isTmp };
}

SeStringBuilder se_string_builder_begin_from_source(bool isTmp, const char* source)
{
    return se_string_builder_begin(isTmp, source ? strlen(source) : 32, source);
}

void se_string_builder_append(SeStringBuilder& builder, const char* source)
{
    se_assert(source);
    const size_t oldLength = builder.length;
    const size_t sourceLength = strlen(source);
    const size_t newLength = oldLength + sourceLength;
    if (newLength > builder.capacity)
    {
        const size_t newCapacity = newLength;
        const AllocatorBindings allocator = builder.isTmp ? app_allocators::frame() : app_allocators::persistent();
        char* const newMemory = se_string_allocate_new_buffer(allocator, newCapacity, oldLength, builder.memory);
        se_string_deallocate_buffer(allocator, builder.memory, builder.capacity);
        builder.memory = newMemory;
        builder.capacity = newCapacity;
    }
    memcpy(builder.memory + oldLength, source, sourceLength);
    builder.length += sourceLength;
}

SeString se_string_builder_end(SeStringBuilder& builder)
{
    if (builder.isTmp)
    {
        return { builder.memory, builder.length, nullptr };
    }
    else
    {
        SePersistentAllocation* const allocation = object_pool::take(g_persistentstrings);
        *allocation =
        {
            .memory = builder.memory,
            .capacity = builder.capacity,
        };
        return { builder.memory, builder.length, allocation };
    }
}

void se_string_from_int64(int64_t value, char* buffer, size_t bufferSize)
{
    snprintf(buffer, bufferSize, "%lld", value);
}

void se_string_from_uint64(uint64_t value, char* buffer, size_t bufferSize)
{
    snprintf(buffer, bufferSize, "%llu", value);
}

void se_string_from_double(double value, char* buffer, size_t bufferSize)
{
    snprintf(buffer, bufferSize, "%f", value);
}

SE_DLL_EXPORT void se_load(SabrinaEngine* engine)
{
    g_iface =
    {
        .create                     = se_string_create,
        .create_from_source         = se_string_create_from_source,
        .destroy                    = se_string_destroy,
        .builder_begin              = se_string_builder_begin,
        .builder_begin_from_source  = se_string_builder_begin_from_source,
        .builder_append             = se_string_builder_append,
        .builder_end                = se_string_builder_end,
        .int64_to_cstr              = se_string_from_int64,
        .uint64_to_cstr             = se_string_from_uint64,
        .double_to_cstr             = se_string_from_double,
    };
    se_init_global_subsystem_pointers(engine);
}

SE_DLL_EXPORT void se_init(SabrinaEngine* engine)
{
    object_pool::construct(g_persistentstrings);
}

SE_DLL_EXPORT void* se_get_interface(SabrinaEngine* engine)
{
    return &g_iface;
}

SE_DLL_EXPORT void se_terminate(SabrinaEngine* engine)
{
    for (auto it : g_persistentstrings) { se_string_persistent_allocation_destroy(&iter::value(it)); }
    object_pool::destroy(g_persistentstrings);
}
