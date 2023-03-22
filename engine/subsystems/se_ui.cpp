
#include "se_ui.hpp"
#include "engine/se_engine.hpp"

#define STBTT_malloc(size, userData) ((void)(userData), _se_ui_stbtt_malloc(size))
void* _se_ui_stbtt_malloc(size_t size)
{
    SeAllocatorBindings allocator = se_allocator_frame();
    return allocator.alloc(allocator.allocator, size, se_default_alignment, se_alloc_tag);
}

#define STBTT_free(ptr, userData) ((void)(userData), _se_ui_stbtt_free(ptr))
void _se_ui_stbtt_free(void* ptr)
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

struct SeRenderAtlasRectSize
{
    size_t width;
    size_t height;
};

struct SeRenderAtlasRect
{
    size_t p1x;
    size_t p1y;
    size_t p2x;
    size_t p2y;
};

struct SeRenderAtlasRectNormalized
{
    float p1x;
    float p1y;
    float p2x;
    float p2y;
};

template<typename Pixel>
struct SeRenderAtlas
{
    Pixel*                                      bitmap;
    size_t                                      width;
    size_t                                      height;
    SeDynamicArray<SeRenderAtlasRect>           rects;
    SeDynamicArray<SeRenderAtlasRectNormalized> uvRects;
};

struct SeRenderAtlasLayoutBuilder
{
    static constexpr size_t STEP = 256;
    static constexpr size_t BORDER = 1;
    struct Node
    {
        SeRenderAtlasRectSize size;
        SeRenderAtlasRect     rect;
    };
    SeDynamicArray<Node>  occupiedNodes;
    SeDynamicArray<Node>  freeNodes;
    size_t              width;
    size_t              height;
};

template<typename PixelDst, typename PixelSrc>
using SeRenderAtlasPutFunciton = void (*)(const PixelSrc* src, PixelDst* dst);

SeRenderAtlasLayoutBuilder _se_render_atlas_layout_builder_begin()
{
    SeRenderAtlasLayoutBuilder builder
    {
        .occupiedNodes  = { },
        .freeNodes      = { },
        .width          = SeRenderAtlasLayoutBuilder::STEP,
        .height         = SeRenderAtlasLayoutBuilder::STEP,
    };
    se_dynamic_array_construct(builder.occupiedNodes, se_allocator_frame(), 64);
    se_dynamic_array_construct(builder.freeNodes, se_allocator_frame(), 64);
    se_dynamic_array_push(builder.freeNodes,
    {
        .size =
        {
            SeRenderAtlasLayoutBuilder::STEP,
            SeRenderAtlasLayoutBuilder::STEP
        },
        .rect =
        {
            0,
            0,
            SeRenderAtlasLayoutBuilder::STEP,
            SeRenderAtlasLayoutBuilder::STEP,
        },
    });
    return builder;
}

const SeRenderAtlasLayoutBuilder::Node* _se_render_atlas_layout_builder_push(SeRenderAtlasLayoutBuilder& builder, SeRenderAtlasRectSize initialRequirement)
{
    se_assert(initialRequirement.width);
    se_assert(initialRequirement.height);
    const SeRenderAtlasRectSize requirement
    {
        .width = initialRequirement.width + SeRenderAtlasLayoutBuilder::BORDER * 2,
        .height = initialRequirement.height + SeRenderAtlasLayoutBuilder::BORDER * 2,
    };
    const SeRenderAtlasLayoutBuilder::Node* bestNode = nullptr;
    while (!bestNode)
    {
        size_t bestDiff = SIZE_MAX;
        for (auto it : builder.freeNodes)
        {
            const SeRenderAtlasLayoutBuilder::Node& candidate = se_iterator_value(it);
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
            se_dynamic_array_push(builder.freeNodes,
            {
                .size =
                {
                    builder.width + SeRenderAtlasLayoutBuilder::STEP,
                    SeRenderAtlasLayoutBuilder::STEP,
                },
                .rect =
                {
                    0,
                    builder.height,
                    builder.width + SeRenderAtlasLayoutBuilder::STEP,
                    builder.height + SeRenderAtlasLayoutBuilder::STEP,
                },
            });
            se_dynamic_array_push(builder.freeNodes,
            {
                .size =
                {
                    SeRenderAtlasLayoutBuilder::STEP,
                    builder.height,
                },
                .rect =
                {
                    builder.width,
                    0,
                    builder.width + SeRenderAtlasLayoutBuilder::STEP,
                    builder.height,
                },
            });
            builder.width += SeRenderAtlasLayoutBuilder::STEP;
            builder.height += SeRenderAtlasLayoutBuilder::STEP;
        }
    }
    const SeRenderAtlasLayoutBuilder::Node bestNodeCached = *bestNode;
    se_dynamic_array_remove(builder.freeNodes, bestNode);
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
        se_dynamic_array_push(builder.freeNodes,
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
        se_dynamic_array_push(builder.freeNodes,
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
        se_dynamic_array_push(builder.freeNodes,
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
        se_dynamic_array_push(builder.freeNodes,
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
    return &se_dynamic_array_push(builder.occupiedNodes,
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
SeRenderAtlas<Pixel> _se_render_atlas_layout_builder_end(SeRenderAtlasLayoutBuilder& builder)
{
    SeAllocatorBindings allocator = se_allocator_persistent();
    const size_t arraySize = se_dynamic_array_size(builder.occupiedNodes);
    SeDynamicArray<SeRenderAtlasRect> rects = se_dynamic_array_create<SeRenderAtlasRect>(allocator, arraySize);
    SeDynamicArray<SeRenderAtlasRectNormalized> uvRects = se_dynamic_array_create<SeRenderAtlasRectNormalized>(allocator, arraySize);
    for (auto it : builder.occupiedNodes)
    {
        const SeRenderAtlasRect rect = se_iterator_value(it).rect;
        se_dynamic_array_push(rects, rect);
        se_dynamic_array_push(uvRects,
        {
            // @NOTE : here we flip first and second Y coords because uv coordinates start from top left corner, not bottom left
            (float)(rect.p1x + SeRenderAtlasLayoutBuilder::BORDER) / (float)builder.width,
            (float)(rect.p2y - SeRenderAtlasLayoutBuilder::BORDER) / (float)builder.height,
            (float)(rect.p2x - SeRenderAtlasLayoutBuilder::BORDER) / (float)builder.width,
            (float)(rect.p1y + SeRenderAtlasLayoutBuilder::BORDER) / (float)builder.height,
        });
    }
    se_dynamic_array_destroy(builder.occupiedNodes);
    se_dynamic_array_destroy(builder.freeNodes);
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

template<typename Pixel>
void _se_render_atlas_destroy(SeRenderAtlas<Pixel>& atlas)
{
    SeAllocatorBindings allocator = se_allocator_persistent();
    allocator.dealloc(allocator.allocator, atlas.bitmap, atlas.width * atlas.height * sizeof(Pixel));
    se_dynamic_array_destroy(atlas.rects);
    se_dynamic_array_destroy(atlas.uvRects);
}

template<typename PixelDst, typename PixelSrc>
void _se_render_atlas_blit(SeRenderAtlas<PixelDst>& atlas, const SeRenderAtlasRect& rect, const PixelSrc* source, SeRenderAtlasPutFunciton<PixelDst, PixelSrc> put)
{
    const size_t sourceWidth = rect.p2x - rect.p1x - SeRenderAtlasLayoutBuilder::BORDER * 2;
    const size_t sourceHeight = rect.p2y - rect.p1y - SeRenderAtlasLayoutBuilder::BORDER * 2;
    const size_t fromX = rect.p1x + SeRenderAtlasLayoutBuilder::BORDER;
    const size_t fromY = rect.p1y + SeRenderAtlasLayoutBuilder::BORDER;
    for (size_t h = 0; h < sourceHeight; h++)
        for (size_t w = 0; w < sourceWidth; w++)
        {
            const PixelSrc* const p = &source[h * sourceWidth + w];
            PixelDst* const to = &atlas.bitmap[(fromY + h) * atlas.width + fromX + w];
            put(p, to);
        }
}

// =======================================================================
//
// Fonts
//
// =======================================================================

struct SeFontInfo
{
    stbtt_fontinfo          stbInfo;
    SeDynamicArray<uint32_t>  supportedCodepoints;
    void*                   memory;
    size_t                  memorySize;
    int                     ascentUnscaled;
    int                     descentUnscaled;
    int                     lineGapUnscaled;
};

SeFontInfo _se_font_info_create(const SeDataProvider& data)
{
    const auto as_int16     = [](const uint8_t* data) -> int16_t    { return data[0] * 256 + data[1]; };
    const auto as_uint16    = [](const uint8_t* data) -> uint16_t   { return data[0] * 256 + data[1]; };
    const auto as_uint32    = [](const uint8_t* data) -> uint32_t   { return (data[0] << 24) + (data[1] << 16) + (data[2] << 8) + data[3]; };
    const auto as_uint8     = [](const uint8_t* data) -> uint8_t    { return *data; };
    //
    // Load data and save it with persistent allocator (data provider returns frame memory)
    //
    se_assert(se_data_provider_is_valid(data));
    auto [dataMemory, dataSize] = se_data_provider_get(data);
    se_assert(dataMemory);
    SeAllocatorBindings allocator = se_allocator_persistent();
    SeFontInfo result
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
    result.supportedCodepoints = se_dynamic_array_create<uint32_t>(se_allocator_persistent(), 256);
    const uint16_t cmapFormat = as_uint16(charData + mappingTableOffset);
    switch (cmapFormat)
    {
        case 0:
        {
            const uint16_t numBytes = as_uint16(charData + mappingTableOffset + 2);
            se_assert(numBytes == 256);
            const uint8_t* const glythIndices = (const uint8_t*)(charData + mappingTableOffset + 6);
            for (uint32_t codepointIt = 0; codepointIt < numBytes; codepointIt++)
                if (as_uint8(glythIndices + codepointIt)) se_dynamic_array_push(result.supportedCodepoints, codepointIt);
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
                        if (as_uint16(charData + glythIndexOffset)) se_dynamic_array_push(result.supportedCodepoints, (uint32_t)codepointIt);
                    }
                else
                    for (uint16_t codepointIt = startCode; codepointIt <= endCode; codepointIt++)
                    {
                        const int32_t idDelta = as_int16(charData + idDeltaTableOffset + segIt * 2);
                        const uint16_t glythIndex = (uint16_t)(idDelta + (int32_t)codepointIt);
                        se_assert(glythIndex >= 0);
                        if (as_uint16(charData + glythIndicesTableOffset + glythIndex * 2)) se_dynamic_array_push(result.supportedCodepoints, (uint32_t)codepointIt);
                    }
            }
        } break;
        case 6:
        {
            const uint16_t firstCode = as_uint16(charData + mappingTableOffset + 6);
            const uint16_t entryCount = as_uint16(charData + mappingTableOffset + 8);
            const uint8_t* const glythIndicesTable = charData + mappingTableOffset + 10;
            for (uint32_t entryIt = 0; entryIt < entryCount; entryIt++)
                if (as_uint16(glythIndicesTable + (entryIt * 2))) se_dynamic_array_push(result.supportedCodepoints, entryIt + firstCode);
        } break;
        case 10:
        {
            const uint32_t firstCode = as_uint16(charData + mappingTableOffset + 12);
            const uint32_t entryCount = as_uint16(charData + mappingTableOffset + 16);
            const uint8_t* const glythIndicesTable = charData + mappingTableOffset + 20;
            for (uint32_t entryIt = 0; entryIt < entryCount; entryIt++)
                if (as_uint16(glythIndicesTable + (entryIt * 2))) se_dynamic_array_push(result.supportedCodepoints, entryIt + firstCode);
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
                    se_dynamic_array_push(result.supportedCodepoints, codepointIt);
            }
        } break;
        default: { se_assert_msg(false, "Unsupported cmap format : {}", cmapFormat); } break;
    }
    se_assert_msg(se_dynamic_array_size(result.supportedCodepoints), "Font doesn't support any unicode codepoints");
    //
    // Get vmetrics
    //
    stbtt_GetFontVMetrics(&result.stbInfo, &result.ascentUnscaled, &result.descentUnscaled, &result.lineGapUnscaled);

    return result;
}

inline void _se_font_info_destroy(SeFontInfo& font)
{
    SeAllocatorBindings allocator = se_allocator_persistent();
    allocator.dealloc(allocator.allocator, font.memory, font.memorySize);
    se_dynamic_array_destroy(font.supportedCodepoints);
}

// =======================================================================
//
// Codepoint hash table
// Special variation of hash table, optimized for a SeUtf32Char codepoint keys
// Mostly a copy paste from original SeHashTable
//
// =======================================================================

template<typename T>
struct SeCodepointHashTable
{
    struct Entry
    {
        SeUtf32Char key;
        uint32_t isOccupied;
        T value;
    };
    SeAllocatorBindings allocator;
    Entry* memory;
    size_t capacity;
    size_t size;
};

template<typename T>
SeCodepointHashTable<T> _se_codepoint_hash_table_create(SeAllocatorBindings allocator, size_t capacity)
{
    using Entry = SeCodepointHashTable<T>::Entry;
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
void _se_codepoint_hash_table_destroy(SeCodepointHashTable<T>& table)
{
    using Entry = SeCodepointHashTable<T>::Entry;
    const SeAllocatorBindings& allocator = table.allocator;
    allocator.dealloc(allocator.allocator, table.memory, sizeof(Entry) * table.capacity);
}

template<typename T>
T* _se_codepoint_hash_table_set(SeCodepointHashTable<T>& table, SeUtf32Char key, const T& value)
{
    using Entry = SeCodepointHashTable<T>::Entry;
    //
    // Expand if needed
    //
    if (table.size >= (table.capacity / 2))
    {
        const size_t oldCapacity = table.capacity;
        const size_t newCapacity = oldCapacity * 2;
        SeCodepointHashTable<T> newTable = _se_codepoint_hash_table_create<T>(table.allocator, newCapacity);
        for (size_t it = 0; it < oldCapacity; it++)
        {
            Entry& entry = table.memory[it];
            if (entry.isOccupied) _se_codepoint_hash_table_set(newTable, entry.key, entry.value);
        }
        _se_codepoint_hash_table_destroy(table);
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
size_t _se_codepoint_hash_table_index_of(const SeCodepointHashTable<T>& table, SeUtf32Char key)
{
    using Entry = SeCodepointHashTable<T>::Entry;
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
inline T* _se_codepoint_hash_table_get(SeCodepointHashTable<T>& table, SeUtf32Char key)
{
    const size_t position = _se_codepoint_hash_table_index_of(table, key);
    return position != table.capacity ? &table.memory[position].value : nullptr;
}

template<typename T>
inline const T* _se_codepoint_hash_table_get(const SeCodepointHashTable<T>& table, SeUtf32Char key)
{
    const size_t position = _se_codepoint_hash_table_index_of(table, key);
    return position != table.capacity ? &table.memory[position].value : nullptr;
}

template<typename T>
inline size_t _se_codepoint_hash_table_size(SeCodepointHashTable<T>& table)
{
    return table.size;
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
CodepointHashTableIterator<Value, SeCodepointHashTable<Value>> begin(SeCodepointHashTable<Value>& table)
{
    if (table.size == 0) return { &table, table.capacity };
    size_t it = 0;
    while (it < table.capacity && !table.memory[it].isOccupied) { it++; }
    return { &table, it };
}

template<typename Value>
CodepointHashTableIterator<Value, SeCodepointHashTable<Value>> end(SeCodepointHashTable<Value>& table)
{
    return { &table, table.capacity };
}

template<typename Value>
CodepointHashTableIterator<const Value, const SeCodepointHashTable<Value>> begin(const SeCodepointHashTable<Value>& table)
{
    if (table.size == 0) return { &table, table.capacity };
    size_t it = 0;
    while (it < table.capacity && !table.memory[it].isOccupied) { it++; }
    return { &table, it };
}

template<typename Value>
CodepointHashTableIterator<const Value, const SeCodepointHashTable<Value>> end(const SeCodepointHashTable<Value>& table)
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

template<typename Value>
const Value& se_iterator_value(const CodepointHashTableIteratorValue<const Value, const SeCodepointHashTable<Value>>& val)
{
    return val.value;
}

template<typename Value>
Value& se_iterator_value(CodepointHashTableIteratorValue<Value, SeCodepointHashTable<Value>>& val)
{
    return val.value;
}

template<typename Value, typename Table>
SeUtf32Char se_iterator_key(const CodepointHashTableIteratorValue<Value, Table>& val)
{
    return val.key;
}

template<typename Value, typename Table>
size_t se_iterator_index(const CodepointHashTableIteratorValue<Value, Table>& val)
{
    return val.iterator->index;
}

// =======================================================================
//
// Font group
//
// =======================================================================

struct SeFontGroup
{
    struct CodepointInfo
    {
        const SeFontInfo*   font;
        uint32_t            atlasRectIndex;
        int                 advanceWidthUnscaled;
        int                 leftSideBearingUnscaled;
    };

    static constexpr float ATLAS_CODEPOINT_HEIGHT_PIX = 64;

    const SeFontInfo*                       fonts[SeUiFontGroupInfo::MAX_FONTS];
    SeRenderAtlas<uint8_t>                  atlas;
    SeTextureRef                            texture;
    SeCodepointHashTable<CodepointInfo>     codepointToInfo;
    int                                     lineGapUnscaled;
    int                                     ascentUnscaled;
    int                                     descentUnscaled;
};

inline int _se_font_group_direct_cast_to_int(SeUtf32Char value)
{
    return *((int*)&value);
};

SeFontGroup _se_font_group_create(const SeFontInfo** fonts, size_t numFonts)
{
    SeFontGroup groupInfo
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
    SeCodepointHashTable<const SeFontInfo*> codepointToFont = _se_codepoint_hash_table_create<const SeFontInfo*>(se_allocator_frame(), 256);
    for (size_t it = 0; it < numFonts; it++)
    {
        const SeFontInfo* font = fonts[it];
        groupInfo.fonts[it] = font;
        for (auto codepointIt : font->supportedCodepoints)
        {
            const uint32_t codepoint = se_iterator_value(codepointIt);
            if (!_se_codepoint_hash_table_get(codepointToFont, codepoint))
            {
                _se_codepoint_hash_table_set(codepointToFont, codepoint, font);
            }
        }
        groupInfo.lineGapUnscaled = groupInfo.lineGapUnscaled > font->lineGapUnscaled ? groupInfo.lineGapUnscaled : font->lineGapUnscaled;
        groupInfo.ascentUnscaled = groupInfo.ascentUnscaled > font->ascentUnscaled ? groupInfo.ascentUnscaled : font->ascentUnscaled;
        groupInfo.descentUnscaled = groupInfo.descentUnscaled < font->descentUnscaled ? groupInfo.descentUnscaled : font->descentUnscaled;
    }
    const size_t numCodepoints = _se_codepoint_hash_table_size(codepointToFont);
    //
    // Get all codepoint bitmaps and fill codepoint infos
    //
    struct CodepointBitmapInfo
    {
        SeRenderAtlasRectSize size;
        uint8_t* memory;
    };
    SeCodepointHashTable<CodepointBitmapInfo> codepointToBitmap = _se_codepoint_hash_table_create<CodepointBitmapInfo>(se_allocator_frame(), numCodepoints);
    groupInfo.codepointToInfo = _se_codepoint_hash_table_create<SeFontGroup::CodepointInfo>(se_allocator_persistent(), numCodepoints);
    for (auto it : codepointToFont)
    {
        //
        // Bitmap
        //
        const SeUtf32Char codepoint = se_iterator_key(it);
        const SeFontInfo* const font = se_iterator_value(it);
        const float scale = stbtt_ScaleForPixelHeight(&font->stbInfo, SeFontGroup::ATLAS_CODEPOINT_HEIGHT_PIX);
        int width;
        int height;
        int xOff;
        int yOff;
        // @TODO : setup custom malloc and free for stbtt
        unsigned char* bitmap = stbtt_GetCodepointBitmap(&font->stbInfo, scale, scale, _se_font_group_direct_cast_to_int(codepoint), &width, &height, &xOff, &yOff);
        _se_codepoint_hash_table_set(codepointToBitmap, codepoint, { { (size_t)width, (size_t)height }, (uint8_t*)bitmap });
        //
        // Info
        //
        int advanceWidth;
        int leftSideBearing;
        stbtt_GetCodepointHMetrics(&font->stbInfo, _se_font_group_direct_cast_to_int(codepoint), &advanceWidth, &leftSideBearing);
        const SeFontGroup::CodepointInfo codepointInfo
        {
            .font                       = font,
            .atlasRectIndex             = 0,
            .advanceWidthUnscaled       = advanceWidth,
            .leftSideBearingUnscaled    = leftSideBearing,
        };
        _se_codepoint_hash_table_set(groupInfo.codepointToInfo, codepoint, codepointInfo);
    }
    //
    // Build atlas layout
    //
    SeHashTable<SeRenderAtlasRect, uint32_t> atlasRectToCodepoint = se_hash_table_create<SeRenderAtlasRect, uint32_t>(se_allocator_frame(), numCodepoints);
    SeRenderAtlasLayoutBuilder builder = _se_render_atlas_layout_builder_begin();
    for (auto it : codepointToBitmap)
    {
        const CodepointBitmapInfo& bitmap = se_iterator_value(it);
        se_assert((bitmap.size.width == 0) == (bitmap.size.height == 0));
        se_assert((bitmap.memory == nullptr) == (bitmap.size.width == 0));
        const SeRenderAtlasLayoutBuilder::Node* node = _se_render_atlas_layout_builder_push(builder, bitmap.memory ? bitmap.size : SeRenderAtlasRectSize{ 1, 1 });
        se_hash_table_set(atlasRectToCodepoint, node->rect, se_iterator_key(it));
    }
    //
    // Fill atlas with bitmaps
    //
    groupInfo.atlas = _se_render_atlas_layout_builder_end<uint8_t>(builder);
    for (auto it : groupInfo.atlas.rects)
    {
        const SeRenderAtlasRect& rect = se_iterator_value(it);
        const uint32_t codepoint = *se_hash_table_get(atlasRectToCodepoint, rect);
        const CodepointBitmapInfo bitmap = *_se_codepoint_hash_table_get(codepointToBitmap, codepoint);
        if (bitmap.memory)
        {
            const SeRenderAtlasPutFunciton<uint8_t, uint8_t> put = [](const uint8_t* from, uint8_t* to) { *to = *from; };
            _se_render_atlas_blit(groupInfo.atlas, rect, bitmap.memory, put);
        }
        SeFontGroup::CodepointInfo* const info = _se_codepoint_hash_table_get(groupInfo.codepointToInfo, codepoint);
        se_assert(info);
        info->atlasRectIndex = (uint32_t)se_iterator_index(it);
    }
    groupInfo.texture = se_render_texture
    ({
        .format     = SeTextureFormat::R_8_UNORM,
        .width      = uint32_t(groupInfo.atlas.width),
        .height     = uint32_t(groupInfo.atlas.height),
        .data       = se_data_provider_from_memory(groupInfo.atlas.bitmap, groupInfo.atlas.width * groupInfo.atlas.height * sizeof(uint8_t)),
    });
    for (auto it : codepointToBitmap)
    {
        const CodepointBitmapInfo bitmap = se_iterator_value(it);
        if (bitmap.memory) stbtt_FreeBitmap((unsigned char*)bitmap.memory, nullptr);
    }

    _se_codepoint_hash_table_destroy(codepointToFont);
    _se_codepoint_hash_table_destroy(codepointToBitmap);
    se_hash_table_destroy(atlasRectToCodepoint);

    return groupInfo;
}

void _se_font_group_destroy(SeFontGroup& group)
{
    _se_render_atlas_destroy(group.atlas);
    _se_codepoint_hash_table_destroy(group.codepointToInfo);
    se_render_destroy(group.texture);
}

float _se_font_group_scale_for_pixel_height(const SeFontGroup& group, float height)
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

// =======================================================================
//
// Ui context
//
// =======================================================================

struct SeUiRenderVertex
{
    float       x;
    float       y;
    float       uvX;
    float       uvY;
    uint32_t    colorIndex;
    float       _pad[3];
};

struct SeUiRenderColorsUnpacked
{
    static constexpr int32_t MODE_TEXT = 0;
    static constexpr int32_t MODE_SIMPLE = 1;

    SeColorUnpacked tint;
    int32_t         mode;
    float           _pad[3];
};

struct SeUiRenderDrawData
{
    uint32_t    firstVertexIndex;
    float       _pad[3];
};

struct SeUiDrawCall
{
    SeTextureRef texture;
};

using SeUiParams = SeUiParam[SeUiParam::_COUNT];

struct SeUiActionFlags
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

struct SeUiStateFlags
{
    enum
    {
        IS_TOGGLED = 0x00000001,
    };
    using Type = uint32_t;
};

struct SeUiQuadCoords
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
struct SeUiObjectScrollInfo
{
    // Updated in se_ui_begin
    float normalizedScrollPosition;

    // Recalculated in se_ui_begin_window and subsequent calls
    float absoluteContentSize;

    // Recalculated in se_ui_begin_window
    float absoluteScrollRange;
    float absoluteScrollPosition;
    float absoluteScrollBarFrom;
    float absoluteScrollBarTo;
    float absoluteScrollBarPosition;
};

struct SeUiObjectData
{
    // Windows data
    SeUiQuadCoords          quad;
    SeUiQuadCoords          workRegion;

    float                   minWidth;
    float                   minHeight;
    SeUiFlags::Type         settingsFlags;
    SeUiActionFlags::Type   actionFlags;

    SeUiObjectScrollInfo    scrollInfoX;
    SeUiObjectScrollInfo    scrollInfoY;

    // Buttons data
    SeUiStateFlags::Type    stateFlags;

    // Text input data
    SeUtf32Char*            codepoints;
    size_t                  codepointsCapacity;
    size_t                  codepointsSize;
    size_t                  codepointsPivot;
    char*                   utf8text;
    size_t                  utf8textCapacity;
};

struct SeUiContext
{
    static constexpr size_t COLORS_BUFFER_CAPACITY = 1024;
    static constexpr size_t COLORS_BUFFER_SIZE_BYTES = sizeof(SeUiRenderColorsUnpacked) * COLORS_BUFFER_CAPACITY;
    static constexpr size_t VERTICES_BUFFER_CAPACITY = 1024 * 32;
    static constexpr size_t VERTICES_BUFFER_SIZE_BYTES = sizeof(SeUiRenderVertex) * VERTICES_BUFFER_CAPACITY;
    static constexpr size_t DRAW_DATAS_BUFFER_CAPACITY = 1024;
    static constexpr size_t DRAW_DATAS_BUFFER_SIZE_BYTES = sizeof(SeUiRenderDrawData) * DRAW_DATAS_BUFFER_CAPACITY;

    SePassRenderTarget                              target;

    const SeFontGroup*                              currentFontGroup;
    SeUiParams                                      currentParams;
    
    SeDynamicArray<SeUiDrawCall>                    frameDrawCalls;
    SeDynamicArray<SeUiRenderDrawData>              frameDrawDatas;
    SeDynamicArray<SeUiRenderVertex>                frameVertices;
    SeDynamicArray<SeUiRenderColorsUnpacked>        frameColors;

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
    SeString                                        lastClickedObjectUid;

    SeString                                        previousActiveObjectUid;
    SeString                                        currentActiveObjectUid;
    bool                                            isJustActivated;

    SeString                                        currentWindowUid;

    bool                                            hasWorkRegion;
    SeUiQuadCoords                                  workRegion;

    SeHashTable<SeString, SeUiObjectData>           uidToObjectData;
    SeHashTable<SeDataProvider, SeFontInfo>         fontInfos;
    SeHashTable<SeUiFontGroupInfo, SeFontGroup>     fontGroups;

    SeProgramRef                                    drawUiVs;
    SeProgramRef                                    drawUiFs;
    SeSamplerRef                                    sampler;
};

const SeUiParams SE_UI_DEFAULT_PARAMS =
{
    /* FONT_COLOR                   */ { .color = se_color_pack({ 1.0f, 1.0f, 1.0f, 1.0f }) },
    /* FONT_HEIGHT                  */ { .dim = 30.0f },
    /* FONT_LINE_GAP                */ { .dim = 5.0f },
    /* PRIMARY_COLOR                */ { .color = se_color_pack({ 0.73f, 0.11f, 0.14f, 1.0f }) },
    /* SECONDARY_COLOR              */ { .color = se_color_pack({ 0.09f, 0.01f, 0.01f, 0.7f }) },
    /* ACCENT_COLOR                 */ { .color = se_color_pack({ 0.88f, 0.73f, 0.73f, 1.0f }) },
    /* WINDOW_TOP_PANEL_THICKNESS   */ { .dim = 16.0f },
    /* WINDOW_BORDER_THICKNESS      */ { .dim = 2.0f },
    /* WINDOW_SCROLL_THICKNESS      */ { .dim = 10.0f },
    /* WINDOW_INNER_PADDING         */ { .dim = 5.0f },
    /* PIVOT_POSITION_X             */ { .dim = 0.0f },
    /* PIVOT_POSITION_Y             */ { .dim = 0.0f },
    /* PIVOT_TYPE_X                 */ { .enumeration = SeUiPivotType::BOTTOM_LEFT },
    /* PIVOT_TYPE_Y                 */ { .enumeration = SeUiPivotType::BOTTOM_LEFT },
    /* INPUT_ELEMENTS_BORDER_SIZE   */ { .dim = 5.0f },
};

SeUiContext g_uiCtx;

template<>
void se_hash_value_builder_absorb<SeUiFontGroupInfo>(SeHashValueBuilder& builder, const SeUiFontGroupInfo& input)
{
    for (size_t it = 0; it < SeUiFontGroupInfo::MAX_FONTS; it++)
    {
        if (!se_data_provider_is_valid(input.fonts[it])) break;
        se_hash_value_builder_absorb(builder, input.fonts[it]);
    }
}

template<>
SeHashValue se_hash_value_generate<SeUiFontGroupInfo>(const SeUiFontGroupInfo& value)
{
    SeHashValueBuilder builder = se_hash_value_builder_begin();
    se_hash_value_builder_absorb(builder, value);
    return se_hash_value_builder_end(builder);
}

template<>
bool se_compare<SeUiRenderColorsUnpacked, SeUiRenderColorsUnpacked>(const SeUiRenderColorsUnpacked& first, const SeUiRenderColorsUnpacked& second)
{
    return se_compare(first.tint, second.tint) && (first.mode == second.mode);
}

inline struct { SeString uid; SeUiObjectData* data; bool isFirst; } _se_ui_object_data_get(const char* cstrUid)
{
    SeUiObjectData* data = se_hash_table_get(g_uiCtx.uidToObjectData, cstrUid);
    const bool isFirstAccess = data == nullptr;
    if (!data)
    {
        data = se_hash_table_set(g_uiCtx.uidToObjectData, se_string_create(cstrUid, SeStringLifetime::PERSISTENT), { });
        *data = {};
    }
    se_assert(data);
    return { se_hash_table_key(g_uiCtx.uidToObjectData, data), data, isFirstAccess };
}

uint32_t _se_ui_set_draw_call(const SeUiRenderColorsUnpacked& colors)
{
    const size_t numDrawCalls = se_dynamic_array_size(g_uiCtx.frameDrawCalls);
    const SeUiDrawCall* const lastDrawCall = numDrawCalls ? &g_uiCtx.frameDrawCalls[numDrawCalls - 1] : nullptr;
    if (!lastDrawCall)
    {
        se_dynamic_array_push(g_uiCtx.frameDrawCalls, { .texture = { } });
        se_dynamic_array_push(g_uiCtx.frameDrawDatas, { .firstVertexIndex = se_dynamic_array_size<uint32_t>(g_uiCtx.frameVertices), });
        se_assert(se_dynamic_array_size(g_uiCtx.frameColors) == 0);
        se_dynamic_array_push(g_uiCtx.frameColors, colors);
    }
    if (!se_compare(g_uiCtx.frameColors[se_dynamic_array_size(g_uiCtx.frameColors) - 1], colors))
        se_dynamic_array_push(g_uiCtx.frameColors, colors);

    return se_dynamic_array_size<uint32_t>(g_uiCtx.frameColors) - 1;
}

uint32_t _se_ui_set_draw_call(SeTextureRef textureRef, const SeUiRenderColorsUnpacked& colors)
{
    //
    // Get draw call
    //
    const size_t numDrawCalls = se_dynamic_array_size(g_uiCtx.frameDrawCalls);
    const SeUiDrawCall* const lastDrawCall = numDrawCalls ? &g_uiCtx.frameDrawCalls[numDrawCalls - 1] : nullptr;
    if (!lastDrawCall || (lastDrawCall->texture && !se_compare(lastDrawCall->texture, textureRef)))
    {
        se_dynamic_array_push(g_uiCtx.frameDrawCalls, { .texture = { } });
        se_dynamic_array_push(g_uiCtx.frameDrawDatas, { .firstVertexIndex = se_dynamic_array_size<uint32_t>(g_uiCtx.frameVertices), });
    }
    //
    // Push texture and color
    //
    SeUiDrawCall* const drawCall = &g_uiCtx.frameDrawCalls[se_dynamic_array_size(g_uiCtx.frameDrawCalls) - 1];
    if (!drawCall->texture) drawCall->texture = textureRef;
    se_assert(se_compare(drawCall->texture, textureRef));
    if (!se_compare(g_uiCtx.frameColors[se_dynamic_array_size(g_uiCtx.frameColors) - 1], colors))
        se_dynamic_array_push(g_uiCtx.frameColors, colors);

    return se_dynamic_array_size<uint32_t>(g_uiCtx.frameColors) - 1;
}

inline SeUiQuadCoords _se_ui_clamp_to_work_region(const SeUiQuadCoords& quad)
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

inline void _se_ui_push_quad(uint32_t colorIndex, const SeUiQuadCoords& unclamped, float u1 = 0, float v1 = 0, float u2 = 0, float v2 = 0)
{
    const SeUiQuadCoords clamped = _se_ui_clamp_to_work_region(unclamped);
    if (clamped.blX == clamped.trX || clamped.blY == clamped.trY) return; // Out of bounds

    const float width = unclamped.trX - unclamped.blX;
    const float height = unclamped.trY - unclamped.blY;
    const float u1clamped = se_lerp(u1, u2, (clamped.blX - unclamped.blX) / width);
    const float v1clamped = se_lerp(v1, v2, (clamped.blY - unclamped.blY) / height);
    const float u2clamped = se_lerp(u2, u1, (unclamped.trX - clamped.trX) / width);
    const float v2clamped = se_lerp(v2, v1, (unclamped.trY - clamped.trY) / height);

    se_dynamic_array_push(g_uiCtx.frameVertices, { clamped.blX, clamped.blY, u1clamped, v1clamped, colorIndex });
    se_dynamic_array_push(g_uiCtx.frameVertices, { clamped.blX, clamped.trY, u1clamped, v2clamped, colorIndex });
    se_dynamic_array_push(g_uiCtx.frameVertices, { clamped.trX, clamped.trY, u2clamped, v2clamped, colorIndex });
    se_dynamic_array_push(g_uiCtx.frameVertices, { clamped.blX, clamped.blY, u1clamped, v1clamped, colorIndex });
    se_dynamic_array_push(g_uiCtx.frameVertices, { clamped.trX, clamped.trY, u2clamped, v2clamped, colorIndex });
    se_dynamic_array_push(g_uiCtx.frameVertices, { clamped.trX, clamped.blY, u2clamped, v1clamped, colorIndex });
}

inline SeUiQuadCoords _se_ui_get_corners(float width, float height)
{
    if (g_uiCtx.hasWorkRegion)
    {
        const SeUiObjectData* const window = se_hash_table_get(g_uiCtx.uidToObjectData, g_uiCtx.currentWindowUid);
        const float blX = window
            ? g_uiCtx.workRegion.blX - window->scrollInfoX.absoluteScrollPosition
            : g_uiCtx.workRegion.blX;
        const float trY = window
            ? window->workRegion.trY - window->scrollInfoY.absoluteContentSize + window->scrollInfoY.absoluteScrollPosition
            : g_uiCtx.workRegion.trY;
        return
        {
            blX,
            trY - height,
            blX + width,
            trY,
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

inline bool _se_ui_is_under_cursor(const SeUiQuadCoords& quad)
{
    return
        (quad.blX <= g_uiCtx.mouseX) &&
        (quad.blY <= g_uiCtx.mouseY) &&
        (quad.trX >= g_uiCtx.mouseX) &&
        (quad.trY >= g_uiCtx.mouseY);
}

// @TODO : move copypasta text-processing code to some base function
template<typename T>
SeUiQuadCoords _se_ui_get_text_bbox(const T* text)
{
    const SeFontGroup* const fontGroup = g_uiCtx.currentFontGroup;
    const float fontHeight = g_uiCtx.currentParams[SeUiParam::FONT_HEIGHT].dim;
    const float baselineX = 0;
    const float baselineY = 0;
    float positionX = 0;
    float biggestAscent = 0;
    float biggestDescent = 0;
    int previousCodepoint = 0;
    for (SeUtf32Char codepoint : SeUtf32Iterator{ text })
    {
        const SeFontGroup::CodepointInfo* const codepointInfo = _se_codepoint_hash_table_get(fontGroup->codepointToInfo, codepoint);
        se_assert(codepointInfo);
        const SeFontInfo* const font = codepointInfo->font;
        const float scale = stbtt_ScaleForPixelHeight(&font->stbInfo, fontHeight);
        const float advance = scale * float(codepointInfo->advanceWidthUnscaled);
        const float bearing = scale * float(codepointInfo->leftSideBearingUnscaled);
        const int codepointSigned = _se_font_group_direct_cast_to_int(codepoint);
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

template<typename T>
float _se_ui_draw_text_at_pos(const T* text, float baselineX, float baselineY, const SeColorUnpacked& tint = { 1.0f, 1.0f, 1.0f, 1.0f })
{
    if (g_uiCtx.hasWorkRegion && baselineX >= g_uiCtx.workRegion.trX)
    {
        return 0.0f;
    }
    const SeFontGroup* const fontGroup = g_uiCtx.currentFontGroup;
    const float fontHeight = g_uiCtx.currentParams[SeUiParam::FONT_HEIGHT].dim;
    const SeColorUnpacked fontColorUntinted = se_color_unpack(g_uiCtx.currentParams[SeUiParam::FONT_COLOR].color);
    const SeColorUnpacked fontColorTinted
    {
        fontColorUntinted.r * tint.r,
        fontColorUntinted.g * tint.g,
        fontColorUntinted.b * tint.b,
        fontColorUntinted.a * tint.a,
    };
    const uint32_t colorIndex = _se_ui_set_draw_call(fontGroup->texture,
    {
        .tint = fontColorTinted,
        .mode = SeUiRenderColorsUnpacked::MODE_TEXT,
    });
    //
    // Process codepoints
    //
    float positionX = baselineX;
    int previousCodepoint = 0;
    for (SeUtf32Char codepoint : SeUtf32Iterator{ text })
    {
        const SeFontGroup::CodepointInfo* const codepointInfo = _se_codepoint_hash_table_get(fontGroup->codepointToInfo, codepoint);
        se_assert(codepointInfo);
        const SeFontInfo* const font = codepointInfo->font;
        const float scale = stbtt_ScaleForPixelHeight(&font->stbInfo, fontHeight);
        const float advance = scale * float(codepointInfo->advanceWidthUnscaled);
        const float bearing = scale * float(codepointInfo->leftSideBearingUnscaled);
        const int codepointSigned = _se_font_group_direct_cast_to_int(codepoint);
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
        const SeRenderAtlasRectNormalized& rect = fontGroup->atlas.uvRects[codepointInfo->atlasRectIndex];
        _se_ui_push_quad(colorIndex, { bottomLeftX, bottomLeftY, topRightX, topRightY }, rect.p1x, rect.p1y, rect.p2x, rect.p2y);
        positionX += advance;
        previousCodepoint = codepointSigned;
    }
    return positionX - baselineX;
}

template<typename T>
float _se_ui_get_text_pivot_offset(const T* text, size_t pivotPosition)
{
    const SeFontGroup* const fontGroup = g_uiCtx.currentFontGroup;
    const float fontHeight = g_uiCtx.currentParams[SeUiParam::FONT_HEIGHT].dim;
    float positionX = 0;
    int previousCodepoint = 0;
    float previousAdditionalAdvance = 0.0f;
    size_t it = 0;
    for (SeUtf32Char codepoint : SeUtf32Iterator{ text })
    {
        if (it++ == pivotPosition)
        {
            positionX -= previousAdditionalAdvance;
            break;
        }
        const SeFontGroup::CodepointInfo* const codepointInfo = _se_codepoint_hash_table_get(fontGroup->codepointToInfo, codepoint);
        se_assert(codepointInfo);
        const SeFontInfo* const font = codepointInfo->font;
        const float scale = stbtt_ScaleForPixelHeight(&font->stbInfo, fontHeight);
        const float advance = scale * float(codepointInfo->advanceWidthUnscaled);
        const float bearing = scale * float(codepointInfo->leftSideBearingUnscaled);
        const int codepointSigned = _se_font_group_direct_cast_to_int(codepoint);
        int x0;
        int y0;
        int x1;
        int y1;
        stbtt_GetCodepointBitmapBox(&font->stbInfo, codepointSigned, scale, scale, &x0, &y0, &x1, &y1);
        const float additionalAdvance = scale * float(stbtt_GetCodepointKernAdvance(&font->stbInfo, previousCodepoint, codepointSigned));
        positionX += additionalAdvance + advance;
        previousCodepoint = codepointSigned;
        previousAdditionalAdvance = additionalAdvance;
    }
    return positionX;
}

void _se_ui_reset_work_region()
{
    g_uiCtx.hasWorkRegion   = false;
    g_uiCtx.workRegion.blX  = 0.0f;
    g_uiCtx.workRegion.blY  = 0.0f;
    g_uiCtx.workRegion.trX  = se_win_get_width<float>();
    g_uiCtx.workRegion.trY  = se_win_get_height<float>();
}

void _se_ui_set_work_region(const SeUiQuadCoords& quad)
{
    g_uiCtx.hasWorkRegion = true;
    g_uiCtx.workRegion = quad;
}

float _se_ui_get_current_font_ascent()
{
    const float fontHeight = g_uiCtx.currentParams[SeUiParam::FONT_HEIGHT].dim;
    const SeFontGroup* const group = g_uiCtx.currentFontGroup;
    // Same as stbtt_ScaleForPixelHeight
    const float scale = fontHeight / float(group->ascentUnscaled - group->descentUnscaled);
    return float(group->ascentUnscaled) * scale;
}

float _se_ui_get_current_font_descent()
{
    const float fontHeight = g_uiCtx.currentParams[SeUiParam::FONT_HEIGHT].dim;
    const SeFontGroup* const group = g_uiCtx.currentFontGroup;
    // Same as stbtt_ScaleForPixelHeight
    const float scale = fontHeight / float(group->ascentUnscaled - group->descentUnscaled);
    return float(group->descentUnscaled) * scale;
}

bool se_ui_begin(const SeUiBeginInfo& info)
{
    g_uiCtx.target            = info.target;
    g_uiCtx.currentFontGroup  = nullptr;
    memcpy(g_uiCtx.currentParams, SE_UI_DEFAULT_PARAMS, sizeof(SeUiParams));

    se_dynamic_array_construct(g_uiCtx.frameDrawCalls, se_allocator_frame(), 16);
    se_dynamic_array_construct(g_uiCtx.frameDrawDatas, se_allocator_frame(), 16);
    se_dynamic_array_construct(g_uiCtx.frameVertices, se_allocator_frame(), 512);
    se_dynamic_array_construct(g_uiCtx.frameColors, se_allocator_frame(), 16);

    const SeMousePos mousePos = se_win_get_mouse_pos();
    const float mouseX = float(mousePos.x);
    const float mouseY = float(mousePos.y);
    g_uiCtx.mouseDeltaX       = mouseX - g_uiCtx.mouseX;
    g_uiCtx.mouseDeltaY       = mouseY - g_uiCtx.mouseY;
    g_uiCtx.mouseX            = mouseX;
    g_uiCtx.mouseY            = mouseY;
    g_uiCtx.isMouseDown       = se_win_is_mouse_button_pressed(SeMouse::LMB);
    g_uiCtx.isMouseJustDown   = se_win_is_mouse_button_just_pressed(SeMouse::LMB);
    g_uiCtx.isJustActivated =
        se_compare(g_uiCtx.previousActiveObjectUid, SeString{ }) &&
        !se_compare(g_uiCtx.currentActiveObjectUid, SeString{ });
    g_uiCtx.previousHoveredObjectUid    = g_uiCtx.currentHoveredObjectUid;
    g_uiCtx.previousHoveredWindowUid    = g_uiCtx.currentHoveredWindowUid;
    g_uiCtx.previousActiveObjectUid     = g_uiCtx.currentActiveObjectUid;
    g_uiCtx.currentHoveredObjectUid     = { };
    g_uiCtx.currentHoveredWindowUid     = { };
    g_uiCtx.currentWindowUid            = { };
    _se_ui_reset_work_region();
    
    if (!g_uiCtx.isMouseDown)
    {
        g_uiCtx.currentActiveObjectUid = { };
    }
    else
    {
        SeUiObjectData* const activeObject = se_hash_table_get(g_uiCtx.uidToObjectData, g_uiCtx.currentActiveObjectUid);
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
            if ((activeObject->settingsFlags & SeUiFlags::MOVABLE) && (activeObject->actionFlags & SeUiActionFlags::CAN_BE_MOVED))
            {
                activeObject->quad.blX = activeObject->quad.blX + g_uiCtx.mouseDeltaX;
                activeObject->quad.blY = activeObject->quad.blY + g_uiCtx.mouseDeltaY;
                activeObject->quad.trX = activeObject->quad.trX + g_uiCtx.mouseDeltaX;
                activeObject->quad.trY = activeObject->quad.trY + g_uiCtx.mouseDeltaY;
            }
            if ((activeObject->settingsFlags & SeUiFlags::RESIZABLE_X) && (activeObject->actionFlags & SeUiActionFlags::CAN_BE_RESIZED_X_LEFT))
            {
                const bool canChangeSize = isPositiveDeltaX ? g_uiCtx.mouseX > activeObject->quad.blX : g_uiCtx.mouseX < activeObject->quad.blX;
                const float actualDelta = canChangeSize
                    ? (g_uiCtx.mouseDeltaX > maxDeltaTowardsCenterX ? maxDeltaTowardsCenterX : g_uiCtx.mouseDeltaX)
                    : 0.0f;
                activeObject->quad.blX = activeObject->quad.blX + actualDelta;
            }
            if ((activeObject->settingsFlags & SeUiFlags::RESIZABLE_X) && (activeObject->actionFlags & SeUiActionFlags::CAN_BE_RESIZED_X_RIGHT))
            {
                const bool canChangeSize = isPositiveDeltaX ? g_uiCtx.mouseX > activeObject->quad.trX : g_uiCtx.mouseX < activeObject->quad.trX;
                const float actualDelta = canChangeSize
                    ? (g_uiCtx.mouseDeltaX < -maxDeltaTowardsCenterX ? -maxDeltaTowardsCenterX : g_uiCtx.mouseDeltaX)
                    : 0.0f;
                activeObject->quad.trX = activeObject->quad.trX + actualDelta;
            }
            if ((activeObject->settingsFlags & SeUiFlags::RESIZABLE_Y) && (activeObject->actionFlags & SeUiActionFlags::CAN_BE_RESIZED_Y_TOP))
            {
                const bool canChangeSize = isPositiveDeltaY ? g_uiCtx.mouseY > activeObject->quad.trY : g_uiCtx.mouseY < activeObject->quad.trY;
                const float actualDelta = canChangeSize
                    ? (g_uiCtx.mouseDeltaY < -maxDeltaTowardsCenterY ? -maxDeltaTowardsCenterY : g_uiCtx.mouseDeltaY)
                    : 0.0f;
                activeObject->quad.trY = activeObject->quad.trY + actualDelta;
            }
            if ((activeObject->settingsFlags & SeUiFlags::RESIZABLE_Y) && (activeObject->actionFlags & SeUiActionFlags::CAN_BE_RESIZED_Y_BOTTOM))
            {
                const bool canChangeSize = isPositiveDeltaY ? g_uiCtx.mouseY > activeObject->quad.blY : g_uiCtx.mouseY < activeObject->quad.blY;
                const float actualDelta = canChangeSize
                    ? (g_uiCtx.mouseDeltaY > maxDeltaTowardsCenterY ? maxDeltaTowardsCenterY : g_uiCtx.mouseDeltaY)
                    : 0.0f;
                activeObject->quad.blY = activeObject->quad.blY + actualDelta;
            }
            if ((activeObject->settingsFlags & SeUiFlags::SCROLLABLE_Y) && (activeObject->actionFlags & SeUiActionFlags::CAN_BE_SCROLLED_BY_MOUSE_MOVE_Y))
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
            if ((activeObject->settingsFlags & SeUiFlags::SCROLLABLE_X) && (activeObject->actionFlags & SeUiActionFlags::CAN_BE_SCROLLED_BY_MOUSE_MOVE_X))
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

    SeUiObjectData* const hoveredWindow = se_hash_table_get(g_uiCtx.uidToObjectData, g_uiCtx.previousHoveredWindowUid);
    if (hoveredWindow)
    {
        const float mouseWheel = float(se_win_get_mouse_wheel());
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

SePassDependencies se_ui_end(SePassDependencies dependencies)
{
    const SePassDependencies result = se_render_begin_graphics_pass
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
    const SeBufferRef projectionBuffer = se_render_scratch_memory_buffer
    ({
        se_data_provider_from_memory
        (
            se_float4x4_transposed(se_render_orthographic(0, se_win_get_width<float>(), 0, se_win_get_height<float>(), 0, 2))
        )
    });
    const SeBufferRef drawDatasBuffer = se_render_scratch_memory_buffer
    ({
        se_data_provider_from_memory(se_dynamic_array_raw(g_uiCtx.frameDrawDatas), se_dynamic_array_raw_size(g_uiCtx.frameDrawDatas))
    });
    const SeBufferRef verticesBuffer = se_render_scratch_memory_buffer
    ({
        se_data_provider_from_memory(se_dynamic_array_raw(g_uiCtx.frameVertices), se_dynamic_array_raw_size(g_uiCtx.frameVertices))
    });
    const SeBufferRef colorBuffer = se_render_scratch_memory_buffer
    ({
        se_data_provider_from_memory(se_dynamic_array_raw(g_uiCtx.frameColors), se_dynamic_array_raw_size(g_uiCtx.frameColors))
    });
    se_render_bind
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
    se_assert(se_dynamic_array_size(g_uiCtx.frameDrawCalls) == se_dynamic_array_size(g_uiCtx.frameDrawDatas));
    const size_t numDrawCalls = se_dynamic_array_size(g_uiCtx.frameDrawCalls);
    for (auto it : g_uiCtx.frameDrawCalls)
    {
        const SeUiDrawCall& drawCall = se_iterator_value(it);
        const size_t drawCallIndex = se_iterator_index(it);
        se_render_bind
        ({
            .set = 1,
            .bindings =
            {
                { .binding = 0, .type = SeBinding::TEXTURE, .texture = { drawCall.texture, g_uiCtx.sampler } },
                { .binding = 1, .type = SeBinding::BUFFER, .buffer = {
                    .buffer = drawDatasBuffer,
                    .offset = drawCallIndex * sizeof(SeUiRenderDrawData),
                    .size   = sizeof(SeUiRenderDrawData),
                } },
            },
        });
        const uint32_t firstVertexIndex = g_uiCtx.frameDrawDatas[se_iterator_index(it)].firstVertexIndex;
        const uint32_t lastVertexIndex = drawCallIndex == (numDrawCalls - 1)
            ? se_dynamic_array_size<uint32_t>(g_uiCtx.frameVertices) - 1
            : g_uiCtx.frameDrawDatas[drawCallIndex + 1].firstVertexIndex;
        se_render_draw
        ({
            .numVertices = lastVertexIndex - firstVertexIndex + 1,
            .numInstances = 1,
        });
    }
    se_render_end_pass();
    //
    // Clear context
    //
    se_dynamic_array_destroy(g_uiCtx.frameDrawCalls);
    se_dynamic_array_destroy(g_uiCtx.frameDrawDatas);
    se_dynamic_array_destroy(g_uiCtx.frameVertices);
    se_dynamic_array_destroy(g_uiCtx.frameColors);
    return result;
}

void se_ui_set_font_group(const SeUiFontGroupInfo& info)
{
    const SeFontGroup* fontGroup = se_hash_table_get(g_uiCtx.fontGroups, info);
    if (!fontGroup)
    {
        const SeFontInfo* fonts[SeUiFontGroupInfo::MAX_FONTS];
        size_t fontIt = 0;
        for (; fontIt < SeUiFontGroupInfo::MAX_FONTS; fontIt++)
        {
            const SeDataProvider& fontData = info.fonts[fontIt];
            if (!se_data_provider_is_valid(fontData))
            {
                break;
            }
            const SeFontInfo* font = se_hash_table_get(g_uiCtx.fontInfos, fontData);
            if (!font)
            {
                font = se_hash_table_set(g_uiCtx.fontInfos, fontData, _se_font_info_create(fontData));
            }
            se_assert(font);
            fonts[fontIt] = font;
        };
        fontGroup = se_hash_table_set(g_uiCtx.fontGroups, info, _se_font_group_create(fonts, fontIt));
    }
    se_assert(fontGroup);
    g_uiCtx.currentFontGroup = fontGroup;
}

void se_ui_set_param(SeUiParam::Type type, const SeUiParam& param)
{
    g_uiCtx.currentParams[type] = param;
}

void se_ui_text(const SeUiTextInfo& info)
{
    const float fontHeight = g_uiCtx.currentParams[SeUiParam::FONT_HEIGHT].dim;
    if (g_uiCtx.hasWorkRegion)
    {
        SeUiObjectData* const window = se_hash_table_get(g_uiCtx.uidToObjectData, g_uiCtx.currentWindowUid);

        const float lineGap = g_uiCtx.currentParams[SeUiParam::FONT_LINE_GAP].dim;
        const float baselineX = window
            ? g_uiCtx.workRegion.blX - window->scrollInfoX.absoluteScrollPosition
            : g_uiCtx.workRegion.blX;
        const float baselineY = window
            ? window->workRegion.trY - window->scrollInfoY.absoluteContentSize - _se_ui_get_current_font_ascent() + window->scrollInfoY.absoluteScrollPosition
            : g_uiCtx.workRegion.trY - _se_ui_get_current_font_ascent();
        const float lineLowestPoint = baselineY + _se_ui_get_current_font_descent();

        float textWidth = 0.0f;
        if (lineLowestPoint < g_uiCtx.workRegion.trY)
        {
            textWidth = _se_ui_draw_text_at_pos(info.utf8text, baselineX, baselineY);
            g_uiCtx.workRegion.trY = lineLowestPoint - lineGap;
        }
        else
        {
            const SeUiQuadCoords textBbox = _se_ui_get_text_bbox(info.utf8text);
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
        const auto [x1, y1, x2, y2] = _se_ui_get_text_bbox(info.utf8text);
        if (g_uiCtx.currentParams[SeUiParam::PIVOT_TYPE_X].enumeration == SeUiPivotType::CENTER) baselineX -= (x2 - x1) / 2.0f;
        if (g_uiCtx.currentParams[SeUiParam::PIVOT_TYPE_Y].enumeration == SeUiPivotType::CENTER) baselineY -= fontHeight / 2.0f;
        _se_ui_draw_text_at_pos(info.utf8text, baselineX, baselineY);
    }
}

bool se_ui_begin_window(const SeUiWindowInfo& info)
{
    se_assert_msg(!g_uiCtx.hasWorkRegion, "Can't begin new window - another window or region is currently active");

    //
    // Get object data, update quad and settings
    //
    const auto [uid, data, isFirst] = _se_ui_object_data_get(info.uid);
    const bool getQuadFromData = (info.flags & (SeUiFlags::RESIZABLE_X | SeUiFlags::RESIZABLE_Y)) && !isFirst;
    const SeUiQuadCoords quad = getQuadFromData
        ? data->quad
        : _se_ui_get_corners(info.width, info.height);
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

    const SeUiQuadCoords scrollBarQuadX
    {
        scrollBarPositionX,
        quad.blY + borderThickness,
        scrollBarPositionX + scrollBarSizeX,
        quad.blY + borderThickness + scrollThickness,
    };

    const SeUiQuadCoords scrollBarQuadY
    {
        quad.trX - borderThickness - scrollThickness,
        scrollBarPositionY - scrollBarSizeY,
        quad.trX - borderThickness,
        scrollBarPositionY,
    };

    //
    // Update other data stuff, active object uid and work region info
    //
    if (_se_ui_is_under_cursor(quad))
    {
        g_uiCtx.currentHoveredObjectUid = uid;
        g_uiCtx.currentHoveredWindowUid = uid;
        if (g_uiCtx.isMouseJustDown)
        {
            static constexpr float BORDER_PANEL_TOLERANCE = 2.0f;

            g_uiCtx.currentActiveObjectUid = uid;
            g_uiCtx.lastClickedObjectUid = uid;

            const bool isTopPanelUnderCursor = ((quad.trY - topPanelThickness) <= g_uiCtx.mouseY);
            const bool isLeftBorderUnderCursor = !isTopPanelUnderCursor && ((quad.blX + borderThickness + BORDER_PANEL_TOLERANCE) >= g_uiCtx.mouseX);
            const bool isRightBorderUnderCursor = !isTopPanelUnderCursor && ((quad.trX - borderThickness - BORDER_PANEL_TOLERANCE) <= g_uiCtx.mouseX);
            const bool isBottomBorderUnderCursor = !isTopPanelUnderCursor && ((quad.blY + borderThickness + BORDER_PANEL_TOLERANCE) >= g_uiCtx.mouseY);
            const bool isAnyBorderUnderCursor = isTopPanelUnderCursor | isLeftBorderUnderCursor | isRightBorderUnderCursor | isBottomBorderUnderCursor;
            const bool isHorizontalScrollUnderCursor = !isAnyBorderUnderCursor && _se_ui_is_under_cursor(scrollBarQuadX);
            const bool isVerticalScrollUnderCursor = !isAnyBorderUnderCursor && _se_ui_is_under_cursor(scrollBarQuadY);

            if (isTopPanelUnderCursor) data->actionFlags |= SeUiActionFlags::CAN_BE_MOVED;
            if (isLeftBorderUnderCursor) data->actionFlags |= SeUiActionFlags::CAN_BE_RESIZED_X_LEFT;
            if (isRightBorderUnderCursor) data->actionFlags |= SeUiActionFlags::CAN_BE_RESIZED_X_RIGHT;
            if (isBottomBorderUnderCursor) data->actionFlags |= SeUiActionFlags::CAN_BE_RESIZED_Y_BOTTOM;
            if (isHorizontalScrollUnderCursor) data->actionFlags |= SeUiActionFlags::CAN_BE_SCROLLED_BY_MOUSE_MOVE_X;
            if (isVerticalScrollUnderCursor) data->actionFlags |= SeUiActionFlags::CAN_BE_SCROLLED_BY_MOUSE_MOVE_Y;
        }
        else if (!g_uiCtx.isMouseDown)
        {
            data->actionFlags = 0;
        }
    }

    //
    // Draw top panel and borders
    //
    const uint32_t colorIndexBorders = _se_ui_set_draw_call
    ({
        .tint = se_color_unpack(g_uiCtx.currentParams[SeUiParam::PRIMARY_COLOR].color),
        .mode = SeUiRenderColorsUnpacked::MODE_SIMPLE,
    });
    _se_ui_push_quad(colorIndexBorders, { quad.blX, quad.trY - topPanelThickness, quad.trX, quad.trY });
    _se_ui_push_quad(colorIndexBorders, { quad.blX, quad.blY, quad.blX + borderThickness, quad.trY - topPanelThickness });
    _se_ui_push_quad(colorIndexBorders, { quad.trX - borderThickness, quad.blY, quad.trX, quad.trY - topPanelThickness });
    _se_ui_push_quad(colorIndexBorders, { quad.blX + borderThickness, quad.blY, quad.trX - borderThickness, quad.blY + borderThickness });

    //
    // Draw background
    //
    const uint32_t colorIndexBackground = _se_ui_set_draw_call
    ({
        .tint = se_color_unpack(g_uiCtx.currentParams[SeUiParam::SECONDARY_COLOR].color),
        .mode = SeUiRenderColorsUnpacked::MODE_SIMPLE,
    });
    _se_ui_push_quad(colorIndexBackground, { quad.blX + borderThickness, quad.blY + borderThickness, quad.trX - borderThickness, quad.trY - topPanelThickness });

    //
    // Draw scroll bars
    //
    if (hasScrollY || hasScrollX)
    {
        // Draw scroll bars connection (bottom right square)
        _se_ui_push_quad(colorIndexBorders,
        {
            quad.trX - borderThickness - scrollThickness,
            quad.blY + borderThickness,
            quad.trX - borderThickness,
            quad.blY + borderThickness + scrollThickness
        });
        const uint32_t colorIndexScrollBar = _se_ui_set_draw_call
        ({
            .tint = se_color_unpack(g_uiCtx.currentParams[SeUiParam::ACCENT_COLOR].color),
            .mode = SeUiRenderColorsUnpacked::MODE_SIMPLE,
        });
        if (hasScrollX)
        {
            _se_ui_push_quad(colorIndexScrollBar, scrollBarQuadX);
        }
        if (hasScrollY)
        {
            _se_ui_push_quad(colorIndexScrollBar, scrollBarQuadY);
        }
    }

    //
    // Set window ui and region
    //
    g_uiCtx.currentWindowUid = uid;
    _se_ui_set_work_region(data->workRegion);

    return true;
}

void se_ui_end_window()
{
    g_uiCtx.currentWindowUid = { };
    _se_ui_reset_work_region();
}

bool se_ui_begin_region(const SeUiRegionInfo& info)
{
    se_assert_msg(!g_uiCtx.hasWorkRegion, "Can't begin new region - another window or region is currently active");

    const SeUiQuadCoords quad = _se_ui_get_corners(info.width, info.height);
    if (info.useDebugColor)
    {
        const uint32_t debugColorIndex = _se_ui_set_draw_call
        ({
            .tint = se_color_unpack(g_uiCtx.currentParams[SeUiParam::ACCENT_COLOR].color),
            .mode = SeUiRenderColorsUnpacked::MODE_SIMPLE,
        });
        _se_ui_push_quad(debugColorIndex, quad);
    }
    _se_ui_set_work_region(quad);

    return true;
}

void se_ui_end_region()
{
    _se_ui_reset_work_region();
}

bool se_ui_button(const SeUiButtonInfo& info)
{
    const auto [uid, data, isFirst] = _se_ui_object_data_get(info.uid);
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
        const float border = g_uiCtx.currentParams[SeUiParam::INPUT_ELEMENTS_BORDER_SIZE].dim;
        const auto [bbx1, bby1, bbx2, bby2] = _se_ui_get_text_bbox(info.utf8text);
        textWidth = bbx2 - bbx1;
        const float textWidthWithBorder = textWidth + border * 2.0f;
        const float textHeightWithBorder = fontHeight + border * 2.0f;
        if (textWidthWithBorder > buttonWidth) buttonWidth = textWidthWithBorder;
        if (textHeightWithBorder > buttonHeight) buttonHeight = textHeightWithBorder;
        textBottomOffsetY = -_se_ui_get_current_font_descent() + border;
    }
    const SeUiQuadCoords quad = _se_ui_get_corners(buttonWidth, buttonHeight);
    //
    // Update state
    //
    const bool isUnderCursor = _se_ui_is_under_cursor(quad) && (g_uiCtx.hasWorkRegion ? _se_ui_is_under_cursor(g_uiCtx.workRegion) : true);
    if (isUnderCursor)
    {
        g_uiCtx.currentHoveredObjectUid = uid;
        if (g_uiCtx.isMouseJustDown)
        {
            g_uiCtx.currentActiveObjectUid = uid;
            g_uiCtx.lastClickedObjectUid = uid;
        }
    }
    const bool isPressed = se_compare(g_uiCtx.previousActiveObjectUid, uid);
    const bool isClicked = isPressed && g_uiCtx.isJustActivated;
    if (isClicked)
    {
        if (data->stateFlags & (1 << SeUiStateFlags::IS_TOGGLED))
            data->stateFlags &= ~(1 << SeUiStateFlags::IS_TOGGLED);
        else
            data->stateFlags |= 1 << SeUiStateFlags::IS_TOGGLED;
    }
    const bool isToggled = data->stateFlags & (1 << SeUiStateFlags::IS_TOGGLED);
    //
    // Draw button
    //
    SeColorUnpacked buttonColor = se_color_unpack(g_uiCtx.currentParams[SeUiParam::PRIMARY_COLOR].color);
    if (isPressed)
    {
        const float SCALE = 0.5f;
        buttonColor.r *= SCALE;
        buttonColor.g *= SCALE;
        buttonColor.b *= SCALE;
    }
    const uint32_t colorIndexButton = _se_ui_set_draw_call({ buttonColor, SeUiRenderColorsUnpacked::MODE_SIMPLE });
    _se_ui_push_quad(colorIndexButton, quad);
    //
    // Draw text
    //
    if (info.utf8text)
    {
        const float posX = quad.blX + buttonWidth / 2.0f - textWidth / 2.0f;
        const float posY = quad.blY + textBottomOffsetY;
        _se_ui_draw_text_at_pos(info.utf8text, posX, posY);
    }
    //
    // Update data
    //
    if (g_uiCtx.hasWorkRegion)
    {
        g_uiCtx.workRegion.trY = se_min(g_uiCtx.workRegion.trY, quad.blY);
    }
    SeUiObjectData* const window = se_hash_table_get(g_uiCtx.uidToObjectData, g_uiCtx.currentWindowUid);
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

const char* se_ui_input_text_line(const SeUiInputTextLineInfo& info)
{
    se_assert_msg(g_uiCtx.hasWorkRegion, "@TODO : support input text line outside of work region");

    const auto [uid, data, isFirst] = _se_ui_object_data_get(info.uid);
    if (isFirst)
    {
        static constexpr const size_t INITIAL_CAPACITY = 128;
        data->codepoints = (SeUtf32Char*)se_alloc(se_allocator_persistent(), INITIAL_CAPACITY * sizeof(SeUtf32Char), se_alloc_tag);
        data->codepointsCapacity = INITIAL_CAPACITY;
        data->codepointsSize = 0;
        data->codepointsPivot = 0;
        data->codepoints[0] = 0;
    }
    const bool isLastClicked = se_compare(g_uiCtx.lastClickedObjectUid, uid);

    //
    // Process character input
    //
    bool isTextUpdated = isFirst;
    if (isLastClicked)
    {
        for (auto it : se_win_get_character_input())
        {
            const SeCharacterInput& input = se_iterator_value(it);
            if (input.type == SeCharacterInput::SPECIAL)
            {
                switch (input.special)
                {
                    case SeKeyboard::BACKSPACE:
                    {
                        if (data->codepointsPivot)
                        {
                            for (size_t codepointIt = data->codepointsPivot; codepointIt < data->codepointsSize; codepointIt++)
                            {
                                data->codepoints[codepointIt - 1] = data->codepoints[codepointIt];
                            }
                            data->codepointsSize -=1;
                            data->codepointsPivot -= 1;
                            data->codepoints[data->codepointsSize] = 0;
                            isTextUpdated = true;
                        }
                    } break;
                    case SeKeyboard::DEL:
                    {
                        if (data->codepointsPivot < data->codepointsSize)
                        {
                            for (size_t codepointIt = data->codepointsPivot; codepointIt < (data->codepointsSize - 1); codepointIt++)
                            {
                                data->codepoints[codepointIt] = data->codepoints[codepointIt + 1];
                            }
                            data->codepointsSize -=1;
                            data->codepoints[data->codepointsSize] = 0;
                            isTextUpdated = true;
                        }
                    } break;
                    case SeKeyboard::ARROW_LEFT:
                    {
                        if (data->codepointsPivot) data->codepointsPivot -= 1;
                    } break;
                    case SeKeyboard::ARROW_RIGHT:
                    {
                        if (data->codepointsPivot < data->codepointsSize) data->codepointsPivot += 1;
                    } break;
                    case SeKeyboard::HOME:
                    {
                        data->codepointsPivot = 0;
                    } break;
                    case SeKeyboard::END:
                    {
                        data->codepointsPivot = data->codepointsSize;
                    } break;
                }
            }
            else // input.type == SeCharacterInput::CHARACTER
            {
                const SeUtf32Char character = input.character;
                const bool needsExpansion = (data->codepointsCapacity - data->codepointsSize) <= 1;
                if (needsExpansion)
                {
                    SeUtf32Char* const newCodepointData = (SeUtf32Char*)se_alloc(se_allocator_persistent(), data->codepointsCapacity * 2 * sizeof(SeUtf32Char), se_alloc_tag);
                    memcpy(newCodepointData, data->codepoints, data->codepointsSize * sizeof(SeUtf32Char));
                    se_dealloc(se_allocator_persistent(), data->codepoints, data->codepointsCapacity * sizeof(SeUtf32Char));
                    data->codepoints = newCodepointData;
                    data->codepointsCapacity = data->codepointsCapacity * 2;
                }
                data->codepoints[data->codepointsSize + 1] = 0;
                for (size_t codepointIt = data->codepointsSize; codepointIt > data->codepointsPivot; codepointIt--)
                {
                    data->codepoints[codepointIt] = data->codepoints[codepointIt - 1];
                }
                data->codepoints[data->codepointsPivot++] = character;
                data->codepointsSize += 1;
                isTextUpdated = true;
            }
        }
    }

    //
    // Update utf-8 version if required
    //
    if (isTextUpdated)
    {
        const size_t requiredSize = data->codepointsSize * SeUtf8Char::CAPACITY + 1; // Every codepoint translates to 4 or less bytes, +1 for null terminator
        const bool needsNewAllocation = (data->utf8textCapacity < requiredSize) || (data->utf8text == nullptr);
        char* const memory = needsNewAllocation
            ? (char*)se_alloc(se_allocator_persistent(), requiredSize, se_alloc_tag)
            : data->utf8text;
        
        size_t pivot = 0;
        for (SeUtf32Char c : SeUtf32Iterator{ data->codepoints })
        {
            const SeUtf8Char utf8char = se_unicode_to_utf8(c);
            const size_t length = se_unicode_length(utf8char);
            memcpy(memory + pivot, utf8char.data, length);
            pivot += length;
        }
        memory[pivot] = 0;

        if (needsNewAllocation)
        {
            if (data->utf8text != nullptr) se_dealloc(se_allocator_persistent(), data->utf8text, data->utf8textCapacity);
            data->utf8text = memory;
            data->utf8textCapacity = requiredSize;
        }
    }

    //
    // Draw stuff
    //
    const float border = g_uiCtx.currentParams[SeUiParam::INPUT_ELEMENTS_BORDER_SIZE].dim;
    const float fontHeight = g_uiCtx.currentParams[SeUiParam::FONT_HEIGHT].dim;
    const SeColorUnpacked backgroundColor = se_color_unpack(g_uiCtx.currentParams[SeUiParam::PRIMARY_COLOR].color);
    const SeColorUnpacked pivotColor = se_color_unpack(g_uiCtx.currentParams[SeUiParam::ACCENT_COLOR].color);
    const float width = 1337.0f * 1984.0f; // @HACK : just a very big number
    const float height = fontHeight + border * 2.0f;

    // Draw backgound
    const SeUiQuadCoords quad = _se_ui_get_corners(width, height);
    const uint32_t backgroundColorIndex = _se_ui_set_draw_call({ backgroundColor, SeUiRenderColorsUnpacked::MODE_SIMPLE });
    _se_ui_push_quad(backgroundColorIndex, quad);

    // Draw pivot
    if (isLastClicked)
    {
        const float pivotOffsetX = _se_ui_get_text_pivot_offset(data->codepoints, data->codepointsPivot);
        const uint32_t pivotColorIndex = _se_ui_set_draw_call({ pivotColor, SeUiRenderColorsUnpacked::MODE_SIMPLE });
        _se_ui_push_quad(pivotColorIndex, { quad.blX + pivotOffsetX, quad.blY, quad.blX + pivotOffsetX + 2.0f, quad.trY });
    }

    // Draw text
    float textWidth = 0.0f;
    if (data->codepointsSize)
    {
        textWidth = _se_ui_draw_text_at_pos(data->codepoints, quad.blX + border, quad.trY - _se_ui_get_current_font_ascent() - border);
    }
    else
    {
        const SeColorUnpacked tint { 1.0f, 1.0f, 1.0f, 0.1f };
        textWidth = _se_ui_draw_text_at_pos(info.hintText, quad.blX + border, quad.trY - _se_ui_get_current_font_ascent() - border, tint);
    }

    //
    // Update data
    //
    const bool isUnderCursor = _se_ui_is_under_cursor(quad) && (g_uiCtx.hasWorkRegion ? _se_ui_is_under_cursor(g_uiCtx.workRegion) : true);
    if (g_uiCtx.isMouseJustDown && isUnderCursor)
    {
        g_uiCtx.lastClickedObjectUid = uid;
    }
    if (g_uiCtx.hasWorkRegion)
    {
        g_uiCtx.workRegion.trY = se_min(g_uiCtx.workRegion.trY, quad.blY);
    }
    SeUiObjectData* const window = se_hash_table_get(g_uiCtx.uidToObjectData, g_uiCtx.currentWindowUid);
    if (window)
    {
        window->scrollInfoX.absoluteContentSize = se_max(window->scrollInfoX.absoluteContentSize, textWidth + border * 2.0f);
        window->scrollInfoY.absoluteContentSize += height;
    }

    return data->utf8text;
}

void _se_ui_init()
{
    g_uiCtx = { };
    g_uiCtx.drawUiVs = se_render_program({ se_data_provider_from_file("ui_draw.vert.spv") });
    g_uiCtx.drawUiFs = se_render_program({ se_data_provider_from_file("ui_draw.frag.spv") });
    g_uiCtx.sampler = se_render_sampler
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
    
    se_hash_table_construct(g_uiCtx.fontInfos, se_allocator_persistent());
    se_hash_table_construct(g_uiCtx.fontGroups, se_allocator_persistent());
    se_hash_table_construct(g_uiCtx.uidToObjectData, se_allocator_persistent());
}

void _se_ui_terminate()
{
    for (auto it : g_uiCtx.uidToObjectData)
    {
        SeUiObjectData& data = se_iterator_value(it);
        if (data.codepoints) se_dealloc(se_allocator_persistent(), data.codepoints, data.codepointsCapacity * sizeof(SeUtf32Char));
        if (data.utf8text) se_dealloc(se_allocator_persistent(), data.utf8text, data.utf8textCapacity);

        SeString key = se_iterator_key(it);
        se_string_destroy(key);
    }
    se_hash_table_destroy(g_uiCtx.uidToObjectData);

    for (auto it : g_uiCtx.fontGroups) _se_font_group_destroy(se_iterator_value(it));
    se_hash_table_destroy(g_uiCtx.fontGroups);

    for (auto it : g_uiCtx.fontInfos) _se_font_info_destroy(se_iterator_value(it));
    se_hash_table_destroy(g_uiCtx.fontInfos);
}
