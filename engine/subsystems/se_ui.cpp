
#include "se_ui.hpp"
#include "engine/engine.hpp"

#define STBTT_malloc(size, userData) ((void)(userData), ui_stbtt_malloc(size))
void* ui_stbtt_malloc(size_t size)
{
    AllocatorBindings allocator = allocators::frame();
    return allocator.alloc(allocator.allocator, size, se_default_alignment, se_alloc_tag);
}

#define STBTT_free(ptr, userData) ((void)(userData), ui_stbtt_free(ptr))
void ui_stbtt_free(void* ptr)
{
    // Nothing
}

#define STBTT_assert se_assert
#define STB_TRUETYPE_IMPLEMENTATION
#include "engine/libs/stb/stb_truetype.h"

// =======================================================================
//
// Render atlas
//
// =======================================================================

struct RenderAtlasRectSize
{
    size_t width;
    size_t height;
};

struct RenderAtlasRect
{
    size_t p1x;
    size_t p1y;
    size_t p2x;
    size_t p2y;
};

struct RenderAtlasRectNormalized
{
    float p1x;
    float p1y;
    float p2x;
    float p2y;
};

template<typename Pixel>
struct RenderAtlas
{
    Pixel*                                  bitmap;
    size_t                                  width;
    size_t                                  height;
    DynamicArray<RenderAtlasRect>           rects;
    DynamicArray<RenderAtlasRectNormalized> uvRects;
};

struct RenderAtlasLayoutBuilder
{
    static constexpr size_t STEP = 256;
    static constexpr size_t BORDER = 1;
    struct Node
    {
        RenderAtlasRectSize size;
        RenderAtlasRect     rect;
    };
    DynamicArray<Node>  occupiedNodes;
    DynamicArray<Node>  freeNodes;
    size_t              width;
    size_t              height;
};

template<typename PixelDst, typename PixelSrc>
using RenderAtlasPutFunciton = void (*)(const PixelSrc* src, PixelDst* dst);

namespace render_atlas
{
    namespace layout_builder
    {
        RenderAtlasLayoutBuilder begin()
        {
            RenderAtlasLayoutBuilder builder
            {
                .occupiedNodes  = { },
                .freeNodes      = { },
                .width          = RenderAtlasLayoutBuilder::STEP,
                .height         = RenderAtlasLayoutBuilder::STEP,
            };
            dynamic_array::construct(builder.occupiedNodes, allocators::frame(), 64);
            dynamic_array::construct(builder.freeNodes, allocators::frame(), 64);
            dynamic_array::push(builder.freeNodes,
            {
                .size =
                {
                    RenderAtlasLayoutBuilder::STEP,
                    RenderAtlasLayoutBuilder::STEP
                },
                .rect =
                {
                    0,
                    0,
                    RenderAtlasLayoutBuilder::STEP,
                    RenderAtlasLayoutBuilder::STEP,
                },
            });
            return builder;
        }

        const RenderAtlasLayoutBuilder::Node* push(RenderAtlasLayoutBuilder& builder, RenderAtlasRectSize initialRequirement)
        {
            se_assert(initialRequirement.width);
            se_assert(initialRequirement.height);
            const RenderAtlasRectSize requirement
            {
                .width = initialRequirement.width + RenderAtlasLayoutBuilder::BORDER * 2,
                .height = initialRequirement.height + RenderAtlasLayoutBuilder::BORDER * 2,
            };
            const RenderAtlasLayoutBuilder::Node* bestNode = nullptr;
            while (!bestNode)
            {
                size_t bestDiff = SIZE_MAX;
                for (auto it : builder.freeNodes)
                {
                    const RenderAtlasLayoutBuilder::Node& candidate = iter::value(it);
                    if(requirement.width > candidate.size.width || requirement.height > candidate.size.height) continue;
                    const size_t diff = (candidate.size.width - requirement.width) + (candidate.size.height - requirement.height);
                    if (diff < bestDiff)
                    {
                        bestDiff = diff;
                        bestNode = &candidate;
                    }
                }
                if (!bestNode)
                {
                    dynamic_array::push(builder.freeNodes,
                    {
                        .size =
                        {
                            builder.width + RenderAtlasLayoutBuilder::STEP,
                            RenderAtlasLayoutBuilder::STEP,
                        },
                        .rect =
                        {
                            0,
                            builder.height,
                            builder.width + RenderAtlasLayoutBuilder::STEP,
                            builder.height + RenderAtlasLayoutBuilder::STEP,
                        },
                    });
                    dynamic_array::push(builder.freeNodes,
                    {
                        .size =
                        {
                            RenderAtlasLayoutBuilder::STEP,
                            builder.height,
                        },
                        .rect =
                        {
                            builder.width,
                            0,
                            builder.width + RenderAtlasLayoutBuilder::STEP,
                            builder.height,
                        },
                    });
                    builder.width += RenderAtlasLayoutBuilder::STEP;
                    builder.height += RenderAtlasLayoutBuilder::STEP;
                }
            }
            const RenderAtlasLayoutBuilder::Node bestNodeCached = *bestNode;
            dynamic_array::remove(builder.freeNodes, bestNode);
            /*
                +-----+-----+  +-----+-----+
                |     |     |  |     |     |
                | req | p12 |  | req |     |
                |     |     |  |     |     |
                +-----+-----+  +-----+ p21 |
                |           |  |     |     |
                |    p11    |  | p22 |     |
                |           |  |     |     |
                +-----------+  +-----+-----+
            */
            const size_t p11 = bestNodeCached.size.width                       * (bestNodeCached.size.height - requirement.height);
            const size_t p12 = (bestNodeCached.size.width - requirement.width) * requirement.height;
            const size_t p21 = (bestNodeCached.size.width - requirement.width) * bestNodeCached.size.height;
            const size_t p22 = requirement.width                               * (bestNodeCached.size.height - requirement.height);
            if (se_max(p11, p12) > se_max(p21, p22))
            {
                dynamic_array::push(builder.freeNodes,
                {
                    .size =
                    {
                        bestNodeCached.size.width,
                        bestNodeCached.size.height - requirement.height,
                    },
                    .rect =
                    {
                        bestNodeCached.rect.p1x,
                        bestNodeCached.rect.p1y + requirement.height,
                        bestNodeCached.rect.p2x,
                        bestNodeCached.rect.p2y,
                    },
                });
                dynamic_array::push(builder.freeNodes,
                {
                    .size =
                    {
                        bestNodeCached.size.width - requirement.width,
                        requirement.height,
                    },
                    .rect =
                    {
                        bestNodeCached.rect.p1x + requirement.width,
                        bestNodeCached.rect.p1y,
                        bestNodeCached.rect.p2x,
                        bestNodeCached.rect.p1y + requirement.height,
                    },
                });
            }
            else
            {
                dynamic_array::push(builder.freeNodes,
                {
                    .size =
                    {
                        requirement.width,
                        bestNodeCached.size.height - requirement.height,
                    },
                    .rect =
                    {
                        bestNodeCached.rect.p1x,
                        bestNodeCached.rect.p1y + requirement.height,
                        bestNodeCached.rect.p1x + requirement.width,
                        bestNodeCached.rect.p2y,
                    },
                });
                dynamic_array::push(builder.freeNodes,
                {
                    .size =
                    {
                        bestNodeCached.size.width - requirement.width,
                        bestNodeCached.size.height,
                    },
                    .rect =
                    {
                        bestNodeCached.rect.p1x + requirement.width,
                        bestNodeCached.rect.p1y,
                        bestNodeCached.rect.p2x,
                        bestNodeCached.rect.p2y,
                    },
                });
            }
            return &dynamic_array::push(builder.occupiedNodes,
            {
                .size = requirement,
                .rect =
                {
                    bestNodeCached.rect.p1x,
                    bestNodeCached.rect.p1y,
                    bestNodeCached.rect.p1x + requirement.width,
                    bestNodeCached.rect.p1y + requirement.height,
                },
            });
        }

        template<typename Pixel>
        RenderAtlas<Pixel> end(RenderAtlasLayoutBuilder& builder)
        {
            AllocatorBindings allocator = allocators::persistent();
            const size_t arraySize = dynamic_array::size(builder.occupiedNodes);
            DynamicArray<RenderAtlasRect> rects = dynamic_array::create<RenderAtlasRect>(allocator, arraySize);
            DynamicArray<RenderAtlasRectNormalized> uvRects = dynamic_array::create<RenderAtlasRectNormalized>(allocator, arraySize);
            for (auto it : builder.occupiedNodes)
            {
                const RenderAtlasRect rect = iter::value(it).rect;
                dynamic_array::push(rects, rect);
                dynamic_array::push(uvRects,
                {
                    // @NOTE : here we flip first and second Y coords because uv coordinates start from top left corner, not bottom left
                    (float)(rect.p1x + RenderAtlasLayoutBuilder::BORDER) / (float)builder.width,
                    (float)(rect.p2y - RenderAtlasLayoutBuilder::BORDER) / (float)builder.height,
                    (float)(rect.p2x - RenderAtlasLayoutBuilder::BORDER) / (float)builder.width,
                    (float)(rect.p1y + RenderAtlasLayoutBuilder::BORDER) / (float)builder.height,
                });
            }
            dynamic_array::destroy(builder.occupiedNodes);
            dynamic_array::destroy(builder.freeNodes);
            Pixel* const bitmap = (Pixel*)allocator.alloc(allocator.allocator, builder.width * builder.height * sizeof(Pixel), se_default_alignment, se_alloc_tag);
            memset(bitmap, 0, builder.width * builder.height * sizeof(Pixel));
            return
            {
                bitmap,
                builder.width,
                builder.height,
                rects,
                uvRects,
            };
        }
    }

    template<typename Pixel>
    void destroy(RenderAtlas<Pixel>& atlas)
    {
        AllocatorBindings allocator = allocators::persistent();
        allocator.dealloc(allocator.allocator, atlas.bitmap, atlas.width * atlas.height * sizeof(Pixel));
        dynamic_array::destroy(atlas.rects);
        dynamic_array::destroy(atlas.uvRects);
    }

    template<typename PixelDst, typename PixelSrc>
    void blit(RenderAtlas<PixelDst>& atlas, const RenderAtlasRect& rect, const PixelSrc* source, RenderAtlasPutFunciton<PixelDst, PixelSrc> put)
    {
        const size_t sourceWidth = rect.p2x - rect.p1x - RenderAtlasLayoutBuilder::BORDER * 2;
        const size_t sourceHeight = rect.p2y - rect.p1y - RenderAtlasLayoutBuilder::BORDER * 2;
        const size_t fromX = rect.p1x + RenderAtlasLayoutBuilder::BORDER;
        const size_t fromY = rect.p1y + RenderAtlasLayoutBuilder::BORDER;
        for (size_t h = 0; h < sourceHeight; h++)
            for (size_t w = 0; w < sourceWidth; w++)
            {
                const PixelSrc* const p = &source[h * sourceWidth + w];
                PixelDst* const to = &atlas.bitmap[(fromY + h) * atlas.width + fromX + w];
                put(p, to);
            }
    }
}

// =======================================================================
//
// Fonts
//
// =======================================================================

struct FontInfo
{
    stbtt_fontinfo          stbInfo;
    DynamicArray<uint32_t>  supportedCodepoints;
    void*                   memory;
    size_t                  memorySize;
    int                     ascentUnscaled;
    int                     descentUnscaled;
    int                     lineGapUnscaled;
};

namespace font_info
{
    FontInfo create(const DataProvider& data)
    {
        auto as_int16   = [](const uint8_t* data) -> int16_t    { return data[0] * 256 + data[1]; };
        auto as_uint16  = [](const uint8_t* data) -> uint16_t   { return data[0] * 256 + data[1]; };
        auto as_uint32  = [](const uint8_t* data) -> uint32_t   { return (data[0] << 24) + (data[1] << 16) + (data[2] << 8) + data[3]; };
        auto as_uint8   = [](const uint8_t* data) -> uint8_t    { return *data; };
        //
        // Load data and save it with persistent allocator (data provider returns frame memory)
        //
        se_assert(data_provider::is_valid(data));
        auto [dataMemory, dataSize] = data_provider::get(data);
        se_assert(dataMemory);
        AllocatorBindings allocator = allocators::persistent();
        FontInfo result
        {
            .stbInfo                = { },
            .supportedCodepoints    = { },
            .memory                 = allocator.alloc(allocator.allocator, dataSize, se_default_alignment, se_alloc_tag),
            .memorySize             = dataSize,
            .ascentUnscaled         = 0,
            .descentUnscaled        = 0,
            .lineGapUnscaled        = 0,
        };
        memcpy(result.memory, dataMemory, dataSize);
        const unsigned char* charData = (const unsigned char*)result.memory;
        if (!stbtt_InitFont(&result.stbInfo, charData, 0)) // @NOTE : We always use only one font from file
        {
            se_assert_msg(false, "Failed to load font");
        }
        //
        // Get cmap offset
        // https://developer.apple.com/fonts/TrueType-Reference-Manual/
        //
        size_t cmapTableOffset = 0;
        const uint32_t cmapTag = (uint32_t('c') << 24) | (uint32_t('m') << 16) | (uint32_t('a') << 8) | uint32_t('p');
        const uint16_t numTables = as_uint16(charData + 4);
        const size_t tableDirOffset = 12;
        for (size_t it = 0; it < numTables; it++)
        {
            const size_t tableDirEntryOffset = tableDirOffset + 16 * it;
            const uint32_t tableTag = as_uint32(charData + tableDirEntryOffset);
            if (tableTag == cmapTag)
            {
                cmapTableOffset = as_uint32(charData + tableDirEntryOffset + 8);
                break;
            }
        }
        se_assert_msg(cmapTableOffset, "Unable to find cmap table in the font file");
        //
        // Find supported mapping table
        //
        size_t mappingTableOffset = 0;
        const uint16_t numSubtables = as_uint16(charData + cmapTableOffset + 2);
        for (size_t it = 0; it < numSubtables; it++)
        {
            const size_t encodingSubtableOffset = cmapTableOffset + 4 + 8 * it;
            const uint16_t platformId = as_uint16(charData + encodingSubtableOffset);
            switch (platformId)
            {
                case 0: // Unicode (any platform-specific id will be fine)
                {
                    mappingTableOffset = cmapTableOffset + as_uint32(charData + encodingSubtableOffset + 4);
                    break;
                } break;
                case 3: // Microsoft
                {
                    const uint16_t platformSpecificId = as_uint16(charData + encodingSubtableOffset + 2);
                    switch (platformSpecificId)
                    {
                        case 1: // Unicode BMP-only (UCS-2)
                        case 10: // Unicode UCS-4
                        {
                            mappingTableOffset = cmapTableOffset + as_uint32(charData + encodingSubtableOffset + 4);
                            break;
                        } break;
                    }
                } break;
            }
            if (mappingTableOffset) break;
        }
        se_assert_msg(mappingTableOffset, "Unable to find supported mapping table in the font file");
        //
        // Save every codepoint supported by this font
        // https://developer.apple.com/fonts/TrueType-Reference-Manual/RM06/Chap6cmap.html
        //
        result.supportedCodepoints = dynamic_array::create<uint32_t>(allocators::persistent(), 256);
        const uint16_t cmapFormat = as_uint16(charData + mappingTableOffset);
        switch (cmapFormat)
        {
            case 0:
            {
                const uint16_t numBytes = as_uint16(charData + mappingTableOffset + 2);
                se_assert(numBytes == 256);
                const uint8_t* const glythIndices = (const uint8_t*)(charData + mappingTableOffset + 6);
                for (uint32_t codepointIt = 0; codepointIt < numBytes; codepointIt++)
                    if (as_uint8(glythIndices + codepointIt)) dynamic_array::push(result.supportedCodepoints, codepointIt);
            } break;
            case 4:
            {
                const uint16_t segCount                 = as_uint16(charData + mappingTableOffset + 6) / 2;
                const size_t endCodeTableOffset         = mappingTableOffset + 14;
                const size_t startCodeTableOffset       = endCodeTableOffset + 2    + segCount * 2;
                const size_t idDeltaTableOffset         = startCodeTableOffset      + segCount * 2;
                const size_t idRangeOffsetTableOffset   = idDeltaTableOffset        + segCount * 2;
                const size_t glythIndicesTableOffset    = idRangeOffsetTableOffset  + segCount * 2;
                for (uint16_t segIt = 0; segIt < segCount; segIt++)
                {
                    const uint16_t startCode        = as_uint16(charData + startCodeTableOffset     + segIt * 2);
                    const uint16_t endCode          = as_uint16(charData + endCodeTableOffset       + segIt * 2);
                    const uint16_t idRangeOffset    = as_uint16(charData + idRangeOffsetTableOffset + segIt * 2);
                    if (startCode == 0xFFFF)
                    {
                        se_assert(endCode == 0xFFFF);
                        break;
                    }
                    if (idRangeOffset)
                        for (uint16_t codepointIt = startCode; codepointIt <= endCode; codepointIt++)
                        {
                            const size_t idRangeOffsetOffset = idRangeOffsetTableOffset + segIt * 2;
                            const size_t glythIndexOffset = idRangeOffset + 2 * (codepointIt - startCode) + idRangeOffsetOffset;
                            if (as_uint16(charData + glythIndexOffset)) dynamic_array::push(result.supportedCodepoints, (uint32_t)codepointIt);
                        }
                    else
                        for (uint16_t codepointIt = startCode; codepointIt <= endCode; codepointIt++)
                        {
                            const int32_t idDelta = as_int16(charData + idDeltaTableOffset + segIt * 2);
                            const uint16_t glythIndex = (uint16_t)(idDelta + (int32_t)codepointIt);
                            se_assert(glythIndex >= 0);
                            if (as_uint16(charData + glythIndicesTableOffset + glythIndex * 2)) dynamic_array::push(result.supportedCodepoints, (uint32_t)codepointIt);
                        }
                }
            } break;
            case 6:
            {
                const uint16_t firstCode = as_uint16(charData + mappingTableOffset + 6);
                const uint16_t entryCount = as_uint16(charData + mappingTableOffset + 8);
                const uint8_t* const glythIndicesTable = charData + mappingTableOffset + 10;
                for (uint32_t entryIt = 0; entryIt < entryCount; entryIt++)
                    if (as_uint16(glythIndicesTable + (entryIt * 2))) dynamic_array::push(result.supportedCodepoints, entryIt + firstCode);
            } break;
            case 10:
            {
                const uint32_t firstCode = as_uint16(charData + mappingTableOffset + 12);
                const uint32_t entryCount = as_uint16(charData + mappingTableOffset + 16);
                const uint8_t* const glythIndicesTable = charData + mappingTableOffset + 20;
                for (uint32_t entryIt = 0; entryIt < entryCount; entryIt++)
                    if (as_uint16(glythIndicesTable + (entryIt * 2))) dynamic_array::push(result.supportedCodepoints, entryIt + firstCode);
            } break;
            case 12:
            case 13:
            {
                const uint32_t numGroups = as_uint32(charData + mappingTableOffset + 12);
                for (uint32_t groupIt = 0; groupIt < numGroups; groupIt++)
                {
                    const uint32_t startCode = as_uint32(charData + mappingTableOffset + 16 + 12 * groupIt);
                    const uint32_t endCode = as_uint32(charData + mappingTableOffset + 16 + 12 * groupIt + 4);
                    for (uint32_t codepointIt = startCode; codepointIt <= endCode; codepointIt++)
                        dynamic_array::push(result.supportedCodepoints, codepointIt);
                }
            } break;
            default: { se_assert_msg(false, "Unsupported cmap format : {}", cmapFormat); } break;
        }
        se_assert_msg(dynamic_array::size(result.supportedCodepoints), "Font doesn't support any unicode codepoints");
        //
        // Get vmetrics
        //
        stbtt_GetFontVMetrics(&result.stbInfo, &result.ascentUnscaled, &result.descentUnscaled, &result.lineGapUnscaled);

        return result;
    }

    inline void destroy(FontInfo& font)
    {
        AllocatorBindings allocator = allocators::persistent();
        allocator.dealloc(allocator.allocator, font.memory, font.memorySize);
        dynamic_array::destroy(font.supportedCodepoints);
    }
}

// =======================================================================
//
// Codepoint hash table
// Special variation of hash table, optimized for a SeUtf32Char codepoint keys
// Mostly a copy paste from original HashTable
//
// =======================================================================

template<typename T>
struct CodepointHashTable
{
    struct Entry
    {
        SeUtf32Char key;
        uint32_t isOccupied;
        T value;
    };
    AllocatorBindings allocator;
    Entry* memory;
    size_t capacity;
    size_t size;
};

namespace codepoint_hash_table
{
    template<typename T>
    CodepointHashTable<T> create(AllocatorBindings allocator, size_t capacity)
    {
        using Entry = CodepointHashTable<T>::Entry;
        Entry* const memory = (Entry*)allocator.alloc(allocator.allocator, sizeof(Entry) * capacity * 2, se_default_alignment, se_alloc_tag);
        memset(memory, 0, sizeof(Entry) * capacity * 2);
        return
        {
            allocator,
            memory,
            capacity * 2,
            0
        };
    }

    template<typename T>
    void destroy(CodepointHashTable<T>& table)
    {
        using Entry = CodepointHashTable<T>::Entry;
        const AllocatorBindings& allocator = table.allocator;
        allocator.dealloc(allocator.allocator, table.memory, sizeof(Entry) * table.capacity);
    }

    template<typename T>
    T* set(CodepointHashTable<T>& table, SeUtf32Char key, const T& value)
    {
        using Entry = CodepointHashTable<T>::Entry;
        //
        // Expand if needed
        //
        if (table.size >= (table.capacity / 2))
        {
            const size_t oldCapacity = table.capacity;
            const size_t newCapacity = oldCapacity * 2;
            CodepointHashTable<T> newTable = codepoint_hash_table::create<T>(table.allocator, newCapacity);
            for (size_t it = 0; it < oldCapacity; it++)
            {
                Entry& entry = table.memory[it];
                if (entry.isOccupied) codepoint_hash_table::set(newTable, entry.key, entry.value);
            }
            codepoint_hash_table::destroy(table);
            table = newTable;
        }
        //
        // Find position
        //
        const size_t capacity = table.capacity;
        const size_t initialPosition = key % capacity;
        size_t currentPosition = initialPosition;
        do
        {
            const Entry& entry = table.memory[currentPosition];
            if (!entry.isOccupied || entry.key == key) break;
            currentPosition = ((currentPosition + 1) % capacity);
        } while (currentPosition != initialPosition);
        //
        // Fill data
        //
        Entry& entry = table.memory[currentPosition];
        if (!entry.isOccupied)
        {
            entry.isOccupied = true;
            entry.key = key;
            table.size += 1;
        }
        memcpy(&entry.value, &value, sizeof(T));
        return &entry.value;
    }

    template<typename T>
    size_t index_of(const CodepointHashTable<T>& table, SeUtf32Char key)
    {
        using Entry = CodepointHashTable<T>::Entry;
        const size_t capacity = table.capacity;
        const size_t initialPosition = key % capacity;
        size_t currentPosition = initialPosition;
        do
        {
            const Entry& data = table.memory[currentPosition];
            if (!data.isOccupied) break;
            if (data.key == key) return currentPosition;
            currentPosition = ((currentPosition + 1) % table.capacity);
        } while (currentPosition != initialPosition);
        return capacity;
    }

    template<typename T>
    inline T* get(CodepointHashTable<T>& table, SeUtf32Char key)
    {
        const size_t position = index_of(table, key);
        return position != table.capacity ? &table.memory[position].value : nullptr;
    }

    template<typename T>
    inline const T* get(const CodepointHashTable<T>& table, SeUtf32Char key)
    {
        const size_t position = index_of(table, key);
        return position != table.capacity ? &table.memory[position].value : nullptr;
    }

    template<typename T>
    inline size_t size(CodepointHashTable<T>& table)
    {
        return table.size;
    }
}

template<typename Value, typename Table>
struct CodepointHashTableIterator;

template<typename Value, typename Table>
struct CodepointHashTableIteratorValue
{
    SeUtf32Char                                 key;
    Value&                                      value;
    CodepointHashTableIterator<Value, Table>*   iterator;
};

template<typename Value, typename Table>
struct CodepointHashTableIterator
{
    Table* table;
    size_t index;

    bool                                            operator != (const CodepointHashTableIterator& other) const;
    CodepointHashTableIteratorValue<Value, Table>   operator *  ();
    CodepointHashTableIterator&                     operator ++ ();
};

template<typename Value>
CodepointHashTableIterator<Value, CodepointHashTable<Value>> begin(CodepointHashTable<Value>& table)
{
    if (table.size == 0) return { &table, table.capacity };
    size_t it = 0;
    while (it < table.capacity && !table.memory[it].isOccupied) { it++; }
    return { &table, it };
}

template<typename Value>
CodepointHashTableIterator<Value, CodepointHashTable<Value>> end(CodepointHashTable<Value>& table)
{
    return { &table, table.capacity };
}

template<typename Value>
CodepointHashTableIterator<const Value, const CodepointHashTable<Value>> begin(const CodepointHashTable<Value>& table)
{
    if (table.size == 0) return { &table, table.capacity };
    size_t it = 0;
    while (it < table.capacity && !table.memory[it].isOccupied) { it++; }
    return { &table, it };
}

template<typename Value>
CodepointHashTableIterator<const Value, const CodepointHashTable<Value>> end(const CodepointHashTable<Value>& table)
{
    return { &table, table.capacity };
}

template<typename Value, typename Table>
bool CodepointHashTableIterator<Value, Table>::operator != (const CodepointHashTableIterator<Value, Table>& other) const
{
    return (table != other.table) || (index != other.index);
}

template<typename Value, typename Table>
CodepointHashTableIteratorValue<Value, Table> CodepointHashTableIterator<Value, Table>::operator * ()
{
    return { table->memory[index].key, table->memory[index].value, this };
}

template<typename Value, typename Table>
CodepointHashTableIterator<Value, Table>& CodepointHashTableIterator<Value, Table>::operator ++ ()
{
    do { index++; } while (index < table->capacity && !table->memory[index].isOccupied);
    return *this;
}

namespace iter
{
    template<typename Value>
    const Value& value(const CodepointHashTableIteratorValue<const Value, const CodepointHashTable<Value>>& val)
    {
        return val.value;
    }

    template<typename Value>
    Value& value(CodepointHashTableIteratorValue<Value, CodepointHashTable<Value>>& val)
    {
        return val.value;
    }

    template<typename Value, typename Table>
    SeUtf32Char key(const CodepointHashTableIteratorValue<Value, Table>& val)
    {
        return val.key;
    }

    template<typename Value, typename Table>
    size_t index(const CodepointHashTableIteratorValue<Value, Table>& val)
    {
        return val.iterator->index;
    }
}

// =======================================================================
//
// Font group
//
// =======================================================================

struct FontGroup
{
    struct CodepointInfo
    {
        const FontInfo* font;
        uint32_t        atlasRectIndex;
        int             advanceWidthUnscaled;
        int             leftSideBearingUnscaled;
    };

    static constexpr float ATLAS_CODEPOINT_HEIGHT_PIX = 64;

    const FontInfo*                     fonts[SeUiFontGroupInfo::MAX_FONTS];
    RenderAtlas<uint8_t>                atlas;
    SeTextureRef                        texture;
    CodepointHashTable<CodepointInfo>   codepointToInfo;
    int                                 lineGapUnscaled;
    int                                 ascentUnscaled;
    int                                 descentUnscaled;
};

namespace font_group
{
    inline int direct_cast_to_int(SeUtf32Char value)
    {
        return *((int*)&value);
    };

    FontGroup create(const FontInfo** fonts, size_t numFonts)
    {
        FontGroup groupInfo
        {
            .fonts              = { },
            .atlas              = { },
            .texture            = { },
            .codepointToInfo    = { },
            .lineGapUnscaled    = 0,
            .ascentUnscaled     = 0,
        };
        //
        // Load all fonts and map codepoints to fonts
        //
        CodepointHashTable<const FontInfo*> codepointToFont = codepoint_hash_table::create<const FontInfo*>(allocators::frame(), 256);
        for (size_t it = 0; it < numFonts; it++)
        {
            const FontInfo* font = fonts[it];
            groupInfo.fonts[it] = font;
            for (auto codepointIt : font->supportedCodepoints)
            {
                const uint32_t codepoint = iter::value(codepointIt);
                if (!codepoint_hash_table::get(codepointToFont, codepoint))
                {
                    codepoint_hash_table::set(codepointToFont, codepoint, font);
                }
            }
            groupInfo.lineGapUnscaled = groupInfo.lineGapUnscaled > font->lineGapUnscaled ? groupInfo.lineGapUnscaled : font->lineGapUnscaled;
            groupInfo.ascentUnscaled = groupInfo.ascentUnscaled > font->ascentUnscaled ? groupInfo.ascentUnscaled : font->ascentUnscaled;
            groupInfo.descentUnscaled = groupInfo.descentUnscaled < font->descentUnscaled ? groupInfo.descentUnscaled : font->descentUnscaled;
        }
        const size_t numCodepoints = codepoint_hash_table::size(codepointToFont);
        //
        // Get all codepoint bitmaps and fill codepoint infos
        //
        struct CodepointBitmapInfo
        {
            RenderAtlasRectSize size;
            uint8_t* memory;
        };
        CodepointHashTable<CodepointBitmapInfo> codepointToBitmap = codepoint_hash_table::create<CodepointBitmapInfo>(allocators::frame(), numCodepoints);
        groupInfo.codepointToInfo = codepoint_hash_table::create<FontGroup::CodepointInfo>(allocators::persistent(), numCodepoints);
        for (auto it : codepointToFont)
        {
            //
            // Bitmap
            //
            const SeUtf32Char codepoint = iter::key(it);
            const FontInfo* const font = iter::value(it);
            const float scale = stbtt_ScaleForPixelHeight(&font->stbInfo, FontGroup::ATLAS_CODEPOINT_HEIGHT_PIX);
            int width;
            int height;
            int xOff;
            int yOff;
            // @TODO : setup custom malloc and free for stbtt
            unsigned char* bitmap = stbtt_GetCodepointBitmap(&font->stbInfo, scale, scale, direct_cast_to_int(codepoint), &width, &height, &xOff, &yOff);
            codepoint_hash_table::set(codepointToBitmap, codepoint, { { (size_t)width, (size_t)height }, (uint8_t*)bitmap });
            //
            // Info
            //
            int advanceWidth;
            int leftSideBearing;
            stbtt_GetCodepointHMetrics(&font->stbInfo, direct_cast_to_int(codepoint), &advanceWidth, &leftSideBearing);
            const FontGroup::CodepointInfo codepointInfo
            {
                .font                       = font,
                .atlasRectIndex             = 0,
                .advanceWidthUnscaled       = advanceWidth,
                .leftSideBearingUnscaled    = leftSideBearing,
            };
            codepoint_hash_table::set(groupInfo.codepointToInfo, codepoint, codepointInfo);
        }
        //
        // Build atlas layout
        //
        HashTable<RenderAtlasRect, uint32_t> atlasRectToCodepoint = hash_table::create<RenderAtlasRect, uint32_t>(allocators::frame(), numCodepoints);
        RenderAtlasLayoutBuilder builder = render_atlas::layout_builder::begin();
        for (auto it : codepointToBitmap)
        {
            const CodepointBitmapInfo& bitmap = iter::value(it);
            se_assert((bitmap.size.width == 0) == (bitmap.size.height == 0));
            se_assert((bitmap.memory == nullptr) == (bitmap.size.width == 0));
            const RenderAtlasLayoutBuilder::Node* node = render_atlas::layout_builder::push(builder, bitmap.memory ? bitmap.size : RenderAtlasRectSize{ 1, 1 });
            hash_table::set(atlasRectToCodepoint, node->rect, iter::key(it));
        }
        //
        // Fill atlas with bitmaps
        //
        groupInfo.atlas = render_atlas::layout_builder::end<uint8_t>(builder);
        for (auto it : groupInfo.atlas.rects)
        {
            const RenderAtlasRect& rect = iter::value(it);
            const uint32_t codepoint = *hash_table::get(atlasRectToCodepoint, rect);
            const CodepointBitmapInfo bitmap = *codepoint_hash_table::get(codepointToBitmap, codepoint);
            if (bitmap.memory)
            {
                const RenderAtlasPutFunciton<uint8_t, uint8_t> put = [](const uint8_t* from, uint8_t* to) { *to = *from; };
                render_atlas::blit(groupInfo.atlas, rect, bitmap.memory, put);
            }
            FontGroup::CodepointInfo* const info = codepoint_hash_table::get(groupInfo.codepointToInfo, codepoint);
            se_assert(info);
            info->atlasRectIndex = (uint32_t)iter::index(it);
        }
        groupInfo.texture = render::texture
        ({
            .format     = SeTextureFormat::R_8_UNORM,
            .width      = uint32_t(groupInfo.atlas.width),
            .height     = uint32_t(groupInfo.atlas.height),
            .data       = data_provider::from_memory(groupInfo.atlas.bitmap, groupInfo.atlas.width * groupInfo.atlas.height * sizeof(uint8_t)),
        });
        for (auto it : codepointToBitmap)
        {
            const CodepointBitmapInfo bitmap = iter::value(it);
            if (bitmap.memory) stbtt_FreeBitmap((unsigned char*)bitmap.memory, nullptr);
        }

        codepoint_hash_table::destroy(codepointToFont);
        codepoint_hash_table::destroy(codepointToBitmap);
        hash_table::destroy(atlasRectToCodepoint);

        return groupInfo;
    }

    void destroy(FontGroup& group)
    {
        render_atlas::destroy(group.atlas);
        codepoint_hash_table::destroy(group.codepointToInfo);
        render::destroy(group.texture);
    }

    float scale_for_pixel_height(const FontGroup& group, float height)
    {
        float result = 0.0f;
        for (size_t it = 0; it < SeUiFontGroupInfo::MAX_FONTS; it++)
        {
            if (!group.fonts[it]) break;
            const float h = stbtt_ScaleForPixelHeight(&group.fonts[it]->stbInfo, height);
            if (h > result) result = h;
        }
        return result;
    }
}

// =======================================================================
//
// Ui context
//
// =======================================================================

struct UiRenderVertex
{
    float       x;
    float       y;
    float       uvX;
    float       uvY;
    uint32_t    colorIndex;
    float       _pad[3];
};

struct UiRenderColorsUnpacked
{
    static constexpr int32_t MODE_TEXT = 0;
    static constexpr int32_t MODE_SIMPLE = 1;

    SeColorUnpacked tint;
    int32_t         mode;
    float           _pad[3];
};

struct UiRenderDrawData
{
    uint32_t    firstVertexIndex;
    float       _pad[3];
};

struct UiDrawCall
{
    SeTextureRef texture;
};

using UiParams = SeUiParam[SeUiParam::_COUNT];

struct UiActionFlags
{
    enum
    {
        CAN_BE_MOVED                    = 0x00000001,
        CAN_BE_RESIZED_X_LEFT           = 0x00000002,
        CAN_BE_RESIZED_X_RIGHT          = 0x00000004,
        CAN_BE_RESIZED_Y_TOP            = 0x00000008,
        CAN_BE_RESIZED_Y_BOTTOM         = 0x00000010,
        CAN_BE_SCROLLED_BY_MOUSE_MOVE_X = 0x00000020,
        CAN_BE_SCROLLED_BY_MOUSE_MOVE_Y = 0x00000040,
    };
    using Type = uint32_t;
};

struct UiStateFlags
{
    enum
    {
        IS_TOGGLED = 0x00000001,
    };
    using Type = uint32_t;
};

struct UiQuadCoords
{
    float blX;
    float blY;
    float trX;
    float trY;
};

/*
    This sturct contains all info required for implemention scroll functionality
    normalizedScrollPosition - value in range of [0, 1], describes current scroll position
    absoluteContentSize - range of data to be scrolled
    absoluteScrollRange - position range of scroll, calculated as *absoluteContentSize - workRegionWidth/Height*
    absoluteScrollPosition - scroll position in range [0, absoluteScrollRange]
    absoluteScrollBarFrom - scroll bar minimum position in screen space, i.e. it will change if window moves
    absoluteScrollBarTo - scroll bar maximum position in screen space, i.e. it will change if window moves
    absoluteScrollBarPosition - scroll bar position in screen space in range [absoluteScrollBarFrom, absoluteScrollBarTo]
*/
struct UiObjectScrollInfo
{
    // Updated in ui::begin
    float normalizedScrollPosition;

    // Recalculated in ui::begin_window and subsequent calls
    float absoluteContentSize;

    // Recalculated in ui::begin_window
    float absoluteScrollRange;
    float absoluteScrollPosition;
    float absoluteScrollBarFrom;
    float absoluteScrollBarTo;
    float absoluteScrollBarPosition;
};

struct UiObjectData
{
    // Windows data
    UiQuadCoords        quad;
    UiQuadCoords        workRegion;

    float               minWidth;
    float               minHeight;
    SeUiFlags::Type     settingsFlags;
    UiActionFlags::Type actionFlags;

    UiObjectScrollInfo  scrollInfoX;
    UiObjectScrollInfo  scrollInfoY;

    // Buttons data
    UiStateFlags::Type  stateFlags;
};

struct UiContext
{
    static constexpr size_t COLORS_BUFFER_CAPACITY = 1024;
    static constexpr size_t COLORS_BUFFER_SIZE_BYTES = sizeof(UiRenderColorsUnpacked) * COLORS_BUFFER_CAPACITY;
    static constexpr size_t VERTICES_BUFFER_CAPACITY = 1024 * 32;
    static constexpr size_t VERTICES_BUFFER_SIZE_BYTES = sizeof(UiRenderVertex) * VERTICES_BUFFER_CAPACITY;
    static constexpr size_t DRAW_DATAS_BUFFER_CAPACITY = 1024;
    static constexpr size_t DRAW_DATAS_BUFFER_SIZE_BYTES = sizeof(UiRenderDrawData) * DRAW_DATAS_BUFFER_CAPACITY;

    SePassRenderTarget                              target;

    const FontGroup*                                currentFontGroup;
    UiParams                                        currentParams;
    
    DynamicArray<UiDrawCall>                        frameDrawCalls;
    DynamicArray<UiRenderDrawData>                  frameDrawDatas;
    DynamicArray<UiRenderVertex>                    frameVertices;
    DynamicArray<UiRenderColorsUnpacked>            frameColors;

    float                                           mouseX;
    float                                           mouseY;
    float                                           mouseDeltaX;
    float                                           mouseDeltaY;
    bool                                            isMouseDown;
    bool                                            isMouseJustDown;

    SeString                                        previousHoveredObjectUid;
    SeString                                        currentHoveredObjectUid;
    SeString                                        previousHoveredWindowUid;
    SeString                                        currentHoveredWindowUid;

    SeString                                        previousActiveObjectUid;
    SeString                                        currentActiveObjectUid;
    SeString                                        currentWindowUid;
    bool                                            isJustActivated;

    bool                                            hasWorkRegion;
    UiQuadCoords                                    workRegion;

    HashTable<SeString, UiObjectData>               uidToObjectData;
    HashTable<DataProvider, FontInfo>               fontInfos;
    HashTable<SeUiFontGroupInfo, FontGroup>         fontGroups;

    SeProgramRef                                    drawUiVs;
    SeProgramRef                                    drawUiFs;
    SeSamplerRef                                    sampler;
};

const UiParams DEFAULT_PARAMS =
{
    /* FONT_COLOR                   */ { .color = col::pack({ 1.0f, 1.0f, 1.0f, 1.0f }) },
    /* FONT_HEIGHT                  */ { .dim = 30.0f },
    /* FONT_LINE_GAP                */ { .dim = 5.0f },
    /* PRIMARY_COLOR                */ { .color = col::pack({ 0.73f, 0.11f, 0.14f, 1.0f }) },
    /* SECONDARY_COLOR              */ { .color = col::pack({ 0.09f, 0.01f, 0.01f, 0.7f }) },
    /* ACCENT_COLOR                 */ { .color = col::pack({ 0.88f, 0.73f, 0.73f, 1.0f }) },
    /* WINDOW_TOP_PANEL_THICKNESS   */ { .dim = 16.0f },
    /* WINDOW_BORDER_THICKNESS      */ { .dim = 2.0f },
    /* WINDOW_SCROLL_THICKNESS      */ { .dim = 10.0f },
    /* WINDOW_INNER_PADDING         */ { .dim = 5.0f },
    /* PIVOT_POSITION_X             */ { .dim = 0.0f },
    /* PIVOT_POSITION_Y             */ { .dim = 0.0f },
    /* PIVOT_TYPE_X                 */ { .enumeration = SeUiPivotType::BOTTOM_LEFT },
    /* PIVOT_TYPE_Y                 */ { .enumeration = SeUiPivotType::BOTTOM_LEFT },
    /* BUTTON_BORDER_SIZE           */ { .dim = 5.0f },
};

UiContext g_uiCtx;

namespace hash_value
{
    namespace builder
    {
        template<>
        void absorb<SeUiFontGroupInfo>(HashValueBuilder& builder, const SeUiFontGroupInfo& input)
        {
            for (size_t it = 0; it < SeUiFontGroupInfo::MAX_FONTS; it++)
            {
                if (!data_provider::is_valid(input.fonts[it])) break;
                hash_value::builder::absorb(builder, input.fonts[it]);
            }
        }
    }

    template<>
    HashValue generate<SeUiFontGroupInfo>(const SeUiFontGroupInfo& value)
    {
        HashValueBuilder builder = hash_value::builder::begin();
        hash_value::builder::absorb(builder, value);
        return hash_value::builder::end(builder);
    }
}

namespace utils
{
    template<>
    bool compare<UiRenderColorsUnpacked, UiRenderColorsUnpacked>(const UiRenderColorsUnpacked& first, const UiRenderColorsUnpacked& second)
    {
        return compare(first.tint, second.tint) && (first.mode == second.mode);
    }
}

namespace ui
{
    namespace impl
    {
        inline struct { SeString uid; UiObjectData* data; bool isFirst; } object_data_get(const char* cstrUid)
        {
            UiObjectData* data = hash_table::get(g_uiCtx.uidToObjectData, cstrUid);
            const bool isFirstAccess = data == nullptr;
            if (!data)
            {
                data = hash_table::set(g_uiCtx.uidToObjectData, string::create(cstrUid, SeStringLifetime::PERSISTENT), { });
            }
            se_assert(data);
            return { hash_table::key(g_uiCtx.uidToObjectData, data), data, isFirstAccess };
        }

        uint32_t set_draw_call(const UiRenderColorsUnpacked& colors)
        {
            const size_t numDrawCalls = dynamic_array::size(g_uiCtx.frameDrawCalls);
            const UiDrawCall* const lastDrawCall = numDrawCalls ? &g_uiCtx.frameDrawCalls[numDrawCalls - 1] : nullptr;
            if (!lastDrawCall)
            {
                dynamic_array::push(g_uiCtx.frameDrawCalls, { .texture = { } });
                dynamic_array::push(g_uiCtx.frameDrawDatas, { .firstVertexIndex = dynamic_array::size<uint32_t>(g_uiCtx.frameVertices), });
                se_assert(dynamic_array::size(g_uiCtx.frameColors) == 0);
                dynamic_array::push(g_uiCtx.frameColors, colors);
            }
            if (!utils::compare(g_uiCtx.frameColors[dynamic_array::size(g_uiCtx.frameColors) - 1], colors))
                dynamic_array::push(g_uiCtx.frameColors, colors);

            return dynamic_array::size<uint32_t>(g_uiCtx.frameColors) - 1;
        }

        uint32_t set_draw_call(SeTextureRef textureRef, const UiRenderColorsUnpacked& colors)
        {
            //
            // Get draw call
            //
            const size_t numDrawCalls = dynamic_array::size(g_uiCtx.frameDrawCalls);
            const UiDrawCall* const lastDrawCall = numDrawCalls ? &g_uiCtx.frameDrawCalls[numDrawCalls - 1] : nullptr;
            if (!lastDrawCall || (lastDrawCall->texture && !utils::compare(lastDrawCall->texture, textureRef)))
            {
                dynamic_array::push(g_uiCtx.frameDrawCalls, { .texture = { } });
                dynamic_array::push(g_uiCtx.frameDrawDatas, { .firstVertexIndex = dynamic_array::size<uint32_t>(g_uiCtx.frameVertices), });
            }
            //
            // Push texture and color
            //
            UiDrawCall* const drawCall = &g_uiCtx.frameDrawCalls[dynamic_array::size(g_uiCtx.frameDrawCalls) - 1];
            if (!drawCall->texture) drawCall->texture = textureRef;
            se_assert(utils::compare(drawCall->texture, textureRef));
            if (!utils::compare(g_uiCtx.frameColors[dynamic_array::size(g_uiCtx.frameColors) - 1], colors))
                dynamic_array::push(g_uiCtx.frameColors, colors);

            return dynamic_array::size<uint32_t>(g_uiCtx.frameColors) - 1;
        }

        inline UiQuadCoords clamp_to_work_region(const UiQuadCoords& quad)
        {
            const float wrx1 = g_uiCtx.workRegion.blX;
            const float wry1 = g_uiCtx.workRegion.blY;
            const float wrx2 = g_uiCtx.workRegion.trX;
            const float wry2 = g_uiCtx.workRegion.trY;
            return
            {
                se_clamp(quad.blX, wrx1, wrx2),
                se_clamp(quad.blY, wry1, wry2),
                se_clamp(quad.trX, wrx1, wrx2),
                se_clamp(quad.trY, wry1, wry2),
            };
        }

        inline void push_quad(uint32_t colorIndex, const UiQuadCoords& unclamped, float u1 = 0, float v1 = 0, float u2 = 0, float v2 = 0)
        {
            const UiQuadCoords clamped = clamp_to_work_region(unclamped);
            if (clamped.blX == clamped.trX || clamped.blY == clamped.trY) return; // Out of bounds

            const float width = unclamped.trX - unclamped.blX;
            const float height = unclamped.trY - unclamped.blY;
            const float u1clamped = se_lerp(u1, u2, (clamped.blX - unclamped.blX) / width);
            const float v1clamped = se_lerp(v1, v2, (clamped.blY - unclamped.blY) / height);
            const float u2clamped = se_lerp(u2, u1, (unclamped.trX - clamped.trX) / width);
            const float v2clamped = se_lerp(v2, v1, (unclamped.trY - clamped.trY) / height);

            dynamic_array::push(g_uiCtx.frameVertices, { clamped.blX, clamped.blY, u1clamped, v1clamped, colorIndex });
            dynamic_array::push(g_uiCtx.frameVertices, { clamped.blX, clamped.trY, u1clamped, v2clamped, colorIndex });
            dynamic_array::push(g_uiCtx.frameVertices, { clamped.trX, clamped.trY, u2clamped, v2clamped, colorIndex });
            dynamic_array::push(g_uiCtx.frameVertices, { clamped.blX, clamped.blY, u1clamped, v1clamped, colorIndex });
            dynamic_array::push(g_uiCtx.frameVertices, { clamped.trX, clamped.trY, u2clamped, v2clamped, colorIndex });
            dynamic_array::push(g_uiCtx.frameVertices, { clamped.trX, clamped.blY, u2clamped, v1clamped, colorIndex });
        }

        inline UiQuadCoords get_corners(float width, float height)
        {
            if (g_uiCtx.hasWorkRegion)
            {
                const UiObjectData* const window = hash_table::get(g_uiCtx.uidToObjectData, g_uiCtx.currentWindowUid);
                const float blX = window
                    ? g_uiCtx.workRegion.blX - window->scrollInfoX.absoluteScrollPosition
                    : g_uiCtx.workRegion.blX;
                const float tlY = window
                    ? window->workRegion.trY - window->scrollInfoY.absoluteContentSize + window->scrollInfoY.absoluteScrollPosition
                    : g_uiCtx.workRegion.trY;
                return
                {
                    blX,
                    tlY - height,
                    blX + width,
                    tlY,
                };
            }
            else
            {
                const bool isCenteredX = g_uiCtx.currentParams[SeUiParam::PIVOT_TYPE_X].enumeration == SeUiPivotType::CENTER;
                const bool isCenteredY = g_uiCtx.currentParams[SeUiParam::PIVOT_TYPE_Y].enumeration == SeUiPivotType::CENTER;
                const float dimX = g_uiCtx.currentParams[SeUiParam::PIVOT_POSITION_X].dim - (isCenteredX ? width / 2.0f : 0.0f);
                const float dimY = g_uiCtx.currentParams[SeUiParam::PIVOT_POSITION_Y].dim - (isCenteredY ? height / 2.0f : 0.0f);
                return { dimX, dimY, dimX + width, dimY + height };
            }
        }

        inline bool is_under_cursor(const UiQuadCoords& quad)
        {
            return
                (quad.blX <= g_uiCtx.mouseX) &&
                (quad.blY <= g_uiCtx.mouseY) &&
                (quad.trX >= g_uiCtx.mouseX) &&
                (quad.trY >= g_uiCtx.mouseY);
        }

        UiQuadCoords get_text_bbox(const char* utf8text)
        {
            const FontGroup* const fontGroup = g_uiCtx.currentFontGroup;
            const float fontHeight = g_uiCtx.currentParams[SeUiParam::FONT_HEIGHT].dim;
            const float baselineX = 0;
            const float baselineY = 0;
            float positionX = 0;
            float biggestAscent = 0;
            float biggestDescent = 0;
            int previousCodepoint = 0;
            for (SeUtf32Char codepoint : SeUtf32Iterator{ utf8text })
            {
                const FontGroup::CodepointInfo* const codepointInfo = codepoint_hash_table::get(fontGroup->codepointToInfo, codepoint);
                se_assert(codepointInfo);
                const FontInfo* const font = codepointInfo->font;
                const float scale = stbtt_ScaleForPixelHeight(&font->stbInfo, fontHeight);
                const float advance = scale * float(codepointInfo->advanceWidthUnscaled);
                const float bearing = scale * float(codepointInfo->leftSideBearingUnscaled);
                const int codepointSigned = font_group::direct_cast_to_int(codepoint);
                int x0;
                int y0;
                int x1;
                int y1;
                stbtt_GetCodepointBitmapBox(&font->stbInfo, codepointSigned, scale, scale, &x0, &y0, &x1, &y1);
                const float descent = float(y1);
                if (biggestDescent < descent) biggestDescent = descent;
                const float ascent = float(-y0);
                if (biggestAscent < ascent) biggestAscent = ascent;
                const float additionalAdvance = scale * float(stbtt_GetCodepointKernAdvance(&font->stbInfo, previousCodepoint, codepointSigned));
                positionX += additionalAdvance + advance;
                previousCodepoint = codepointSigned;
            }
            return { 0, -biggestDescent, positionX, biggestAscent };
        }

        float draw_text_at_pos(const char* utf8text, float baselineX, float baselineY)
        {
            if (g_uiCtx.hasWorkRegion && baselineX >= g_uiCtx.workRegion.trX)
            {
                return 0.0f;
            }
            const FontGroup* const fontGroup = g_uiCtx.currentFontGroup;
            const float fontHeight = g_uiCtx.currentParams[SeUiParam::FONT_HEIGHT].dim;
            const uint32_t colorIndex = set_draw_call(fontGroup->texture,
            {
                .tint = col::unpack(g_uiCtx.currentParams[SeUiParam::FONT_COLOR].color),
                .mode = UiRenderColorsUnpacked::MODE_TEXT,
            });
            //
            // Process codepoints
            //
            float positionX = baselineX;
            int previousCodepoint = 0;
            for (SeUtf32Char codepoint : SeUtf32Iterator{ utf8text })
            {
                const FontGroup::CodepointInfo* const codepointInfo = codepoint_hash_table::get(fontGroup->codepointToInfo, codepoint);
                se_assert(codepointInfo);
                const FontInfo* const font = codepointInfo->font;
                const float scale = stbtt_ScaleForPixelHeight(&font->stbInfo, fontHeight);
                const float bearing = scale * float(codepointInfo->leftSideBearingUnscaled);
                const int codepointSigned = font_group::direct_cast_to_int(codepoint);
                int x0;
                int y0;
                int x1;
                int y1;
                stbtt_GetCodepointBitmapBox(&font->stbInfo, codepointSigned, scale, scale, &x0, &y0, &x1, &y1);
                const float width = float(x1 - x0);
                const float height = float(y1 - y0);
                const float additionalAdvance = scale * float(stbtt_GetCodepointKernAdvance(&font->stbInfo, previousCodepoint, codepointSigned));
                positionX += additionalAdvance;
                const float bottomLeftX = positionX + float(x0) - bearing;
                const float bottomLeftY = baselineY - float(y1);
                const float topRightX = bottomLeftX + width;
                const float topRightY = bottomLeftY + height;
                const RenderAtlasRectNormalized& rect = fontGroup->atlas.uvRects[codepointInfo->atlasRectIndex];
                push_quad(colorIndex, { bottomLeftX, bottomLeftY, topRightX, topRightY }, rect.p1x, rect.p1y, rect.p2x, rect.p2y);
                positionX += scale * float(codepointInfo->advanceWidthUnscaled);
                previousCodepoint = codepointSigned;
            }
            return positionX - baselineX;
        }

        void reset_work_region()
        {
            g_uiCtx.hasWorkRegion   = false;
            g_uiCtx.workRegion.blX  = 0.0f;
            g_uiCtx.workRegion.blY  = 0.0f;
            g_uiCtx.workRegion.trX  = win::get_width<float>();
            g_uiCtx.workRegion.trY  = win::get_height<float>();
        }

        void set_work_region(const UiQuadCoords& quad)
        {
            g_uiCtx.hasWorkRegion = true;
            g_uiCtx.workRegion = quad;
        }

        float get_current_font_ascent()
        {
            const float fontHeight = g_uiCtx.currentParams[SeUiParam::FONT_HEIGHT].dim;
            const FontGroup* const group = g_uiCtx.currentFontGroup;
            // Same as stbtt_ScaleForPixelHeight
            const float scale = fontHeight / float(group->ascentUnscaled - group->descentUnscaled);
            return float(group->ascentUnscaled) * scale;
        }

        float get_current_font_descent()
        {
            const float fontHeight = g_uiCtx.currentParams[SeUiParam::FONT_HEIGHT].dim;
            const FontGroup* const group = g_uiCtx.currentFontGroup;
            // Same as stbtt_ScaleForPixelHeight
            const float scale = fontHeight / float(group->ascentUnscaled - group->descentUnscaled);
            return float(group->descentUnscaled) * scale;
        }
    }

    bool begin(const SeUiBeginInfo& info)
    {
        g_uiCtx.target            = info.target;
        g_uiCtx.currentFontGroup  = nullptr;
        memcpy(g_uiCtx.currentParams, DEFAULT_PARAMS, sizeof(UiParams));

        dynamic_array::construct(g_uiCtx.frameDrawCalls, allocators::frame(), 16);
        dynamic_array::construct(g_uiCtx.frameDrawDatas, allocators::frame(), 16);
        dynamic_array::construct(g_uiCtx.frameVertices, allocators::frame(), 512);
        dynamic_array::construct(g_uiCtx.frameColors, allocators::frame(), 16);

        const SeMousePos mousePos = win::get_mouse_pos();
        const float mouseX = float(mousePos.x);
        const float mouseY = float(mousePos.y);
        g_uiCtx.mouseDeltaX       = mouseX - g_uiCtx.mouseX;
        g_uiCtx.mouseDeltaY       = mouseY - g_uiCtx.mouseY;
        g_uiCtx.mouseX            = mouseX;
        g_uiCtx.mouseY            = mouseY;
        g_uiCtx.isMouseDown       = win::is_mouse_button_pressed(SeMouse::LMB);
        g_uiCtx.isMouseJustDown   = win::is_mouse_button_just_pressed(SeMouse::LMB);
        g_uiCtx.isJustActivated =
            utils::compare(g_uiCtx.previousActiveObjectUid, SeString{ }) &&
            !utils::compare(g_uiCtx.currentActiveObjectUid, SeString{ });
        g_uiCtx.previousHoveredObjectUid    = g_uiCtx.currentHoveredObjectUid;
        g_uiCtx.previousHoveredWindowUid    = g_uiCtx.currentHoveredWindowUid;
        g_uiCtx.previousActiveObjectUid     = g_uiCtx.currentActiveObjectUid;
        g_uiCtx.currentHoveredObjectUid     = { };
        g_uiCtx.currentHoveredWindowUid     = { };
        g_uiCtx.currentWindowUid            = { };
        impl::reset_work_region();
        
        if (!g_uiCtx.isMouseDown)
        {
            g_uiCtx.currentActiveObjectUid = { };
        }
        else
        {
            UiObjectData* const activeObject = hash_table::get(g_uiCtx.uidToObjectData, g_uiCtx.currentActiveObjectUid);
            if (activeObject)
            {
                const float currentWidth = activeObject->quad.trX - activeObject->quad.blX;
                const float currentHeight = activeObject->quad.trY - activeObject->quad.blY;
                se_assert(currentWidth >= 0.0f);
                se_assert(currentHeight >= 0.0f);
                const bool isPositiveDeltaX = g_uiCtx.mouseDeltaX >= 0.0f;
                const bool isPositiveDeltaY = g_uiCtx.mouseDeltaY >= 0.0f;
                const float maxDeltaTowardsCenterX = (activeObject->quad.trX - activeObject->quad.blX) - activeObject->minWidth;
                const float maxDeltaTowardsCenterY = (activeObject->quad.trY - activeObject->quad.blY) - activeObject->minHeight;
                se_assert(maxDeltaTowardsCenterX >= 0.0f);
                se_assert(maxDeltaTowardsCenterY >= 0.0f);
                if ((activeObject->settingsFlags & SeUiFlags::MOVABLE) && (activeObject->actionFlags & UiActionFlags::CAN_BE_MOVED))
                {
                    activeObject->quad.blX = activeObject->quad.blX + g_uiCtx.mouseDeltaX;
                    activeObject->quad.blY = activeObject->quad.blY + g_uiCtx.mouseDeltaY;
                    activeObject->quad.trX = activeObject->quad.trX + g_uiCtx.mouseDeltaX;
                    activeObject->quad.trY = activeObject->quad.trY + g_uiCtx.mouseDeltaY;
                }
                if ((activeObject->settingsFlags & SeUiFlags::RESIZABLE_X) && (activeObject->actionFlags & UiActionFlags::CAN_BE_RESIZED_X_LEFT))
                {
                    const bool canChangeSize = isPositiveDeltaX ? g_uiCtx.mouseX > activeObject->quad.blX : g_uiCtx.mouseX < activeObject->quad.blX;
                    const float actualDelta = canChangeSize
                        ? (g_uiCtx.mouseDeltaX > maxDeltaTowardsCenterX ? maxDeltaTowardsCenterX : g_uiCtx.mouseDeltaX)
                        : 0.0f;
                    activeObject->quad.blX = activeObject->quad.blX + actualDelta;
                }
                if ((activeObject->settingsFlags & SeUiFlags::RESIZABLE_X) && (activeObject->actionFlags & UiActionFlags::CAN_BE_RESIZED_X_RIGHT))
                {
                    const bool canChangeSize = isPositiveDeltaX ? g_uiCtx.mouseX > activeObject->quad.trX : g_uiCtx.mouseX < activeObject->quad.trX;
                    const float actualDelta = canChangeSize
                        ? (g_uiCtx.mouseDeltaX < -maxDeltaTowardsCenterX ? -maxDeltaTowardsCenterX : g_uiCtx.mouseDeltaX)
                        : 0.0f;
                    activeObject->quad.trX = activeObject->quad.trX + actualDelta;
                }
                if ((activeObject->settingsFlags & SeUiFlags::RESIZABLE_Y) && (activeObject->actionFlags & UiActionFlags::CAN_BE_RESIZED_Y_TOP))
                {
                    const bool canChangeSize = isPositiveDeltaY ? g_uiCtx.mouseY > activeObject->quad.trY : g_uiCtx.mouseY < activeObject->quad.trY;
                    const float actualDelta = canChangeSize
                        ? (g_uiCtx.mouseDeltaY < -maxDeltaTowardsCenterY ? -maxDeltaTowardsCenterY : g_uiCtx.mouseDeltaY)
                        : 0.0f;
                    activeObject->quad.trY = activeObject->quad.trY + actualDelta;
                }
                if ((activeObject->settingsFlags & SeUiFlags::RESIZABLE_Y) && (activeObject->actionFlags & UiActionFlags::CAN_BE_RESIZED_Y_BOTTOM))
                {
                    const bool canChangeSize = isPositiveDeltaY ? g_uiCtx.mouseY > activeObject->quad.blY : g_uiCtx.mouseY < activeObject->quad.blY;
                    const float actualDelta = canChangeSize
                        ? (g_uiCtx.mouseDeltaY > maxDeltaTowardsCenterY ? maxDeltaTowardsCenterY : g_uiCtx.mouseDeltaY)
                        : 0.0f;
                    activeObject->quad.blY = activeObject->quad.blY + actualDelta;
                }
                if ((activeObject->settingsFlags & SeUiFlags::SCROLLABLE_Y) && (activeObject->actionFlags & UiActionFlags::CAN_BE_SCROLLED_BY_MOUSE_MOVE_Y))
                {
                    const bool canMove = g_uiCtx.mouseY <= activeObject->quad.trY && g_uiCtx.mouseY >= activeObject->quad.blY;
                    if (canMove)
                    {
                        const float barFrom = activeObject->scrollInfoY.absoluteScrollBarFrom; // top
                        const float barTo = activeObject->scrollInfoY.absoluteScrollBarTo; // bottom
                        const float barCurrent = activeObject->scrollInfoY.absoluteScrollBarPosition;
                        const float barNew = se_clamp(barCurrent + g_uiCtx.mouseDeltaY, barTo, barFrom);
                        activeObject->scrollInfoY.normalizedScrollPosition = se_saturate(se_safe_divide((barFrom - barNew), (barFrom - barTo), 0.0f));
                    }
                }
                if ((activeObject->settingsFlags & SeUiFlags::SCROLLABLE_X) && (activeObject->actionFlags & UiActionFlags::CAN_BE_SCROLLED_BY_MOUSE_MOVE_X))
                {
                    const bool canMove = g_uiCtx.mouseX <= activeObject->quad.trX && g_uiCtx.mouseX >= activeObject->quad.blX;
                    if (canMove)
                    {
                        const float barFrom = activeObject->scrollInfoX.absoluteScrollBarFrom; // left
                        const float barTo = activeObject->scrollInfoX.absoluteScrollBarTo; // right
                        const float barCurrent = activeObject->scrollInfoX.absoluteScrollBarPosition;
                        const float barNew = se_clamp(barCurrent + g_uiCtx.mouseDeltaX, barFrom, barTo);
                        activeObject->scrollInfoX.normalizedScrollPosition = se_saturate(se_safe_divide((barNew - barFrom), (barTo - barFrom), 0.0f));
                    }
                }
            }
        }

        UiObjectData* const hoveredWindow = hash_table::get(g_uiCtx.uidToObjectData, g_uiCtx.previousHoveredWindowUid);
        if (hoveredWindow)
        {
            const float mouseWheel = float(win::get_mouse_wheel());
            if (mouseWheel)
            {
                constexpr const float SCROLL_SENSITIVITY = 4.0f;
                const float scrollCurrent = hoveredWindow->scrollInfoY.absoluteScrollPosition;
                const float scrollMax = hoveredWindow->scrollInfoY.absoluteScrollRange;
                const float scrollNew = se_clamp(scrollCurrent - (mouseWheel * SCROLL_SENSITIVITY), 0.0f, scrollMax);
                hoveredWindow->scrollInfoY.normalizedScrollPosition = se_saturate(se_safe_divide(scrollNew, scrollMax, 0.0f));
            }
        }

        return true;
    }

    SePassDependencies end(SePassDependencies dependencies)
    {
        const SePassDependencies result = render::begin_graphics_pass
        ({
            .dependencies           = dependencies,
            .vertexProgram          = { .program = g_uiCtx.drawUiVs, },
            .fragmentProgram        = { .program = g_uiCtx.drawUiFs, },
            .frontStencilOpState    = { .isEnabled = false, },
            .backStencilOpState     = { .isEnabled = false, },
            .depthState             = { .isTestEnabled = false, .isWriteEnabled = false, },
            .polygonMode            = SePipelinePolygonMode::FILL,
            .cullMode               = SePipelineCullMode::NONE,
            .frontFace              = SePipelineFrontFace::CLOCKWISE,
            .samplingType           = SeSamplingType::_1,
            .renderTargets          = { g_uiCtx.target },
        });
        //
        // Bind stuff
        //
        const SeBufferRef projectionBuffer = render::scratch_memory_buffer
        ({
            data_provider::from_memory
            (
                float4x4::transposed(render::orthographic(0, win::get_width<float>(), 0, win::get_height<float>(), 0, 2))
            )
        });
        const SeBufferRef drawDatasBuffer = render::scratch_memory_buffer
        ({
            data_provider::from_memory(dynamic_array::raw(g_uiCtx.frameDrawDatas), dynamic_array::raw_size(g_uiCtx.frameDrawDatas))
        });
        const SeBufferRef verticesBuffer = render::scratch_memory_buffer
        ({
            data_provider::from_memory(dynamic_array::raw(g_uiCtx.frameVertices), dynamic_array::raw_size(g_uiCtx.frameVertices))
        });
        const SeBufferRef colorBuffer = render::scratch_memory_buffer
        ({
            data_provider::from_memory(dynamic_array::raw(g_uiCtx.frameColors), dynamic_array::raw_size(g_uiCtx.frameColors))
        });
        render::bind
        ({
            .set = 0,
            .bindings =
            {
                { .binding = 0, .type = SeBinding::BUFFER, .buffer = { projectionBuffer } },
                { .binding = 1, .type = SeBinding::BUFFER, .buffer = { verticesBuffer } },
                { .binding = 2, .type = SeBinding::BUFFER, .buffer = { colorBuffer } },
            }
        });
        //
        // Process draw calls
        //
        se_assert(dynamic_array::size(g_uiCtx.frameDrawCalls) == dynamic_array::size(g_uiCtx.frameDrawDatas));
        const size_t numDrawCalls = dynamic_array::size(g_uiCtx.frameDrawCalls);
        for (auto it : g_uiCtx.frameDrawCalls)
        {
            const UiDrawCall& drawCall = iter::value(it);
            const size_t drawCallIndex = iter::index(it);
            render::bind
            ({
                .set = 1,
                .bindings =
                {
                    { .binding = 0, .type = SeBinding::TEXTURE, .texture = { drawCall.texture, g_uiCtx.sampler } },
                    { .binding = 1, .type = SeBinding::BUFFER, .buffer = {
                        .buffer = drawDatasBuffer,
                        .offset = drawCallIndex * sizeof(UiRenderDrawData),
                        .size   = sizeof(UiRenderDrawData),
                    } },
                },
            });
            const uint32_t firstVertexIndex = g_uiCtx.frameDrawDatas[iter::index(it)].firstVertexIndex;
            const uint32_t lastVertexIndex = drawCallIndex == (numDrawCalls - 1)
                ? dynamic_array::size<uint32_t>(g_uiCtx.frameVertices) - 1
                : g_uiCtx.frameDrawDatas[drawCallIndex + 1].firstVertexIndex;
            render::draw
            ({
                .numVertices = lastVertexIndex - firstVertexIndex + 1,
                .numInstances = 1,
            });
        }
        render::end_pass();
        //
        // Clear context
        //
        dynamic_array::destroy(g_uiCtx.frameDrawCalls);
        dynamic_array::destroy(g_uiCtx.frameDrawDatas);
        dynamic_array::destroy(g_uiCtx.frameVertices);
        dynamic_array::destroy(g_uiCtx.frameColors);
        return result;
    }

    void set_font_group(const SeUiFontGroupInfo& info)
    {
        const FontGroup* fontGroup = hash_table::get(g_uiCtx.fontGroups, info);
        if (!fontGroup)
        {
            const FontInfo* fonts[SeUiFontGroupInfo::MAX_FONTS];
            size_t fontIt = 0;
            for (; fontIt < SeUiFontGroupInfo::MAX_FONTS; fontIt++)
            {
                const DataProvider& fontData = info.fonts[fontIt];
                if (!data_provider::is_valid(fontData))
                {
                    break;
                }
                const FontInfo* font = hash_table::get(g_uiCtx.fontInfos, fontData);
                if (!font)
                {
                    font = hash_table::set(g_uiCtx.fontInfos, fontData, font_info::create(fontData));
                }
                se_assert(font);
                fonts[fontIt] = font;
            };
            fontGroup = hash_table::set(g_uiCtx.fontGroups, info, font_group::create(fonts, fontIt));
        }
        se_assert(fontGroup);
        g_uiCtx.currentFontGroup = fontGroup;
    }

    void set_param(SeUiParam::Type type, const SeUiParam& param)
    {
        g_uiCtx.currentParams[type] = param;
    }

    void text(const SeUiTextInfo& info)
    {
        const float fontHeight = g_uiCtx.currentParams[SeUiParam::FONT_HEIGHT].dim;
        if (g_uiCtx.hasWorkRegion)
        {
            UiObjectData* const window = hash_table::get(g_uiCtx.uidToObjectData, g_uiCtx.currentWindowUid);

            const float lineGap = g_uiCtx.currentParams[SeUiParam::FONT_LINE_GAP].dim;
            const float baselineX = window
                ? g_uiCtx.workRegion.blX - window->scrollInfoX.absoluteScrollPosition
                : g_uiCtx.workRegion.blX;
            const float baselineY = window
                ? window->workRegion.trY - window->scrollInfoY.absoluteContentSize - impl::get_current_font_ascent() + window->scrollInfoY.absoluteScrollPosition
                : g_uiCtx.workRegion.trY - impl::get_current_font_ascent();
            const float lineLowestPoint = baselineY + impl::get_current_font_descent();

            float textWidth = 0.0f;
            if (lineLowestPoint < g_uiCtx.workRegion.trY)
            {
                textWidth = impl::draw_text_at_pos(info.utf8text, baselineX, baselineY);
                g_uiCtx.workRegion.trY = lineLowestPoint;
            }
            else
            {
                const UiQuadCoords textBbox = impl::get_text_bbox(info.utf8text);
                textWidth = textBbox.trX - textBbox.blX;
            }
            
            if (window)
            {
                window->scrollInfoX.absoluteContentSize = se_max(window->scrollInfoX.absoluteContentSize, textWidth);
                window->scrollInfoY.absoluteContentSize += fontHeight + lineGap;
            }
        }
        else
        {
            float baselineX = g_uiCtx.currentParams[SeUiParam::PIVOT_POSITION_X].dim;
            float baselineY = g_uiCtx.currentParams[SeUiParam::PIVOT_POSITION_Y].dim;
            const auto [x1, y1, x2, y2] = impl::get_text_bbox(info.utf8text);
            if (g_uiCtx.currentParams[SeUiParam::PIVOT_TYPE_X].enumeration == SeUiPivotType::CENTER) baselineX -= (x2 - x1) / 2.0f;
            if (g_uiCtx.currentParams[SeUiParam::PIVOT_TYPE_Y].enumeration == SeUiPivotType::CENTER) baselineY -= fontHeight / 2.0f;
            impl::draw_text_at_pos(info.utf8text, baselineX, baselineY);
        }
    }

    bool begin_window(const SeUiWindowInfo& info)
    {
        se_assert_msg(!g_uiCtx.hasWorkRegion, "Can't begin new window - another window or region is currently active");

        //
        // Get object data, update quad and settings
        //
        auto [uid, data, isFirst] = impl::object_data_get(info.uid);
        const bool getQuadFromData = (info.flags & (SeUiFlags::RESIZABLE_X | SeUiFlags::RESIZABLE_Y)) && !isFirst;
        const UiQuadCoords quad = getQuadFromData
            ? data->quad
            : impl::get_corners(info.width, info.height);
        if (!getQuadFromData)
        {
            data->quad = quad;
        }
        data->settingsFlags = info.flags;
        
        const bool hasScrollX = info.flags & SeUiFlags::SCROLLABLE_X;
        const bool hasScrollY = info.flags & SeUiFlags::SCROLLABLE_Y;
        const float topPanelThickness = g_uiCtx.currentParams[SeUiParam::WINDOW_TOP_PANEL_THICKNESS].dim;
        const float borderThickness = g_uiCtx.currentParams[SeUiParam::WINDOW_BORDER_THICKNESS].dim;
        const float scrollThickness = g_uiCtx.currentParams[SeUiParam::WINDOW_SCROLL_THICKNESS].dim;
        const float padding = g_uiCtx.currentParams[SeUiParam::WINDOW_INNER_PADDING].dim;

        data->workRegion =
        {
            quad.blX + borderThickness + padding,
            quad.blY + borderThickness + padding + (hasScrollX ? scrollThickness : 0.0f),
            quad.trX - borderThickness - padding - (hasScrollY ? scrollThickness : 0.0f),
            quad.trY - topPanelThickness - padding,
        };

        const float windowWorkRegionWidth   = data->workRegion.trX - data->workRegion.blX;
        const float windowWorkRegionHeight  = data->workRegion.trY - data->workRegion.blY;

        //
        // Scroll stuff
        //
        constexpr const float MIN_SCROLL_BAR_SIZE = 5.0f;

        const float absoluteScrollRangeX    = se_max(0.0f, data->scrollInfoX.absoluteContentSize - windowWorkRegionWidth);
        const float absoluteScrollRangeY    = se_max(0.0f, data->scrollInfoY.absoluteContentSize - windowWorkRegionHeight);
        const float absoluteScrollPositionX = absoluteScrollRangeX * data->scrollInfoX.normalizedScrollPosition;
        const float absoluteScrollPositionY = absoluteScrollRangeY * data->scrollInfoY.normalizedScrollPosition;

        const float scrollBarRangeLeftX     = quad.blX + borderThickness;
        const float scrollBarRangeRightX    = quad.trX - borderThickness - scrollThickness;
        const float scrollBarRangeBottomY   = quad.blY + borderThickness + scrollThickness;
        const float scrollBarRangeTopY      = quad.trY - topPanelThickness;

        const float maxScrollBarSizeX   = scrollBarRangeRightX - scrollBarRangeLeftX;
        const float maxScrollBarSizeY   = scrollBarRangeTopY - scrollBarRangeBottomY;
        const float scrollBarScaleX     = data->scrollInfoX.absoluteContentSize ? se_saturate(windowWorkRegionWidth / data->scrollInfoX.absoluteContentSize) : 1.0f;
        const float scrollBarScaleY     = data->scrollInfoY.absoluteContentSize ? se_saturate(windowWorkRegionHeight / data->scrollInfoY.absoluteContentSize) : 1.0f;
        const float scrollBarSizeX      = se_max(maxScrollBarSizeX * scrollBarScaleX, MIN_SCROLL_BAR_SIZE);
        const float scrollBarSizeY      = se_max(maxScrollBarSizeY * scrollBarScaleY, MIN_SCROLL_BAR_SIZE);
        const float scrollBarPositionX  = scrollBarRangeLeftX + ((maxScrollBarSizeX - scrollBarSizeX) * data->scrollInfoX.normalizedScrollPosition);
        const float scrollBarPositionY  = scrollBarRangeTopY - ((maxScrollBarSizeY - scrollBarSizeY) * data->scrollInfoY.normalizedScrollPosition);

        data->scrollInfoX.absoluteContentSize       = 0.0f;
        data->scrollInfoX.absoluteScrollRange       = absoluteScrollRangeX;
        data->scrollInfoX.absoluteScrollPosition    = absoluteScrollPositionX;
        data->scrollInfoX.absoluteScrollBarFrom     = scrollBarRangeLeftX;
        data->scrollInfoX.absoluteScrollBarTo       = scrollBarRangeRightX - scrollBarSizeX;
        data->scrollInfoX.absoluteScrollBarPosition = scrollBarPositionX;

        data->scrollInfoY.absoluteContentSize       = 0.0f;
        data->scrollInfoY.absoluteScrollRange       = absoluteScrollRangeY;
        data->scrollInfoY.absoluteScrollPosition    = absoluteScrollPositionY;
        data->scrollInfoY.absoluteScrollBarFrom     = scrollBarRangeTopY;
        data->scrollInfoY.absoluteScrollBarTo       = scrollBarRangeBottomY + scrollBarSizeY;
        data->scrollInfoY.absoluteScrollBarPosition = scrollBarPositionY;

        data->minWidth = borderThickness * 2.0f + padding * 2.0f + scrollThickness + MIN_SCROLL_BAR_SIZE;
        data->minHeight = topPanelThickness + borderThickness + padding * 2.0f + scrollThickness + MIN_SCROLL_BAR_SIZE;

        const UiQuadCoords scrollBarQuadX
        {
            scrollBarPositionX,
            quad.blY + borderThickness,
            scrollBarPositionX + scrollBarSizeX,
            quad.blY + borderThickness + scrollThickness,
        };

        const UiQuadCoords scrollBarQuadY
        {
            quad.trX - borderThickness - scrollThickness,
            scrollBarPositionY - scrollBarSizeY,
            quad.trX - borderThickness,
            scrollBarPositionY,
        };

        //
        // Update other data stuff, active object uid and work region info
        //
        if (impl::is_under_cursor(quad))
        {
            g_uiCtx.currentHoveredObjectUid = uid;
            g_uiCtx.currentHoveredWindowUid = uid;
            if (g_uiCtx.isMouseJustDown)
            {
                static constexpr float BORDER_PANEL_TOLERANCE = 2.0f;

                g_uiCtx.currentActiveObjectUid = uid;

                const bool isTopPanelUnderCursor = ((quad.trY - topPanelThickness) <= g_uiCtx.mouseY);
                const bool isLeftBorderUnderCursor = !isTopPanelUnderCursor && ((quad.blX + borderThickness + BORDER_PANEL_TOLERANCE) >= g_uiCtx.mouseX);
                const bool isRightBorderUnderCursor = !isTopPanelUnderCursor && ((quad.trX - borderThickness - BORDER_PANEL_TOLERANCE) <= g_uiCtx.mouseX);
                const bool isBottomBorderUnderCursor = !isTopPanelUnderCursor && ((quad.blY + borderThickness + BORDER_PANEL_TOLERANCE) >= g_uiCtx.mouseY);
                const bool isAnyBorderUnderCursor = isTopPanelUnderCursor | isLeftBorderUnderCursor | isRightBorderUnderCursor | isBottomBorderUnderCursor;
                const bool isHorizontalScrollUnderCursor = !isAnyBorderUnderCursor && impl::is_under_cursor(scrollBarQuadX);
                const bool isVerticalScrollUnderCursor = !isAnyBorderUnderCursor && impl::is_under_cursor(scrollBarQuadY);

                if (isTopPanelUnderCursor) data->actionFlags |= UiActionFlags::CAN_BE_MOVED;
                if (isLeftBorderUnderCursor) data->actionFlags |= UiActionFlags::CAN_BE_RESIZED_X_LEFT;
                if (isRightBorderUnderCursor) data->actionFlags |= UiActionFlags::CAN_BE_RESIZED_X_RIGHT;
                if (isBottomBorderUnderCursor) data->actionFlags |= UiActionFlags::CAN_BE_RESIZED_Y_BOTTOM;
                if (isHorizontalScrollUnderCursor) data->actionFlags |= UiActionFlags::CAN_BE_SCROLLED_BY_MOUSE_MOVE_X;
                if (isVerticalScrollUnderCursor) data->actionFlags |= UiActionFlags::CAN_BE_SCROLLED_BY_MOUSE_MOVE_Y;
            }
            else if (!g_uiCtx.isMouseDown)
            {
                data->actionFlags = 0;
            }
        }

        //
        // Draw top panel and borders
        //
        const uint32_t colorIndexBorders = impl::set_draw_call
        ({
            .tint = col::unpack(g_uiCtx.currentParams[SeUiParam::PRIMARY_COLOR].color),
            .mode = UiRenderColorsUnpacked::MODE_SIMPLE,
        });
        impl::push_quad(colorIndexBorders, { quad.blX, quad.trY - topPanelThickness, quad.trX, quad.trY });
        impl::push_quad(colorIndexBorders, { quad.blX, quad.blY, quad.blX + borderThickness, quad.trY - topPanelThickness });
        impl::push_quad(colorIndexBorders, { quad.trX - borderThickness, quad.blY, quad.trX, quad.trY - topPanelThickness });
        impl::push_quad(colorIndexBorders, { quad.blX + borderThickness, quad.blY, quad.trX - borderThickness, quad.blY + borderThickness });

        //
        // Draw background
        //
        const uint32_t colorIndexBackground = impl::set_draw_call
        ({
            .tint = col::unpack(g_uiCtx.currentParams[SeUiParam::SECONDARY_COLOR].color),
            .mode = UiRenderColorsUnpacked::MODE_SIMPLE,
        });
        impl::push_quad(colorIndexBackground, { quad.blX + borderThickness, quad.blY + borderThickness, quad.trX - borderThickness, quad.trY - topPanelThickness });

        //
        // Draw scroll bars
        //
        if (hasScrollY || hasScrollX)
        {
            // Draw scroll bars connection (bottom right square)
            impl::push_quad(colorIndexBorders,
            {
                quad.trX - borderThickness - scrollThickness,
                quad.blY + borderThickness,
                quad.trX - borderThickness,
                quad.blY + borderThickness + scrollThickness
            });
            const uint32_t colorIndexScrollBar = impl::set_draw_call
            ({
                .tint = col::unpack(g_uiCtx.currentParams[SeUiParam::ACCENT_COLOR].color),
                .mode = UiRenderColorsUnpacked::MODE_SIMPLE,
            });
            if (hasScrollX)
            {
                impl::push_quad(colorIndexScrollBar, scrollBarQuadX);
            }
            if (hasScrollY)
            {
                impl::push_quad(colorIndexScrollBar, scrollBarQuadY);
            }
        }

        //
        // Set window ui and region
        //
        g_uiCtx.currentWindowUid = uid;
        impl::set_work_region(data->workRegion);

        return true;
    }

    void end_window()
    {
        g_uiCtx.currentWindowUid = { };
        impl::reset_work_region();
    }

    bool begin_region(const SeUiRegionInfo& info)
    {
        se_assert_msg(!g_uiCtx.hasWorkRegion, "Can't begin new region - another window or region is currently active");

        const UiQuadCoords quad = impl::get_corners(info.width, info.height);
        if (info.useDebugColor)
        {
            const uint32_t debugColorIndex = impl::set_draw_call
            ({
                .tint = col::unpack(g_uiCtx.currentParams[SeUiParam::ACCENT_COLOR].color),
                .mode = UiRenderColorsUnpacked::MODE_SIMPLE,
            });
            impl::push_quad(debugColorIndex, quad);
        }
        impl::set_work_region(quad);

        return true;
    }

    void end_region()
    {
        impl::reset_work_region();
    }

    bool button(const SeUiButtonInfo& info)
    {
        auto [uid, data, isFirst] = impl::object_data_get(info.uid);
        //
        // Get button size
        //
        float buttonWidth = info.width;
        float buttonHeight = info.height;
        float textBottomOffsetY = 0.0f;
        float textWidth = 0.0f;
        if (info.utf8text)
        {
            const float fontHeight = g_uiCtx.currentParams[SeUiParam::FONT_HEIGHT].dim;
            const float border = g_uiCtx.currentParams[SeUiParam::BUTTON_BORDER_SIZE].dim;
            const auto [bbx1, bby1, bbx2, bby2] = impl::get_text_bbox(info.utf8text);
            textWidth = bbx2 - bbx1;
            const float textWidthWithBorder = textWidth + border * 2.0f;
            const float textHeightWithBorder = fontHeight + border * 2.0f;
            if (textWidthWithBorder > buttonWidth) buttonWidth = textWidthWithBorder;
            if (textHeightWithBorder > buttonHeight) buttonHeight = textHeightWithBorder;
            textBottomOffsetY = -impl::get_current_font_descent() + border;
        }
        const UiQuadCoords quad = impl::get_corners(buttonWidth, buttonHeight);
        //
        // Update state
        //
        const bool isUnderCursor = impl::is_under_cursor(quad) && (g_uiCtx.hasWorkRegion ? impl::is_under_cursor(g_uiCtx.workRegion) : true);
        if (isUnderCursor)
        {
            g_uiCtx.currentHoveredObjectUid = uid;
            if (g_uiCtx.isMouseJustDown)
            {
                g_uiCtx.currentActiveObjectUid = uid;
            }
        }
        const bool isPressed = utils::compare(g_uiCtx.previousActiveObjectUid, uid);
        const bool isClicked = isPressed && g_uiCtx.isJustActivated;
        if (isClicked)
        {
            if (data->stateFlags & (1 << UiStateFlags::IS_TOGGLED))
                data->stateFlags &= ~(1 << UiStateFlags::IS_TOGGLED);
            else
                data->stateFlags |= 1 << UiStateFlags::IS_TOGGLED;
        }
        const bool isToggled = data->stateFlags & (1 << UiStateFlags::IS_TOGGLED);
        //
        // Draw button
        //
        SeColorUnpacked buttonColor = col::unpack(g_uiCtx.currentParams[SeUiParam::PRIMARY_COLOR].color);
        if (isPressed)
        {
            const float SCALE = 0.5f;
            buttonColor.r *= SCALE;
            buttonColor.g *= SCALE;
            buttonColor.b *= SCALE;
        }
        const uint32_t colorIndexButton = impl::set_draw_call({ buttonColor, UiRenderColorsUnpacked::MODE_SIMPLE });
        impl::push_quad(colorIndexButton, quad);
        //
        // Draw text
        //
        if (info.utf8text)
        {
            const float posX = quad.blX + buttonWidth / 2.0f - textWidth / 2.0f;
            const float posY = quad.blY + textBottomOffsetY;
            impl::draw_text_at_pos(info.utf8text, posX, posY);
        }
        //
        // Update data
        //
        if (g_uiCtx.hasWorkRegion)
        {
            g_uiCtx.workRegion.trY = se_min(g_uiCtx.workRegion.trY, quad.blY);
        }
        UiObjectData* const window = hash_table::get(g_uiCtx.uidToObjectData, g_uiCtx.currentWindowUid);
        if (window)
        {
            window->scrollInfoX.absoluteContentSize = se_max(window->scrollInfoX.absoluteContentSize, buttonWidth);
            window->scrollInfoY.absoluteContentSize += buttonHeight;
        }

        return
            (info.mode == SeUiButtonMode::HOLD && isPressed) ||
            (info.mode == SeUiButtonMode::TOGGLE && isToggled) ||
            (info.mode == SeUiButtonMode::CLICK && isClicked);
    }

    void engine::init()
    {
        g_uiCtx = { };
        g_uiCtx.drawUiVs = render::program({ data_provider::from_file("ui_draw.vert.spv") });
        g_uiCtx.drawUiFs = render::program({ data_provider::from_file("ui_draw.frag.spv") });
        g_uiCtx.sampler = render::sampler
        ({
            .magFilter          = SeSamplerFilter::LINEAR,
            .minFilter          = SeSamplerFilter::LINEAR,
            .addressModeU       = SeSamplerAddressMode::REPEAT,
            .addressModeV       = SeSamplerAddressMode::REPEAT,
            .addressModeW       = SeSamplerAddressMode::REPEAT,
            .mipmapMode         = SeSamplerMipmapMode::LINEAR,
            .mipLodBias         = 0.0f,
            .minLod             = 0.0f,
            .maxLod             = 0.0f,
            .anisotropyEnable   = false,
            .maxAnisotropy      = 0.0f,
            .compareEnabled     = false,
            .compareOp          = SeCompareOp::ALWAYS,
        });
        
        hash_table::construct(g_uiCtx.fontInfos, allocators::persistent());
        hash_table::construct(g_uiCtx.fontGroups, allocators::persistent());
        hash_table::construct(g_uiCtx.uidToObjectData, allocators::persistent());
    }

    void engine::terminate()
    {
        for (auto it : g_uiCtx.uidToObjectData) string::destroy(iter::key(it));
        hash_table::destroy(g_uiCtx.uidToObjectData);

        for (auto it : g_uiCtx.fontGroups) font_group::destroy(iter::value(it));
        hash_table::destroy(g_uiCtx.fontGroups);

        for (auto it : g_uiCtx.fontInfos) font_info::destroy(iter::value(it));
        hash_table::destroy(g_uiCtx.fontInfos);
    }
}
