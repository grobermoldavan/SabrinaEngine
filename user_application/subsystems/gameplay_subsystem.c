
#include <stdio.h>

#include "gameplay_subsystem.h"

#define SE_CONTAINERS_IMPL
#include "engine/engine.h"

SeWindowSubsystemInterface* windowInterface;
SeRenderAbstractionSubsystemInterface* renderInterface;
SeApplicationAllocatorsSubsystemInterface* allocatorsInterface;

SeWindowHandle windowHandle;
SeRenderObject* renderDevice;
SeRenderObject* renderPass;
SeRenderObject* vs;
SeRenderObject* fs;
SeRenderObject* renderPipeline;
se_sbuffer(SeRenderObject*) framebuffers;

SE_DLL_EXPORT void se_load(struct SabrinaEngine* engine)
{
    printf("Load\n");
}

SE_DLL_EXPORT void se_unload(struct SabrinaEngine* engine)
{
    printf("Unload\n");
}

SE_DLL_EXPORT void se_init(struct SabrinaEngine* engine)
{
    printf("Init\n");

    windowInterface = (SeWindowSubsystemInterface*)engine->find_subsystem_interface(engine, SE_WINDOW_SUBSYSTEM_NAME);
    renderInterface = (SeRenderAbstractionSubsystemInterface*)engine->find_subsystem_interface(engine, SE_VULKAN_RENDER_SUBSYSTEM_NAME);
    allocatorsInterface = (SeApplicationAllocatorsSubsystemInterface*)engine->find_subsystem_interface(engine, SE_APPLICATION_ALLOCATORS_SUBSYSTEM_NAME);

    SePlatformInterface* platformInterface = &engine->platformIface;
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
    {
        SeRenderPassAttachment attachments[] =
        {
            (SeRenderPassAttachment)
            {
                .format     = SE_TEXTURE_FORMAT_SWAP_CHAIN,
                .loadOp     = SE_LOAD_CLEAR,
                .storeOp    = SE_STORE_STORE,
                .samples    = SE_SAMPLE_1,
            }
        };
        uint32_t colorRefs[] = { 0 };
        SeRenderPassSubpass subpasses[] =
        {
            (SeRenderPassSubpass)
            {
                .colorRefs      = colorRefs,
                .numColorRefs   = se_array_size(colorRefs),
                .inputRefs      = NULL,
                .numInputRefs   = 0,
                .resolveRefs    = NULL,
                .numResolveRefs = 0,
                .depthOp        = SE_DEPTH_NOTHING,
            }
        };
        SeRenderPassCreateInfo createInfo = (SeRenderPassCreateInfo)
        {
            .subpasses              = subpasses,
            .numSubpasses           = se_array_size(subpasses),
            .colorAttachments       = attachments,
            .numColorAttachments    = se_array_size(attachments),
            .depthStencilAttachment = NULL,
            .device                 = renderDevice,
        };
        renderPass = renderInterface->render_pass_create(&createInfo);
    }
    {
        SeFile shader = {0};
        SeFileContent content = {0};
        platformInterface->file_load(&shader, "../assets/shaders/simple.vert.spv", SE_FILE_READ);
        platformInterface->file_read(&content, &shader, allocatorsInterface->frameAllocator);
        SeRenderProgramCreateInfo createInfo = (SeRenderProgramCreateInfo)
        {
            .device = renderDevice,
            .bytecode = (uint32_t*)content.memory,
            .codeSizeBytes = content.size,
        };
        vs = renderInterface->program_create(&createInfo);
        platformInterface->file_free_content(&content);
        platformInterface->file_unload(&shader);
    }
    {
        SeFile shader = {0};
        SeFileContent content = {0};
        platformInterface->file_load(&shader, "../assets/shaders/simple.frag.spv", SE_FILE_READ);
        platformInterface->file_read(&content, &shader, allocatorsInterface->frameAllocator);
        SeRenderProgramCreateInfo createInfo = (SeRenderProgramCreateInfo)
        {
            .device = renderDevice,
            .bytecode = (uint32_t*)content.memory,
            .codeSizeBytes = content.size,
        };
        fs = renderInterface->program_create(&createInfo);
        platformInterface->file_free_content(&content);
        platformInterface->file_unload(&shader);
    }
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
            .poligonMode            = SE_POLIGON_FILL,
            .cullMode               = SE_CULL_NONE,
            .frontFace              = SE_CLOCKWISE,
            .multisamplingType      = SE_SAMPLE_1,
        };
        renderPipeline = renderInterface->render_pipeline_graphics_create(&createInfo);
    }
    {
        size_t numSwapChainTextures = renderInterface->get_swap_chain_textures_num(renderDevice);
        se_sbuffer_construct(framebuffers, numSwapChainTextures, allocatorsInterface->persistentAllocator);
        for (size_t it = 0; it < numSwapChainTextures; it++)
        {
            SeRenderObject* attachments[] = { renderInterface->get_swap_chain_texture(renderDevice, it) };
            SeFramebufferCreateInfo createInfo = (SeFramebufferCreateInfo)
            {
                .attachmentsPtr = attachments,
                .numAttachments = 1,
                .renderPass     = renderPass,
                .device         = renderDevice,
            };
            SeRenderObject* fb = renderInterface->framebuffer_create(&createInfo);
            se_sbuffer_push(framebuffers, fb);
        }
    }
}

SE_DLL_EXPORT void se_terminate(struct SabrinaEngine* engine)
{
    printf("Terminate\n");

    renderInterface->device_wait(renderDevice);
    for (size_t it = 0; it < se_sbuffer_size(framebuffers); it++)
    {
        framebuffers[it]->destroy(framebuffers[it]);
    }
    renderPipeline->destroy(renderPipeline);
    vs->destroy(vs);
    fs->destroy(fs);
    renderPass->destroy(renderPass);
    renderDevice->destroy(renderDevice);
    windowInterface->destroy(windowHandle);
}

SE_DLL_EXPORT void se_update(struct SabrinaEngine* engine)
{
    const SeWindowSubsystemInput* input = windowInterface->get_input(windowHandle);
    if (input->isCloseButtonPressed || se_is_keyboard_button_pressed(input, SE_ESCAPE)) engine->shouldRun = false;

    renderInterface->begin_frame(renderDevice);
    {
        SeCommandBufferRequestInfo request = (SeCommandBufferRequestInfo)
        {
            .device = renderDevice,
            .usage = SE_USAGE_GRAPHICS,
        };
        SeRenderObject* cmd = renderInterface->command_buffer_request(&request);
        const size_t textureIndex = renderInterface->get_active_swap_chain_texture_index(renderDevice);
        SeCommandBindPipelineInfo bind = (SeCommandBindPipelineInfo)
        {
            .pipeline = renderPipeline,
            .framebuffer = framebuffers[textureIndex],
        };
        renderInterface->command_bind_pipeline(cmd, &bind);
        SeCommandDrawInfo draw = (SeCommandDrawInfo)
        {
            .vertexCount = 3,
        };
        renderInterface->command_draw(cmd, &draw);
        renderInterface->command_buffer_submit(cmd);
    }
    renderInterface->end_frame(renderDevice);
}
