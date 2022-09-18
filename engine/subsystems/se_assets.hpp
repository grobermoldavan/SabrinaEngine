#ifndef _SE_ASSETS_HPP_
#define _SE_ASSETS_HPP_

#include "engine/se_common_includes.hpp"

struct SeAssetCategory
{
    using Type = uint8_t;
    enum : Type
    {
        RENDER_TEXTURE,
        RENDER_BUFFER,
        RENDER_PROGRAM,
        RENDER_MODEL,
        FONT,
        SOUND,
        __COUNT,
    };
};

using SeAssetHandle = uint64_t;

namespace assets
{
    template<typename AssetType> SeAssetHandle              add(const typename AssetType::Info& info);
    template<typename AssetType> typename AssetType::Value* access(SeAssetHandle handle);
                                 void                       remove(SeAssetHandle handle);

    namespace engine
    {
        void init(const SeSettings* settings);
        void terminate();
    }
}

#endif
