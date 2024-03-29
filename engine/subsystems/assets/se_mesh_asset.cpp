
#include "se_mesh_asset.hpp"
#include "engine/se_engine.hpp"

#define CGLTF_MALLOC(size) malloc(size)
#define CGLTF_FREE(ptr) free(ptr)

// @NOTE : msvc doesn't like fopen, strncpy, strcpy stuff in cgltf lib
//         + #define _CRT_SECURE_NO_WARNINGS doesn't work for some reason
#ifdef _MSC_VER
#   pragma warning(disable:4996)
#endif

#define CGLTF_IMPLEMENTATION
#include "engine/libs/cgltf/cgltf.h"

#ifdef _MSC_VER
#   pragma warning(default:4996)
#endif

constexpr const char* SE_CGLTF_RESULT_TO_STR[] =
{
    "cgltf_result_success",
    "cgltf_result_data_too_short",
    "cgltf_result_unknown_format",
    "cgltf_result_invalid_json",
    "cgltf_result_invalid_gltf",
    "cgltf_result_invalid_options",
    "cgltf_result_file_not_found",
    "cgltf_result_io_error",
    "cgltf_result_out_of_memory",
    "cgltf_result_legacy_gltf",
    "cgltf_result_max_enum",
};

constexpr size_t SE_MAX_CGLTF_ALLOCATIONS = 256;
struct
{
    void* ptr;
    size_t size;
} g_cgltfAllocations[SE_MAX_CGLTF_ALLOCATIONS];
size_t g_numCgltfAllocations = 0;

void* _se_mesh_cgltf_alloc(void* userData, cgltf_size size)
{
    const SeAllocatorBindings allocator = se_allocator_persistent();
    // @NOTE : this is a hack - cgltf library expects _some allocation_ even if requested allocation size is zero
    const size_t allocationSize = size ? size : 4;
    void* const ptr = allocator.alloc(allocator.allocator, allocationSize, se_default_alignment, se_alloc_tag);
    se_assert(g_numCgltfAllocations < SE_MAX_CGLTF_ALLOCATIONS);
    g_cgltfAllocations[g_numCgltfAllocations++] = { ptr, allocationSize };
    return ptr;
}

void _se_mesh_cgltf_free(void* userData, void* ptr)
{
    const SeAllocatorBindings allocator = se_allocator_persistent();
    for (size_t it = 0; it < g_numCgltfAllocations; it++)
    {
        if (g_cgltfAllocations[it].ptr == ptr)
        {
            allocator.dealloc(allocator.allocator, ptr, g_cgltfAllocations[it].size);
            g_cgltfAllocations[it] = g_cgltfAllocations[g_numCgltfAllocations - 1];
            g_numCgltfAllocations -= 1;
            break;
        }
    }
}

constexpr size_t SE_MAX_CGLTF_FILES = 256;
struct
{
    SeFileHandle file;
    SeFileContent content;
} g_cgltfFiles[SE_MAX_CGLTF_FILES];
size_t g_numCgltfFiles = 0;

cgltf_result _se_mesh_cgltf_read(const cgltf_memory_options* memory_options, const cgltf_file_options* file_options, const char* path, cgltf_size* size, void** data)
{
    se_assert(g_numCgltfFiles < SE_MAX_CGLTF_FILES);
    auto* const fileEntry = &g_cgltfFiles[g_numCgltfFiles++];

    fileEntry->file = se_fs_file_find_recursive(path);
    se_assert(fileEntry->file);

    fileEntry->content = se_fs_file_read(fileEntry->file, se_allocator_frame());

    *size = fileEntry->content.dataSize;
    *data = fileEntry->content.data;

    return cgltf_result_success;
}

void _se_mesh_cgltf_release(const cgltf_memory_options* memory_options, const cgltf_file_options* file_options, void* data)
{
    se_assert(g_numCgltfFiles);
    for (size_t it = 0; it < g_numCgltfFiles; it++)
    {
        if (g_cgltfFiles[it].content.data == data)
        {
            se_fs_file_content_free(g_cgltfFiles[it].content);
            g_cgltfFiles[it] = g_cgltfFiles[g_numCgltfFiles - 1];
            g_numCgltfFiles -= 1;
            break;
        }
    }
}

const cgltf_options SE_CGLTF_OPTIONS
{
    .type = cgltf_file_type_invalid, /* invalid == auto detect */
    .json_token_count = 0, /* 0 == auto */
    .memory =
    {
        .alloc_func = _se_mesh_cgltf_alloc,
        .free_func = _se_mesh_cgltf_free,
        .user_data = nullptr,
    },
    .file =
    {
        .read = _se_mesh_cgltf_read,
        .release = _se_mesh_cgltf_release,
        .user_data = nullptr,
    },
};

inline SeFloat4x4 _se_mesh_gltf_get_node_local_transform(const cgltf_node* node)
{
    // @NOTE : this is a gltf default right-handed transform. It will be converted to left-handed later in SeMeshAsset::load function
    SeFloat4x4 rhsTransform;

    if (node->has_matrix)
    {
        // @NOTE : gltf stores matricies in column-major order, but out math library stores them in row-major order
        #define _m(r, c) node->matrix[c * 4 + r]
        se_assert(!(node->has_translation | node->has_rotation | node->has_scale));
        rhsTransform =
        {
            _m(0, 0), _m(0, 1), _m(0, 2), _m(0, 3),
            _m(1, 0), _m(1, 1), _m(1, 2), _m(1, 3),
            _m(2, 0), _m(2, 1), _m(2, 2), _m(2, 3),
            _m(3, 0), _m(3, 1), _m(3, 2), _m(3, 3),
        };
        #undef _m
    }
    else
    {
        se_assert(!node->has_matrix);
        const SeFloat4x4 translation = node->has_translation
            ? se_float4x4_from_position({ node->translation[0], node->translation[1], node->translation[2] })
            : SE_F4X4_IDENTITY;
        // @NOTE : gltf stores quaternions as { x, y, z, w }, but out math library stores them as { w, x, y, z }
        const SeFloat4x4 rotation = node->has_rotation
            ? se_quaternion_to_rotation_mat({ node->rotation[3], node->rotation[0], node->rotation[1], node->rotation[2] })
            : SE_F4X4_IDENTITY;
        const SeFloat4x4 scale = node->has_scale
            ? se_float4x4_from_scale({ node->scale[0], node->scale[1], node->scale[2] })
            : SE_F4X4_IDENTITY;
        rhsTransform = se_float4x4_mul(translation, se_float4x4_mul(rotation, scale));
    }

    return rhsTransform;
}

SeMeshNodeMask _se_mesh_process_gltf_node
(
    SeMeshAsset::Value* meshAsset,
    const cgltf_data* data,
    const cgltf_node* node,
    const size_t* meshIndexToGeometryArrayIndex,
    const SeFloat4x4& accumulatedLocalTransform,
    const SeFloat4x4& accumulatedModelTransform
)
{
    const SeFloat4x4 nodeTrfLs = _se_mesh_gltf_get_node_local_transform(node);
    const SeFloat4x4 nodeTrfLocalAccumulated = se_float4x4_mul(nodeTrfLs, accumulatedLocalTransform);
    const SeFloat4x4 nodeTrfModelAccumulated = se_float4x4_mul(nodeTrfLs, accumulatedModelTransform);

    SeMeshNode* seNode = nullptr;
    if (const cgltf_mesh* const cgltfMesh = node->mesh)
    {
        se_assert(cgltfMesh >= data->meshes);
        se_assert(size_t(cgltfMesh - data->meshes) < data->meshes_count);
        const size_t meshIndex = size_t(cgltfMesh - data->meshes);
        const size_t firstGeometryIndex = meshIndexToGeometryArrayIndex[meshIndex];
        const size_t numMeshGeometries = meshIndex == (data->meshes_count - 1)
            ? meshAsset->numGeometries - firstGeometryIndex
            : meshIndexToGeometryArrayIndex[meshIndex + 1] - firstGeometryIndex;

        if (numMeshGeometries == 0)
        {
            se_message("Mesh {} : contains zero geometry data, skip", cgltfMesh->name);
        }
        else
        {
            const char* const nodeNameCstr = node->name ? node->name : SE_MESH_UNNAMED_NODE;

            se_assert(meshAsset->numNodes < SE_MESH_MAX_NODES);
            seNode = &meshAsset->nodes[meshAsset->numNodes++];
            *seNode =
            {
                .localTrf = nodeTrfLocalAccumulated,
                .modelTrf = nodeTrfModelAccumulated,
                .geometryMask = { },
                .childNodesMask = { },
                .name = se_string_create(nodeNameCstr, SeStringLifetime::PERSISTENT)
            };

            size_t localGeometryIndex = 0;
            for (size_t it = 0; it < cgltfMesh->primitives_count; it++)
            {
                const cgltf_primitive& primitive = cgltfMesh->primitives[it];
                if (primitive.type != cgltf_primitive_type_triangles)
                {
                    se_message("Mesh {} : skipping primitive at index {} because of unsupported type {}", cgltfMesh->name, it, int(primitive.type));
                    continue;
                }

                const size_t globalGeometryIndex = firstGeometryIndex + localGeometryIndex;
                se_assert(localGeometryIndex < numMeshGeometries);
                se_assert(globalGeometryIndex < meshAsset->numGeometries);
                se_assert(globalGeometryIndex < SE_MESH_MAX_GEOMETRIES);

                se_bm_set(seNode->geometryMask, globalGeometryIndex);
                localGeometryIndex += 1;

                se_assert(meshAsset->numNodes > 0);
                SeMeshGeometry& geometry = meshAsset->geometries[globalGeometryIndex];
                se_bm_set(geometry.nodes, meshAsset->numNodes - 1);
            }
            se_assert(localGeometryIndex == numMeshGeometries);
        }
    }

    SeMeshNodeMask resultMask = {};
    if (seNode) se_bm_set(resultMask, meshAsset->numNodes - 1);

    for (size_t it = 0; it < node->children_count; it++)
    {
        const SeMeshNodeMask childMask = _se_mesh_process_gltf_node
        (
            meshAsset,
            data,
            node->children[it],
            meshIndexToGeometryArrayIndex,
            seNode ? SE_F4X4_IDENTITY : nodeTrfLocalAccumulated,
            nodeTrfModelAccumulated
        );
        if (seNode)
            se_bm_set(seNode->childNodesMask, childMask);
        else
            se_bm_set(resultMask, childMask);
    }

    return resultMask;
}

using SeMeshGltfCopyFunc = void (*)(uint8_t*, const uint8_t*);

template<typename ComponentT, size_t numComponents, float defaultW>
void _se_mesh_gltf_copy_vector(uint8_t* dst, const uint8_t* src)
{
    static_assert(numComponents <= 4 && numComponents >= 1);
    const ComponentT* const _src = (const ComponentT*)src;
    SeFloat4 result;
    if      constexpr (numComponents == 2)  result = { float(_src[0]), float(_src[1]), 0.0f, defaultW };
    else if constexpr (numComponents == 3)  result = { float(_src[0]), float(_src[1]), float(_src[2]), defaultW };
    else if constexpr (numComponents == 4)  result = { float(_src[0]), float(_src[1]), float(_src[2]), float(_src[3]) };
    else static_assert("Unsupported number of components");
    memcpy(dst, &result, sizeof(SeFloat4));
}

template<typename DestinationT, typename SourceT>
void _se_mesh_gltf_copy_triangle_indices(uint8_t* dst, const uint8_t* src)
{
    DestinationT* const _dst = (DestinationT*)dst;
    const SourceT* const _src = (const SourceT*)src;
    // @NOTE : This is not a mistake - we actually want to switch first and third indices
    //         Check _se_mesh_gltf_load_buffer_of_indices_from_accessor for extended expalanation
    _dst[0] = DestinationT(_src[2]);
    _dst[1] = DestinationT(_src[1]);
    _dst[2] = DestinationT(_src[0]);
}

SeBufferRef _se_mesh_gltf_load_buffer_from_accessor
(
    const cgltf_accessor* accessor,
    SeMeshGltfCopyFunc copyPfn,
    size_t dstElementStride,
    size_t numOfProcessedElementsInSingleCopy
)
{
    se_assert_msg(!accessor->is_sparse, "todo : support sparce accessors");

    const cgltf_buffer_view* const view = accessor->buffer_view;
                
    const uint8_t* const sourceBuffer = (uint8_t*)view->buffer->data;
    se_assert(sourceBuffer);

    const size_t sourceDataOffset = accessor->offset + view->offset;
    // @NOTE : for some reason correct stride is stored in cgltf_accessor and not in cgltf_buffer_view
    const size_t sourceElementStride = accessor->stride;
    se_assert(sourceElementStride);

    const size_t numElements = accessor->count;
    const size_t dstBufferSize = numElements * dstElementStride;
    uint8_t* const dstBuffer = (uint8_t*)se_alloc(se_allocator_frame(), dstBufferSize, se_alloc_tag);

    se_assert((numElements % numOfProcessedElementsInSingleCopy) == 0);
    const size_t numIterations = numElements / numOfProcessedElementsInSingleCopy;
    for (size_t elIt = 0; elIt < numIterations; elIt++)
    {
        const size_t sourceElementOffset = sourceDataOffset + elIt * sourceElementStride * numOfProcessedElementsInSingleCopy;
        const uint8_t* const sourceElement = sourceBuffer + sourceElementOffset;
        const size_t dstElementOffset = elIt * dstElementStride * numOfProcessedElementsInSingleCopy;
        uint8_t* const dstElement = dstBuffer + dstElementOffset;
        copyPfn(dstElement, sourceElement);
    }

    return se_render_memory_buffer({ se_data_provider_from_memory(dstBuffer, dstBufferSize) });
}

SeBufferRef _se_mesh_gltf_load_buffer_of_vectors_from_accessor(const cgltf_accessor* accessor, bool isPositionComponent)
{
    const bool isMatrix =
        accessor->type == cgltf_type_mat2 ||
        accessor->type == cgltf_type_mat3 ||
        accessor->type == cgltf_type_mat4;
    se_assert_msg(!isMatrix, "Vector buffer : unexpected matrix component");
    se_assert_msg(accessor->type != cgltf_type_scalar, "Vector buffer : unexpected scalar component");
    se_assert_msg(accessor->component_type == cgltf_component_type_r_32f, "Vector buffer : unexpected non-float component");

    const size_t dstElementStride = sizeof(SeFloat4);

    const SeMeshGltfCopyFunc copyPfn =
        accessor->type == cgltf_type_vec2 ? (isPositionComponent ? _se_mesh_gltf_copy_vector<float, 2, 1.0f> : _se_mesh_gltf_copy_vector<float, 2, 0.0f>) :
        accessor->type == cgltf_type_vec3 ? (isPositionComponent ? _se_mesh_gltf_copy_vector<float, 3, 1.0f> : _se_mesh_gltf_copy_vector<float, 3, 0.0f>) :
        accessor->type == cgltf_type_vec4 ? (isPositionComponent ? _se_mesh_gltf_copy_vector<float, 4, 1.0f> : _se_mesh_gltf_copy_vector<float, 4, 0.0f>) : nullptr;
    se_assert(copyPfn);

    return _se_mesh_gltf_load_buffer_from_accessor(accessor, copyPfn, dstElementStride, 1);
}

SeBufferRef _se_mesh_gltf_load_buffer_of_indices_from_accessor(const cgltf_accessor* accessor)
{
    se_assert_msg(accessor->type == cgltf_type_scalar, "Index buffer : expected scalar component");
    se_assert_msg(accessor->component_type != cgltf_component_type_r_32f, "Index buffer : unexpected float component");
    se_assert((accessor->count % 3) == 0);

    const size_t dstElementStride = sizeof(uint32_t);

    const SeMeshGltfCopyFunc copyPfn =
        accessor->component_type == cgltf_component_type_r_8    ? _se_mesh_gltf_copy_triangle_indices<uint32_t, int8_t> :
        accessor->component_type == cgltf_component_type_r_8u   ? _se_mesh_gltf_copy_triangle_indices<uint32_t, uint8_t> :
        accessor->component_type == cgltf_component_type_r_16   ? _se_mesh_gltf_copy_triangle_indices<uint32_t, int16_t> :
        accessor->component_type == cgltf_component_type_r_16u  ? _se_mesh_gltf_copy_triangle_indices<uint32_t, uint16_t> :
        accessor->component_type == cgltf_component_type_r_32u  ? _se_mesh_gltf_copy_triangle_indices<uint32_t, uint32_t> : nullptr;
    se_assert(copyPfn);

    // @NOTE : when we copying the indices buffer, we switch every first and third indices to make triangles face the other side.
    //         This is required since we mirror the whole mesh (check "Load nodes hierarchy" in SeMeshAsset::load) to convert it from
    //         right-handed coordinate system to left-handed coordinate system
    return _se_mesh_gltf_load_buffer_from_accessor(accessor, copyPfn, dstElementStride, 3);
}

SeMeshAsset::Intermediate SeMeshAsset::get_intermediate_data(const Info& info)
{
    const auto [data, size] = se_data_provider_get(info.data);
    se_assert(data);

    cgltf_data* gltfData = nullptr;
    const cgltf_result result = cgltf_parse(&SE_CGLTF_OPTIONS, data, size, &gltfData);
    se_assert_msg(result == cgltf_result_success, "Failed to parse gltf file. Result is : {}", SE_CGLTF_RESULT_TO_STR[result]);

    return
    {
        .data = data,
        .size = size,
        .gltfData = gltfData,
    };
}

uint32_t SeMeshAsset::count_cpu_usage_min(const Info& info, const Intermediate& intermediateData)
{
    return sizeof(SeMeshAsset::Value);
}

uint32_t SeMeshAsset::count_cpu_usage_max(const Info& info, const Intermediate& intermediateData)
{
    return sizeof(SeMeshAsset::Value);
}

uint32_t SeMeshAsset::count_gpu_usage_min(const Info& info, const Intermediate& intermediateData)
{
    return 1; // dummy
}

uint32_t SeMeshAsset::count_gpu_usage_max(const Info& info, const Intermediate& intermediateData)
{
    return 1; // dummy
}

void* SeMeshAsset::load(const Info& info, const Intermediate& intermediateData)
{
    //
    // Load buffers
    //
    se_assert(info.data.type == SeDataProvider::FROM_FILE);
    const char* const filePath = se_fs_full_path(info.data.file.handle);
    const SeFolderHandle fileFolder = se_fs_file_get_folder(info.data.file.handle);
    const cgltf_result bufferLoadResult = cgltf_load_buffers(&SE_CGLTF_OPTIONS, intermediateData.gltfData, filePath);
    se_assert_msg(bufferLoadResult == cgltf_result_success, "Unable to load cgltf buffers. Error is {}", SE_CGLTF_RESULT_TO_STR[bufferLoadResult]);

    const cgltf_data* const gltf = intermediateData.gltfData;
    SeMeshAsset::Value* const meshAsset = se_alloc<SeMeshAsset::Value>(se_allocator_persistent(), se_alloc_tag);
    *meshAsset = {};
    
    //
    // Load every texture set
    //
    for (size_t matIt = 0; matIt < gltf->materials_count; matIt++)
    {
        SeMeshTextureSet textureSet = {};

        const cgltf_material& material = gltf->materials[matIt];
        if (material.has_pbr_metallic_roughness)
        {
            const cgltf_pbr_metallic_roughness& mr = material.pbr_metallic_roughness;
            const cgltf_texture* const colorTexture = mr.base_color_texture.texture;
            if (colorTexture)
            {
                const char* const textureFileName = colorTexture->image->uri;
                const SeFileHandle textureFile = se_fs_file_find(textureFileName, fileFolder);
                textureSet.colorTexture = se_render_texture
                ({
                    .format = SeTextureFormat::RGBA_8_SRGB,
                    .data = se_data_provider_from_file(textureFile),
                });
            }
        }

        se_assert(meshAsset->numTextureSets < SE_MESH_MAX_TEXTURE_SETS);
        meshAsset->textureSets[meshAsset->numTextureSets++] = textureSet;
    }

    //
    // Load every mesh
    //
    size_t meshIndexToGeometryArrayIndex[SE_MESH_MAX_GEOMETRIES];
    for (size_t meshIt = 0; meshIt < gltf->meshes_count; meshIt++)
    {
        const cgltf_mesh& cgltfMesh = gltf->meshes[meshIt];
        meshIndexToGeometryArrayIndex[meshIt] = meshAsset->numGeometries;

        for (size_t primIt = 0; primIt < cgltfMesh.primitives_count; primIt++)
        {
            const cgltf_primitive& primitive = cgltfMesh.primitives[primIt];
            if (primitive.type != cgltf_primitive_type_triangles)
            {
                se_message("Mesh {} : skipping primitive at index {} because of unsupported type {}", cgltfMesh.name, primIt, int(primitive.type));
                continue;
            }
            SeMeshGeometry seGeometry = {};
            //
            // Assign texture set
            //
            se_assert(primitive.material);
            se_assert(primitive.material >= gltf->materials);
            se_assert(size_t(primitive.material - gltf->materials) < meshAsset->numTextureSets);
            seGeometry.textureSetIndex = size_t(primitive.material - gltf->materials);
            //
            // Load all components
            //
            for (size_t attrIt = 0; attrIt < primitive.attributes_count; attrIt++)
            {
                const cgltf_attribute& attribute = primitive.attributes[attrIt];
                
                const bool isRequiredAttribute =
                    (attribute.type == cgltf_attribute_type_position) |
                    (attribute.type == cgltf_attribute_type_normal)   |
                    (attribute.type == cgltf_attribute_type_tangent)  |
                    (attribute.type == cgltf_attribute_type_texcoord) ;
                if (!isRequiredAttribute) continue;

                se_assert(attribute.data->count <= UINT32_MAX);
                const uint32_t numElements = uint32_t(attribute.data->count);
                if (!seGeometry.numVertices) seGeometry.numVertices = numElements;
                se_assert_msg(seGeometry.numVertices == numElements, "All mesh components are expected to have the same number of elements");

                const SeBufferRef renderBuffer = _se_mesh_gltf_load_buffer_of_vectors_from_accessor(attribute.data, attribute.type == cgltf_attribute_type_position);
                switch (attribute.type)
                {
                    case cgltf_attribute_type_position: { se_assert(!seGeometry.positionBuffer);  seGeometry.positionBuffer = renderBuffer; } break;
                    case cgltf_attribute_type_normal:   { se_assert(!seGeometry.normalBuffer);    seGeometry.normalBuffer   = renderBuffer; } break;
                    case cgltf_attribute_type_tangent:  { se_assert(!seGeometry.tangentBuffer);   seGeometry.tangentBuffer  = renderBuffer; } break;
                    case cgltf_attribute_type_texcoord: { se_assert(!seGeometry.uvBuffer);        seGeometry.uvBuffer       = renderBuffer; } break;
                }
            }
            //
            // Load indices
            //
            if (primitive.indices)
            {
                seGeometry.indicesBuffer = _se_mesh_gltf_load_buffer_of_indices_from_accessor(primitive.indices);
                se_assert(primitive.indices->count < UINT32_MAX);
                seGeometry.numIndices = uint32_t(primitive.indices->count);
            }
            else
            {
                const size_t indicesBufferSize = sizeof(uint32_t) * seGeometry.numVertices;
                uint32_t* const indices = (uint32_t*)se_alloc(se_allocator_frame(), indicesBufferSize, se_alloc_tag);
                se_assert((seGeometry.numVertices % 3) == 0);
                for (uint32_t it = 0; it < seGeometry.numVertices; it++) indices[it] = it;
                seGeometry.indicesBuffer = se_render_memory_buffer({ se_data_provider_from_memory(indices, indicesBufferSize) });
                seGeometry.numIndices = seGeometry.numVertices;
            }
            se_assert(seGeometry.numIndices);
            se_assert(seGeometry.numVertices);

            se_assert(meshAsset->numGeometries < SE_MESH_MAX_GEOMETRIES);
            meshAsset->geometries[meshAsset->numGeometries++] = seGeometry;
        }
    }

    //
    // Load nodes hierarchy
    //
    for (size_t sceneIt = 0; sceneIt < gltf->scenes_count; sceneIt++)
    {
        const cgltf_scene* const scene = &gltf->scenes[sceneIt];
        for (size_t nodeIt = 0; nodeIt < scene->nodes_count; nodeIt++)
        {
            const cgltf_node* const node = scene->nodes[nodeIt];
            // @NOTE : this transform coverts model from gltf coordinate system
            //         (right-handed (z-forward, y-up, x-left) https://github.com/KhronosGroup/glTF/blob/main/specification/2.0/figures/coordinate-system.png)
            //         to Sabrina Engine's coordinate system (left-handed (z-forward, y-up, x-right))
            constexpr SeFloat4x4 GLTF_TO_SE_CONVERSION =
            {
                -1, 0, 0, 0,
                 0, 1, 0, 0,
                 0, 0, 1, 0,
                 0, 0, 0, 1,
            };
            const SeBitMask nodes = _se_mesh_process_gltf_node
            (
                meshAsset,
                gltf,
                node,
                meshIndexToGeometryArrayIndex,
                GLTF_TO_SE_CONVERSION,
                GLTF_TO_SE_CONVERSION
            );
            se_bm_set(meshAsset->rootNodes, nodes);
        }
    }

    return meshAsset;
}

void SeMeshAsset::unload(void* data)
{
    SeMeshAsset::Value* const mesh = (SeMeshAsset::Value*)data;

    for (size_t it = 0; it < mesh->numGeometries; it++)
    {
        const SeMeshGeometry& geometry = mesh->geometries[it];
        if (geometry.indicesBuffer) se_render_destroy(geometry.indicesBuffer);
        if (geometry.positionBuffer) se_render_destroy(geometry.positionBuffer);
        if (geometry.normalBuffer) se_render_destroy(geometry.normalBuffer);
        if (geometry.tangentBuffer) se_render_destroy(geometry.tangentBuffer);
        if (geometry.uvBuffer) se_render_destroy(geometry.uvBuffer);
    }

    for (size_t it = 0; it < mesh->numTextureSets; it++)
    {
        const SeMeshTextureSet& set = mesh->textureSets[it];
        if (set.colorTexture) se_render_destroy(set.colorTexture);
    }

    for (size_t it = 0; it < mesh->numNodes; it++)
    {
        const SeMeshNode& node = mesh->nodes[it];
        se_string_destroy(node.name);
    }

    se_dealloc(se_allocator_persistent(), mesh);
}

void SeMeshAsset::maximize(void* data)
{
    // SeMeshAsset::Value* const value = (SeMeshAsset::Value*)data;
    // nothing for now
}

void SeMeshAsset::minimize(void* data)
{
    // SeMeshAsset::Value* const value = (SeMeshAsset::Value*)data;
    // nothing for now
}

bool SeMeshIteratorInstance::operator != (const SeMeshIteratorInstance& other) const
{
    return !(mesh == other.mesh && index == other.index);
}

SeMeshIteratorValue SeMeshIteratorInstance::operator *  ()
{
    se_dynamic_array_reset(transforms);

    se_assert(index < mesh->numGeometries);
    const SeMeshGeometry& geometry = mesh->geometries[index];

    for (size_t nodeIt = 0; nodeIt < SE_MESH_MAX_NODES; nodeIt++)
    {
        if (!se_bm_get(geometry.nodes, nodeIt)) continue;

        se_assert(nodeIt < mesh->numNodes);
        const SeMeshNode& node = mesh->nodes[nodeIt];

        for (auto instanceIt : instances)
        {
            const SeMeshInstanceData& instance = se_iterator_value(instanceIt);
            const SeFloat4x4& trf = instance.transformWs;
            const SeFloat4x4 transformWs = se_float4x4_mul(trf, node.modelTrf);
            const SeFloat4x4 mvp = se_float4x4_mul(viewProjectionMatrix, transformWs);
            se_dynamic_array_push(transforms, se_float4x4_transposed(mvp));
        }
    }

    return { &geometry, transforms };
}

SeMeshIteratorInstance& SeMeshIteratorInstance::operator ++ ()
{
    index += 1;
    return *this;
}

SeMeshIteratorInstance begin(const SeMeshIterator& iterator)
{
    se_assert(iterator.mesh);
    return
    {
        .mesh = iterator.mesh,
        .instances = iterator.instances,
        .viewProjectionMatrix = iterator.viewProjectionMatrix,
        .index = 0,
        .transforms = se_dynamic_array_create<SeFloat4x4>(se_allocator_frame(), se_dynamic_array_size(iterator.instances) * 4),
    };
}

SeMeshIteratorInstance end(const SeMeshIterator& iterator)
{
    se_assert(iterator.mesh);
    return
    {
        .mesh = iterator.mesh,
        .instances = iterator.instances,
        .viewProjectionMatrix = iterator.viewProjectionMatrix,
        .index = iterator.mesh->numGeometries,
        .transforms = {},
    };
}
