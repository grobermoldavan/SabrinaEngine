#ifndef _SE_ENGINE_HPP_
#define _SE_ENGINE_HPP_

#include "engine/se_common_includes.hpp"
#include "engine/se_math.hpp"
#include "engine/se_allocator_bindings.hpp"
#include "engine/se_hash.hpp"
#include "engine/se_utils.hpp"
#include "engine/se_containers.hpp"
#include "engine/se_data_providers.hpp"

#include "engine/render/se_render.hpp"
#include "engine/subsystems/se_platform.hpp"
#include "engine/subsystems/se_application_allocators.hpp"
#include "engine/subsystems/se_string.hpp"
#include "engine/subsystems/se_debug.hpp"
#include "engine/subsystems/se_window.hpp"
#include "engine/subsystems/se_assets.hpp"
#include "engine/subsystems/se_ui.hpp"

struct SeSettings
{
    const char* applicationName;
    bool        isFullscreenWindow;
    bool        isResizableWindow;
    uint32_t    windowWidth;
    uint32_t    windowHeight;
    size_t      maxAssetsCpuUsage;
    size_t      maxAssetsGpuUsage;
};

struct SeUpdateInfo
{
    float dt;
    size_t frame;
};

using SeInitPfn = void (*)();
using SeUpdatePfn = void (*)(const SeUpdateInfo&);
using SeTerminatePfn = void (*)();

namespace engine
{
    void run(const SeSettings* settings, SeInitPfn init, SeUpdatePfn update, SeTerminatePfn terminate);
    void stop();
}


#endif
