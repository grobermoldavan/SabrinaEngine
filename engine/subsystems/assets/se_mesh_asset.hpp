#ifndef _SE_MESH_ASSET_HPP_
#define _SE_MESH_ASSET_HPP_

/*
    Implementation notes:
        - currently we expect all geometry to have a clockwise front face
            - @TODO : add support for counter-clockwise front faces (https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#instantiation)
        - only triangle geometry gets processed (no triangle strips, triangle fans and other types)
        - only a single texcoord supported for a single geometry (gltf supports arbitrary amount of tex coords)
*/

#include "engine/se_common_includes.hpp"
#include "engine/se_data_providers.hpp"
#include "engine/se_math.hpp"
#include "engine/se_utils.hpp"
#include "engine/render/se_render.hpp"

#include "se_asset_category.hpp"

constexpr const size_t SE_MESH_MAX_NODES = 128;
constexpr const size_t SE_MESH_MAX_GEOMETRIES = 128;
constexpr const size_t SE_MESH_MAX_TEXTURE_SETS = 64;

using SeMeshNodeMask = SeBitMask<SE_MESH_MAX_NODES>;
using SeMeshGeometryMask = SeBitMask<SE_MESH_MAX_GEOMETRIES>;

struct SeMeshAssetInfo
{
    DataProvider data;
};

struct SeMeshNode
{
    SeFloat4x4          localTrf;
    SeFloat4x4          modelTrf;

    SeMeshGeometryMask  geometryMask;
    SeMeshNodeMask      childNodesMask;
};

struct SeMeshGeometry
{
    // @NOTE : All nodes that reference this geometry. Used in mesh iterator
    SeMeshNodeMask  nodes;

    SeBufferRef     indicesBuffer;
    SeBufferRef     positionBuffer;
    SeBufferRef     normalBuffer;
    SeBufferRef     tangentBuffer;
    SeBufferRef     uvBuffer;
    
    uint32_t        numVertices;
    uint32_t        numIndices;

    size_t          textureSetIndex;
};

struct SeMeshTextureSet
{
    SeTextureRef colorTexture;
};

struct SeMeshAssetValue
{
    SeMeshNode          nodes[SE_MESH_MAX_NODES];
    SeMeshGeometry      geometries[SE_MESH_MAX_GEOMETRIES];
    SeMeshTextureSet    textureSets[SE_MESH_MAX_TEXTURE_SETS];

    size_t              numGeometries;
    size_t              numNodes;
    size_t              numTextureSets;

    SeMeshNodeMask      rootNodes;
};

struct SeMeshAssetIntermediateData
{
    void* data;
    size_t size;
    struct cgltf_data* gltfData;
};

struct SeMeshAsset
{
    static constexpr SeAssetCategory::Type CATEGORY = SeAssetCategory::MESH;

    using Info = SeMeshAssetInfo;
    using Value = SeMeshAssetValue;
    using Intermediate = SeMeshAssetIntermediateData;

    static Intermediate get_intermediate_data(const Info& info);

    static uint32_t count_cpu_usage_min(const Info& info, const Intermediate& intermediateData);
    static uint32_t count_cpu_usage_max(const Info& info, const Intermediate& intermediateData);
    static uint32_t count_gpu_usage_min(const Info& info, const Intermediate& intermediateData);
    static uint32_t count_gpu_usage_max(const Info& info, const Intermediate& intermediateData);
    static void*    load(const Info& info, const Intermediate& intermediateData);
    static void     unload(void* data);
    static void     maximize(void* data);
    static void     minimize(void* data);
};

struct SeMeshInstanceData
{
    SeFloat4x4 transformWs;
};

struct SeMeshIterator
{
    const SeMeshAssetValue* mesh;
    const DynamicArray<SeMeshInstanceData> instances;
    const SeFloat4x4 viewProjectionMatrix;
};

struct SeMeshIteratorValue
{
    const SeMeshGeometry* geometry;
    const DynamicArray<SeFloat4x4> transformsWs;
};

struct SeMeshIteratorInstance
{
    const SeMeshAssetValue*                 mesh;
    const DynamicArray<SeMeshInstanceData>  instances;
    const SeFloat4x4                        viewProjectionMatrix;

    size_t                                  index;
    DynamicArray<SeFloat4x4>                transforms;

    bool                    operator != (const SeMeshIteratorInstance& other) const;
    SeMeshIteratorValue     operator *  ();
    SeMeshIteratorInstance& operator ++ ();
};

SeMeshIteratorInstance begin(const SeMeshIterator& iterator);
SeMeshIteratorInstance end(const SeMeshIterator& iterator);

#endif
