#ifndef _SE_UTILS_HPP_
#define _SE_UTILS_HPP_

#include "common_includes.hpp"

namespace utils
{
    template<typename First, typename Second> struct IsComparable { static constexpr bool value = std::is_same_v<First, Second>; };
    template<typename First, typename Second> concept comparable_to = IsComparable<First, Second>::value;

    inline bool compare_raw(const void* first, const void* second, size_t size)
    {
        return memcmp(first, second, size) == 0;
    }

    template<typename First, typename Second>
    bool compare(const First& first, const Second& second)
    {
        return std::is_same_v<First, Second> && compare_raw(&first, &second, sizeof(First));
    }

    template<size_t value>
    inline constexpr size_t power_of_two()
    {
        static_assert(((value - 1) & value) == 0);
        return value;
    }

    inline bool is_power_of_two(size_t value)
    {
        return ((value - 1) & value) == 0;
    }

    template<typename Flags, typename T>
    Flags _flagify(Flags prev, T flag)
    {
        return prev | (Flags(1) << Flags(flag));
    }

    template<typename Flags, typename T, typename ... Other>
    Flags _flagify(Flags prev, T flag, Other ... other)
    {
        return _flagify(prev | (Flags(1) << Flags(flag)), other...);
    }

    template<typename Flags, typename T, typename ... Other>
    Flags flagify(T flag, Other ... other)
    {
        return _flagify(Flags(0), flag, other...);
    }

    template<typename Flags, typename T>
    Flags flagify(T flag)
    {
        return _flagify(Flags(0), flag);
    }
}

#endif
