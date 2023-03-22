
#include "engine/se_engine.hpp"
#include "engine/se_engine.cpp"

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

SeDataProvider vertexProgramData;
SeDataProvider fragmentProgramData;

void init()
{
    vertexProgramData = se_data_provider_from_file("flat_color.vert.spv");
    fragmentProgramData = se_data_provider_from_file("flat_color.frag.spv");
}

void terminate()
{
    
}

void update(const SeUpdateInfo& info)
{
    if (se_win_is_close_button_pressed() || se_win_is_keyboard_button_pressed(SeKeyboard::ESCAPE)) se_engine_stop();

    static const InputInstanceData instances[] =
    {
        { .trfWS = SE_F4X4_IDENTITY },
    };
    static const InputVertex vertices[] =
    {
        { .positionLS = { -1, -1, 3 }, .uv = { 0, 0 }, .color = { 0.7f, 0.5f, 0.5f, 1.0f, } },
        { .positionLS = {  1, -1, 3 }, .uv = { 1, 1 }, .color = { 0.7f, 0.5f, 0.5f, 1.0f, } },
        { .positionLS = {  0,  1, 3 }, .uv = { 1, 0 }, .color = { 0.7f, 0.5f, 0.5f, 1.0f, } },
    };
    static const float aspect = ((float)se_win_get_width()) / ((float)se_win_get_height());
    static const FrameData frameData
    {
        .viewProjection = se_float4x4_transposed
        (
            se_float4x4_mul
            (
                se_render_perspective(60, aspect, 0.1f, 100.0f),
                se_float4x4_inverted(se_float4x4_look_at({ 0, 0, 0 }, { 0, 0, 1 }, { 0, 1, 0 }))
            )
        ),
    };
    if (se_render_begin_frame())
    {
        static const SeProgramRef vertexProgram = se_render_program({ vertexProgramData });
        static const SeProgramRef fragmentProgram = se_render_program({ fragmentProgramData });
        static const SeBufferRef frameDataBuffer = se_render_memory_buffer({ se_data_provider_from_memory(&frameData, sizeof(frameData)) });
        static const SeBufferRef instancesBuffer = se_render_memory_buffer({ se_data_provider_from_memory(instances, sizeof(instances)) });
        static const SeBufferRef verticesBuffer = se_render_memory_buffer({ se_data_provider_from_memory(vertices, sizeof(vertices)) });
        se_render_begin_graphics_pass
        ({
            .dependencies           = 0,
            .vertexProgram          = { .program = vertexProgram, },
            .fragmentProgram        = { .program = fragmentProgram, },
            .frontStencilOpState    = { .isEnabled = false, },
            .backStencilOpState     = { .isEnabled = false, },
            .depthState             = { .isTestEnabled = false, .isWriteEnabled = false, },
            .polygonMode            = SePipelinePolygonMode::FILL,
            .cullMode               = SePipelineCullMode::NONE,
            .frontFace              = SePipelineFrontFace::CLOCKWISE,
            .samplingType           = SeSamplingType::_1,
            .renderTargets          = { { se_render_swap_chain_texture(), SeRenderTargetLoadOp::CLEAR } },
            .depthStencilTarget     = { },
        });
        {
            se_render_bind({ .set = 0, .bindings =
            {
                { .binding = 0, .type = SeBinding::BUFFER, .buffer = { frameDataBuffer } }
            } });
            se_render_bind({ .set = 1, .bindings =
            {
                { .binding = 0, .type = SeBinding::BUFFER, .buffer = { verticesBuffer } },
                { .binding = 1, .type = SeBinding::BUFFER, .buffer = { instancesBuffer } }
            } });
            se_render_draw({ .numVertices = se_array_size(vertices), .numInstances = se_array_size(instances) });
        }
        se_render_end_pass();
        se_render_end_frame();
    }
}

int main(int argc, char* argv[])
{
    const SeSettings settings
    {
        .applicationName        = "Sabrina engine - triangle example",
        .isFullscreenWindow     = false,
        .isResizableWindow      = false,
        .windowWidth            = 640,
        .windowHeight           = 480,
        .createUserDataFolder   = false,
    };
    se_engine_run(settings, init, update, terminate);
    return 0;
}
