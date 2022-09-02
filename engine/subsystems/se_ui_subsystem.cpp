
#include "se_ui_subsystem.hpp"
#include "engine/engine.hpp"

#define STBTT_malloc(size, userData) ((void)(userData), ui_stbtt_malloc(size))
void* ui_stbtt_malloc(size_t size)
{
    AllocatorBindings allocator = app_allocators::frame();
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
// Iterator for utf-8 codepoints
//
// =======================================================================

struct Utf8Iterator
{
    const void* data;
    size_t index;

    bool            operator != (const Utf8Iterator& other) const;
    uint32_t        operator *  ();
    Utf8Iterator&   operator ++ ();
};

struct Utf8
{
    const void* data;
};

Utf8Iterator begin(Utf8 data)
{
    return { data.data, 0 };
}

Utf8Iterator end(Utf8 data)
{
    return { &data.data, strlen((const char*)data.data) };
}

bool Utf8Iterator::operator != (const Utf8Iterator& other) const
{
    return data != other.data && index != other.index;
}

uint32_t Utf8Iterator::operator * ()
{
    const uint8_t* const utf8Sequence = ((const uint8_t*)data) + index;
    uint32_t result = 0;
    if ((utf8Sequence[0] & 0x80) == 0)
    {
        result = utf8Sequence[0];
    }
    else if ((utf8Sequence[0] & 0xe0) == 0xc0)
    {
        result =
            (((uint32_t)utf8Sequence[0] & 0x1f) << 6)   |
            (((uint32_t)utf8Sequence[1] & 0x3f));
    }
    else if ((utf8Sequence[0] & 0xf0) == 0xe0)
    {
        result =
            (((uint32_t)utf8Sequence[0] & 0x0f) << 12)  |
            (((uint32_t)utf8Sequence[1] & 0x3f) << 6)   |
            (((uint32_t)utf8Sequence[2] & 0x3f));
    }
    else
    {
        result =
            (((uint32_t)utf8Sequence[0] & 0x07) << 18)  |
            (((uint32_t)utf8Sequence[1] & 0x3f) << 12)  |
            (((uint32_t)utf8Sequence[2] & 0x3f) << 6)   |
            (((uint32_t)utf8Sequence[3] & 0x3f));
    }
    return result;
}

Utf8Iterator& Utf8Iterator::operator ++ ()
{
    // This is based on https://github.com/skeeto/branchless-utf8/blob/master/utf8.h
    static const char lengths[] =
    {
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 3, 3, 4, 0
    };
    const uint8_t firstByte = ((const uint8_t*)data)[index];
    index += lengths[firstByte >> 3];
    return *this;
}

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
            dynamic_array::construct(builder.occupiedNodes, app_allocators::frame(), 64);
            dynamic_array::construct(builder.freeNodes, app_allocators::frame(), 64);
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
            AllocatorBindings allocator = app_allocators::persistent();
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
        AllocatorBindings allocator = app_allocators::persistent();
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
        AllocatorBindings allocator = app_allocators::persistent();
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
        result.supportedCodepoints = dynamic_array::create<uint32_t>(app_allocators::persistent(), 256);
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
        AllocatorBindings allocator = app_allocators::persistent();
        allocator.dealloc(allocator.allocator, font.memory, font.memorySize);
        dynamic_array::destroy(font.supportedCodepoints);
    }
}

// =======================================================================
//
// Codepoint hash table
// Special variation of hash table, optimized for a uint32_t codepoint keys
// Mostly a copy paste from original HashTable
//
// =======================================================================

template<typename T>
struct CodepointHashTable
{
    struct Entry
    {
        uint32_t key;
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
    T* set(CodepointHashTable<T>& table, uint32_t key, const T& value)
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
    size_t index_of(const CodepointHashTable<T>& table, uint32_t key)
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
    inline T* get(CodepointHashTable<T>& table, uint32_t key)
    {
        const size_t position = index_of(table, key);
        return position != table.capacity ? &table.memory[position].value : nullptr;
    }

    template<typename T>
    inline const T* get(const CodepointHashTable<T>& table, uint32_t key)
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
    uint32_t                                    key;
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
    uint32_t key(const CodepointHashTableIteratorValue<Value, Table>& val)
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
    SeTextureInfo                       textureInfo;
    CodepointHashTable<CodepointInfo>   codepointToInfo;
    int                                 lineGapUnscaled;
    int                                 ascentUnscaled;
    int                                 descentUnscaled;
};

namespace font_group
{
    inline int direct_cast_to_int(uint32_t value)
    {
        return *((int*)&value);
    };

    FontGroup create(const FontInfo** fonts, size_t numFonts)
    {
        FontGroup groupInfo
        {
            .fonts              = { },
            .atlas              = { },
            .textureInfo        = { },
            .codepointToInfo    = { },
            .lineGapUnscaled    = 0,
            .ascentUnscaled     = 0,
        };
        //
        // Load all fonts and map codepoints to fonts
        //
        CodepointHashTable<const FontInfo*> codepointToFont = codepoint_hash_table::create<const FontInfo*>(app_allocators::frame(), 256);
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
        CodepointHashTable<CodepointBitmapInfo> codepointToBitmap = codepoint_hash_table::create<CodepointBitmapInfo>(app_allocators::frame(), numCodepoints);
        groupInfo.codepointToInfo = codepoint_hash_table::create<FontGroup::CodepointInfo>(app_allocators::persistent(), numCodepoints);
        for (auto it : codepointToFont)
        {
            //
            // Bitmap
            //
            const uint32_t codepoint = iter::key(it);
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
        HashTable<RenderAtlasRect, uint32_t> atlasRectToCodepoint = hash_table::create<RenderAtlasRect, uint32_t>(app_allocators::frame(), numCodepoints);
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
        groupInfo.textureInfo =
        {
            .width  = groupInfo.atlas.width,
            .height = groupInfo.atlas.height,
            .format = SE_TEXTURE_FORMAT_R_8,
            .data   = data_provider::from_memory(groupInfo.atlas.bitmap, groupInfo.atlas.width * groupInfo.atlas.height * sizeof(uint8_t)),
        };
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

    ColorUnpacked   tint;
    int32_t         mode;
    float           _pad[3];
};

struct UiDrawCall
{
    DynamicArray<UiRenderVertex>            vertices;
    DynamicArray<UiRenderColorsUnpacked>    colorsArray;
    SeRenderRef                             texture;
};

struct UiTextureData
{
    enum Type
    {
        TYPE_FONT,
        TYPE_WINDOW,
    };
    Type type;
    union
    {
        struct { const FontGroup* fontGroup; } font;
        struct { DataProvider data; } window;
    };
};

using UiParams = SeUiParam[SeUiParam::_COUNT];

struct UiActionFlags
{
    enum
    {
        CAN_BE_MOVED            = 0x00000001,
        CAN_BE_RESIZED_X_LEFT   = 0x00000002,
        CAN_BE_RESIZED_X_RIGHT  = 0x00000004,
        CAN_BE_RESIZED_Y_TOP    = 0x00000008,
        CAN_BE_RESIZED_Y_BOTTOM = 0x00000010,
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

struct UiObjectData
{
    float               bottomLeftX;
    float               bottomLeftY;
    float               topRightX;
    float               topRightY;
    float               minWidth;
    float               minHeight;
    SeUiFlags::Type     settingsFlags;
    UiActionFlags::Type actionFlags;
    UiStateFlags::Type  stateFlags;
};

struct UiContext
{
    const SeRenderAbstractionSubsystemInterface*    render;
    SePassRenderTarget                              target;

    const FontGroup*                                currentFontGroup;
    UiParams                                        currentParams;
    
    DynamicArray<UiDrawCall>                        frameDrawCalls;
    HashTable<UiTextureData, SeRenderRef>           frameTextureDataToRef;
    float                                           mouseX;
    float                                           mouseY;
    float                                           mouseDeltaX;
    float                                           mouseDeltaY;
    bool                                            isMouseDown;
    bool                                            isMouseJustDown;

    SeString                                        previousHoveredObjectUid;
    SeString                                        currentHoveredObjectUid;
    SeString                                        previousActiveObjectUid;
    SeString                                        currentActiveObjectUid;
    SeString                                        currentWindowUid;
    bool                                            isJustActivated;

    float                                           workRegionBottomLeftX;
    float                                           workRegionBottomLeftY;
    float                                           workRegionTopRightX;
    float                                           workRegionTopRightY;

    HashTable<SeString, UiObjectData>               uidToObjectData;
};

const UiParams DEFAULT_PARAMS =
{
    /* FONT_COLOR                   */ { .color = col::pack({ 1.0f, 1.0f, 1.0f, 1.0f }) },
    /* FONT_HEIGHT                  */ { .dim = 10.0f },
    /* FONT_LINE_STEP               */ { .dim = 12.0f },
    /* PRIMARY_COLOR                */ { .color = col::pack({ 0.87f, 0.81f, 0.83f, 1.0f }) },
    /* SECONDARY_COLOR              */ { .color = col::pack({ 0.36f, 0.45f, 0.58f, 0.8f }) },
    /* ACCENT_COLOR                 */ { .color = col::pack({ 0.95f, 0.75f, 0.79f, 1.0f }) },
    /* WINDOW_TOP_PANEL_THICKNESS   */ { .dim = 16.0f },
    /* WINDOW_BORDER_THICKNESS      */ { .dim = 2.0f },
    /* PIVOT_POSITION_X             */ { .dim = 0.0f },
    /* PIVOT_POSITION_Y             */ { .dim = 0.0f },
    /* PIVOT_TYPE_X                 */ { .pivot = SeUiPivotType::BOTTOM_LEFT },
    /* PIVOT_TYPE_Y                 */ { .pivot = SeUiPivotType::BOTTOM_LEFT },
    /* BUTTON_BORDER_SIZE           */ { .dim = 5.0f },
};

SeUiSubsystemInterface                  g_iface;
UiContext                               g_currentContext;

HashTable<DataProvider, FontInfo>       g_fontInfos;
HashTable<SeUiFontGroupInfo, FontGroup> g_fontGroups;

DataProvider                            g_drawUiVs;
DataProvider                            g_drawUiFs;

namespace hash_value
{
    namespace builder
    {
        template<>
        void absorb<UiTextureData>(HashValueBuilder& builder, const UiTextureData& value)
        {
            switch (value.type)
            {
                case UiTextureData::TYPE_FONT:      { hash_value::builder::absorb(builder, value.font.fontGroup); } break;
                case UiTextureData::TYPE_WINDOW:    { hash_value::builder::absorb(builder, value.window.data); } break;
                default:                            { }
            }
        }
    }

    template<>
    HashValue generate<UiTextureData>(const UiTextureData& value)
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

    template<>
    bool compare<UiTextureData, UiTextureData>(const UiTextureData& first, const UiTextureData& second)
    {
        if (first.type != second.type) return false;
        if (first.type == UiTextureData::TYPE_FONT)
        {
            return compare(first.font.fontGroup, second.font.fontGroup);
        }
        else if (first.type == UiTextureData::TYPE_WINDOW)
        {
            return compare(first.window.data, second.window.data);
        }
        else
        {
            se_assert(false);
            return false;
        }
    }
}

namespace ui_impl
{
    inline UiTextureData ui_texture_data_create(const FontGroup* fontGroup)
    {
        return
        {
            .type = UiTextureData::TYPE_FONT,
            .font = { fontGroup },
        };
    }

    inline UiTextureData ui_texture_data_create(DataProvider windowTextureData)
    {
        return
        {
            .type = UiTextureData::TYPE_WINDOW,
            .window = { windowTextureData },
        };
    }

    struct { SeString uid; UiObjectData* data; bool isFirst; } object_data_get(const char* cstrUid)
    {
        UiObjectData* data = hash_table::get(g_currentContext.uidToObjectData, cstrUid);
        const bool isFirstAccess = data == nullptr;
        if (!data)
        {
            data = hash_table::set(g_currentContext.uidToObjectData, string::create(cstrUid, SeStringLifetime::Persistent), { });
        }
        se_assert(data);
        return { hash_table::key(g_currentContext.uidToObjectData, data), data, isFirstAccess };
    }

    struct { UiDrawCall* drawCall; uint32_t colorIndex; } get_draw_call(const UiRenderColorsUnpacked& colors)
    {
        const size_t numDrawCalls = dynamic_array::size(g_currentContext.frameDrawCalls);
        const UiDrawCall* const lastDrawCall = numDrawCalls ? &g_currentContext.frameDrawCalls[numDrawCalls - 1] : nullptr;
        if (!lastDrawCall)
        {
            UiDrawCall& pushedDrawCall = dynamic_array::push(g_currentContext.frameDrawCalls,
            {
                .vertices       = dynamic_array::create<UiRenderVertex>(app_allocators::frame(), 256),
                .colorsArray    = dynamic_array::create<UiRenderColorsUnpacked>(app_allocators::frame(), 256),
                .texture        = NULL_RENDER_REF,
            });
            dynamic_array::push(pushedDrawCall.colorsArray, colors);
        }
        UiDrawCall* const drawCall = &g_currentContext.frameDrawCalls[dynamic_array::size(g_currentContext.frameDrawCalls) - 1];
        if (!utils::compare(drawCall->colorsArray[dynamic_array::size(drawCall->colorsArray) - 1], colors))
        {
            dynamic_array::push(drawCall->colorsArray, colors);
        }
        return { drawCall, dynamic_array::size<uint32_t>(drawCall->colorsArray) - 1 };
    }

    struct { UiDrawCall* drawCall; uint32_t colorIndex; } get_draw_call(const UiTextureData& textureData, const SeTextureInfo& textureInfo, const UiRenderColorsUnpacked& colors)
    {
        //
        // Get texture
        //
        SeRenderRef* textureRef = hash_table::get(g_currentContext.frameTextureDataToRef, textureData);
        if (!textureRef)
        {
            const SeRenderAbstractionSubsystemInterface* render = g_currentContext.render;
            textureRef = hash_table::set(g_currentContext.frameTextureDataToRef, textureData, render->texture(textureInfo));
        }
        se_assert(textureRef);
        //
        // Get draw call
        //
        const size_t numDrawCalls = dynamic_array::size(g_currentContext.frameDrawCalls);
        const UiDrawCall* const lastDrawCall = numDrawCalls ? &g_currentContext.frameDrawCalls[numDrawCalls - 1] : nullptr;
        if (!lastDrawCall || (lastDrawCall->texture != NULL_RENDER_REF && lastDrawCall->texture != *textureRef))
        {
            UiDrawCall& pushedDrawCall = dynamic_array::push(g_currentContext.frameDrawCalls,
            {
                .vertices       = dynamic_array::create<UiRenderVertex>(app_allocators::frame(), 256),
                .colorsArray    = dynamic_array::create<UiRenderColorsUnpacked>(app_allocators::frame(), 256),
                .texture        = *textureRef,
            });
            dynamic_array::push(pushedDrawCall.colorsArray, colors);
        }
        //
        // Push texture and color
        //
        UiDrawCall* const drawCall = &g_currentContext.frameDrawCalls[dynamic_array::size(g_currentContext.frameDrawCalls) - 1];
        if (drawCall->texture == NULL_RENDER_REF)
        {
            drawCall->texture = *textureRef;
        }
        se_assert(drawCall->texture == *textureRef);
        if (!utils::compare(drawCall->colorsArray[dynamic_array::size(drawCall->colorsArray) - 1], colors))
        {
            dynamic_array::push(drawCall->colorsArray, colors);
        }
        return { drawCall, dynamic_array::size<uint32_t>(drawCall->colorsArray) - 1 };
    }

    inline void push_quad(UiDrawCall* drawCall, uint32_t colorIndex, float x1, float y1, float x2, float y2)
    {
        dynamic_array::push(drawCall->vertices, { x1, y1, 0, 0, colorIndex });
        dynamic_array::push(drawCall->vertices, { x1, y2, 0, 0, colorIndex });
        dynamic_array::push(drawCall->vertices, { x2, y2, 0, 0, colorIndex });
        dynamic_array::push(drawCall->vertices, { x1, y1, 0, 0, colorIndex });
        dynamic_array::push(drawCall->vertices, { x2, y2, 0, 0, colorIndex });
        dynamic_array::push(drawCall->vertices, { x2, y1, 0, 0, colorIndex });
    };

    inline struct { float x1; float y1; float x2; float y2; } get_corners(float width, float height)
    {
        const bool isCenteredX = g_currentContext.currentParams[SeUiParam::PIVOT_TYPE_X].pivot == SeUiPivotType::CENTER;
        const bool isCenteredY = g_currentContext.currentParams[SeUiParam::PIVOT_TYPE_Y].pivot == SeUiPivotType::CENTER;
        const float dimX = g_currentContext.currentParams[SeUiParam::PIVOT_POSITION_X].dim - (isCenteredX ? width / 2.0f : 0.0f);
        const float dimY = g_currentContext.currentParams[SeUiParam::PIVOT_POSITION_Y].dim - (isCenteredY ? height / 2.0f : 0.0f);
        return { dimX, dimY, dimX + width, dimY + height };
    }

    inline bool is_under_cursor(float blX, float blY, float trX, float trY)
    {
        return
            (blX <= g_currentContext.mouseX) &&
            (blY <= g_currentContext.mouseY) &&
            (trX >= g_currentContext.mouseX) &&
            (trY >= g_currentContext.mouseY);
    }

    struct { float x1; float y1; float x2; float y2; } get_text_bbox(const char* utf8text)
    {
        const FontGroup* const fontGroup = g_currentContext.currentFontGroup;
        const float textHeight = g_currentContext.currentParams[SeUiParam::FONT_HEIGHT].dim;
        const float baselineX = 0;
        const float baselineY = 0;
        float positionX = 0;
        float biggestAscent = 0;
        float biggestDescent = 0;
        int previousCodepoint = 0;
        for (uint32_t codepoint : Utf8{ utf8text })
        {
            const FontGroup::CodepointInfo* const codepointInfo = codepoint_hash_table::get(fontGroup->codepointToInfo, codepoint);
            se_assert(codepointInfo);
            const FontInfo* const font = codepointInfo->font;
            // This scale calculation is similar to stbtt_ScaleForPixelHeight but ignores descend parameter of the font
            const float scale = textHeight / (float)font->ascentUnscaled;
            const float advance = scale * (float)codepointInfo->advanceWidthUnscaled;
            const float bearing = scale * (float)codepointInfo->leftSideBearingUnscaled;
            const int codepointSigned = font_group::direct_cast_to_int(codepoint);
            int x0;
            int y0;
            int x1;
            int y1;
            stbtt_GetCodepointBitmapBox(&font->stbInfo, codepointSigned, scale, scale, &x0, &y0, &x1, &y1);
            const float descent = (float)y1;
            if (biggestDescent < descent) biggestDescent = descent;
            const float ascent = (float)-y0;
            if (biggestAscent < ascent) biggestAscent = ascent;
            const float additionalAdvance = scale * (float)stbtt_GetCodepointKernAdvance(&font->stbInfo, previousCodepoint, codepointSigned);
            positionX += additionalAdvance + advance;
            previousCodepoint = codepointSigned;
        }
        return { 0, -biggestDescent, positionX, biggestAscent };
    }

    bool begin(const SeUiBeginInfo& info)
    {
        se_assert(info.render);
        
        g_currentContext.render            = info.render;
        g_currentContext.target            = info.target;
        g_currentContext.currentFontGroup  = nullptr;
        memcpy(g_currentContext.currentParams, DEFAULT_PARAMS, sizeof(UiParams));
        dynamic_array::construct(g_currentContext.frameDrawCalls, app_allocators::frame(), 16);
        hash_table::construct(g_currentContext.frameTextureDataToRef, app_allocators::frame(), 16);

        const SeWindowSubsystemInput* const input = win::get_input();
        const float mouseX = (float)input->mouseX;
        const float mouseY = (float)input->mouseY;
        g_currentContext.mouseDeltaX       = mouseX - g_currentContext.mouseX;
        g_currentContext.mouseDeltaY       = mouseY - g_currentContext.mouseY;
        g_currentContext.mouseX            = mouseX;
        g_currentContext.mouseY            = mouseY;
        g_currentContext.isMouseDown       = win::is_mouse_button_pressed(input, SE_LMB);
        g_currentContext.isMouseJustDown   = win::is_mouse_button_just_pressed(input, SE_LMB);
        g_currentContext.isJustActivated =
            utils::compare(g_currentContext.previousActiveObjectUid, SeString{ }) &&
            !utils::compare(g_currentContext.currentActiveObjectUid, SeString{ });
        g_currentContext.previousHoveredObjectUid = g_currentContext.currentHoveredObjectUid;
        g_currentContext.previousActiveObjectUid = g_currentContext.currentActiveObjectUid;
        g_currentContext.currentHoveredObjectUid = { };
        g_currentContext.currentWindowUid = { };
        if (!g_currentContext.isMouseDown)
        {
            g_currentContext.currentActiveObjectUid = { };
        }
        else
        {
            UiObjectData* const activeObject = hash_table::get(g_currentContext.uidToObjectData, g_currentContext.currentActiveObjectUid);
            if (activeObject)
            {
                const float currentWidth = activeObject->topRightX - activeObject->bottomLeftX;
                const float currentHeight = activeObject->topRightY - activeObject->bottomLeftY;
                se_assert(currentWidth >= 0.0f);
                se_assert(currentHeight >= 0.0f);
                const bool isPositiveDeltaX = g_currentContext.mouseDeltaX >= 0.0f;
                const bool isPositiveDeltaY = g_currentContext.mouseDeltaY >= 0.0f;
                const float maxDeltaTowardsCenterX = (activeObject->topRightX - activeObject->bottomLeftX) - activeObject->minWidth;
                const float maxDeltaTowardsCenterY = (activeObject->topRightY - activeObject->bottomLeftY) - activeObject->minHeight;
                se_assert(maxDeltaTowardsCenterX >= 0.0f);
                se_assert(maxDeltaTowardsCenterY >= 0.0f);
                if ((activeObject->settingsFlags & SeUiFlags::MOVABLE) && (activeObject->actionFlags & UiActionFlags::CAN_BE_MOVED))
                {
                    activeObject->bottomLeftX = activeObject->bottomLeftX + g_currentContext.mouseDeltaX;
                    activeObject->bottomLeftY = activeObject->bottomLeftY + g_currentContext.mouseDeltaY;
                    activeObject->topRightX = activeObject->topRightX + g_currentContext.mouseDeltaX;
                    activeObject->topRightY = activeObject->topRightY + g_currentContext.mouseDeltaY;
                }
                if ((activeObject->settingsFlags & SeUiFlags::RESIZABLE_X) && (activeObject->actionFlags & UiActionFlags::CAN_BE_RESIZED_X_LEFT))
                {
                    const bool canChangeSize = isPositiveDeltaX ? g_currentContext.mouseX > activeObject->bottomLeftX : g_currentContext.mouseX < activeObject->bottomLeftX;
                    const float actualDelta = canChangeSize
                        ? (g_currentContext.mouseDeltaX > maxDeltaTowardsCenterX ? maxDeltaTowardsCenterX : g_currentContext.mouseDeltaX)
                        : 0.0f;
                    activeObject->bottomLeftX = activeObject->bottomLeftX + actualDelta;
                }
                if ((activeObject->settingsFlags & SeUiFlags::RESIZABLE_X) && (activeObject->actionFlags & UiActionFlags::CAN_BE_RESIZED_X_RIGHT))
                {
                    const bool canChangeSize = isPositiveDeltaX ? g_currentContext.mouseX > activeObject->topRightX : g_currentContext.mouseX < activeObject->topRightX;
                    const float actualDelta = canChangeSize
                        ? (g_currentContext.mouseDeltaX < -maxDeltaTowardsCenterX ? -maxDeltaTowardsCenterX : g_currentContext.mouseDeltaX)
                        : 0.0f;
                    activeObject->topRightX = activeObject->topRightX + actualDelta;
                }
                if ((activeObject->settingsFlags & SeUiFlags::RESIZABLE_Y) && (activeObject->actionFlags & UiActionFlags::CAN_BE_RESIZED_Y_TOP))
                {
                    const bool canChangeSize = isPositiveDeltaY ? g_currentContext.mouseY > activeObject->topRightY : g_currentContext.mouseY < activeObject->topRightY;
                    const float actualDelta = canChangeSize
                        ? (g_currentContext.mouseDeltaY < -maxDeltaTowardsCenterY ? -maxDeltaTowardsCenterY : g_currentContext.mouseDeltaY)
                        : 0.0f;
                    activeObject->topRightY = activeObject->topRightY + actualDelta;
                }
                if ((activeObject->settingsFlags & SeUiFlags::RESIZABLE_Y) && (activeObject->actionFlags & UiActionFlags::CAN_BE_RESIZED_Y_BOTTOM))
                {
                    const bool canChangeSize = isPositiveDeltaY ? g_currentContext.mouseY > activeObject->bottomLeftY : g_currentContext.mouseY < activeObject->bottomLeftY;
                    const float actualDelta = canChangeSize
                        ? (g_currentContext.mouseDeltaY > maxDeltaTowardsCenterY ? maxDeltaTowardsCenterY : g_currentContext.mouseDeltaY)
                        : 0.0f;
                    activeObject->bottomLeftY = activeObject->bottomLeftY + actualDelta;
                }
            }
        }
        return true;
    }

    SePassDependencies end(SePassDependencies dependencies)
    {
        const SeRenderAbstractionSubsystemInterface* const render = g_currentContext.render;
        const SeRenderRef vertexProgram = render->program({ g_drawUiVs });
        const SeRenderRef fragmentProgram = render->program({ g_drawUiFs });
        const SeRenderRef sampler = render->sampler
        ({
            .magFilter          = SE_SAMPLER_FILTER_LINEAR,
            .minFilter          = SE_SAMPLER_FILTER_LINEAR,
            .addressModeU       = SE_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeV       = SE_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeW       = SE_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .mipmapMode         = SE_SAMPLER_MIPMAP_MODE_LINEAR,
            .mipLodBias         = 0.0f,
            .minLod             = 0.0f,
            .maxLod             = 0.0f,
            .anisotropyEnable   = false,
            .maxAnisotropy      = 0.0f,
            .compareEnabled     = false,
            .compareOp          = SE_COMPARE_OP_ALWAYS,
        });
        const SeRenderRef pipeline = render->graphics_pipeline
        ({
            .vertexProgram          = { .program = vertexProgram, },
            .fragmentProgram        = { .program = fragmentProgram, },
            .frontStencilOpState    = { .isEnabled = false, },
            .backStencilOpState     = { .isEnabled = false, },
            .depthState             = { .isTestEnabled = false, .isWriteEnabled = false, },
            .polygonMode            = SE_PIPELINE_POLYGON_FILL_MODE_FILL,
            .cullMode               = SE_PIPELINE_CULL_MODE_NONE,
            .frontFace              = SE_PIPELINE_FRONT_FACE_CLOCKWISE,
            .samplingType           = SE_SAMPLING_1,
        });
        const SePassDependencies result = render->begin_pass
        ({
            .dependencies       = dependencies,
            .pipeline           = pipeline,
            .renderTargets      = { g_currentContext.target },
            .numRenderTargets   = 1,
            .depthStencilTarget = 0,
            .hasDepthStencil    = false,
        });
        //
        // Bind projection matrix
        //
        const SeTextureSize targetSize = render->texture_size(g_currentContext.target.texture);
        se_assert(targetSize.z == 1);
        const SeRenderRef projectionBuffer = render->memory_buffer
        ({
            data_provider::from_memory
            (
                float4x4::transposed(render->orthographic(0, (float)targetSize.x, 0, (float)targetSize.y, 0, 2))
            )
        });
        render->bind({ .set = 0, .bindings = { { 0, projectionBuffer } } });
        //
        // Process draw calls
        //
        for (auto it : g_currentContext.frameDrawCalls)
        {
            const UiDrawCall& drawCall = iter::value(it);
            if (!dynamic_array::size(drawCall.vertices))
            {
                continue;
            }
            const SeRenderRef colorsBuffer = render->memory_buffer
            ({
                data_provider::from_memory(dynamic_array::raw(drawCall.colorsArray), dynamic_array::raw_size(drawCall.colorsArray))
            });
            const SeRenderRef verticesBuffer = render->memory_buffer
            ({
                data_provider::from_memory(dynamic_array::raw(drawCall.vertices), dynamic_array::raw_size(drawCall.vertices))
            });
            render->bind
            ({
                .set = 1,
                .bindings =
                {
                    { 0, drawCall.texture, sampler },
                    { 1, colorsBuffer },
                    { 2, verticesBuffer },
                },
            });
            render->draw({ dynamic_array::size<uint32_t>(drawCall.vertices), 1 });
        }
        render->end_pass();
        //
        // Clear context
        //
        for (auto it : g_currentContext.frameDrawCalls)
        {
            UiDrawCall& drawCall = iter::value(it);
            dynamic_array::destroy(drawCall.vertices);
            dynamic_array::destroy(drawCall.colorsArray);
        }
        dynamic_array::destroy(g_currentContext.frameDrawCalls);
        hash_table::destroy(g_currentContext.frameTextureDataToRef);
        return result;
    }

    void set_font_group(const SeUiFontGroupInfo& info)
    {
        const FontGroup* fontGroup = hash_table::get(g_fontGroups, info);
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
                const FontInfo* font = hash_table::get(g_fontInfos, fontData);
                if (!font)
                {
                    font = hash_table::set(g_fontInfos, fontData, font_info::create(fontData));
                }
                se_assert(font);
                fonts[fontIt] = font;
            };
            fontGroup = hash_table::set(g_fontGroups, info, font_group::create(fonts, fontIt));
        }
        se_assert(fontGroup);
        g_currentContext.currentFontGroup = fontGroup;
    }

    void set_param(SeUiParam::Type type, const SeUiParam& param)
    {
        g_currentContext.currentParams[type] = param;
    }

    void text(const SeUiTextInfo& info)
    {
        se_assert(info.utf8text);
        se_assert(g_currentContext.currentFontGroup);
        //
        // Get base data and draw call
        //
        const FontGroup* const fontGroup = g_currentContext.currentFontGroup;
        const float textHeight = g_currentContext.currentParams[SeUiParam::FONT_HEIGHT].dim;
        const float lineStep = g_currentContext.currentParams[SeUiParam::FONT_LINE_STEP].dim;
        const float baselineX = g_currentContext.currentParams[SeUiParam::PIVOT_POSITION_X].dim;
        const float baselineY = g_currentContext.currentParams[SeUiParam::PIVOT_POSITION_Y].dim;
        const auto [drawCall, colorIndex] = get_draw_call(ui_texture_data_create(fontGroup), fontGroup->textureInfo,
        {
            .tint = col::unpack(g_currentContext.currentParams[SeUiParam::FONT_COLOR].color),
            .mode = UiRenderColorsUnpacked::MODE_TEXT,
        });
        const UiObjectData* const activeWindow = hash_table::get(g_currentContext.uidToObjectData, g_currentContext.currentWindowUid);
        //
        // Process codepoints
        //
        const float positionYStep = lineStep;
        float positionX = activeWindow ? g_currentContext.workRegionBottomLeftX : baselineX;
        float positionY = activeWindow ? g_currentContext.workRegionTopRightY - lineStep : baselineY;
        int previousCodepoint = 0;
        const size_t from = dynamic_array::size(drawCall->vertices);
        for (uint32_t codepoint : Utf8{ info.utf8text })
        {
            const FontGroup::CodepointInfo* const codepointInfo = codepoint_hash_table::get(fontGroup->codepointToInfo, codepoint);
            se_assert(codepointInfo);
            const FontInfo* const font = codepointInfo->font;
            // This scale calculation is similar to stbtt_ScaleForPixelHeight but ignores descend parameter of the font
            const float scale = textHeight / (float)font->ascentUnscaled;
            const float advance = scale * (float)codepointInfo->advanceWidthUnscaled;
            const float bearing = scale * (float)codepointInfo->leftSideBearingUnscaled;
            const int codepointSigned = font_group::direct_cast_to_int(codepoint);
            int x0;
            int y0;
            int x1;
            int y1;
            stbtt_GetCodepointBitmapBox(&font->stbInfo, codepointSigned, scale, scale, &x0, &y0, &x1, &y1);
            const float width = (float)(x1 - x0);
            const float height = (float)(y1 - y0);
            float baseX = positionX + (float)x0 - bearing;
            float baseY = positionY - (float)y1;
            // Clamp to active window
            if (activeWindow)
            {
                bool doesFitVertically = true;
                while (true)
                {
                    baseX = positionX + (float)x0 - bearing;
                    baseY = positionY - (float)y1;
                    doesFitVertically = baseY > g_currentContext.workRegionBottomLeftY;
                    if (!doesFitVertically)
                    {
                        break;
                    }
                    const bool doesFitHorisontally = (baseX + width) < g_currentContext.workRegionTopRightX;
                    if (doesFitHorisontally)
                    {
                        break;
                    }
                    previousCodepoint = 0;
                    positionY -= positionYStep;
                    positionX = g_currentContext.workRegionBottomLeftX;
                }
                if (!doesFitVertically) break;
            }
            const float additionalAdvance = scale * (float)stbtt_GetCodepointKernAdvance(&font->stbInfo, previousCodepoint, codepointSigned);
            positionX += additionalAdvance;
            const RenderAtlasRectNormalized& rect = fontGroup->atlas.uvRects[codepointInfo->atlasRectIndex];
            dynamic_array::push(drawCall->vertices, { baseX        , baseY         , rect.p1x, rect.p1y, colorIndex, });
            dynamic_array::push(drawCall->vertices, { baseX        , baseY + height, rect.p1x, rect.p2y, colorIndex, });
            dynamic_array::push(drawCall->vertices, { baseX + width, baseY + height, rect.p2x, rect.p2y, colorIndex, });
            dynamic_array::push(drawCall->vertices, { baseX        , baseY         , rect.p1x, rect.p1y, colorIndex, });
            dynamic_array::push(drawCall->vertices, { baseX + width, baseY + height, rect.p2x, rect.p2y, colorIndex, });
            dynamic_array::push(drawCall->vertices, { baseX + width, baseY         , rect.p2x, rect.p1y, colorIndex, });
            positionX += advance;
            previousCodepoint = codepointSigned;
        }
        //
        // Center if needed
        //
        if (activeWindow)
        {
            // @TODO : support SeUiParam::PIVOT_TYPE_X and SeUiParam::PIVOT_TYPE_Y for in-window text
            g_currentContext.workRegionTopRightY = positionY;
        }
        else
        {
            const bool isCenteredX = g_currentContext.currentParams[SeUiParam::PIVOT_TYPE_X].pivot == SeUiPivotType::CENTER;
            const bool isCenteredY = g_currentContext.currentParams[SeUiParam::PIVOT_TYPE_Y].pivot == SeUiPivotType::CENTER;
            if (isCenteredX || isCenteredY)
            {
                const float halfWidth = (positionX - baselineX) / 2.0f;
                const float halfHeight = textHeight / 2.0f;
                const size_t to = dynamic_array::size(drawCall->vertices);
                for (size_t it = from; it < to; it++)
                {
                    UiRenderVertex* const vert = &drawCall->vertices[it];
                    if (isCenteredX) vert->x -= halfWidth;
                    if (isCenteredY) vert->y -= halfHeight;
                }
            }
        }
#if 0
        const auto [drawCallBaseline, colorIndexBaseline] = get_draw_call
        ({
            .tint = { 1, 1, 1, 1 },
            .mode = UiRenderColorsUnpacked::MODE_SIMPLE,
        });
        push_quad(drawCallBaseline, colorIndexBaseline, baselineX, baselineY, baselineX + 2, baselineY + 2);
#endif
    }

    bool begin_window(const SeUiWindowInfo& info)
    {
        static constexpr float BORDER_PANEL_TOLERANCE = 5.0f;
        //
        // Update object data
        //
        auto [uid, data, isFirst] = object_data_get(info.uid);
        auto [bottomLeftX, bottomLeftY, topRightX, topRightY] = get_corners(info.width, info.height);
        const bool isResizeable = info.flags & (SeUiFlags::RESIZABLE_X | SeUiFlags::RESIZABLE_Y);
        if (isResizeable && !isFirst)
        {
            bottomLeftX = data->bottomLeftX;
            bottomLeftY = data->bottomLeftY;
            topRightX = data->topRightX;
            topRightY = data->topRightY;
        }
        data->bottomLeftX = bottomLeftX;
        data->bottomLeftY = bottomLeftY;
        data->topRightX = topRightX;
        data->topRightY = topRightY;
        data->settingsFlags = info.flags;
        const float topPanelThickness = g_currentContext.currentParams[SeUiParam::WINDOW_TOP_PANEL_THICKNESS].dim;
        const float borderThickness = g_currentContext.currentParams[SeUiParam::WINDOW_BORDER_THICKNESS].dim;
        data->minWidth = topPanelThickness * 3.0f;
        data->minHeight = topPanelThickness + borderThickness * 2.0f;
        //
        // Update action flags, active object uid and work region info
        //
        if (is_under_cursor(bottomLeftX, bottomLeftY, topRightX, topRightY))
        {
            g_currentContext.currentHoveredObjectUid = uid;
            if (g_currentContext.isMouseJustDown)
            {
                g_currentContext.currentActiveObjectUid = uid;
                const bool isTopPanelUnderCursor = ((topRightY - topPanelThickness) <= g_currentContext.mouseY);
                const bool isLeftBorderUnderCursor = !isTopPanelUnderCursor && ((bottomLeftX + borderThickness + BORDER_PANEL_TOLERANCE) >= g_currentContext.mouseX);
                const bool isRightBorderUnderCursor = !isTopPanelUnderCursor && ((topRightX - borderThickness - BORDER_PANEL_TOLERANCE) <= g_currentContext.mouseX);
                const bool isBottomBorderUnderCursor = !isTopPanelUnderCursor && ((bottomLeftY + borderThickness + BORDER_PANEL_TOLERANCE) >= g_currentContext.mouseY);
                if (isTopPanelUnderCursor) data->actionFlags |= UiActionFlags::CAN_BE_MOVED;
                if (isLeftBorderUnderCursor) data->actionFlags |= UiActionFlags::CAN_BE_RESIZED_X_LEFT;
                if (isRightBorderUnderCursor) data->actionFlags |= UiActionFlags::CAN_BE_RESIZED_X_RIGHT;
                if (isBottomBorderUnderCursor) data->actionFlags |= UiActionFlags::CAN_BE_RESIZED_Y_BOTTOM;
            }
            else if (!g_currentContext.isMouseDown)
            {
                data->actionFlags = 0;
            }
        }
        g_currentContext.currentWindowUid = uid;
        g_currentContext.workRegionBottomLeftX = bottomLeftX + borderThickness;
        g_currentContext.workRegionBottomLeftY = bottomLeftY + borderThickness;
        g_currentContext.workRegionTopRightX = topRightX - borderThickness;
        g_currentContext.workRegionTopRightY = topRightY - topPanelThickness;
        //
        // Draw top panel and borders
        //
        const auto [drawCallBorders, colorIndexBorders] = get_draw_call
        ({
            .tint = col::unpack(g_currentContext.currentParams[SeUiParam::PRIMARY_COLOR].color),
            .mode = UiRenderColorsUnpacked::MODE_SIMPLE,
        });
        push_quad(drawCallBorders, colorIndexBorders, bottomLeftX, topRightY - topPanelThickness, topRightX, topRightY);
        push_quad(drawCallBorders, colorIndexBorders, bottomLeftX, bottomLeftY, bottomLeftX + borderThickness, topRightY - topPanelThickness);
        push_quad(drawCallBorders, colorIndexBorders, topRightX - borderThickness, bottomLeftY, topRightX, topRightY - topPanelThickness);
        push_quad(drawCallBorders, colorIndexBorders, bottomLeftX + borderThickness, bottomLeftY, topRightX - borderThickness, bottomLeftY + borderThickness);
        //
        // Draw background
        //
        const auto [drawCallBackground, colorIndexBackground] = get_draw_call
        ({
            .tint = col::unpack(g_currentContext.currentParams[SeUiParam::SECONDARY_COLOR].color),
            .mode = UiRenderColorsUnpacked::MODE_SIMPLE,
        });
        push_quad(drawCallBackground, colorIndexBackground, bottomLeftX + borderThickness, bottomLeftY + borderThickness, topRightX - borderThickness, topRightY - topPanelThickness);
        return true;
    }

    void end_window()
    {
        g_currentContext.currentWindowUid = { };
    }

    bool button(const SeUiButtonInfo& info)
    {
        auto [uid, data, isFirst] = object_data_get(info.uid);
        //
        // Get button size
        //
        float buttonWidth = info.width;
        float buttonHeight = info.height;
        float textOffsetY = 0.0f;
        if (info.utf8text)
        {
            const float border = g_currentContext.currentParams[SeUiParam::BUTTON_BORDER_SIZE].dim;
            const auto [bbx1, bby1, bbx2, bby2] = get_text_bbox(info.utf8text);
            const float textWidth = bbx2 - bbx1 + border * 2.0f;
            const float textHeight = bby2 - bby1 + border * 2.0f;
            if (textWidth > buttonWidth) buttonWidth = textWidth;
            if (textHeight > buttonHeight) buttonHeight = textHeight;
            // With negative boundin_box_y1 value we need to draw text above of the pivot, so we negate this value to get a positive result
            textOffsetY = -bby1 + border;
        }
        auto [bottomLeftX, bottomLeftY, topRightX, topRightY] = get_corners(buttonWidth, buttonHeight);
        //
        // Update state
        //
        const bool isUnderCursor = is_under_cursor(bottomLeftX, bottomLeftY, topRightX, topRightY);
        if (isUnderCursor)
        {
            g_currentContext.currentHoveredObjectUid = uid;
            if (g_currentContext.isMouseJustDown)
            {
                g_currentContext.currentActiveObjectUid = uid;
            }
        }
        const bool isPressed = utils::compare(g_currentContext.previousActiveObjectUid, uid);
        if (isPressed && g_currentContext.isJustActivated)
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
        ColorUnpacked buttonColor = col::unpack(g_currentContext.currentParams[SeUiParam::PRIMARY_COLOR].color);
        if (isPressed)
        {
            const float SCALE = 0.5f;
            buttonColor.r *= SCALE;
            buttonColor.g *= SCALE;
            buttonColor.b *= SCALE;
        }
        const auto [drawCallBorders, colorIndexBorders] = get_draw_call({ buttonColor, UiRenderColorsUnpacked::MODE_SIMPLE });
        push_quad(drawCallBorders, colorIndexBorders, bottomLeftX, bottomLeftY, topRightX, topRightY);
        //
        // Draw text
        //
        if (info.utf8text)
        {
            const float cachedPivotX = g_currentContext.currentParams[SeUiParam::PIVOT_POSITION_X].dim;
            const float cachedPivotY = g_currentContext.currentParams[SeUiParam::PIVOT_POSITION_Y].dim;
            const SeUiPivotType cachedPivotTypeX = g_currentContext.currentParams[SeUiParam::PIVOT_TYPE_X].pivot;
            const SeUiPivotType cachedPivotTypeY = g_currentContext.currentParams[SeUiParam::PIVOT_TYPE_Y].pivot;
            g_currentContext.currentParams[SeUiParam::PIVOT_TYPE_X].pivot = SeUiPivotType::CENTER;
            g_currentContext.currentParams[SeUiParam::PIVOT_TYPE_Y].pivot = SeUiPivotType::BOTTOM_LEFT;
            g_currentContext.currentParams[SeUiParam::PIVOT_POSITION_X].dim = bottomLeftX + buttonWidth / 2.0f;
            g_currentContext.currentParams[SeUiParam::PIVOT_POSITION_Y].dim = bottomLeftY + textOffsetY;
            text({ info.utf8text });
            g_currentContext.currentParams[SeUiParam::PIVOT_TYPE_X].pivot = cachedPivotTypeX;
            g_currentContext.currentParams[SeUiParam::PIVOT_TYPE_Y].pivot = cachedPivotTypeY;
            g_currentContext.currentParams[SeUiParam::PIVOT_POSITION_X].dim = cachedPivotX;
            g_currentContext.currentParams[SeUiParam::PIVOT_POSITION_Y].dim = cachedPivotY;
        }
        return
            (info.mode == SeUiButtonMode::HOLD && isPressed) ||
            (info.mode == SeUiButtonMode::TOGGLE && isToggled);
    }
}

SE_DLL_EXPORT void se_load(SabrinaEngine* engine)
{
    g_iface =
    {
        .begin_ui           = ui_impl::begin,
        .end_ui             = ui_impl::end,
        .set_font_group     = ui_impl::set_font_group,
        .set_param          = ui_impl::set_param,
        .text               = ui_impl::text,
        .begin_window       = ui_impl::begin_window,
        .end_window         = ui_impl::end_window,
        .button             = ui_impl::button,
    };
    se_init_global_subsystem_pointers(engine);
}

SE_DLL_EXPORT void se_init(SabrinaEngine* engine)
{
    g_drawUiVs = data_provider::from_file("assets/default/shaders/ui_draw.vert.spv");
    g_drawUiFs = data_provider::from_file("assets/default/shaders/ui_draw.frag.spv");
    hash_table::construct(g_fontInfos, app_allocators::persistent());
    hash_table::construct(g_fontGroups, app_allocators::persistent());

    g_currentContext = { };
    hash_table::construct(g_currentContext.uidToObjectData, app_allocators::persistent());
}

SE_DLL_EXPORT void* se_get_interface(SabrinaEngine* engine)
{
    return &g_iface;
}

SE_DLL_EXPORT void se_terminate(SabrinaEngine* engine)
{
    for (auto it : g_currentContext.uidToObjectData) string::destroy(iter::key(it));
    hash_table::destroy(g_currentContext.uidToObjectData);

    data_provider::destroy(g_drawUiVs);
    data_provider::destroy(g_drawUiFs);
    for (auto it : g_fontGroups) font_group::destroy(iter::value(it));
    for (auto it : g_fontInfos) font_info::destroy(iter::value(it));
    hash_table::destroy(g_fontGroups);
    hash_table::destroy(g_fontInfos);
}
