#ifndef _SE_DATA_PROVIDERS_HPP_
#define _SE_DATA_PROVIDERS_HPP_

#include "se_common_includes.hpp"
#include "se_hash.hpp"
#include "se_utils.hpp"
#include "subsystems/file_system/se_file_system_data_types.hpp"

struct DataProvider
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

namespace utils
{
    template<> bool compare<DataProvider, DataProvider>(const DataProvider& first, const DataProvider& second);
}

namespace hash_value
{
    namespace builder
    {
        template<> void absorb<DataProvider>(HashValueBuilder& builder, const DataProvider& value);
    }
    template<> HashValue generate<DataProvider>(const DataProvider& value);
}

namespace data_provider
{
    bool                                    is_valid(const DataProvider& provider);
    template<size_t Size> DataProvider      from_memory(const char (&memory)[Size]);
    DataProvider                            from_memory(const void* memory, size_t size);
    template<se_not_cstring T> DataProvider from_memory(const T& obj);
    DataProvider                            from_file(const char* path);
    DataProvider                            from_file(SeFileHandle file);
    DataProviderResult                      get(const DataProvider& provider);
}

#endif
