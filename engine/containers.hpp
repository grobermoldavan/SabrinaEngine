#ifndef _SE_CONTAINERS_H_
#define _SE_CONTAINERS_H_

#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <type_traits>

#include "common_includes.hpp"
#include "allocator_bindings.hpp"
#include "engine/subsystems/se_debug_subsystem.hpp"
#include "engine/subsystems/se_platform_subsystem.hpp"
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

namespace dynamic_array
{
    template<typename T>
    void construct(DynamicArray<T>& array, SeAllocatorBindings allocator, size_t capacity = 4)
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
    DynamicArray<T> create(SeAllocatorBindings allocator, size_t capacity = 4)
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
    DynamicArray<T> create_zeroed(SeAllocatorBindings allocator, size_t capacity = 4)
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

    template<typename Size = size_t, typename T>
    inline Size size(const DynamicArray<T>& array)
    {
        return (Size)array.size;
    }

    template<typename T>
    inline size_t capacity(const DynamicArray<T>& array)
    {
        return array.capacity;
    }

    template<typename T>
    inline SeAllocatorBindings& allocator(const DynamicArray<T>& array)
    {
        return array.allocator;
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
        pushed = nullptr;
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
}

template <typename T, typename Array>
struct DynamicArrayIterator;

template <typename T, typename Array>
struct DynamicArrayIteratorValue
{
    T& value;
    DynamicArrayIterator<T, Array>* iterator;
};

template <typename T, typename Array>
struct DynamicArrayIterator
{
    Array* arr;
    size_t index;

    bool                                operator != (const DynamicArrayIterator& other) const;
    DynamicArrayIteratorValue<T, Array> operator *  ();
    DynamicArrayIterator&               operator ++ ();
};

template<typename T>
DynamicArrayIterator<T, DynamicArray<T>> begin(DynamicArray<T>& arr)
{
    return { &arr, 0 };
}

template<typename T>
DynamicArrayIterator<T, DynamicArray<T>> end(DynamicArray<T>& arr)
{
    return { &arr, arr.size };
}

template<typename T>
DynamicArrayIterator<const T, const DynamicArray<T>> begin(const DynamicArray<T>& arr)
{
    return { &arr, 0 };
}

template<typename T>
DynamicArrayIterator<const T, const DynamicArray<T>> end(const DynamicArray<T>& arr)
{
    return { &arr, arr.size };
}

template<typename T, typename Array>
bool DynamicArrayIterator<T, Array>::operator != (const DynamicArrayIterator<T, Array>& other) const
{
    return (arr != other.arr) || (index != other.index);
}

template<typename T, typename Array>
DynamicArrayIteratorValue<T, Array> DynamicArrayIterator<T, Array>::operator * ()
{
    return { arr->memory[index], this };
}

template<typename T, typename Array>
DynamicArrayIterator<T, Array>& DynamicArrayIterator<T, Array>::operator ++ ()
{
    index += 1;
    return *this;
}

namespace iter
{
    template<typename T>
    T& value(DynamicArrayIteratorValue<T, DynamicArray<T>>& value)
    {
        return value.value;
    }

    template<typename T>
    const T& value(const DynamicArrayIteratorValue<const T, const DynamicArray<T>>& value)
    {
        return value.value;
    }

    template<typename ValueT, typename Array>
    size_t index(const DynamicArrayIteratorValue<ValueT, Array>& value)
    {
        return value.iterator->index;
    }

    template<typename T>
    void remove(DynamicArrayIteratorValue<T, DynamicArray<T>>& value)
    {
        dynamic_array::remove(*value.iterator->arr, value.iterator->index);
        value.iterator->index -= 1;
    }
}

/*
    Expandable virtual memory.

    Basically a stack that uses OS virtual memory system and commits memory pages on demand.
*/

template<typename T>
struct ExpandableVirtualMemory
{
    const SePlatformSubsystemInterface* platform;
    T* base;
    size_t reservedRaw;
    size_t commitedRaw;
    size_t usedRaw;
};

namespace expandable_virtual_memory
{
    template<typename T>
    ExpandableVirtualMemory<T> create(const SePlatformSubsystemInterface* platform, size_t size)
    {
        return
        {
            .platform       = platform,
            .base           = (T*)platform->mem_reserve(size),
            .reservedRaw    = size,
            .commitedRaw    = 0,
            .usedRaw        = 0,
        };
    }

    template<typename T>
    void destroy(ExpandableVirtualMemory<T>& memory)
    {
        memory.platform->mem_release(memory.base, memory.reservedRaw);
    }

    template<typename T>
    void add(ExpandableVirtualMemory<T>& memory, size_t numberOfElementsToAdd)
    {
        memory.usedRaw += numberOfElementsToAdd * sizeof(T);
        se_assert(memory.usedRaw <= memory.reservedRaw);
        if (memory.usedRaw > memory.commitedRaw)
        {
            const size_t memPageSize = memory.platform->get_mem_page_size();
            const size_t requredToCommit = memory.usedRaw - memory.commitedRaw;
            const size_t numPagesToCommit = 1 + ((requredToCommit - 1) / memPageSize);
            const size_t memoryToCommit = numPagesToCommit * memPageSize;
            memory.platform->mem_commit(((uint8_t*)memory.base) + memory.commitedRaw, memoryToCommit);
            memory.commitedRaw += memoryToCommit;
        }
    }

    template<typename T>
    inline T* raw(const ExpandableVirtualMemory<T>& memory)
    {
        return memory.base;
    }

    template<typename T>
    inline size_t used(const ExpandableVirtualMemory<T>& memory)
    {
        return memory.usedRaw;
    }
}

/*
    Object pool.

    Container for an objects of the same size.
*/

template<typename T>
struct ObjectPoolEntry
{
    T value;
    uint32_t generation;
};

template<typename T>
struct ObjectPool
{
    ExpandableVirtualMemory<uint8_t> ledger;
    ExpandableVirtualMemory<ObjectPoolEntry<T>> objectMemory;
};

template<typename T>
struct ObjectPoolEntryRef
{
    ObjectPool<T>* pool;
    uint32_t index;
    uint32_t generation;

    T* operator -> ();
    const T* operator -> () const;
    T* operator * ();
    const T* operator * () const;
};

namespace object_pool
{
    template<typename T>
    inline void construct(ObjectPool<T>& pool, const SePlatformSubsystemInterface* platform)
    {
        pool =
        {
            .ledger             = expandable_virtual_memory::create<uint8_t>(platform, se_gigabytes(16)),
            .objectMemory       = expandable_virtual_memory::create<ObjectPoolEntry<T>>(platform, se_gigabytes(16)),
        };
    }

    template<typename T>
    inline ObjectPool<T> create(const SePlatformSubsystemInterface* platform)
    {
        return
        {
            .ledger             = expandable_virtual_memory::create<uint8_t>(platform, se_gigabytes(16)),
            .objectMemory       = expandable_virtual_memory::create<ObjectPoolEntry<T>>(platform, se_gigabytes(16)),
        };
    }

    template<typename T>
    inline void destroy(ObjectPool<T>& pool)
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
        const size_t ledgerUsed = expandable_virtual_memory::used(pool.ledger); 
        for (size_t it = 0; it < ledgerUsed; it++)
        {
            uint8_t* byte = expandable_virtual_memory::raw(pool.ledger) + it;
            if (*byte == 255) continue;
            for (uint8_t byteIt = 0; byteIt < 8; byteIt++)
            {
                if ((*byte & (1 << byteIt)) == 0)
                {
                    const size_t indexOffset = it * 8 + byteIt;
                    *byte |= (1 << byteIt);
                    ObjectPoolEntry<T>* entry = expandable_virtual_memory::raw(pool.objectMemory) + indexOffset;
                    entry->generation += 1;
                    return &entry->value;
                }
            }
            se_assert_msg(false, "Invalid code path");
        }
        const size_t indexOffset = ledgerUsed * 8;
        expandable_virtual_memory::add(pool.ledger, 1);
        expandable_virtual_memory::add(pool.objectMemory, 8);
        uint8_t* byte = expandable_virtual_memory::raw(pool.ledger) + ledgerUsed;
        *byte |= 1;
        ObjectPoolEntry<T>* entry = expandable_virtual_memory::raw(pool.objectMemory) + indexOffset;
        entry->generation += 1;
        return &entry->value;
    }

    template<typename T>
    size_t index_of(const ObjectPool<T>& pool, const T* object)
    {
        const intptr_t base = (intptr_t)expandable_virtual_memory::raw(pool.objectMemory);
        const intptr_t end = base + (intptr_t)expandable_virtual_memory::used(pool.objectMemory);
        const intptr_t candidate = (intptr_t)object;
        se_assert_msg(candidate >= base && candidate < end, "Can't get index of an object in pool : pointer is not in the pool range");
        const size_t memoryOffset = candidate - base;
        se_assert_msg((memoryOffset % sizeof(ObjectPoolEntry<T>)) == 0, "Can't get index of an object in pool : wrong pointer provided");
        return memoryOffset / sizeof(ObjectPoolEntry<T>);
    }

    template<typename T>
    bool is_taken(const ObjectPool<T>& pool, size_t index)
    {
        se_assert_msg((expandable_virtual_memory::used(pool.ledger) * 8) > index, "Can't get check if object is taken from pool : index is out of range");
        const uint8_t* byte = ((uint8_t*)expandable_virtual_memory::raw(pool.ledger)) + (index / 8);
        return *byte & (1 << (index % 8));
    }

    template<typename T>
    T* access(ObjectPool<T>& pool, size_t index)
    {
        se_assert(object_pool::is_taken(pool, index));
        ObjectPoolEntry<T>* entry = expandable_virtual_memory::raw(pool.objectMemory) + index;
        return &entry->value;
    }

    template<typename T>
    const T* access(const ObjectPool<T>& pool, size_t index)
    {
        se_assert(object_pool::is_taken(pool, index));
        const ObjectPoolEntry<T>* entry = expandable_virtual_memory::raw(pool.objectMemory) + index;
        return &entry->value;
    }

    template<typename T>
    void release(ObjectPool<T>& pool, const T* object)
    {
        const size_t index = object_pool::index_of(pool, object);
        se_assert(object_pool::is_taken(pool, index));
        uint8_t* ledgerByte = expandable_virtual_memory::raw(pool.ledger) + (index / 8);
        se_assert_msg(*ledgerByte & (1 << (index % 8)), "Can't return object to the pool : object is already returned");
        *ledgerByte &= ~(1 << (index % 8));
    }

    template<typename T>
    ObjectPoolEntryRef<T> to_ref(ObjectPool<T>& pool, const T* object)
    {
        const size_t index = object_pool::index_of(pool, object);
        const ObjectPoolEntry<T>* entry = expandable_virtual_memory::raw(pool.objectMemory) + index;
        return
        {
            &pool,
            (uint32_t)index,
            entry->generation,
        };
    }

    template<typename T>
    ObjectPoolEntryRef<T> to_ref(ObjectPool<T>& pool, size_t index)
    {
        const ObjectPoolEntry<T>* entry = expandable_virtual_memory::raw(pool.objectMemory) + index;
        return
        {
            &pool,
            (uint32_t)index,
            entry->generation,
        };
    }

    template<typename T>
    T* from_ref(ObjectPool<T>& pool, ObjectPoolEntryRef<T> ref)
    {
        ObjectPoolEntry<T>* entry = expandable_virtual_memory::raw(pool.objectMemory) + ref.index;
        return (entry->generation != ref.generation) || !object_pool::is_taken(pool, ref.index) ? nullptr : &entry->value;
    }

    template<typename T>
    const T* from_ref(const ObjectPool<T>& pool, ObjectPoolEntryRef<T> ref)
    {
        const ObjectPoolEntry<T>* entry = expandable_virtual_memory::raw(pool.objectMemory) + ref.index;
        return (entry->generation != ref.generation) || !object_pool::is_taken(pool, ref.index) ? nullptr : &entry->value;
    }
}

template<typename T>
T* ObjectPoolEntryRef<T>::operator -> ()
{
    return object_pool::from_ref(*pool, *this);
}

template<typename T>
const T* ObjectPoolEntryRef<T>::operator -> () const
{
    return object_pool::from_ref(*pool, *this);;
}

template<typename T>
T* ObjectPoolEntryRef<T>::operator * ()
{
    return object_pool::from_ref(*pool, *this);
}

template<typename T>
const T* ObjectPoolEntryRef<T>::operator * () const
{
    return object_pool::from_ref(*pool, *this);;
}

template<typename T>
struct ObjectPoolIteratorValue
{
    T& val;
};

template<typename Pool, typename T>
struct ObjectPoolIterator
{
    Pool& pool;
    size_t index;

    bool                        operator != (const ObjectPoolIterator& other) const;
    ObjectPoolIteratorValue<T>  operator *  ();
    ObjectPoolIterator&         operator ++ ();
};

template<typename T>
ObjectPoolIterator<ObjectPool<T>, T> begin(ObjectPool<T>& pool)
{
    ObjectPoolIterator<ObjectPool<T>, T> result = { pool, SIZE_MAX };
    result.operator ++();
    return result;
}

template<typename T>
ObjectPoolIterator<ObjectPool<T>, T> end(ObjectPool<T>& pool)
{
    return { pool, expandable_virtual_memory::used(pool.ledger) * 8 };
}

template<typename T>
ObjectPoolIterator<const ObjectPool<T>, const T> begin(const ObjectPool<T>& pool)
{
    ObjectPoolIterator<const ObjectPool<T>, const T> result = { pool, SIZE_MAX };
    result.operator ++();
    return result;
}

template<typename T>
ObjectPoolIterator<const ObjectPool<T>, const T> end(const ObjectPool<T>& pool)
{
    return { pool, expandable_virtual_memory::used(pool.ledger) * 8 };
}

template<typename Pool, typename T>
bool ObjectPoolIterator<Pool, T>::operator != (const ObjectPoolIterator<Pool, T>& other) const
{
    return (&pool != &other.pool) || (index != other.index);
}

template<typename Pool, typename T>
ObjectPoolIteratorValue<T> ObjectPoolIterator<Pool, T>::operator * ()
{
    return { *object_pool::access<T>(pool, index) };
}

template<typename Pool, typename T>
ObjectPoolIterator<Pool, T>& ObjectPoolIterator<Pool, T>::operator ++ ()
{
    const size_t used = expandable_virtual_memory::used(pool.ledger);
    const size_t ledgerSize = used * 8;
    const uint8_t* ledger = expandable_virtual_memory::raw(pool.ledger);
    
    for (index += 1; index < ledgerSize; index += 1)
    {
        const uint8_t ledgerByte = *(ledger + (index / 8));
        const bool isOccupied = ledgerByte & (1 << (index % 8));
        if (isOccupied) break;
    }

    return *this;
}

namespace iter
{
    template<typename T>
    T& value(ObjectPoolIteratorValue<T>& val)
    {
        return val.val;
    }

    template<typename T>
    const T& value(ObjectPoolIteratorValue<const T>& val)
    {
        return val.val;
    }
}

/*
    Hash table.

    https://en.wikipedia.org/wiki/Linear_probing
*/

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
        HashValueBuilder create()
        {
            HashValueBuilder result = { };
            MeowBegin(&result, MeowDefaultSeed);
            return result;
        }

        void absorb_raw(HashValueBuilder& builder, const HashValueInput& input)
        {
            MeowAbsorb(&builder, input.size, input.data);
        }

        template<typename T>
        void absorb(HashValueBuilder& builder, const T& input)
        {
            hash_value::builder::absorb_raw(builder, { (void*)&input, sizeof(T) });
        }

        HashValue end(HashValueBuilder& builder)
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

    inline bool is_equal(const HashValue& first, const HashValue& second)
    {
        return MeowHashesAreEqual(first, second);
    }
}

template<typename Key, typename Value>
struct HashTable
{
    using KeyType = Key;
    using ValueType = Key;
    struct Entry
    {
        Key         key;
        Value       value;
        HashValue   hash;
        bool        isOccupied;
    };

    SeAllocatorBindings allocator;
    Entry*              memory;
    size_t              capacity;
    size_t              size;
};

namespace hash_table
{
    namespace impl
    {
        inline size_t hash_to_index(HashValue hash, size_t capacity)
        {
            return (MeowU64From(hash, 0) % (capacity / 2)) + (MeowU64From(hash, 1) % (capacity / 2 + 1));
        }

        template<typename Key, typename Value>
        size_t index_of(HashTable<Key, Value>& table, const Key& key)
        {
            using Entry = HashTable<Key, Value>::Entry;
            HashValue hash = hash_value::generate(key);
            const size_t initialPosition = impl::hash_to_index(hash, table.capacity);
            size_t currentPosition = initialPosition;
            do
            {
                Entry& data = table.memory[currentPosition];
                if (!data.isOccupied) break;
                const bool isCorrectData = hash_value::is_equal(data.hash, hash) && __se_memcmp(&data.key, &key, sizeof(Key));
                if (isCorrectData)
                {
                    return currentPosition;
                }
                currentPosition = ((currentPosition + 1) % table.capacity);
            } while (currentPosition != initialPosition);
            return table.capacity;
        }

        template<typename Key, typename Value>
        void remove(HashTable<Key, Value>& table, size_t index)
        {
            using Entry = HashTable<Key, Value>::Entry;
            Entry* dataToRemove = &table.memory[index];
            se_assert(dataToRemove->isOccupied);
            dataToRemove->isOccupied = false;
            table.size -= 1;
            Entry* dataToReplace = dataToRemove;
            for (size_t currentPosition = ((index + 1) % table.capacity);
                currentPosition != index;
                currentPosition = ((currentPosition + 1) % table.capacity))
            {
                Entry* data = &table.memory[currentPosition];
                if (!data->isOccupied) break;
                if (impl::hash_to_index(data->hash, table.capacity) <= impl::hash_to_index(dataToReplace->hash, table.capacity))
                {
                    __se_memcpy(dataToReplace, data, sizeof(Entry));
                    data->isOccupied = false;
                    dataToReplace = data;
                }
            }
        }
    }

    template<typename Key, typename Value>
    void construct(HashTable<Key, Value>& table, SeAllocatorBindings allocator, size_t capacity = 4)
    {
        using Entry = HashTable<Key, Value>::Entry;
        table =
        {
            .allocator  = allocator,
            .memory     = nullptr,
            .capacity   = capacity,
            .size       = 0,
        };
        const size_t allocSize = sizeof(Entry) * capacity;
        table.memory = (Entry*)allocator.alloc(allocator.allocator, allocSize, se_default_alignment, se_alloc_tag);
        __se_memset(table.memory, 0, allocSize);
    }

    template<typename Key, typename Value>
    HashTable<Key, Value> create(SeAllocatorBindings allocator, size_t capacity = 4)
    {
        using Entry = HashTable<Key, Value>::Entry;
        HashTable<Key, Value> result =
        {
            .allocator  = allocator,
            .memory     = nullptr,
            .capacity   = capacity,
            .size       = 0,
        };
        const size_t allocSize = sizeof(Entry) * capacity;
        result.memory = (Entry*)allocator.alloc(allocator.allocator, allocSize, se_default_alignment, se_alloc_tag);
        __se_memset(result.memory, 0, allocSize);
        return result;
    }

    template<typename Key, typename Value>
    void destroy(HashTable<Key, Value>& table)
    {
        table.allocator.dealloc(table.allocator.allocator, table.memory, sizeof(HashTable<Key, Value>::Entry) * table.capacity);
    }

    template<typename Key, typename Value>
    Value* set(HashTable<Key, Value>& table, const Key& key, const Value& value)
    {
        using Entry = HashTable<Key, Value>::Entry;
        HashValue hash = hash_value::generate(key);
        //
        // Expand if needed
        //
        if (table.size >= (table.capacity / 2))
        {
            const size_t oldCapacity = table.capacity;
            const size_t newCapacity = oldCapacity * 2;
            HashTable<Key, Value> newTable = hash_table::create<Key, Value>(table.allocator, newCapacity);
            for (size_t it = 0; it < oldCapacity; it++)
            {
                Entry& entry = table.memory[it];
                if (entry.isOccupied) hash_table::set(newTable, entry.key, entry.value);
            }
            hash_table::destroy(table);
            table = newTable;
        }
        //
        // Find position
        //
        const size_t capacity = table.capacity;
        const size_t initialPosition = impl::hash_to_index(hash, capacity);
        size_t currentPosition = initialPosition;
        do
        {
            const Entry& entry = table.memory[currentPosition];
            if (!entry.isOccupied)
            {
                break;
            }
            else if (hash_value::is_equal(entry.hash, hash) && __se_memcmp(&entry.key, &key, sizeof(Key)))
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
            __se_memcpy(&entry.key, &key, sizeof(Key));
            __se_memcpy(&entry.value, &value, sizeof(Value));
            table.size += 1;
        }
        else 
        {
            __se_memcpy(&entry.value, &value, sizeof(Value));
        }
        return &entry.value;
    }

    template<typename Key, typename Value>
    Value* get(HashTable<Key, Value>& table, const Key& key)
    {
        const size_t position = impl::index_of(table, key);
        return position == table.capacity ? nullptr : &table.memory[position].value;
    }

    template<typename Key, typename Value>
    void remove(HashTable<Key, Value>& table, const Key& key)
    {
        const size_t position = impl::index_of(table, key);
        if (position != table.capacity) impl::remove(table, position);
    }

    template<typename Key, typename Value>
    void reset(HashTable<Key, Value>& table)
    {
        for (size_t it = 0; it < table.capacity; it++)
            table.memory[it].isOccupied = false;
    }

    template<typename Key, typename Value>
    size_t size(const HashTable<Key, Value>& table)
    {
        return table.size;
    }
}

template<typename Key, typename Value, typename Table>
struct HashTableIterator;

template<typename Key, typename Value, typename Table>
struct HashTableIteratorValue
{
    const Key&                              key;
    Value&                                  value;
    HashTableIterator<Key, Value, Table>*   iterator;
};

// @NOTE : typename Value can be const version of Table's Value type
//         and table can be const too
//         e.g. if table is HashTable<int, float> itrator can ve either
//         HashTableIterator<int, float, HashTable<int, float>> if we iterating over const table or
//         HashTableIterator<int, const float, const HashTable<int, float>> while iterating over non-const table
template<typename Key, typename Value, typename Table>
struct HashTableIterator
{
    Table* table;
    size_t index;

    bool                                        operator != (const HashTableIterator& other) const;
    HashTableIteratorValue<Key, Value, Table>   operator *  ();
    HashTableIterator&                          operator ++ ();
};

template<typename Key, typename Value>
HashTableIterator<Key, Value, HashTable<Key, Value>> begin(HashTable<Key, Value>& table)
{
    if (table.size == 0) return { &table, table.capacity };
    size_t it = 0;
    while (it < table.capacity && !table.memory[it].isOccupied) { it++; }
    return { &table, it };
}

template<typename Key, typename Value>
HashTableIterator<Key, Value, HashTable<Key, Value>> end(HashTable<Key, Value>& table)
{
    return { &table, table.capacity };
}

template<typename Key, typename Value>
HashTableIterator<Key, const Value, const HashTable<Key, Value>> begin(const HashTable<Key, Value>& table)
{
    if (table.size == 0) return { &table, table.capacity };
    size_t it = 0;
    while (it < table.capacity && !table.memory[it].isOccupied) { it++; }
    return { &table, it };
}

template<typename Key, typename Value>
HashTableIterator<Key, const Value, const HashTable<Key, Value>> end(const HashTable<Key, Value>& table)
{
    return { &table, table.capacity };
}

template<typename Key, typename Value, typename Table>
bool HashTableIterator<Key, Value, Table>::operator != (const HashTableIterator<Key, Value, Table>& other) const
{
    return (table != other.table) || (index != other.index);
}

template<typename Key, typename Value, typename Table>
HashTableIteratorValue<Key, Value, Table> HashTableIterator<Key, Value, Table>::operator * ()
{
    return { table->memory[index].key, table->memory[index].value, this };
}

template<typename Key, typename Value, typename Table>
HashTableIterator<Key, Value, Table>& HashTableIterator<Key, Value, Table>::operator ++ ()
{
    do { index++; } while (index < table->capacity && !table->memory[index].isOccupied);
    return *this;
}

namespace iter
{
    template<typename Key, typename Value>
    const Value& value(const HashTableIteratorValue<Key, const Value, const HashTable<Key, Value>>& val)
    {
        return val.value;
    }

    template<typename Key, typename Value>
    Value& value(HashTableIteratorValue<Key, Value, HashTable<Key, Value>>& val)
    {
        return val.value;
    }

    template<typename Key, typename Value, typename Table>
    const Key& key(const HashTableIteratorValue<Key, Value, Table>& val)
    {
        return val.key;
    }

    template<typename Key, typename Value, typename Table>
    size_t index(const HashTableIteratorValue<Key, Value, Table>& val)
    {
        return val.iterator->index;
    }

    template<typename Key, typename Value>
    void remove(HashTableIteratorValue<Key, Value, HashTable<Key, Value>>& val)
    {
        hash_table::remove(*val.iterator->table, val.key);
        val.iterator->index -= 1;
    }
}

/*
    Thread-safe queue

    Implementation is taken from http://rsdn.org/forum/cpp/3730905.1
*/

template<typename T>
struct ThreadSafeQueue
{
    struct Cell
    {
        uint64_t sequence;
        T data;
    };
    typedef uint8_t CachelinePadding[64];

    SeAllocatorBindings allocator;
    const SePlatformSubsystemInterface* platform;

    CachelinePadding    pad0;
    Cell*               buffer;
    uint64_t            bufferMask;
    CachelinePadding    pad1;
    uint64_t            enqueuePos;
    CachelinePadding    pad2; 
    uint64_t            dequeuePos;
    CachelinePadding    pad3;
};

namespace thread_safe_queue
{
    template<typename T>
    void construct(ThreadSafeQueue<T>& queue, SeAllocatorBindings allocator, const SePlatformSubsystemInterface* platform, size_t capacity)
    {
        se_assert(utils::is_power_of_two(capacity));

        using Cell = ThreadSafeQueue<T>::Cell;
        Cell* memory = (Cell*)allocator.alloc(allocator.allocator, capacity * sizeof(Cell), se_default_alignment, se_alloc_tag);
        memset(memory, 0, capacity * sizeof(Cell));
        for (size_t it = 0; it < capacity; it++) memory[it].sequence = (uint64_t)it;
        queue =
        {
            .allocator  = allocator,
            .platform   = platform,
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
    void destroy(ThreadSafeQueue<T>& queue)
    {
        using Cell = ThreadSafeQueue<T>::Cell;
        SeAllocatorBindings& allocator = queue.allocator;
        allocator.dealloc(allocator.allocator, (void*)queue.buffer, (queue.bufferMask + 1) * sizeof(Cell));
    }

    template<typename T>
    bool enqueue(ThreadSafeQueue<T>& queue, const T& data)
    {
        typename ThreadSafeQueue<T>::Cell* cell;
        // Load current enqueue position
        uint64_t pos = queue.platform->atomic_64_bit_load(&queue.enqueuePos, SE_RELAXED);
        while(true)
        {
            // Get current cell
            cell = &queue.buffer[pos & queue.bufferMask];
            // Load sequence of current cell
            uint64_t seq = queue.platform->atomic_64_bit_load(&cell->sequence, SE_ACQUIRE);
            intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);
            // Cell is ready for write
            if (diff == 0)
            {
                // Try to increment enqueue position
                if (queue.platform->atomic_64_bit_cas(&queue.enqueuePos, &pos, pos + 1, SE_RELAXED))
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
                pos = queue.platform->atomic_64_bit_load(&queue.enqueuePos, SE_RELAXED);
            }
        }
        // Write data
        cell->data = data;
        // Update sequence
        queue.platform->atomic_64_bit_store(&cell->sequence, pos + 1, SE_RELEASE);
        return true;
    }

    template<typename T>
    bool dequeue(ThreadSafeQueue<T>& queue, T* data)
    {
        typename ThreadSafeQueue<T>::Cell* cell;
        // Load current dequeue position
        uint64_t pos = queue.platform->atomic_64_bit_load(&queue.dequeuePos, SE_RELAXED);
        while(true)
        {
            // Get current cell
            cell = &queue.buffer[pos & queue.bufferMask];
            // Load sequence of current cell
            uint64_t seq = queue.platform->atomic_64_bit_load(&cell->sequence, SE_ACQUIRE);
            intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1);
            // Cell is ready for read
            if (diff == 0)
            {
                // Try to increment dequeue position
                if (queue.platform->atomic_64_bit_cas(&queue.dequeuePos, &pos, pos + 1, SE_RELAXED))
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
                pos = queue.platform->atomic_64_bit_load(&queue.dequeuePos, SE_RELAXED);
            }
        }
        // Read data
        *data = cell->data;
        // Update sequence
        queue.platform->atomic_64_bit_store(&cell->sequence, pos + queue.bufferMask + 1, SE_RELEASE);
        return true;
    }
}

#endif
