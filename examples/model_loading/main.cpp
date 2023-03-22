
#include "engine/se_engine.hpp"
#include "engine/se_engine.cpp"
#include "debug_camera.hpp"

DebugCamera g_camera;

SeAssetHandle g_duckMesh;
SeAssetHandle g_cameraMesh;

SeProgramRef g_drawVs;
SeProgramRef g_drawFs;
SeTextureRef g_depthTexture;
SeSamplerRef g_sampler;

void init()
{
    g_duckMesh = se_asset_add<SeMeshAsset>({ se_data_provider_from_file("duck.gltf") });
    g_cameraMesh = se_asset_add<SeMeshAsset>({ se_data_provider_from_file("AntiqueCamera.gltf") });
    g_drawVs = se_render_program({ se_data_provider_from_file("mesh_unlit.vert.spv") });
    g_drawFs = se_render_program({ se_data_provider_from_file("mesh_unlit.frag.spv") });
    g_depthTexture = se_render_texture
    ({
        .format = SeTextureFormat::DEPTH_STENCIL,
        .width  = se_win_get_width(),
        .height = se_win_get_height(),
    });
    g_sampler = se_render_sampler
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

void draw(const SeMeshAssetValue* mesh, const SeMeshGeometry* geometry, const SeDynamicArray<SeFloat4x4>& transforms)
{
    const SeDataProvider data = se_data_provider_from_memory(se_dynamic_array_raw(transforms), se_dynamic_array_raw_size(transforms));
    const SeBufferRef instancesBuffer = se_render_scratch_memory_buffer({ data });
    const SeTextureRef colorTexture = mesh->textureSets[geometry->textureSetIndex].colorTexture;
    se_render_bind
    ({
        .set = 0,
        .bindings =
        {
            { .binding = 0, .type = SeBinding::BUFFER, .buffer = geometry->positionBuffer },
            { .binding = 1, .type = SeBinding::BUFFER, .buffer = geometry->uvBuffer },
            { .binding = 2, .type = SeBinding::BUFFER, .buffer = geometry->indicesBuffer },
            { .binding = 3, .type = SeBinding::BUFFER, .buffer = instancesBuffer },
            { .binding = 4, .type = SeBinding::TEXTURE, .texture = { colorTexture, g_sampler } },
        }
    });
    se_render_draw({ geometry->numIndices, se_dynamic_array_size<uint32_t>(transforms) });
}

void update(const SeUpdateInfo& info)
{
    if (se_win_is_close_button_pressed()) se_engine_stop();

    debug_camera_update(&g_camera, info.dt);

    if (se_render_begin_frame())
    {
        const SeTextureSize swapChainSize = se_render_texture_size(se_render_swap_chain_texture());
        const SeTextureSize depthSize = se_render_texture_size(g_depthTexture);
        if (!se_compare(depthSize, swapChainSize))
        {
            se_render_destroy(g_depthTexture);
            g_depthTexture = se_render_texture
            ({
                .format = SeTextureFormat::DEPTH_STENCIL,
                .width  = uint32_t(swapChainSize.width),
                .height = uint32_t(swapChainSize.height),
            });
        }
        
        se_render_begin_graphics_pass
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
            .renderTargets          = { se_render_swap_chain_texture(), SeRenderTargetLoadOp::CLEAR },
            .depthStencilTarget     = { g_depthTexture, SeRenderTargetLoadOp::CLEAR },
        });

        const float aspect = se_win_get_width<float>() / se_win_get_height<float>();
        const SeFloat4x4 projection = se_render_perspective(60, aspect, 0.1f, 100.0f);
        const SeFloat4x4 viewProjection = projection * se_float4x4_inverted(g_camera.trf);

        const SeMeshAsset::Value* const cameraMesh = se_asset_access<SeMeshAsset>(g_cameraMesh);
        const auto cameraInstances = se_dynamic_array_create<SeMeshInstanceData>(se_allocator_frame(),
        {
            { SE_F4X4_IDENTITY },
            { se_float4x4_from_position({ 4, 0, 0 }) },
            { se_float4x4_from_position({ -4, 0, 0 }) },
        });
        for (auto it : SeMeshIterator{ cameraMesh, cameraInstances, viewProjection })
        {
            draw(cameraMesh, it.geometry, it.transformsWs);
        }

        const SeMeshAsset::Value* const duckMesh = se_asset_access<SeMeshAsset>(g_duckMesh);
        const auto duckInstances = se_dynamic_array_create<SeMeshInstanceData>(se_allocator_frame(),
        {
            { se_float4x4_from_position({ 0, 0, -2 }) },
            { se_float4x4_from_position({ 4, 0, -2 }) },
            { se_float4x4_from_position({ -4, 0, -2 }) },
        });
        for (auto it : SeMeshIterator{ duckMesh, duckInstances, viewProjection })
        {
            draw(duckMesh, it.geometry, it.transformsWs);
        }

        se_render_end_pass();

        se_render_end_frame();
    }
}

int main(int argc, char* argv[])
{
    const SeSettings settings
    {
        .applicationName        = "Sabrina engine - model loading example",
        .isFullscreenWindow     = false,
        .isResizableWindow      = true,
        .windowWidth            = 640,
        .windowHeight           = 480,
        .maxAssetsCpuUsage      = se_megabytes(512),
        .maxAssetsGpuUsage      = se_megabytes(512),
        .createUserDataFolder   = false,
    };
    se_engine_run(settings, init, update, terminate);
    return 0;
}
