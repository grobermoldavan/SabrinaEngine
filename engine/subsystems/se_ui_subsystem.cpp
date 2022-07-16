
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

        const RenderAtlasLayoutBuilder::Node* push(RenderAtlasLayoutBuilder& builder, RenderAtlasRectSize requirement)
        {
            se_assert(requirement.width);
            se_assert(requirement.height);
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
                    (float)rect.p1x / (float)builder.width,
                    (float)rect.p2y / (float)builder.height,
                    (float)rect.p2x / (float)builder.width,
                    (float)rect.p1y / (float)builder.height,
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
        const size_t width = rect.p2x - rect.p1x;
        const size_t height = rect.p2y - rect.p1y;
        for (size_t h = 0; h < height; h++)
            for (size_t w = 0; w < width; w++)
            {
                const PixelSrc* const p = &source[h * width + w];
                PixelDst* const to = &atlas.bitmap[(rect.p1y + h) * atlas.width + rect.p1x + w];
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
    int                     ascendUnscaled;
    int                     descendUnscaled;
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
            .ascendUnscaled         = 0,
            .descendUnscaled        = 0,
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
        stbtt_GetFontVMetrics(&result.stbInfo, &result.ascendUnscaled, &result.descendUnscaled, &result.lineGapUnscaled);

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
    HashTable<uint32_t, CodepointInfo>  codepointToInfo;
};

namespace font_group
{
    int direct_cast_to_int(uint32_t value)
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
        };
        //
        // Load all fonts and map codepoints to fonts
        //
        HashTable<uint32_t, const FontInfo*> codepointToFont = hash_table::create<uint32_t, const FontInfo*>(app_allocators::frame(), 256);
        for (size_t it = 0; it < numFonts; it++)
        {
            const FontInfo* font = fonts[it];
            groupInfo.fonts[it] = font;
            for (auto codepointIt : font->supportedCodepoints)
            {
                const uint32_t codepoint = iter::value(codepointIt);
                if (!hash_table::get(codepointToFont, codepoint))
                {
                    hash_table::set(codepointToFont, codepoint, font);
                }
            }
        }
        const size_t numCodepoints = hash_table::size(codepointToFont);
        //
        // Get all codepoint bitmaps and fill codepoint infos
        //
        struct CodepointBitmapInfo
        {
            RenderAtlasRectSize size;
            uint8_t* memory;
        };
        HashTable<uint32_t, CodepointBitmapInfo> codepointToBitmap = hash_table::create<uint32_t, CodepointBitmapInfo>(app_allocators::frame(), numCodepoints);
        hash_table::construct(groupInfo.codepointToInfo, app_allocators::persistent(), numCodepoints);
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
            hash_table::set(codepointToBitmap, codepoint, { { (size_t)width, (size_t)height }, (uint8_t*)bitmap });
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
            hash_table::set(groupInfo.codepointToInfo, codepoint, codepointInfo);
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
            const CodepointBitmapInfo bitmap = *hash_table::get(codepointToBitmap, codepoint);
            if (bitmap.memory)
            {
                const RenderAtlasPutFunciton<uint8_t, uint8_t> put = [](const uint8_t* from, uint8_t* to) { *to = *from; };
                render_atlas::blit(groupInfo.atlas, rect, bitmap.memory, put);
            }
            FontGroup::CodepointInfo* info = hash_table::get(groupInfo.codepointToInfo, codepoint);
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

        hash_table::destroy(codepointToFont);
        hash_table::destroy(codepointToBitmap);
        hash_table::destroy(atlasRectToCodepoint);

        return groupInfo;
    }

    void destroy(FontGroup& group)
    {
        render_atlas::destroy(group.atlas);
        hash_table::destroy(group.codepointToInfo);
    }
}

// =======================================================================
//
// Ui context
//
// =======================================================================

struct UiRenderVertex
{
    float x;
    float y;
    float uvX;
    float uvY;
    uint32_t colorIndex;
    float _pad[3];
};

/*
    Ui render colors is a special struct that describes coloring for ui elements.
    Coloring works the following way:
    1. Sample color fron texture in the fragment shader
    2. For each r, g, b and a component:
        1. Get component value from mask (mask.r for r component) (float)
        2. Get component value from sampled color (float)
        3. Get corresponding *Compoenent (where * is r, g, b or a) value (vec4)
        4. Multiply mask component value, sampled component value and *Component value
    3. Sum resulting vec4 values for each component
    4. Divide resulting sum with clamp(sum of all components from texture sample, minDivider, maxDivider)

    This colloring gives ability to work with any number of components in sampled texture.
    If we work with a single-component texture (SE_TEXTURE_FORMAT_R_8 for font rendering),
    we use only rComponent value with mask == { 1, 0, 0, 0 }, minDivider == 1.0f and maxDivider == 1.0f
*/
struct UiRenderColorsUnpacked
{
    ColorUnpacked   rComponent;
    ColorUnpacked   gComponent;
    ColorUnpacked   bComponent;
    SeFloat4        mask;
    float           minDivider;
    float           maxDivider;
    float           _pad[2];
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

using UiStyle = SeUiStyleParam[SeUiStyleParam::_COUNT];

struct UiObjectData
{
    float bottomLeftX;
    float bottomLeftY;
    float topRightX;
    float topRightY;
};

struct UiObjectUid
{
    char data[64];
};

struct UiContext
{
    // Frame state
    const SeRenderAbstractionSubsystemInterface*    render;
    SeDeviceHandle                                  device;
    SePassRenderTarget                              target;
    SeWindowHandle                                  window;

    const FontGroup*                                currentFontGroup;
    DataProvider                                    currentWindowData;
    UiStyle                                         currentStyle;
    
    DynamicArray<UiDrawCall>                        frameDrawCalls;
    HashTable<UiTextureData, SeRenderRef>           frameTextureDataToRef;
    float                                           mouseX;
    float                                           mouseY;
    float                                           mouseDeltaX;
    float                                           mouseDeltaY;
    bool                                            isMouseDown;
    bool                                            isMouseJustDown;

    // Persistent state
    HashTable<UiObjectUid, UiObjectData>            uidToObjectData;
    UiObjectUid                                     hoveredObjectUid;
    UiObjectUid                                     activeObjectUid;
};

const UiStyle DEFAULT_STYLE =
{
    { .color = col::pack({ 1.0f, 1.0f, 1.0f, 1.0f }) },
    { .color = col::pack({ 0.87f, 0.81f, 0.83f, 1.0f }) },
    { .color = col::pack({ 0.36f, 0.45f, 0.58f, 0.8f }) },
    { .color = col::pack({ 0.95f, 0.75f, 0.79f, 1.0f }) },
    { .dim = ui_dim::pix(16.0f) },
    { .dim = ui_dim::pix(2.0f) },
};

SeUiSubsystemInterface                  g_iface;
HashTable<SeUiBeginInfo, UiContext>     g_contextTable;
UiContext*                              g_currentContext = nullptr;

HashTable<DataProvider, FontInfo>       g_fontInfos;
HashTable<SeUiFontGroupInfo, FontGroup> g_fontGroups;

DataProvider                            g_drawUiVs;
DataProvider                            g_drawUiFs;
DataProvider                            g_windowDefaultTexture;

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
    bool compare<UiRenderColorsUnpacked>(const UiRenderColorsUnpacked& first, const UiRenderColorsUnpacked& second)
    {
        return
            compare(first.rComponent, second.rComponent)  &&
            compare(first.gComponent, second.gComponent)  &&
            compare(first.bComponent, second.bComponent)  &&
            compare(first.mask, second.mask)              &&
            (first.minDivider   == second.minDivider)     &&
            (first.maxDivider   == second.maxDivider);
    }

    template<>
    bool compare<UiTextureData>(const UiTextureData& first, const UiTextureData& second)
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
    inline float dim_to_pix(SeUiDim dim)
    {
        switch (dim.type)
        {
            case SeUiDim::PIXELS:           { return dim.dim; }
            case SeUiDim::TARGET_RELATIVE:  { se_assert(false); }
            default:                        { se_assert(false); }
        }
        return 0;
    }

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

    inline UiObjectUid object_uid_create(const char* str)
    {
        se_assert(str);
        const size_t len = strlen(str);
        se_assert(len && len < sizeof(UiObjectUid::data));
        UiObjectUid uid = { };
        memcpy(uid.data, str, len);
        return uid;
    }

    inline bool object_data_get(UiObjectData** outData, UiObjectUid* outUid, const char* cstrUid)
    {
        const UiObjectUid uid = object_uid_create(cstrUid);
        UiObjectData* data = hash_table::get(g_currentContext->uidToObjectData, uid);
        const bool isFirstAccess = data == nullptr;
        if (!data)
        {
            data = hash_table::set(g_currentContext->uidToObjectData, uid, { });
        }
        se_assert(data);
        *outData = data;
        *outUid = uid;
        return isFirstAccess;
    }

    UiDrawCall* get_draw_call(const UiTextureData& textureData, const SeTextureInfo& textureInfo, const UiRenderColorsUnpacked& colors)
    {
        //
        // Get texture
        //
        SeRenderRef* textureRef = hash_table::get(g_currentContext->frameTextureDataToRef, textureData);
        if (!textureRef)
        {
            const SeDeviceHandle device = g_currentContext->device;
            const SeRenderAbstractionSubsystemInterface* render = g_currentContext->render;
            textureRef = hash_table::set(g_currentContext->frameTextureDataToRef, textureData, render->texture(device, textureInfo));
        }
        se_assert(textureRef);
        //
        // Get draw call
        //
        const size_t numDrawCalls = dynamic_array::size(g_currentContext->frameDrawCalls);
        const UiDrawCall* const lastDrawCall = numDrawCalls ? &g_currentContext->frameDrawCalls[numDrawCalls - 1] : nullptr;
        if (!lastDrawCall || lastDrawCall->texture != *textureRef)
        {
            UiDrawCall& pushedDrawCall = dynamic_array::push(g_currentContext->frameDrawCalls,
            {
                .vertices       = dynamic_array::create<UiRenderVertex>(app_allocators::frame(), 256),
                .colorsArray    = dynamic_array::create<UiRenderColorsUnpacked>(app_allocators::frame(), 256),
                .texture        = *textureRef,
            });
            dynamic_array::push(pushedDrawCall.colorsArray, colors);
        }
        UiDrawCall* const drawCall = &g_currentContext->frameDrawCalls[dynamic_array::size(g_currentContext->frameDrawCalls) - 1];
        se_assert(drawCall->texture == *textureRef);
        if (!utils::compare(drawCall->colorsArray[dynamic_array::size(drawCall->colorsArray) - 1], colors))
        {
            dynamic_array::push(drawCall->colorsArray, colors);
        }
        return drawCall;
    }

    bool begin(const SeUiBeginInfo& info)
    {
        se_assert(!g_currentContext);
        se_assert(info.render);
        se_assert(info.device);
        //
        // Get context
        //
        g_currentContext = hash_table::get(g_contextTable, info);
        if (!g_currentContext)
        {
            g_currentContext = hash_table::set(g_contextTable, info, { });
            hash_table::construct(g_currentContext->uidToObjectData, app_allocators::persistent());
        }
        //
        // Set frame data
        //
        g_currentContext->render            = info.render;
        g_currentContext->device            = info.device;
        g_currentContext->target            = info.target;
        g_currentContext->window            = info.window;
        g_currentContext->currentFontGroup  = nullptr;
        g_currentContext->currentWindowData = g_windowDefaultTexture;
        memcpy(g_currentContext->currentStyle, DEFAULT_STYLE, sizeof(UiStyle));
        dynamic_array::construct(g_currentContext->frameDrawCalls, app_allocators::frame(), 16);
        hash_table::construct(g_currentContext->frameTextureDataToRef, app_allocators::frame(), 16);
        if (g_currentContext->window != NULL_WINDOW_HANDLE)
        {
            const SeWindowSubsystemInput* input = win::get_input(g_currentContext->window);
            const float mouseX = (float)input->mouseX;
            const float mouseY = (float)input->mouseY;
            g_currentContext->mouseDeltaX       = mouseX - g_currentContext->mouseX;
            g_currentContext->mouseDeltaY       = mouseY - g_currentContext->mouseY;
            g_currentContext->mouseX            = mouseX;
            g_currentContext->mouseY            = mouseY;
            g_currentContext->isMouseDown       = win::is_mouse_button_pressed(input, SE_LMB);
            g_currentContext->isMouseJustDown   = win::is_mouse_button_just_pressed(input, SE_LMB);
        }
        //
        // Update active object
        //
        if (!g_currentContext->isMouseDown)
        {
            g_currentContext->activeObjectUid = { };
        }
        else
        {
            UiObjectData* const activeObject = hash_table::get(g_currentContext->uidToObjectData, g_currentContext->activeObjectUid);
            if (activeObject)
            {
                activeObject->bottomLeftX = activeObject->bottomLeftX + g_currentContext->mouseDeltaX;
                activeObject->bottomLeftY = activeObject->bottomLeftY + g_currentContext->mouseDeltaY;
                activeObject->topRightX = activeObject->topRightX + g_currentContext->mouseDeltaX;
                activeObject->topRightY = activeObject->topRightY + g_currentContext->mouseDeltaY;
            }
        }
        return g_currentContext != nullptr; // Must be always true
    }

    void end()
    {
        se_assert(g_currentContext);
        const SeDeviceHandle device = g_currentContext->device;
        const SeRenderAbstractionSubsystemInterface* const render = g_currentContext->render;
        const SeRenderRef vertexProgram = render->program(device, { g_drawUiVs });
        const SeRenderRef fragmentProgram = render->program(device, { g_drawUiFs });
        const SeRenderRef sampler = render->sampler(device,
        {
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
        const SeRenderRef pipeline = render->graphics_pipeline(device,
        {
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
        render->begin_pass(device,
        {
            .id                 = 0,
            .dependencies       = 0,
            .pipeline           = pipeline,
            .renderTargets      = { g_currentContext->target },
            .numRenderTargets   = 1,
            .depthStencilTarget = 0,
            .hasDepthStencil    = false,
        });
        //
        // Bind projection matrix
        //
        const SeTextureSize targetSize = render->texture_size(device, g_currentContext->target.texture);
        se_assert(targetSize.z == 1);
        const SeRenderRef projectionBuffer = render->memory_buffer(device,
        {
            data_provider::from_memory
            (
                float4x4::transposed(render->orthographic(0, (float)targetSize.x, 0, (float)targetSize.y, 0, 2))
            )
        });
        render->bind(device, { .set = 0, .bindings = { { 0, projectionBuffer } } });
        //
        // Process draw calls
        //
        for (auto it : g_currentContext->frameDrawCalls)
        {
            const UiDrawCall& drawCall = iter::value(it);
            if (!dynamic_array::size(drawCall.vertices))
            {
                continue;
            }
            // Alloc buffers
            const SeRenderRef colorsBuffer = render->memory_buffer(device,
            {
                data_provider::from_memory(dynamic_array::raw(drawCall.colorsArray), dynamic_array::raw_size(drawCall.colorsArray))
            });
            const SeRenderRef verticesBuffer = render->memory_buffer(device,
            {
                data_provider::from_memory(dynamic_array::raw(drawCall.vertices), dynamic_array::raw_size(drawCall.vertices))
            });
            // Bind and draw
            render->bind(device,
            {
                .set = 1,
                .bindings =
                {
                    { 0, drawCall.texture, sampler },
                    { 1, colorsBuffer },
                    { 2, verticesBuffer },
                },
            });
            render->draw(device, { dynamic_array::size<uint32_t>(drawCall.vertices), 1 });
        }
        render->end_pass(device);
        //
        // Clear context
        //
        for (auto it : g_currentContext->frameDrawCalls)
        {
            dynamic_array::destroy(iter::value(it).vertices);
        }
        dynamic_array::destroy(g_currentContext->frameDrawCalls);
        hash_table::destroy(g_currentContext->frameTextureDataToRef);
        g_currentContext = nullptr;
    }

    void set_font_group(const SeUiFontGroupInfo& info)
    {
        se_assert(g_currentContext);
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
        g_currentContext->currentFontGroup = fontGroup;
    }

    void set_style_param(SeUiStyleParam::Type type, const SeUiStyleParam& param)
    {
        se_assert(g_currentContext);
        g_currentContext->currentStyle[type] = param;
    }

    void text_line(const SeUiTextLineInfo& info)
    {
        se_assert(info.utf8text);
        se_assert(g_currentContext);
        se_assert(g_currentContext->currentFontGroup);
        //
        // Get base data and draw call
        //
        const FontGroup* const fontGroup = g_currentContext->currentFontGroup;
        const float textHeight = dim_to_pix(info.height);
        const float baselineX = dim_to_pix(info.baselineX);
        const float baselineY = dim_to_pix(info.baselineY);
        UiDrawCall* const drawCall = get_draw_call(ui_texture_data_create(fontGroup), fontGroup->textureInfo,
        {
            .rComponent = col::unpack(g_currentContext->currentStyle[SeUiStyleParam::FONT_COLOR].color),
            .gComponent = { 0.0f, 0.0f, 0.0f, 0.0f },
            .bComponent = { 0.0f, 0.0f, 0.0f, 0.0f },
            .mask       = { 1.0f, 0.0f, 0.0f, 0.0f },
            .minDivider = 1.0f,
            .maxDivider = 1.0f,
        });
        const uint32_t colorIndex = dynamic_array::size<uint32_t>(drawCall->colorsArray) - 1;
        //
        // Process codepoints
        //
        float positionX = baselineX;
        int previousCodepoint = 0;
        for (uint32_t codepoint : Utf8{ info.utf8text })
        {
            const FontGroup::CodepointInfo* const codepointInfo = hash_table::get(fontGroup->codepointToInfo, codepoint);
            se_assert(codepointInfo);
            const FontInfo* const font = codepointInfo->font;
            // This scale calculation is similar to stbtt_ScaleForPixelHeight but ignores descend parameter of the font
            const float scale = textHeight / (float)font->ascendUnscaled;
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
            const float additionalAdvance = scale * (float)stbtt_GetCodepointKernAdvance(&font->stbInfo, previousCodepoint, codepointSigned);
            positionX += additionalAdvance;
            const RenderAtlasRectNormalized& rect = fontGroup->atlas.uvRects[codepointInfo->atlasRectIndex];
            const float baseX = positionX + (float)x0 - bearing;
            const float baseY = baselineY - (float)y1;
            dynamic_array::push(drawCall->vertices, { baseX        , baseY         , rect.p1x, rect.p1y, colorIndex, });
            dynamic_array::push(drawCall->vertices, { baseX        , baseY + height, rect.p1x, rect.p2y, colorIndex, });
            dynamic_array::push(drawCall->vertices, { baseX + width, baseY + height, rect.p2x, rect.p2y, colorIndex, });
            dynamic_array::push(drawCall->vertices, { baseX        , baseY         , rect.p1x, rect.p1y, colorIndex, });
            dynamic_array::push(drawCall->vertices, { baseX + width, baseY + height, rect.p2x, rect.p2y, colorIndex, });
            dynamic_array::push(drawCall->vertices, { baseX + width, baseY         , rect.p2x, rect.p1y, colorIndex, });
            positionX += advance;
            previousCodepoint = codepointSigned;
        }
    }

    bool begin_window(const SeUiWindowInfo& info)
    {
        se_assert(g_currentContext);
        //
        // Get base data and update object state (hovered/active)
        //
        UiObjectData* data = nullptr;
        UiObjectUid uid;
        const bool isFirst = object_data_get(&data, &uid, info.uid);
        const float bottomLeftX         = isFirst ? dim_to_pix(info.bottomLeftX) : data->bottomLeftX;
        const float bottomLeftY         = isFirst ? dim_to_pix(info.bottomLeftY) : data->bottomLeftY;
        const float topRightX           = isFirst ? dim_to_pix(info.topRightX) : data->topRightX;
        const float topRightY           = isFirst ? dim_to_pix(info.topRightY) : data->topRightY;
        const float topPanelThickness   = dim_to_pix(g_currentContext->currentStyle[SeUiStyleParam::WINDOW_TOP_PANEL_THICKNESS].dim);
        const float borderThickness     = dim_to_pix(g_currentContext->currentStyle[SeUiStyleParam::WINDOW_BORDER_THICKNESS].dim);
        if (isFirst)
        {
            data->bottomLeftX = bottomLeftX;
            data->bottomLeftY = bottomLeftY;
            data->topRightX = topRightX;
            data->topRightY = topRightY;
        }
        const bool isUnderCursor =
            (bottomLeftX <= g_currentContext->mouseX) &&
            (bottomLeftY <= g_currentContext->mouseY) &&
            (topRightX   >= g_currentContext->mouseX) &&
            (topRightY   >= g_currentContext->mouseY);
        if (isUnderCursor)
        {
            g_currentContext->hoveredObjectUid = uid;
            if (g_currentContext->isMouseJustDown)
            {
                g_currentContext->activeObjectUid = uid;
            }
        }
        //
        // Get draw call
        //
        UiDrawCall* const drawCall = get_draw_call(ui_texture_data_create(g_currentContext->currentWindowData),
        {
            .format = SE_TEXTURE_FORMAT_RGBA_8,
            .data   = g_currentContext->currentWindowData,
        },
        {
            .rComponent = col::unpack(g_currentContext->currentStyle[SeUiStyleParam::PRIMARY_COLOR].color),
            .gComponent = col::unpack(g_currentContext->currentStyle[SeUiStyleParam::SECONDARY_COLOR].color),
            .bComponent = col::unpack(g_currentContext->currentStyle[SeUiStyleParam::ACCENT_COLOR].color),
            .mask       = { 1.0f, 1.0f, 1.0f, 0.0f },
            .minDivider = 1.0f,
            .maxDivider = 3.0f,
        });
        const uint32_t colorIndex = dynamic_array::size<uint32_t>(drawCall->colorsArray) - 1;
        //
        // Push window data
        //
        auto pushQuad = [&](float x1, float y1, float u1, float v1, float x2, float y2, float u2, float v2)
        {
            dynamic_array::push(drawCall->vertices, { x1, y1, u1, v1, colorIndex });
            dynamic_array::push(drawCall->vertices, { x1, y2, u1, v2, colorIndex });
            dynamic_array::push(drawCall->vertices, { x2, y2, u2, v2, colorIndex });
            dynamic_array::push(drawCall->vertices, { x1, y1, u1, v1, colorIndex });
            dynamic_array::push(drawCall->vertices, { x2, y2, u2, v2, colorIndex });
            dynamic_array::push(drawCall->vertices, { x2, y1, u2, v1, colorIndex });
        };
        constexpr float oneThird = 1.0f / 3.0f;
        constexpr float oneFourth = 1.0f / 4.0f;
        //
        // Top row
        //
        pushQuad
        (
            bottomLeftX, topRightY - topPanelThickness, 0.0f, oneThird,
            bottomLeftX + topPanelThickness, topRightY, oneFourth, 0.0f
        );
        pushQuad
        (
            bottomLeftX + topPanelThickness, topRightY - topPanelThickness, oneFourth, oneThird,
            topRightX - topPanelThickness, topRightY, oneFourth * 2.0f, 0.0f
        );
        pushQuad
        (
            topRightX - topPanelThickness, topRightY - topPanelThickness, oneFourth * 2.0f, oneThird,
            topRightX, topRightY, oneFourth * 3.0f, 0.0f
        );
        //
        // Middle row
        //
        pushQuad
        (
            bottomLeftX, bottomLeftY + borderThickness, 0.0f, oneThird * 2.0f,
            bottomLeftX + borderThickness, topRightY - topPanelThickness, oneFourth, oneThird
        );
        pushQuad
        (
            bottomLeftX + borderThickness, bottomLeftY + borderThickness, oneFourth, oneThird * 2.0f,
            topRightX - borderThickness, topRightY - topPanelThickness, oneFourth * 2.0f, oneThird
        );
        pushQuad
        (
            topRightX - borderThickness, bottomLeftY + borderThickness, oneFourth * 2.0f, oneThird * 2.0f,
            topRightX, topRightY - topPanelThickness, oneFourth * 3.0f, oneThird
        );
        //
        // Bottom row
        //
        pushQuad
        (
            bottomLeftX, bottomLeftY, 0.0f, 1.0f,
            bottomLeftX + borderThickness, bottomLeftY + borderThickness, oneFourth, oneThird * 2.0f
        );
        pushQuad
        (
            bottomLeftX + borderThickness, bottomLeftY, oneFourth, 1.0f,
            topRightX - borderThickness, bottomLeftY + borderThickness, oneFourth * 2.0f, oneThird * 2.0f
        );
        pushQuad
        (
            topRightX - borderThickness, bottomLeftY, oneFourth * 2.0f, 1.0f,
            topRightX, bottomLeftY + borderThickness, oneFourth * 3.0f, oneThird * 2.0f
        );
        //
        // Button!
        //
        pushQuad
        (
            topRightX - topPanelThickness - topPanelThickness, topRightY - topPanelThickness, oneFourth * 3.0f, oneThird,
            topRightX - topPanelThickness, topRightY, 1.0f, 0.0f
        );
        return true;
    }

    void end_window()
    {

    }
}

SE_DLL_EXPORT void se_load(SabrinaEngine* engine)
{
    g_iface =
    {
        .begin_ui           = ui_impl::begin,
        .end_ui             = ui_impl::end,
        .set_font_group     = ui_impl::set_font_group,
        .set_style_param    = ui_impl::set_style_param,
        .text_line          = ui_impl::text_line,
        .begin_window       = ui_impl::begin_window,
        .end_window         = ui_impl::end_window,
    };
    se_init_global_subsystem_pointers(engine);
}

SE_DLL_EXPORT void se_init(SabrinaEngine* engine)
{
    g_drawUiVs = data_provider::from_file("assets/default/shaders/ui_draw.vert.spv");
    g_drawUiFs = data_provider::from_file("assets/default/shaders/ui_draw.frag.spv");
    g_windowDefaultTexture = data_provider::from_file("assets/default/textures/ui_window_default.png");
    hash_table::construct(g_contextTable, app_allocators::persistent());
    hash_table::construct(g_fontInfos, app_allocators::persistent());
    hash_table::construct(g_fontGroups, app_allocators::persistent());
}

SE_DLL_EXPORT void* se_get_interface(SabrinaEngine* engine)
{
    return &g_iface;
}

SE_DLL_EXPORT void se_terminate(SabrinaEngine* engine)
{
    data_provider::destroy(g_drawUiVs);
    data_provider::destroy(g_drawUiFs);
    data_provider::destroy(g_windowDefaultTexture);
    hash_table::destroy(g_contextTable);
    for (auto it : g_fontGroups) font_group::destroy(iter::value(it));
    for (auto it : g_fontInfos) font_info::destroy(iter::value(it));
    hash_table::destroy(g_fontGroups);
    hash_table::destroy(g_fontInfos);
}
