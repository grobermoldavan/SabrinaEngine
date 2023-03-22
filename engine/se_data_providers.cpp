
#include "se_data_providers.hpp"
#include "subsystems/se_file_system.hpp"
#include "subsystems/se_application_allocators.hpp"

template<>
bool se_compare<SeDataProvider, SeDataProvider>(const SeDataProvider& first, const SeDataProvider& second)
{
    if (first.type != second.type) return false;
    switch (first.type)
    {
        case SeDataProvider::FROM_MEMORY: return se_compare(first.memory, second.memory);
        case SeDataProvider::FROM_FILE:   return se_compare(first.file.handle, second.file.handle);
        default:                        return true;
    }
}

template<>
void se_hash_value_builder_absorb<SeDataProvider>(SeHashValueBuilder& builder, const SeDataProvider& value)
{
    switch (value.type)
    {
        case SeDataProvider::FROM_MEMORY:
        {
            se_hash_value_builder_absorb_raw(builder, { value.memory.ptr, value.memory.size });
        } break;
        case SeDataProvider::FROM_FILE:
        {
            se_hash_value_builder_absorb(builder, value.file.handle);
        } break;
        default: { }
    }
}

template<>
SeHashValue se_hash_value_generate<SeDataProvider>(const SeDataProvider& value)
{
    SeHashValueBuilder builder = se_hash_value_builder_begin();
    se_hash_value_builder_absorb(builder, value);
    return se_hash_value_builder_end(builder);
}

inline bool se_data_provider_is_valid(const SeDataProvider& provider)
{
    return  provider.type == SeDataProvider::FROM_MEMORY ||
            provider.type == SeDataProvider::FROM_FILE    ;
}

template<size_t Size>
SeDataProvider se_data_provider_from_memory(const char (&memory)[Size])
{
    return
    {
        .type   = SeDataProvider::FROM_MEMORY,
        .memory = { (void*)memory, Size - 1 },
    };
}

SeDataProvider se_data_provider_from_memory(const void* memory, size_t size)
{
    return
    {
        .type   = SeDataProvider::FROM_MEMORY,
        .memory = { (void*)memory, size },
    };
}

template<se_not_cstring T>
SeDataProvider se_data_provider_from_memory(const T& obj)
{
    return
    {
        .type   = SeDataProvider::FROM_MEMORY,
        .memory = { (void*)&obj, sizeof(T) },
    };
}

SeDataProvider se_data_provider_from_file(const char* path)
{
    return
    {
        .type = SeDataProvider::FROM_FILE,
        .file = { se_fs_file_find_recursive(path) },
    };
}

SeDataProvider se_data_provider_from_file(SeFileHandle file)
{
    return
    {
        .type = SeDataProvider::FROM_FILE,
        .file = { file },
    };
}

DataProviderResult se_data_provider_get(const SeDataProvider& provider)
{
    switch (provider.type)
    {
        case SeDataProvider::UNINITIALIZED:
        {
            return { };
        } break;
        case SeDataProvider::FROM_MEMORY:
        {
            return { provider.memory.ptr, provider.memory.size };
        } break;
        case SeDataProvider::FROM_FILE:
        {
            const SeFileContent content = se_fs_file_read(provider.file.handle, se_allocator_frame());
            return { content.data, content.dataSize };
        } break;
        default:
        {
            return { };
        }
    }
}
