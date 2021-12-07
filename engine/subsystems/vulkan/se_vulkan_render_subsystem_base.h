#ifndef _SE_VULKAN_RENDER_SUBSYSTEM_BASE_H_
#define _SE_VULKAN_RENDER_SUBSYSTEM_BASE_H_

#ifdef _WIN32
#   define VK_USE_PLATFORM_WIN32_KHR
#else
#   error Unsupported platform
#endif

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#include "engine/libs/volk/volk.h"
#include "engine/debug.h"

#define se_vk_check(cmd) do { VkResult __result = cmd; se_assert(__result == VK_SUCCESS); } while(0)
#define se_vk_expect_handle(renderObjPtr, expectedHandleType, msg) se_assert(renderObjPtr->handleType == expectedHandleType && msg" - incorrect render object type (expected "#expectedHandleType")")

#endif
