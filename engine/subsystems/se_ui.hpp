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
    SeDataProvider fonts[MAX_FONTS];
};

using SeUiEnumType = uint32_t;

struct SeUiPivotType
{
    enum : SeUiEnumType
    {
        BOTTOM_LEFT,
        CENTER,
    };
};

struct SeUiParam
{
    enum Type
    {
        FONT_COLOR                  = 0,
        FONT_HEIGHT                 = 1,
        FONT_LINE_GAP               = 2,
        PRIMARY_COLOR               = 3,
        SECONDARY_COLOR             = 4,
        ACCENT_COLOR                = 5,
        WINDOW_TOP_PANEL_THICKNESS  = 6,
        WINDOW_BORDER_THICKNESS     = 7,
        WINDOW_SCROLL_THICKNESS     = 8,
        WINDOW_INNER_PADDING        = 9,
        PIVOT_POSITION_X            = 10,
        PIVOT_POSITION_Y            = 11,
        PIVOT_TYPE_X                = 12,
        PIVOT_TYPE_Y                = 13,
        INPUT_ELEMENTS_BORDER_SIZE  = 14,
        _COUNT,
    };
    union
    {
        SeColorPacked   color;
        float           dim;
        SeUiEnumType    enumeration;
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
        MOVABLE         = 0x00000001,
        RESIZABLE_X     = 0x00000002,
        RESIZABLE_Y     = 0x00000004,
        SCROLLABLE_X    = 0x00000008,
        SCROLLABLE_Y    = 0x00000010,
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

struct SeUiRegionInfo
{
    float   width;
    float   height;
    bool    useDebugColor;
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

struct SeUiInputTextLineInfo
{
    const char* uid;
    const char* hintText;
};

bool                se_ui_begin           (const SeUiBeginInfo& info);
SePassDependencies  se_ui_end             (SePassDependencies dependencies);
void                se_ui_set_font_group  (const SeUiFontGroupInfo& info);
void                se_ui_set_param       (SeUiParam::Type type, const SeUiParam& param);
void                se_ui_text            (const SeUiTextInfo& info);
bool                se_ui_begin_window    (const SeUiWindowInfo& info);
void                se_ui_end_window      ();
bool                se_ui_begin_region    (const SeUiRegionInfo& info);
void                se_ui_end_region      ();
bool                se_ui_button          (const SeUiButtonInfo& info);
const char*         se_ui_input_text_line (const SeUiInputTextLineInfo& info);

void _se_ui_init();
void _se_ui_terminate();

template<> void se_hash_value_builder_absorb<SeUiFontGroupInfo>(SeHashValueBuilder& builder, const SeUiFontGroupInfo& input);
template<> SeHashValue se_hash_value_generate<SeUiFontGroupInfo>(const SeUiFontGroupInfo& value);

#endif
