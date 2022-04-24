
#include <string.h>
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

const SePlatformSubsystemInterface*               platformInterface;
const SeWindowSubsystemInterface*                 windowInterface;
const SeRenderAbstractionSubsystemInterface*      render;
const SeApplicationAllocatorsSubsystemInterface*  allocatorsInterface;

SeWindowHandle window;
SeDeviceHandle device;

SeRenderRef vertexProgram;
SeRenderRef fragmentProgram;

SeRenderRef sync_load_shader(const char* path)
{
    SeFile shader = { };
    SeFileContent content = { };
    platformInterface->file_load(&shader, path, SE_FILE_READ);
    platformInterface->file_read(&content, &shader, allocatorsInterface->frameAllocator);
    SeRenderRef program = render->program(device,
    {
        .bytecode       = (uint32_t*)content.memory,
        .bytecodeSize   = content.size,
    });
    platformInterface->file_free_content(&content);
    platformInterface->file_unload(&shader);
    return program;
}

SE_DLL_EXPORT void se_init(SabrinaEngine* engine)
{
    windowInterface = se_get_subsystem_interface<SeWindowSubsystemInterface>(engine);
    render = se_get_subsystem_interface<SeRenderAbstractionSubsystemInterface>(engine);
    allocatorsInterface = se_get_subsystem_interface<SeApplicationAllocatorsSubsystemInterface>(engine);
    platformInterface = se_get_subsystem_interface<SePlatformSubsystemInterface>(engine);
    
    SeWindowSubsystemCreateInfo windowInfo
    {
        .name           = "Sabrina engine - triangle example",
        .isFullscreen   = false,
        .isResizable    = true,
        .width          = 640,
        .height         = 480,
    };
    window = windowInterface->create(&windowInfo);
    device = render->device_create({ .window = window });
    vertexProgram = sync_load_shader("assets/default/shaders/flat_color.vert.spv");
    fragmentProgram = sync_load_shader("assets/default/shaders/flat_color.frag.spv");
}

SE_DLL_EXPORT void se_terminate(SabrinaEngine* engine)
{
    render->device_destroy(device);
    windowInterface->destroy(window);
}

SE_DLL_EXPORT void se_update(SabrinaEngine* engine, const SeUpdateInfo* info)
{
    const SeWindowSubsystemInput* input = windowInterface->get_input(window);
    if (input->isCloseButtonPressed || se_is_keyboard_button_pressed(input, SE_ESCAPE)) engine->shouldRun = false;

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
    const float aspect = ((float)windowInterface->get_width(window)) / ((float)windowInterface->get_height(window));
    const FrameData frameData
    {
        .viewProjection = se_f4x4_transposed
        (
            se_f4x4_mul_f4x4
            (
                render->perspective_projection_matrix(60, aspect, 0.1f, 100.0f),
                se_f4x4_inverted(se_look_at({ 0, 0, 0 }, { 0, 0, 1 }, { 0, 1, 0 }))
            )
        ),
    };

    render->begin_frame(device);
    {
        const SeRenderRef frameDataBuffer = render->memory_buffer(device, { .size = sizeof(frameData), .data = &frameData });
        const SeRenderRef instancesBuffer = render->memory_buffer(device, { .size = sizeof(instances), .data = instances });
        const SeRenderRef verticesBuffer = render->memory_buffer(device, { .size = sizeof(vertices), .data = vertices });
        const SeRenderRef pipeline = render->graphics_pipeline(device,
        {
            .device                 = device,
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
        render->begin_pass(device,
        {
            .id                 = 0,
            .dependencies       = 0,
            .pipeline           = pipeline,
            .renderTargets      = { { render->swap_chain_texture(device), SE_PASS_RENDER_TARGET_LOAD_OP_CLEAR } },
            .numRenderTargets   = 1,
            .depthStencilTarget = { },
            .hasDepthStencil    = false,
        });
        {
            render->bind(device, { .set = 0, .bindings = { { 0, frameDataBuffer } }, .numBindings = 1 });
            render->bind(device, { .set = 1, .bindings = { { 0, verticesBuffer }, { 1, instancesBuffer } }, .numBindings = 2 });
            render->draw(device, { .numVertices = se_array_size(vertices), .numInstances = se_array_size(instances) });
        }
        render->end_pass(device);
    }
    render->end_frame(device);
}
