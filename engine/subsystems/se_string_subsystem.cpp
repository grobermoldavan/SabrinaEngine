
#include "se_string_subsystem.hpp"
#include "se_platform_subsystem.hpp"
#include "se_application_allocators_subsystem.hpp"
#include "engine/containers.hpp"
#include "engine/se_math.hpp"
#include "engine/debug.hpp"
#include "engine/engine.hpp"

#include <cstring>

struct SeStringData
{
    char* memory;
    size_t capacity;
};

static          SeStringSubsystemInterface      g_iface;
static const    SePlatformSubsystemInterface*   g_platformIface;

static ObjectPool<SeStringData> g_strings;

static char* se_string_allocate_new_buffer(SeAllocatorBindings& allocator, size_t capacity)
{
    char* memory = (char*)allocator.alloc(allocator.allocator, capacity + 1, se_default_alignment, se_alloc_tag);
    memset(memory, 0, capacity + 1);
    return memory;
}

static char* se_string_allocate_new_buffer(SeAllocatorBindings& allocator, size_t capacity, size_t sourceLength, const char* source)
{
    char* memory = (char*)allocator.alloc(allocator.allocator, capacity + 1, se_default_alignment, se_alloc_tag);

    const size_t copySize = se_min(capacity, sourceLength);
    const size_t zeroSize = capacity - copySize + 1;
    if (copySize) memcpy(memory, source, copySize);
    memset(memory + copySize, 0, zeroSize);
    
    return memory;
}

static void se_string_deallocate_buffer(SeAllocatorBindings& allocator, const char* buffer, size_t length)
{
    allocator.dealloc(allocator.allocator, (void*)buffer, length + 1);
}

static SeString se_string_create(bool isTmp, size_t length, const char* source)
{
    SeAllocatorBindings allocator = isTmp ? app_allocators::frame() : app_allocators::persistent();
    char* memory = source
        ? se_string_allocate_new_buffer(allocator, length, strlen(source), source)
        : se_string_allocate_new_buffer(allocator, length);
    SeStringData* data = object_pool::take(g_strings);
    *data =
    {
        .memory = memory,
        .capacity = length,
    };
    return { memory, length, data };
}

static SeString se_string_create_from_source(bool isTmp, const char* source)
{
    se_assert(source);
    return se_string_create(isTmp, strlen(source), source);
}

static void se_string_data_destroy(const SeStringData* data)
{
    SeAllocatorBindings allocator = app_allocators::persistent();
    se_string_deallocate_buffer(allocator, data->memory, data->capacity);
}

static void se_string_destroy(const SeString& string)
{
    if (!string.internaldData) return;

    const SeStringData* data = (SeStringData*)string.internaldData;
    se_string_data_destroy(data);
    object_pool::release(g_strings, data);
}

static SeStringBuilder se_string_builder_begin(bool isTmp, size_t capacity, const char* source)
{
    SeAllocatorBindings allocator = isTmp ? app_allocators::frame() : app_allocators::persistent();
    const size_t sourceLength = source ? strlen(source) : 0;
    char* memory = source
        ? se_string_allocate_new_buffer(allocator, capacity, sourceLength, source)
        : se_string_allocate_new_buffer(allocator, capacity);
    return { memory, sourceLength, capacity, isTmp };
}

static SeStringBuilder se_string_builder_begin_from_source(bool isTmp, const char* source)
{
    return se_string_builder_begin(isTmp, source ? strlen(source) : 32, source);
}

static void se_string_builder_append(SeStringBuilder& builder, const char* source)
{
    se_assert(source);
    const size_t oldLength = builder.length;
    const size_t sourceLength = strlen(source);
    const size_t newLength = oldLength + sourceLength;
    if (newLength > builder.capacity)
    {
        const size_t newCapacity = newLength;
        SeAllocatorBindings allocator = builder.isTmp ? app_allocators::frame() : app_allocators::persistent();
        char* memory = se_string_allocate_new_buffer(allocator, newCapacity, oldLength, builder.memory);
        se_string_deallocate_buffer(allocator, builder.memory, builder.capacity);
        builder.memory = memory;
        builder.capacity = newCapacity;
    }
    memcpy(builder.memory + oldLength, source, sourceLength);
    builder.length += sourceLength;
}

static SeString se_string_builder_end(SeStringBuilder& builder)
{
    SeStringData* data = object_pool::take(g_strings);
    *data =
    {
        .memory = builder.memory,
        .capacity = builder.capacity,
    };
    return { builder.memory, builder.length, data };
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
    g_platformIface = se_get_subsystem_interface<SePlatformSubsystemInterface>(engine);
}

SE_DLL_EXPORT void se_init(SabrinaEngine* engine)
{
    object_pool::construct(g_strings, g_platformIface);
}

SE_DLL_EXPORT void* se_get_interface(SabrinaEngine* engine)
{
    return &g_iface;
}

SE_DLL_EXPORT void se_terminate(SabrinaEngine* engine)
{
    for (auto it : g_strings) { se_string_data_destroy(&iter::value(it)); }
    object_pool::destroy(g_strings);
}
