
#include <string.h>
#include "triangle_subsystem.h"
#include "engine/engine.h"

typedef struct FrameData
{
    SeFloat4x4 viewProjection;
} FrameData;

typedef struct InputVertex
{
    SeFloat3 positionLS;
    float pad1;
    SeFloat2 uv;
    float pad2[2];
} InputVertex;

typedef struct InputInstanceData
{
    SeFloat4x4 trfWS;
} InputInstanceData;

SePlatformInterface* platformInterface;
SeWindowSubsystemInterface* windowInterface;
SeRenderAbstractionSubsystemInterface* renderInterface;
SeApplicationAllocatorsSubsystemInterface* allocatorsInterface;

SeWindowHandle windowHandle;
SeRenderObject* renderDevice;

SeRenderObject* vs;
SeRenderObject* fs;
SeRenderObject* renderPass;
SeRenderObject* renderPipeline;
se_sbuffer(SeRenderObject*) framebuffers;

SeRenderObject* frameDataBuffer;
SeRenderObject* verticesBuffer;
SeRenderObject* instancesBuffer;

SeRenderObject* sync_load_shader(const char* path)
{
    SeFile shader = {0};
    SeFileContent content = {0};
    platformInterface->file_load(&shader, path, SE_FILE_READ);
    platformInterface->file_read(&content, &shader, allocatorsInterface->frameAllocator);
    SeRenderProgramCreateInfo createInfo = (SeRenderProgramCreateInfo)
    {
        .device = renderDevice,
        .bytecode = (uint32_t*)content.memory,
        .codeSizeBytes = content.size,
    };
    SeRenderObject* program = renderInterface->program_create(&createInfo);
    platformInterface->file_free_content(&content);
    platformInterface->file_unload(&shader);
    return program;
}

SE_DLL_EXPORT void se_init(SabrinaEngine* engine)
{
    windowInterface = (SeWindowSubsystemInterface*)engine->find_subsystem_interface(engine, SE_WINDOW_SUBSYSTEM_NAME);
    renderInterface = (SeRenderAbstractionSubsystemInterface*)engine->find_subsystem_interface(engine, SE_VULKAN_RENDER_SUBSYSTEM_NAME);
    allocatorsInterface = (SeApplicationAllocatorsSubsystemInterface*)engine->find_subsystem_interface(engine, SE_APPLICATION_ALLOCATORS_SUBSYSTEM_NAME);
    platformInterface = &engine->platformIface;
    //
    // Create window
    //
    {
        SeWindowSubsystemCreateInfo createInfo = (SeWindowSubsystemCreateInfo)
        {
            .name           = "Drawing quad",
            .isFullscreen   = false,
            .isResizable    = false,
            .width          = 640,
            .height         = 480,
        };
        windowHandle = windowInterface->create(&createInfo);
    }
    //
    // Create render device
    //
    {
        SeRenderDeviceCreateInfo createInfo = (SeRenderDeviceCreateInfo)
        {
            .window = &windowHandle, .persistentAllocator = allocatorsInterface->persistentAllocator, .frameAllocator = allocatorsInterface->frameAllocator,
        };
        renderDevice = renderInterface->device_create(&createInfo);
    }
    //
    // Create shaders
    //
    {
        vs = sync_load_shader("assets/default/shaders/flat.vert.spv");
        fs = sync_load_shader("assets/default/shaders/flat.frag.spv");
    }
    //
    // Create render pass
    //
    {
        SeRenderPassAttachment colorAttachments[] =
        {
            {
                .format     = SE_TEXTURE_FORMAT_SWAP_CHAIN,
                .loadOp     = SE_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp    = SE_ATTACHMENT_STORE_OP_STORE,
                .samples    = SE_SAMPLE_1,
            }
        };
        uint32_t colorRefs[] = { 0, };
        SeRenderPassSubpass subpasses[] =
        {
            {
                .colorRefs      = colorRefs,
                .numColorRefs   = se_array_size(colorRefs),
                .inputRefs      = NULL,
                .numInputRefs   = 0,
                .resolveRefs    = NULL,
                .numResolveRefs = 0,
                .depthOp        = SE_DEPTH_OP_NOTHING,
            },
        };
        SeRenderPassCreateInfo createInfo = (SeRenderPassCreateInfo)
        {
            .subpasses              = subpasses,
            .numSubpasses           = se_array_size(subpasses),
            .colorAttachments       = colorAttachments,
            .numColorAttachments    = se_array_size(colorAttachments),
            .depthStencilAttachment = NULL,
            .device                 = renderDevice,
        };
        renderPass = renderInterface->render_pass_create(&createInfo);
    }
    //
    // Create render pipeline
    //
    {
        SeGraphicsRenderPipelineCreateInfo createInfo = (SeGraphicsRenderPipelineCreateInfo)
        {
            .device                 = renderDevice,
            .renderPass             = renderPass,
            .vertexProgram          = vs,
            .fragmentProgram        = fs,
            .frontStencilOpState    = NULL,
            .backStencilOpState     = NULL,
            .depthTestState         = NULL,
            .subpassIndex           = 0,
            .poligonMode            = SE_PIPELINE_POLIGON_FILL_MODE_FILL,
            .cullMode               = SE_PIPELINE_CULL_MODE_NONE,
            .frontFace              = SE_PIPELINE_FRONT_FACE_CLOCKWISE,
            .multisamplingType      = SE_SAMPLE_1,
        };
        renderPipeline = renderInterface->render_pipeline_graphics_create(&createInfo);
    }
    //
    // Create framebuffers
    //
    {
        const size_t numSwapChainImages = renderInterface->get_swap_chain_textures_num(renderDevice);
        se_sbuffer_construct(framebuffers, numSwapChainImages, allocatorsInterface->persistentAllocator);
        for (size_t it = 0; it < numSwapChainImages; it++)
        {
            SeRenderObject* attachments[] = { renderInterface->get_swap_chain_texture(renderDevice, it) };
            SeFramebufferCreateInfo createInfo = (SeFramebufferCreateInfo)
            {
                .attachmentsPtr = attachments,
                .numAttachments = se_array_size(attachments),
                .renderPass     = renderPass,
                .device         = renderDevice,
            };
            SeRenderObject* fb = renderInterface->framebuffer_create(&createInfo);
            se_sbuffer_push(framebuffers, fb);
        }
    }
    //
    // Fill data buffers
    //
    {
        const float aspect = (float)windowInterface->get_width(windowHandle) / (float)windowInterface->get_height(windowHandle);
        FrameData data =
        {
            .viewProjection = se_f4x4_transposed
            (
                se_f4x4_mul_f4x4
                (
                    se_perspective_projection(60, aspect, 0.1f, 100.0f),
                    se_f4x4_inverted(se_look_at((SeFloat3){ 0, 0, 0 }, (SeFloat3){ 0, 0, 1 }, (SeFloat3){ 0, 1, 0 }))
                )
            ),
        };
        SeMemoryBufferCreateInfo createInfo = (SeMemoryBufferCreateInfo)
        {
            .device     = renderDevice,
            .size       = sizeof(data),
            .usage      = SE_MEMORY_BUFFER_USAGE_UNIFORM_BUFFER,
            .visibility = SE_MEMORY_BUFFER_VISIBILITY_GPU_AND_CPU,
        };
        frameDataBuffer = renderInterface->memory_buffer_create(&createInfo);
        void* memory = renderInterface->memory_buffer_get_mapped_address(frameDataBuffer);
        memcpy(memory, &data, sizeof(data));
    }
    {
        InputVertex vertices[] =
        {
            { .positionLS = { -1, -1, 3 }, .uv = { 0, 0 } },
            { .positionLS = {  1, -1, 3 }, .uv = { 1, 1 } },
            { .positionLS = {  0,  1, 3 }, .uv = { 1, 0 } },
        };
        SeMemoryBufferCreateInfo createInfo = (SeMemoryBufferCreateInfo)
        {
            .device     = renderDevice,
            .size       = sizeof(vertices),
            .usage      = SE_MEMORY_BUFFER_USAGE_STORAGE_BUFFER,
            .visibility = SE_MEMORY_BUFFER_VISIBILITY_GPU_AND_CPU,
        };
        verticesBuffer = renderInterface->memory_buffer_create(&createInfo);
        void* memory = renderInterface->memory_buffer_get_mapped_address(verticesBuffer);
        memcpy(memory, &vertices, sizeof(vertices));
    }
    {
        InputInstanceData instances[] =
        {
            { .trfWS = SE_F4X4_IDENTITY },
        };
        SeMemoryBufferCreateInfo createInfo = (SeMemoryBufferCreateInfo)
        {
            .device     = renderDevice,
            .size       = sizeof(instances),
            .usage      = SE_MEMORY_BUFFER_USAGE_STORAGE_BUFFER,
            .visibility = SE_MEMORY_BUFFER_VISIBILITY_GPU_AND_CPU,
        };
        instancesBuffer = renderInterface->memory_buffer_create(&createInfo);
        void* memory = renderInterface->memory_buffer_get_mapped_address(instancesBuffer);
        memcpy(memory, &instances, sizeof(instances));
    }
}

SE_DLL_EXPORT void se_terminate(SabrinaEngine* engine)
{
    //
    // Destroy render resources
    //
    renderInterface->device_wait(renderDevice);
    frameDataBuffer->destroy(frameDataBuffer);
    verticesBuffer->destroy(verticesBuffer);
    instancesBuffer->destroy(instancesBuffer);
    const size_t numFramebuffers = se_sbuffer_size(framebuffers);
    for (size_t it = 0; it < numFramebuffers; it++) framebuffers[it]->destroy(framebuffers[it]);
    se_sbuffer_destroy(framebuffers);
    renderPipeline->destroy(renderPipeline);
    renderPass->destroy(renderPass);
    fs->destroy(fs);
    vs->destroy(vs);
    renderDevice->destroy(renderDevice);
    //
    // Destroy window
    //
    windowInterface->destroy(windowHandle);
}

SE_DLL_EXPORT void se_update(SabrinaEngine* engine, const SeUpdateInfo* info)
{
    const SeWindowSubsystemInput* input = windowInterface->get_input(windowHandle);
    if (input->isCloseButtonPressed || se_is_keyboard_button_pressed(input, SE_ESCAPE)) engine->shouldRun = false;

    renderInterface->begin_frame(renderDevice);
    {
        SeRenderObject* framebuffer = framebuffers[renderInterface->get_active_swap_chain_texture_index(renderDevice)];
        SeRenderObject* bindings0[] = { frameDataBuffer, }; 
        SeRenderObject* bindings1[] = { verticesBuffer, instancesBuffer, };
        SeResourceSetCreateInfo setCreateInfos[] =
        {
            {
                .device         = renderDevice,
                .pipeline       = renderPipeline,
                .set            = 0,
                .bindings       = bindings0,
                .numBindings    = se_array_size(bindings0),
            },
            {
                .device         = renderDevice,
                .pipeline       = renderPipeline,
                .set            = 1,
                .bindings       = bindings1,
                .numBindings    = se_array_size(bindings1),
            },
        };
        SeRenderObject* resourceSets[2] = {0};
        resourceSets[0] = renderInterface->resource_set_create(&setCreateInfos[0]);
        resourceSets[1] = renderInterface->resource_set_create(&setCreateInfos[1]);
        {
            SeCommandBufferRequestInfo requestInfo = (SeCommandBufferRequestInfo) { renderDevice, SE_COMMAND_BUFFER_USAGE_GRAPHICS };
            SeRenderObject* cmd = renderInterface->command_buffer_request(&requestInfo);
            SeCommandBindPipelineInfo bindPipeline = (SeCommandBindPipelineInfo) { renderPipeline, framebuffer };
            renderInterface->command_bind_pipeline(cmd, &bindPipeline);
            renderInterface->command_bind_resource_set(cmd, &((SeCommandBindResourceSetInfo) { .resourceSet = resourceSets[0] }));
            renderInterface->command_bind_resource_set(cmd, &((SeCommandBindResourceSetInfo) { .resourceSet = resourceSets[1] }));
            renderInterface->command_draw(cmd, &((SeCommandDrawInfo) { .numVertices = 3, .numInstances = 1 }));
            renderInterface->command_buffer_submit(cmd);
        }
        resourceSets[0]->destroy(resourceSets[0]);
        resourceSets[1]->destroy(resourceSets[1]);
    }
    renderInterface->end_frame(renderDevice);
}
