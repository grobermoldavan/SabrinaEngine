#ifndef _SE_ASSETS_HPP_
#define _SE_ASSETS_HPP_

#include "engine/se_common_includes.hpp"
#include "engine/se_data_providers.hpp"
#include "engine/se_math.hpp"

#include "assets/se_asset_category.hpp"
#include "assets/se_mesh_asset.hpp"

using SeAssetHandle = uint64_t;

template<typename AssetType> SeAssetHandle              se_asset_add(const typename AssetType::Info& info);
template<typename AssetType> typename AssetType::Value* se_asset_access(SeAssetHandle handle);
                                void                    se_asset_remove(SeAssetHandle handle);

void _se_asset_init(const SeSettings& settings);
void _se_asset_terminate();

#endif
