
#include "se_vulkan_render_subsystem_render_pass.hpp"
#include "se_vulkan_render_subsystem_base.hpp"
#include "se_vulkan_render_subsystem_device.hpp"
#include "se_vulkan_render_subsystem_memory.hpp"
#include "se_vulkan_render_subsystem_utils.hpp"
#include "se_vulkan_render_subsystem_program.hpp"
#include "se_vulkan_render_subsystem_texture.hpp"

#define SE_VK_RP_MAX_SUBPASS_DEPENDENCIES 64

#define se_vk_rp_is_bit_set(mask, bit)      (((mask) >> (bit)) & 1)
#define se_vk_rp_for_each_set_bit(mask, it) for (uint32_t it = 0; it < (sizeof(mask) * 8); it++) if (se_vk_rp_is_bit_set(mask, it))
#define se_vk_rp_for_each_bit(mask, it)     for (uint32_t it = 0; it < (sizeof(mask) * 8); it++)

struct SeVkSubpassIntermediateData
{
    VkAttachmentReference   colorAttachmentRefs[SE_VK_GENERAL_BITMASK_WIDTH];
    VkAttachmentReference   inputAttachmentRefs[SE_VK_GENERAL_BITMASK_WIDTH];
    VkAttachmentReference   resolveAttachmentRefs[SE_VK_GENERAL_BITMASK_WIDTH];
    uint32_t                preserveAttachmentRefs[SE_VK_GENERAL_BITMASK_WIDTH];
    VkAttachmentReference   depthStencilReference;
    uint32_t                numColorRefs;
    uint32_t                numInputRefs;
};

size_t g_renderPassIndex = 0;

uint32_t se_vk_render_pass_count_flags(uint32_t flags)
{
    uint32_t count = 0;
    se_vk_rp_for_each_set_bit(flags, it) count += 1;
    return count;
}

VkAttachmentReference* se_vk_render_pass_find_attachment_reference(VkAttachmentReference* refs, size_t numRefs, uint32_t attachmentIndex)
{
    for (size_t it = 0; it < numRefs; it++)
        if (refs[it].attachment == attachmentIndex)
            return &refs[it];
    return nullptr;
}

void se_vk_render_pass_construct(SeVkRenderPass* pass, SeVkRenderPassInfo* info)
{
    SeVkMemoryManager* const memoryManager = &info->device->memoryManager;
    const VkAllocationCallbacks* const callbacks = se_vk_memory_manager_get_callbacks(memoryManager);
    const VkDevice logicalHandle = se_vk_device_get_logical_handle(info->device);
    *pass =
    {
        .object                 = { SE_VK_TYPE_PASS, g_renderPassIndex++ },
        .info                   = *info,
        .handle                 = VK_NULL_HANDLE,
        .attachmentLayoutInfos  = { },
        .clearValues            = { },
    };
    
    const bool isStencilSupported = se_vk_device_is_stencil_supported(pass->info.device);
    const bool hasDepthStencilAttachment = pass->info.hasDepthStencilAttachment;
    const VkSampleCountFlags supprotedAttachmentSampleCounts = se_vk_device_get_supported_framebuffer_multisample_types(pass->info.device);
    const uint32_t numAttachments = pass->info.numColorAttachments + (hasDepthStencilAttachment ? 1 : 0);
    const uint32_t numSubpasses = pass->info.numSubpasses;
    se_assert(numAttachments <= SE_VK_GENERAL_BITMASK_WIDTH);

    //
    // Attachments descriptions
    //
    VkAttachmentDescription attachmentDescriptions[SE_VK_GENERAL_BITMASK_WIDTH];
    if (hasDepthStencilAttachment)
    {
        attachmentDescriptions[numAttachments - 1] =
        {
            .flags          = 0,
            .format         = se_vk_device_get_depth_stencil_format(pass->info.device),
            .samples        = pass->info.depthStencilAttachment.sampling,
            .loadOp         = pass->info.depthStencilAttachment.loadOp,
            .storeOp        = pass->info.depthStencilAttachment.storeOp,
            .stencilLoadOp  = isStencilSupported ? pass->info.depthStencilAttachment.loadOp : VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = isStencilSupported ? pass->info.depthStencilAttachment.storeOp : VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout  = isStencilSupported ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
            .finalLayout    = VK_IMAGE_LAYOUT_UNDEFINED, // will be filled later
        };
    }
    for (size_t it = 0; it < pass->info.numColorAttachments; it++)
    {
        SeVkRenderPassAttachment* attachment = &pass->info.colorAttachments[it];
        attachmentDescriptions[it] =
        {
            .flags          = 0,
            .format         = attachment->format,
            .samples        = attachment->sampling,
            .loadOp         = attachment->loadOp,
            .storeOp        = attachment->storeOp,
            .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout  = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .finalLayout    = VK_IMAGE_LAYOUT_UNDEFINED, // will be filled later
        };
    }
    //
    // Subpass descriptions
    //
    SeVkSubpassIntermediateData subpassIntermediateDatas[SE_VK_GENERAL_BITMASK_WIDTH] = {0};
    VkSubpassDescription subpassDescriptions[SE_VK_GENERAL_BITMASK_WIDTH] = {0};
    // (globalUsedAttachmentsMask & 1 << attachmentIndex) == 1 if attachment was already used in the render pass
    SeVkGeneralBitmask globalUsedAttachmentsMask = 0;
    for (uint32_t it = 0; it < numSubpasses; it++)
    {
        SeVkRenderPassSubpass* subpassInfo = &pass->info.subpasses[it];
        SeVkSubpassIntermediateData* subpassIntermediateData = &subpassIntermediateDatas[it];
        // (localUsedAttachmentsMask & (1 << attachmentIndex)) != 0 if attachment was already used in the subpass
        SeVkGeneralBitmask localUsedAttachmentsMask = 0;
        //
        // Fill color, input and resolve attachments
        //
        // References layouts will be filled later
        se_vk_rp_for_each_set_bit(subpassInfo->colorRefs, ref)
        {
            subpassIntermediateData->colorAttachmentRefs[subpassIntermediateData->numColorRefs++] = { ref, VK_IMAGE_LAYOUT_UNDEFINED };
            globalUsedAttachmentsMask |= 1 << ref;
            localUsedAttachmentsMask |= 1 << ref;
        }
        se_vk_rp_for_each_set_bit(subpassInfo->inputRefs, ref)
        {
            subpassIntermediateData->inputAttachmentRefs[subpassIntermediateData->numInputRefs++] = { ref, VK_IMAGE_LAYOUT_UNDEFINED };
            globalUsedAttachmentsMask |= 1 << ref;
            localUsedAttachmentsMask |= 1 << ref;
        }
        SeVkGeneralBitmask usedFromResolveRefs = 0;
        SeVkGeneralBitmask usedToResolveRefs = 0;
        for (uint32_t refIt = 0; refIt < subpassInfo->numResolveRefs; refIt++)
        {
            const SeVkResolveRef ref = subpassInfo->resolveRefs[refIt];
            se_assert_msg(!(usedFromResolveRefs & ref.from), "Can't resolve same attachment twice in a single subpass");
            se_assert_msg(!(usedToResolveRefs & ref.to), "Can't resolve to the same attachment twice in a single subpass");
            se_assert_msg(!(usedFromResolveRefs & ref.to), "Can't resolve to the attachment that is already used as a resolve source");
            se_assert_msg(!(usedToResolveRefs & ref.from), "Can't resolve attachments that is alredy used as a resolve destination");
            se_assert_msg(ref.from < pass->info.numColorAttachments, "Invalid resolve source reference");
            se_assert_msg(ref.to < pass->info.numColorAttachments, "Invalid resolve target reference");
            subpassIntermediateData->resolveAttachmentRefs[ref.from] = { ref.to, VK_IMAGE_LAYOUT_UNDEFINED };
            usedFromResolveRefs |= 1 << ref.from;
            usedToResolveRefs |= 1 << ref.to;
            globalUsedAttachmentsMask |= 1 << ref.from;
            localUsedAttachmentsMask  |= 1 << ref.from;
            globalUsedAttachmentsMask |= 1 << ref.to;
            localUsedAttachmentsMask  |= 1 << ref.to;
        }
        se_vk_rp_for_each_bit(usedFromResolveRefs, ref)
        {
            if (!se_vk_rp_is_bit_set(usedFromResolveRefs, ref))
                subpassIntermediateData->resolveAttachmentRefs[ref] = { VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_UNDEFINED };
        }
        //
        // Fill depth stencil ref
        //
        if (subpassInfo->depthRead || subpassInfo->depthWrite)
        {
            subpassIntermediateData->depthStencilReference = { numAttachments - 1, VK_IMAGE_LAYOUT_UNDEFINED };
            globalUsedAttachmentsMask |= 1 << (numAttachments - 1);
            localUsedAttachmentsMask |= 1 << (numAttachments - 1);
        }
        else
        {
            subpassIntermediateData->depthStencilReference = { VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_UNDEFINED };
        }
        //
        // Fill preserve info
        //
        uint32_t numPreserveRefs = 0;
        for (uint32_t attachmentIt = 0; attachmentIt < numAttachments; attachmentIt++)
        {
            // If attachment was used before, but current subpass is not using it, we preserve it
            if (se_vk_rp_is_bit_set(globalUsedAttachmentsMask, attachmentIt) && !se_vk_rp_is_bit_set(localUsedAttachmentsMask, attachmentIt))
            {
                subpassIntermediateData->preserveAttachmentRefs[numPreserveRefs++] = attachmentIt;
            }
        }
        //
        // Setup the description
        //
        VkAttachmentReference* depthRef = nullptr;
        if (subpassIntermediateData->depthStencilReference.attachment != VK_ATTACHMENT_UNUSED)
        {
            depthRef = &subpassIntermediateData->depthStencilReference;
        }
        subpassDescriptions[it] =
        {
            .flags                      = 0,
            .pipelineBindPoint          = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .inputAttachmentCount       = subpassIntermediateData->numInputRefs,
            .pInputAttachments          = subpassIntermediateData->inputAttachmentRefs,
            .colorAttachmentCount       = subpassIntermediateData->numColorRefs,
            .pColorAttachments          = subpassIntermediateData->colorAttachmentRefs,
            .pResolveAttachments        = usedFromResolveRefs ? subpassIntermediateData->resolveAttachmentRefs : nullptr,
            .pDepthStencilAttachment    = depthRef,
            .preserveAttachmentCount    = numPreserveRefs,
            .pPreserveAttachments       = subpassIntermediateData->preserveAttachmentRefs,
        };
    }
    //
    // Dependencies and layouts
    //
    // Each bit represents subpass dependency of some kind
    SeVkGeneralBitmask externalColorDependencies        = 0; // If subpass uses color or resolve from external subpass
    SeVkGeneralBitmask externalDepthDependencies        = 0; // If subpass uses depth from external subpass
    SeVkGeneralBitmask externalInputDependencies        = 0; // If subpass uses input from external subpass
    SeVkGeneralBitmask selfColorDependencies            = 0; // If subpass has same attachment as input and color attachment
    SeVkGeneralBitmask selfDepthDependencies            = 0; // If subpass has same attachment as input and depth attachment
    SeVkGeneralBitmask inputReadDependencies            = 0; // If subpass uses attachment as input
    SeVkGeneralBitmask colorReadWriteDependencies       = 0; // If subpass uses attachment as color or resolve
    SeVkGeneralBitmask depthStencilWriteDependencies    = 0; // If subpass uses attachment as depth write
    SeVkGeneralBitmask depthStencilReadDependencies     = 0; // If subpass uses attachment as depth read
    //
    // Set dependency flags and attachments layouts for each subpass
    //
    for (uint32_t attachmentIt = 0; attachmentIt < numAttachments; attachmentIt++)
    {
        VkAttachmentDescription* attachment = &attachmentDescriptions[attachmentIt];
        bool isUsed = false;
        const VkImageLayout initialLayout = attachment->initialLayout;
        VkImageLayout currentLayout = attachment->initialLayout;
        for (uint32_t subpassIt = 0; subpassIt < numSubpasses; subpassIt++)
        {
            const SeVkGeneralBitmask subpassBitMask = 1 << subpassIt;
            SeVkRenderPassSubpass* subpassInfo = &pass->info.subpasses[subpassIt];
            SeVkSubpassIntermediateData* subpassIntermediateData = &subpassIntermediateDatas[subpassIt];
            VkAttachmentReference* color    = se_vk_render_pass_find_attachment_reference(subpassIntermediateData->colorAttachmentRefs, subpassIntermediateData->numColorRefs, attachmentIt);
            VkAttachmentReference* input    = se_vk_render_pass_find_attachment_reference(subpassIntermediateData->inputAttachmentRefs, subpassIntermediateData->numInputRefs, attachmentIt);
            VkAttachmentReference* resolve  = se_vk_render_pass_find_attachment_reference(subpassIntermediateData->resolveAttachmentRefs, subpassIntermediateData->numColorRefs, attachmentIt);
            VkAttachmentReference* depth    = (subpassIntermediateData->depthStencilReference.attachment != VK_ATTACHMENT_UNUSED) && (subpassIntermediateData->depthStencilReference.attachment == attachmentIt)
                                                ? &subpassIntermediateData->depthStencilReference
                                                : nullptr;
            if (!color && !input && !resolve && !depth)
            {
                continue;
            }
            if (color) { se_assert_msg(!depth, "Can't have same attachment as color and depth reference (in a single subpass)"); }
            if (depth) { se_assert_msg(!color, "Can't have same attachment as color and depth reference (in a single subpass)"); }
            if (color || resolve)
            {
                if (currentLayout != VK_IMAGE_LAYOUT_GENERAL) currentLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                if (!isUsed && currentLayout != initialLayout)
                {
                    externalColorDependencies |= subpassBitMask;
                }
                colorReadWriteDependencies |= subpassBitMask;
            }
            if (depth)
            {
                const bool isWrite = subpassInfo->depthWrite;
                const bool isRead = subpassInfo->depthRead;
                se_assert_msg(isRead, "If depth attachment is used in a subpass, it must be at least read");
                if (isRead && isWrite)
                {
                    if (currentLayout != VK_IMAGE_LAYOUT_GENERAL)
                    {
                        currentLayout = isStencilSupported
                            ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
                            : VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
                    }
                    depthStencilWriteDependencies |= subpassBitMask;
                }
                else
                {
                    if (currentLayout != VK_IMAGE_LAYOUT_GENERAL)
                    {
                        currentLayout = isStencilSupported
                            ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
                            : VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL;
                    }
                }
                if (!isUsed && currentLayout != initialLayout)
                {
                    externalDepthDependencies |= subpassBitMask;
                }
                depthStencilReadDependencies |= subpassBitMask;
            }
            if (input)
            {
                if (color || resolve || depth)
                {
                    currentLayout = VK_IMAGE_LAYOUT_GENERAL;
                    if (color) selfColorDependencies |= subpassBitMask;
                    if (depth) selfDepthDependencies |= subpassBitMask;
                }
                else if (currentLayout != VK_IMAGE_LAYOUT_GENERAL)
                {
                    currentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                }
                if (!isUsed && currentLayout != initialLayout)
                {
                    externalInputDependencies |= subpassBitMask;
                }
                inputReadDependencies |= subpassBitMask;
            }
            if (color) color->layout = currentLayout;
            if (input) input->layout = currentLayout;
            if (resolve) resolve->layout = currentLayout;
            if (depth) depth->layout = currentLayout;
            isUsed = true;
        }
        attachment->finalLayout = currentLayout;
    }
    //
    // Fill subpass dependencies array
    //
    const SeVkGeneralBitmask allExternalDependencies = externalColorDependencies | externalInputDependencies | externalDepthDependencies;
    const SeVkGeneralBitmask allSelfDependencies = selfColorDependencies | selfDepthDependencies;
    const uint32_t numSubpassDependencies = se_vk_render_pass_count_flags(allExternalDependencies) + se_vk_render_pass_count_flags(allSelfDependencies) + numSubpasses - 1;
    se_assert_msg(numSubpassDependencies < SE_VK_RP_MAX_SUBPASS_DEPENDENCIES, "Max number of subpass dependecies is exceeded");
    VkSubpassDependency subpassDependencies[SE_VK_RP_MAX_SUBPASS_DEPENDENCIES] = {0};
    size_t subpassDependencyIt = 0;
    se_vk_rp_for_each_set_bit(allExternalDependencies, it)
    {
        VkSubpassDependency dependency = {0};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = it;
        if (se_vk_rp_is_bit_set(externalColorDependencies, it))
        {
            dependency.srcStageMask |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependency.srcAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            dependency.dstStageMask |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependency.dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        }
        if (se_vk_rp_is_bit_set(externalInputDependencies, it))
        {
            dependency.srcStageMask |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            dependency.srcAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            dependency.dstStageMask |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT /*| VK_PIPELINE_STAGE_VERTEX_SHADER_BIT*/;
            dependency.dstAccessMask |= VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
        }
        if (se_vk_rp_is_bit_set(externalDepthDependencies, it))
        {
            dependency.srcStageMask |= VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            dependency.srcAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            dependency.dstStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            dependency.dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        }
        subpassDependencies[subpassDependencyIt++] = dependency;
    }
    se_vk_rp_for_each_set_bit(allSelfDependencies, it)
    {
        VkSubpassDependency dependency = { };
        dependency.srcSubpass = it;
        dependency.dstSubpass = it;
        dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
        if (se_vk_rp_is_bit_set(selfColorDependencies, it))
        {
            dependency.srcStageMask |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependency.srcAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        }
        if (se_vk_rp_is_bit_set(selfDepthDependencies, it))
        {
            dependency.srcStageMask |= VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
            dependency.srcAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        }
        dependency.dstStageMask |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependency.dstAccessMask |= VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
        subpassDependencies[subpassDependencyIt++] = dependency;
    }
    for (uint32_t it = 1; it < numSubpasses; it++)
    {
        // Each subpass must (?) write at least to color or depth attachment
        se_assert(se_vk_rp_is_bit_set(colorReadWriteDependencies, it) || se_vk_rp_is_bit_set(depthStencilWriteDependencies, it));
        se_assert(se_vk_rp_is_bit_set(colorReadWriteDependencies, it - 1) || se_vk_rp_is_bit_set(depthStencilWriteDependencies, it - 1));
        VkSubpassDependency dependency = { };
        dependency.srcSubpass = it - 1;
        dependency.dstSubpass = it;
        dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
        if (se_vk_rp_is_bit_set(colorReadWriteDependencies, it - 1))
        {
            dependency.srcStageMask |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependency.srcAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        }
        if (se_vk_rp_is_bit_set(depthStencilWriteDependencies, it - 1))
        {
            dependency.srcStageMask |= VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            dependency.srcAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        }
        if (se_vk_rp_is_bit_set(inputReadDependencies, it))
        {
            dependency.dstStageMask |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT /*| VK_PIPELINE_STAGE_VERTEX_SHADER_BIT*/;
            dependency.dstAccessMask |= VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
        }
        if (se_vk_rp_is_bit_set(colorReadWriteDependencies, it))
        {
            dependency.dstStageMask |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependency.dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        }
        if (se_vk_rp_is_bit_set(depthStencilWriteDependencies, it))
        {
            dependency.dstStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            dependency.dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;;
        }
        if (se_vk_rp_is_bit_set(depthStencilReadDependencies, it))
        {
            dependency.dstStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            dependency.dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        }
        dependency.dstStageMask |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependency.dstAccessMask |= VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
        subpassDependencies[subpassDependencyIt++] = dependency;
    }
    //
    // Finally creating render pass
    //
    VkRenderPassCreateInfo renderPassInfo =
    {
        .sType              = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .pNext              = nullptr,
        .flags              = 0,
        .attachmentCount    = numAttachments,
        .pAttachments       = attachmentDescriptions,
        .subpassCount       = numSubpasses,
        .pSubpasses         = subpassDescriptions,
        .dependencyCount    = numSubpassDependencies,
        .pDependencies      = subpassDependencies,
    };
    se_vk_check(vkCreateRenderPass(logicalHandle, &renderPassInfo, callbacks, &pass->handle));
    //
    // Fill attachment layout infos and clear values
    //
    for (uint32_t it = 0; it < numAttachments; it++)
    {
        pass->attachmentLayoutInfos[it] =
        {
            .initialLayout  = attachmentDescriptions[it].initialLayout,
            .finalLayout    = attachmentDescriptions[it].finalLayout,
        };
        pass->clearValues[it] = it == pass->info.numColorAttachments
            ? pass->info.colorAttachments[it].clearValue
            : pass->info.depthStencilAttachment.clearValue;
    }
}

void se_vk_render_pass_destroy(SeVkRenderPass* pass)
{
    const VkAllocationCallbacks* const callbacks = se_vk_memory_manager_get_callbacks(&pass->info.device->memoryManager);
    vkDestroyRenderPass(se_vk_device_get_logical_handle(pass->info.device), pass->handle, callbacks);
}

void se_vk_render_pass_validate_fragment_program_setup(SeVkRenderPass* pass, SeVkProgram* program, size_t subpassIndex)
{
    SeVkRenderPassSubpass* subpass = &pass->info.subpasses[subpassIndex];
    SimpleSpirvReflection* reflection = &program->reflection;
    se_assert(reflection->shaderType == SSR_SHADER_TYPE_FRAGMENT);

    SeVkGeneralBitmask shaderSubpassInputAttachmentMask = 0;
    for (size_t it = 0; it < reflection->numUniforms; it++)
    {
        SsrUniform* uniform = &reflection->uniforms[it];
        if (uniform->kind == SSR_UNIFORM_INPUT_ATTACHMENT)
        {
            se_assert(uniform->inputAttachmentIndex < SE_VK_GENERAL_BITMASK_WIDTH);
            shaderSubpassInputAttachmentMask |= 1 << uniform->inputAttachmentIndex;
        }
    }
    se_assert_msg(shaderSubpassInputAttachmentMask == subpass->inputRefs, "Mismatch between fragment shader subpass inputs and render pass input attachments");

    SeVkGeneralBitmask shaderColorAttachmentMask = 0;
    for (size_t it = 0; it < reflection->numOutputs; it++)
    {
        SsrShaderIO* output = &reflection->outputs[it];
        if (!output->isBuiltIn)
        {
            se_assert(output->location < SSR_UNIFORM_INPUT_ATTACHMENT);
            shaderColorAttachmentMask |= 1 << output->location;
        }
    }
    se_assert_msg(shaderColorAttachmentMask == subpass->colorRefs, "Mismatch between fragment shader outputs and render pass color attachments");
}

uint32_t se_vk_render_pass_get_num_color_attachments(SeVkRenderPass* pass, size_t subpassIndex)
{
    return se_vk_render_pass_count_flags(pass->info.subpasses[subpassIndex].colorRefs);
}

void se_vk_render_pass_validate_framebuffer_textures(SeVkRenderPass* pass, SeVkTexture** textures, size_t numTextures)
{
    se_assert(se_vk_render_pass_num_attachments(pass) == numTextures);
    for (size_t it = 0; it < numTextures; it++)
    {
        se_assert(pass->info.colorAttachments[it].format == textures[it]->format);
    }
}

VkRenderPassBeginInfo se_vk_render_pass_get_begin_info(SeVkRenderPass* pass, VkFramebuffer framebuffer, VkRect2D renderArea)
{
    const uint32_t numAttachments = se_vk_render_pass_num_attachments(pass);
    return
    {
        .sType              = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext              = nullptr,
        .renderPass         = pass->handle,
        .framebuffer        = framebuffer,
        .renderArea         = renderArea,
        .clearValueCount    = numAttachments,
        .pClearValues       = pass->clearValues,
    };
}

#undef se_vk_rp_is_bit_set
#undef se_vk_rp_for_each_set_bit
#undef se_vk_rp_for_each_bit
