#ifndef _SE_VULKAN_RENDER_ABSTRACTION_SUBSYSTEM_H_
#define _SE_VULKAN_RENDER_ABSTRACTION_SUBSYSTEM_H_

#include "engine/render_abstraction_interface.hpp"

struct SeVulkanRenderAbstractionSubsystem
{
    using Interface = SeRenderAbstractionSubsystemInterface;
    static constexpr const char* NAME = "se_vulkan_render_abstraction_subsystem";
};

#endif
