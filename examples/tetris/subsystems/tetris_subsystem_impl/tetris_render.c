
#include <string.h>
#include "tetris_render.h"
#include "tetris_controller.h"
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

typedef struct InputIndex
{
    uint32_t index;
    float pad[3];
} InputIndex;

typedef struct InputInstance
{
    SeFloat4x4 trfWS;
    SeFloat4 tint;
} InputInstance;

SeRenderObject* tetris_render_sync_load_shader(TetrisRenderState* renderState, const char* path)
{
    SeFile shader = {0};
    SeFileContent content = {0};
    renderState->platformInterface->file_load(&shader, path, SE_FILE_READ);
    renderState->platformInterface->file_read(&content, &shader, renderState->allocatorsInterface->frameAllocator);
    SeRenderProgramCreateInfo createInfo = (SeRenderProgramCreateInfo)
    {
        .device = renderState->renderDevice,
        .bytecode = (uint32_t*)content.memory,
        .codeSizeBytes = content.size,
    };
    SeRenderObject* program = renderState->renderInterface->program_create(&createInfo);
    renderState->platformInterface->file_free_content(&content);
    renderState->platformInterface->file_unload(&shader);
    return program;
}

void tetris_render_init(TetrisRenderInitInfo* initInfo)
{
    TetrisRenderState* renderState = initInfo->renderState;
    renderState->renderInterface = (SeRenderAbstractionSubsystemInterface*)initInfo->engine->find_subsystem_interface(initInfo->engine, SE_VULKAN_RENDER_SUBSYSTEM_NAME);
    renderState->allocatorsInterface = (SeApplicationAllocatorsSubsystemInterface*)initInfo->engine->find_subsystem_interface(initInfo->engine, SE_APPLICATION_ALLOCATORS_SUBSYSTEM_NAME);
    renderState->windowInterface = (SeWindowSubsystemInterface*)initInfo->engine->find_subsystem_interface(initInfo->engine, SE_WINDOW_SUBSYSTEM_NAME);
    renderState->platformInterface = &initInfo->engine->platformIface;
    //
    // Create render device
    //
    {
        SeRenderDeviceCreateInfo createInfo = (SeRenderDeviceCreateInfo)
        {
            .window                 = initInfo->window,
            .persistentAllocator    = renderState->allocatorsInterface->persistentAllocator,
            .frameAllocator         = renderState->allocatorsInterface->frameAllocator,
        };
        renderState->renderDevice = renderState->renderInterface->device_create(&createInfo);
    }
    //
    // Init draw pipline
    //
    {
        renderState->drawVs = tetris_render_sync_load_shader(renderState, "assets/application/shaders/tetris_draw.vert.spv");
        renderState->drawFs = tetris_render_sync_load_shader(renderState, "assets/application/shaders/tetris_draw.frag.spv");
        SeRenderObject* swapChainTexture = renderState->renderInterface->get_swap_chain_texture(renderState->renderDevice, 0);
        {
            SeTextureCreateInfo createInfo = (SeTextureCreateInfo)
            {
                .device = renderState->renderDevice,
                .width  = renderState->renderInterface->texture_get_width(swapChainTexture),
                .height = renderState->renderInterface->texture_get_height(swapChainTexture),
                .usage  = SE_TEXTURE_USAGE_RENDER_PASS_ATTACHMENT | SE_TEXTURE_USAGE_TEXTURE,
                .format = SE_TEXTURE_FORMAT_RGBA_8,
            };
            renderState->drawColorTexture = renderState->renderInterface->texture_create(&createInfo);
        }
        {
            SeTextureCreateInfo createInfo = (SeTextureCreateInfo)
            {
                .device = renderState->renderDevice,
                .width  = renderState->renderInterface->texture_get_width(swapChainTexture),
                .height = renderState->renderInterface->texture_get_height(swapChainTexture),
                .usage  = SE_TEXTURE_USAGE_RENDER_PASS_ATTACHMENT,
                .format = SE_TEXTURE_FORMAT_DEPTH_STENCIL,
            };
            renderState->drawDepthTexture = renderState->renderInterface->texture_create(&createInfo);
        }
        {
            SeRenderPassAttachment colorAttachments[] =
            {
                {
                    .format     = SE_TEXTURE_FORMAT_RGBA_8,
                    .loadOp     = SE_ATTACHMENT_LOAD_OP_CLEAR,
                    .storeOp    = SE_ATTACHMENT_STORE_OP_STORE,
                    .sampling   = SE_SAMPLING_1,
                },
            };
            SeRenderPassAttachment depthAttachment = (SeRenderPassAttachment)
            {
                .format     = SE_TEXTURE_FORMAT_DEPTH_STENCIL,
                .loadOp     = SE_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp    = SE_ATTACHMENT_STORE_OP_STORE,
                .sampling   = SE_SAMPLING_1,
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
                .device                 = renderState->renderDevice,
            };
            renderState->drawRenderPass = renderState->renderInterface->render_pass_create(&createInfo);
        }
        {
            SeDepthTestState depthTest = (SeDepthTestState)
            {
                .isTestEnabled  = true,
                .isWriteEnabled = true,
            };
            SeGraphicsRenderPipelineCreateInfo createInfo = (SeGraphicsRenderPipelineCreateInfo)
            {
                .device                 = renderState->renderDevice,
                .renderPass             = renderState->drawRenderPass,
                .vertexProgram          = renderState->drawVs,
                .fragmentProgram        = renderState->drawFs,
                .frontStencilOpState    = NULL,
                .backStencilOpState     = NULL,
                .depthTestState         = &depthTest,
                .subpassIndex           = 0,
                .poligonMode            = SE_PIPELINE_POLIGON_FILL_MODE_FILL,
                .cullMode               = SE_PIPELINE_CULL_MODE_BACK,
                .frontFace              = SE_PIPELINE_FRONT_FACE_CLOCKWISE,
                .samplingType           = SE_SAMPLING_1,
            };
            renderState->drawRenderPipeline = renderState->renderInterface->render_pipeline_graphics_create(&createInfo);
        }
        {
            SeRenderObject* attachments[] = { renderState->drawColorTexture, renderState->drawDepthTexture };
            SeFramebufferCreateInfo createInfo = (SeFramebufferCreateInfo)
            {
                .attachmentsPtr = attachments,
                .numAttachments = se_array_size(attachments),
                .renderPass     = renderState->drawRenderPass,
                .device         = renderState->renderDevice,
            };
            renderState->drawFramebuffer = renderState->renderInterface->framebuffer_create(&createInfo);
        }
    }
    //
    // Init present pipline
    //
    {
        renderState->presentVs = tetris_render_sync_load_shader(renderState, "assets/default/shaders/present.vert.spv");
        renderState->presentFs = tetris_render_sync_load_shader(renderState, "assets/default/shaders/present.frag.spv");
        SeRenderObject* swapChainTexture = renderState->renderInterface->get_swap_chain_texture(renderState->renderDevice, 0);
        {
            SeRenderPassAttachment colorAttachments[] =
            {
                {
                    .format     = SE_TEXTURE_FORMAT_SWAP_CHAIN,
                    .loadOp     = SE_ATTACHMENT_LOAD_OP_CLEAR,
                    .storeOp    = SE_ATTACHMENT_STORE_OP_STORE,
                    .sampling   = SE_SAMPLING_1,
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
                .device                 = renderState->renderDevice,
            };
            renderState->presentRenderPass = renderState->renderInterface->render_pass_create(&createInfo);
        }
        {
            SeDepthTestState depthTest = (SeDepthTestState)
            {
                .isTestEnabled          = false,
                .isWriteEnabled         = false,
            };
            SeGraphicsRenderPipelineCreateInfo createInfo = (SeGraphicsRenderPipelineCreateInfo)
            {
                .device                 = renderState->renderDevice,
                .renderPass             = renderState->presentRenderPass,
                .vertexProgram          = renderState->presentVs,
                .fragmentProgram        = renderState->presentFs,
                .frontStencilOpState    = NULL,
                .backStencilOpState     = NULL,
                .depthTestState         = &depthTest,
                .subpassIndex           = 0,
                .poligonMode            = SE_PIPELINE_POLIGON_FILL_MODE_FILL,
                .cullMode               = SE_PIPELINE_CULL_MODE_NONE,
                .frontFace              = SE_PIPELINE_FRONT_FACE_CLOCKWISE,
                .samplingType      = SE_SAMPLING_1,
            };
            renderState->presentRenderPipeline = renderState->renderInterface->render_pipeline_graphics_create(&createInfo);
        }
        {
            const size_t numSwapChainTextures = renderState->renderInterface->get_swap_chain_textures_num(renderState->renderDevice);
            se_sbuffer_construct(renderState->presentFramebuffers, numSwapChainTextures, renderState->allocatorsInterface->persistentAllocator);
            for (size_t it = 0; it < numSwapChainTextures; it++)
            {
                SeRenderObject* attachments[] = { renderState->renderInterface->get_swap_chain_texture(renderState->renderDevice, it) };
                SeFramebufferCreateInfo createInfo = (SeFramebufferCreateInfo)
                {
                    .attachmentsPtr = attachments,
                    .numAttachments = se_array_size(attachments),
                    .renderPass     = renderState->presentRenderPass,
                    .device         = renderState->renderDevice,
                };
                SeRenderObject* fb = renderState->renderInterface->framebuffer_create(&createInfo);
                se_sbuffer_push(renderState->presentFramebuffers, fb);
            }
        }
    }
    //
    // Fill buffers
    //
    {
        //
        // Frame data buffer
        //
        {
            SeMemoryBufferCreateInfo memoryBufferCreateInfo = (SeMemoryBufferCreateInfo)
            {
                .device     = renderState->renderDevice,
                .size       = sizeof(FrameData),
                .usage      = SE_MEMORY_BUFFER_USAGE_TRANSFER_DST | SE_MEMORY_BUFFER_USAGE_UNIFORM_BUFFER,
                .visibility = SE_MEMORY_BUFFER_VISIBILITY_GPU_AND_CPU,
            };
            renderState->frameDataBuffer = renderState->renderInterface->memory_buffer_create(&memoryBufferCreateInfo);
            SeFloat4x4 view = se_f4x4_mul_f4x4
            (
                se_f4x4_translation((SeFloat3){ (float)TETRIS_FIELD_WIDTH / 2.0f, -3, -20 }),
                se_f4x4_rotation((SeFloat3){ -30, 0, 0 })
            );
            SeFloat4x4 projection;
            renderState->renderInterface->perspective_projection_matrix
            (
                &projection,
                60,
                ((float)renderState->windowInterface->get_width(*initInfo->window)) / ((float)renderState->windowInterface->get_height(*initInfo->window)),
                0.1f,
                100.0f
            );
            SeFloat4x4 vp = se_f4x4_transposed(se_f4x4_mul_f4x4
            (
                projection,
                se_f4x4_inverted(view)
            ));
            void* addr = renderState->renderInterface->memory_buffer_get_mapped_address(renderState->frameDataBuffer);
            memcpy(addr, &vp, sizeof(vp));
        }
        //
        // Cube vertices (filled ones at init)
        //
        {
            InputVertex verts[] =
            {
                // Bottom side
                { .positionLS = { -0.5f , -0.5f ,  0.5f }, .uv = { 0, 0, }, },
                { .positionLS = { -0.5f , -0.5f , -0.5f }, .uv = { 0, 1, }, },
                { .positionLS = {  0.5f , -0.5f , -0.5f }, .uv = { 1, 1, }, },
                { .positionLS = {  0.5f , -0.5f ,  0.5f }, .uv = { 1, 0, }, },
                // Top side
                { .positionLS = { -0.5f ,  0.5f , -0.5f }, .uv = { 0, 0, }, },
                { .positionLS = { -0.5f ,  0.5f ,  0.5f }, .uv = { 0, 1, }, },
                { .positionLS = {  0.5f ,  0.5f ,  0.5f }, .uv = { 1, 1, }, },
                { .positionLS = {  0.5f ,  0.5f , -0.5f }, .uv = { 1, 0, }, },
                // Close side
                { .positionLS = { -0.5f , -0.5f , -0.5f }, .uv = { 0, 0, }, },
                { .positionLS = { -0.5f ,  0.5f , -0.5f }, .uv = { 0, 1, }, },
                { .positionLS = {  0.5f ,  0.5f , -0.5f }, .uv = { 1, 1, }, },
                { .positionLS = {  0.5f , -0.5f , -0.5f }, .uv = { 1, 0, }, },
                // Far side
                { .positionLS = {  0.5f , -0.5f , 0.5f }, .uv = { 0, 0, }, },
                { .positionLS = {  0.5f ,  0.5f , 0.5f }, .uv = { 0, 1, }, },
                { .positionLS = { -0.5f ,  0.5f , 0.5f }, .uv = { 1, 1, }, },
                { .positionLS = { -0.5f , -0.5f , 0.5f }, .uv = { 1, 0, }, },
                // Left side
                { .positionLS = { -0.5f , -0.5f ,  0.5f }, .uv = { 0, 0, }, },
                { .positionLS = { -0.5f ,  0.5f ,  0.5f }, .uv = { 0, 1, }, },
                { .positionLS = { -0.5f ,  0.5f , -0.5f }, .uv = { 1, 1, }, },
                { .positionLS = { -0.5f , -0.5f , -0.5f }, .uv = { 1, 0, }, },
                // Right side
                { .positionLS = {  0.5f , -0.5f , -0.5f }, .uv = { 0, 0, }, },
                { .positionLS = {  0.5f ,  0.5f , -0.5f }, .uv = { 0, 1, }, },
                { .positionLS = {  0.5f ,  0.5f ,  0.5f }, .uv = { 1, 1, }, },
                { .positionLS = {  0.5f , -0.5f ,  0.5f }, .uv = { 1, 0, }, },
            };
            SeMemoryBufferCreateInfo memoryBufferCreateInfo = (SeMemoryBufferCreateInfo)
            {
                .device     = renderState->renderDevice,
                .size       = sizeof(verts),
                .usage      = SE_MEMORY_BUFFER_USAGE_TRANSFER_DST | SE_MEMORY_BUFFER_USAGE_STORAGE_BUFFER,
                .visibility = SE_MEMORY_BUFFER_VISIBILITY_GPU_AND_CPU,
            };
            renderState->cubeVerticesBuffer = renderState->renderInterface->memory_buffer_create(&memoryBufferCreateInfo);
            void* addr = renderState->renderInterface->memory_buffer_get_mapped_address(renderState->cubeVerticesBuffer);
            memcpy(addr, verts, sizeof(verts));
        }
        //
        // Cude indices (filled ones at init)
        //
        {
            InputIndex indices[] =
            {
                { .index = 0      }, { .index = 1      }, { .index = 2      }, { .index = 0      }, { .index = 2      }, { .index = 3      }, // Bottom side
                { .index = 0 + 4  }, { .index = 1 + 4  }, { .index = 2 + 4  }, { .index = 0 + 4  }, { .index = 2 + 4  }, { .index = 3 + 4  }, // Top side
                { .index = 0 + 8  }, { .index = 1 + 8  }, { .index = 2 + 8  }, { .index = 0 + 8  }, { .index = 2 + 8  }, { .index = 3 + 8  }, // Close side
                { .index = 0 + 12 }, { .index = 1 + 12 }, { .index = 2 + 12 }, { .index = 0 + 12 }, { .index = 2 + 12 }, { .index = 3 + 12 }, // Far side
                { .index = 0 + 16 }, { .index = 1 + 16 }, { .index = 2 + 16 }, { .index = 0 + 16 }, { .index = 2 + 16 }, { .index = 3 + 16 }, // Left side
                { .index = 0 + 20 }, { .index = 1 + 20 }, { .index = 2 + 20 }, { .index = 0 + 20 }, { .index = 2 + 20 }, { .index = 3 + 20 }, // Right side
            };
            SeMemoryBufferCreateInfo memoryBufferCreateInfo = (SeMemoryBufferCreateInfo)
            {
                .device     = renderState->renderDevice,
                .size       = sizeof(indices),
                .usage      = SE_MEMORY_BUFFER_USAGE_TRANSFER_DST | SE_MEMORY_BUFFER_USAGE_STORAGE_BUFFER,
                .visibility = SE_MEMORY_BUFFER_VISIBILITY_GPU_AND_CPU,
            };
            renderState->cubeIndicesBuffer = renderState->renderInterface->memory_buffer_create(&memoryBufferCreateInfo);
            void* addr = renderState->renderInterface->memory_buffer_get_mapped_address(renderState->cubeIndicesBuffer);
            memcpy(addr, indices, sizeof(indices));
        }
        //
        // Cube instances (filled every frame)
        //
        {
            SeMemoryBufferCreateInfo memoryBufferCreateInfo = (SeMemoryBufferCreateInfo)
            {
                .device     = renderState->renderDevice,
                .size       = TETRIS_FIELD_WIDTH * TETRIS_FIELD_HEIGHT * sizeof(InputInstance),
                .usage      = SE_MEMORY_BUFFER_USAGE_TRANSFER_DST | SE_MEMORY_BUFFER_USAGE_STORAGE_BUFFER,
                .visibility = SE_MEMORY_BUFFER_VISIBILITY_GPU_AND_CPU,
            };
            const size_t numSwapChainTextures = renderState->renderInterface->get_swap_chain_textures_num(renderState->renderDevice);
            se_sbuffer_construct(renderState->cubeInstanceBuffers, numSwapChainTextures, renderState->allocatorsInterface->persistentAllocator);
            se_sbuffer_set_size(renderState->cubeInstanceBuffers, numSwapChainTextures);
            for (size_t it = 0; it < numSwapChainTextures; it++)
                renderState->cubeInstanceBuffers[it] = renderState->renderInterface->memory_buffer_create(&memoryBufferCreateInfo);
        }
        //
        // Grid vertices (filled ones at init)
        //
        {
            InputVertex verts[] =
            {
                // Bottom line
                { .positionLS = { -0.5f , -0.5f ,  0.5f }, .uv = { 0, 0, }, },
                { .positionLS = { -0.45f, -0.45f,  0.5f }, .uv = { 0, 1, }, },
                { .positionLS = {  0.45f, -0.45f,  0.5f }, .uv = { 1, 1, }, },
                { .positionLS = {  0.5f , -0.5f ,  0.5f }, .uv = { 1, 0, }, },
                // Top line
                { .positionLS = { -0.45f,  0.45f,  0.5f }, .uv = { 0, 0, }, },
                { .positionLS = { -0.5f ,  0.5f ,  0.5f }, .uv = { 0, 1, }, },
                { .positionLS = {  0.5f ,  0.5f ,  0.5f }, .uv = { 1, 1, }, },
                { .positionLS = {  0.45f,  0.45f,  0.5f }, .uv = { 1, 0, }, },
                // Left line
                { .positionLS = { -0.5f , -0.5f ,  0.5f }, .uv = { 0, 0, }, },
                { .positionLS = { -0.5f ,  0.5f ,  0.5f }, .uv = { 0, 1, }, },
                { .positionLS = { -0.45f,  0.45f,  0.5f }, .uv = { 1, 1, }, },
                { .positionLS = { -0.45f, -0.45f,  0.5f }, .uv = { 1, 0, }, },
                // Right line
                { .positionLS = {  0.45f, -0.45f, 0.5f }, .uv = { 0, 0, }, },
                { .positionLS = {  0.45f,  0.45f, 0.5f }, .uv = { 0, 1, }, },
                { .positionLS = {  0.5f ,  0.5f , 0.5f }, .uv = { 1, 1, }, },
                { .positionLS = {  0.5f , -0.5f , 0.5f }, .uv = { 1, 0, }, },
            };
            SeMemoryBufferCreateInfo memoryBufferCreateInfo = (SeMemoryBufferCreateInfo)
            {
                .device     = renderState->renderDevice,
                .size       = sizeof(verts),
                .usage      = SE_MEMORY_BUFFER_USAGE_TRANSFER_DST | SE_MEMORY_BUFFER_USAGE_STORAGE_BUFFER,
                .visibility = SE_MEMORY_BUFFER_VISIBILITY_GPU_AND_CPU,
            };
            renderState->gridVerticesBuffer = renderState->renderInterface->memory_buffer_create(&memoryBufferCreateInfo);
            void* addr = renderState->renderInterface->memory_buffer_get_mapped_address(renderState->gridVerticesBuffer);
            memcpy(addr, verts, sizeof(verts));
        }
        //
        // Grid indices (filled ones at init)
        //
        {
            InputIndex indices[] =
            {
                { .index = 0      }, { .index = 1      }, { .index = 2      }, { .index = 0      }, { .index = 2      }, { .index = 3      }, // Bottom line
                { .index = 0 + 4  }, { .index = 1 + 4  }, { .index = 2 + 4  }, { .index = 0 + 4  }, { .index = 2 + 4  }, { .index = 3 + 4  }, // Top line
                { .index = 0 + 8  }, { .index = 1 + 8  }, { .index = 2 + 8  }, { .index = 0 + 8  }, { .index = 2 + 8  }, { .index = 3 + 8  }, // Left line
                { .index = 0 + 12 }, { .index = 1 + 12 }, { .index = 2 + 12 }, { .index = 0 + 12 }, { .index = 2 + 12 }, { .index = 3 + 12 }, // Right line
            };
            SeMemoryBufferCreateInfo memoryBufferCreateInfo = (SeMemoryBufferCreateInfo)
            {
                .device     = renderState->renderDevice,
                .size       = sizeof(indices),
                .usage      = SE_MEMORY_BUFFER_USAGE_TRANSFER_DST | SE_MEMORY_BUFFER_USAGE_STORAGE_BUFFER,
                .visibility = SE_MEMORY_BUFFER_VISIBILITY_GPU_AND_CPU,
            };
            renderState->gridIndicesBuffer = renderState->renderInterface->memory_buffer_create(&memoryBufferCreateInfo);
            void* addr = renderState->renderInterface->memory_buffer_get_mapped_address(renderState->gridIndicesBuffer);
            memcpy(addr, indices, sizeof(indices));
        }
        //
        // Grid instances (filled ones at init)
        //
        {
            SeMemoryBufferCreateInfo memoryBufferCreateInfo = (SeMemoryBufferCreateInfo)
            {
                .device     = renderState->renderDevice,
                .size       = TETRIS_FIELD_WIDTH * TETRIS_FIELD_HEIGHT * sizeof(InputInstance),
                .usage      = SE_MEMORY_BUFFER_USAGE_TRANSFER_DST | SE_MEMORY_BUFFER_USAGE_STORAGE_BUFFER,
                .visibility = SE_MEMORY_BUFFER_VISIBILITY_GPU_AND_CPU,
            };
            renderState->gridInstanceBuffer = renderState->renderInterface->memory_buffer_create(&memoryBufferCreateInfo);
            se_sbuffer(InputInstance) instances = NULL;
            se_sbuffer_construct(instances, TETRIS_FIELD_WIDTH * TETRIS_FIELD_HEIGHT, renderState->allocatorsInterface->frameAllocator);
            for (int x = 0; x < TETRIS_FIELD_WIDTH; x++)
                for (int y = 0; y < TETRIS_FIELD_HEIGHT; y++)
                {
                    SeTransform trf = SE_T_IDENTITY;
                    se_t_set_position(&trf, (SeFloat3){ x, y, 0 });
                    InputInstance instance = (InputInstance)
                    {
                        .trfWS = se_f4x4_transposed(trf.matrix),
                        .tint = (SeFloat4){ 0, 1, 1, 1 },
                    };
                    se_sbuffer_push(instances, instance);
                }
            void* addr = renderState->renderInterface->memory_buffer_get_mapped_address(renderState->gridInstanceBuffer);
            memcpy(addr, instances, TETRIS_FIELD_WIDTH * TETRIS_FIELD_HEIGHT * sizeof(InputInstance));
        }
    }
}

void tetris_render_terminate(TetrisRenderState* renderState)
{
    renderState->renderInterface->device_wait(renderState->renderDevice);

    for (size_t it = 0; it < se_sbuffer_size(renderState->cubeInstanceBuffers); it++)
        renderState->cubeInstanceBuffers[it]->destroy(renderState->cubeInstanceBuffers[it]);
    se_sbuffer_destroy(renderState->cubeInstanceBuffers);
    renderState->cubeIndicesBuffer->destroy(renderState->cubeIndicesBuffer);
    renderState->cubeVerticesBuffer->destroy(renderState->cubeVerticesBuffer);
    renderState->frameDataBuffer->destroy(renderState->frameDataBuffer);

    renderState->gridInstanceBuffer->destroy(renderState->gridInstanceBuffer);
    renderState->gridIndicesBuffer->destroy(renderState->gridIndicesBuffer);
    renderState->gridVerticesBuffer->destroy(renderState->gridVerticesBuffer);

    renderState->drawFramebuffer->destroy(renderState->drawFramebuffer);
    renderState->drawDepthTexture->destroy(renderState->drawDepthTexture);
    renderState->drawColorTexture->destroy(renderState->drawColorTexture);
    renderState->drawRenderPipeline->destroy(renderState->drawRenderPipeline);
    renderState->drawFs->destroy(renderState->drawFs);
    renderState->drawVs->destroy(renderState->drawVs);
    renderState->drawRenderPass->destroy(renderState->drawRenderPass);

    for (size_t it = 0; it < se_sbuffer_size(renderState->presentFramebuffers); it++)
        renderState->presentFramebuffers[it]->destroy(renderState->presentFramebuffers[it]);
    se_sbuffer_destroy(renderState->presentFramebuffers);
    renderState->presentRenderPipeline->destroy(renderState->presentRenderPipeline);
    renderState->presentFs->destroy(renderState->presentFs);
    renderState->presentVs->destroy(renderState->presentVs);
    renderState->presentRenderPass->destroy(renderState->presentRenderPass);

    renderState->renderDevice->destroy(renderState->renderDevice);
}

void tetris_render_update(TetrisRenderState* renderState, TetrisState* state, const SeWindowSubsystemInput* input, float dt)
{
    //
    // Fill instance buffer
    //
    const size_t activeSwapChainImageIndex = renderState->renderInterface->get_active_swap_chain_texture_index(renderState->renderDevice);
    uint32_t numCubeInstances = 0;
    SeRenderObject* cubeInstanceBuffer = renderState->cubeInstanceBuffers[activeSwapChainImageIndex];
    InputInstance* instancesPtr = (InputInstance*)renderState->renderInterface->memory_buffer_get_mapped_address(cubeInstanceBuffer);
    for (int x = 0; x < TETRIS_FIELD_WIDTH; x++)
        for (int y = 0; y < TETRIS_FIELD_HEIGHT; y++)
            if (state->field[x][y])
            {
                SeTransform trf = SE_T_IDENTITY;
                se_t_set_position(&trf, (SeFloat3){ x, y, 0 });
                instancesPtr[numCubeInstances++] = (InputInstance)
                {
                    .trfWS = se_f4x4_transposed(trf.matrix),
                    .tint = state->field[x][y] == TETRIS_FILLED_ACTIVE_FIGURE_SQUARE
                        ? (SeFloat4){ 0, 1, 1, 1 }
                        : (SeFloat4){ 1, 1, 1, 1 },
                };
            }
    renderState->renderInterface->begin_frame(renderState->renderDevice);
    //
    // Draw pass
    //
    {
        SeRenderObject* bindingsSet0[] = { renderState->frameDataBuffer, };
        SeResourceSetCreateInfo set0CreateInfo = (SeResourceSetCreateInfo)
        {
            .device         = renderState->renderDevice,
            .pipeline       = renderState->drawRenderPipeline,
            .set            = 0,
            .bindings       = bindingsSet0,
            .numBindings    = se_array_size(bindingsSet0),
        };
        SeRenderObject* resourceSet0 = renderState->renderInterface->resource_set_create(&set0CreateInfo);
        SeRenderObject* bindingsSet1[] = { renderState->cubeVerticesBuffer, renderState->cubeIndicesBuffer, cubeInstanceBuffer };
        SeResourceSetCreateInfo set1CreateInfo = (SeResourceSetCreateInfo)
        {
            .device         = renderState->renderDevice,
            .pipeline       = renderState->drawRenderPipeline,
            .set            = 1,
            .bindings       = bindingsSet1,
            .numBindings    = se_array_size(bindingsSet1),
        };
        SeRenderObject* resourceSet1 = renderState->renderInterface->resource_set_create(&set1CreateInfo);
        SeRenderObject* bindingsSet2[] = { renderState->gridVerticesBuffer, renderState->gridIndicesBuffer, renderState->gridInstanceBuffer };
        SeResourceSetCreateInfo set2CreateInfo = (SeResourceSetCreateInfo)
        {
            .device         = renderState->renderDevice,
            .pipeline       = renderState->drawRenderPipeline,
            .set            = 1,
            .bindings       = bindingsSet2,
            .numBindings    = se_array_size(bindingsSet2),
        };
        SeRenderObject* resourceSet2 = renderState->renderInterface->resource_set_create(&set2CreateInfo);
        {
            SeCommandBufferRequestInfo cmdRequest = (SeCommandBufferRequestInfo) { .device = renderState->renderDevice, .usage = SE_COMMAND_BUFFER_USAGE_GRAPHICS, };
            SeRenderObject* cmd = renderState->renderInterface->command_buffer_request(&cmdRequest);
            SeCommandBindPipelineInfo bindPipeline = (SeCommandBindPipelineInfo) { .pipeline = renderState->drawRenderPipeline, .framebuffer = renderState->drawFramebuffer };
            renderState->renderInterface->command_bind_pipeline(cmd, &bindPipeline);
            SeCommandBindResourceSetInfo bindSet0 = (SeCommandBindResourceSetInfo) { resourceSet0 };
            SeCommandBindResourceSetInfo bindSet1 = (SeCommandBindResourceSetInfo) { resourceSet1 };
            renderState->renderInterface->command_bind_resource_set(cmd, &bindSet0);
            renderState->renderInterface->command_bind_resource_set(cmd, &bindSet1);
            SeCommandDrawInfo drawCubes = (SeCommandDrawInfo) { .numVertices = 36, .numInstances = numCubeInstances };
            renderState->renderInterface->command_draw(cmd, &drawCubes);
            SeCommandBindResourceSetInfo bindSet2 = (SeCommandBindResourceSetInfo) { resourceSet2 };
            renderState->renderInterface->command_bind_resource_set(cmd, &bindSet2);
            SeCommandDrawInfo drawGrid = (SeCommandDrawInfo) { .numVertices = 24, .numInstances = TETRIS_FIELD_WIDTH * TETRIS_FIELD_HEIGHT };
            renderState->renderInterface->command_draw(cmd, &drawGrid);
            renderState->renderInterface->command_buffer_submit(cmd);
        }
        resourceSet0->destroy(resourceSet0);
        resourceSet1->destroy(resourceSet1);
    }
    //
    // Present pass
    //
    {
        SeRenderObject* bindingsSet0[] = { renderState->drawColorTexture, };
        SeResourceSetCreateInfo set0CreateInfo = (SeResourceSetCreateInfo)
        {
            .device         = renderState->renderDevice,
            .pipeline       = renderState->presentRenderPipeline,
            .set            = 0,
            .bindings       = bindingsSet0,
            .numBindings    = se_array_size(bindingsSet0),
        };
        SeRenderObject* resourceSet0 = renderState->renderInterface->resource_set_create(&set0CreateInfo);
        {
            SeRenderObject* framebuffer = renderState->presentFramebuffers[activeSwapChainImageIndex];
            SeCommandBufferRequestInfo cmdRequest = (SeCommandBufferRequestInfo) { .device = renderState->renderDevice, .usage = SE_COMMAND_BUFFER_USAGE_GRAPHICS, };
            SeRenderObject* cmd = renderState->renderInterface->command_buffer_request(&cmdRequest);
            SeCommandBindPipelineInfo bindPipeline = (SeCommandBindPipelineInfo) { .pipeline = renderState->presentRenderPipeline, .framebuffer = framebuffer };
            renderState->renderInterface->command_bind_pipeline(cmd, &bindPipeline);
            SeCommandBindResourceSetInfo bindSet0 = (SeCommandBindResourceSetInfo) { resourceSet0 };
            renderState->renderInterface->command_bind_resource_set(cmd, &bindSet0);
            SeCommandDrawInfo draw = (SeCommandDrawInfo) { .numVertices = 3, .numInstances = 1 };
            renderState->renderInterface->command_draw(cmd, &draw);
            renderState->renderInterface->command_buffer_submit(cmd);
        }
        resourceSet0->destroy(resourceSet0);
    }
    renderState->renderInterface->end_frame(renderState->renderDevice);
}
