
#include "cubes.hpp"
#include "debug_camera.hpp"

//
// Based on http://paulbourke.net/geometry/polygonise/
//

#define CHUNK_DIM 64
#define NUM_VERTS ((CHUNK_DIM - 1) * (CHUNK_DIM - 1) * (CHUNK_DIM - 1) * 5 * 3)

struct Vertex
{
    SeFloat4 position;
    SeFloat4 normal;
};

struct FrameData
{
    SeFloat4x4 viewProjection;
    float time;
    float isoLevel;
    float _pad[2];
};

SeProgramRef    g_clearChunkCs;
SeProgramRef    g_generateChunkCs;
SeProgramRef    g_triangulateChunkCs;
SeProgramRef    g_renderChunkVs;
SeProgramRef    g_renderChunkFs;
SeTextureRef    g_grassTexture;
SeTextureRef    g_rockTexture;
SeTextureRef    g_depthTexture;

SeBufferRef     g_edgeTableBuffer;
SeBufferRef     g_triangleTableBuffer;
SeBufferRef     g_gridValuesBuffer;
SeBufferRef     g_geometryBuffer;

SeSamplerRef    g_sampler;

DebugCamera     g_camera;

void init()
{
    g_clearChunkCs          = render::program({ data_provider::from_file("assets/application/shaders/clear_chunk.comp.spv") });
    g_generateChunkCs       = render::program({ data_provider::from_file("assets/application/shaders/generate_chunk.comp.spv") });
    g_triangulateChunkCs    = render::program({ data_provider::from_file("assets/application/shaders/triangulate_chunk.comp.spv") });
    g_renderChunkVs         = render::program({ data_provider::from_file("assets/application/shaders/render_chunk.vert.spv") });
    g_renderChunkFs         = render::program({ data_provider::from_file("assets/application/shaders/render_chunk.frag.spv") });

    g_grassTexture = render::texture
    ({
        .format = SeTextureFormat::RGBA_8_SRGB,
        .data   = data_provider::from_file("assets/application/textures/grass.png"),
    });
    g_rockTexture = render::texture
    ({
        .format = SeTextureFormat::RGBA_8_SRGB,
        .data   = data_provider::from_file("assets/application/textures/rocks.png"),
    });
    g_depthTexture = render::texture
    ({
        .format = SeTextureFormat::DEPTH_STENCIL,
        .width  = win::get_width<uint32_t>(),
        .height = win::get_height<uint32_t>(),
    });
    
    g_edgeTableBuffer       = render::memory_buffer({ data_provider::from_memory(EDGE_TABLE, sizeof(EDGE_TABLE)) });
    g_triangleTableBuffer   = render::memory_buffer({ data_provider::from_memory(TRIANGLE_TABLE, sizeof(TRIANGLE_TABLE)) });
    g_gridValuesBuffer      = render::memory_buffer({ data_provider::from_memory(nullptr, sizeof(float) * CHUNK_DIM * CHUNK_DIM * CHUNK_DIM) });
    g_geometryBuffer        = render::memory_buffer({ data_provider::from_memory(nullptr, sizeof(Vertex) * NUM_VERTS) });

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
    
    debug_camera_construct(&g_camera, { CHUNK_DIM / 2, CHUNK_DIM, -8 });
}

void terminate()
{

}

SePassDependencies execute_compute(SeBufferRef frameDataBuffer, SeProgramRef program, SePassDependencies deps)
{
    const SeProgramWithConstants computeProgramInfo
    {
        .program = program,
        .specializationConstants =
        {
            { .constantId = 0, .asUint = CHUNK_DIM, },
            { .constantId = 1, .asUint = CHUNK_DIM, },
            { .constantId = 2, .asUint = CHUNK_DIM, },
        },
        .numSpecializationConstants = 3,
    };
    const SePassDependencies resultDeps = render::begin_compute_pass({ .dependencies = deps, .program = computeProgramInfo });
    render::bind({ .set = 0, .bindings = { { .binding = 0, .buffer = frameDataBuffer } } });
    render::bind
    ({
        .set = 1,
        .bindings =
        {
            { .binding = 0, .buffer = g_gridValuesBuffer },
            { .binding = 1, .buffer = g_geometryBuffer },
            { .binding = 2, .buffer = g_edgeTableBuffer },
            { .binding = 3, .buffer = g_triangleTableBuffer }
        }
    });
    const SeComputeWorkgroupSize workgroupSize = render::workgroup_size(program);
    render::dispatch
    ({
        1 + ((CHUNK_DIM - 1) / workgroupSize.x),
        1 + ((CHUNK_DIM - 1) / workgroupSize.y),
        1 + ((CHUNK_DIM - 1) / workgroupSize.z),   
    });
    render::end_pass();
    return resultDeps;
};

void update(const SeUpdateInfo& updateInfo)
{
    if (win::is_close_button_pressed() || win::is_keyboard_button_pressed(SeKeyboard::ESCAPE)) engine::stop();

    //
    // Fill frame data
    //
    static float time = 0.0f;
    static float isoLevel = 0.5f;
    time += updateInfo.dt;
    isoLevel += float(win::get_mouse_wheel()) * 0.01f;
    const SeFloat4x4 projection = render::perspective
    (
        90,
        win::get_width<float>() / win::get_height<float>(),
        0.1f,
        200.0f
    );
    debug_camera_update(&g_camera, updateInfo.dt);
    const FrameData frameData
    {
        .viewProjection = float4x4::transposed(projection * float4x4::inverted(g_camera.trf)),
        .time           = time,
        .isoLevel       = isoLevel,
    };

    if (render::begin_frame())
    {
        //
        // Recreate depth texture on resize
        //
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
        const SeBufferRef frameDataBuffer = render::scratch_memory_buffer({ data_provider::from_memory(frameData) });
        const SePassDependencies clearChunkDependency = execute_compute(frameDataBuffer, g_clearChunkCs, 0);
        const SePassDependencies generateChunkDependency = execute_compute(frameDataBuffer, g_generateChunkCs, clearChunkDependency);
        const SePassDependencies triangulateChunkDependency = execute_compute(frameDataBuffer, g_triangulateChunkCs, generateChunkDependency);
        render::begin_graphics_pass
        ({
            .dependencies           = triangulateChunkDependency,
            .vertexProgram          = { .program = g_renderChunkVs, },
            .fragmentProgram        = { .program = g_renderChunkFs, },
            .frontStencilOpState    = { .isEnabled = false, },
            .backStencilOpState     = { .isEnabled = false, },
            .depthState             = { .isTestEnabled = true, .isWriteEnabled = true },
            .polygonMode            = SePipelinePolygonMode::FILL,
            .cullMode               = SePipelineCullMode::BACK,
            .frontFace              = SePipelineFrontFace::COUNTER_CLOCKWISE,
            .samplingType           = SeSamplingType::_1,
            .renderTargets          = { { render::swap_chain_texture(), SeRenderTargetLoadOp::CLEAR } },
            .depthStencilTarget     = { g_depthTexture, SeRenderTargetLoadOp::CLEAR },
        });
        {
            render::bind({ .set = 0, .bindings =
            {
                { .binding = 0, .type = SeBinding::TEXTURE, .buffer = { frameDataBuffer } }
            } });
            render::bind({ .set = 1, .bindings =
            {
                { .binding = 0, .type = SeBinding::TEXTURE, .buffer = { g_geometryBuffer } }
            } });
            render::bind({ .set = 2, .bindings =
            {
                { .binding = 0, .type = SeBinding::TEXTURE, .texture = { g_grassTexture, g_sampler } },
                { .binding = 1, .type = SeBinding::TEXTURE, .texture = { g_rockTexture, g_sampler } }
            } });
            render::draw({ .numVertices = NUM_VERTS, .numInstances = 1 });
        }
        render::end_pass();
        render::end_frame();
    }
}
