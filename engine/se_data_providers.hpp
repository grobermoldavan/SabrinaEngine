#ifndef _SE_DATA_PROVIDERS_HPP_
#define _SE_DATA_PROVIDERS_HPP_

#include "se_common_includes.hpp"
#include "se_hash.hpp"
#include "se_utils.hpp"
#include "subsystems/file_system/se_file_system_data_types.hpp"

struct SeDataProvider
{
    enum
    {
        UNINITIALIZED,
        FROM_MEMORY,
        FROM_FILE,
    } type;
    union
    {
        struct
        {
            void* ptr;
            size_t size;
        } memory;
        struct
        {
            SeFileHandle handle;
        } file;
    };
};

struct DataProviderResult
{
    void* memory;
    size_t size;
};

template<> bool se_compare<SeDataProvider, SeDataProvider>(const SeDataProvider& first, const SeDataProvider& second);

template<> void se_hash_value_builder_absorb<SeDataProvider>(SeHashValueBuilder& builder, const SeDataProvider& value);
template<> SeHashValue se_hash_value_generate<SeDataProvider>(const SeDataProvider& value);

bool                                        se_data_provider_is_valid(const SeDataProvider& provider);
template<size_t Size> SeDataProvider        se_data_provider_from_memory(const char (&memory)[Size]);
SeDataProvider                              se_data_provider_from_memory(const void* memory, size_t size);
template<se_not_cstring T> SeDataProvider   se_data_provider_from_memory(const T& obj);
SeDataProvider                              se_data_provider_from_file(const char* path);
SeDataProvider                              se_data_provider_from_file(SeFileHandle file);
DataProviderResult                          se_data_provider_get(const SeDataProvider& provider);

#endif
