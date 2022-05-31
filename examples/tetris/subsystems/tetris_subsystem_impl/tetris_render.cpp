
#include <string.h>
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

extern SeWindowHandle g_window;
extern const SeRenderAbstractionSubsystemInterface* g_render;
extern TetrisState g_state;

SeDeviceHandle  g_device;
SeRenderRef     g_drawVs;
SeRenderRef     g_drawFs;
SeRenderRef     g_presentVs;
SeRenderRef     g_presentFs;

SeRenderRef sync_load_shader(const char* path)
{
    SeFile shader = platform::get()->file_load(path, SE_FILE_READ);
    SeFileContent content = platform::get()->file_read(&shader, app_allocators::frame());
    SeProgramInfo createInfo
    {
        .bytecode = (uint32_t*)content.memory,
        .bytecodeSize = content.size,
    };
    SeRenderRef program = g_render->program(g_device, createInfo);
    platform::get()->file_free_content(&content);
    platform::get()->file_unload(&shader);
    return program;
}

void tetris_render_init()
{
    g_device    = g_render->device_create({ .window = g_window, });   
    g_drawVs    = sync_load_shader("assets/application/shaders/tetris_draw.vert.spv");
    g_drawFs    = sync_load_shader("assets/application/shaders/tetris_draw.frag.spv");
    g_presentVs = sync_load_shader("assets/default/shaders/present.vert.spv");
    g_presentFs = sync_load_shader("assets/default/shaders/present.frag.spv");
}

void tetris_render_terminate()
{
    g_render->device_destroy(g_device);
}

void tetris_render_update(const SeWindowSubsystemInput* input, float dt)
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
    const float aspect = ((float)win::get_width(g_window)) / ((float)win::get_height(g_window));
    const SeFloat4x4 perspective = g_render->perspective_projection_matrix(60, aspect, 0.1f, 100.0f);
    const FrameData frameData = { .viewProjection = float4x4::transposed(perspective * float4x4::inverted(view)) };
    //
    // Render
    //
    g_render->begin_frame(g_device);
    {
        //
        // Offscreen pass
        //
        const SeRenderRef drawPipeline = g_render->graphics_pipeline(g_device,
        {
            .vertexProgram          = { .program = g_drawVs, },
            .fragmentProgram        = { .program = g_drawFs, },
            .frontStencilOpState    = { .isEnabled = false, },
            .backStencilOpState     = { .isEnabled = false, },
            .depthState             = { .isTestEnabled = true, .isWriteEnabled = true, },
            .polygonMode            = SE_PIPELINE_POLYGON_FILL_MODE_FILL,
            .cullMode               = SE_PIPELINE_CULL_MODE_BACK,
            .frontFace              = SE_PIPELINE_FRONT_FACE_CLOCKWISE,
            .samplingType           = SE_SAMPLING_1,
        });
        const SeRenderRef colorTexture = g_render->texture(g_device,
        {
            .width  = win::get_width(g_window),
            .height = win::get_height(g_window),
            .format = SE_TEXTURE_FORMAT_RGBA_8,
            .data   = { },
        });
        const SeRenderRef depthTexture = g_render->texture(g_device,
        {
            .width  = win::get_width(g_window),
            .height = win::get_height(g_window),
            .format = SE_TEXTURE_FORMAT_DEPTH_STENCIL,
            .data   = { },
        });
        g_render->begin_pass(g_device,
        {
            .id                 = 0,
            .dependencies       = 0,
            .pipeline           = drawPipeline,
            .renderTargets      = { { colorTexture, SE_PASS_RENDER_TARGET_LOAD_OP_CLEAR } },
            .numRenderTargets   = 1,
            .depthStencilTarget = { depthTexture, SE_PASS_RENDER_TARGET_LOAD_OP_CLEAR },
            .hasDepthStencil    = true,
        });
        {
            const SeRenderRef gridVerticesBuffer    = g_render->memory_buffer(g_device, { data_provider::from_memory(gridVertices, sizeof(gridVertices)) });
            const SeRenderRef gridIndicesBuffer     = g_render->memory_buffer(g_device, { data_provider::from_memory(gridIndices, sizeof(gridIndices)) });
            const SeRenderRef gridInstancesBuffer   = g_render->memory_buffer(g_device, { data_provider::from_memory(gridInstances, sizeof(gridInstances)) });
            const SeRenderRef frameDataBuffer       = g_render->memory_buffer(g_device, { data_provider::from_memory(&frameData, sizeof(frameData)) });
            g_render->bind(g_device, { .set = 0, .bindings = { { 0, frameDataBuffer } } });
            if (numCubeInstances)
            {
                const SeRenderRef cubeVerticesBuffer    = g_render->memory_buffer(g_device, { data_provider::from_memory(cubeVertices, sizeof(cubeVertices)) });
                const SeRenderRef cubeIndicesBuffer     = g_render->memory_buffer(g_device, { data_provider::from_memory(cubeIndices, sizeof(cubeIndices)) });
                const SeRenderRef cubeInstancesBuffer   = g_render->memory_buffer(g_device, { data_provider::from_memory(cubeInstances, numCubeInstances * sizeof(InputInstance)) });
                g_render->bind(g_device, { .set = 1, .bindings =
                { 
                    { 0, cubeVerticesBuffer  },
                    { 1, cubeIndicesBuffer   },
                    { 2, cubeInstancesBuffer }, 
                } });
                g_render->draw(g_device, { .numVertices = se_array_size(cubeIndices), .numInstances = numCubeInstances });
            }
            g_render->bind(g_device, { .set = 1, .bindings =
            { 
                { 0, gridVerticesBuffer  },
                { 1, gridIndicesBuffer   },
                { 2, gridInstancesBuffer }, 
            } });
            g_render->draw(g_device, { .numVertices = se_array_size(gridIndices), .numInstances = se_array_size(gridInstances) });
        }
        g_render->end_pass(g_device);
        //
        // Presentation pass
        //
        const SeRenderRef presentPipeline = g_render->graphics_pipeline(g_device,
        {
            .vertexProgram          = { .program = g_presentVs, },
            .fragmentProgram        = { .program = g_presentFs, },
            .frontStencilOpState    = { .isEnabled = false, },
            .backStencilOpState     = { .isEnabled = false, },
            .depthState             = { .isTestEnabled = false, .isWriteEnabled = false, },
            .polygonMode            = SE_PIPELINE_POLYGON_FILL_MODE_FILL,
            .cullMode               = SE_PIPELINE_CULL_MODE_NONE,
            .frontFace              = SE_PIPELINE_FRONT_FACE_CLOCKWISE,
            .samplingType           = SE_SAMPLING_1,
        });
        const SeRenderRef sampler = g_render->sampler(g_device,
        {
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
        g_render->begin_pass(g_device,
        {
            .id                 = 0,
            .dependencies       = 1,
            .pipeline           = presentPipeline,
            .renderTargets      = { { g_render->swap_chain_texture(g_device), SE_PASS_RENDER_TARGET_LOAD_OP_CLEAR } },
            .numRenderTargets   = 1,
            .depthStencilTarget = { },
            .hasDepthStencil    = false,
        });
        {
            g_render->bind(g_device, { .set = 0, .bindings = { { 0, colorTexture, sampler } }  });
            g_render->draw(g_device, { .numVertices = 4, .numInstances = 1 });
        }
        g_render->end_pass(g_device);
    }
    g_render->end_frame(g_device);
}
