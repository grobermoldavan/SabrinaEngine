
#include "se_string.hpp"
#include "engine/se_engine.hpp"

struct SePersistentAllocation
{
    char* memory;
    size_t capacity;
};

SeObjectPool<SePersistentAllocation> g_presistentStringPool;

inline char* _se_string_allocate_new_buffer(const SeAllocatorBindings& allocator, size_t capacity)
{
    char* memory = (char*)allocator.alloc(allocator.allocator, capacity + 1, se_default_alignment, se_alloc_tag);
    memset(memory, 0, capacity + 1);
    return memory;
}

char* _se_string_allocate_new_buffer(const SeAllocatorBindings& allocator, size_t capacity, size_t sourceLength, const char* source)
{
    char* memory = (char*)allocator.alloc(allocator.allocator, capacity + 1, se_default_alignment, se_alloc_tag);

    const size_t copySize = se_min(capacity, sourceLength);
    const size_t zeroSize = capacity - copySize + 1;
    if (copySize) memcpy(memory, source, copySize);
    memset(memory + copySize, 0, zeroSize);
    
    return memory;
}

inline void _se_string_deallocate_buffer(const SeAllocatorBindings& allocator, const char* buffer, size_t length)
{
    allocator.dealloc(allocator.allocator, (void*)buffer, length + 1);
}

SeString _se_string_create(bool isTmp, size_t length, const char* source)
{
    if (!isTmp)
    {
        se_dbg_message("Allocating new string. Length : {}, text : \"{}\"", length, source);
    }

    const SeAllocatorBindings allocator = isTmp ? se_allocator_frame() : se_allocator_persistent();
    char* const memory = source
        ? _se_string_allocate_new_buffer(allocator, length, strlen(source), source)
        : _se_string_allocate_new_buffer(allocator, length);
    if (isTmp)
    {
        return { memory, length, nullptr };
    }
    else
    {
        SePersistentAllocation* const allocation = se_object_pool_take(g_presistentStringPool);
        *allocation =
        {
            .memory = memory,
            .capacity = length,
        };
        return { memory, length, allocation };
    }
}

inline SeString _se_string_create_from_source(bool isTmp, const char* source)
{
    se_assert(source);
    return _se_string_create(isTmp, strlen(source), source);
}

inline void _se_string_persistent_allocation_destroy(const SePersistentAllocation* data)
{
    SeAllocatorBindings allocator = se_allocator_persistent();
    _se_string_deallocate_buffer(allocator, data->memory, data->capacity);
}

void _se_string_destroy(const SeString& string)
{
    if (!string.internaldData) return;

    const SePersistentAllocation* const allocation = (SePersistentAllocation*)string.internaldData;
    _se_string_persistent_allocation_destroy(allocation);
    se_object_pool_release(g_presistentStringPool, allocation);
}

SeStringBuilder _se_string_builder_begin(bool isTmp, size_t capacity, const char* source)
{
    SeAllocatorBindings allocator = isTmp ? se_allocator_frame() : se_allocator_persistent();
    const size_t sourceLength = source ? strlen(source) : 0;
    char* memory = source
        ? _se_string_allocate_new_buffer(allocator, capacity, sourceLength, source)
        : _se_string_allocate_new_buffer(allocator, capacity);
    return { memory, sourceLength, capacity, isTmp };
}

inline SeStringBuilder _se_string_builder_begin_from_source(bool isTmp, const char* source)
{
    return _se_string_builder_begin(isTmp, source ? strlen(source) : 32, source);
}

void _se_string_builder_append_with_specified_length(SeStringBuilder& builder, const char* source, size_t sourceLength)
{
    se_assert(source);
    const size_t oldLength = builder.length;
    const size_t newLength = oldLength + sourceLength;
    if (newLength > builder.capacity)
    {
        const size_t newCapacity = newLength;
        const SeAllocatorBindings allocator = builder.isTmp ? se_allocator_frame() : se_allocator_persistent();
        char* const newMemory = _se_string_allocate_new_buffer(allocator, newCapacity, oldLength, builder.memory);
        _se_string_deallocate_buffer(allocator, builder.memory, builder.capacity);
        builder.memory = newMemory;
        builder.capacity = newCapacity;
    }
    memcpy(builder.memory + oldLength, source, sourceLength);
    builder.length += sourceLength;
}

inline void _se_string_builder_append(SeStringBuilder& builder, const char* source)
{
    _se_string_builder_append_with_specified_length(builder, source, strlen(source));
}

void _se_string_builder_append_fmt(SeStringBuilder& builder, const char* fmt, const char** args, size_t numArgs)
{
    enum ParserState
    {
        IN_TEXT,
        IN_ARG,
    };
    ParserState state = IN_TEXT;
    size_t argIt = 0;
    size_t fmtPivot = 0;
    const size_t fmtLength = strlen(fmt);
    for (size_t it = 0; it < fmtLength; it++)
    {
        switch (state)
        {
            case IN_TEXT:
            {
                if (fmt[it] == '{')
                {
                    const size_t textCopySize = it - fmtPivot;
                    const char* arg = args[argIt++];
                    _se_string_builder_append_with_specified_length(builder, fmt + fmtPivot, textCopySize);
                    _se_string_builder_append(builder, arg);
                    state = IN_ARG;
                }
            } break;
            case IN_ARG:
            {
                if (fmt[it] == '}')
                {
                    fmtPivot = it + 1;
                    state = IN_TEXT;
                }
            } break;
        }
    }
    _se_string_builder_append_with_specified_length(builder, fmt + fmtPivot, fmtLength - fmtPivot);
}

SeString _se_string_builder_end(SeStringBuilder& builder)
{
    if (builder.isTmp)
    {
        return { builder.memory, builder.length, nullptr };
    }
    else
    {
        se_dbg_message("Allocating new string from builder. Capacity : {}, text : \"{}\"", builder.capacity, builder.memory);
        SePersistentAllocation* const allocation = se_object_pool_take(g_presistentStringPool);
        *allocation =
        {
            .memory = builder.memory,
            .capacity = builder.capacity,
        };
        return { builder.memory, builder.length, allocation };
    }
}

template<size_t it, typename Arg>
inline void _se_string_fill_args(const char** argsStr, const Arg& arg)
{
    argsStr[it] = se_string_cstr(se_string_cast(arg, SeStringLifetime::TEMPORARY));
}

template<size_t it, typename Arg, typename ... Other>
void _se_string_fill_args(const char** argsStr, const Arg& arg, const Other& ... other)
{
    argsStr[it] = se_string_cstr(se_string_cast(arg, SeStringLifetime::TEMPORARY));
    _se_string_fill_args<it + 1, Other...>(argsStr, other...);
}

inline const char* se_string_cstr(const SeString& str)
{
    return str.memory;
}

inline char* se_string_cstr(SeString& str)
{
    return str.memory;
}

inline size_t se_string_length(const SeString& str)
{
    return str.length;
}

inline SeString se_string_create(const SeString& source, SeStringLifetime lifetime)
{
    return _se_string_create_from_source(lifetime == SeStringLifetime::TEMPORARY, se_string_cstr(source));
}

inline SeString se_string_create(const char* source, SeStringLifetime lifetime)
{
    return _se_string_create_from_source(lifetime == SeStringLifetime::TEMPORARY, source);
}

template<typename ... Args>
inline SeString se_string_create_fmt(SeStringLifetime lifetime, const char* fmt, const Args& ... args)
{
    SeStringBuilder builder = se_string_builder_begin(nullptr, lifetime);
    se_string_builder_append_fmt(builder, fmt, args...);
    return se_string_builder_end(builder);
}

inline SeString se_string_create(size_t length, SeStringLifetime lifetime)
{
    return _se_string_create(lifetime == SeStringLifetime::TEMPORARY, length, nullptr);
}

inline void se_string_destroy(const SeString& str)
{
    _se_string_destroy(str);
}

template<typename T>
inline SeString se_string_cast(const T& value, SeStringLifetime lifetime)
{
    return se_string_create("!!! se_string_cast operation is not specified !!!");
}

template<std::unsigned_integral T>
inline SeString se_string_cast(const T& value, SeStringLifetime lifetime)
{
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%llu", uint64_t(value));
    return se_string_create(buffer, lifetime);
}

template<std::signed_integral T>
inline SeString se_string_cast(const T& value, SeStringLifetime lifetime)
{
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%lld", int64_t(value));
    return se_string_create(buffer, lifetime);
}

template<std::floating_point T>
inline SeString se_string_cast(const T& value, SeStringLifetime lifetime)
{
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%f", value);
    return se_string_create(buffer, lifetime);
}

template<se_cstring T>
inline SeString se_string_cast(const T& value, SeStringLifetime lifetime)
{
    return value == nullptr ? se_string_cast(nullptr, lifetime) : se_string_create(value, lifetime);
}

template<>
inline SeString se_string_cast<SeString>(const SeString& value, SeStringLifetime lifetime)
{
    return se_string_create(value, lifetime);
}

template<>
inline SeString se_string_cast<nullptr_t>(const nullptr_t& value, SeStringLifetime lifetime)
{
    return se_string_create("nullptr", lifetime);
}

void _se_string_init()
{
    se_object_pool_construct(g_presistentStringPool);
}

void _se_string_terminate()
{
    for (auto it : g_presistentStringPool) { _se_string_persistent_allocation_destroy(&se_iterator_value(it)); }
    se_object_pool_destroy(g_presistentStringPool);
}

inline SeStringBuilder se_string_builder_begin(const char* source, SeStringLifetime lifetime)
{
    return _se_string_builder_begin_from_source(lifetime == SeStringLifetime::TEMPORARY, source);
}

inline SeStringBuilder se_string_builder_begin(size_t capacity, const char* source, SeStringLifetime lifetime)
{
    return _se_string_builder_begin(lifetime == SeStringLifetime::TEMPORARY, capacity, source);
}

inline void se_string_builder_append(SeStringBuilder& builder, const char* source)
{
    _se_string_builder_append(builder, source);
}

inline void se_string_builder_append(SeStringBuilder& builder, char symbol)
{
    const char smallStr[2] = { symbol, 0 };
    _se_string_builder_append(builder, smallStr);
}

inline void se_string_builder_append(SeStringBuilder& builder, const SeString& source)
{
    _se_string_builder_append(builder, source.memory);
}

template<typename ... Args>
inline void se_string_builder_append_fmt(SeStringBuilder& builder, const char* fmt, const Args& ... args)
{
    if constexpr (sizeof...(Args) == 0)
    {
        _se_string_builder_append(builder, fmt);
    }
    else
    {
        const char* argsStr[sizeof...(args)];
        _se_string_fill_args<0, Args...>(argsStr, args...);
        _se_string_builder_append_fmt(builder, fmt, (const char**)argsStr, sizeof...(args));
    }
}

template<typename ... Args>
void se_string_builder_append_with_separator(SeStringBuilder& builder, const char* separator, const Args& ... args)
{
    constexpr size_t NUM_ARGS = sizeof...(args);
    static_assert(NUM_ARGS > 0);
    const char* argsStr[NUM_ARGS];
    _se_string_fill_args<0, Args...>(argsStr, args...);
    for (size_t it = 0; it < NUM_ARGS; it++)
    {
        _se_string_builder_append(builder, argsStr[it]);
        if (it < (NUM_ARGS - 1)) _se_string_builder_append(builder, separator);
    }
}

inline SeString se_string_builder_end(SeStringBuilder& builder)
{
    return _se_string_builder_end(builder);
}

template<>
inline bool se_compare<SeString, SeString>(const SeString& first, const SeString& second)
{
    return first.length == second.length && se_compare_raw(first.memory, second.memory, first.length);
}

template<se_cstring T>
inline bool se_compare(const SeString& first, const T& second)
{
    const char* const cstr = (const char*)second;
    return first.length == se_cstr_len(cstr) && se_compare_raw(first.memory, cstr, first.length);
}

template<se_cstring T>
inline bool se_compare(const T& first, const SeString& second)
{
    const char* const cstr = (const char*)first;
    return se_cstr_len(cstr) == second.length && se_compare_raw(cstr, second.memory, second.length);
}

template<se_cstring First, se_cstring Second>
inline bool se_compare(const First& first, const Second& second)
{
    const char* const firstCstr = (const char*)first;
    const char* const secondCstr = (const char*)second;
    const size_t firstLength = se_cstr_len(firstCstr);
    const size_t secondLength = se_cstr_len(secondCstr);
    const bool isCorrectLength = firstLength == secondLength;
    return isCorrectLength && se_compare_raw(firstCstr, secondCstr, firstLength);
}

template<>
inline void se_hash_value_builder_absorb<SeString>(SeHashValueBuilder& builder, const SeString& value)
{
    se_hash_value_builder_absorb_raw(builder, { value.memory, value.length });
}

template<se_cstring T>
inline void se_hash_value_builder_absorb(SeHashValueBuilder& builder, const T& value)
{
    const char* const cstr = (const char*)value;
    se_hash_value_builder_absorb_raw(builder, { (void*)cstr, strlen(cstr) });
}

template<>
inline SeHashValue se_hash_value_generate<SeString>(const SeString& value)
{
    return se_hash_value_generate_raw({ value.memory, value.length });
}

template<se_cstring T>
inline SeHashValue se_hash_value_generate(const T& value)
{
    const char* const cstr = (const char*)value;
    return se_hash_value_generate_raw({ (void*)cstr, strlen(cstr) });
}

template<se_cstrw Arg>
void _se_stringw_process(const wchar_t** elements, size_t index, const Arg& arg)
{
    elements[index] = (const wchar_t*)arg;
}

template<se_cstrw Arg, typename ... Args>
void _se_stringw_process(const wchar_t** elements, size_t index, const Arg& arg, const Args& ... args)
{
    elements[index] = (const wchar_t*)arg;
    _se_stringw_process(elements, index + 1, args...);
}

template<typename ... Args>
SeStringW se_stringw_create(const SeAllocatorBindings& allocator, const Args& ... args)
{
    const wchar_t* argsCstr[sizeof...(args)];
    _se_stringw_process(argsCstr, 0, args...);

    size_t argsLength[sizeof...(args)];
    size_t length = 0;
    for (size_t it = 0; it < sizeof...(args); it++)
    {
        const size_t argLength = se_cstr_len(argsCstr[it]);
        length += argLength;
        argsLength[it] = argLength;
    }

    wchar_t* const buffer = (wchar_t*)se_alloc(allocator, sizeof(wchar_t) * (length + 1), se_alloc_tag);
    size_t pivot = 0;
    for (size_t it = 0; it < sizeof...(args); it++)
    {
        memcpy(buffer + pivot, argsCstr[it], sizeof(wchar_t) * argsLength[it]);
        pivot += argsLength[it];
    }
    buffer[length] = 0;
    return { buffer, length };
}

inline const wchar_t* se_stringw_cstr(const SeStringW& str)
{
    return str.buffer;
}

SeString se_stringw_to_utf8(const SeStringW& str)
{
    const wchar_t* const source = str.buffer;
    const size_t sourceLength = str.length;
    const size_t targetLength = se_platform_wchar_to_utf8_required_length(source, sourceLength);
    
    // se_string_create allocates string with additional null-terminated character, so we don't need to set it manually
    SeString result = se_string_create(targetLength, SeStringLifetime::PERSISTENT);
    char* const target = se_string_cstr(result);
    se_platform_wchar_to_utf8(source, sourceLength, target, targetLength);
    
    return result;
}

SeStringW se_stringw_from_utf8(const SeAllocatorBindings& allocator, const char* source)
{
    const size_t sourceLength = se_cstr_len(source);
    const size_t targetLength = se_platform_utf8_to_wchar_required_length(source, sourceLength);

    wchar_t* const target = (wchar_t*)se_alloc(allocator, sizeof(wchar_t) * (targetLength + 1), se_alloc_tag);
    se_platform_utf8_to_wchar(source, sourceLength, target, targetLength);

    target[targetLength] = 0;
    return { target, size_t(targetLength) };
}

void se_stringw_destroy(SeStringW& str)
{
    se_dealloc(se_allocator_persistent(), str.buffer, sizeof(wchar_t) * (str.length + 1));
    str.buffer = nullptr;
    str.length = 0;
}
