#ifndef _SE_ASSET_CATEGORY_HPP_
#define _SE_ASSET_CATEGORY_HPP_

#include "engine/se_common_includes.hpp"

struct SeAssetCategory
{
    using Type = uint8_t;
    enum : Type
    {
        MESH,
        SOUND,
        __COUNT,
    };
};

#endif
