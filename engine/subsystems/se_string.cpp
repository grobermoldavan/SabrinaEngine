
#include "se_string.hpp"
#include "engine/engine.hpp"

struct SePersistentAllocation
{
    char* memory;
    size_t capacity;
};

ObjectPool<SePersistentAllocation> g_presistentStringPool;

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
    if (!isTmp)
    {
        debug::message("Allocating new string. Length : {}, text : \"{}\"", length, source);
    }

    const AllocatorBindings allocator = isTmp ? allocators::frame() : allocators::persistent();
    char* const memory = source
        ? se_string_allocate_new_buffer(allocator, length, strlen(source), source)
        : se_string_allocate_new_buffer(allocator, length);
    if (isTmp)
    {
        return { memory, length, nullptr };
    }
    else
    {
        SePersistentAllocation* const allocation = object_pool::take(g_presistentStringPool);
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
    AllocatorBindings allocator = allocators::persistent();
    se_string_deallocate_buffer(allocator, data->memory, data->capacity);
}

void se_string_destroy(const SeString& string)
{
    if (!string.internaldData) return;

    const SePersistentAllocation* const allocation = (SePersistentAllocation*)string.internaldData;
    se_string_persistent_allocation_destroy(allocation);
    object_pool::release(g_presistentStringPool, allocation);
}

SeStringBuilder se_string_builder_begin(bool isTmp, size_t capacity, const char* source)
{
    AllocatorBindings allocator = isTmp ? allocators::frame() : allocators::persistent();
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

void se_string_builder_append_with_specified_length(SeStringBuilder& builder, const char* source, size_t sourceLength)
{
    se_assert(source);
    const size_t oldLength = builder.length;
    const size_t newLength = oldLength + sourceLength;
    if (newLength > builder.capacity)
    {
        const size_t newCapacity = newLength;
        const AllocatorBindings allocator = builder.isTmp ? allocators::frame() : allocators::persistent();
        char* const newMemory = se_string_allocate_new_buffer(allocator, newCapacity, oldLength, builder.memory);
        se_string_deallocate_buffer(allocator, builder.memory, builder.capacity);
        builder.memory = newMemory;
        builder.capacity = newCapacity;
    }
    memcpy(builder.memory + oldLength, source, sourceLength);
    builder.length += sourceLength;
}

void se_string_builder_append(SeStringBuilder& builder, const char* source)
{
    se_string_builder_append_with_specified_length(builder, source, strlen(source));
}

void se_string_builder_append_fmt(SeStringBuilder& builder, const char* fmt, const char** args, size_t numArgs)
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
                    se_string_builder_append_with_specified_length(builder, fmt + fmtPivot, textCopySize);
                    se_string_builder_append(builder, arg);
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
    se_string_builder_append_with_specified_length(builder, fmt + fmtPivot, fmtLength - fmtPivot);
}

SeString se_string_builder_end(SeStringBuilder& builder)
{
    if (builder.isTmp)
    {
        return { builder.memory, builder.length, nullptr };
    }
    else
    {
        debug::message("Allocating new string from builder. Capacity : {}, text : \"{}\"", builder.capacity, builder.memory);
        SePersistentAllocation* const allocation = object_pool::take(g_presistentStringPool);
        *allocation =
        {
            .memory = builder.memory,
            .capacity = builder.capacity,
        };
        return { builder.memory, builder.length, allocation };
    }
}

namespace string
{
    inline const char* cstr(const SeString& str)
    {
        return str.memory;
    }

    inline char* cstr(SeString& str)
    {
        return str.memory;
    }

    inline size_t length(const SeString& str)
    {
        return str.length;
    }

    inline SeString create(const SeString& source, SeStringLifetime lifetime)
    {
        return se_string_create_from_source(lifetime == SeStringLifetime::TEMPORARY, string::cstr(source));
    }

    inline SeString create(const char* source, SeStringLifetime lifetime)
    {
        return se_string_create_from_source(lifetime == SeStringLifetime::TEMPORARY, source);
    }

    template<typename ... Args>
    inline SeString create_fmt(SeStringLifetime lifetime, const char* fmt, const Args& ... args)
    {
        SeStringBuilder builder = string_builder::begin(nullptr, lifetime);
        string_builder::append_fmt(builder, fmt, args...);
        return string_builder::end(builder);
    }

    inline SeString create(size_t length, SeStringLifetime lifetime)
    {
        return se_string_create(lifetime == SeStringLifetime::TEMPORARY, length, nullptr);
    }

    inline void destroy(const SeString& str)
    {
        se_string_destroy(str);
    }

    template<typename T>
    inline SeString cast(const T& value, SeStringLifetime lifetime)
    {
        return string::create("!!! string::cast operation is not specified !!!");
    }

    template<std::unsigned_integral T>
    inline SeString cast(const T& value, SeStringLifetime lifetime)
    {
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%llu", uint64_t(value));
        return string::create(buffer, lifetime);
    }

    template<std::signed_integral T>
    inline SeString cast(const T& value, SeStringLifetime lifetime)
    {
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%lld", int64_t(value));
        return string::create(buffer, lifetime);
    }

    template<std::floating_point T>
    inline SeString cast(const T& value, SeStringLifetime lifetime)
    {
        char buffer[64];
        snprintf(buffer, sizeof(buffer), "%f", value);
        return string::create(buffer, lifetime);
    }

    template<se_cstring T>
    inline SeString cast(const T& value, SeStringLifetime lifetime)
    {
        return value == nullptr ? string::cast(nullptr, lifetime) : string::create(value, lifetime);
    }

    template<>
    inline SeString cast<SeString>(const SeString& value, SeStringLifetime lifetime)
    {
        return string::create(value, lifetime);
    }

    template<>
    inline SeString cast<nullptr_t>(const nullptr_t& value, SeStringLifetime lifetime)
    {
        return string::create("nullptr", lifetime);
    }

    void engine::init()
    {
        object_pool::construct(g_presistentStringPool);
    }

    void engine::terminate()
    {
        for (auto it : g_presistentStringPool) { se_string_persistent_allocation_destroy(&iter::value(it)); }
        object_pool::destroy(g_presistentStringPool);
    }
}

namespace string_builder
{
    namespace impl
    {
        template<size_t it, typename Arg>
        void fill_args(const char** argsStr, const Arg& arg)
        {
            argsStr[it] = string::cstr(string::cast(arg, SeStringLifetime::TEMPORARY));
        }

        template<size_t it, typename Arg, typename ... Other>
        void fill_args(const char** argsStr, const Arg& arg, const Other& ... other)
        {
            argsStr[it] = string::cstr(string::cast(arg, SeStringLifetime::TEMPORARY));
            fill_args<it + 1, Other...>(argsStr, other...);
        }
    }

    SeStringBuilder begin(const char* source, SeStringLifetime lifetime)
    {
        return se_string_builder_begin_from_source(lifetime == SeStringLifetime::TEMPORARY, source);
    }

    SeStringBuilder begin(size_t capacity, const char* source, SeStringLifetime lifetime)
    {
        return se_string_builder_begin(lifetime == SeStringLifetime::TEMPORARY, capacity, source);
    }

    void append(SeStringBuilder& builder, const char* source)
    {
        se_string_builder_append(builder, source);
    }

    void append(SeStringBuilder& builder, char symbol)
    {
        const char smallStr[2] = { symbol, 0 };
        se_string_builder_append(builder, smallStr);
    }

    void append(SeStringBuilder& builder, const SeString& source)
    {
        se_string_builder_append(builder, source.memory);
    }

    template<typename ... Args>
    void append_fmt(SeStringBuilder& builder, const char* fmt, const Args& ... args)
    {
        if constexpr (sizeof...(Args) == 0)
        {
            se_string_builder_append(builder, fmt);
        }
        else
        {
            const char* argsStr[sizeof...(args)];
            impl::fill_args<0, Args...>(argsStr, args...);
            se_string_builder_append_fmt(builder, fmt, (const char**)argsStr, sizeof...(args));
        }
    }

    template<typename ... Args>
    void append_with_separator(SeStringBuilder& builder, const char* separator, const Args& ... args)
    {
        constexpr size_t NUM_ARGS = sizeof...(args);
        static_assert(NUM_ARGS > 0);
        const char* argsStr[NUM_ARGS];
        impl::fill_args<0, Args...>(argsStr, args...);
        for (size_t it = 0; it < NUM_ARGS; it++)
        {
            se_string_builder_append(builder, argsStr[it]);
            if (it < (NUM_ARGS - 1)) se_string_builder_append(builder, separator);
        }
    }

    SeString end(SeStringBuilder& builder)
    {
        return se_string_builder_end(builder);
    }
}

namespace utils
{
    template<>
    bool compare<SeString, SeString>(const SeString& first, const SeString& second)
    {
        return first.length == second.length && compare_raw(first.memory, second.memory, first.length);
    }

    template<se_cstring T>
    bool compare(const SeString& first, const T& second)
    {
        const char* const cstr = (const char*)second;
        return first.length == strlen(cstr) && compare_raw(first.memory, cstr, first.length);
    }

    template<se_cstring T>
    bool compare(const T& first, const SeString& second)
    {
        const char* const cstr = (const char*)first;
        return strlen(cstr) == second.length && compare_raw(cstr, second.memory, second.length);
    }

    template<se_cstring First, se_cstring Second>
    bool compare(const First& first, const Second& second)
    {
        const char* const firstCstr = (const char*)first;
        const char* const secondCstr = (const char*)second;
        const size_t firstLength = strlen(firstCstr);
        const size_t secondLength = strlen(secondCstr);
        const bool isCorrectLength = firstLength == secondLength;
        return isCorrectLength && compare_raw(firstCstr, secondCstr, firstLength);
    }
}

namespace hash_value
{
    namespace builder
    {
        template<>
        void absorb<SeString>(HashValueBuilder& builder, const SeString& value)
        {
            hash_value::builder::absorb_raw(builder, { value.memory, value.length });
        }

        template<se_cstring T>
        void absorb(HashValueBuilder& builder, const T& value)
        {
            const char* const cstr = (const char*)value;
            hash_value::builder::absorb_raw(builder, { (void*)cstr, strlen(cstr) });
        }
    }

    template<>
    HashValue generate<SeString>(const SeString& value)
    {
        return hash_value::generate_raw({ value.memory, value.length });
    }

    template<se_cstring T>
    HashValue generate(const T& value)
    {
        const char* const cstr = (const char*)value;
        return hash_value::generate_raw({ (void*)cstr, strlen(cstr) });
    }
}

namespace stringw
{
    namespace impl
    {
        template<se_cstrw Arg>
        void process(const wchar_t** elements, size_t index, const Arg& arg)
        {
            elements[index] = (const wchar_t*)arg;
        }

        template<se_cstrw Arg, typename ... Args>
        void process(const wchar_t** elements, size_t index, const Arg& arg, const Args& ... args)
        {
            elements[index] = (const wchar_t*)arg;
            process(elements, index + 1, args...);
        }
    }

    template<typename ... Args>
    SeStringW create(const AllocatorBindings& allocator, const Args& ... args)
    {
        const wchar_t* argsCstr[sizeof...(args)];
        impl::process(argsCstr, 0, args...);

        size_t argsLength[sizeof...(args)];
        size_t length = 0;
        for (size_t it = 0; it < sizeof...(args); it++)
        {
            const size_t argLength = utils::cstr_len(argsCstr[it]);
            length += argLength;
            argsLength[it] = argLength;
        }

        wchar_t* const buffer = (wchar_t*)allocator_bindings::alloc(allocator, sizeof(wchar_t) * (length + 1), se_alloc_tag);
        size_t pivot = 0;
        for (size_t it = 0; it < sizeof...(args); it++)
        {
            memcpy(buffer + pivot, argsCstr[it], sizeof(wchar_t) * argsLength[it]);
            pivot += argsLength[it];
        }
        buffer[length] = 0;
        return { buffer, length };
    }

    inline const wchar_t* cstr(const SeStringW& str)
    {
        return str.buffer;
    }

    SeString to_utf8(const SeStringW& str)
    {
        const wchar_t* const source = str.buffer;
        const size_t sourceLength = str.length;
        const size_t targetLength = platform::wchar_to_utf8_required_length(source, sourceLength);
        
        // string::create allocates string with additional null-terminated character, so we don't need to set it manually
        SeString result = string::create(targetLength, SeStringLifetime::PERSISTENT);
        char* const target = string::cstr(result);
        platform::wchar_to_utf8(source, sourceLength, target, targetLength);
        
        return result;
    }

    SeStringW from_utf8(const AllocatorBindings& allocator, const char* source)
    {
        const size_t sourceLength = utils::cstr_len(source);
        const size_t targetLength = platform::utf8_to_wchar_required_length(source, sourceLength);

        wchar_t* const target = (wchar_t*)allocator_bindings::alloc(allocator, sizeof(wchar_t) * (targetLength + 1), se_alloc_tag);
        platform::utf8_to_wchar(source, sourceLength, target, targetLength);

        target[targetLength] = 0;
        return { target, size_t(targetLength) };
    }

    void destroy(SeStringW& str)
    {
        allocator_bindings::dealloc(allocators::persistent(), str.buffer, sizeof(wchar_t) * (str.length + 1));
        str.buffer = nullptr;
        str.length = 0;
    }
}
