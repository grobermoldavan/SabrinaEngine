
#include "cubes.hpp"
#include "debug_camera.hpp"

//
// Based on http://paulbourke.net/geometry/polygonise/
//

#define CHUNK_DIM 64

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

DataProvider    g_clearChunkCsData;
DataProvider    g_generateChunkCsData;
DataProvider    g_triangulateChunkCsData;
DataProvider    g_renderChunkVsData;
DataProvider    g_renderChunkFsData;

SeRenderRef     g_edgeTableBuffer;
SeRenderRef     g_triangleTableBuffer;
SeRenderRef     g_gridValuesBuffer;
SeRenderRef     g_geometryBuffer;

DataProvider    g_grassTextureData;
DataProvider    g_rockTextureData;

DebugCamera     g_camera;

void init()
{
    //
    // Load shaders
    //
    g_clearChunkCsData          = data_provider::from_file("assets/application/shaders/clear_chunk.comp.spv");
    g_generateChunkCsData       = data_provider::from_file("assets/application/shaders/generate_chunk.comp.spv");
    g_triangulateChunkCsData    = data_provider::from_file("assets/application/shaders/triangulate_chunk.comp.spv");
    g_renderChunkVsData         = data_provider::from_file("assets/application/shaders/render_chunk.vert.spv");
    g_renderChunkFsData         = data_provider::from_file("assets/application/shaders/render_chunk.frag.spv");
    //
    // Load textures
    //
    g_grassTextureData = data_provider::from_file("assets/application/textures/grass.png");
    g_rockTextureData  = data_provider::from_file("assets/application/textures/rocks.png");
    //
    // Allocate buffers
    //
    constexpr uint32_t numVerts = (CHUNK_DIM - 1) * (CHUNK_DIM - 1) * (CHUNK_DIM - 1) * 5 * 3;
    g_edgeTableBuffer     = render::memory_buffer({ data_provider::from_memory(EDGE_TABLE, sizeof(EDGE_TABLE)) });
    g_triangleTableBuffer = render::memory_buffer({ data_provider::from_memory(TRIANGLE_TABLE, sizeof(TRIANGLE_TABLE)) });
    g_gridValuesBuffer    = render::memory_buffer({ data_provider::from_memory(nullptr, sizeof(float) * CHUNK_DIM * CHUNK_DIM * CHUNK_DIM) });
    g_geometryBuffer      = render::memory_buffer({ data_provider::from_memory(nullptr, sizeof(Vertex) * numVerts) });
    //
    // Init camera
    //
    debug_camera_construct(&g_camera, { CHUNK_DIM / 2, CHUNK_DIM, -8 });
}

void terminate()
{
    data_provider::destroy(g_clearChunkCsData);
    data_provider::destroy(g_generateChunkCsData);
    data_provider::destroy(g_triangulateChunkCsData);
    data_provider::destroy(g_renderChunkVsData);
    data_provider::destroy(g_renderChunkFsData);
    data_provider::destroy(g_grassTextureData);
    data_provider::destroy(g_rockTextureData);
}

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
        ((float)win::get_width()) / ((float)win::get_height()),
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
        constexpr uint32_t numVerts = (CHUNK_DIM - 1) * (CHUNK_DIM - 1) * (CHUNK_DIM - 1) * 5 * 3;
        const SeRenderRef clearChunkCs          = render::program({ g_clearChunkCsData });
        const SeRenderRef generateChunkCs       = render::program({ g_generateChunkCsData });
        const SeRenderRef triangulateChunkCs    = render::program({ g_triangulateChunkCsData });
        const SeRenderRef renderChunkVs         = render::program({ g_renderChunkVsData });
        const SeRenderRef renderChunkFs         = render::program({ g_renderChunkFsData });
        const SeRenderRef frameDataBuffer       = render::memory_buffer({ data_provider::from_memory(&frameData, sizeof(frameData)) });
        SeProgramWithConstants computeProgramInfo
        {
            .program = { /* filled later */ },
            .specializationConstants =
            {
                { .constantId = 0, .asUint = CHUNK_DIM, },
                { .constantId = 1, .asUint = CHUNK_DIM, },
                { .constantId = 2, .asUint = CHUNK_DIM, },
            },
            .numSpecializationConstants = 3,
        };
        auto executeComputePass = [&](SeRenderRef program)
        {
            render::bind({ .set = 0, .bindings = { { 0, frameDataBuffer } } });
            render::bind
            ({
                .set = 1,
                .bindings = { { 0, g_gridValuesBuffer }, { 1, g_geometryBuffer }, { 2, g_edgeTableBuffer }, { 3, g_triangleTableBuffer } }
            });
            const SeComputeWorkgroupSize workgroupSize = render::workgroup_size(program);
            render::dispatch
            ({
                1 + ((CHUNK_DIM - 1) / workgroupSize.x),
                1 + ((CHUNK_DIM - 1) / workgroupSize.y),
                1 + ((CHUNK_DIM - 1) / workgroupSize.z),   
            });
        };
        enum
        {
            CLEAR_CHUNK_PASS,
            GENERATE_CHUNK_PASS,
            TRIANGULATE_CHUNK_PASS,
        };
        //
        // Clear pass
        //
        computeProgramInfo.program = clearChunkCs;
        const SeRenderRef clearChunkPipeline = render::compute_pipeline({ computeProgramInfo });
        const SePassDependencies clearChunkDependency = render::begin_pass({ .dependencies = 0, .pipeline = clearChunkPipeline });
        executeComputePass(clearChunkCs);
        render::end_pass();
        //
        // Generate pass
        //
        computeProgramInfo.program = generateChunkCs;
        const SeRenderRef generateChunkPipeline = render::compute_pipeline({ computeProgramInfo });
        const SePassDependencies generateChunkDependency = render::begin_pass({ .dependencies = clearChunkDependency, .pipeline = generateChunkPipeline });
        executeComputePass(generateChunkCs);
        render::end_pass();
        //
        // Triangulate pass
        //
        computeProgramInfo.program = triangulateChunkCs;
        const SeRenderRef triangulateChunkPipeline = render::compute_pipeline({ computeProgramInfo });
        const SePassDependencies triangulateChunkDependency = render::begin_pass({ .dependencies = generateChunkDependency, .pipeline = triangulateChunkPipeline });
        executeComputePass(triangulateChunkCs);
        render::end_pass();
        //
        // Draw pass
        //
        const SeRenderRef pipeline = render::graphics_pipeline
        ({
            .vertexProgram          = { .program = renderChunkVs, },
            .fragmentProgram        = { .program = renderChunkFs, },
            .frontStencilOpState    = { .isEnabled = false, },
            .backStencilOpState     = { .isEnabled = false, },
            .depthState             = { .isTestEnabled = true, .isWriteEnabled = true },
            .polygonMode            = SE_PIPELINE_POLYGON_FILL_MODE_FILL,
            .cullMode               = SE_PIPELINE_CULL_MODE_BACK,
            .frontFace              = SE_PIPELINE_FRONT_FACE_COUNTER_CLOCKWISE,
            .samplingType           = SE_SAMPLING_1,
        });
        const SeRenderRef depthTexture = render::texture
        ({
            .width  = win::get_width(),
            .height = win::get_height(),
            .format = SE_TEXTURE_FORMAT_DEPTH_STENCIL,
        });
        render::begin_pass
        ({
            .dependencies       = triangulateChunkDependency,
            .pipeline           = pipeline,
            .renderTargets      = { { render::swap_chain_texture(), SE_PASS_RENDER_TARGET_LOAD_OP_CLEAR } },
            .numRenderTargets   = 1,
            .depthStencilTarget = { depthTexture, SE_PASS_RENDER_TARGET_LOAD_OP_CLEAR },
            .hasDepthStencil    = true,
        });
        const SeRenderRef grassTexture = render::texture
        ({
            .format = SE_TEXTURE_FORMAT_RGBA_8,
            .data   = g_grassTextureData,
        });
        const SeRenderRef rockTexture = render::texture
        ({
            .format = SE_TEXTURE_FORMAT_RGBA_8,
            .data   = g_rockTextureData,
        });
        const SeRenderRef sampler = render::sampler
        ({
            .magFilter          = SE_SAMPLER_FILTER_LINEAR,
            .minFilter          = SE_SAMPLER_FILTER_LINEAR,
            .addressModeU       = SE_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeV       = SE_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeW       = SE_SAMPLER_ADDRESS_MODE_REPEAT,
            .mipmapMode         = SE_SAMPLER_MIPMAP_MODE_LINEAR,
            .mipLodBias         = 0.0f,
            .minLod             = 0.0f,
            .maxLod             = 0.0f,
            .anisotropyEnable   = false,
            .maxAnisotropy      = 0.0f,
            .compareEnabled     = false,
            .compareOp          = SE_COMPARE_OP_ALWAYS,
        });
        {
            render::bind({ .set = 0, .bindings = { { 0, frameDataBuffer } } });
            render::bind({ .set = 1, .bindings = { { 0, g_geometryBuffer } } });
            render::bind({ .set = 2, .bindings = { { 0, grassTexture, sampler }, { 1, rockTexture, sampler } } });
            render::draw({ .numVertices = numVerts, .numInstances = 1 });
        }
        render::end_pass();
        render::end_frame();
    }
}
