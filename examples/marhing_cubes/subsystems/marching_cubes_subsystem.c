
#include "marching_cubes_subsystem.h"
#include "engine/engine.h"

#define CHUNK_SIZE (64 * 64)

SeWindowSubsystemInterface* windowIface;
SeApplicationAllocatorsSubsystemInterface* allocatorsIface;
SeRenderAbstractionSubsystemInterface* renderIface;
SePlatformInterface* platformIface;

SeWindowHandle window;
SeRenderObject* renderDevice;
SeRenderObject* generateChunkCs;
SeRenderObject* triangulateChunkCs;
SeRenderObject* renderChunkVs;
SeRenderObject* renderChunkFs;

SeRenderObject* sync_load_shader(const char* path)
{
    SeFile shader = {0};
    SeFileContent content = {0};
    platformIface->file_load(&shader, path, SE_FILE_READ);
    platformIface->file_read(&content, &shader, allocatorsIface->frameAllocator);
    SeRenderProgramCreateInfo createInfo = (SeRenderProgramCreateInfo)
    {
        .device = renderDevice,
        .bytecode = (uint32_t*)content.memory,
        .codeSizeBytes = content.size,
    };
    SeRenderObject* program = renderIface->program_create(&createInfo);
    platformIface->file_free_content(&content);
    platformIface->file_unload(&shader);
    return program;
}

SE_DLL_EXPORT void se_init(SabrinaEngine* engine)
{
    windowIface = (SeWindowSubsystemInterface*)engine->find_subsystem_interface(engine, SE_WINDOW_SUBSYSTEM_NAME);
    allocatorsIface = (SeApplicationAllocatorsSubsystemInterface*)engine->find_subsystem_interface(engine, SE_APPLICATION_ALLOCATORS_SUBSYSTEM_NAME);
    renderIface = (SeRenderAbstractionSubsystemInterface*)engine->find_subsystem_interface(engine, SE_VULKAN_RENDER_SUBSYSTEM_NAME);
    platformIface = &engine->platformIface;
    //
    // Create window
    //
    {
        SeWindowSubsystemCreateInfo createInfo = (SeWindowSubsystemCreateInfo)
        {
            .name           = "Sabrina engine - marching cubes example",
            .isFullscreen   = false,
            .isResizable    = true,
            .width          = 640,
            .height         = 480,
        };
        window = windowIface->create(&createInfo);
    }
    //
    // Create device
    //
    {
        SeRenderDeviceCreateInfo createInfo = (SeRenderDeviceCreateInfo)
        {
            .window                 = &window,
            .persistentAllocator    = allocatorsIface->persistentAllocator,
            .frameAllocator         = allocatorsIface->frameAllocator,
            .platform               = platformIface,
        };
        renderDevice = renderIface->device_create(&createInfo);
    }
    //
    // Load shaders
    //
    {
        generateChunkCs     = sync_load_shader("assets/application/shaders/generate_chunk.comp.spv");
        triangulateChunkCs  = sync_load_shader("assets/application/shaders/triangulate_chunk.comp.spv");
        renderChunkVs       = sync_load_shader("assets/application/shaders/render_chunk.vert.spv");
        renderChunkFs       = sync_load_shader("assets/application/shaders/render_chunk.frag.spv");
    }
    //
    // Create pipelines
    //
    {

    }
}

SE_DLL_EXPORT void se_terminate(SabrinaEngine* engine)
{
    renderIface->device_wait(renderDevice);

    renderDevice->destroy(renderDevice);
    generateChunkCs->destroy(generateChunkCs);
    triangulateChunkCs->destroy(triangulateChunkCs);
    renderChunkVs->destroy(renderChunkVs);
    renderChunkFs->destroy(renderChunkFs);

    windowIface->destroy(window);
}

