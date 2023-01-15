
#include "engine/engine.hpp"
#include "engine/engine.cpp"

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

DataProvider vertexProgramData;
DataProvider fragmentProgramData;

void init()
{
    vertexProgramData = data_provider::from_file("flat_color.vert.spv");
    fragmentProgramData = data_provider::from_file("flat_color.frag.spv");
}

void terminate()
{
    
}

void update(const SeUpdateInfo& info)
{
    if (win::is_close_button_pressed() || win::is_keyboard_button_pressed(SeKeyboard::ESCAPE)) engine::stop();

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
    static const float aspect = ((float)win::get_width()) / ((float)win::get_height());
    static const FrameData frameData
    {
        .viewProjection = float4x4::transposed
        (
            float4x4::mul
            (
                render::perspective(60, aspect, 0.1f, 100.0f),
                float4x4::inverted(float4x4::look_at({ 0, 0, 0 }, { 0, 0, 1 }, { 0, 1, 0 }))
            )
        ),
    };
    if (render::begin_frame())
    {
        static const SeProgramRef vertexProgram = render::program({ vertexProgramData });
        static const SeProgramRef fragmentProgram = render::program({ fragmentProgramData });
        static const SeBufferRef frameDataBuffer = render::memory_buffer({ data_provider::from_memory(&frameData, sizeof(frameData)) });
        static const SeBufferRef instancesBuffer = render::memory_buffer({ data_provider::from_memory(instances, sizeof(instances)) });
        static const SeBufferRef verticesBuffer = render::memory_buffer({ data_provider::from_memory(vertices, sizeof(vertices)) });
        render::begin_graphics_pass
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
            .renderTargets          = { { render::swap_chain_texture(), SeRenderTargetLoadOp::CLEAR } },
            .depthStencilTarget     = { },
        });
        {
            render::bind({ .set = 0, .bindings =
            {
                { .binding = 0, .type = SeBinding::BUFFER, .buffer = { frameDataBuffer } }
            } });
            render::bind({ .set = 1, .bindings =
            {
                { .binding = 0, .type = SeBinding::BUFFER, .buffer = { verticesBuffer } },
                { .binding = 1, .type = SeBinding::BUFFER, .buffer = { instancesBuffer } }
            } });
            render::draw({ .numVertices = se_array_size(vertices), .numInstances = se_array_size(instances) });
        }
        render::end_pass();
        render::end_frame();
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
    engine::run(settings, init, update, terminate);
    return 0;
}
