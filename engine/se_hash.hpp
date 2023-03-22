#ifndef _SE_HASH_HPP_
#define _SE_HASH_HPP_

#include "engine/se_common_includes.hpp"
#include "engine/libs/meow_hash/meow_hash_x64_aesni.h"

using SeHashValue = meow_u128;
using SeHashValueBuilder = meow_state;

struct SeHashValueInput
{
    void* data;
    size_t size;
};

inline SeHashValueBuilder se_hash_value_builder_begin()
{
    SeHashValueBuilder result = { };
    MeowBegin(&result, MeowDefaultSeed);
    return result;
}

inline void se_hash_value_builder_absorb_raw(SeHashValueBuilder& builder, const SeHashValueInput& input)
{
    MeowAbsorb(&builder, input.size, input.data);
}

template<typename T>
void se_hash_value_builder_absorb(SeHashValueBuilder& builder, const T& input)
{
    se_hash_value_builder_absorb_raw(builder, { (void*)&input, sizeof(T) });
}

inline SeHashValue se_hash_value_builder_end(SeHashValueBuilder& builder)
{
    return MeowEnd(&builder, nullptr);
}

SeHashValue se_hash_value_generate_raw(const SeHashValueInput& input)
{
    return MeowHash(MeowDefaultSeed, input.size, input.data);
}

template<typename Value>
SeHashValue se_hash_value_generate(const Value& value)
{
    return MeowHash(MeowDefaultSeed, sizeof(Value), (void*)&value);
}

// @NOTE : I have no idea why, but msvc's optimizations make this function crash (probably on _mm_cmpeq_epi8 instruction)
#ifdef _MSC_VER
#   pragma optimize("", off)
#endif

inline bool se_hash_value_is_equal(const SeHashValue& first, const SeHashValue& second)
{
    return MeowHashesAreEqual(first, second);
}

#ifdef _MSC_VER
#   pragma optimize("", on)
#endif

#endif
