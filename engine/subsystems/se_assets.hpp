#ifndef _SE_ASSETS_HPP_
#define _SE_ASSETS_HPP_

#include "engine/se_common_includes.hpp"
#include "engine/se_data_providers.hpp"
#include "engine/se_math.hpp"

#include "assets/se_asset_category.hpp"
#include "assets/se_mesh_asset.hpp"

using SeAssetHandle = uint64_t;

namespace assets
{
    template<typename AssetType> SeAssetHandle              add(const typename AssetType::Info& info);
    template<typename AssetType> typename AssetType::Value* access(SeAssetHandle handle);
                                 void                       remove(SeAssetHandle handle);

    namespace engine
    {
        void init(const SeSettings& settings);
        void terminate();
    }
}

#endif
