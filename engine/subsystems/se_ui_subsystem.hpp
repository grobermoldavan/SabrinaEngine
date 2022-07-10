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
    SeUiDim pix(float dim)
    {
        return { SeUiDim::PIXELS, dim };
    }

    SeUiDim rel(float dim)
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
    const char*                                     uid;
    const SeRenderAbstractionSubsystemInterface*    render;
    SeDeviceHandle                                  device;
    SePassRenderTarget                              target;
};

struct SeUiTextLineInfo
{
    const char* utf8text;
    SeUiDim     height;
    SeUiDim     baselineX;
    SeUiDim     baselineY;
};

struct SeUiWindowInfo
{
    enum Flags
    {
        RESIZABLE_X,
        RESIZABLE_Y,
        HIDEABLE,
    };
    const char* uid;
    SeUiDim     bottomLeftX;
    SeUiDim     bottomLeftY;
    SeUiDim     topRightX;
    SeUiDim     topRightY;
    uint64_t    flags;
};

struct SeUiSubsystemInterface
{
    static constexpr const char* NAME = "SeUiSubsystemInterface";

    bool (*begin_ui)(const SeUiBeginInfo& info);
    void (*end_ui)();

    void (*set_font_group)(const SeUiFontGroupInfo& info);
    void (*set_font_color)(ColorPacked color);

    void (*text_line)(const SeUiTextLineInfo& info);
    bool (*begin_window)(const SeUiWindowInfo& info);
    void (*end_window)();
};

struct SeUiSubsystem
{
    using Interface = SeUiSubsystemInterface;
    static constexpr const char* NAME = "se_ui_subsystem";
};

#define SE_UI_SUBSYSTEM_GLOBAL_NAME g_uiSubsystemIface
const SeUiSubsystemInterface* SE_UI_SUBSYSTEM_GLOBAL_NAME = nullptr;

namespace ui
{
    inline bool begin(const SeUiBeginInfo& info)
    {
        return SE_UI_SUBSYSTEM_GLOBAL_NAME->begin_ui(info);
    }

    inline void end()
    {
        SE_UI_SUBSYSTEM_GLOBAL_NAME->end_ui();
    }

    inline void set_font_group(const SeUiFontGroupInfo& info)
    {
        SE_UI_SUBSYSTEM_GLOBAL_NAME->set_font_group(info);
    }

    inline void set_font_color(ColorPacked color)
    {
        SE_UI_SUBSYSTEM_GLOBAL_NAME->set_font_color(color);
    }

    inline void text_line(const SeUiTextLineInfo& info)
    {
        SE_UI_SUBSYSTEM_GLOBAL_NAME->text_line(info);
    }

    inline bool begin_window(const SeUiWindowInfo& info)
    {
        return SE_UI_SUBSYSTEM_GLOBAL_NAME->begin_window(info);
    }

    inline void end_window()
    {
        SE_UI_SUBSYSTEM_GLOBAL_NAME->end_window();
    }
}

namespace hash_value
{
    namespace builder
    {
        template<>
        void absorb<SeUiBeginInfo>(HashValueBuilder& builder, const SeUiBeginInfo& input)
        {
            hash_value::builder::absorb_raw(builder, { (void*)input.uid, strlen(input.uid) });
        }

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
    HashValue generate<SeUiBeginInfo>(const SeUiBeginInfo& value)
    {
        HashValueBuilder builder = hash_value::builder::begin();
        hash_value::builder::absorb(builder, value);
        return hash_value::builder::end(builder);
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
