
#include <stdio.h>

#include "gameplay_subsystem.h"
#include "engine/engine.h"

SeWindowHandle windowHandle;
SeRenderObject* renderDevice;

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
    SeApplicationAllocatorsSubsystemInterface* allocators = (SeApplicationAllocatorsSubsystemInterface*)engine->find_subsystem_interface(engine, SE_APPLICATION_ALLOCATORS_SUBSYSTEM_NAME);
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
        SeRenderAbstractionSubsystemInterface* renderInterface = (SeRenderAbstractionSubsystemInterface*)engine->find_subsystem_interface(engine, SE_VULKAN_RENDER_SUBSYSTEM_NAME);
        SeRenderDeviceCreateInfo createInfo = (SeRenderDeviceCreateInfo)
        {
            .window = &windowHandle,
            .persistentAllocator = allocators->appAllocator,
            .frameAllocator = allocators->frameAllocator,
        };
        renderDevice = renderInterface->device_create(&createInfo);
    }
}

SE_DLL_EXPORT void se_terminate(struct SabrinaEngine* engine)
{
    printf("Terminate\n");

    SeWindowSubsystemInterface* windowInterface = (SeWindowSubsystemInterface*)engine->find_subsystem_interface(engine, SE_WINDOW_SUBSYSTEM_NAME);
    
    renderDevice->destroy(renderDevice);
    windowInterface->destroy(windowHandle);
}

SE_DLL_EXPORT void se_update(struct SabrinaEngine* engine)
{
    struct SeWindowSubsystemInterface* windowInterface = (struct SeWindowSubsystemInterface*)engine->find_subsystem_interface(engine, SE_WINDOW_SUBSYSTEM_NAME);
    const struct SeWindowSubsystemInput* input = windowInterface->get_input(windowHandle);
    if (se_is_keyboard_button_pressed(input, SE_ESCAPE)) engine->shouldRun = false;
}
