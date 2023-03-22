#ifndef _SE_UTILS_HPP_
#define _SE_UTILS_HPP_

#include "se_common_includes.hpp"

template<typename First, typename Second> struct SeIsComparable { static constexpr bool value = std::is_same_v<First, Second>; };
template<typename First, typename Second> concept se_comparable_to = SeIsComparable<First, Second>::value;

template<size_t NUM_FLAGS>
struct SeBitMask
{
    using Entry = uint64_t;
    static constexpr size_t ARRAY_SIZE = ((NUM_FLAGS - 1) / (sizeof(Entry) * 8)) + 1;
    Entry mask[ARRAY_SIZE];
};

template <typename T> concept se_cstrw = std::is_convertible_v<T, const wchar_t*>;
template <typename T> concept se_cstring = std::is_convertible_v<T, const char*>;
template <typename T> concept se_not_cstring = !std::is_convertible_v<T, const char*>;

template<typename T>
struct SeResult
{
    T value;
    bool result;
    operator T () const { return value; }
    operator bool () const { return result; }
};

inline bool se_compare_raw(const void* first, const void* second, size_t size)
{
    return memcmp(first, second, size) == 0;
}

template<typename First, typename Second>
inline bool se_compare(const First& first, const Second& second)
{
    static_assert(std::is_same_v<First, Second>);
    return se_compare_raw(&first, &second, sizeof(First));
}

template<size_t value>
inline constexpr size_t se_power_of_two()
{
    static_assert(((value - 1) & value) == 0);
    return value;
}

inline bool se_is_power_of_two(size_t value)
{
    return ((value - 1) & value) == 0;
}

template<typename Flags, typename T>
inline Flags _se_flagify(Flags prev, T flag)
{
    return prev | (Flags(1) << Flags(flag));
}

template<typename Flags, typename T, typename ... Other>
inline Flags _se_flagify(Flags prev, T flag, Other ... other)
{
    return _se_flagify(prev | (Flags(1) << Flags(flag)), other...);
}

template<typename Flags, typename T, typename ... Other>
inline Flags se_flagify(T flag, Other ... other)
{
    return _se_flagify(Flags(0), flag, other...);
}

template<typename Flags, typename T>
inline Flags se_flagify(T flag)
{
    return _se_flagify(Flags(0), flag);
}

template<size_t NUM_FLAGS>
inline void se_bm_set(SeBitMask<NUM_FLAGS>& mask, size_t flag)
{
    using Entry = SeBitMask<NUM_FLAGS>::Entry;
    constexpr size_t ENTRY_SIZE_BITS = (sizeof(Entry) * 8);
    const size_t entryIndex = flag / ENTRY_SIZE_BITS;
    const size_t inEntryIndex = flag % ENTRY_SIZE_BITS;
    mask.mask[entryIndex] |= Entry(1) << inEntryIndex;
}

template<size_t NUM_FLAGS>
inline void se_bm_set(SeBitMask<NUM_FLAGS>& target, const SeBitMask<NUM_FLAGS>& source)
{
    using Entry = SeBitMask<NUM_FLAGS>::Entry;
    for (size_t it = 0; it < SeBitMask<NUM_FLAGS>::ARRAY_SIZE; it++)
    {
        target.mask[it] |= source.mask[it];
    }
}

template<size_t NUM_FLAGS>
inline void se_bm_unset(SeBitMask<NUM_FLAGS>& mask, size_t flag)
{
    using Entry = SeBitMask<NUM_FLAGS>::Entry;
    constexpr size_t ENTRY_SIZE_BITS = (sizeof(Entry) * 8);
    const size_t entryIndex = flag / ENTRY_SIZE_BITS;
    const size_t inEntryIndex = flag % ENTRY_SIZE_BITS;
    mask.mask[entryIndex] &= ~(Entry(1) << inEntryIndex);
}

template<size_t NUM_FLAGS>
inline bool se_bm_get(const SeBitMask<NUM_FLAGS>& mask, size_t flag)
{
    using Entry = SeBitMask<NUM_FLAGS>::Entry;
    constexpr size_t ENTRY_SIZE_BITS = (sizeof(Entry) * 8);
    const size_t entryIndex = flag / ENTRY_SIZE_BITS;
    const size_t inEntryIndex = flag % ENTRY_SIZE_BITS;
    return mask.mask[entryIndex] & (Entry(1) << inEntryIndex);
}

template<se_cstring Str>
inline bool se_contains(const Str& str, char symbol)
{
    const char* ptr = (const char*)str;
    while (*ptr)
    {
        if (*ptr == symbol) return true;
        ptr += 1;
    }
    return false;
}

template<typename T>
inline size_t se_cstr_len(const T* str)
{
    if      constexpr (std::is_same_v<T, char>) return strlen(str);
    else if constexpr (std::is_same_v<T, wchar_t>) return wcslen(str);
    else    static_assert(!"Unsupported string type");
}

template<typename T, typename ... Args>
inline int se_cstr_sprintf(T* str, size_t strCapacity, const T* fmt, const Args& ... args)
{
    if      constexpr (std::is_same_v<T, char>)     return snprintf(str, strCapacity, fmt, args...);
    else if constexpr (std::is_same_v<T, wchar_t>)  return swprintf(str, strCapacity, fmt, args...);
    else    static_assert(!"Unsupported string type");
}

#endif
