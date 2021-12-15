
#include "offscreen_subsystem.h"
#include "engine/engine.h"
#include "debug_camera.h"

SePlatformInterface* platformInterface;
SeWindowSubsystemInterface* windowInterface;
SeRenderAbstractionSubsystemInterface* renderInterface;
SeApplicationAllocatorsSubsystemInterface* allocatorsInterface;

SeWindowHandle windowHandle;
SeRenderObject* renderDevice;

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

SeRenderObject* frameDataBuffer;
SeRenderObject* verticesBuffer;
SeRenderObject* instanceDataBuffer;

SeRenderObject* flatRenderPass;
SeRenderObject* flatVs;
SeRenderObject* flatFs;
SeRenderObject* flatRenderPipeline;
SeRenderObject* flatColorTexture;   
SeRenderObject* flatDepthTexture;
SeRenderObject* flatFramebuffer;

SeRenderObject* presentRenderPass;
SeRenderObject* presentVs;
SeRenderObject* presentFs;
SeRenderObject* presentRenderPipeline;
se_sbuffer(SeRenderObject*) presentFramebuffers;

DebugCamera camera;

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

void terminate_flat_pipeline()
{
    flatRenderPass->destroy(flatRenderPass);
    flatVs->destroy(flatVs);
    flatFs->destroy(flatFs);
    flatRenderPipeline->destroy(flatRenderPipeline);
    flatColorTexture->destroy(flatColorTexture);
    flatDepthTexture->destroy(flatDepthTexture);
    flatFramebuffer->destroy(flatFramebuffer);
}

void terminate_present_pipeline()
{
    presentRenderPass->destroy(presentRenderPass);
    presentVs->destroy(presentVs);
    presentFs->destroy(presentFs);
    presentRenderPipeline->destroy(presentRenderPipeline);
    for (size_t it = 0; it < se_sbuffer_size(presentFramebuffers); it++)
    {
        presentFramebuffers[it]->destroy(presentFramebuffers[it]);
    }
}

void init_flat_pipeline()
{
    flatVs = sync_load_shader("assets/default/shaders/flat.vert.spv");
    flatFs = sync_load_shader("assets/default/shaders/flat.frag.spv");
    SeRenderObject* swapChainTexture = renderInterface->get_swap_chain_texture(renderDevice, 0);
    {
        SeTextureCreateInfo createInfo = (SeTextureCreateInfo)
        {
            .device = renderDevice,
            .width  = renderInterface->texture_get_width(swapChainTexture),
            .height = renderInterface->texture_get_height(swapChainTexture),
            .usage  = SE_TEXTURE_USAGE_RENDER_PASS_ATTACHMENT | SE_TEXTURE_USAGE_TEXTURE,
            .format = SE_TEXTURE_FORMAT_RGBA_8,
        };
        flatColorTexture = renderInterface->texture_create(&createInfo);
    }
    {
        SeTextureCreateInfo createInfo = (SeTextureCreateInfo)
        {
            .device = renderDevice,
            .width  = renderInterface->texture_get_width(swapChainTexture),
            .height = renderInterface->texture_get_height(swapChainTexture),
            .usage  = SE_TEXTURE_USAGE_RENDER_PASS_ATTACHMENT,
            .format = SE_TEXTURE_FORMAT_DEPTH_STENCIL,
        };
        flatDepthTexture = renderInterface->texture_create(&createInfo);
    }
    {
        SeRenderPassAttachment colorAttachments[] =
        {
            {
                .format     = SE_TEXTURE_FORMAT_RGBA_8,
                .loadOp     = SE_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp    = SE_ATTACHMENT_STORE_OP_STORE,
                .samples    = SE_SAMPLE_1,
            },
        };
        SeRenderPassAttachment depthAttachment = (SeRenderPassAttachment)
        {
            .format     = SE_TEXTURE_FORMAT_DEPTH_STENCIL,
            .loadOp     = SE_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp    = SE_ATTACHMENT_STORE_OP_STORE,
            .samples    = SE_SAMPLE_1,
        };
        uint32_t colorRefs[] = { 0 };
        SeRenderPassSubpass subpasses[] =
        {
            {
                .colorRefs      = colorRefs,
                .numColorRefs   = se_array_size(colorRefs),
                .inputRefs      = NULL,
                .numInputRefs   = 0,
                .resolveRefs    = NULL,
                .numResolveRefs = 0,
                .depthOp        = SE_DEPTH_OP_READ_WRITE,
            }
        };
        SeRenderPassCreateInfo createInfo = (SeRenderPassCreateInfo)
        {
            .subpasses              = subpasses,
            .numSubpasses           = se_array_size(subpasses),
            .colorAttachments       = colorAttachments,
            .numColorAttachments    = se_array_size(colorAttachments),
            .depthStencilAttachment = &depthAttachment,
            .device                 = renderDevice,
        };
        flatRenderPass = renderInterface->render_pass_create(&createInfo);
    }
    {
        SeDepthTestState depthTest = (SeDepthTestState)
        {
            .minDepthBounds         = 0,
            .maxDepthBounds         = 1,
            .isTestEnabled          = true,
            .isWriteEnabled         = true,
            .isBoundsTestEnabled    = false,
            .compareOp              = SE_COMPARE_OP_LESS,
        };
        SeGraphicsRenderPipelineCreateInfo createInfo = (SeGraphicsRenderPipelineCreateInfo)
        {
            .device                 = renderDevice,
            .renderPass             = flatRenderPass,
            .vertexProgram          = flatVs,
            .fragmentProgram        = flatFs,
            .frontStencilOpState    = NULL,
            .backStencilOpState     = NULL,
            .depthTestState         = &depthTest,
            .subpassIndex           = 0,
            .poligonMode            = SE_PIPELINE_POLIGON_FILL_MODE_FILL,
            .cullMode               = SE_PIPELINE_CULL_MODE_NONE,
            .frontFace              = SE_PIPELINE_FRONT_FACE_CLOCKWISE,
            .multisamplingType      = SE_SAMPLE_1,
        };
        flatRenderPipeline = renderInterface->render_pipeline_graphics_create(&createInfo);
    }
    {
        SeRenderObject* attachments[] = { flatColorTexture, flatDepthTexture };
        SeFramebufferCreateInfo createInfo = (SeFramebufferCreateInfo)
        {
            .attachmentsPtr = attachments,
            .numAttachments = se_array_size(attachments),
            .renderPass     = flatRenderPass,
            .device         = renderDevice,
        };
        flatFramebuffer = renderInterface->framebuffer_create(&createInfo);
    }
}

void init_present_pipeline()
{
    presentVs = sync_load_shader("assets/default/shaders/present.vert.spv");
    presentFs = sync_load_shader("assets/default/shaders/present.frag.spv");
    SeRenderObject* swapChainTexture = renderInterface->get_swap_chain_texture(renderDevice, 0);
    {
        SeRenderPassAttachment colorAttachments[] =
        {
            {
                .format     = SE_TEXTURE_FORMAT_SWAP_CHAIN,
                .loadOp     = SE_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp    = SE_ATTACHMENT_STORE_OP_STORE,
                .samples    = SE_SAMPLE_1,
            },
        };
        uint32_t colorRefs[] = { 0 };
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
            }
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
        presentRenderPass = renderInterface->render_pass_create(&createInfo);
    }
    {
        SeDepthTestState depthTest = (SeDepthTestState)
        {
            .minDepthBounds         = 0,
            .maxDepthBounds         = 1,
            .isTestEnabled          = false,
            .isWriteEnabled         = false,
            .isBoundsTestEnabled    = false,
            .compareOp              = SE_COMPARE_OP_ALWAYS,
        };
        SeGraphicsRenderPipelineCreateInfo createInfo = (SeGraphicsRenderPipelineCreateInfo)
        {
            .device                 = renderDevice,
            .renderPass             = presentRenderPass,
            .vertexProgram          = presentVs,
            .fragmentProgram        = presentFs,
            .frontStencilOpState    = NULL,
            .backStencilOpState     = NULL,
            .depthTestState         = &depthTest,
            .subpassIndex           = 0,
            .poligonMode            = SE_PIPELINE_POLIGON_FILL_MODE_FILL,
            .cullMode               = SE_PIPELINE_CULL_MODE_NONE,
            .frontFace              = SE_PIPELINE_FRONT_FACE_CLOCKWISE,
            .multisamplingType      = SE_SAMPLE_1,
        };
        presentRenderPipeline = renderInterface->render_pipeline_graphics_create(&createInfo);
    }
    {
        const size_t numSwapChainTextures = renderInterface->get_swap_chain_textures_num(renderDevice);
        se_sbuffer_construct(presentFramebuffers, numSwapChainTextures, allocatorsInterface->persistentAllocator);
        for (size_t it = 0; it < numSwapChainTextures; it++)
        {
            SeRenderObject* attachments[] = { renderInterface->get_swap_chain_texture(renderDevice, it) };
            SeFramebufferCreateInfo createInfo = (SeFramebufferCreateInfo)
            {
                .attachmentsPtr = attachments,
                .numAttachments = se_array_size(attachments),
                .renderPass     = presentRenderPass,
                .device         = renderDevice,
            };
            SeRenderObject* fb = renderInterface->framebuffer_create(&createInfo);
            se_sbuffer_push(presentFramebuffers, fb);
        }
    }
}

SE_DLL_EXPORT void se_init(SabrinaEngine* engine)
{
    windowInterface = (SeWindowSubsystemInterface*)engine->find_subsystem_interface(engine, SE_WINDOW_SUBSYSTEM_NAME);
    renderInterface = (SeRenderAbstractionSubsystemInterface*)engine->find_subsystem_interface(engine, SE_VULKAN_RENDER_SUBSYSTEM_NAME);
    allocatorsInterface = (SeApplicationAllocatorsSubsystemInterface*)engine->find_subsystem_interface(engine, SE_APPLICATION_ALLOCATORS_SUBSYSTEM_NAME);
    platformInterface = &engine->platformIface;
    {
        SeWindowSubsystemInterface* windowInterface = (SeWindowSubsystemInterface*)engine->find_subsystem_interface(engine, SE_WINDOW_SUBSYSTEM_NAME);
        SeWindowSubsystemCreateInfo createInfo = (SeWindowSubsystemCreateInfo)
        {
            .name           = "Sabrina Engine",
            .isFullscreen   = false,
            .isResizable    = false,
            .width          = 640,
            .height         = 480,
        };
        windowHandle = windowInterface->create(&createInfo);
    }
    {
        SeRenderDeviceCreateInfo createInfo = (SeRenderDeviceCreateInfo)
        {
            .window                 = &windowHandle,
            .persistentAllocator    = allocatorsInterface->persistentAllocator,
            .frameAllocator         = allocatorsInterface->frameAllocator,
        };
        renderDevice = renderInterface->device_create(&createInfo);
    }
    init_flat_pipeline();
    init_present_pipeline();
    {
        FrameData frameData = {0};
        frameData.viewProjection = se_f4x4_transposed(se_f4x4_mul_f4x4(se_perspective_projection(60.0f, 4.0f / 3.0f, 0.1f, 100.0f), se_f4x4_translation((SeFloat3){ 0, 0, 0 })));
        SeMemoryBufferCreateInfo memoryBufferCreateInfo = (SeMemoryBufferCreateInfo)
        {
            .device     = renderDevice,
            .size       = sizeof(frameData),
            .usage      = SE_MEMORY_BUFFER_USAGE_TRANSFER_DST | SE_MEMORY_BUFFER_USAGE_UNIFORM_BUFFER,
            .visibility = SE_MEMORY_BUFFER_VISIBILITY_GPU_AND_CPU,
        };
        frameDataBuffer = renderInterface->memory_buffer_create(&memoryBufferCreateInfo);
        void* addr = renderInterface->memory_buffer_get_mapped_address(frameDataBuffer);
        memcpy(addr, &frameData, sizeof(frameData));
    }
    {
        InputVertex verts[] =
        {
            { .positionLS = { 0 , 0 , 0 }, .uv = { 0, 0, }, },
            { .positionLS = { 0 , 10, 0 }, .uv = { 0, 1, }, },
            { .positionLS = { 10, 10, 0 }, .uv = { 1, 1, }, },
            { .positionLS = { 0 , 0 , 0 }, .uv = { 0, 0, }, },
            { .positionLS = { 10, 10, 0 }, .uv = { 1, 1, }, },
            { .positionLS = { 10, 0 , 0 }, .uv = { 1, 0, }, },
        };
        SeMemoryBufferCreateInfo memoryBufferCreateInfo = (SeMemoryBufferCreateInfo)
        {
            .device     = renderDevice,
            .size       = sizeof(verts),
            .usage      = SE_MEMORY_BUFFER_USAGE_TRANSFER_DST | SE_MEMORY_BUFFER_USAGE_STORAGE_BUFFER,
            .visibility = SE_MEMORY_BUFFER_VISIBILITY_GPU_AND_CPU,
        };
        verticesBuffer = renderInterface->memory_buffer_create(&memoryBufferCreateInfo);
        void* addr = renderInterface->memory_buffer_get_mapped_address(verticesBuffer);
        memcpy(addr, verts, sizeof(verts));
    }
    {
        InputInstanceData insts[] = { { SE_F4X4_IDENTITY }, { SE_F4X4_IDENTITY }, { SE_F4X4_IDENTITY } };
        insts[0].trfWS = se_f4x4_transposed(se_f4x4_translation((SeFloat3){ 0   , 0   , 1.0f }));
        insts[1].trfWS = se_f4x4_transposed(se_f4x4_translation((SeFloat3){ -10 , 0   , 2.0f }));
        insts[2].trfWS = se_f4x4_transposed(se_f4x4_translation((SeFloat3){ -10 , -10 , 3.0f }));
        SeMemoryBufferCreateInfo memoryBufferCreateInfo = (SeMemoryBufferCreateInfo)
        {
            .device     = renderDevice,
            .size       = sizeof(insts),
            .usage      = SE_MEMORY_BUFFER_USAGE_TRANSFER_DST | SE_MEMORY_BUFFER_USAGE_STORAGE_BUFFER,
            .visibility = SE_MEMORY_BUFFER_VISIBILITY_GPU_AND_CPU,
        };
        instanceDataBuffer = renderInterface->memory_buffer_create(&memoryBufferCreateInfo);
        void* addr = renderInterface->memory_buffer_get_mapped_address(instanceDataBuffer);
        memcpy(addr, insts, sizeof(insts));
    }
    debug_camera_construct(&camera, 60, 4.0f / 3.0f, 0.1f, 100.0f);
}

SE_DLL_EXPORT void se_terminate(SabrinaEngine* engine)
{
    renderInterface->device_wait(renderDevice);
    
    terminate_flat_pipeline();
    terminate_present_pipeline();
    frameDataBuffer->destroy(frameDataBuffer);
    verticesBuffer->destroy(verticesBuffer);
    instanceDataBuffer->destroy(instanceDataBuffer);
    renderDevice->destroy(renderDevice);
    windowInterface->destroy(windowHandle);
}

SE_DLL_EXPORT void se_update(SabrinaEngine* engine, const SeUpdateInfo* info)
{
    const SeWindowSubsystemInput* input = windowInterface->get_input(windowHandle);
    if (input->isCloseButtonPressed || se_is_keyboard_button_pressed(input, SE_ESCAPE)) engine->shouldRun = false;

    debug_camera_update(&camera, input, info->dt);

    renderInterface->begin_frame(renderDevice);

    FrameData frameData = {0};
    frameData.viewProjection = se_f4x4_transposed(debug_camera_get_view_projection(&camera));
    void* addr = renderInterface->memory_buffer_get_mapped_address(frameDataBuffer);
    memcpy(addr, &frameData, sizeof(frameData));

    //
    // Flat pass
    //
    {
        SeRenderObject* bindingsSet0[] = { frameDataBuffer, };
        SeResourceSetCreateInfo set0CreateInfo = (SeResourceSetCreateInfo)
        {
            .device         = renderDevice,
            .pipeline       = flatRenderPipeline,
            .set            = 0,
            .bindings       = bindingsSet0,
            .numBindings    = se_array_size(bindingsSet0),
        };
        SeRenderObject* resourceSet0 = renderInterface->resource_set_create(&set0CreateInfo);
        SeRenderObject* bindingsSet1[] = { verticesBuffer, instanceDataBuffer };
        SeResourceSetCreateInfo set1CreateInfo = (SeResourceSetCreateInfo)
        {
            .device         = renderDevice,
            .pipeline       = flatRenderPipeline,
            .set            = 1,
            .bindings       = bindingsSet1,
            .numBindings    = se_array_size(bindingsSet1),
        };
        SeRenderObject* resourceSet1 = renderInterface->resource_set_create(&set1CreateInfo);
        {
            SeCommandBufferRequestInfo cmdRequest = (SeCommandBufferRequestInfo) { .device = renderDevice, .usage = SE_COMMAND_BUFFER_USAGE_GRAPHICS, };
            SeRenderObject* cmd = renderInterface->command_buffer_request(&cmdRequest);
            SeCommandBindPipelineInfo bindPipeline = (SeCommandBindPipelineInfo) { .pipeline = flatRenderPipeline, .framebuffer = flatFramebuffer };
            renderInterface->command_bind_pipeline(cmd, &bindPipeline);
            SeCommandBindResourceSetInfo bindSet0 = (SeCommandBindResourceSetInfo) { resourceSet0 };
            SeCommandBindResourceSetInfo bindSet1 = (SeCommandBindResourceSetInfo) { resourceSet1 };
            renderInterface->command_bind_resource_set(cmd, &bindSet0);
            renderInterface->command_bind_resource_set(cmd, &bindSet1);
            SeCommandDrawInfo draw = (SeCommandDrawInfo) { .numVertices = 6, .numInstances = 3 };
            renderInterface->command_draw(cmd, &draw);
            renderInterface->command_buffer_submit(cmd);
        }
        resourceSet0->destroy(resourceSet0);
        resourceSet1->destroy(resourceSet1);
    }
    //
    // Present pass
    //
    {
        SeRenderObject* bindingsSet0[] = { flatColorTexture, };
        SeResourceSetCreateInfo set0CreateInfo = (SeResourceSetCreateInfo)
        {
            .device         = renderDevice,
            .pipeline       = presentRenderPipeline,
            .set            = 0,
            .bindings       = bindingsSet0,
            .numBindings    = se_array_size(bindingsSet0),
        };
        SeRenderObject* resourceSet0 = renderInterface->resource_set_create(&set0CreateInfo);
        {
            SeRenderObject* framebuffer = presentFramebuffers[renderInterface->get_active_swap_chain_texture_index(renderDevice)];
            SeCommandBufferRequestInfo cmdRequest = (SeCommandBufferRequestInfo) { .device = renderDevice, .usage = SE_COMMAND_BUFFER_USAGE_GRAPHICS, };
            SeRenderObject* cmd = renderInterface->command_buffer_request(&cmdRequest);
            SeCommandBindPipelineInfo bindPipeline = (SeCommandBindPipelineInfo) { .pipeline = presentRenderPipeline, .framebuffer = framebuffer };
            renderInterface->command_bind_pipeline(cmd, &bindPipeline);
            SeCommandBindResourceSetInfo bindSet0 = (SeCommandBindResourceSetInfo) { resourceSet0 };
            renderInterface->command_bind_resource_set(cmd, &bindSet0);
            SeCommandDrawInfo draw = (SeCommandDrawInfo) { .numVertices = 3, .numInstances = 1 };
            renderInterface->command_draw(cmd, &draw);
            renderInterface->command_buffer_submit(cmd);
        }
        resourceSet0->destroy(resourceSet0);
    }
    renderInterface->end_frame(renderDevice);
}
