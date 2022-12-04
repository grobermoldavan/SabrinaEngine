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
#include "engine/render/se_render.hpp"

#include "se_asset_category.hpp"

using SeMeshNodeMask = uint64_t;
using SeMeshGeometryMask = uint64_t;

struct SeMeshAssetInfo
{
    DataProvider data;
};

struct SeMeshNode
{
    SeFloat4x4          localTrf;
    SeMeshGeometryMask  geometryMask;
    SeMeshNodeMask      childNodesMask;
};

struct SeMeshGeometry
{
    SeBufferRef     indicesBuffer;
    SeBufferRef     positionBuffer;
    SeBufferRef     normalBuffer;
    SeBufferRef     tangentBuffer;
    SeBufferRef     uvBuffer;
    
    uint32_t        numVertices;
    uint32_t        numIndices;
};

struct SeMeshAssetValue
{
    // @TODO : increase max number of nodes and geometries
    static constexpr size_t MAX_GEOMETRIES = 64;
    static constexpr size_t MAX_NODES = 64;

    SeMeshGeometry  geometries[MAX_GEOMETRIES];
    SeMeshNode      nodes[MAX_NODES];
    size_t          numGeometries;
    size_t          numNodes;
    SeMeshNodeMask  rootNodes;
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

#endif
