
#include "se_vulkan_render_subsystem_render_pass.h"
#include "se_vulkan_render_subsystem_base.h"
#include "se_vulkan_render_subsystem_device.h"
#include "se_vulkan_render_subsystem_memory.h"
#include "se_vulkan_render_subsystem_utils.h"
#include "se_vulkan_render_subsystem_render_program.h"
#include "se_vulkan_render_subsystem_framebuffer.h"
#include "se_vulkan_render_subsystem_in_flight_manager.h"
#include "engine/libs/ssr/simple_spirv_reflection.h"
#include "engine/allocator_bindings.h"
#include "engine/render_abstraction_interface.h"

#define se_vk_rp_is_bit_set(mask, bit)      (((mask) >> (bit)) & 1)
#define se_vk_rp_for_each_set_bit(mask, it) for (uint32_t it = 0; it < (sizeof(mask) * 8); it++) if (se_vk_rp_is_bit_set(mask, it))
#define se_vk_rp_for_each_bit(mask, it)     for (uint32_t it = 0; it < (sizeof(mask) * 8); it++)

typedef uint32_t SeVkRpBitmask;
#define SE_VK_RP_MAX_BITMASK_WIDTH (sizeof(SeVkRpBitmask) * 8)
#define SE_VK_RP_MAX_SUBPASS_DEPENDENCIES 64

typedef struct SeVkSubpassInfo
{
    uint32_t inputAttachmentRefsBitmask;
    uint32_t colorAttachmentRefsBitmask;
} SeVkSubpassInfo;

typedef struct SeVkRenderPassAttachmentInfo
{
    VkImageLayout initialLayout;
    VkImageLayout finalLayout;
    VkFormat format;
} SeVkRenderPassAttachmentInfo;

typedef struct SeVkRenderPass
{
    SeRenderObject object;
    SeRenderObject* device;
    VkRenderPass handle;
    SeVkSubpassInfo subpassInfos[SE_VK_RP_MAX_BITMASK_WIDTH];
    SeVkRenderPassAttachmentInfo attachmentInfos[SE_VK_RP_MAX_BITMASK_WIDTH];
    VkClearValue clearValues[SE_VK_RP_MAX_BITMASK_WIDTH];
    size_t numSubpasses;
    size_t numAttachments;
} SeVkRenderPass;

typedef struct SeVkSubpassIntermediateData
{
    VkAttachmentReference colorAttachmentRefs[SE_VK_RP_MAX_BITMASK_WIDTH];
    VkAttachmentReference inputAttachmentRefs[SE_VK_RP_MAX_BITMASK_WIDTH];
    VkAttachmentReference resolveAttachmentRefs[SE_VK_RP_MAX_BITMASK_WIDTH];
    uint32_t preserveAttachmentRefs[SE_VK_RP_MAX_BITMASK_WIDTH];
    VkAttachmentReference depthStencilReference;
} SeVkSubpassIntermediateData;

static uint32_t se_vk_render_pass_count_flags(uint32_t flags)
{
    uint32_t count = 0;
    se_vk_rp_for_each_set_bit(flags, it) count += 1;
    return count;
}

static VkAttachmentReference* se_vk_render_pass_find_attachment_reference(VkAttachmentReference* refs, size_t numRefs, uint32_t attachmentIndex)
{
    for (size_t it = 0; it < numRefs; it++)
    {
        if (refs[it].attachment == attachmentIndex)
        {
            return &refs[it];
        }
    }
    return NULL;
}

SeRenderObject* se_vk_render_pass_create(SeRenderPassCreateInfo* createInfo)
{
    se_vk_expect_handle(createInfo->device, SE_RENDER_HANDLE_TYPE_DEVICE, "Can't create render pass");
    SeVkMemoryManager* memoryManager = se_vk_device_get_memory_manager(createInfo->device);
    VkAllocationCallbacks* callbacks = se_vk_memory_manager_get_callbacks(memoryManager);
    SeAllocatorBindings* allocator = memoryManager->cpu_persistentAllocator;
    VkDevice logicalHandle = se_vk_device_get_logical_handle(createInfo->device);
    //
    //
    //
    const bool isStencilSupported = se_vk_device_is_stencil_supported(createInfo->device);
    const bool hasDepthStencilAttachment = createInfo->depthStencilAttachment != NULL;
    const VkSampleCountFlags supprotedAttachmentSampleCounts = se_vk_device_get_supported_framebuffer_multisample_types(createInfo->device);
    SeVkRenderPass* pass = allocator->alloc(allocator->allocator, sizeof(SeVkRenderPass), se_default_alignment, se_alloc_tag);
    pass->object.destroy = se_vk_render_pass_submit_for_deffered_destruction;
    pass->object.handleType = SE_RENDER_HANDLE_TYPE_PASS;
    pass->device = createInfo->device;
    //
    // Convert size_t values to uint32_t
    //
    // @TODO : safe cast from size_t to uint32_t
    const uint32_t numRenderPassAttachments = (uint32_t)(createInfo->numColorAttachments + (hasDepthStencilAttachment ? 1 : 0));
    const uint32_t numSubpasses = (uint32_t)createInfo->numSubpasses;
    //
    // Basic validation
    //
    se_assert((createInfo->numColorAttachments + (hasDepthStencilAttachment ? 1 : 0)) <= SE_VK_RP_MAX_BITMASK_WIDTH && "Max number of attachment descriptions is 32");
    se_assert(createInfo->numSubpasses <= SE_VK_RP_MAX_BITMASK_WIDTH && "Max number of subpasses is 32");
    //
    // Attachments descriptions
    //
    VkAttachmentDescription renderPassAttachmentDescriptions[SE_VK_RP_MAX_BITMASK_WIDTH] = {0};
    if (hasDepthStencilAttachment)
    {
        renderPassAttachmentDescriptions[numRenderPassAttachments - 1] = (VkAttachmentDescription)
        {
            .flags          = 0,
            .format         = se_vk_device_get_depth_stencil_format(createInfo->device),
            .samples        = se_vk_utils_pick_sample_count(se_vk_utils_to_vk_sample_count(createInfo->depthStencilAttachment->samples), supprotedAttachmentSampleCounts),
            .loadOp         = se_vk_utils_to_vk_load_op(createInfo->depthStencilAttachment->loadOp),
            .storeOp        = se_vk_utils_to_vk_store_op(createInfo->depthStencilAttachment->storeOp),
            .stencilLoadOp  = isStencilSupported ? se_vk_utils_to_vk_load_op(createInfo->depthStencilAttachment->loadOp) : VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = isStencilSupported ? se_vk_utils_to_vk_store_op(createInfo->depthStencilAttachment->storeOp) : VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout  = isStencilSupported ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
            .finalLayout    = VK_IMAGE_LAYOUT_UNDEFINED, // will be filled later
        };
    }
    for (size_t it = 0; it < createInfo->numColorAttachments; it++)
    {
        SeRenderPassAttachment* attachment = &createInfo->colorAttachments[it];
        if (attachment->format == SE_TEXTURE_FORMAT_DEPTH_STENCIL)
        {
            se_assert(!"todo");
        }
        else
        {
            const bool isSwapChainImage = attachment->format == SE_TEXTURE_FORMAT_SWAP_CHAIN;
            renderPassAttachmentDescriptions[it] = (VkAttachmentDescription)
            {
                .flags          = 0,
                .format         = isSwapChainImage ? se_vk_device_get_swap_chain_format(createInfo->device) : se_vk_utils_to_vk_format(attachment->format),
                .samples        = se_vk_utils_pick_sample_count(se_vk_utils_to_vk_sample_count(attachment->samples), supprotedAttachmentSampleCounts),
                .loadOp         = se_vk_utils_to_vk_load_op(attachment->loadOp),
                .storeOp        = se_vk_utils_to_vk_store_op(attachment->storeOp),
                .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout  = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .finalLayout    = VK_IMAGE_LAYOUT_UNDEFINED, // will be filled later
            };
        }
    }
    //
    // Subpass descriptions
    //
    SeVkSubpassIntermediateData subpassIntermediateDatas[SE_VK_RP_MAX_BITMASK_WIDTH] = {0};
    VkSubpassDescription subpassDescriptions[SE_VK_RP_MAX_BITMASK_WIDTH] = {0};
    // (globalUsedAttachmentsMask & 1 << attachmentIndex) == 1 if attachment was already used in the render pass
    SeVkRpBitmask globalUsedAttachmentsMask = 0;
    for (size_t it = 0; it < numSubpasses; it++)
    {
        SeRenderPassSubpass* subpassCreateInfo = &createInfo->subpasses[it];
        SeVkSubpassIntermediateData* subpassIntermediateData = &subpassIntermediateDatas[it];
        // localUsedAttachmentsMask & 1 << attachmentIndex == 1 if attachment was already used in the subpass
        SeVkRpBitmask localUsedAttachmentsMask = 0;
        //
        // Fill color, input and resolve attachments
        //
        // References layouts will be filled later
        size_t refCounter = 0;
        for (size_t refIt = 0; refIt < subpassCreateInfo->numColorRefs; refIt++)
        {
            uint32_t ref = subpassCreateInfo->colorRefs[refIt];
            subpassIntermediateData->colorAttachmentRefs[refCounter] = (VkAttachmentReference){ ref, VK_IMAGE_LAYOUT_UNDEFINED };
            globalUsedAttachmentsMask |= 1 << ref;
            localUsedAttachmentsMask |= 1 << ref;
            refCounter += 1;
        }
        refCounter = 0;
        for (size_t refIt = 0; refIt < subpassCreateInfo->numInputRefs; refIt++)
        {
            uint32_t ref = subpassCreateInfo->inputRefs[refIt];
            subpassIntermediateData->inputAttachmentRefs[refCounter] = (VkAttachmentReference){ ref, VK_IMAGE_LAYOUT_UNDEFINED };
            globalUsedAttachmentsMask |= 1 << ref;
            localUsedAttachmentsMask |= 1 << ref;
            refCounter += 1;
        }
        refCounter = 0;
        for (size_t refIt = 0; refIt < subpassCreateInfo->numResolveRefs; refIt++)
        {
            uint32_t ref = subpassCreateInfo->resolveRefs[refIt];
            ref = ref == SE_RENDER_PASS_UNUSED_ATTACHMENT ? VK_ATTACHMENT_UNUSED : ref;
            subpassIntermediateData->resolveAttachmentRefs[refCounter] = (VkAttachmentReference){ ref, VK_IMAGE_LAYOUT_UNDEFINED };
            if (ref != VK_ATTACHMENT_UNUSED)
            {
                globalUsedAttachmentsMask |= 1 << ref;
                localUsedAttachmentsMask |= 1 << ref;
            }
            refCounter += 1;
        }
        //
        // Fill depth stencil ref
        //
        switch (subpassCreateInfo->depthOp)
        {
            case SE_DEPTH_OP_NOTHING:
            {
                subpassIntermediateData->depthStencilReference = (VkAttachmentReference){ VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_UNDEFINED };
            } break;
            case SE_DEPTH_OP_READ_WRITE:
            {
                subpassIntermediateData->depthStencilReference = (VkAttachmentReference)
                {
                    numRenderPassAttachments - 1,
                    VK_IMAGE_LAYOUT_UNDEFINED
                };
                globalUsedAttachmentsMask |= 1 << (numRenderPassAttachments - 1);
                localUsedAttachmentsMask |= 1 << (numRenderPassAttachments - 1);
            } break;
            case SE_DEPTH_OP_READ:
            {
                subpassIntermediateData->depthStencilReference = (VkAttachmentReference)
                {
                    VK_ATTACHMENT_UNUSED,
                    hasDepthStencilAttachment
                        ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
                        : VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL
                };
                globalUsedAttachmentsMask |= 1 << (numRenderPassAttachments - 1);
                localUsedAttachmentsMask |= 1 << (numRenderPassAttachments - 1);
            } break;
        }
        //
        // Fill preserve info
        //
        size_t preserveRefsSize = 0;
        uint32_t preserveRefsMask = 0;
        for (uint32_t attachmentIt = 0; attachmentIt < numRenderPassAttachments; attachmentIt++)
        {
            // If attachment was used before, but current subpass is not using it, we preserve it
            if (se_vk_rp_is_bit_set(globalUsedAttachmentsMask, attachmentIt) && !se_vk_rp_is_bit_set(localUsedAttachmentsMask, attachmentIt))
            {
                preserveRefsSize += 1;
                preserveRefsMask |= 1 << attachmentIt;
            }
        }
        refCounter = 0;
        se_vk_rp_for_each_set_bit(preserveRefsMask, preserveIt)
        {
            subpassIntermediateData->preserveAttachmentRefs[refCounter++] = preserveIt;
        }
        //
        // Setup the description
        //
        VkAttachmentReference* depthRef = NULL;
        if (subpassIntermediateData->depthStencilReference.attachment != VK_ATTACHMENT_UNUSED)
        {
            depthRef = &subpassIntermediateData->depthStencilReference;
        }
        subpassDescriptions[it] = (VkSubpassDescription)
        {
            .flags                      = 0,
            .pipelineBindPoint          = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .inputAttachmentCount       = (uint32_t)subpassCreateInfo->numInputRefs, // TODO : safe cast
            .pInputAttachments          = subpassIntermediateData->inputAttachmentRefs,
            .colorAttachmentCount       = (uint32_t)subpassCreateInfo->numColorRefs, // TODO : safe cast
            .pColorAttachments          = subpassIntermediateData->colorAttachmentRefs,
            .pResolveAttachments        = subpassCreateInfo->numResolveRefs ? subpassIntermediateData->resolveAttachmentRefs : NULL,
            .pDepthStencilAttachment    = depthRef,
            .preserveAttachmentCount    = (uint32_t)preserveRefsSize, // TODO : safe cast
            .pPreserveAttachments       = subpassIntermediateData->preserveAttachmentRefs,
        };
    }
    //
    // Dependencies and layouts
    //
    // Each bit represents subpass dependency of some kind
    SeVkRpBitmask externalColorDependencies = 0; // If subpass uses color or resolve from external subpass
    SeVkRpBitmask externalDepthDependencies = 0; // If subpass uses depth from external subpass
    SeVkRpBitmask externalInputDependencies = 0; // If subpass uses input from external subpass
    SeVkRpBitmask selfColorDependencies = 0; // If subpass has same attachment as input and color attachment
    SeVkRpBitmask selfDepthDependencies = 0; // If subpass has same attachment as input and depth attachment
    SeVkRpBitmask inputReadDependencies = 0; // If subpass uses attachment as input
    SeVkRpBitmask colorReadWriteDependencies = 0; // If subpass uses attachment as color or resolve
    SeVkRpBitmask depthStencilWriteDependencies = 0; // If subpass uses attachment as depth read
    SeVkRpBitmask depthStencilReadDependencies = 0; // If subpass uses attachment as depth write
    //
    // Set dependency flags and attachments layouts for each subpass
    //
    for (size_t attachmentIt = 0; attachmentIt < numRenderPassAttachments; attachmentIt++)
    {
        VkAttachmentDescription* attachment = &renderPassAttachmentDescriptions[attachmentIt];
        bool isUsed = false;
        const VkImageLayout initialLayout = attachment->initialLayout;
        VkImageLayout currentLayout = attachment->initialLayout;
        for (size_t subpassIt = 0; subpassIt < numSubpasses; subpassIt++)
        {
            const uint32_t subpassBitMask = 1 << subpassIt;
            SeRenderPassSubpass* subpassCreateInfo = &createInfo->subpasses[subpassIt];
            SeVkSubpassIntermediateData* subpass = &subpassIntermediateDatas[subpassIt];
            VkAttachmentReference* color    = se_vk_render_pass_find_attachment_reference(subpass->colorAttachmentRefs, subpassCreateInfo->numColorRefs, attachmentIt);
            VkAttachmentReference* input    = se_vk_render_pass_find_attachment_reference(subpass->inputAttachmentRefs, subpassCreateInfo->numInputRefs, attachmentIt);
            VkAttachmentReference* resolve  = se_vk_render_pass_find_attachment_reference(subpass->resolveAttachmentRefs, subpassCreateInfo->numResolveRefs, attachmentIt);
            VkAttachmentReference* depth    =   subpass->depthStencilReference.attachment != VK_ATTACHMENT_UNUSED &&
                                                subpass->depthStencilReference.attachment == attachmentIt ?
                                                &subpass->depthStencilReference : NULL;
            if (!color && !input && !resolve && !depth)
            {
                continue;
            }
            if (color) se_assert(!depth && "Can't have same attachment as color and depth reference (in a single subpass)");
            if (depth) se_assert(!color && "Can't have same attachment as color and depth reference (in a single subpass)");
            if (color)
            {
                if (currentLayout != VK_IMAGE_LAYOUT_GENERAL) currentLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                if (!isUsed && currentLayout != initialLayout)
                {
                    externalColorDependencies |= subpassBitMask;
                }
                colorReadWriteDependencies |= subpassBitMask;
            }
            if (resolve)
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
                SeDepthOp depthOp = createInfo->subpasses[subpassIt].depthOp;
                se_assert(depthOp != SE_DEPTH_OP_NOTHING && "Subpass has depth attachment, but depth op is not specified. Something is wrong (?)");
                if (depthOp == SE_DEPTH_OP_READ_WRITE)
                {
                    if (currentLayout != VK_IMAGE_LAYOUT_GENERAL)
                    {
                        currentLayout = isStencilSupported
                            ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
                            : VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
                    }
                    depthStencilWriteDependencies |= subpassBitMask;
                }
                else if (depthOp == SE_DEPTH_OP_READ)
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
    const uint32_t allExternalDependencies = externalColorDependencies | externalInputDependencies | externalDepthDependencies;
    const uint32_t allSelfDependencies = selfColorDependencies | selfDepthDependencies;
    const uint32_t numSubpassDependencies = se_vk_render_pass_count_flags(allExternalDependencies) + se_vk_render_pass_count_flags(allSelfDependencies) + numSubpasses - 1;
    se_assert(numSubpassDependencies < SE_VK_RP_MAX_SUBPASS_DEPENDENCIES && "Max number of subpass dependecies is 64");
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
        VkSubpassDependency dependency = {0};
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
        VkSubpassDependency dependency = {0};
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
    VkRenderPassCreateInfo renderPassInfo = (VkRenderPassCreateInfo)
    {
        .sType              = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .pNext              = NULL,
        .flags              = 0,
        .attachmentCount    = numRenderPassAttachments,
        .pAttachments       = renderPassAttachmentDescriptions,
        .subpassCount       = numSubpasses,
        .pSubpasses         = subpassDescriptions,
        .dependencyCount    = numSubpassDependencies,
        .pDependencies      = subpassDependencies,
    };
    se_vk_check(vkCreateRenderPass(logicalHandle, &renderPassInfo, callbacks, &pass->handle));
    //
    // Fill subpass and attachment infos
    //
    pass->numSubpasses = createInfo->numSubpasses;
    for (size_t it = 0; it < pass->numSubpasses; it++)
    {
        SeRenderPassSubpass* subpassCreateInfo = &createInfo->subpasses[it];
        SeVkSubpassInfo* subpassInfo = &pass->subpassInfos[it];
        for (size_t refIt = 0; refIt < subpassCreateInfo->numInputRefs; refIt++) subpassInfo->inputAttachmentRefsBitmask |= 1 << subpassCreateInfo->inputRefs[refIt];
        for (size_t refIt = 0; refIt < subpassCreateInfo->numColorRefs; refIt++) subpassInfo->colorAttachmentRefsBitmask |= 1 << subpassCreateInfo->colorRefs[refIt];
    }
    pass->numAttachments = numRenderPassAttachments;
    for (size_t it = 0; it < pass->numAttachments; it++)
    {
        const VkFormat format = renderPassAttachmentDescriptions[it].format;
        pass->attachmentInfos[it] = (SeVkRenderPassAttachmentInfo)
        {
            .initialLayout  = renderPassAttachmentDescriptions[it].initialLayout,
            .finalLayout    = renderPassAttachmentDescriptions[it].finalLayout,
            .format         = format,
        };
        const bool isColorFormat =
            format == VK_FORMAT_D16_UNORM           ||
            format == VK_FORMAT_D32_SFLOAT          ||
            format == VK_FORMAT_D16_UNORM_S8_UINT   ||
            format == VK_FORMAT_D24_UNORM_S8_UINT   ||
            format == VK_FORMAT_D32_SFLOAT_S8_UINT  ? false : true;
        //
        // @NOTE : Engine always uses reverse depth, so .depthStencil is hardcoded to { 0, 0 }.
        //         If you want to change this, then se_vk_perspective_projection_matrix must be reevaluated in
        //         order to remap Z coord to the new range (instead of [1, 0]), as well as render pipeline depthCompareOp.
        //
        pass->clearValues[it] = isColorFormat
            ? (VkClearValue) { .color = { 0, 0, 0, 1, } }
            : (VkClearValue) { .depthStencil = { 0, 0 } };
    }
    return (SeRenderObject*)pass;
}

void se_vk_render_pass_submit_for_deffered_destruction(SeRenderObject* _pass)
{
    se_vk_expect_handle(_pass, SE_RENDER_HANDLE_TYPE_PASS, "Can't submit render pass for deffered destruction");
    SeVkInFlightManager* inFlightManager = se_vk_device_get_in_flight_manager(((SeVkRenderPass*)_pass)->device);
    se_vk_in_flight_manager_submit_deffered_destruction(inFlightManager, (SeVkDefferedDestruction) { _pass, se_vk_render_pass_destroy });
}

void se_vk_render_pass_destroy(SeRenderObject* _pass)
{
    se_vk_expect_handle(_pass, SE_RENDER_HANDLE_TYPE_PASS, "Can't destroy render pass");
    SeVkRenderPass* pass = (SeVkRenderPass*)_pass;
    SeVkMemoryManager* memoryManager = se_vk_device_get_memory_manager(pass->device);
    VkAllocationCallbacks* callbacks = se_vk_memory_manager_get_callbacks(memoryManager);
    vkDestroyRenderPass(se_vk_device_get_logical_handle(pass->device), pass->handle, callbacks);
    memoryManager->cpu_persistentAllocator->dealloc(memoryManager->cpu_persistentAllocator->allocator, pass, sizeof(SeVkRenderPass));
}

size_t se_vk_render_pass_get_num_subpasses(struct SeRenderObject* _pass)
{
    se_vk_expect_handle(_pass, SE_RENDER_HANDLE_TYPE_PASS, "Can't get num subpasses");
    SeVkRenderPass* pass = (SeVkRenderPass*)_pass;
    return pass->numSubpasses;
}

void se_vk_render_pass_validate_fragment_program_setup(SeRenderObject* _pass, SeRenderObject* fragmentProgram, size_t subpassIndex)
{
    se_vk_expect_handle(_pass, SE_RENDER_HANDLE_TYPE_PASS, "Can't validate fragment program setup");
    se_vk_expect_handle(fragmentProgram, SE_RENDER_HANDLE_TYPE_PROGRAM, "Can't validate fragment program setup");
    SeVkRenderPass* pass = (SeVkRenderPass*)_pass;
    SeVkSubpassInfo* subpass = &pass->subpassInfos[subpassIndex];
    SimpleSpirvReflection* reflection = se_vk_render_program_get_reflection(fragmentProgram);
    uint32_t shaderSubpassInputAttachmentMask = 0;
    for (size_t it = 0; it < reflection->numUniforms; it++)
    {
        SsrUniform* uniform = &reflection->uniforms[it];
        if (uniform->kind == SSR_UNIFORM_INPUT_ATTACHMENT)
        {
            se_assert(uniform->inputAttachmentIndex < 32);
            shaderSubpassInputAttachmentMask |= 1 << uniform->inputAttachmentIndex;
        }
    }
    se_assert((shaderSubpassInputAttachmentMask == subpass->inputAttachmentRefsBitmask) && "Mismatch between fragment shader subpass inputs and render pass input attachments");
    uint32_t shaderColorAttachmentMask = 0;
    for (size_t it = 0; it < reflection->numOutputs; it++)
    {
        SsrShaderIO* output = &reflection->outputs[it];
        if (!output->isBuiltIn)
        {
            se_assert(output->location < 32);
            shaderColorAttachmentMask |= 1 << output->location;
        }
    }
    se_assert((shaderColorAttachmentMask == subpass->colorAttachmentRefsBitmask) && "Mismatch between fragment shader outputs and render pass color attachments");
}

uint32_t se_vk_render_pass_get_num_color_attachments(SeRenderObject* _pass, size_t subpassIndex)
{
    se_vk_expect_handle(_pass, SE_RENDER_HANDLE_TYPE_PASS, "Can't get num render pass color attachments");
    SeVkRenderPass* pass = (SeVkRenderPass*)_pass;
    return se_vk_render_pass_count_flags(pass->subpassInfos[subpassIndex].colorAttachmentRefsBitmask);
}

VkRenderPass se_vk_render_pass_get_handle(struct SeRenderObject* _pass)
{
    se_vk_expect_handle(_pass, SE_RENDER_HANDLE_TYPE_PASS, "Can't get render pass handle");
    SeVkRenderPass* pass = (SeVkRenderPass*)_pass;
    return pass->handle;
}

void se_vk_render_pass_validate_framebuffer_textures(SeRenderObject* _pass, SeRenderObject** textures, size_t numTextures)
{
    se_vk_expect_handle(_pass, SE_RENDER_HANDLE_TYPE_PASS, "Can't validate framebuffer textures");
    SeVkRenderPass* pass = (SeVkRenderPass*)_pass;
    se_assert(pass->numAttachments == numTextures);
    for (size_t it = 0; it < numTextures; it++)
    {
        se_vk_expect_handle(textures[it], SE_RENDER_HANDLE_TYPE_TEXTURE, "Can't validate framebuffer textures");
        se_assert(pass->attachmentInfos[it].format == se_vk_texture_get_format(textures[it]));
    }
}

VkRenderPassBeginInfo se_vk_render_pass_get_begin_info(SeRenderObject* _pass, SeRenderObject* framebuffer, VkRect2D renderArea)
{
    se_vk_expect_handle(_pass, SE_RENDER_HANDLE_TYPE_PASS, "Can't get render pass begin info");
    se_vk_expect_handle(framebuffer, SE_RENDER_HANDLE_TYPE_FRAMEBUFFER, "Can't get render pass begin info");
    SeVkRenderPass* pass = (SeVkRenderPass*)_pass;
    return (VkRenderPassBeginInfo)
    {
        .sType              = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext              = NULL,
        .renderPass         = pass->handle,
        .framebuffer        = se_vk_framebuffer_get_handle(framebuffer),
        .renderArea         = renderArea,
        .clearValueCount    = (uint32_t)pass->numAttachments, // @TODO : safe cast
        .pClearValues       = pass->clearValues,
    };
}

VkImageLayout se_vk_render_pass_get_initial_attachment_layout(SeRenderObject* _pass, size_t attachmentIndex)
{
    se_vk_expect_handle(_pass, SE_RENDER_HANDLE_TYPE_PASS, "Can't get render pass initial attachment layout");
    SeVkRenderPass* pass = (SeVkRenderPass*)_pass;
    return pass->attachmentInfos[attachmentIndex].initialLayout;
}

#undef se_vk_rp_is_bit_set
#undef se_vk_rp_for_each_set_bit
#undef se_vk_rp_for_each_bit
