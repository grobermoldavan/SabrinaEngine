
#include "se_data_providers.hpp"
#include "subsystems/se_file_system.hpp"
#include "subsystems/se_application_allocators.hpp"

namespace utils
{
    template<>
    bool compare<DataProvider, DataProvider>(const DataProvider& first, const DataProvider& second)
    {
        if (first.type != second.type) return false;
        switch (first.type)
        {
            case DataProvider::FROM_MEMORY: return compare(first.memory, second.memory);
            case DataProvider::FROM_FILE:   return compare(first.file.handle, second.file.handle);
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
                    hash_value::builder::absorb(builder, value.file.handle);
                } break;
                default: { }
            }
        }
    }

    template<>
    HashValue generate<DataProvider>(const DataProvider& value)
    {
        HashValueBuilder builder = hash_value::builder::begin();
        hash_value::builder::absorb(builder, value);
        return hash_value::builder::end(builder);
    }
}

namespace data_provider
{
    inline bool is_valid(const DataProvider& provider)
    {
        return  provider.type == DataProvider::FROM_MEMORY ||
                provider.type == DataProvider::FROM_FILE    ;
    }

    template<size_t Size>
    DataProvider from_memory(const char (&memory)[Size])
    {
        return
        {
            .type   = DataProvider::FROM_MEMORY,
            .memory = { (void*)memory, Size - 1 },
        };
    }

    DataProvider from_memory(const void* memory, size_t size)
    {
        return
        {
            .type   = DataProvider::FROM_MEMORY,
            .memory = { (void*)memory, size },
        };
    }

    template<se_not_cstring T>
    DataProvider from_memory(const T& obj)
    {
        return
        {
            .type   = DataProvider::FROM_MEMORY,
            .memory = { (void*)&obj, sizeof(T) },
        };
    }

    DataProvider from_file(const char* path)
    {
        return
        {
            .type = DataProvider::FROM_FILE,
            .file = { fs::file_find_recursive(path) },
        };
    }

    DataProvider from_file(SeFileHandle file)
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
                const SeFileContent content = fs::file_read(provider.file.handle, allocators::frame());
                return { content.data, content.dataSize };
            } break;
            default:
            {
                return { };
            }
        }
    }
}
