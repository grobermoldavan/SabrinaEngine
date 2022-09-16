
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

extern TetrisState g_state;

DataProvider    g_drawVsData;
DataProvider    g_drawFsData;
DataProvider    g_presentVsData;
DataProvider    g_presentFsData;
DataProvider    g_fontDataEnglish;

void tetris_render_init()
{
    g_drawVsData    = data_provider::from_file("assets/application/shaders/tetris_draw.vert.spv");
    g_drawFsData    = data_provider::from_file("assets/application/shaders/tetris_draw.frag.spv");
    g_presentVsData = data_provider::from_file("assets/default/shaders/present.vert.spv");
    g_presentFsData = data_provider::from_file("assets/default/shaders/present.frag.spv");
    g_fontDataEnglish = data_provider::from_file("assets/default/fonts/shahd serif.ttf");
}

void tetris_render_terminate()
{
    data_provider::destroy(g_drawVsData);
    data_provider::destroy(g_drawFsData);
    data_provider::destroy(g_presentVsData);
    data_provider::destroy(g_presentFsData);
    data_provider::destroy(g_fontDataEnglish);
}

void tetris_render_update(float dt)
{
    //
    // Cube data
    //
    const static InputVertex cubeVertices[] =
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
    const static InputIndex cubeIndices[] =
    {
        { .index = 0      }, { .index = 1      }, { .index = 2      }, { .index = 0      }, { .index = 2      }, { .index = 3      }, // Bottom side
        { .index = 0 + 4  }, { .index = 1 + 4  }, { .index = 2 + 4  }, { .index = 0 + 4  }, { .index = 2 + 4  }, { .index = 3 + 4  }, // Top side
        { .index = 0 + 8  }, { .index = 1 + 8  }, { .index = 2 + 8  }, { .index = 0 + 8  }, { .index = 2 + 8  }, { .index = 3 + 8  }, // Close side
        { .index = 0 + 12 }, { .index = 1 + 12 }, { .index = 2 + 12 }, { .index = 0 + 12 }, { .index = 2 + 12 }, { .index = 3 + 12 }, // Far side
        { .index = 0 + 16 }, { .index = 1 + 16 }, { .index = 2 + 16 }, { .index = 0 + 16 }, { .index = 2 + 16 }, { .index = 3 + 16 }, // Left side
        { .index = 0 + 20 }, { .index = 1 + 20 }, { .index = 2 + 20 }, { .index = 0 + 20 }, { .index = 2 + 20 }, { .index = 3 + 20 }, // Right side
    };
    InputInstance cubeInstances[TETRIS_FIELD_WIDTH * TETRIS_FIELD_HEIGHT] = { };
    uint32_t numCubeInstances = 0;
    for (int y = 0; y < TETRIS_FIELD_HEIGHT; y++)
        for (int x = 0; x < TETRIS_FIELD_WIDTH; x++)
            if (g_state.field[x][y])
            {
                SeFloat4x4 trf = float4x4::from_position({ (float)x, (float)y, 0 });
                cubeInstances[numCubeInstances++] = InputInstance
                {
                    .trfWS = float4x4::transposed(trf),
                    .tint = g_state.field[x][y] == TETRIS_FILLED_ACTIVE_FIGURE_SQUARE ? SeFloat4{ 0, 1, 1, 1 } : SeFloat4{ 1, 1, 1, 1 },
                };
            }
    //
    // Grid data
    //
    const static InputVertex gridVertices[] =
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
    const static InputIndex gridIndices[] =
    {
        { .index = 0      }, { .index = 1      }, { .index = 2      }, { .index = 0      }, { .index = 2      }, { .index = 3      }, // Bottom line
        { .index = 0 + 4  }, { .index = 1 + 4  }, { .index = 2 + 4  }, { .index = 0 + 4  }, { .index = 2 + 4  }, { .index = 3 + 4  }, // Top line
        { .index = 0 + 8  }, { .index = 1 + 8  }, { .index = 2 + 8  }, { .index = 0 + 8  }, { .index = 2 + 8  }, { .index = 3 + 8  }, // Left line
        { .index = 0 + 12 }, { .index = 1 + 12 }, { .index = 2 + 12 }, { .index = 0 + 12 }, { .index = 2 + 12 }, { .index = 3 + 12 }, // Right line
    };
    InputInstance gridInstances[TETRIS_FIELD_WIDTH * TETRIS_FIELD_HEIGHT];
    for (int y = 0; y < TETRIS_FIELD_HEIGHT; y++)
        for (int x = 0; x < TETRIS_FIELD_WIDTH; x++)
        {
            const SeFloat4x4 trf = float4x4::from_position({ (float)x, (float)y, 0 });
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
    const float aspect = ((float)win::get_width()) / ((float)win::get_height());
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
        const SeRenderRef drawVs = render::program({ g_drawVsData });
        const SeRenderRef drawFs = render::program({ g_drawFsData });
        const SeRenderRef drawPipeline = render::graphics_pipeline
        ({
            .vertexProgram          = { .program = drawVs, },
            .fragmentProgram        = { .program = drawFs, },
            .frontStencilOpState    = { .isEnabled = false, },
            .backStencilOpState     = { .isEnabled = false, },
            .depthState             = { .isTestEnabled = true, .isWriteEnabled = true, },
            .polygonMode            = SE_PIPELINE_POLYGON_FILL_MODE_FILL,
            .cullMode               = SE_PIPELINE_CULL_MODE_BACK,
            .frontFace              = SE_PIPELINE_FRONT_FACE_CLOCKWISE,
            .samplingType           = SE_SAMPLING_1,
        });
        const SeRenderRef colorTexture = render::texture
        ({
            .width  = win::get_width(),
            .height = win::get_height(),
            .format = SE_TEXTURE_FORMAT_RGBA_8,
            .data   = { },
        });
        const SeRenderRef depthTexture = render::texture
        ({
            .width  = win::get_width(),
            .height = win::get_height(),
            .format = SE_TEXTURE_FORMAT_DEPTH_STENCIL,
            .data   = { },
        });
        const SePassDependencies drawDependency = render::begin_pass
        ({
            .dependencies       = 0,
            .pipeline           = drawPipeline,
            .renderTargets      = { { colorTexture, SE_PASS_RENDER_TARGET_LOAD_OP_CLEAR } },
            .numRenderTargets   = 1,
            .depthStencilTarget = { depthTexture, SE_PASS_RENDER_TARGET_LOAD_OP_CLEAR },
            .hasDepthStencil    = true,
        });
        {
            const SeRenderRef gridVerticesBuffer    = render::memory_buffer({ data_provider::from_memory(gridVertices, sizeof(gridVertices)) });
            const SeRenderRef gridIndicesBuffer     = render::memory_buffer({ data_provider::from_memory(gridIndices, sizeof(gridIndices)) });
            const SeRenderRef gridInstancesBuffer   = render::memory_buffer({ data_provider::from_memory(gridInstances, sizeof(gridInstances)) });
            const SeRenderRef frameDataBuffer       = render::memory_buffer({ data_provider::from_memory(&frameData, sizeof(frameData)) });
            render::bind({ .set = 0, .bindings = { { 0, frameDataBuffer } } });
            if (numCubeInstances)
            {
                const SeRenderRef cubeVerticesBuffer    = render::memory_buffer({ data_provider::from_memory(cubeVertices, sizeof(cubeVertices)) });
                const SeRenderRef cubeIndicesBuffer     = render::memory_buffer({ data_provider::from_memory(cubeIndices, sizeof(cubeIndices)) });
                const SeRenderRef cubeInstancesBuffer   = render::memory_buffer({ data_provider::from_memory(cubeInstances, numCubeInstances * sizeof(InputInstance)) });
                render::bind({ .set = 1, .bindings =
                { 
                    { 0, cubeVerticesBuffer  },
                    { 1, cubeIndicesBuffer   },
                    { 2, cubeInstancesBuffer }, 
                } });
                render::draw({ .numVertices = se_array_size(cubeIndices), .numInstances = numCubeInstances });
            }
            render::bind({ .set = 1, .bindings =
            { 
                { 0, gridVerticesBuffer  },
                { 1, gridIndicesBuffer   },
                { 2, gridInstancesBuffer }, 
            } });
            render::draw({ .numVertices = se_array_size(gridIndices), .numInstances = se_array_size(gridInstances) });
        }
        render::end_pass();
        //
        // Ui pass
        //
        SePassDependencies uiDependency = 0;
        if (ui::begin({ colorTexture, SE_PASS_RENDER_TARGET_LOAD_OP_LOAD }))
        {
            ui::set_font_group({ g_fontDataEnglish });
            ui::set_param(SeUiParam::FONT_HEIGHT, { .dim = 30.0f });
            ui::set_param(SeUiParam::PIVOT_TYPE_X, { .pivot = SeUiPivotType::CENTER });
            ui::set_param(SeUiParam::PIVOT_POSITION_X, { .dim = win::get_width<float>() * 0.5f });
            ui::set_param(SeUiParam::PIVOT_POSITION_Y, { .dim = win::get_height<float>() - 40.0f });
            ui::text({ "Epic tetris game" });
            ui::set_param(SeUiParam::PIVOT_POSITION_Y, { .dim = win::get_height<float>() - 80.0f });
            ui::text({ string::cstr(string::create_fmt(SeStringLifetime::Temporary, "Points : {}", g_state.points)) });
            uiDependency = ui::end(drawDependency);
        }
        //
        // Presentation pass
        //
        const SeRenderRef presentVs = render::program({ g_presentVsData });
        const SeRenderRef presentFs = render::program({ g_presentFsData });
        const SeRenderRef presentPipeline = render::graphics_pipeline
        ({
            .vertexProgram          = { .program = presentVs, },
            .fragmentProgram        = { .program = presentFs, },
            .frontStencilOpState    = { .isEnabled = false, },
            .backStencilOpState     = { .isEnabled = false, },
            .depthState             = { .isTestEnabled = false, .isWriteEnabled = false, },
            .polygonMode            = SE_PIPELINE_POLYGON_FILL_MODE_FILL,
            .cullMode               = SE_PIPELINE_CULL_MODE_NONE,
            .frontFace              = SE_PIPELINE_FRONT_FACE_CLOCKWISE,
            .samplingType           = SE_SAMPLING_1,
        });
        const SeRenderRef sampler = render::sampler
        ({
            .magFilter          = SE_SAMPLER_FILTER_LINEAR,
            .minFilter          = SE_SAMPLER_FILTER_LINEAR,
            .addressModeU       = SE_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeV       = SE_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeW       = SE_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .mipmapMode         = SE_SAMPLER_MIPMAP_MODE_LINEAR,
            .mipLodBias         = 0.0f,
            .minLod             = 0.0f,
            .maxLod             = 0.0f,
            .anisotropyEnable   = false,
            .maxAnisotropy      = 0.0f,
            .compareEnabled     = false,
            .compareOp          = SE_COMPARE_OP_ALWAYS,
        });
        render::begin_pass
        ({
            .dependencies       = drawDependency | uiDependency,
            .pipeline           = presentPipeline,
            .renderTargets      = { { render::swap_chain_texture(), SE_PASS_RENDER_TARGET_LOAD_OP_CLEAR } },
            .numRenderTargets   = 1,
            .depthStencilTarget = { },
            .hasDepthStencil    = false,
        });
        {
            render::bind({ .set = 0, .bindings = { { 0, colorTexture, sampler } }  });
            render::draw({ .numVertices = 4, .numInstances = 1 });
        }
        render::end_pass();
        render::end_frame();
    }
}
