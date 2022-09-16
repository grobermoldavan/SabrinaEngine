#ifndef _SE_UI_SUBSYSTEM_HPP_
#define _SE_UI_SUBSYSTEM_HPP_

#include "engine/render/se_render.hpp"
#include "engine/se_common_includes.hpp"
#include "engine/se_data_providers.hpp"
#include "engine/se_hash.hpp"

struct SeUiBeginInfo
{
    SePassRenderTarget target;
};

struct SeUiFontGroupInfo
{
    static constexpr size_t MAX_FONTS = 8;
    DataProvider fonts[MAX_FONTS];
};

enum struct SeUiPivotType : uint32_t
{
    BOTTOM_LEFT,
    CENTER,
};

struct SeUiParam
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
        WINDOW_INNER_PADDING        = 8,
        PIVOT_POSITION_X            = 9,
        PIVOT_POSITION_Y            = 10,
        PIVOT_TYPE_X                = 11,
        PIVOT_TYPE_Y                = 12,
        BUTTON_BORDER_SIZE          = 13,
        _COUNT,
    };
    union
    {
        ColorPacked     color;
        float           dim;
        SeUiPivotType   pivot;
    };
};

struct SeUiTextInfo
{
    const char* utf8text;
};

struct SeUiFlags
{
    enum
    {
        MOVABLE     = 0x00000001,
        RESIZABLE_X = 0x00000002,
        RESIZABLE_Y = 0x00000004,
    };
    using Type = uint32_t;
};

struct SeUiWindowInfo
{
    const char*     uid;
    float           width;
    float           height;
    SeUiFlags::Type flags;
};

enum struct SeUiButtonMode
{
    HOLD,
    TOGGLE,
    CLICK,
};

struct SeUiButtonInfo
{
    const char*     uid;
    const char*     utf8text;
    float           width;
    float           height;
    SeUiButtonMode  mode;
};

namespace ui
{
    bool                begin_ui        (const SeUiBeginInfo& info);
    SePassDependencies  end_ui          (SePassDependencies dependencies);
    void                set_font_group  (const SeUiFontGroupInfo& info);
    void                set_param       (SeUiParam::Type type, const SeUiParam& param);
    void                text            (const SeUiTextInfo& info);
    bool                begin_window    (const SeUiWindowInfo& info);
    void                end_window      ();
    bool                button          (const SeUiButtonInfo& info);

    namespace engine
    {
        void init();
        void terminate();
    }
}

namespace hash_value
{
    namespace builder
    {
        template<> void absorb<SeUiFontGroupInfo>(HashValueBuilder& builder, const SeUiFontGroupInfo& input);
    }
    template<> HashValue generate<SeUiFontGroupInfo>(const SeUiFontGroupInfo& value);
}

#endif
