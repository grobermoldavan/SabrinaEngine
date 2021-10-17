#ifndef _USER_APP_GAMEPLAY_SUBSYSTEM_H_
#define _USER_APP_GAMEPLAY_SUBSYSTEM_H_

#include "engine/common_includes.h"

#define USER_APP_GAMEPLAY_SUBSYSTEM_NAME "gameplay_subsystem"

SE_DLL_EXPORT void se_load(struct SabrinaEngine*);
SE_DLL_EXPORT void se_unload(struct SabrinaEngine*);
SE_DLL_EXPORT void se_init(struct SabrinaEngine*);
SE_DLL_EXPORT void se_terminate(struct SabrinaEngine*);
SE_DLL_EXPORT void se_update(struct SabrinaEngine*);

#endif
