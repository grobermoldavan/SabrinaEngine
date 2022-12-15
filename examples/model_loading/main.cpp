
#include "engine/engine.hpp"
#include "engine/engine.cpp"
#include "debug_camera.hpp"

DebugCamera g_camera;

SeAssetHandle g_meshHandle;

SeProgramRef g_drawVs;
SeProgramRef g_drawFs;
SeTextureRef g_depthTexture;
SeSamplerRef g_sampler;

void init()
{
    g_meshHandle = assets::add<SeMeshAsset>({ data_provider::from_file("duck.gltf") });
    g_drawVs = render::program({ data_provider::from_file("mesh_unlit.vert.spv") });
    g_drawFs = render::program({ data_provider::from_file("mesh_unlit.frag.spv") });
    g_depthTexture = render::texture
    ({
        .format = SeTextureFormat::DEPTH_STENCIL,
        .width  = win::get_width(),
        .height = win::get_height(),
    });
    g_sampler = render::sampler
    ({
        .magFilter          = SeSamplerFilter::LINEAR,
        .minFilter          = SeSamplerFilter::LINEAR,
        .addressModeU       = SeSamplerAddressMode::REPEAT,
        .addressModeV       = SeSamplerAddressMode::REPEAT,
        .addressModeW       = SeSamplerAddressMode::REPEAT,
        .mipmapMode         = SeSamplerMipmapMode::LINEAR,
        .mipLodBias         = 0.0f,
        .minLod             = 0.0f,
        .maxLod             = 0.0f,
        .anisotropyEnable   = false,
        .maxAnisotropy      = 0.0f,
        .compareEnabled     = false,
        .compareOp          = SeCompareOp::ALWAYS,
    });

    debug_camera_construct(&g_camera, { 0, 0, -10 });
}

void terminate()
{
    
}

using MeshCallback = void(*)(const SeMeshAsset::Value* mesh, const SeFloat4x4& trf, const SeMeshGeometry* geometry);

void process_mesh_node_recursive(const SeMeshAsset::Value* mesh, const SeMeshNode* node, const SeFloat4x4& parentTrf, MeshCallback cb)
{
    const SeFloat4x4 nodeTrf = float4x4::mul(node->localTrf, parentTrf);
    for (size_t it = 0; it < SeMeshAssetValue::MAX_GEOMETRIES; it++)
    {
        const bool isValidIndex = node->geometryMask & (1ull << it);
        if (!isValidIndex) continue;
        cb(mesh, nodeTrf, &mesh->geometries[it]);
    }
    for (size_t it = 0; it < SeMeshAssetValue::MAX_NODES; it++)
    {
        const bool isChild = node->childNodesMask & (1ull << it);
        if (!isChild) continue;
        process_mesh_node_recursive(mesh, &mesh->nodes[it], nodeTrf, cb);
    }
}

void process_mesh(const SeMeshAsset::Value* mesh, const SeFloat4x4& trf, MeshCallback cb)
{
    for (size_t it = 0; it < SeMeshAssetValue::MAX_NODES; it++)
    {
        const bool isRoot = mesh->rootNodes & (1ull << it);
        if (!isRoot) continue;
        process_mesh_node_recursive(mesh, &mesh->nodes[it], trf, cb);
    }
}

void update(const SeUpdateInfo& info)
{
    if (win::is_close_button_pressed()) engine::stop();

    debug_camera_update(&g_camera, info.dt);

    if (render::begin_frame())
    {
        render::begin_graphics_pass
        ({
            .dependencies           = 0,
            .vertexProgram          = { g_drawVs },
            .fragmentProgram        = { g_drawFs },
            .frontStencilOpState    = { .isEnabled = false },
            .backStencilOpState     = { .isEnabled = false },
            .depthState             = { .isTestEnabled = true, .isWriteEnabled = true },
            .polygonMode            = SePipelinePolygonMode::FILL,
            .cullMode               = SePipelineCullMode::BACK,
            .frontFace              = SePipelineFrontFace::CLOCKWISE,
            .samplingType           = SeSamplingType::_1,
            .renderTargets          = { render::swap_chain_texture(), SeRenderTargetLoadOp::CLEAR },
            .depthStencilTarget     = { g_depthTexture, SeRenderTargetLoadOp::CLEAR },
        });

        const SeMeshAsset::Value* const mesh = assets::access<SeMeshAsset>(g_meshHandle);
        const SeFloat4x4 meshTrf = SE_F4X4_IDENTITY;
        process_mesh(mesh, meshTrf, [](const SeMeshAsset::Value* mesh, const SeFloat4x4& trf, const SeMeshGeometry* geometry)
        {
            const SeFloat4x4 view = g_camera.trf;
            const float aspect = win::get_width<float>() / win::get_height<float>();
            const SeFloat4x4 perspective = render::perspective(60, aspect, 0.1f, 100.0f);
            const SeFloat4x4 vp = perspective * float4x4::inverted(view);

            const SeFloat4x4 mvp = float4x4::transposed(float4x4::mul(vp, trf));
            const SeBufferRef instances = render::scratch_memory_buffer({ data_provider::from_memory(mvp) });

            const SeTextureRef colorTexture = mesh->textureSets[geometry->textureSetIndex].colorTexture;
            render::bind
            ({
                .set = 0,
                .bindings =
                {
                    { .binding = 0, .type = SeBinding::BUFFER, .buffer = geometry->positionBuffer },
                    { .binding = 1, .type = SeBinding::BUFFER, .buffer = geometry->uvBuffer },
                    { .binding = 2, .type = SeBinding::BUFFER, .buffer = geometry->indicesBuffer },
                    { .binding = 3, .type = SeBinding::BUFFER, .buffer = instances },
                    { .binding = 4, .type = SeBinding::TEXTURE, .texture = { colorTexture, g_sampler } },
                }
            });
            render::draw({ geometry->numIndices, 1 });
        });

        render::end_pass();

        render::end_frame();
    }
}

int main(int argc, char* argv[])
{
    const SeSettings settings
    {
        .applicationName    = "Sabrina engine - model loading example",
        .isFullscreenWindow = false,
        .isResizableWindow  = true,
        .windowWidth        = 640,
        .windowHeight       = 480,
        .maxAssetsCpuUsage  = se_megabytes(512),
        .maxAssetsGpuUsage  = se_megabytes(512),
    };
    engine::run(&settings, init, update, terminate);
    return 0;
}
