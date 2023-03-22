
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
    g_clearChunkCs          = se_render_program({ se_data_provider_from_file("clear_chunk.comp.spv") });
    g_generateChunkCs       = se_render_program({ se_data_provider_from_file("generate_chunk.comp.spv") });
    g_triangulateChunkCs    = se_render_program({ se_data_provider_from_file("triangulate_chunk.comp.spv") });
    g_renderChunkVs         = se_render_program({ se_data_provider_from_file("render_chunk.vert.spv") });
    g_renderChunkFs         = se_render_program({ se_data_provider_from_file("render_chunk.frag.spv") });

    g_grassTexture = se_render_texture
    ({
        .format = SeTextureFormat::RGBA_8_SRGB,
        .data   = se_data_provider_from_file("grass.png"),
    });
    g_rockTexture = se_render_texture
    ({
        .format = SeTextureFormat::RGBA_8_SRGB,
        .data   = se_data_provider_from_file("rocks.png"),
    });
    g_depthTexture = se_render_texture
    ({
        .format = SeTextureFormat::DEPTH_STENCIL,
        .width  = se_win_get_width<uint32_t>(),
        .height = se_win_get_height<uint32_t>(),
    });
    
    g_edgeTableBuffer       = se_render_memory_buffer({ se_data_provider_from_memory(EDGE_TABLE, sizeof(EDGE_TABLE)) });
    g_triangleTableBuffer   = se_render_memory_buffer({ se_data_provider_from_memory(TRIANGLE_TABLE, sizeof(TRIANGLE_TABLE)) });
    g_gridValuesBuffer      = se_render_memory_buffer({ se_data_provider_from_memory(nullptr, sizeof(float) * CHUNK_DIM * CHUNK_DIM * CHUNK_DIM) });
    g_geometryBuffer        = se_render_memory_buffer({ se_data_provider_from_memory(nullptr, sizeof(Vertex) * NUM_VERTS) });

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
    const SePassDependencies resultDeps = se_render_begin_compute_pass({ .dependencies = deps, .program = computeProgramInfo });
    se_render_bind
    ({
        .set = 0,
        .bindings =
        {
            { .binding = 0, .type = SeBinding::BUFFER, .buffer = { frameDataBuffer } }
        } 
    });
    se_render_bind
    ({
        .set = 1,
        .bindings =
        {
            { .binding = 0, .type = SeBinding::BUFFER, .buffer = { g_gridValuesBuffer } },
            { .binding = 1, .type = SeBinding::BUFFER, .buffer = { g_geometryBuffer } },
            { .binding = 2, .type = SeBinding::BUFFER, .buffer = { g_edgeTableBuffer } },
            { .binding = 3, .type = SeBinding::BUFFER, .buffer = { g_triangleTableBuffer } }
        }
    });
    const SeComputeWorkgroupSize workgroupSize = se_render_workgroup_size(program);
    se_render_dispatch
    ({
        1 + ((CHUNK_DIM - 1) / workgroupSize.x),
        1 + ((CHUNK_DIM - 1) / workgroupSize.y),
        1 + ((CHUNK_DIM - 1) / workgroupSize.z),   
    });
    se_render_end_pass();
    return resultDeps;
};

void update(const SeUpdateInfo& updateInfo)
{
    if (se_win_is_close_button_pressed() || se_win_is_keyboard_button_pressed(SeKeyboard::ESCAPE)) se_engine_stop();

    //
    // Fill frame data
    //
    static float time = 0.0f;
    static float isoLevel = 0.5f;
    time += updateInfo.dt;
    isoLevel += float(se_win_get_mouse_wheel()) * 0.01f;
    const SeFloat4x4 projection = se_render_perspective
    (
        90,
        se_win_get_width<float>() / se_win_get_height<float>(),
        0.1f,
        200.0f
    );
    debug_camera_update(&g_camera, updateInfo.dt);
    const FrameData frameData
    {
        .viewProjection = se_float4x4_transposed(projection * se_float4x4_inverted(g_camera.trf)),
        .time           = time,
        .isoLevel       = isoLevel,
    };

    if (se_render_begin_frame())
    {
        //
        // Recreate depth texture on resize
        //
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
        const SeBufferRef frameDataBuffer = se_render_scratch_memory_buffer({ se_data_provider_from_memory(frameData) });
        const SePassDependencies clearChunkDependency = execute_compute(frameDataBuffer, g_clearChunkCs, 0);
        const SePassDependencies generateChunkDependency = execute_compute(frameDataBuffer, g_generateChunkCs, clearChunkDependency);
        const SePassDependencies triangulateChunkDependency = execute_compute(frameDataBuffer, g_triangulateChunkCs, generateChunkDependency);
        se_render_begin_graphics_pass
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
            .renderTargets          = { { se_render_swap_chain_texture(), SeRenderTargetLoadOp::CLEAR } },
            .depthStencilTarget     = { g_depthTexture, SeRenderTargetLoadOp::CLEAR },
        });
        {
            se_render_bind
            ({
                .set = 0,
                .bindings =
                {
                    { .binding = 0, .type = SeBinding::BUFFER, .buffer = { frameDataBuffer } }
                }
            });
            se_render_bind
            ({
                .set = 1,
                .bindings =
                {
                    { .binding = 0, .type = SeBinding::BUFFER, .buffer = { g_geometryBuffer } }
                }
            });
            se_render_bind
            ({
                .set = 2,
                .bindings =
                {
                    { .binding = 0, .type = SeBinding::TEXTURE, .texture = { g_grassTexture, g_sampler } },
                    { .binding = 1, .type = SeBinding::TEXTURE, .texture = { g_rockTexture, g_sampler } }
                }
            });
            se_render_draw({ .numVertices = NUM_VERTS, .numInstances = 1 });
        }
        se_render_end_pass();
        se_render_end_frame();
    }
}
