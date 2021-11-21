
#include "se_vulkan_render_subsystem_render_pipeline.h"
#include "se_vulkan_render_subsystem_base.h"
#include "se_vulkan_render_subsystem_device.h"
#include "se_vulkan_render_subsystem_memory.h"
#include "se_vulkan_render_subsystem_utils.h"
#include "se_vulkan_render_subsystem_render_program.h"
#include "se_vulkan_render_subsystem_render_pass.h"
#include "engine/libs/ssr/simple_spirv_reflection.h"
#include "engine/allocator_bindings.h"
#include "engine/render_abstraction_interface.h"
#include "engine/containers.h"

#define SE_VK_MAX_POOL_SIZES 32
#define SE_VK_MAX_DESCRIPTOR_SET_LAYOUTS 8
#define SE_VK_MAX_POOL_SETS 64

typedef struct SeVkDescriptorPool
{
    VkDescriptorPool handle;
    size_t numAllocations;
    bool isLastAllocationSuccessful;
} SeVkDescriptorPool;

typedef struct SeVkDescriptorSetLayout
{
    se_sbuffer(SeVkDescriptorPool) pools;
    VkDescriptorPoolSize poolSizes[SE_VK_MAX_POOL_SIZES];
    size_t numPoolSizes;
    VkDescriptorPoolCreateInfo poolCreateInfo;
    VkDescriptorSetLayout handle;
} SeVkDescriptorSetLayout;

typedef struct SeVkRenderPipeline
{
    SeRenderObject object;
    SeRenderObject* device;
    SeRenderObject* renderPass;
    VkPipeline handle;
    VkPipelineLayout layout;
    VkPipelineBindPoint bindPoint;
    SeVkDescriptorSetLayout descriptorSetLayouts[SE_VK_MAX_DESCRIPTOR_SET_LAYOUTS];
    size_t numDescriptorSetLayouts;
} SeVkRenderPipeline;

typedef struct SeVkDescriptorSetLayoutCreateInfos
{
    se_sbuffer(VkDescriptorSetLayoutCreateInfo) createInfos;
    se_sbuffer(VkDescriptorSetLayoutBinding) bindings;
} SeVkDescriptorSetLayoutCreateInfos;

bool se_vk_render_pipeline_has_vertex_input(const SimpleSpirvReflection* reflection)
{
    for (size_t it = 0; it < reflection->numInputs; it++)
    {
        const SsrShaderIO* input = &reflection->inputs[it];
        if (!input->isBuiltIn) return true;
    }
    return false;
}

SeVkDescriptorSetLayoutCreateInfos se_vk_render_pipeline_get_discriptor_set_layout_create_infos(SeAllocatorBindings* allocator, const SimpleSpirvReflection** programReflections, size_t numProgramReflections)
{
    uint32_t setBindingMasks[32] = {0};
    //
    // Fill binding masks
    //
    for (size_t it = 0; it < numProgramReflections; it++)
    {
        const SimpleSpirvReflection* reflection = programReflections[it];
        for (size_t uniformIt = 0; uniformIt < reflection->numUniforms; uniformIt++)
        {
            const SsrUniform* uniform = &reflection->uniforms[uniformIt];
            se_assert(uniform->set < 32);
            se_assert(uniform->binding < 32);
            setBindingMasks[uniform->set] |= 1 << uniform->binding;
        }
    }
    bool isEmptySetFound = false;
    for (size_t it = 0; it < se_array_size(setBindingMasks); it++)
    {
        se_assert((!isEmptySetFound || !setBindingMasks[it]) && "Empty descriptor sets in between of non-empty ones are not supported");
        isEmptySetFound = isEmptySetFound || !setBindingMasks[it];
    }
    //
    // Allocate memory
    //
    size_t numLayouts = 0;
    size_t numBindings = 0;
    for (size_t it = 0; it < se_array_size(setBindingMasks); it++)
    {
        if (!setBindingMasks[it]) continue;
        numLayouts += 1;
        for (size_t maskIt = 0; maskIt < 32; maskIt++)
        {
            if (setBindingMasks[it] & (1 << maskIt)) numBindings += 1;
        }
    }
    se_sbuffer(VkDescriptorSetLayoutCreateInfo) layoutCreateInfos = NULL;
    se_sbuffer(VkDescriptorSetLayoutBinding) bindings = NULL;
    se_sbuffer_construct(layoutCreateInfos, numLayouts, allocator);
    se_sbuffer_construct(bindings, numBindings, allocator);
    se_sbuffer_set_size(layoutCreateInfos, numLayouts);
    se_sbuffer_set_size(bindings, numBindings);
    // ~((uint32_t)0) is an unused binding
    for (size_t it = 0; it < numBindings; it++) bindings[it].binding = ~((uint32_t)0);
    //
    // Set layout create infos
    //
    {
        size_t layoutCreateInfosIt = 0;
        size_t layoutBindingsIt = 0;
        for (size_t it = 0; it < se_array_size(setBindingMasks); it++)
        {
            if (!setBindingMasks[it]) continue;
            uint32_t numBindingsInSet = 0;
            for (size_t maskIt = 0; maskIt < 32; maskIt++)
            {
                if (setBindingMasks[it] & (1 << maskIt)) numBindingsInSet += 1;
            }
            layoutCreateInfos[layoutCreateInfosIt++] = (VkDescriptorSetLayoutCreateInfo)
            {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .pNext = NULL,
                .flags = 0,
                .bindingCount = numBindingsInSet,
                .pBindings = &bindings[layoutBindingsIt],
            };
            layoutBindingsIt += numBindingsInSet;
        }
    }
    //
    // Fill binding data
    //
    for (size_t it = 0; it < numProgramReflections; it++)
    {
        const SimpleSpirvReflection* reflection = programReflections[it];
        se_assert(reflection->type == SSR_SHADER_VERTEX || reflection->type == SSR_SHADER_FRAGMENT);
        const VkShaderStageFlags programStage =
            reflection->type == SSR_SHADER_VERTEX ? VK_SHADER_STAGE_VERTEX_BIT :
            reflection->type == SSR_SHADER_FRAGMENT ? VK_SHADER_STAGE_FRAGMENT_BIT :
            0;
        for (size_t uniformIt = 0; uniformIt < reflection->numUniforms; uniformIt++)
        {
            const SsrUniform* uniform = &reflection->uniforms[uniformIt];
            const VkDescriptorType uniformDescriptorType =
                SSR_UNIFORM_SAMPLER                 ? VK_DESCRIPTOR_TYPE_SAMPLER :
                SSR_UNIFORM_SAMPLED_IMAGE           ? VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE :
                SSR_UNIFORM_STORAGE_IMAGE           ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE :
                SSR_UNIFORM_COMBINED_IMAGE_SAMPLER  ? VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER :
                SSR_UNIFORM_UNIFORM_TEXEL_BUFFER    ? VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER :
                SSR_UNIFORM_STORAGE_TEXEL_BUFFER    ? VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER :
                SSR_UNIFORM_UNIFORM_BUFFER          ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER :
                SSR_UNIFORM_STORAGE_BUFFER          ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER :
                SSR_UNIFORM_INPUT_ATTACHMENT        ? VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT :
                SSR_UNIFORM_ACCELERATION_STRUCTURE  ? VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR :
                ~0;
            se_assert(uniformDescriptorType != ~0);
            VkDescriptorSetLayoutCreateInfo* layoutCreateInfo = &layoutCreateInfos[uniform->set];
            const size_t bindingsArrayInitialOffset = layoutCreateInfo->pBindings - bindings;
            VkDescriptorSetLayoutBinding* binding = NULL;
            for (size_t bindingsIt = 0; bindingsIt < layoutCreateInfo->bindingCount; bindingsIt++)
            {
                VkDescriptorSetLayoutBinding* candidate = &bindings[bindingsArrayInitialOffset + it];
                if (candidate->binding == ~((uint32_t)0) || candidate->binding == uniform->binding)
                {
                    binding = candidate;
                    break;
                }
            }
            se_assert(binding);
            const uint32_t uniformDescriptorCount =
                uniform->type->type == SSR_TYPE_ARRAY &&
                !uniform->type->info.array.isRuntimeArray
                ? (uint32_t)(uniform->type->info.array.size) // @TODO : safe cast
                : 1;
            if (binding->binding != ~((uint32_t)0))
            {
                se_assert
                (
                    binding->descriptorType == uniformDescriptorType &&
                    binding->descriptorCount == uniformDescriptorCount &&
                    "Same binding at the same location must have the same descriptor type in each shader"
                );
                binding->stageFlags |= programStage;
            }
            else
            {
                binding->binding = uniform->binding;
                binding->descriptorType = uniformDescriptorType;
                binding->descriptorCount = uniformDescriptorCount;
                binding->stageFlags = programStage;
            }
        }
    }
    return (SeVkDescriptorSetLayoutCreateInfos)
    {
        layoutCreateInfos,
        bindings
    };
}

void se_vk_render_pipeline_destroy_descriptor_set_layout_create_infos(SeVkDescriptorSetLayoutCreateInfos* infos)
{
    se_sbuffer_destroy(infos->createInfos);
    se_sbuffer_destroy(infos->bindings);
}

void se_vk_render_pipeline_add_pool(SeVkDescriptorSetLayout* layout, SeRenderObject* device)
{
    SeVkMemoryManager* memoryManager = se_vk_device_get_memory_manager(device);
    VkAllocationCallbacks* callbacks = se_vk_memory_manager_get_callbacks(memoryManager);
    VkDevice logicalHandle = se_vk_device_get_logical_handle(device);
    SeVkDescriptorPool pool = {0};
    se_vk_check(vkCreateDescriptorPool(logicalHandle, &layout->poolCreateInfo, callbacks, &pool.handle));
    pool.isLastAllocationSuccessful = true;
    pool.numAllocations = 0;
    se_sbuffer_push(layout->pools, pool);
}

VkStencilOpState se_vk_render_pipeline_stencil_op_state(SeStencilOpState* state)
{
    return (VkStencilOpState)
    {
        .failOp         = se_vk_utils_to_vk_stencil_op(state->failOp),
        .passOp         = se_vk_utils_to_vk_stencil_op(state->passOp),
        .depthFailOp    = se_vk_utils_to_vk_stencil_op(state->depthFailOp),
        .compareOp      = se_vk_utils_to_vk_compare_op(state->compareOp),
        .compareMask    = state->compareMask,
        .writeMask      = state->writeMask,
        .reference      = state->reference,
    };
}

SeRenderObject* se_vk_render_pipeline_graphics_create(SeGraphicsRenderPipelineCreateInfo* createInfo)
{
    SeRenderObject* device = createInfo->device;
    SeVkMemoryManager* memoryManager = se_vk_device_get_memory_manager(device);
    VkAllocationCallbacks* callbacks = se_vk_memory_manager_get_callbacks(memoryManager);
    SeAllocatorBindings* allocator = memoryManager->cpu_allocator;
    VkDevice logicalHandle = se_vk_device_get_logical_handle(device);
    //
    // Initial setup
    //
    SeVkRenderPipeline* pipeline = allocator->alloc(allocator->allocator, sizeof(SeVkRenderPipeline), se_default_alignment);
    pipeline->object.handleType = SE_RENDER_PIPELINE;
    pipeline->object.destroy = se_vk_render_pipeline_destroy;
    pipeline->device = device;
    pipeline->renderPass = createInfo->renderPass;
    pipeline->bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    //
    // Basic info and validation
    //
    const SimpleSpirvReflection* vertexReflection = se_vk_render_program_get_reflection(createInfo->vertexProgram);
    const SimpleSpirvReflection* fragmentReflection = se_vk_render_program_get_reflection(createInfo->fragmentProgram);
    const bool isStencilSupported = se_vk_device_is_stencil_supported(device);
    const bool hasVertexInput = se_vk_render_pipeline_has_vertex_input(vertexReflection);
    se_assert(!hasVertexInput && "Vertex shader inputs are not supported");
    se_assert(!vertexReflection->pushConstantType && "Push constants are not supported");
    se_assert(!fragmentReflection->pushConstantType && "Push constants are not supported");
    se_assert((createInfo->subpassIndex < se_vk_render_pass_get_num_subpasses(createInfo->renderPass)) && "Incorrect pipeline subpass index");
    se_vk_render_pass_validate_fragment_program_setup(createInfo->renderPass, createInfo->fragmentProgram, createInfo->subpassIndex);
    //
    // Descriptor set layouts and pools
    //
    {
        const SimpleSpirvReflection* reflections[] = { vertexReflection, fragmentReflection };
        SeVkDescriptorSetLayoutCreateInfos layoutCreateInfos = se_vk_render_pipeline_get_discriptor_set_layout_create_infos(allocator, reflections, 2);
        pipeline->numDescriptorSetLayouts = se_sbuffer_size(layoutCreateInfos.createInfos);
        se_assert(pipeline->numDescriptorSetLayouts < se_array_size(pipeline->descriptorSetLayouts));
        for (size_t it = 0; it < se_sbuffer_size(layoutCreateInfos.createInfos); it++)
        {
            VkDescriptorSetLayoutCreateInfo* layoutCreateInfo = &layoutCreateInfos.createInfos[it];
            SeVkDescriptorSetLayout* layout = &pipeline->descriptorSetLayouts[it];
            se_vk_check(vkCreateDescriptorSetLayout(logicalHandle, layoutCreateInfo, callbacks, &layout->handle));
            size_t numUniqueDescriptorTypes = 0;
            uint32_t descriptorTypeMask = 0;
            for (size_t bindingIt = 0; bindingIt < layoutCreateInfo->bindingCount; bindingIt++)
            {
                const VkDescriptorType descriptorType = layoutCreateInfo->pBindings[bindingIt].descriptorType;
                const uint32_t flag =
                        descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER                    ? 1 << 0 :
                        descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER     ? 1 << 1 :
                        descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE              ? 1 << 2 :
                        descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE              ? 1 << 3 :
                        descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER       ? 1 << 4 :
                        descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER       ? 1 << 5 :
                        descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER             ? 1 << 6 :
                        descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER             ? 1 << 7 :
                        descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC     ? 1 << 8 :
                        descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC     ? 1 << 9 :
                        descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT           ? 1 << 10 :
                        descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT   ? 1 << 11 :
                        descriptorType == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR ? 1 << 12 :
                        descriptorType == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV  ? 1 << 13 :
                        descriptorType == VK_DESCRIPTOR_TYPE_MUTABLE_VALVE              ? 1 << 14 :
                        0;
                se_assert(flag);
                if (!(descriptorTypeMask & flag))
                {
                    numUniqueDescriptorTypes += 1;
                    descriptorTypeMask |= flag;
                }
            }
            layout->numPoolSizes = numUniqueDescriptorTypes;
            for (size_t sizeIt = 0; sizeIt < layout->numPoolSizes; sizeIt++) layout->poolSizes[sizeIt].type = VK_DESCRIPTOR_TYPE_MAX_ENUM;
            for (size_t bindingIt = 0; bindingIt < layoutCreateInfo->bindingCount; bindingIt++)
            {
                const VkDescriptorSetLayoutBinding* binding = &layoutCreateInfo->pBindings[bindingIt];
                for (size_t poolSizeIt = 0; poolSizeIt < layout->numPoolSizes; poolSizeIt++)
                {
                    VkDescriptorPoolSize* poolSize = &layout->poolSizes[poolSizeIt];
                    if (poolSize->type == VK_DESCRIPTOR_TYPE_MAX_ENUM)
                    {
                        poolSize->type = binding->descriptorType;
                        poolSize->descriptorCount = binding->descriptorCount * SE_VK_MAX_POOL_SETS;
                    }
                    else if (poolSize->type == binding->descriptorType)
                    {
                        poolSize->descriptorCount += binding->descriptorCount * SE_VK_MAX_POOL_SETS;
                    }
                }
            }
            layout->poolCreateInfo = (VkDescriptorPoolCreateInfo)
            {
                .sType          = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                .pNext          = NULL,
                .flags          = 0,
                .maxSets        = SE_VK_MAX_POOL_SETS,
                .poolSizeCount  = (uint32_t)layout->numPoolSizes, // @TODO : safe cast
                .pPoolSizes     = layout->poolSizes,
            };
            se_sbuffer_construct(&layout->pools, 4, allocator);
            se_vk_render_pipeline_add_pool(layout, device);
            se_vk_render_pipeline_destroy_descriptor_set_layout_create_infos(&layoutCreateInfos);
        }
    }
    //
    // Pipeline layout
    //
    {
        VkDescriptorSetLayout descriptorSetLayoutHandles[SE_VK_MAX_DESCRIPTOR_SET_LAYOUTS] = {0};
        for (size_t it = 0; it < pipeline->numDescriptorSetLayouts; it++)
        {
            descriptorSetLayoutHandles[it] = pipeline->descriptorSetLayouts[it].handle;
        }
        VkPipelineLayoutCreateInfo pipelineLayoutInfo = (VkPipelineLayoutCreateInfo)
        {
            .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pNext                  = NULL,
            .flags                  = 0,
            .setLayoutCount         = (uint32_t)pipeline->numDescriptorSetLayouts, // @TODO : safe cast
            .pSetLayouts            = descriptorSetLayoutHandles,
            .pushConstantRangeCount = 0,
            .pPushConstantRanges    = NULL,
        };
        se_vk_check(vkCreatePipelineLayout(logicalHandle, &pipelineLayoutInfo, callbacks, &pipeline->layout));
    }
    //
    // Render pipeline
    //
    {
        VkPipelineShaderStageCreateInfo shaderStages[] =
        {
            se_vk_render_program_get_shader_stage_create_info(createInfo->vertexProgram),
            se_vk_render_program_get_shader_stage_create_info(createInfo->fragmentProgram),
        };
        VkExtent2D swapChainExtent = se_vk_device_get_swap_chain_extent(device);
        VkPipelineVertexInputStateCreateInfo vertexInputInfo = se_vk_utils_vertex_input_state_create_info(0, NULL, 0, NULL);
        VkPipelineInputAssemblyStateCreateInfo inputAssembly = se_vk_utils_input_assembly_state_create_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE);
        SeVkViewportScissor viewportScissor = se_vk_utils_default_viewport_scissor(swapChainExtent.width, swapChainExtent.height);
        VkPipelineViewportStateCreateInfo viewportState = se_vk_utils_viewport_state_create_info(&viewportScissor.viewport, &viewportScissor.scissor);
        VkPipelineRasterizationStateCreateInfo rasterizationState = se_vk_utils_rasterization_state_create_info(se_vk_utils_to_vk_polygon_mode(createInfo->poligonMode), se_vk_utils_to_vk_cull_mode(createInfo->cullMode), se_vk_utils_to_vk_front_face(createInfo->frontFace));
        VkPipelineMultisampleStateCreateInfo multisampleState = se_vk_utils_multisample_state_create_info(se_vk_utils_pick_sample_count(se_vk_utils_to_vk_sample_count(createInfo->multisamplingType), se_vk_device_get_supported_framebuffer_multisample_types(device)));
        VkStencilOpState frontStencilOpState = isStencilSupported && createInfo->frontStencilOpState ? se_vk_render_pipeline_stencil_op_state(createInfo->frontStencilOpState) : (VkStencilOpState){0};
        VkStencilOpState backStencilOpState = isStencilSupported && createInfo->backStencilOpState ? se_vk_render_pipeline_stencil_op_state(createInfo->backStencilOpState) : (VkStencilOpState){0};
        VkPipelineDepthStencilStateCreateInfo depthStencilState = (VkPipelineDepthStencilStateCreateInfo)
        {
            .sType                  = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .pNext                  = NULL,
            .flags                  = 0,
            .depthTestEnable        = createInfo->depthTestState ? se_vk_utils_to_vk_bool(createInfo->depthTestState->isTestEnabled) : VK_FALSE,
            .depthWriteEnable       = createInfo->depthTestState ? se_vk_utils_to_vk_bool(createInfo->depthTestState->isWriteEnabled) : VK_FALSE,
            .depthCompareOp         = createInfo->depthTestState ? se_vk_utils_to_vk_compare_op(createInfo->depthTestState->compareOp) : VK_COMPARE_OP_ALWAYS,
            .depthBoundsTestEnable  = createInfo->depthTestState ? se_vk_utils_to_vk_bool(createInfo->depthTestState->isBoundsTestEnabled) : VK_FALSE,
            .stencilTestEnable      = se_vk_utils_to_vk_bool(isStencilSupported && (createInfo->frontStencilOpState || createInfo->backStencilOpState)),
            .front                  = frontStencilOpState,
            .back                   = backStencilOpState,
            .minDepthBounds         = createInfo->depthTestState ? createInfo->depthTestState->minDepthBounds : 0.0f,
            .maxDepthBounds         = createInfo->depthTestState ? createInfo->depthTestState->maxDepthBounds : 1.0f,
        };
        VkPipelineColorBlendAttachmentState colorBlendAttachments[32] = { 0 /* COLOR BLENDING IS UNSUPPORTED */ };
        for (size_t it = 0; it < se_array_size(colorBlendAttachments); it++) colorBlendAttachments[it].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        uint32_t numColorAttachments = se_vk_render_pass_get_num_color_attachments(createInfo->renderPass, createInfo->subpassIndex);
        VkPipelineColorBlendStateCreateInfo colorBlending = se_vk_utils_color_blending_create_info(colorBlendAttachments, numColorAttachments);
        VkPipelineDynamicStateCreateInfo dynamicState = se_vk_utils_dynamic_state_default_create_info();
        VkGraphicsPipelineCreateInfo pipelineCreateInfo = (VkGraphicsPipelineCreateInfo)
        {
            .sType                  = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .pNext                  = NULL,
            .flags                  = 0,
            .stageCount             = se_array_size(shaderStages),
            .pStages                = shaderStages,
            .pVertexInputState      = &vertexInputInfo,
            .pInputAssemblyState    = &inputAssembly,
            .pTessellationState     = NULL,
            .pViewportState         = &viewportState,
            .pRasterizationState    = &rasterizationState,
            .pMultisampleState      = &multisampleState,
            .pDepthStencilState     = &depthStencilState,
            .pColorBlendState       = &colorBlending,
            .pDynamicState          = &dynamicState,
            .layout                 = pipeline->layout,
            .renderPass             = se_vk_render_pass_get_handle(createInfo->renderPass),
            .subpass                = (uint32_t)createInfo->subpassIndex, // @TODO : safe cast
            .basePipelineHandle     = VK_NULL_HANDLE,
            .basePipelineIndex      = -1,
        };
        se_vk_check(vkCreateGraphicsPipelines(logicalHandle, VK_NULL_HANDLE, 1, &pipelineCreateInfo, callbacks, &pipeline->handle));
    }
    return (SeRenderObject*)pipeline;
}

void se_vk_render_pipeline_destroy(SeRenderObject* _pipeline)
{
    SeVkRenderPipeline* pipeline = (SeVkRenderPipeline*)_pipeline;
    VkDevice logicalHandle = se_vk_device_get_logical_handle(pipeline->device);
    SeVkMemoryManager* memoryManager = se_vk_device_get_memory_manager(pipeline->device);
    VkAllocationCallbacks* callbacks = se_vk_memory_manager_get_callbacks(memoryManager);

    for (size_t layoutIt = 0; layoutIt < pipeline->numDescriptorSetLayouts; layoutIt++)
    {
        SeVkDescriptorSetLayout* layout = &pipeline->descriptorSetLayouts[layoutIt];
        for (size_t poolIt = 0; poolIt < se_sbuffer_size(layout->pools); poolIt++)
        {
            SeVkDescriptorPool* pool = &layout->pools[poolIt];
            se_assert(pool->numAllocations == 0 && "All descriptor sets must be destroyed before the pipeline");
            vkDestroyDescriptorPool(logicalHandle, pool->handle, callbacks);
        }
        se_sbuffer_destroy(&layout->pools);
        vkDestroyDescriptorSetLayout(logicalHandle, layout->handle, callbacks);
    }
    vkDestroyPipeline(logicalHandle, pipeline->handle, callbacks);
    vkDestroyPipelineLayout(logicalHandle, pipeline->layout, callbacks);
    memoryManager->cpu_allocator->dealloc(memoryManager->cpu_allocator->allocator, pipeline, sizeof(SeVkRenderPipeline));
}

SeRenderObject* se_vk_render_pipeline_get_render_pass(SeRenderObject* _pipeline)
{
    SeVkRenderPipeline* pipeline = (SeVkRenderPipeline*)_pipeline;
    return pipeline->renderPass;
}

VkPipelineBindPoint se_vk_render_pipeline_get_bind_point(SeRenderObject* _pipeline)
{
    SeVkRenderPipeline* pipeline = (SeVkRenderPipeline*)_pipeline;
    return pipeline->bindPoint;
}

VkPipeline se_vk_render_pipeline_get_handle(SeRenderObject* _pipeline)
{
    SeVkRenderPipeline* pipeline = (SeVkRenderPipeline*)_pipeline;
    return pipeline->handle;
}
