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

using HashValue = meow_u128;

namespace hash_value
{
    namespace impl
    {
        struct MultiHashInput
        {
            void* data;
            size_t size;
        };

        template<size_t index, typename Value>
        void fill_hash_inputs(MultiHashInput* inputs, const Value& value)
        {
            inputs[index] = MultiHashInput{ .data = (void*)&value, .size = sizeof(Value) };
        }

        template<size_t index, typename Value, typename ... Other>
        void fill_hash_inputs(MultiHashInput* inputs, const Value& value, const Other& ... other)
        {
            inputs[index] = MultiHashInput{ .data = (void*)&value, .size = sizeof(Value) };
            fill_hash_inputs<index + 1, Other...>(inputs, other...);
        }
    }

    template<typename Value>
    HashValue generate(const Value& value)
    {
        return MeowHash(MeowDefaultSeed, sizeof(Value), (void*)&value);
    }

    template<typename Value, typename ... Other>
    HashValue generate(const Value& value, const Other& ... other)
    {
        constexpr size_t numInputs = 1 + sizeof...(Other);
        impl::MultiHashInput inputs[numInputs];
        impl::fill_hash_inputs<0>(inputs, value, other...);
        meow_state state = { };
        MeowBegin(&state, MeowDefaultSeed);
        for (size_t it = 0; it < numInputs; it++)
        {
            MeowAbsorb(&state, inputs[it].size, inputs[it].data);
        }
        return MeowEnd(&state, nullptr);
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
    struct Iterator
    {
        struct IteratorValue
        {
            const Key&  key;
            Value&      value;
            Iterator*   iterator;
        };
        HashTable* table;
        size_t index;

        bool            operator != (const Iterator& other);
        IteratorValue   operator *  ();
        Iterator&       operator ++ ();
    };

    SeAllocatorBindings allocator;
    Entry*              memory;
    size_t              capacity;
    size_t              size;
};

template<typename Key, typename Value>
typename HashTable<Key, Value>::Iterator begin(HashTable<Key, Value>& table)
{
    if (table.size == 0) return { &table, table.capacity };
    size_t it = 0;
    while (it < table.capacity && !table.memory[it].isOccupied) { it++; }
    return { &table, it };
}

template<typename Key, typename Value>
typename HashTable<Key, Value>::Iterator end(HashTable<Key, Value>& table)
{
    return { &table, table.capacity };
}

template<typename Key, typename Value>
bool HashTable<Key, Value>::Iterator::operator != (const typename HashTable<Key, Value>::Iterator& other)
{
    return (table != other.table) || (index != other.index);
}

template<typename Key, typename Value>
typename HashTable<Key, Value>::Iterator::IteratorValue HashTable<Key, Value>::Iterator::operator * ()
{
    return{ table->memory[index].key, table->memory[index].value, this };
}

template<typename Key, typename Value>
typename HashTable<Key, Value>::Iterator& HashTable<Key, Value>::Iterator::operator ++ ()
{
    do { index++; } while (index < table->capacity && !table->memory[index].isOccupied);
    return *this;
}

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
            Entry& dataToRemove = table.memory[index];
            dataToRemove.isOccupied = false;
            table.size -= 1;
            Entry& dataToReplace = dataToRemove;
            for (size_t currentPosition = ((index + 1) % table.capacity); currentPosition != index; currentPosition = ((currentPosition + 1) % table.capacity))
            {
                Entry& data = table.memory[currentPosition];
                if (!data.isOccupied) break;
                if (hash_to_index(data.hash, table.capacity) <= hash_to_index(dataToReplace.hash, table.capacity))
                {
                    __se_memcpy(&dataToReplace, &data, sizeof(Entry));
                    data.isOccupied = false;
                    dataToReplace = data;
                }
            }
        }
    }

    template<typename Key, typename Value>
    void construct(HashTable<Key, Value>& table, SeAllocatorBindings& allocator, size_t capacity = 4)
    {
        table =
        {
            .allocator  = allocator,
            .memory     = nullptr,
            .capacity   = capacity,
            .size       = 0,
        };
        const size_t allocSize = sizeof(HashTable<Key, Value>::Entry) * capacity;
        table.memory = (typename HashTable<Key, Value>::Entry*)allocator.alloc(allocator.allocator, allocSize, se_default_alignment, se_alloc_tag);
        __se_memset(table.memory, 0, allocSize);
    }

    template<typename Key, typename Value>
    HashTable<Key, Value> create(SeAllocatorBindings& allocator, size_t capacity = 4)
    {
        HashTable<Key, Value> result =
        {
            .allocator  = allocator,
            .memory     = nullptr,
            .capacity   = capacity,
            .size       = 0,
        };
        const size_t allocSize = sizeof(HashTable<Key, Value>::Entry) * capacity;
        result.memory = (typename HashTable<Key, Value>::Entry*)allocator.alloc(allocator.allocator, allocSize, se_default_alignment, se_alloc_tag);
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
            Entry& entry = table.memory[currentPosition];
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
    void remove(typename HashTable<Key, Value>::Iterator::IteratorValue val)
    {
        remove(*val.iterator->table, val.key);
        val.iterator->index -= 1;
    }
}

#endif
