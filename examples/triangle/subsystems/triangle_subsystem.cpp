﻿
#include "triangle_subsystem.hpp"
#include "engine/engine.hpp"

struct FrameData
{
    SeFloat4x4 viewProjection;
};

struct InputVertex
{
    SeFloat3    positionLS;
    float       pad1;
    SeFloat2    uv;
    float       pad2[2];
    SeFloat4    color;
};

struct InputInstanceData
{
    SeFloat4x4 trfWS;
};

const SeRenderAbstractionSubsystemInterface* render;

DataProvider vertexProgramData;
DataProvider fragmentProgramData;

SE_DLL_EXPORT void se_init(SabrinaEngine* engine)
{
    se_init_global_subsystem_pointers(engine);
    render = se_get_subsystem_interface<SeRenderAbstractionSubsystemInterface>(engine);
    vertexProgramData = data_provider::from_file("assets/default/shaders/flat_color.vert.spv");
    fragmentProgramData = data_provider::from_file("assets/default/shaders/flat_color.frag.spv");
}

SE_DLL_EXPORT void se_terminate(SabrinaEngine* engine)
{
    data_provider::destroy(vertexProgramData);
    data_provider::destroy(fragmentProgramData);
}

SE_DLL_EXPORT void se_update(SabrinaEngine* engine, const SeUpdateInfo* info)
{
    const SeWindowSubsystemInput* input = win::get_input();
    if (input->isCloseButtonPressed || win::is_keyboard_button_pressed(input, SE_ESCAPE)) engine->shouldRun = false;

    const InputInstanceData instances[] =
    {
        { .trfWS = SE_F4X4_IDENTITY },
    };
    const InputVertex vertices[] =
    {
        { .positionLS = { -1, -1, 3 }, .uv = { 0, 0 }, .color = { 0.7f, 0.5f, 0.5f, 1.0f, } },
        { .positionLS = {  1, -1, 3 }, .uv = { 1, 1 }, .color = { 0.7f, 0.5f, 0.5f, 1.0f, } },
        { .positionLS = {  0,  1, 3 }, .uv = { 1, 0 }, .color = { 0.7f, 0.5f, 0.5f, 1.0f, } },
    };
    const float aspect = ((float)win::get_width()) / ((float)win::get_height());
    const FrameData frameData
    {
        .viewProjection = float4x4::transposed
        (
            float4x4::mul
            (
                render->perspective(60, aspect, 0.1f, 100.0f),
                float4x4::inverted(float4x4::look_at({ 0, 0, 0 }, { 0, 0, 1 }, { 0, 1, 0 }))
            )
        ),
    };
    if (render->begin_frame())
    {
        const SeRenderRef vertexProgram = render->program({ vertexProgramData });
        const SeRenderRef fragmentProgram = render->program({ fragmentProgramData });
        const SeRenderRef frameDataBuffer = render->memory_buffer({ data_provider::from_memory(&frameData, sizeof(frameData)) });
        const SeRenderRef instancesBuffer = render->memory_buffer({ data_provider::from_memory(instances, sizeof(instances)) });
        const SeRenderRef verticesBuffer = render->memory_buffer({ data_provider::from_memory(vertices, sizeof(vertices)) });
        const SeRenderRef pipeline = render->graphics_pipeline
        ({
            .vertexProgram          = { .program = vertexProgram, },
            .fragmentProgram        = { .program = fragmentProgram, },
            .frontStencilOpState    = { .isEnabled = false, },
            .backStencilOpState     = { .isEnabled = false, },
            .depthState             = { .isTestEnabled = false, .isWriteEnabled = false, },
            .polygonMode            = SE_PIPELINE_POLYGON_FILL_MODE_FILL,
            .cullMode               = SE_PIPELINE_CULL_MODE_NONE,
            .frontFace              = SE_PIPELINE_FRONT_FACE_CLOCKWISE,
            .samplingType           = SE_SAMPLING_1,
        });
        render->begin_pass
        ({
            .dependencies       = 0,
            .pipeline           = pipeline,
            .renderTargets      = { { render->swap_chain_texture(), SE_PASS_RENDER_TARGET_LOAD_OP_CLEAR } },
            .numRenderTargets   = 1,
            .depthStencilTarget = { },
            .hasDepthStencil    = false,
        });
        {
            render->bind({ .set = 0, .bindings = { { 0, frameDataBuffer } } });
            render->bind({ .set = 1, .bindings = { { 0, verticesBuffer }, { 1, instancesBuffer } } });
            render->draw({ .numVertices = se_array_size(vertices), .numInstances = se_array_size(instances) });
        }
        render->end_pass();
        render->end_frame();
    }
}
