
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

    template<typename Pixel>
    void blit(RenderAtlas<Pixel>& atlas, const RenderAtlasRect& rect, const Pixel* source)
    {
        const size_t width = rect.p2x - rect.p1x;
        const size_t height = rect.p2y - rect.p1y;
        for (size_t h = 0; h < height; h++)
            for (size_t w = 0; w < width; w++)
            {
                const Pixel p = source[h * width + w];
                Pixel* const to = &atlas.bitmap[(rect.p1y + h) * atlas.width + rect.p1x + w];
                *to = p;
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

        se_assert(data_provider::is_valid(data));
        auto [dataMemory, dataSize] = data_provider::get(data);
        se_assert(dataMemory);
        const unsigned char* charData = (const unsigned char*)dataMemory;
        FontInfo result = { };
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

    static constexpr float ATLAS_CODEPOINT_HEIGHT_PIX = 128;

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
            const RenderAtlasRect rect = iter::value(it);
            const uint32_t codepoint = *hash_table::get(atlasRectToCodepoint, rect);
            const CodepointBitmapInfo bitmap = *hash_table::get(codepointToBitmap, codepoint);
            if (bitmap.memory)
            {
                render_atlas::blit(groupInfo.atlas, iter::value(it), bitmap.memory);
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
            .data   = data_provider::from_memory(groupInfo.atlas.bitmap, groupInfo.atlas.width * groupInfo.atlas.height),
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

struct UiCodepointRenderInfo
{
    uint32_t    atlasRectIndex;
    float       x; // bottom left
    float       y; // bottom left
    float       width;
    float       height;
    float       pad[3];
};

struct UiContext
{
    const SeRenderAbstractionSubsystemInterface*    render;
    SeDeviceHandle                                  device;
    HashTable<DataProvider, FontInfo>               fontDataToInfo;
    HashTable<SeUiFontGroupInfo, FontGroup>         groupDataToInfo;
    const FontGroup*                                activeFountGroup;
    SeRenderRef                                     activeRenderTarget;
};

SeUiSubsystemInterface                  g_iface;
HashTable<SeWindowHandle, UiContext>    g_contextTable;
UiContext*                              g_currentContext = nullptr;
DataProvider                            g_drawTextVs;
DataProvider                            g_drawTextFs;

namespace ui_impl
{
    float dim_to_pix(SeUiDim dim)
    {
        switch (dim.type)
        {
            case SeUiDim::PIXELS:           { return dim.dim; }
            case SeUiDim::CENTIMETERS:      { se_assert(false); }
            case SeUiDim::TARGET_RELATIVE:  { se_assert(false); }
            default:                        { se_assert(false); }
        }
        return 0;
    }

    void begin(const SeUiBeginInfo& info)
    {
        se_assert(!g_currentContext);
        se_assert(info.render);
        se_assert(info.device);

        g_currentContext = hash_table::get(g_contextTable, info.window);
        if (!g_currentContext)
        {
            g_currentContext = hash_table::set(g_contextTable, info.window,
            {
                .render             = info.render,
                .device             = info.device,
                .fontDataToInfo     = hash_table::create<DataProvider, FontInfo>(app_allocators::persistent()),
                .groupDataToInfo    = hash_table::create<SeUiFontGroupInfo, FontGroup>(app_allocators::persistent()),
                .activeFountGroup   = nullptr,
            });
        }
    }

    void end()
    {
        se_assert(g_currentContext);

        g_currentContext->render->end_pass(g_currentContext->device);
        g_currentContext = nullptr;
    }

    void set_render_target(SeRenderRef target)
    {
        se_assert(g_currentContext);

        const SeDeviceHandle device = g_currentContext->device;
        const SeRenderAbstractionSubsystemInterface* const render = g_currentContext->render;

        const SeRenderRef vertexProgram = render->program(device, { g_drawTextVs });
        const SeRenderRef fragmentProgram = render->program(device, { g_drawTextFs });
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
            .renderTargets      = { target, SE_PASS_RENDER_TARGET_LOAD_OP_CLEAR }, // @TODO : remove clear from here
            .numRenderTargets   = 1,
            .depthStencilTarget = 0,
            .hasDepthStencil    = false,
        });
        //
        // Bind camera projection matrix
        //
        const SeTextureSize targetSize = render->texture_size(device, target);
        se_assert(targetSize.z == 1);
        const SeRenderRef projectionBuffer = render->memory_buffer(device,
        {
            data_provider::from_memory(float4x4::transposed(render->orthographic(0, (float)targetSize.x, 0, (float)targetSize.y, 0, 100)))
        });
        render->bind(device,
        {
            .set = 0,
            .bindings =
            {
                { 0, projectionBuffer }
            }
        });
    }

    void set_font_group(const SeUiFontGroupInfo& info)
    {
        se_assert(g_currentContext);
        static_assert(sizeof(int) == sizeof(uint32_t)); // We directly casting uint32_t to int

        const FontGroup* groupInfo = hash_table::get(g_currentContext->groupDataToInfo, info);
        if (!groupInfo)
        {
            const FontInfo* fonts[SeUiFontGroupInfo::MAX_FONTS];
            size_t fontIt;
            for (fontIt = 0; fontIt < SeUiFontGroupInfo::MAX_FONTS; fontIt++)
            {
                const DataProvider& data = info.fonts[fontIt];
                if (!data_provider::is_valid(data)) break;        
                const FontInfo* fontInfo = hash_table::get(g_currentContext->fontDataToInfo, data);
                if (!fontInfo)
                {
                    fontInfo = hash_table::set(g_currentContext->fontDataToInfo, data, font_info::create(data));
                }
                fonts[fontIt] = fontInfo;
            }
            groupInfo = hash_table::set(g_currentContext->groupDataToInfo, info, font_group::create(fonts, fontIt));
        }
        g_currentContext->activeFountGroup = groupInfo;
        //
        // Bind font atlas
        //
        const SeDeviceHandle device = g_currentContext->device;
        const SeRenderAbstractionSubsystemInterface* const render = g_currentContext->render;
        const SeRenderRef atlasTexture = render->texture(device, groupInfo->textureInfo);
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
        const SeRenderRef atlasRectsBuffer = render->memory_buffer(device,
        {
            data_provider::from_memory
            (
                dynamic_array::raw(groupInfo->atlas.uvRects),
                dynamic_array::raw_size(groupInfo->atlas.uvRects)
            )
        });
        render->bind(device,
        {
            .set = 1,
            .bindings =
            {
                { 0, atlasTexture, sampler },
                { 1, atlasRectsBuffer }
            }
        });
    }

    void text_line(const SeUiTextLineInfo& info)
    {
        se_assert(info.utf8text);
        se_assert(g_currentContext);
        se_assert(g_currentContext->render);
        se_assert(g_currentContext->activeFountGroup);
        const SeDeviceHandle device = g_currentContext->device;
        const SeRenderAbstractionSubsystemInterface* const render = g_currentContext->render;
        const FontGroup* const fontGroup = g_currentContext->activeFountGroup;
        const float baselineX = dim_to_pix(info.baselineX);
        const float baselineY = dim_to_pix(info.baselineY);
        const float height = dim_to_pix(info.height);
        //
        // Prepare buffers
        //
        // @TODO : use some kind of dynamic array instead of the preallocated one
        UiCodepointRenderInfo codepointInfos[64];
        uint32_t numCodepoints = 0;
        int previousCodepoint = 0;
        float positionX = baselineX;
        for (uint32_t codepoint : Utf8{ info.utf8text })
        {
            const FontGroup::CodepointInfo* const codepointInfo = hash_table::get(fontGroup->codepointToInfo, codepoint);
            const FontInfo* const font = codepointInfo->font;
            // This scale calculation is similar to stbtt_ScaleForPixelHeight but ignores descend parameter of the font
            const float scale = height / (float)font->ascendUnscaled;
            const float advance = scale * (float)codepointInfo->advanceWidthUnscaled;
            const float bearing = scale * (float)codepointInfo->leftSideBearingUnscaled;
            const int codepointSigned = font_group::direct_cast_to_int(codepoint);
            int x0;
            int y0;
            int x1;
            int y1;
            stbtt_GetCodepointBitmapBox(&font->stbInfo, codepointSigned, scale, scale, &x0, &y0, &x1, &y1);
            const float codepointWidth = (float)(x1 - x0);
            const float codepointHeight = (float)(y1 - y0);
            if (numCodepoints)
            {
                const float additionalAdvance = scale * (float)stbtt_GetCodepointKernAdvance(&font->stbInfo, previousCodepoint, codepointSigned);
                positionX += additionalAdvance;
            }
            codepointInfos[numCodepoints] =
            {
                codepointInfo ? codepointInfo->atlasRectIndex : 0,
                positionX + (float)x0 - bearing,
                baselineY - (float)y1,
                codepointWidth,
                codepointHeight,
            };
            positionX += advance;
            numCodepoints += 1;
            previousCodepoint = codepointSigned;
        }
        const SeRenderRef indicesBuffer = render->memory_buffer(device,
        {
            data_provider::from_memory(codepointInfos, numCodepoints * sizeof(codepointInfos[0]))
        });
        //
        // Bind stuff and draw
        //
        render->bind(device,
        {
            .set = 2,
            .bindings =
            {
                { 0, indicesBuffer }
            }
        });
        render->draw(device, { 6, numCodepoints });
    }
}

SE_DLL_EXPORT void se_load(SabrinaEngine* engine)
{
    g_iface =
    {
        .begin_ui           = ui_impl::begin,
        .end_ui             = ui_impl::end,
        .set_render_target  = ui_impl::set_render_target,
        .set_font_group     = ui_impl::set_font_group,
        .text_line          = ui_impl::text_line,
    };
    se_init_global_subsystem_pointers(engine);
}

SE_DLL_EXPORT void se_init(SabrinaEngine* engine)
{
    hash_table::construct(g_contextTable, app_allocators::persistent());
    g_drawTextVs = data_provider::from_file("assets/default/shaders/draw_text.vert.spv");
    g_drawTextFs = data_provider::from_file("assets/default/shaders/draw_text.frag.spv");
}

SE_DLL_EXPORT void* se_get_interface(SabrinaEngine* engine)
{
    return &g_iface;
}

SE_DLL_EXPORT void se_terminate(SabrinaEngine* engine)
{
    data_provider::destroy(g_drawTextVs);
    data_provider::destroy(g_drawTextFs);
    hash_table::destroy(g_contextTable);
}
