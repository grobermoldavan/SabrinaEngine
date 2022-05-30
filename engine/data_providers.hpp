#ifndef _SE_DATA_PROVIDERS_HPP_
#define _SE_DATA_PROVIDERS_HPP_

#include "hash.hpp"
#include "utils.hpp"
#include "subsystems/se_platform_subsystem.hpp"
#include "subsystems/se_application_allocators_subsystem.hpp"

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
            SeFile file;
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
    template<>
    bool compare<DataProvider>(const DataProvider& first, const DataProvider& second)
    {
        if (first.type != second.type) return false;
        switch (first.type)
        {
            case DataProvider::FROM_MEMORY: return compare(first.memory, second.memory);
            case DataProvider::FROM_FILE:   return compare(first.file.file.fullPath, second.file.file.fullPath);
            default:                        return true;
        }
    }
}

namespace hash_value
{
    namespace builder
    {
        template<>
        void absorb<DataProvider>(HashValueBuilder& builder, const DataProvider& value)
        {
            switch (value.type)
            {
                case DataProvider::FROM_MEMORY:
                {
                    hash_value::builder::absorb_raw(builder, { value.memory.ptr, value.memory.size });
                } break;
                case DataProvider::FROM_FILE:
                {
                    hash_value::builder::absorb(builder, value.file.file.fullPath);
                } break;
                default: { }
            }
        }
    }

    template<>
    HashValue generate<DataProvider>(const DataProvider& value)
    {
        HashValueBuilder builder = hash_value::builder::create();
        hash_value::builder::absorb(builder, value);
        return hash_value::builder::end(builder);
    }
}

namespace data_provider
{
    bool is_valid(const DataProvider& provider)
    {
        return  provider.type == DataProvider::FROM_MEMORY ||
                provider.type == DataProvider::FROM_FILE    ;
    }

    DataProvider from_memory(const void* memory, size_t size)
    {
        return
        {
            .type   = DataProvider::FROM_MEMORY,
            .memory = { (void*)memory, size },
        };
    }

    DataProvider from_file(const SeFile& file)
    {
        return
        {
            .type = DataProvider::FROM_FILE,
            .file = { file },
        };
    }

    DataProviderResult get(const DataProvider& provider)
    {
        switch (provider.type)
        {
            case DataProvider::UNINITIALIZED:
            {
                return { };
            } break;
            case DataProvider::FROM_MEMORY:
            {
                return { provider.memory.ptr, provider.memory.size };
            } break;
            case DataProvider::FROM_FILE:
            {
                SeFileContent content = platform::get()->file_read(&provider.file.file, app_allocators::frame());
                return { content.memory, content.size };
            } break;
            default:
            {
                return { };
            }
        }
    }
}

#endif
