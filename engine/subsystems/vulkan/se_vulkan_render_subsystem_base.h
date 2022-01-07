#ifndef _SE_VULKAN_RENDER_SUBSYSTEM_BASE_H_
#define _SE_VULKAN_RENDER_SUBSYSTEM_BASE_H_

#ifdef _WIN32
#   define VK_USE_PLATFORM_WIN32_KHR
#else
#   error Unsupported platform
#endif

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <inttypes.h>
#include <stdbool.h>

#include "engine/libs/volk/volk.h"
#include "engine/render_abstraction_interface.h"
#include "engine/debug.h"

#ifdef SE_DEBUG
typedef struct SeVkRenderObject
{
    SeRenderObject base;
    size_t refCounter;
} SeVkRenderObject;
#define se_vk_add_external_resource_dependency(objPtr) ((SeVkRenderObject*)objPtr)->refCounter++
#define se_vk_remove_external_resource_dependency(objPtr) (se_assert(((SeVkRenderObject*)objPtr)->refCounter), ((SeVkRenderObject*)objPtr)->refCounter--)
#define se_vk_check_external_resource_dependencies(objPtr) se_assert(((SeVkRenderObject*)objPtr)->refCounter == 0)
#define se_vk_render_object(handleType, destroyFunc) (SeVkRenderObject){ .base = (SeRenderObject){ handleType, destroyFunc }, 0 }
#define se_vk_render_object_handle_type(objPtr) ((SeVkRenderObject*)objPtr)->base.handleType
#else
typedef SeRenderObject SeVkRenderObject;
#define se_vk_add_external_resource_dependency(objPtr)
#define se_vk_remove_external_resource_dependency(objPtr)
#define se_vk_check_external_resource_dependencies(objPtr)
#define se_vk_render_object(handleType, destroyFunc) (SeVkRenderObject){ handleType, destroyFunc }
#define se_vk_render_object_handle_type(objPtr) ((SeVkRenderObject*)objPtr)->handleType
#endif

typedef uint32_t SeVkGeneralBitmask;
#define SE_VK_GENERAL_BITMASK_WIDTH (sizeof(SeVkGeneralBitmask) * 8)

#define se_vk_check(cmd) do { VkResult __result = cmd; se_assert(__result == VK_SUCCESS); } while(0)
#define se_vk_expect_handle(renderObjPtr, expectedHandleType, msg) se_assert(se_vk_render_object_handle_type(renderObjPtr) == expectedHandleType && msg" - incorrect render object type (expected "#expectedHandleType")")

#endif
