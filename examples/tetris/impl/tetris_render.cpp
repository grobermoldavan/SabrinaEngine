
#include "tetris_render.hpp"
#include "tetris_controller.hpp"
#include "engine/engine.hpp"

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

DataProvider    g_fontDataEnglish;

void tetris_render_init()
{
    g_drawVs    = render::program({ data_provider::from_file("tetris_draw.vert.spv") });
    g_drawFs    = render::program({ data_provider::from_file("tetris_draw.frag.spv") });
    g_presentVs = render::program({ data_provider::from_file("present.vert.spv") });
    g_presentFs = render::program({ data_provider::from_file("present.frag.spv") });

    g_colorTexture = render::texture
    ({
        .format = SeTextureFormat::RGBA_8_SRGB,
        .width  = win::get_width(),
        .height = win::get_height(),
    });
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

    g_gridVerticesBuffer    = render::memory_buffer({ data_provider::from_memory(g_gridVertices) });
    g_gridIndicesBuffer     = render::memory_buffer({ data_provider::from_memory(g_gridIndices) });
    g_cubeVerticesBuffer    = render::memory_buffer({ data_provider::from_memory(g_cubeVertices) });
    g_cubeIndicesBuffer     = render::memory_buffer({ data_provider::from_memory(g_cubeIndices) });

    g_fontDataEnglish = data_provider::from_file("shahd serif.ttf");
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
                const SeFloat4x4 trf = float4x4::from_position({ float(x), float(y), 0 });
                cubeInstances[numCubeInstances++] =
                {
                    .trfWS = float4x4::transposed(trf),
                    .tint = g_state.field[x][y] == TETRIS_FILLED_ACTIVE_FIGURE_SQUARE ? SeFloat4{ 0, 1, 1, 1 } : SeFloat4{ 1, 1, 1, 1 },
                };
            }
    InputInstance gridInstances[TETRIS_FIELD_WIDTH * TETRIS_FIELD_HEIGHT];
    for (int y = 0; y < TETRIS_FIELD_HEIGHT; y++)
        for (int x = 0; x < TETRIS_FIELD_WIDTH; x++)
        {
            const SeFloat4x4 trf = float4x4::from_position({ float(x), float(y), 0 });
            gridInstances[x + y * TETRIS_FIELD_WIDTH] = 
            {
                .trfWS = float4x4::transposed(trf),
                .tint = { 0, 1, 1, 1 },
            };
        }
    //
    // Frame data
    //
    const SeFloat4x4 view = float4x4::mul
    (
        float4x4::from_position({ (float)TETRIS_FIELD_WIDTH / 2.0f, -3, -20 }),
        float4x4::from_rotation({ -30, 0, 0 })
    );
    const float aspect = win::get_width<float>() / win::get_height<float>();
    const SeFloat4x4 perspective = render::perspective(60, aspect, 0.1f, 100.0f);
    const FrameData frameData = { .viewProjection = float4x4::transposed(perspective * float4x4::inverted(view)) };
    //
    // Render
    //
    if (render::begin_frame())
    {
        //
        // Offscreen pass
        //
        const SePassDependencies drawDependency = render::begin_graphics_pass
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
            const SeBufferRef frameDataBuffer = render::scratch_memory_buffer({ data_provider::from_memory(frameData) });
            render::bind({ .set = 0, .bindings =
            {
                { .binding = 0, .type = SeBinding::BUFFER, .buffer = { frameDataBuffer } }
            } });
            if (numCubeInstances)
            {
                const SeBufferRef cubeInstancesBuffer = render::scratch_memory_buffer({ data_provider::from_memory(cubeInstances) });
                render::bind({ .set = 1, .bindings =
                { 
                    { .binding = 0, .type = SeBinding::BUFFER, .buffer = { g_cubeVerticesBuffer  } },
                    { .binding = 1, .type = SeBinding::BUFFER, .buffer = { g_cubeIndicesBuffer   } },
                    { .binding = 2, .type = SeBinding::BUFFER, .buffer = { cubeInstancesBuffer   } }, 
                } });
                render::draw({ .numVertices = se_array_size(g_cubeIndices), .numInstances = numCubeInstances });
            }
            const SeBufferRef gridInstancesBuffer = render::scratch_memory_buffer({ data_provider::from_memory(gridInstances) });
            render::bind({ .set = 1, .bindings =
            { 
                { .binding = 0, .type = SeBinding::BUFFER, .buffer = { g_gridVerticesBuffer  } },
                { .binding = 1, .type = SeBinding::BUFFER, .buffer = { g_gridIndicesBuffer   } },
                { .binding = 2, .type = SeBinding::BUFFER, .buffer = { gridInstancesBuffer } }, 
            } });
            render::draw({ .numVertices = se_array_size(g_gridIndices), .numInstances = se_array_size(gridInstances) });
        }
        render::end_pass();
        //
        // Ui pass
        //
        SePassDependencies uiDependency = 0;
        if (ui::begin({ g_colorTexture, SeRenderTargetLoadOp::LOAD }))
        {
            ui::set_font_group({ g_fontDataEnglish });
            ui::set_param(SeUiParam::FONT_HEIGHT, { .dim = 30.0f });
            ui::set_param(SeUiParam::PIVOT_TYPE_X, { .pivot = SeUiPivotType::CENTER });
            ui::set_param(SeUiParam::PIVOT_POSITION_X, { .dim = win::get_width<float>() * 0.5f });
            ui::set_param(SeUiParam::PIVOT_POSITION_Y, { .dim = win::get_height<float>() - 40.0f });
            ui::text({ "Epic tetris game" });
            ui::set_param(SeUiParam::PIVOT_POSITION_Y, { .dim = win::get_height<float>() - 80.0f });
            ui::text({ string::cstr(string::create_fmt(SeStringLifetime::TEMPORARY, "Points : {}", g_state.points)) });
            uiDependency = ui::end(drawDependency);
        }
        //
        // Presentation pass
        //
        render::begin_graphics_pass
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
            .renderTargets          = { { render::swap_chain_texture(), SeRenderTargetLoadOp::CLEAR } },
        });
        {
            render::bind({ .set = 0, .bindings =
            {
                { .binding = 0, .type = SeBinding::TEXTURE, .texture = { g_colorTexture, g_sampler } }
            } });
            render::draw({ .numVertices = 4, .numInstances = 1 });
        }
        render::end_pass();
        render::end_frame();
    }
}
