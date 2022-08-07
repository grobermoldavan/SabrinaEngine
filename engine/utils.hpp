#ifndef _SE_UTILS_HPP_
#define _SE_UTILS_HPP_

#include "common_includes.hpp"

namespace utils
{
    inline bool compare_raw(const void* first, const void* second, size_t size)
    {
        return memcmp(first, second, size) == 0;
    }

    template<typename T>
    bool compare(const T& first, const T& second)
    {
        return compare_raw(&first, &second, sizeof(T));
    }

    template<size_t value>
    inline size_t power_of_two()
    {
        static_assert(((value - 1) & value) == 0);
        return value;
    }

    inline bool is_power_of_two(size_t value)
    {
        return ((value - 1) & value) == 0;
    }
}

#endif
