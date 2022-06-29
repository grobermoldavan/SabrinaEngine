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
        CENTIMETERS,
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

    SeUiDim cm(float dim)
    {
        return { SeUiDim::CENTIMETERS, dim };
    }

    SeUiDim rel(float dim)
    {
        return { SeUiDim::TARGET_RELATIVE, dim };
    }
}

struct SeUiBeginInfo
{
    const struct SeWindowHandle& window;
    const SeRenderAbstractionSubsystemInterface* render;
    SeDeviceHandle device;
};

struct SeUiTextLineInfo
{
    const char* utf8text;
    SeUiDim height;
    SeUiDim baselineX;
    SeUiDim baselineY;
};

struct SeUiFontGroupInfo
{
    static constexpr size_t MAX_FONTS = 8;
    DataProvider fonts[MAX_FONTS];
};

struct SeUiSubsystemInterface
{
    static constexpr const char* NAME = "SeUiSubsystemInterface";

    void (*begin_ui)(const SeUiBeginInfo& info);
    void (*end_ui)();

    void (*set_render_target)(SeRenderRef target);
    void (*set_font_group)(const SeUiFontGroupInfo& info);
    void (*text_line)(const SeUiTextLineInfo& info);
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
    inline void begin(const SeUiBeginInfo& info)
    {
        SE_UI_SUBSYSTEM_GLOBAL_NAME->begin_ui(info);
    }

    inline void end()
    {
        SE_UI_SUBSYSTEM_GLOBAL_NAME->end_ui();
    }

    inline void set_font_group(const SeUiFontGroupInfo& info)
    {
        SE_UI_SUBSYSTEM_GLOBAL_NAME->set_font_group(info);
    }

    inline void set_render_target(SeRenderRef target)
    {
        SE_UI_SUBSYSTEM_GLOBAL_NAME->set_render_target(target);
    }

    inline void text_line(const SeUiTextLineInfo& info)
    {
        SE_UI_SUBSYSTEM_GLOBAL_NAME->text_line(info);
    }
}

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
