
#include "tetris_render.hpp"
#include "tetris_controller.hpp"
#include "engine/se_engine.hpp"

struct FrameData
{
    SeFloat4x4 viewProjection;
};

struct InputVertex
{
    SeFloat4 positionLS;
    SeFloat4 uv;
};

struct InputIndex
{
    uint32_t index;
    float pad[3];
};

struct InputInstance
{
    SeFloat4x4 trfWS;
    SeFloat4 tint;
};

const InputVertex g_cubeVertices[] =
{
    // Bottom side
    { .positionLS = { -0.5f , -0.5f ,  0.5f }, .uv = { 0, 0 }, },
    { .positionLS = { -0.5f , -0.5f , -0.5f }, .uv = { 0, 1 }, },
    { .positionLS = {  0.5f , -0.5f , -0.5f }, .uv = { 1, 1 }, },
    { .positionLS = {  0.5f , -0.5f ,  0.5f }, .uv = { 1, 0 }, },
    // Top side
    { .positionLS = { -0.5f ,  0.5f , -0.5f }, .uv = { 0, 0 }, },
    { .positionLS = { -0.5f ,  0.5f ,  0.5f }, .uv = { 0, 1 }, },
    { .positionLS = {  0.5f ,  0.5f ,  0.5f }, .uv = { 1, 1 }, },
    { .positionLS = {  0.5f ,  0.5f , -0.5f }, .uv = { 1, 0 }, },
    // Close side
    { .positionLS = { -0.5f , -0.5f , -0.5f }, .uv = { 0, 0 }, },
    { .positionLS = { -0.5f ,  0.5f , -0.5f }, .uv = { 0, 1 }, },
    { .positionLS = {  0.5f ,  0.5f , -0.5f }, .uv = { 1, 1 }, },
    { .positionLS = {  0.5f , -0.5f , -0.5f }, .uv = { 1, 0 }, },
    // Far side
    { .positionLS = {  0.5f , -0.5f ,  0.5f }, .uv = { 0, 0 }, },
    { .positionLS = {  0.5f ,  0.5f ,  0.5f }, .uv = { 0, 1 }, },
    { .positionLS = { -0.5f ,  0.5f ,  0.5f }, .uv = { 1, 1 }, },
    { .positionLS = { -0.5f , -0.5f ,  0.5f }, .uv = { 1, 0 }, },
    // Left side
    { .positionLS = { -0.5f , -0.5f ,  0.5f }, .uv = { 0, 0 }, },
    { .positionLS = { -0.5f ,  0.5f ,  0.5f }, .uv = { 0, 1 }, },
    { .positionLS = { -0.5f ,  0.5f , -0.5f }, .uv = { 1, 1 }, },
    { .positionLS = { -0.5f , -0.5f , -0.5f }, .uv = { 1, 0 }, },
    // Right side
    { .positionLS = {  0.5f , -0.5f , -0.5f }, .uv = { 0, 0 }, },
    { .positionLS = {  0.5f ,  0.5f , -0.5f }, .uv = { 0, 1 }, },
    { .positionLS = {  0.5f ,  0.5f ,  0.5f }, .uv = { 1, 1 }, },
    { .positionLS = {  0.5f , -0.5f ,  0.5f }, .uv = { 1, 0 }, },
};

const InputIndex g_cubeIndices[] =
{
    { .index = 0      }, { .index = 1      }, { .index = 2      }, { .index = 0      }, { .index = 2      }, { .index = 3      }, // Bottom side
    { .index = 0 + 4  }, { .index = 1 + 4  }, { .index = 2 + 4  }, { .index = 0 + 4  }, { .index = 2 + 4  }, { .index = 3 + 4  }, // Top side
    { .index = 0 + 8  }, { .index = 1 + 8  }, { .index = 2 + 8  }, { .index = 0 + 8  }, { .index = 2 + 8  }, { .index = 3 + 8  }, // Close side
    { .index = 0 + 12 }, { .index = 1 + 12 }, { .index = 2 + 12 }, { .index = 0 + 12 }, { .index = 2 + 12 }, { .index = 3 + 12 }, // Far side
    { .index = 0 + 16 }, { .index = 1 + 16 }, { .index = 2 + 16 }, { .index = 0 + 16 }, { .index = 2 + 16 }, { .index = 3 + 16 }, // Left side
    { .index = 0 + 20 }, { .index = 1 + 20 }, { .index = 2 + 20 }, { .index = 0 + 20 }, { .index = 2 + 20 }, { .index = 3 + 20 }, // Right side
};

const InputVertex g_gridVertices[] =
{
    // Bottom line
    { .positionLS = { -0.5f , -0.5f , 0.5f }, .uv = { 0, 0 }, },
    { .positionLS = { -0.45f, -0.45f, 0.5f }, .uv = { 0, 1 }, },
    { .positionLS = {  0.45f, -0.45f, 0.5f }, .uv = { 1, 1 }, },
    { .positionLS = {  0.5f , -0.5f , 0.5f }, .uv = { 1, 0 }, },
    // Top line
    { .positionLS = { -0.45f,  0.45f, 0.5f }, .uv = { 0, 0 }, },
    { .positionLS = { -0.5f ,  0.5f , 0.5f }, .uv = { 0, 1 }, },
    { .positionLS = {  0.5f ,  0.5f , 0.5f }, .uv = { 1, 1 }, },
    { .positionLS = {  0.45f,  0.45f, 0.5f }, .uv = { 1, 0 }, },
    // Left line
    { .positionLS = { -0.5f , -0.5f , 0.5f }, .uv = { 0, 0 }, },
    { .positionLS = { -0.5f ,  0.5f , 0.5f }, .uv = { 0, 1 }, },
    { .positionLS = { -0.45f,  0.45f, 0.5f }, .uv = { 1, 1 }, },
    { .positionLS = { -0.45f, -0.45f, 0.5f }, .uv = { 1, 0 }, },
    // Right line
    { .positionLS = {  0.45f, -0.45f, 0.5f }, .uv = { 0, 0 }, },
    { .positionLS = {  0.45f,  0.45f, 0.5f }, .uv = { 0, 1 }, },
    { .positionLS = {  0.5f ,  0.5f , 0.5f }, .uv = { 1, 1 }, },
    { .positionLS = {  0.5f , -0.5f , 0.5f }, .uv = { 1, 0 }, },
};

const InputIndex g_gridIndices[] =
{
    { .index = 0      }, { .index = 1      }, { .index = 2      }, { .index = 0      }, { .index = 2      }, { .index = 3      }, // Bottom line
    { .index = 0 + 4  }, { .index = 1 + 4  }, { .index = 2 + 4  }, { .index = 0 + 4  }, { .index = 2 + 4  }, { .index = 3 + 4  }, // Top line
    { .index = 0 + 8  }, { .index = 1 + 8  }, { .index = 2 + 8  }, { .index = 0 + 8  }, { .index = 2 + 8  }, { .index = 3 + 8  }, // Left line
    { .index = 0 + 12 }, { .index = 1 + 12 }, { .index = 2 + 12 }, { .index = 0 + 12 }, { .index = 2 + 12 }, { .index = 3 + 12 }, // Right line
};

extern TetrisState g_state;

SeProgramRef    g_drawVs;
SeProgramRef    g_drawFs;
SeProgramRef    g_presentVs;
SeProgramRef    g_presentFs;

SeTextureRef    g_colorTexture;
SeTextureRef    g_depthTexture;

SeSamplerRef    g_sampler;

SeBufferRef     g_gridVerticesBuffer;
SeBufferRef     g_gridIndicesBuffer;
SeBufferRef     g_cubeVerticesBuffer;
SeBufferRef     g_cubeIndicesBuffer;

SeDataProvider    g_fontDataEnglish;

void tetris_render_init()
{
    g_drawVs    = se_render_program({ se_data_provider_from_file("tetris_draw.vert.spv") });
    g_drawFs    = se_render_program({ se_data_provider_from_file("tetris_draw.frag.spv") });
    g_presentVs = se_render_program({ se_data_provider_from_file("present.vert.spv") });
    g_presentFs = se_render_program({ se_data_provider_from_file("present.frag.spv") });

    g_colorTexture = se_render_texture
    ({
        .format = SeTextureFormat::RGBA_8_SRGB,
        .width  = se_win_get_width(),
        .height = se_win_get_height(),
    });
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
        .addressModeU       = SeSamplerAddressMode::CLAMP_TO_EDGE,
        .addressModeV       = SeSamplerAddressMode::CLAMP_TO_EDGE,
        .addressModeW       = SeSamplerAddressMode::CLAMP_TO_EDGE,
        .mipmapMode         = SeSamplerMipmapMode::LINEAR,
        .mipLodBias         = 0.0f,
        .minLod             = 0.0f,
        .maxLod             = 0.0f,
        .anisotropyEnable   = false,
        .maxAnisotropy      = 0.0f,
        .compareEnabled     = false,
        .compareOp          = SeCompareOp::ALWAYS,
    });

    g_gridVerticesBuffer    = se_render_memory_buffer({ se_data_provider_from_memory(g_gridVertices) });
    g_gridIndicesBuffer     = se_render_memory_buffer({ se_data_provider_from_memory(g_gridIndices) });
    g_cubeVerticesBuffer    = se_render_memory_buffer({ se_data_provider_from_memory(g_cubeVertices) });
    g_cubeIndicesBuffer     = se_render_memory_buffer({ se_data_provider_from_memory(g_cubeIndices) });

    g_fontDataEnglish = se_data_provider_from_file("shahd serif.ttf");
}

void tetris_render_terminate()
{
    
}

void tetris_render_update(float dt)
{
    InputInstance cubeInstances[TETRIS_FIELD_WIDTH * TETRIS_FIELD_HEIGHT] = { };
    uint32_t numCubeInstances = 0;
    for (int y = 0; y < TETRIS_FIELD_HEIGHT; y++)
        for (int x = 0; x < TETRIS_FIELD_WIDTH; x++)
            if (g_state.field[x][y])
            {
                const SeFloat4x4 trf = se_float4x4_from_position({ float(x), float(y), 0 });
                cubeInstances[numCubeInstances++] =
                {
                    .trfWS = se_float4x4_transposed(trf),
                    .tint = g_state.field[x][y] == TETRIS_FILLED_ACTIVE_FIGURE_SQUARE ? SeFloat4{ 0, 1, 1, 1 } : SeFloat4{ 1, 1, 1, 1 },
                };
            }
    InputInstance gridInstances[TETRIS_FIELD_WIDTH * TETRIS_FIELD_HEIGHT];
    for (int y = 0; y < TETRIS_FIELD_HEIGHT; y++)
        for (int x = 0; x < TETRIS_FIELD_WIDTH; x++)
        {
            const SeFloat4x4 trf = se_float4x4_from_position({ float(x), float(y), 0 });
            gridInstances[x + y * TETRIS_FIELD_WIDTH] = 
            {
                .trfWS = se_float4x4_transposed(trf),
                .tint = { 0, 1, 1, 1 },
            };
        }
    //
    // Frame data
    //
    const SeFloat4x4 view = se_float4x4_mul
    (
        se_float4x4_from_position({ (float)TETRIS_FIELD_WIDTH / 2.0f, -3, -20 }),
        se_float4x4_from_rotation({ -30, 0, 0 })
    );
    const float aspect = se_win_get_width<float>() / se_win_get_height<float>();
    const SeFloat4x4 perspective = se_render_perspective(60, aspect, 0.1f, 100.0f);
    const FrameData frameData = { .viewProjection = se_float4x4_transposed(perspective * se_float4x4_inverted(view)) };
    //
    // Render
    //
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

            se_render_destroy(g_colorTexture);
            g_colorTexture = se_render_texture
            ({
                .format = SeTextureFormat::RGBA_8_SRGB,
                .width  = uint32_t(swapChainSize.width),
                .height = uint32_t(swapChainSize.height),
            });
        }

        //
        // Offscreen pass
        //
        const SePassDependencies drawDependency = se_render_begin_graphics_pass
        ({
            .dependencies           = 0,
            .vertexProgram          = { .program = g_drawVs, },
            .fragmentProgram        = { .program = g_drawFs, },
            .frontStencilOpState    = { .isEnabled = false, },
            .backStencilOpState     = { .isEnabled = false, },
            .depthState             = { .isTestEnabled = true, .isWriteEnabled = true, },
            .polygonMode            = SePipelinePolygonMode::FILL,
            .cullMode               = SePipelineCullMode::BACK,
            .frontFace              = SePipelineFrontFace::CLOCKWISE,
            .samplingType           = SeSamplingType::_1,
            .renderTargets          = { { g_colorTexture, SeRenderTargetLoadOp::CLEAR } },
            .depthStencilTarget     = { g_depthTexture, SeRenderTargetLoadOp::CLEAR },
        });
        {
            const SeBufferRef frameDataBuffer = se_render_scratch_memory_buffer({ se_data_provider_from_memory(frameData) });
            se_render_bind({ .set = 0, .bindings =
            {
                { .binding = 0, .type = SeBinding::BUFFER, .buffer = { frameDataBuffer } }
            } });
            if (numCubeInstances)
            {
                const SeBufferRef cubeInstancesBuffer = se_render_scratch_memory_buffer({ se_data_provider_from_memory(cubeInstances) });
                se_render_bind({ .set = 1, .bindings =
                { 
                    { .binding = 0, .type = SeBinding::BUFFER, .buffer = { g_cubeVerticesBuffer  } },
                    { .binding = 1, .type = SeBinding::BUFFER, .buffer = { g_cubeIndicesBuffer   } },
                    { .binding = 2, .type = SeBinding::BUFFER, .buffer = { cubeInstancesBuffer   } }, 
                } });
                se_render_draw({ .numVertices = se_array_size(g_cubeIndices), .numInstances = numCubeInstances });
            }
            const SeBufferRef gridInstancesBuffer = se_render_scratch_memory_buffer({ se_data_provider_from_memory(gridInstances) });
            se_render_bind({ .set = 1, .bindings =
            { 
                { .binding = 0, .type = SeBinding::BUFFER, .buffer = { g_gridVerticesBuffer  } },
                { .binding = 1, .type = SeBinding::BUFFER, .buffer = { g_gridIndicesBuffer   } },
                { .binding = 2, .type = SeBinding::BUFFER, .buffer = { gridInstancesBuffer } }, 
            } });
            se_render_draw({ .numVertices = se_array_size(g_gridIndices), .numInstances = se_array_size(gridInstances) });
        }
        se_render_end_pass();
        //
        // Ui pass
        //
        SePassDependencies uiDependency = 0;
        if (se_ui_begin({ g_colorTexture, SeRenderTargetLoadOp::LOAD }))
        {
            se_ui_set_font_group({ g_fontDataEnglish });
            se_ui_set_param(SeUiParam::FONT_HEIGHT, { .dim = 30.0f });
            se_ui_set_param(SeUiParam::PIVOT_TYPE_X, { .enumeration = SeUiPivotType::CENTER });
            se_ui_set_param(SeUiParam::PIVOT_POSITION_X, { .dim = se_win_get_width<float>() * 0.5f });
            se_ui_set_param(SeUiParam::PIVOT_POSITION_Y, { .dim = se_win_get_height<float>() - 40.0f });
            se_ui_text({ "Epic tetris game" });
            se_ui_set_param(SeUiParam::PIVOT_POSITION_Y, { .dim = se_win_get_height<float>() - 80.0f });
            se_ui_text({ se_string_cstr(se_string_create_fmt(SeStringLifetime::TEMPORARY, "Points : {}", g_state.points)) });
            uiDependency = se_ui_end(drawDependency);
        }
        //
        // Presentation pass
        //
        se_render_begin_graphics_pass
        ({
            .dependencies           = drawDependency | uiDependency,
            .vertexProgram          = { .program = g_presentVs, },
            .fragmentProgram        = { .program = g_presentFs, },
            .frontStencilOpState    = { .isEnabled = false, },
            .backStencilOpState     = { .isEnabled = false, },
            .depthState             = { .isTestEnabled = false, .isWriteEnabled = false, },
            .polygonMode            = SePipelinePolygonMode::FILL,
            .cullMode               = SePipelineCullMode::NONE,
            .frontFace              = SePipelineFrontFace::CLOCKWISE,
            .samplingType           = SeSamplingType::_1,
            .renderTargets          = { { se_render_swap_chain_texture(), SeRenderTargetLoadOp::CLEAR } },
        });
        {
            se_render_bind({ .set = 0, .bindings =
            {
                { .binding = 0, .type = SeBinding::TEXTURE, .texture = { g_colorTexture, g_sampler } }
            } });
            se_render_draw({ .numVertices = 4, .numInstances = 1 });
        }
        se_render_end_pass();
        se_render_end_frame();
    }
}
