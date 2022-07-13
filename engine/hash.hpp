#ifndef _SE_HASH_HPP_
#define _SE_HASH_HPP_

#include "engine/common_includes.hpp"
#include "engine/libs/meow_hash/meow_hash_x64_aesni.h"

using HashValue = meow_u128;
using HashValueBuilder = meow_state;

struct HashValueInput
{
    void* data;
    size_t size;
};

namespace hash_value
{
    namespace builder
    {
        inline HashValueBuilder begin()
        {
            HashValueBuilder result = { };
            MeowBegin(&result, MeowDefaultSeed);
            return result;
        }

        inline void absorb_raw(HashValueBuilder& builder, const HashValueInput& input)
        {
            MeowAbsorb(&builder, input.size, input.data);
        }

        template<typename T>
        void absorb(HashValueBuilder& builder, const T& input)
        {
            hash_value::builder::absorb_raw(builder, { (void*)&input, sizeof(T) });
        }

        inline HashValue end(HashValueBuilder& builder)
        {
            return MeowEnd(&builder, nullptr);
        }
    }

    HashValue generate_raw(const HashValueInput& input)
    {
        return MeowHash(MeowDefaultSeed, input.size, input.data);
    }

    template<typename Value>
    HashValue generate(const Value& value)
    {
        return MeowHash(MeowDefaultSeed, sizeof(Value), (void*)&value);
    }

// @NOTE : I have no idea why, but msvc's optimizations make this function crash (probably on _mm_cmpeq_epi8 instruction)
#ifdef _MSC_VER
#   pragma optimize("", off)
#endif

    inline bool is_equal(const HashValue& first, const HashValue& second)
    {
        return MeowHashesAreEqual(first, second);
    }

#ifdef _MSC_VER
#   pragma optimize("", on)
#endif

}

#endif
