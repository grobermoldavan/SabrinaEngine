#ifndef _SE_UI_SUBSYSTEM_HPP_
#define _SE_UI_SUBSYSTEM_HPP_

#include "engine/render_abstraction_interface.hpp"
#include "engine/common_includes.hpp"
#include "engine/data_providers.hpp"
#include "engine/hash.hpp"

struct SeUiDim
{
    enum Type
    {
        PIXELS,
        TARGET_RELATIVE,
    };
    Type type;
    float dim;
};

namespace ui_dim
{
    inline constexpr SeUiDim pix(float dim)
    {
        return { SeUiDim::PIXELS, dim };
    }

    inline constexpr SeUiDim rel(float dim)
    {
        return { SeUiDim::TARGET_RELATIVE, dim };
    }
}

struct SeUiFontGroupInfo
{
    static constexpr size_t MAX_FONTS = 8;
    DataProvider fonts[MAX_FONTS];
};

struct SeUiBeginInfo
{
    const SeRenderAbstractionSubsystemInterface*    render;
    SePassRenderTarget                              target;
};

struct SeUiTextLineInfo
{
    const char* utf8text;
    SeUiDim     baselineX;
    SeUiDim     baselineY;
};

struct SeUiStyleParam
{
    enum Type
    {
        FONT_COLOR                  = 0,
        FONT_HEIGHT                 = 1,
        FONT_LINE_STEP              = 2,
        PRIMARY_COLOR               = 3,
        SECONDARY_COLOR             = 4,
        ACCENT_COLOR                = 5,
        WINDOW_TOP_PANEL_THICKNESS  = 6,
        WINDOW_BORDER_THICKNESS     = 7,
        _COUNT,
    };
    union
    {
        ColorPacked color;
        SeUiDim dim;
    };
};

struct SeUiFlags
{
    enum
    {
        MOVABLE     = 0x00000001,
        RESIZABLE_X = 0x00000002,
        RESIZABLE_Y = 0x00000004,
        HIDEABLE    = 0x00000008,
    };
};

struct SeUiWindowInfo
{
    const char* uid;
    SeUiDim     bottomLeftX;
    SeUiDim     bottomLeftY;
    SeUiDim     topRightX;
    SeUiDim     topRightY;
    uint32_t    flags;
};

struct SeUiWindowTextInfo
{
    const char* utf8text;
};

struct SeUiSubsystemInterface
{
    static constexpr const char* NAME = "SeUiSubsystemInterface";

    bool (*begin_ui)(const SeUiBeginInfo& info);
    SePassDependencies (*end_ui)(SePassDependencies dependencies);

    void (*set_font_group)(const SeUiFontGroupInfo& info);
    void (*set_style_param)(SeUiStyleParam::Type type, const SeUiStyleParam& param);

    void (*text_line)(const SeUiTextLineInfo& info);

    bool (*begin_window)(const SeUiWindowInfo& info);
    void (*end_window)();
    void (*window_text)(const SeUiWindowTextInfo& info);
};

struct SeUiSubsystem
{
    using Interface = SeUiSubsystemInterface;
    static constexpr const char* NAME = "se_ui_subsystem";
};

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

#endif
