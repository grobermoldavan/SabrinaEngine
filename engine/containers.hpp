#ifndef _SE_CONTAINERS_H_
#define _SE_CONTAINERS_H_

#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <type_traits>

#include "allocator_bindings.hpp"
#include "common_includes.hpp"
#include "platform.hpp"
#include "engine/libs/meow_hash/meow_hash_x64_aesni.h"

#define __se_memcpy(dst, src, size) memcpy(dst, src, size)
#define __se_memset(dst, val, size) memset(dst, val, size)
#define __se_memcmp(a, b, size)     (memcmp(a, b, size) == 0)

/*
    Dynamic array
*/

template<typename T>
struct DynamicArray
{
    SeAllocatorBindings allocator;
    T* memory;
    size_t size;
    size_t capacity;

    T& operator [] (size_t index)
    {
        return memory[index];
    }

    const T& operator [] (size_t index) const
    {
        return memory[index];
    }
};

namespace dynamic_array
{
    template<typename T>
    void construct(DynamicArray<T>& array, SeAllocatorBindings& allocator, size_t capacity = 4)
    {
        array =
        {
            .allocator  = allocator,
            .memory     = (T*)allocator.alloc(allocator.allocator, sizeof(T) * capacity, se_default_alignment, se_alloc_tag),
            .size       = 0,
            .capacity   = capacity,
        };
    }

    template<typename T>
    DynamicArray<T> create(SeAllocatorBindings& allocator, size_t capacity = 4)
    {
        return
        {
            .allocator  = allocator,
            .memory     = (T*)allocator.alloc(allocator.allocator, sizeof(T) * capacity, se_default_alignment, se_alloc_tag),
            .size       = 0,
            .capacity   = capacity,
        };
    }

    template<typename T>
    DynamicArray<T> create_zeroed(SeAllocatorBindings& allocator, size_t capacity = 4)
    {
        DynamicArray<T> result
        {
            .allocator  = allocator,
            .memory     = (T*)allocator.alloc(allocator.allocator, sizeof(T) * capacity, se_default_alignment, se_alloc_tag),
            .size       = 0,
            .capacity   = capacity,
        };
        __se_memset(result.memory, 0, sizeof(T) * capacity);
    }

    template<typename T>
    void destroy(DynamicArray<T>& array)
    {
        array.allocator.dealloc(array.allocator.allocator, array.memory, sizeof(T) * array.capacity);
    }

    template<typename T>
    void reserve(DynamicArray<T>& array, size_t capacity)
    {
        if (array.capacity < capacity)
        {
            SeAllocatorBindings& allocator = array.allocator;
            const size_t newCapacity = capacity;
            const size_t oldCapacity = array.capacity;
            T* newMemory = (T*)allocator.alloc(allocator.allocator, sizeof(T) * newCapacity, se_default_alignment, se_alloc_tag);
            __se_memcpy(newMemory, array.memory, sizeof(T) * oldCapacity);
            allocator.dealloc(allocator.allocator, array.memory, sizeof(T) * oldCapacity);
            array.memory = newMemory;
            array.capacity = newCapacity;
        }
    }

    template<typename T>
    inline size_t size(const DynamicArray<T>& array)
    {
        return array.size;
    }

    template<typename T>
    T& add(DynamicArray<T>& array)
    {
        if (array.size == array.capacity)
        {
            dynamic_array::reserve(array, array.capacity * 2);
        }
        return array.memory[array.size++];
    }

    template<typename T>
    T& push(DynamicArray<T>& array, const T& val)
    {
        T& pushed = dynamic_array::add(array);
        pushed = val;
        return pushed;
    }

    template<typename T>
    requires std::is_pointer_v<T>
    static T& push(DynamicArray<T>& array, nullptr_t)
    {
        T& pushed = dynamic_array::add(array);
        return pushed;
    }

    template<typename T>
    void remove(DynamicArray<T>& array, size_t index)
    {
        se_assert(index < array.size);
        array.size -= 1;
        array.memory[index] = array.memory[array.size];
    }

    template<typename T>
    void remove(DynamicArray<T>& array, const T* val)
    {
        const ptrdiff_t offset = val - array.memory;
        se_assert(offset >= 0);
        se_assert((size_t)offset < array.size);
        array.size -= 1;
        array.memory[offset] = array.memory[array.size];
    }

    template<typename T>
    inline void reset(DynamicArray<T>& array)
    {
        array.size = 0;
    }

    template<typename T>
    inline void force_set_size(DynamicArray<T>& array, size_t size)
    {
        dynamic_array::reserve(array, size);
        array.size = size;
    }

    template<typename T>
    inline T* raw(DynamicArray<T>& array)
    {
        return array.memory;
    }
};

/*
    Expandable virtual memory.

    Basically a stack that uses OS virtual memory system and commits memory pages on demand.
*/

struct ExpandableVirtualMemory
{
    SePlatformInterface* platform;
    void* base;
    size_t reserved;
    size_t commited;
    size_t used;
};

namespace expandable_virtual_memory
{
    ExpandableVirtualMemory create(SePlatformInterface* platform, size_t size)
    {
        return
        {
            .platform   = platform,
            .base       = platform->mem_reserve(size),
            .reserved   = size,
            .commited   = 0,
            .used       = 0,
        };
    }

    void destroy(ExpandableVirtualMemory& memory)
    {
        memory.platform->mem_release(memory.base, memory.reserved);
    }

    void add(ExpandableVirtualMemory& memory, size_t addition)
    {
        memory.used += addition;
        se_assert(memory.used <= memory.reserved);
        if (memory.used > memory.commited)
        {
            const size_t memPageSize = memory.platform->get_mem_page_size();
            const size_t requredToCommit = memory.used - memory.commited;
            const size_t numPagesToCommit = 1 + ((requredToCommit - 1) / memPageSize);
            const size_t memoryToCommit = numPagesToCommit * memPageSize;
            memory.platform->mem_commit(((uint8_t*)memory.base) + memory.commited, memoryToCommit);
            memory.commited += memoryToCommit;
        }
    }

    inline void* raw(const ExpandableVirtualMemory& memory)
    {
        return memory.base;
    }

    inline size_t used(const ExpandableVirtualMemory& memory)
    {
        return memory.used;
    }
}

/*
    Object pool.

    Container for an objects of the same size.
*/

struct TypelessObjectPool
{
    ExpandableVirtualMemory ledger;
    ExpandableVirtualMemory objectMemory;
    size_t currentCapacity;
};

template<typename T>
struct ObjectPool : TypelessObjectPool { };

namespace object_pool
{
    template<typename T>
    inline void construct(ObjectPool<T>& pool, SePlatformInterface* platform)
    {
        pool =
        {
            .ledger             = expandable_virtual_memory::create(platform, se_gigabytes(16)),
            .objectMemory       = expandable_virtual_memory::create(platform, se_gigabytes(16)),
            .currentCapacity    = 0,
        };
    }

    template<typename T>
    inline ObjectPool<T> create(SePlatformInterface* platform)
    {
        return
        {
            .ledger             = expandable_virtual_memory::create(platform, se_gigabytes(16)),
            .objectMemory       = expandable_virtual_memory::create(platform, se_gigabytes(16)),
            .currentCapacity    = 0,
        };
    }

    inline TypelessObjectPool create_typeless(SePlatformInterface* platform)
    {
        return
        {
            .ledger             = expandable_virtual_memory::create(platform, se_gigabytes(16)),
            .objectMemory       = expandable_virtual_memory::create(platform, se_gigabytes(16)),
            .currentCapacity    = 0,
        };
    }

    template<typename T>
    inline void destroy(ObjectPool<T>& pool)
    {
        expandable_virtual_memory::destroy(pool.ledger);
        expandable_virtual_memory::destroy(pool.objectMemory);
    }

    inline void destroy(TypelessObjectPool& pool)
    {
        expandable_virtual_memory::destroy(pool.ledger);
        expandable_virtual_memory::destroy(pool.objectMemory);
    }

    template<typename T>
    inline void reset(ObjectPool<T>& pool)
    {
        __se_memset(expandable_virtual_memory::raw(pool.ledger), 0, expandable_virtual_memory::used(pool.ledger));
    }

    template<typename T>
    T* take(ObjectPool<T>& pool)
    {
        for (size_t it = 0; it < expandable_virtual_memory::used(pool.ledger); it++)
        {
            uint8_t* byte = ((uint8_t*)expandable_virtual_memory::raw(pool.ledger)) + it;
            if (*byte == 255) continue;
            for (uint8_t byteIt = 0; byteIt < 8; byteIt++)
            {
                if ((*byte & (1 << byteIt)) == 0)
                {
                    const size_t ledgerOffset = it * 8 + byteIt;
                    const size_t memoryOffset = ledgerOffset * sizeof(T);
                    *byte |= (1 << byteIt);
                    void* memory = ((uint8_t*)expandable_virtual_memory::raw(pool.objectMemory)) + memoryOffset;
                    return (T*)memory;
                }
            }
            se_assert_msg(false, "Invalid code path");
        }
        const size_t ledgerUsed = expandable_virtual_memory::used(pool.ledger);
        const size_t ledgerOffset = ledgerUsed * 8;
        const size_t memoryOffset = ledgerOffset * sizeof(T);
        expandable_virtual_memory::add(pool.ledger, 1);
        expandable_virtual_memory::add(pool.objectMemory, sizeof(T) * 8);
        uint8_t* byte = ((uint8_t*)expandable_virtual_memory::raw(pool.ledger)) + ledgerUsed;
        *byte |= 1;
        void* memory = ((uint8_t*)expandable_virtual_memory::raw(pool.objectMemory)) + memoryOffset;
        return (T*)memory;
    }

    template<typename T>
    size_t index_of(const ObjectPool<T>& pool, const T* object)
    {
        const intptr_t base = (intptr_t)expandable_virtual_memory::raw(pool.objectMemory);
        const intptr_t end = base + (intptr_t)expandable_virtual_memory::used(pool.objectMemory);
        const intptr_t candidate = (intptr_t)object;
        se_assert_msg(candidate >= base && candidate < end, "Can't get index of an object in pool : pointer is not in the pool range");
        const size_t memoryOffset = candidate - base;
        se_assert_msg((memoryOffset % sizeof(T)) == 0, "Can't get index of an object in pool : wrong pointer provided");
        return memoryOffset / sizeof(T);
    }

    template<typename T>
    bool is_taken(const ObjectPool<T>& pool, size_t index)
    {
        se_assert_msg((pool.ledger.used * 8) > index, "Can't get check if object is taken from pool : index is out of range");
        const uint8_t* byte = ((uint8_t*)expandable_virtual_memory::raw(pool.ledger)) + (index / 8);
        return *byte & (1 << (index % 8));
    }

    template<typename T>
    void release(ObjectPool<T>& pool, const T* object)
    {
        const size_t index = object_pool::index_of(pool, object);
        se_assert(object_pool::is_taken(pool, index));
        uint8_t* byte = ((uint8_t*)expandable_virtual_memory::raw(pool.ledger)) + (index / 8);
        se_assert_msg(*byte & (1 << (index % 8)), "Can't return object to the pool : object is already returned");
        *byte &= ~(1 << (index % 8));
    }

    template<typename T>
    inline T* access(ObjectPool<T>& pool, size_t index)
    {
        se_assert(object_pool::is_taken(pool, index));
        return ((T*)expandable_virtual_memory::raw(pool.objectMemory)) + index;
    }

    template<typename T>
    using ForEachCb = void(*)(T*);

    template<typename T>
    void for_each(ObjectPool<T>& pool, ForEachCb<T> cb)
    {
        for (size_t it = 0; it < expandable_virtual_memory::used(pool.ledger) * 8; it++)
        {
            if (object_pool::is_taken(pool, it)) cb(object_pool::access(pool, it));
        }
    }

    template<typename T>
    inline ObjectPool<T>& from_typeless(TypelessObjectPool& pool)
    {
        return (ObjectPool<T>&)pool;
    }
}

/*
    Hash table.

    https://en.wikipedia.org/wiki/Linear_probing
*/

#define SeHash meow_u128

#define __se_hash_to_index(hash, capacity)          ((MeowU64From(hash, 0) % (capacity / 2)) + (MeowU64From(hash, 1) % (capacity / 2 + 1)))
#define __se_hash_entry_size(keySize, valSize)      (sizeof(__SeHashTableObjectData) + keySize + valSize)
#define __se_hash_table_access(table, index)        ((__SeHashTableObjectData*)(((char*)table->memory) + (__se_hash_entry_size(table->keySize, table->valueSize) * index)))
#define __se_hash_table_key_ptr(data)               (((char*)data) + sizeof(__SeHashTableObjectData))
#define __se_hash_table_value_ptr(data, keySize)    (((char*)data) + sizeof(__SeHashTableObjectData) + keySize)

typedef struct __SeHashTableObjectData
{
    SeHash hash;
    bool isOccupied;
} __SeHashTableObjectData;

typedef struct SeHashTable
{
    SeAllocatorBindings* allocator;
    void* memory;
    size_t keySize;
    size_t valueSize;
    size_t capacity;
    size_t size;
} SeHashTable;

typedef struct SeHashTableCreateInfo
{
    SeAllocatorBindings* allocator;
    size_t keySize;
    size_t valueSize;
    size_t capacity;
} SeHashTableCreateInfo;

typedef struct SeHashInput
{
    const void* data;
    size_t size;
} SeHashInput;

#define se_hash_cmp(h1, h2) MeowHashesAreEqual(h1, h2)

#define se_hash(src, size) MeowHash(MeowDefaultSeed, size, (void*)src)

SeHash se_hash_multiple(SeHashInput* inputs, size_t numInputs)
{
    meow_state state = {0};
    MeowBegin(&state, MeowDefaultSeed);
    for (size_t it = 0; it < numInputs; it++)
    {
        MeowAbsorb(&state, inputs[it].size, (void*)inputs[it].data);
    }
    return MeowEnd(&state, nullptr);
}

void se_hash_table_construct(SeHashTable* table, SeHashTableCreateInfo* createInfo)
{
    void* memory = createInfo->allocator->alloc
    (
        createInfo->allocator->allocator,
        createInfo->capacity * __se_hash_entry_size(createInfo->keySize, createInfo->valueSize),
        se_default_alignment,
        se_alloc_tag
    );
    __se_memset(memory, 0, createInfo->capacity * __se_hash_entry_size(createInfo->keySize, createInfo->valueSize));
    *table =
    {
        .allocator  = createInfo->allocator,
        .memory     = memory,
        .keySize    = createInfo->keySize,
        .valueSize  = createInfo->valueSize,
        .capacity   = createInfo->capacity,
        .size       = 0,
    };
}

void se_hash_table_destroy(SeHashTable* table)
{
    table->allocator->dealloc
    (
        table->allocator->allocator,
        table->memory,
        table->capacity * __se_hash_entry_size(table->keySize, table->valueSize)
    );
}

#ifdef SE_DEBUG
#define se_hash_table_set(KeyType, ValueType, table, hash, keyPtr, valuePtr) (ValueType*)__se_hash_table_set(table, hash, keyPtr, valuePtr, sizeof(KeyType), sizeof(ValueType))
void* __se_hash_table_set(SeHashTable* table, SeHash hash, const void* key, void* value, size_t keySize, size_t valueSize)
{
    se_assert(table->keySize == keySize);
    se_assert(table->valueSize == valueSize);
#else
#define se_hash_table_set(KeyType, ValueType, table, hash, keyPtr, valuePtr) (ValueType*)__se_hash_table_set(table, hash, keyPtr, valuePtr)
void* __se_hash_table_set(SeHashTable* table, SeHash hash, const void* key, void* value)
{
#endif
    //
    // Expand if needed
    //
    if (table->size >= (table->capacity / 2))
    {
        SeHashTable newTable;
        SeHashTableCreateInfo createInfo =
        {
            .allocator  = table->allocator,
            .keySize    = table->keySize,
            .valueSize  = table->valueSize,
            .capacity   = table->capacity * 2,
        };
        se_hash_table_construct(&newTable, &createInfo);
        for (size_t it = 0; it < table->capacity; it++)
        {
            __SeHashTableObjectData* data = __se_hash_table_access(table, it);
            if (data->isOccupied)
#ifdef SE_DEBUG
                __se_hash_table_set(&newTable, data->hash, __se_hash_table_key_ptr(data), __se_hash_table_value_ptr(data, table->keySize), table->keySize, table->valueSize);
#else
                __se_hash_table_set(&newTable, data->hash, __se_hash_table_key_ptr(data), __se_hash_table_value_ptr(data, table->keySize));
#endif
        }
        se_hash_table_destroy(table);
        *table = newTable;
    }
    //
    // Add new value
    //
    __SeHashTableObjectData* data = nullptr;
    const size_t initialPosition = __se_hash_to_index(hash, table->capacity);
    size_t currentPosition = initialPosition;
    do
    {
        data = __se_hash_table_access(table, currentPosition);
        if (!data->isOccupied)
        {
            break;
        }
        else if (se_hash_cmp(data->hash, hash) && __se_memcmp(__se_hash_table_key_ptr(data), key, table->keySize))
        {
            break;
        }
        currentPosition = ((currentPosition + 1) % table->capacity);
    } while (currentPosition != initialPosition);
    if (!data->isOccupied)
    {
        data->hash = hash;
        data->isOccupied = true;
        __se_memcpy(__se_hash_table_key_ptr(data), key, table->keySize);
        __se_memcpy(__se_hash_table_value_ptr(data, table->keySize), value, table->valueSize);
        table->size += 1;
    }
    else 
    {
        __se_memcpy(__se_hash_table_value_ptr(data, table->keySize), value, table->valueSize);
    }
    return __se_hash_table_value_ptr(data, table->keySize);
}

#ifdef SE_DEBUG
#define se_hash_table_get(KeyType, ValueType, table, hash, keyPtr) (ValueType*)__se_hash_table_get(table, hash, keyPtr, sizeof(KeyType), sizeof(ValueType))
void* __se_hash_table_get(SeHashTable* table, SeHash hash, const void* key, size_t keySize, size_t valueSize)
{
    se_assert(table->keySize == keySize);
    se_assert(table->valueSize == valueSize);
#else
#define se_hash_table_get(KeyType, ValueType, table, hash, keyPtr) (ValueType*)__se_hash_table_get(table, hash, keyPtr)
void* __se_hash_table_get(SeHashTable* table, SeHash hash, const void* key)
{
#endif
    const size_t initialPosition = __se_hash_to_index(hash, table->capacity);
    size_t currentPosition = initialPosition;
    do
    {
        __SeHashTableObjectData* data = __se_hash_table_access(table, currentPosition);
        if (!data->isOccupied) break;
        const bool isCorrectData = se_hash_cmp(data->hash, hash) && __se_memcmp(__se_hash_table_key_ptr(data), key, table->keySize);
        if (isCorrectData)
        {
            return __se_hash_table_value_ptr(data, table->keySize);
        }
        currentPosition = ((currentPosition + 1) % table->capacity);
    } while (currentPosition != initialPosition);
    return nullptr;
}

void __se_hash_table_remove_data(SeHashTable* table, __SeHashTableObjectData* dataToRemove)
{
    se_assert((((char*)dataToRemove) - ((char*)table->memory)) % __se_hash_entry_size(table->keySize, table->valueSize) == 0);
    const size_t removedDataPosition = (((char*)dataToRemove) - ((char*)table->memory)) / __se_hash_entry_size(table->keySize, table->valueSize);
    dataToRemove->isOccupied = false;
    table->size -= 1;
    __SeHashTableObjectData* dataToReplace = dataToRemove;
    for (size_t currentPosition = ((removedDataPosition + 1) % table->capacity); currentPosition != removedDataPosition; currentPosition = ((currentPosition + 1) % table->capacity))
    {
        __SeHashTableObjectData* data = __se_hash_table_access(table, currentPosition);
        if (!data->isOccupied) break;
        if (__se_hash_to_index(data->hash, table->capacity) <= __se_hash_to_index(dataToReplace->hash, table->capacity))
        {
            __se_memcpy(dataToReplace, data, __se_hash_entry_size(table->keySize, table->valueSize));
            data->isOccupied = false;
            dataToReplace = data;
        }
    }
}

#ifdef SE_DEBUG
#define se_hash_table_remove(KeyType, ValueType, table, hash, key) __se_hash_table_remove(table, hash, &key, sizeof(KeyType), sizeof(ValueType))
void __se_hash_table_remove(SeHashTable* table, SeHash hash, const void* key, size_t keySize, size_t valueSize)
{
    se_assert(table->keySize == keySize);
    se_assert(table->valueSize == valueSize);
#else
#define se_hash_table_remove(KeyType, ValueType, table, hash, key) __se_hash_table_remove(table, hash, &key)
void __se_hash_table_remove(SeHashTable* table, SeHash hash, const void* key)
{
#endif
#ifdef SE_DEBUG
    void* val = __se_hash_table_get(table, hash, key, table->keySize, table->valueSize);
#else
    void* val = __se_hash_table_get(table, hash, key);
#endif
    if (val)
    {
        __SeHashTableObjectData* dataToRemove = (__SeHashTableObjectData*)(((char*)val) - table->keySize - sizeof(__SeHashTableObjectData));
        __se_hash_table_remove_data(table, dataToRemove);
        // dataToRemove->isOccupied = false;
        // table->size -= 1;
        // const size_t removedDataPosition = (((char*)dataToRemove) - ((char*)table->memory)) / __se_hash_entry_size(table->keySize, table->valueSize);
        // __SeHashTableObjectData* dataToReplace = dataToRemove;
        // for (size_t currentPosition = ((removedDataPosition + 1) % table->capacity); currentPosition != removedDataPosition; currentPosition = ((currentPosition + 1) % table->capacity))
        // {
        //     __SeHashTableObjectData* data = __se_hash_table_access(table, currentPosition);
        //     if (!data->isOccupied) break;
        //     if (__se_hash_to_index(data->hash, table->capacity) <= __se_hash_to_index(dataToReplace->hash, table->capacity))
        //     {
        //         __se_memcpy(dataToReplace, data, __se_hash_entry_size(table->keySize, table->valueSize));
        //         data->isOccupied = false;
        //         dataToReplace = data;
        //     }
        // }
    }
}

void se_hash_table_reset(SeHashTable* table)
{
    for (size_t it = 0; it < table->capacity; it++)
        __se_hash_table_access(table, it)->isOccupied = false;
}

#define se_hash_table_remove_if(ValueType, tablePtr, objName, condition, destructor)    \
    {                                                                                   \
        for (size_t __it = 0; __it < (tablePtr)->capacity; __it++)                      \
        {                                                                               \
            __SeHashTableObjectData* __data = __se_hash_table_access((tablePtr), __it); \
            if (__data->isOccupied)                                                     \
            {                                                                           \
                ValueType* objName = (ValueType*)__se_hash_table_value_ptr(__data, (tablePtr)->keySize); \
                if (condition)                                                          \
                {                                                                       \
                    destructor;                                                         \
                    __se_hash_table_remove_data((tablePtr), __data);                    \
                    __it -= 1;                                                          \
                }                                                                       \
            }                                                                           \
        }                                                                               \
    }

#define se_hash_table_for_each(KeyType, ValueType, tablePtr, keyName, valName, code)    \
    {                                                                                   \
        for (size_t __it = 0; __it < (tablePtr)->capacity; __it++)                      \
        {                                                                               \
            __SeHashTableObjectData* __data = __se_hash_table_access((tablePtr), __it); \
            if (__data->isOccupied)                                                     \
            {                                                                           \
                KeyType* keyName = (KeyType*)__se_hash_table_key_ptr(__data); \
                ValueType* valName = (ValueType*)__se_hash_table_value_ptr(__data, (tablePtr)->keySize); \
                code;                                                                   \
            }                                                                           \
        }                                                                               \
    }

#endif
