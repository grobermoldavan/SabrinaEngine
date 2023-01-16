
#include "engine/engine.hpp"
#include "engine/engine.cpp"
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
    g_duckMesh = assets::add<SeMeshAsset>({ data_provider::from_file("duck.gltf") });
    g_cameraMesh = assets::add<SeMeshAsset>({ data_provider::from_file("AntiqueCamera.gltf") });
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

void draw(const SeMeshAssetValue* mesh, const SeMeshGeometry* geometry, const DynamicArray<SeFloat4x4>& transforms)
{
    const DataProvider data = data_provider::from_memory(dynamic_array::raw(transforms), dynamic_array::raw_size(transforms));
    const SeBufferRef instancesBuffer = render::scratch_memory_buffer({ data });
    const SeTextureRef colorTexture = mesh->textureSets[geometry->textureSetIndex].colorTexture;
    render::bind
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
    render::draw({ geometry->numIndices, dynamic_array::size<uint32_t>(transforms) });
}

void update(const SeUpdateInfo& info)
{
    if (win::is_close_button_pressed()) engine::stop();

    debug_camera_update(&g_camera, info.dt);

    if (render::begin_frame())
    {
        const SeTextureSize swapChainSize = render::texture_size(render::swap_chain_texture());
        const SeTextureSize depthSize = render::texture_size(g_depthTexture);
        if (!utils::compare(depthSize, swapChainSize))
        {
            render::destroy(g_depthTexture);
            g_depthTexture = render::texture
            ({
                .format = SeTextureFormat::DEPTH_STENCIL,
                .width  = uint32_t(swapChainSize.width),
                .height = uint32_t(swapChainSize.height),
            });
        }
        
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

        const float aspect = win::get_width<float>() / win::get_height<float>();
        const SeFloat4x4 projection = render::perspective(60, aspect, 0.1f, 100.0f);
        const SeFloat4x4 viewProjection = projection * float4x4::inverted(g_camera.trf);

        const SeMeshAsset::Value* const cameraMesh = assets::access<SeMeshAsset>(g_cameraMesh);
        const auto cameraInstances = dynamic_array::create<SeMeshInstanceData>(allocators::frame(),
        {
            { SE_F4X4_IDENTITY },
            { float4x4::from_position({ 4, 0, 0 }) },
            { float4x4::from_position({ -4, 0, 0 }) },
        });
        for (auto it : SeMeshIterator{ cameraMesh, cameraInstances, viewProjection })
        {
            draw(cameraMesh, it.geometry, it.transformsWs);
        }

        const SeMeshAsset::Value* const duckMesh = assets::access<SeMeshAsset>(g_duckMesh);
        const auto duckInstances = dynamic_array::create<SeMeshInstanceData>(allocators::frame(),
        {
            { float4x4::from_position({ 0, 0, -2 }) },
            { float4x4::from_position({ 4, 0, -2 }) },
            { float4x4::from_position({ -4, 0, -2 }) },
        });
        for (auto it : SeMeshIterator{ duckMesh, duckInstances, viewProjection })
        {
            draw(duckMesh, it.geometry, it.transformsWs);
        }

        render::end_pass();

        render::end_frame();
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
    engine::run(settings, init, update, terminate);
    return 0;
}
