
#include <stdio.h>

#include "engine/engine.h"
#include "gameplay_subsystem.h"

struct SeAllocatorBindings gPersistentAllocator;
struct SeAllocatorBindings gFrameAllocator;

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
}

SE_DLL_EXPORT void se_terminate(struct SabrinaEngine* engine)
{
    printf("Terminate\n");
}

SE_DLL_EXPORT void se_update(struct SabrinaEngine* engine)
{
    static int i = 0;
    printf("Update %d\n", i++);
    if (i == 1000) engine->shouldRun = false;
}
