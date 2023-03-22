#ifndef _SE_CONTAINERS_HPP_
#define _SE_CONTAINERS_HPP_

#include <stdlib.h>

#include "se_common_includes.hpp"
#include "se_allocator_bindings.hpp"
#include "se_hash.hpp"
#include "se_utils.hpp"
#include "subsystems/se_debug.hpp"
#include "subsystems/se_platform.hpp"
#include "subsystems/se_string.hpp" // This file is included only because of se_compare definitions

/*
    Dynamic array
*/

template<typename T>
struct SeDynamicArray
{
    SeAllocatorBindings   allocator;
    T*                  memory;
    size_t              size;
    size_t              capacity;

    T& operator [] (size_t index)
    {
        return memory[index];
    }

    const T& operator [] (size_t index) const
    {
        return memory[index];
    }
};

template<typename T>
void se_dynamic_array_construct(SeDynamicArray<T>& array, SeAllocatorBindings allocator, size_t capacity = 4)
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
SeDynamicArray<T> se_dynamic_array_create(SeAllocatorBindings allocator, size_t capacity = 4)
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
SeDynamicArray<T> se_dynamic_array_create(SeAllocatorBindings allocator, std::initializer_list<T> source)
{
    const size_t size = source.size();
    T* const memory = (T*)allocator.alloc(allocator.allocator, sizeof(T) * size, se_default_alignment, se_alloc_tag);
    memcpy(memory, source.begin(), sizeof(T) * size);
    return
    {
        .allocator  = allocator,
        .memory     = memory,
        .size       = size,
        .capacity   = size,
    };
}

template<typename T>
SeDynamicArray<T> se_dynamic_array_create_zeroed(SeAllocatorBindings allocator, size_t capacity = 4)
{
    SeDynamicArray<T> result
    {
        .allocator  = allocator,
        .memory     = (T*)allocator.alloc(allocator.allocator, sizeof(T) * capacity, se_default_alignment, se_alloc_tag),
        .size       = 0,
        .capacity   = capacity,
    };
    memset(result.memory, 0, sizeof(T) * capacity);
    return result;
}

template<typename T>
void se_dynamic_array_destroy(SeDynamicArray<T>& array)
{
    if (array.memory) array.allocator.dealloc(array.allocator.allocator, array.memory, sizeof(T) * array.capacity);
}

template<typename T>
void se_dynamic_array_reserve(SeDynamicArray<T>& array, size_t capacity)
{
    if (array.capacity < capacity)
    {
        const SeAllocatorBindings& allocator = array.allocator;
        const size_t newCapacity = capacity;
        const size_t oldCapacity = array.capacity;
        T* newMemory = (T*)allocator.alloc(allocator.allocator, sizeof(T) * newCapacity, se_default_alignment, se_alloc_tag);
        memcpy(newMemory, array.memory, sizeof(T) * oldCapacity);
        allocator.dealloc(allocator.allocator, array.memory, sizeof(T) * oldCapacity);
        array.memory = newMemory;
        array.capacity = newCapacity;
    }
}

template<typename Size = size_t, typename T>
inline Size se_dynamic_array_size(const SeDynamicArray<T>& array)
{
    return Size(array.size);
}

template<typename Size = size_t, typename T>
inline Size se_dynamic_array_raw_size(const SeDynamicArray<T>& array)
{
    return Size(array.size * sizeof(T));
}

template<typename T>
inline size_t se_dynamic_array_capacity(const SeDynamicArray<T>& array)
{
    return array.capacity;
}

template<typename T>
inline const SeAllocatorBindings& se_dynamic_array_allocator(const SeDynamicArray<T>& array)
{
    return array.allocator;
}

template<typename T>
T& se_dynamic_array_add(SeDynamicArray<T>& array)
{
    if (array.size == array.capacity)
    {
        se_dynamic_array_reserve(array, array.capacity * 2);
    }
    return array.memory[array.size++];
}

template<typename T>
T& se_dynamic_array_push(SeDynamicArray<T>& array, const T& val)
{
    T& pushed = se_dynamic_array_add(array);
    pushed = val;
    return pushed;
}

template<typename T>
requires std::is_pointer_v<T>
T& se_dynamic_array_push(SeDynamicArray<T>& array, nullptr_t)
{
    T& pushed = se_dynamic_array_add(array);
    pushed = nullptr;
    return pushed;
}

template<typename T>
void se_dynamic_array_remove_idx(SeDynamicArray<T>& array, size_t index)
{
    se_assert(index < array.size);
    array.size -= 1;
    array.memory[index] = array.memory[array.size];
}

template<typename T>
void se_dynamic_array_remove(SeDynamicArray<T>& array, const T* val)
{
    const ptrdiff_t offset = val - array.memory;
    se_assert(offset >= 0);
    se_assert((size_t)offset < array.size);
    array.size -= 1;
    array.memory[offset] = array.memory[array.size];
}

template<typename T>
inline void se_dynamic_array_reset(SeDynamicArray<T>& array)
{
    array.size = 0;
}

template<typename T>
inline void se_dynamic_array_force_set_size(SeDynamicArray<T>& array, size_t size)
{
    se_dynamic_array_reserve(array, size);
    array.size = size;
}

template<typename T>
inline T* se_dynamic_array_raw(SeDynamicArray<T>& array)
{
    return array.memory;
}

template<typename T>
inline const T* se_dynamic_array_raw(const SeDynamicArray<T>& array)
{
    return array.memory;
}

template<typename T>
inline const T* se_dynamic_array_last(const SeDynamicArray<T>& array)
{
    return array.size ? &array.memory[array.size - 1] : nullptr;
}

template<typename T>
inline T* se_dynamic_array_last(SeDynamicArray<T>& array)
{
    return array.size ? &array.memory[array.size - 1] : nullptr;
}

template <typename T, typename Array>
struct SeDynamicArrayIterator;

template <typename T, typename Array>
struct SeDynamicArrayIteratorValue
{
    T& value;
    SeDynamicArrayIterator<T, Array>* iterator;
};

template <typename T, typename Array>
struct SeDynamicArrayIterator
{
    Array* arr;
    size_t index;

    bool                                    operator != (const SeDynamicArrayIterator& other) const;
    SeDynamicArrayIteratorValue<T, Array>   operator *  ();
    SeDynamicArrayIterator&                 operator ++ ();
};

template<typename T>
SeDynamicArrayIterator<T, SeDynamicArray<T>> begin(SeDynamicArray<T>& arr)
{
    return { &arr, 0 };
}

template<typename T>
SeDynamicArrayIterator<T, SeDynamicArray<T>> end(SeDynamicArray<T>& arr)
{
    return { &arr, SIZE_MAX };
}

template<typename T>
SeDynamicArrayIterator<const T, const SeDynamicArray<T>> begin(const SeDynamicArray<T>& arr)
{
    return { &arr, 0 };
}

template<typename T>
SeDynamicArrayIterator<const T, const SeDynamicArray<T>> end(const SeDynamicArray<T>& arr)
{
    return { &arr, SIZE_MAX };
}

template<typename T, typename Array>
bool SeDynamicArrayIterator<T, Array>::operator != (const SeDynamicArrayIterator<T, Array>& other) const
{
    const size_t actualIndex1 = index == SIZE_MAX ? arr->size : index;
    const size_t actualIndex2 = other.index == SIZE_MAX ? other.arr->size : other.index;
    return (arr != other.arr) || (actualIndex1 != actualIndex2);
}

template<typename T, typename Array>
SeDynamicArrayIteratorValue<T, Array> SeDynamicArrayIterator<T, Array>::operator * ()
{
    se_assert_msg(index != SIZE_MAX, "\"end\" iterator can't be dereferenced");
    return { arr->memory[index], this };
}

template<typename T, typename Array>
SeDynamicArrayIterator<T, Array>& SeDynamicArrayIterator<T, Array>::operator ++ ()
{
    index += 1;
    return *this;
}

template<typename T>
T& se_iterator_value(SeDynamicArrayIteratorValue<T, SeDynamicArray<T>>& value)
{
    return value.value;
}

template<typename T>
const T& se_iterator_value(const SeDynamicArrayIteratorValue<const T, const SeDynamicArray<T>>& value)
{
    return value.value;
}

template<typename ValueT, typename Array>
size_t se_iterator_index(const SeDynamicArrayIteratorValue<ValueT, Array>& value)
{
    return value.iterator->index;
}

template<typename T>
void se_iterator_remove(SeDynamicArrayIteratorValue<T, SeDynamicArray<T>>& value)
{
    se_dynamic_array_remove_idx(*value.iterator->arr, value.iterator->index);
    value.iterator->index -= 1;
}

/*
    Expandable virtual memory.

    Basically a stack that uses OS virtual memory system and commits memory pages on demand.
*/

template<typename T>
struct SeExpandableVirtualMemory
{
    T* base;
    size_t reservedRaw;
    size_t commitedRaw;
    size_t usedRaw;
};

template<typename T>
SeExpandableVirtualMemory<T> se_expandable_virtual_memory_create(size_t size)
{
    return
    {
        .base           = (T*)se_platform_mem_reserve(size),
        .reservedRaw    = size,
        .commitedRaw    = 0,
        .usedRaw        = 0,
    };
}

template<typename T>
void se_expandable_virtual_memory_destroy(SeExpandableVirtualMemory<T>& memory)
{
    if (memory.base) se_platform_mem_release(memory.base, memory.reservedRaw);
}

template<typename T>
void se_expandable_virtual_memory_add(SeExpandableVirtualMemory<T>& memory, size_t numberOfElementsToAdd)
{
    memory.usedRaw += numberOfElementsToAdd * sizeof(T);
    se_assert(memory.usedRaw <= memory.reservedRaw);
    if (memory.usedRaw > memory.commitedRaw)
    {
        const size_t memPageSize = se_platform_get_mem_page_size();
        const size_t requredToCommit = memory.usedRaw - memory.commitedRaw;
        const size_t numPagesToCommit = 1 + ((requredToCommit - 1) / memPageSize);
        const size_t memoryToCommit = numPagesToCommit * memPageSize;
        se_platform_mem_commit(((uint8_t*)memory.base) + memory.commitedRaw, memoryToCommit);
        memory.commitedRaw += memoryToCommit;
    }
}

template<typename T>
inline T* se_expandable_virtual_memory_raw(const SeExpandableVirtualMemory<T>& memory)
{
    return memory.base;
}

template<typename T>
inline size_t se_expandable_virtual_memory_used(const SeExpandableVirtualMemory<T>& memory)
{
    return memory.usedRaw;
}

/*
    Object pool.

    Container for an objects of the same size.
*/

template<typename T>
struct SeObjectPoolEntry
{
    T value;
    uint32_t generation;
};

template<typename T>
struct SeObjectPool
{
    SeExpandableVirtualMemory<uint8_t> ledger;
    SeExpandableVirtualMemory<SeObjectPoolEntry<T>> objectMemory;
};

template<typename T>
struct SeObjectPoolEntryRef
{
    SeObjectPool<T>* pool;
    uint32_t index;
    uint32_t generation;

    T* operator -> ();
    const T* operator -> () const;
    T* operator * ();
    const T* operator * () const;
    operator bool () const;
};

template<typename T>
bool se_compare(const SeObjectPoolEntryRef<T>& first, const SeObjectPoolEntryRef<T>& second)
{
    return (first.pool == second.pool) & (first.index == second.index) & (first.generation == second.generation);
}

template<typename T>
inline void se_object_pool_construct(SeObjectPool<T>& pool)
{
    pool =
    {
        .ledger         = se_expandable_virtual_memory_create<uint8_t>(se_gigabytes(16)),
        .objectMemory   = se_expandable_virtual_memory_create<SeObjectPoolEntry<T>>(se_gigabytes(16)),
    };
}

template<typename T>
inline SeObjectPool<T> se_object_pool_create()
{
    return
    {
        .ledger         = se_expandable_virtual_memory_create<uint8_t>(se_gigabytes(16)),
        .objectMemory   = se_expandable_virtual_memory_create<SeObjectPoolEntry<T>>(se_gigabytes(16)),
    };
}

template<typename T>
inline void se_object_pool_destroy(SeObjectPool<T>& pool)
{
    se_expandable_virtual_memory_destroy(pool.ledger);
    se_expandable_virtual_memory_destroy(pool.objectMemory);
}

template<typename T>
inline void se_object_pool_reset(SeObjectPool<T>& pool)
{
    memset(se_expandable_virtual_memory_raw(pool.ledger), 0, se_expandable_virtual_memory_used(pool.ledger));
}

template<typename T>
T* se_object_pool_take(SeObjectPool<T>& pool)
{
    const size_t ledgerUsed = se_expandable_virtual_memory_used(pool.ledger); 
    for (size_t it = 0; it < ledgerUsed; it++)
    {
        uint8_t* byte = se_expandable_virtual_memory_raw(pool.ledger) + it;
        if (*byte == 255) continue;
        for (uint8_t byteIt = 0; byteIt < 8; byteIt++)
        {
            if ((*byte & (1 << byteIt)) == 0)
            {
                const size_t indexOffset = it * 8 + byteIt;
                *byte |= (1 << byteIt);
                SeObjectPoolEntry<T>* entry = se_expandable_virtual_memory_raw(pool.objectMemory) + indexOffset;
                entry->generation += 1;
                return &entry->value;
            }
        }
        se_assert_msg(false, "Invalid code path");
    }
    const size_t indexOffset = ledgerUsed * 8;
    se_expandable_virtual_memory_add(pool.ledger, 1);
    se_expandable_virtual_memory_add(pool.objectMemory, 8);
    uint8_t* byte = se_expandable_virtual_memory_raw(pool.ledger) + ledgerUsed;
    *byte |= 1;
    SeObjectPoolEntry<T>* entry = se_expandable_virtual_memory_raw(pool.objectMemory) + indexOffset;
    entry->generation += 1;
    entry->value = { };
    return &entry->value;
}

template<typename T>
inline size_t se_object_pool_index_of(const SeObjectPool<T>& pool, const T* object)
{
    const intptr_t base = (intptr_t)se_expandable_virtual_memory_raw(pool.objectMemory);
    const intptr_t end = base + (intptr_t)se_expandable_virtual_memory_used(pool.objectMemory);
    const intptr_t candidate = (intptr_t)object;
    se_assert_msg(candidate >= base && candidate < end, "Can't get index of an object in pool : pointer is not in the pool range");
    const size_t memoryOffset = candidate - base;
    se_assert_msg((memoryOffset % sizeof(SeObjectPoolEntry<T>)) == 0, "Can't get index of an object in pool : wrong pointer provided");
    return memoryOffset / sizeof(SeObjectPoolEntry<T>);
}

template<typename T>
inline bool se_object_pool_is_taken(const SeObjectPool<T>& pool, size_t index)
{
    se_assert_msg((se_expandable_virtual_memory_used(pool.ledger) * 8) > index, "Can't get check if object is taken from pool : index is out of range");
    const uint8_t* byte = ((uint8_t*)se_expandable_virtual_memory_raw(pool.ledger)) + (index / 8);
    return *byte & (1 << (index % 8));
}

template<typename T>
inline T* se_object_pool_access(SeObjectPool<T>& pool, size_t index)
{
    se_assert(se_object_pool_is_taken(pool, index));
    SeObjectPoolEntry<T>* entry = se_expandable_virtual_memory_raw(pool.objectMemory) + index;
    return &entry->value;
}

template<typename T>
inline const T* se_object_pool_access(const SeObjectPool<T>& pool, size_t index)
{
    se_assert(se_object_pool_is_taken(pool, index));
    const SeObjectPoolEntry<T>* entry = se_expandable_virtual_memory_raw(pool.objectMemory) + index;
    return &entry->value;
}

template<typename T>
inline T* se_object_pool_access(SeObjectPool<T>& pool, size_t index, uint32_t generation)
{
    se_assert(se_object_pool_is_taken(pool, index));
    SeObjectPoolEntry<T>* entry = se_expandable_virtual_memory_raw(pool.objectMemory) + index;
    return entry->generation == generation ? &entry->value : nullptr;
}

template<typename T>
inline const T* se_object_pool_access(const SeObjectPool<T>& pool, size_t index, uint32_t generation)
{
    se_assert(se_object_pool_is_taken(pool, index));
    const SeObjectPoolEntry<T>* entry = se_expandable_virtual_memory_raw(pool.objectMemory) + index;
    return entry->generation == generation ? &entry->value : nullptr;
}

template<typename T>
inline void se_object_pool_release(SeObjectPool<T>& pool, const T* object)
{
    const size_t index = se_object_pool_index_of(pool, object);
    se_assert(se_object_pool_is_taken(pool, index));
    uint8_t* ledgerByte = se_expandable_virtual_memory_raw(pool.ledger) + (index / 8);
    se_assert_msg(*ledgerByte & (1 << (index % 8)), "Can't return object to the pool : object is already returned");
    *ledgerByte &= ~(1 << (index % 8));
}

template<typename T>
inline SeObjectPoolEntryRef<T> se_object_pool_to_ref(SeObjectPool<T>& pool, const T* object)
{
    const size_t index = se_object_pool_index_of(pool, object);
    const SeObjectPoolEntry<T>* entry = se_expandable_virtual_memory_raw(pool.objectMemory) + index;
    return
    {
        &pool,
        (uint32_t)index,
        entry->generation,
    };
}

template<typename T>
inline SeObjectPoolEntryRef<T> se_object_pool_to_ref(SeObjectPool<T>& pool, size_t index)
{
    const SeObjectPoolEntry<T>* entry = se_expandable_virtual_memory_raw(pool.objectMemory) + index;
    return
    {
        &pool,
        (uint32_t)index,
        entry->generation,
    };
}

template<typename T>
inline T* se_object_pool_from_ref(SeObjectPool<T>& pool, SeObjectPoolEntryRef<T> ref)
{
    SeObjectPoolEntry<T>* entry = se_expandable_virtual_memory_raw(pool.objectMemory) + ref.index;
    return (entry->generation != ref.generation) || !se_object_pool_is_taken(pool, ref.index) ? nullptr : &entry->value;
}

template<typename T>
inline const T* se_object_pool_from_ref(const SeObjectPool<T>& pool, SeObjectPoolEntryRef<T> ref)
{
    const SeObjectPoolEntry<T>* entry = se_expandable_virtual_memory_raw(pool.objectMemory) + ref.index;
    return (entry->generation != ref.generation) || !se_object_pool_is_taken(pool, ref.index) ? nullptr : &entry->value;
}

template<typename T>
inline T* SeObjectPoolEntryRef<T>::operator -> ()
{
    return se_object_pool_from_ref(*pool, *this);
}

template<typename T>
inline const T* SeObjectPoolEntryRef<T>::operator -> () const
{
    return se_object_pool_from_ref(*pool, *this);;
}

template<typename T>
inline T* SeObjectPoolEntryRef<T>::operator * ()
{
    return se_object_pool_from_ref(*pool, *this);
}

template<typename T>
inline const T* SeObjectPoolEntryRef<T>::operator * () const
{
    return se_object_pool_from_ref(*pool, *this);;
}

template<typename T>
inline SeObjectPoolEntryRef<T>::operator bool () const
{
    return pool != nullptr;
}

template<typename T>
struct SeObjectPoolIteratorValue
{
    T& val;
};

template<typename Pool, typename T>
struct SeObjectPoolIterator
{
    Pool& pool;
    size_t index;

    bool                            operator != (const SeObjectPoolIterator& other) const;
    SeObjectPoolIteratorValue<T>    operator *  ();
    SeObjectPoolIterator&           operator ++ ();
};

template<typename T>
inline SeObjectPoolIterator<SeObjectPool<T>, T> begin(SeObjectPool<T>& pool)
{
    SeObjectPoolIterator<SeObjectPool<T>, T> result = { pool, SIZE_MAX };
    result.operator ++();
    return result;
}

template<typename T>
inline SeObjectPoolIterator<SeObjectPool<T>, T> end(SeObjectPool<T>& pool)
{
    return { pool, se_expandable_virtual_memory_used(pool.ledger) * 8 };
}

template<typename T>
inline SeObjectPoolIterator<const SeObjectPool<T>, const T> begin(const SeObjectPool<T>& pool)
{
    SeObjectPoolIterator<const SeObjectPool<T>, const T> result = { pool, SIZE_MAX };
    result.operator ++();
    return result;
}

template<typename T>
inline SeObjectPoolIterator<const SeObjectPool<T>, const T> end(const SeObjectPool<T>& pool)
{
    return { pool, se_expandable_virtual_memory_used(pool.ledger) * 8 };
}

template<typename Pool, typename T>
inline bool SeObjectPoolIterator<Pool, T>::operator != (const SeObjectPoolIterator<Pool, T>& other) const
{
    return (&pool != &other.pool) || (index != other.index);
}

template<typename Pool, typename T>
inline SeObjectPoolIteratorValue<T> SeObjectPoolIterator<Pool, T>::operator * ()
{
    return { *se_object_pool_access<T>(pool, index) };
}

template<typename Pool, typename T>
SeObjectPoolIterator<Pool, T>& SeObjectPoolIterator<Pool, T>::operator ++ ()
{
    const size_t used = se_expandable_virtual_memory_used(pool.ledger);
    const size_t ledgerSize = used * 8;
    const uint8_t* ledger = se_expandable_virtual_memory_raw(pool.ledger);
    
    for (index += 1; index < ledgerSize; index += 1)
    {
        const uint8_t ledgerByte = *(ledger + (index / 8));
        const bool isOccupied = ledgerByte & (1 << (index % 8));
        if (isOccupied) break;
    }

    return *this;
}

template<typename T>
inline T& se_iterator_value(SeObjectPoolIteratorValue<T>& val)
{
    return val.val;
}

template<typename T>
inline const T& se_iterator_value(SeObjectPoolIteratorValue<const T>& val)
{
    return val.val;
}

/*
    Hash table.

    https://en.wikipedia.org/wiki/Linear_probing
*/

template<typename Key, typename Value>
struct SeHashTable
{
    using KeyType = Key;
    using ValueType = Key;
    struct Entry
    {
        Key         key;
        Value       value;
        SeHashValue   hash;
        bool        isOccupied;
    };

    SeAllocatorBindings   allocator;
    Entry*              memory;
    size_t              capacity;
    size_t              size;
};


inline size_t _se_hash_table_hash_to_index(SeHashValue hash, size_t capacity)
{
    return (MeowU64From(hash, 0) % (capacity / 2)) + (MeowU64From(hash, 1) % (capacity / 2 + 1));
}

template<typename Key, typename Value, typename ProvidedKey>
requires se_comparable_to<Key, ProvidedKey>
size_t _se_hash_table_index_of(const SeHashTable<Key, Value>& table, const ProvidedKey& key)
{
    using Entry = SeHashTable<Key, Value>::Entry;
    SeHashValue hash = se_hash_value_generate(key);
    const size_t initialPosition = _se_hash_table_hash_to_index(hash, table.capacity);
    size_t currentPosition = initialPosition;
    do
    {
        const Entry& data = table.memory[currentPosition];
        if (!data.isOccupied) break;
        const bool isCorrectData = se_hash_value_is_equal(data.hash, hash) && se_compare(data.key, key);
        if (isCorrectData)
        {
            return currentPosition;
        }
        currentPosition = ((currentPosition + 1) % table.capacity);
    } while (currentPosition != initialPosition);
    return table.capacity;
}

template<typename Key, typename Value>
void _se_hash_table_remove(SeHashTable<Key, Value>& table, size_t index)
{
    using Entry = SeHashTable<Key, Value>::Entry;
    Entry* dataToRemove = &table.memory[index];
    se_assert(dataToRemove->isOccupied);
    dataToRemove->isOccupied = false;
    table.size -= 1;
    Entry* dataToReplace = dataToRemove;
    size_t lastReplacedPosition = index;
    for (size_t currentPosition = ((index + 1) % table.capacity);
        currentPosition != index;
        currentPosition = ((currentPosition + 1) % table.capacity))
    {
        Entry* data = &table.memory[currentPosition];
        if (!data->isOccupied) break;
        if (_se_hash_table_hash_to_index(data->hash, table.capacity) <= lastReplacedPosition)
        {
            memcpy(dataToReplace, data, sizeof(Entry));
            data->isOccupied = false;
            dataToReplace = data;
            lastReplacedPosition = currentPosition;
        }
    }
}

template<typename Key, typename Value>
void se_hash_table_construct(SeHashTable<Key, Value>& table, SeAllocatorBindings allocator, size_t capacity = 4)
{
    using Entry = SeHashTable<Key, Value>::Entry;
    table =
    {
        .allocator  = allocator,
        .memory     = nullptr,
        .capacity   = capacity * 2,
        .size       = 0,
    };
    const size_t allocSize = sizeof(Entry) * capacity * 2;
    table.memory = (Entry*)allocator.alloc(allocator.allocator, allocSize, se_default_alignment, se_alloc_tag);
    memset(table.memory, 0, allocSize);
}

template<typename Key, typename Value>
SeHashTable<Key, Value> se_hash_table_create(SeAllocatorBindings allocator, size_t capacity = 4)
{
    using Entry = SeHashTable<Key, Value>::Entry;
    SeHashTable<Key, Value> result =
    {
        .allocator  = allocator,
        .memory     = nullptr,
        .capacity   = capacity * 2,
        .size       = 0,
    };
    const size_t allocSize = sizeof(Entry) * capacity * 2;
    result.memory = (Entry*)allocator.alloc(allocator.allocator, allocSize, se_default_alignment, se_alloc_tag);
    memset(result.memory, 0, allocSize);
    return result;
}

template<typename Key, typename Value>
void se_hash_table_destroy(SeHashTable<Key, Value>& table)
{
    if (table.memory) table.allocator.dealloc(table.allocator.allocator, table.memory, sizeof(SeHashTable<Key, Value>::Entry) * table.capacity);
}

template<typename Key, typename Value>
Value* se_hash_table_set(SeHashTable<Key, Value>& table, const Key& key, const Value& value)
{
    using Entry = SeHashTable<Key, Value>::Entry;
    SeHashValue hash = se_hash_value_generate(key);
    //
    // Expand if needed
    //
    if (table.size >= (table.capacity / 2))
    {
        const size_t oldCapacity = table.capacity;
        const size_t newCapacity = oldCapacity * 2;
        SeHashTable<Key, Value> newTable = se_hash_table_create<Key, Value>(table.allocator, newCapacity);
        for (size_t it = 0; it < oldCapacity; it++)
        {
            Entry& entry = table.memory[it];
            if (entry.isOccupied) se_hash_table_set(newTable, entry.key, entry.value);
        }
        se_hash_table_destroy(table);
        table = newTable;
    }
    //
    // Find position
    //
    const size_t capacity = table.capacity;
    const size_t initialPosition = _se_hash_table_hash_to_index(hash, capacity);
    size_t currentPosition = initialPosition;
    do
    {
        const Entry& entry = table.memory[currentPosition];
        if (!entry.isOccupied)
        {
            break;
        }
        else if (se_hash_value_is_equal(entry.hash, hash) && se_compare(entry.key, key))
        {
            break;
        }
        currentPosition = ((currentPosition + 1) % capacity);
    } while (currentPosition != initialPosition);
    //
    // Fill data
    //
    Entry& entry = table.memory[currentPosition];
    if (!entry.isOccupied)
    {
        entry.hash = hash;
        entry.isOccupied = true;
        memcpy(&entry.key, &key, sizeof(Key));
        memcpy(&entry.value, &value, sizeof(Value));
        table.size += 1;
    }
    else 
    {
        memcpy(&entry.value, &value, sizeof(Value));
    }
    return &entry.value;
}

template<typename Key, typename Value, typename ProvidedKey>
requires se_comparable_to<Key, ProvidedKey>
Value* se_hash_table_get(SeHashTable<Key, Value>& table, const ProvidedKey& key)
{
    const size_t position = _se_hash_table_index_of(table, key);
    return position == table.capacity ? nullptr : &table.memory[position].value;
}

template<typename Key, typename Value, typename ProvidedKey>
requires se_comparable_to<Key, ProvidedKey>
const Value* se_hash_table_get(const SeHashTable<Key, Value>& table, const ProvidedKey& key)
{
    const size_t position = _se_hash_table_index_of(table, key);
    return position == table.capacity ? nullptr : &table.memory[position].value;
}

template<typename Key, typename Value, typename ProvidedKey>
requires se_comparable_to<Key, ProvidedKey>
void se_hash_table_remove(SeHashTable<Key, Value>& table, const ProvidedKey& key)
{
    const size_t position = _se_hash_table_index_of(table, key);
    if (position != table.capacity) _se_hash_table_remove(table, position);
}

template<typename Key, typename Value>
void se_hash_table_reset(SeHashTable<Key, Value>& table)
{
    for (size_t it = 0; it < table.capacity; it++)
        table.memory[it].isOccupied = false;
}

template<typename Key, typename Value>
inline size_t se_hash_table_size(const SeHashTable<Key, Value>& table)
{
    return table.size;
}

template<typename Key, typename Value>
Key se_hash_table_key(const SeHashTable<Key, Value>& table, const Value* value)
{
    using Entry = SeHashTable<Key, Value>::Entry;
    
    const intptr_t from = (intptr_t)table.memory;
    const intptr_t to = (intptr_t)table.memory + table.capacity * sizeof(Entry);
    const intptr_t candidate = (intptr_t)value;
    se_assert(candidate >= from && candidate < to);

    const size_t offsetInEntryStructure = offsetof(Entry, value);
    const size_t index = (candidate - from - offsetInEntryStructure) / sizeof(Entry);
    const Entry& entry = table.memory[index];
    se_assert(entry.isOccupied);

    return entry.key;
}

template<typename Key, typename Value, typename Table>
struct SeHashTableIterator;

template<typename Key, typename Value, typename Table>
struct SeHashTableIteratorValue
{
    const Key&                              key;
    Value&                                  value;
    SeHashTableIterator<Key, Value, Table>* iterator;
};

// @NOTE : typename Value can be const version of Table's Value type
//         and table can be const too
//         e.g. if table is SeHashTable<int, float> itrator can ve either
//         SeHashTableIterator<int, float, SeHashTable<int, float>> if we iterating over const table or
//         SeHashTableIterator<int, const float, const SeHashTable<int, float>> while iterating over non-const table
template<typename Key, typename Value, typename Table>
struct SeHashTableIterator
{
    Table* table;
    size_t index;

    bool                                            operator != (const SeHashTableIterator& other) const;
    SeHashTableIteratorValue<Key, Value, Table>     operator *  ();
    SeHashTableIterator&                            operator ++ ();
};

template<typename Key, typename Value>
SeHashTableIterator<Key, Value, SeHashTable<Key, Value>> begin(SeHashTable<Key, Value>& table)
{
    if (table.size == 0) return { &table, table.capacity };
    size_t it = 0;
    while (it < table.capacity && !table.memory[it].isOccupied) { it++; }
    return { &table, it };
}

template<typename Key, typename Value>
inline SeHashTableIterator<Key, Value, SeHashTable<Key, Value>> end(SeHashTable<Key, Value>& table)
{
    return { &table, table.capacity };
}

template<typename Key, typename Value>
SeHashTableIterator<Key, const Value, const SeHashTable<Key, Value>> begin(const SeHashTable<Key, Value>& table)
{
    if (table.size == 0) return { &table, table.capacity };
    size_t it = 0;
    while (it < table.capacity && !table.memory[it].isOccupied) { it++; }
    return { &table, it };
}

template<typename Key, typename Value>
inline SeHashTableIterator<Key, const Value, const SeHashTable<Key, Value>> end(const SeHashTable<Key, Value>& table)
{
    return { &table, table.capacity };
}

template<typename Key, typename Value, typename Table>
inline bool SeHashTableIterator<Key, Value, Table>::operator != (const SeHashTableIterator<Key, Value, Table>& other) const
{
    return (table != other.table) || (index != other.index);
}

template<typename Key, typename Value, typename Table>
inline SeHashTableIteratorValue<Key, Value, Table> SeHashTableIterator<Key, Value, Table>::operator * ()
{
    return { table->memory[index].key, table->memory[index].value, this };
}

template<typename Key, typename Value, typename Table>
inline SeHashTableIterator<Key, Value, Table>& SeHashTableIterator<Key, Value, Table>::operator ++ ()
{
    do { index++; } while (index < table->capacity && !table->memory[index].isOccupied);
    return *this;
}

template<typename Key, typename Value>
inline const Value& se_iterator_value(const SeHashTableIteratorValue<Key, const Value, const SeHashTable<Key, Value>>& val)
{
    return val.value;
}

template<typename Key, typename Value>
inline Value& se_iterator_value(SeHashTableIteratorValue<Key, Value, SeHashTable<Key, Value>>& val)
{
    return val.value;
}

template<typename Key, typename Value, typename Table>
inline const Key& se_iterator_key(const SeHashTableIteratorValue<Key, Value, Table>& val)
{
    return val.key;
}

template<typename Key, typename Value, typename Table>
inline size_t se_iterator_index(const SeHashTableIteratorValue<Key, Value, Table>& val)
{
    return val.iterator->index;
}

template<typename Key, typename Value>
inline void se_iterator_remove(SeHashTableIteratorValue<Key, Value, SeHashTable<Key, Value>>& val)
{
    se_hash_table_remove(*val.iterator->table, val.key);
    val.iterator->index -= 1;
}

/*
    Thread-safe queue

    Implementation is taken from http://rsdn.org/forum/cpp/3730905.1
*/

template<typename T>
struct SeThreadSafeQueue
{
    struct Cell
    {
        uint64_t sequence;
        T data;
    };
    typedef uint8_t CachelinePadding[64];

    SeAllocatorBindings   allocator;

    CachelinePadding    pad0;
    Cell*               buffer;
    uint64_t            bufferMask;
    CachelinePadding    pad1;
    uint64_t            enqueuePos;
    CachelinePadding    pad2; 
    uint64_t            dequeuePos;
    CachelinePadding    pad3;
};

template<typename T>
void se_thread_safe_queue_construct(SeThreadSafeQueue<T>& queue, SeAllocatorBindings allocator, size_t capacity)
{
    se_assert(se_is_power_of_two(capacity));

    using Cell = SeThreadSafeQueue<T>::Cell;
    Cell* memory = (Cell*)allocator.alloc(allocator.allocator, capacity * sizeof(Cell), se_default_alignment, se_alloc_tag);
    memset(memory, 0, capacity * sizeof(Cell));
    for (size_t it = 0; it < capacity; it++) memory[it].sequence = (uint64_t)it;
    queue =
    {
        .allocator  = allocator,
        .pad0       = { },
        .buffer     = memory,
        .bufferMask = (uint64_t)(capacity - 1),
        .pad1       = { },
        .enqueuePos = 0,
        .pad2       = { },
        .dequeuePos = 0,
        .pad3       = { },
    };
}

template<typename T>
void se_thread_safe_queue_destroy(SeThreadSafeQueue<T>& queue)
{
    using Cell = SeThreadSafeQueue<T>::Cell;
    const SeAllocatorBindings& allocator = queue.allocator;
    if (queue.buffer) allocator.dealloc(allocator.allocator, (void*)queue.buffer, (queue.bufferMask + 1) * sizeof(Cell));
}

template<typename T>
bool se_thread_safe_queue_enqueue(SeThreadSafeQueue<T>& queue, const T& data)
{
    typename SeThreadSafeQueue<T>::Cell* cell;
    // Load current enqueue position
    uint64_t pos = se_platform_atomic_64_bit_load(&queue.enqueuePos, SE_RELAXED);
    while(true)
    {
        // Get current cell
        cell = &queue.buffer[pos & queue.bufferMask];
        // Load sequence of current cell
        uint64_t seq = se_platform_atomic_64_bit_load(&cell->sequence, SE_ACQUIRE);
        intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);
        // Cell is ready for write
        if (diff == 0)
        {
            // Try to increment enqueue position
            if (se_platform_atomic_64_bit_cas(&queue.enqueuePos, &pos, pos + 1, SE_RELAXED))
            {
                // If success, quit from the loop
                break;
            }
            // Else try again
        }
        // Queue is full
        else if (diff < 0)
        {
            return false;
        }
        // Cell was changed by some other thread
        else
        {
            // Load current enqueue position and try again
            pos = se_platform_atomic_64_bit_load(&queue.enqueuePos, SE_RELAXED);
        }
    }
    // Write data
    cell->data = data;
    // Update sequence
    se_platform_atomic_64_bit_store(&cell->sequence, pos + 1, SE_RELEASE);
    return true;
}

template<typename T>
bool se_thread_safe_queue_dequeue(SeThreadSafeQueue<T>& queue, T* data)
{
    typename SeThreadSafeQueue<T>::Cell* cell;
    // Load current dequeue position
    uint64_t pos = se_platform_atomic_64_bit_load(&queue.dequeuePos, SE_RELAXED);
    while(true)
    {
        // Get current cell
        cell = &queue.buffer[pos & queue.bufferMask];
        // Load sequence of current cell
        uint64_t seq = se_platform_atomic_64_bit_load(&cell->sequence, SE_ACQUIRE);
        intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1);
        // Cell is ready for read
        if (diff == 0)
        {
            // Try to increment dequeue position
            if (se_platform_atomic_64_bit_cas(&queue.dequeuePos, &pos, pos + 1, SE_RELAXED))
            {
                // If success, quit from the loop
                break;
            }
            // Else try again
        }
        // Queue is empty
        else if (diff < 0)
        {
            return false;
        }
        // Cell was changed by some other thread
        else
        {
            // Load current dequeue position and try again
            pos = se_platform_atomic_64_bit_load(&queue.dequeuePos, SE_RELAXED);
        }
    }
    // Read data
    *data = cell->data;
    // Update sequence
    se_platform_atomic_64_bit_store(&cell->sequence, pos + queue.bufferMask + 1, SE_RELEASE);
    return true;
}

#endif
