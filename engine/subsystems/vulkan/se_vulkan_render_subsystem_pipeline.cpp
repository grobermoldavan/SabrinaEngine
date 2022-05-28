
#include "se_vulkan_render_subsystem_pipeline.hpp"
#include "se_vulkan_render_subsystem_base.hpp"
#include "se_vulkan_render_subsystem_device.hpp"
#include "se_vulkan_render_subsystem_memory.hpp"
#include "se_vulkan_render_subsystem_utils.hpp"
#include "se_vulkan_render_subsystem_program.hpp"
#include "se_vulkan_render_subsystem_render_pass.hpp"
#include "engine/subsystems/se_application_allocators_subsystem.hpp"
#include "engine/libs/ssr/simple_spirv_reflection.h"
#include "engine/allocator_bindings.hpp"
#include "engine/containers.hpp"

#define SE_VK_RENDER_PIPELINE_NUMBER_OF_SETS_IN_POOL 32

struct SeVkDescriptorSetLayoutCreateInfos
{
    DynamicArray<VkDescriptorSetLayoutCreateInfo> createInfos;
    DynamicArray<VkDescriptorSetLayoutBinding> bindings;
};

static size_t g_pipelineIndex = 0;

static bool se_vk_pipeline_has_vertex_input(const SimpleSpirvReflection* reflection)
{
    for (size_t it = 0; it < reflection->numInputs; it++)
    {
        const SsrShaderIO* input = &reflection->inputs[it];
        if (!input->isBuiltIn) return true;
    }
    return false;
}

static SeVkDescriptorSetLayoutCreateInfos se_vk_pipeline_get_discriptor_set_layout_create_infos(AllocatorBindings& allocator, const SimpleSpirvReflection** programReflections, size_t numProgramReflections)
{
    SeVkGeneralBitmask setBindingMasks[SE_VK_RENDER_PIPELINE_MAX_DESCRIPTOR_SETS] = {0};
    //
    // Fill binding masks
    //
    for (size_t it = 0; it < numProgramReflections; it++)
    {
        const SimpleSpirvReflection* reflection = programReflections[it];
        for (size_t uniformIt = 0; uniformIt < reflection->numUniforms; uniformIt++)
        {
            const SsrUniform* uniform = &reflection->uniforms[uniformIt];
            se_assert(uniform->set < SE_VK_RENDER_PIPELINE_MAX_DESCRIPTOR_SETS);
            se_assert(uniform->binding < SE_VK_GENERAL_BITMASK_WIDTH);
            setBindingMasks[uniform->set] |= 1 << uniform->binding;
        }
    }
    bool isEmptySetFound = false;
    for (size_t it = 0; it < SE_VK_RENDER_PIPELINE_MAX_DESCRIPTOR_SETS; it++)
    {
        se_assert((!isEmptySetFound || !setBindingMasks[it]) && "Empty descriptor sets in between of non-empty ones are not supported");
        isEmptySetFound = isEmptySetFound || !setBindingMasks[it];
    }
    //
    // Allocate memory
    //
    size_t numLayouts = 0;
    size_t numBindings = 0;
    for (size_t it = 0; it < SE_VK_RENDER_PIPELINE_MAX_DESCRIPTOR_SETS; it++)
    {
        if (!setBindingMasks[it]) continue;
        numLayouts += 1;
        for (size_t maskIt = 0; maskIt < SE_VK_GENERAL_BITMASK_WIDTH; maskIt++)
        {
            if (setBindingMasks[it] & (1 << maskIt)) numBindings += 1;
        }
    }
    DynamicArray<VkDescriptorSetLayoutCreateInfo> layoutCreateInfos;
    DynamicArray<VkDescriptorSetLayoutBinding> bindings;
    dynamic_array::construct(layoutCreateInfos, allocator, numLayouts);
    dynamic_array::construct(bindings, allocator, numBindings);
    // ~((uint32_t)0) is an unused binding
    for (size_t it = 0; it < numBindings; it++)
    {
        dynamic_array::push(bindings,
        {
            .binding            = ~((uint32_t)0),
            .descriptorType     = (VkDescriptorType)0,
            .descriptorCount    = 0,
            .stageFlags         = (VkShaderStageFlags)0,
            .pImmutableSamplers = nullptr,
        });
    }
    //
    // Set layout create infos
    //
    {
        size_t layoutBindingsIt = 0;
        for (size_t it = 0; it < SE_VK_RENDER_PIPELINE_MAX_DESCRIPTOR_SETS; it++)
        {
            if (!setBindingMasks[it]) continue;
            uint32_t numBindingsInSet = 0;
            for (size_t maskIt = 0; maskIt < 32; maskIt++)
            {
                if (setBindingMasks[it] & (1 << maskIt)) numBindingsInSet += 1;
            }
            dynamic_array::push(layoutCreateInfos,
            {
                .sType          = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .pNext          = nullptr,
                .flags          = 0,
                .bindingCount   = numBindingsInSet,
                .pBindings      = &bindings[layoutBindingsIt],
            });
            layoutBindingsIt += numBindingsInSet;
        }
    }
    //
    // Fill binding data
    //
    for (size_t it = 0; it < numProgramReflections; it++)
    {
        const SimpleSpirvReflection* reflection = programReflections[it];
        const VkShaderStageFlags programStage =
            reflection->shaderType == SSR_SHADER_TYPE_VERTEX ? VK_SHADER_STAGE_VERTEX_BIT :
            reflection->shaderType == SSR_SHADER_TYPE_FRAGMENT ? VK_SHADER_STAGE_FRAGMENT_BIT :
            reflection->shaderType == SSR_SHADER_TYPE_COMPUTE ? VK_SHADER_STAGE_COMPUTE_BIT :
            0;
        for (size_t uniformIt = 0; uniformIt < reflection->numUniforms; uniformIt++)
        {
            const SsrUniform* uniform = &reflection->uniforms[uniformIt];
            const VkDescriptorType uniformDescriptorType =
                uniform->kind == SSR_UNIFORM_SAMPLER                 ? (VkDescriptorType)VK_DESCRIPTOR_TYPE_SAMPLER :
                uniform->kind == SSR_UNIFORM_SAMPLED_IMAGE           ? (VkDescriptorType)VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE :
                uniform->kind == SSR_UNIFORM_STORAGE_IMAGE           ? (VkDescriptorType)VK_DESCRIPTOR_TYPE_STORAGE_IMAGE :
                uniform->kind == SSR_UNIFORM_COMBINED_IMAGE_SAMPLER  ? (VkDescriptorType)VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER :
                uniform->kind == SSR_UNIFORM_UNIFORM_TEXEL_BUFFER    ? (VkDescriptorType)VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER :
                uniform->kind == SSR_UNIFORM_STORAGE_TEXEL_BUFFER    ? (VkDescriptorType)VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER :
                uniform->kind == SSR_UNIFORM_UNIFORM_BUFFER          ? (VkDescriptorType)VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER :
                uniform->kind == SSR_UNIFORM_STORAGE_BUFFER          ? (VkDescriptorType)VK_DESCRIPTOR_TYPE_STORAGE_BUFFER :
                uniform->kind == SSR_UNIFORM_INPUT_ATTACHMENT        ? (VkDescriptorType)VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT :
                uniform->kind == SSR_UNIFORM_ACCELERATION_STRUCTURE  ? (VkDescriptorType)VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR :
                (VkDescriptorType)~0;
            se_assert(uniformDescriptorType != ~0);
            VkDescriptorSetLayoutCreateInfo* layoutCreateInfo = &layoutCreateInfos[uniform->set];
            const size_t bindingsArrayInitialOffset = layoutCreateInfo->pBindings - dynamic_array::raw(bindings);
            VkDescriptorSetLayoutBinding* binding = nullptr;
            for (size_t bindingsIt = 0; bindingsIt < layoutCreateInfo->bindingCount; bindingsIt++)
            {
                VkDescriptorSetLayoutBinding* candidate = &bindings[bindingsArrayInitialOffset + bindingsIt];
                if (candidate->binding == ~((uint32_t)0) || candidate->binding == uniform->binding)
                {
                    binding = candidate;
                    break;
                }
            }
            se_assert(binding);
            const uint32_t uniformDescriptorCount =
                ssr_is_array(uniform->type) && ssr_get_array_size_kind(uniform->type) == SSR_ARRAY_SIZE_CONSTANT
                ? (uint32_t)(uniform->type->info.array.size) // @TODO : safe cast
                : 1;
            se_assert_msg(ssr_get_array_size_kind(uniform->type) != SSR_ARRAY_SIZE_SPECIALIZATION_CONSTANT_BASED, "todo : specialization constants support for arrays");
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
    return
    {
        layoutCreateInfos,
        bindings
    };
}

static void se_vk_pipeline_destroy_descriptor_set_layout_create_infos(SeVkDescriptorSetLayoutCreateInfos* infos)
{
    dynamic_array::destroy(infos->createInfos);
    dynamic_array::destroy(infos->bindings);
}

static void se_vk_pipeline_create_descriptor_sets_and_layout(SeVkPipeline* pipeline, const SimpleSpirvReflection** reflections, size_t numReflections)
{
    SeVkDevice* device = pipeline->device;
    VkDevice logicalHandle = se_vk_device_get_logical_handle(device);
    SeVkMemoryManager* memoryManager = &device->memoryManager;
    VkAllocationCallbacks* callbacks = se_vk_memory_manager_get_callbacks(memoryManager);
    AllocatorBindings frameAllocator = app_allocators::frame();
    //
    // Descriptor set layouts and pools
    //
    {
        SeVkDescriptorSetLayoutCreateInfos layoutCreateInfos = se_vk_pipeline_get_discriptor_set_layout_create_infos(frameAllocator, reflections, numReflections);
        pipeline->numDescriptorSetLayouts = dynamic_array::size(layoutCreateInfos.createInfos);
        for (size_t it = 0; it < pipeline->numDescriptorSetLayouts; it++)
        {
            VkDescriptorSetLayoutCreateInfo* layoutCreateInfo = &layoutCreateInfos.createInfos[it];
            SeVkDescriptorSetLayout* layout = &pipeline->descriptorSetLayouts[it];
            layout->numBindings = layoutCreateInfo->bindingCount;
            se_vk_check(vkCreateDescriptorSetLayout(logicalHandle, layoutCreateInfo, callbacks, &layout->handle));
            size_t numUniqueDescriptorTypes = 0;
            uint32_t descriptorTypeMask = 0;
            for (size_t bindingIt = 0; bindingIt < layoutCreateInfo->bindingCount; bindingIt++)
            {
                const VkDescriptorType descriptorType = layoutCreateInfo->pBindings[bindingIt].descriptorType;
                layout->bindingInfos[bindingIt] =
                {
                    .descriptorType = descriptorType,
                };
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
                        poolSize->descriptorCount = binding->descriptorCount * SE_VK_RENDER_PIPELINE_NUMBER_OF_SETS_IN_POOL;
                    }
                    else if (poolSize->type == binding->descriptorType)
                    {
                        poolSize->descriptorCount += binding->descriptorCount * SE_VK_RENDER_PIPELINE_NUMBER_OF_SETS_IN_POOL;
                    }
                }
            }
            layout->poolCreateInfo =
            {
                .sType          = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                .pNext          = nullptr,
                .flags          = 0,
                .maxSets        = SE_VK_RENDER_PIPELINE_NUMBER_OF_SETS_IN_POOL,
                .poolSizeCount  = (uint32_t)layout->numPoolSizes, // @TODO : safe cast
                .pPoolSizes     = layout->poolSizes,
            };
        }
        se_vk_pipeline_destroy_descriptor_set_layout_create_infos(&layoutCreateInfos);
    }
    //
    // Pipeline layout
    //
    {
        VkDescriptorSetLayout descriptorSetLayoutHandles[SE_VK_RENDER_PIPELINE_MAX_DESCRIPTOR_SETS] = {0};
        for (size_t it = 0; it < pipeline->numDescriptorSetLayouts; it++)
        {
            descriptorSetLayoutHandles[it] = pipeline->descriptorSetLayouts[it].handle;
        }
        VkPipelineLayoutCreateInfo pipelineLayoutInfo =
        {
            .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pNext                  = nullptr,
            .flags                  = 0,
            .setLayoutCount         = (uint32_t)pipeline->numDescriptorSetLayouts, // @TODO : safe cast
            .pSetLayouts            = descriptorSetLayoutHandles,
            .pushConstantRangeCount = 0,
            .pPushConstantRanges    = nullptr,
        };
        se_vk_check(vkCreatePipelineLayout(logicalHandle, &pipelineLayoutInfo, callbacks, &pipeline->layout));
    }
}

void se_vk_pipeline_graphics_construct(SeVkPipeline* pipeline, SeVkGraphicsPipelineInfo* info)
{
    SeVkDevice* device = info->device;
    VkDevice logicalHandle = se_vk_device_get_logical_handle(device);
    SeVkMemoryManager* memoryManager = &device->memoryManager;
    VkAllocationCallbacks* callbacks = se_vk_memory_manager_get_callbacks(memoryManager);
    AllocatorBindings frameAllocator = app_allocators::frame();

    SeVkProgram* vertexProgram = info->vertexProgram.program;
    SeVkProgram* fragmentProgram = info->fragmentProgram.program;
    *pipeline =
    {
        .object                     = { SE_VK_TYPE_GRAPHICS_PIPELINE, g_pipelineIndex++ },
        .device                     = device,
        .bindPoint                  = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .handle                     = VK_NULL_HANDLE,
        .layout                     = VK_NULL_HANDLE,
        .descriptorSetLayouts       = { },
        .numDescriptorSetLayouts    = 0,
        .dependencies               = { .graphics = { vertexProgram, fragmentProgram, info->pass } },
    };

    const SimpleSpirvReflection* vertexReflection = &vertexProgram->reflection;
    const SimpleSpirvReflection* fragmentReflection = &fragmentProgram->reflection;
    const VkBool32 isStencilSupported = se_vk_device_is_stencil_supported(device);
    se_assert(!se_vk_pipeline_has_vertex_input(vertexReflection) && "Vertex shader inputs are not supported");
    se_assert(!vertexReflection->pushConstantType && "Push constants are not supported");
    se_assert(!fragmentReflection->pushConstantType && "Push constants are not supported");
    se_assert((info->subpassIndex < se_vk_render_pass_num_subpasses(info->pass)) && "Incorrect pipeline subpass index");
    se_vk_render_pass_validate_fragment_program_setup(info->pass, fragmentProgram, info->subpassIndex);
    
    const SimpleSpirvReflection* reflections[] = { vertexReflection, fragmentReflection };
    se_vk_pipeline_create_descriptor_sets_and_layout(pipeline, reflections, se_array_size(reflections));
    
    const VkPipelineShaderStageCreateInfo shaderStages[] =
    {
        se_vk_program_get_shader_stage_create_info(device, &info->vertexProgram, frameAllocator),
        se_vk_program_get_shader_stage_create_info(device, &info->fragmentProgram, frameAllocator),
    };
    const VkExtent2D swapChainExtent = se_vk_device_get_swap_chain_extent(device);
    const VkPipelineVertexInputStateCreateInfo vertexInputInfo = se_vk_utils_vertex_input_state_create_info(0, nullptr, 0, nullptr);
    const VkPipelineInputAssemblyStateCreateInfo inputAssembly = se_vk_utils_input_assembly_state_create_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE);
    const SeVkViewportScissor viewportScissor = se_vk_utils_default_viewport_scissor(swapChainExtent.width, swapChainExtent.height);
    const VkPipelineViewportStateCreateInfo viewportState = se_vk_utils_viewport_state_create_info(&viewportScissor.viewport, &viewportScissor.scissor);
    const VkPipelineRasterizationStateCreateInfo rasterizationState = se_vk_utils_rasterization_state_create_info(info->polygonMode, info->cullMode, info->frontFace);
    const VkPipelineMultisampleStateCreateInfo multisampleState = se_vk_utils_multisample_state_create_info(se_vk_utils_pick_sample_count(info->sampling, se_vk_device_get_supported_framebuffer_multisample_types(device)));
    //
    // @NOTE : Engine always uses reverse depth, so depthCompareOp is hardcoded to VK_COMPARE_OP_GREATER.
    //         If you want to change this, then se_vk_perspective_projection_matrix must be reevaluated in
    //         order to remap Z coord to the new range (instead of [1, 0]), as well as render pass depth
    //         clear values.
    //
    const VkPipelineDepthStencilStateCreateInfo depthStencilState
    {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .pNext                  = nullptr,
        .flags                  = 0,
        .depthTestEnable        = info->isDepthTestEnabled,
        .depthWriteEnable       = info->isDepthWriteEnabled,
        .depthCompareOp         = VK_COMPARE_OP_GREATER,
        .depthBoundsTestEnable  = VK_FALSE,
        .stencilTestEnable      = isStencilSupported && info->isStencilTestEnabled,
        .front                  = info->frontStencilOpState,
        .back                   = info->backStencilOpState,
        .minDepthBounds         = 0.0f,
        .maxDepthBounds         = 0.0f,
    };
    VkPipelineColorBlendAttachmentState colorBlendAttachments[32] = { 0 /* COLOR BLENDING IS UNSUPPORTED */ };
    for (size_t it = 0; it < se_array_size(colorBlendAttachments); it++) colorBlendAttachments[it].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    const uint32_t numColorAttachments = se_vk_render_pass_get_num_color_attachments(info->pass, info->subpassIndex);
    const VkPipelineColorBlendStateCreateInfo colorBlending = se_vk_utils_color_blending_create_info(colorBlendAttachments, numColorAttachments);
    const VkPipelineDynamicStateCreateInfo dynamicState = se_vk_utils_dynamic_state_default_create_info();
    VkGraphicsPipelineCreateInfo pipelineCreateInfo
    {
        .sType                  = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext                  = nullptr,
        .flags                  = 0,
        .stageCount             = se_array_size(shaderStages),
        .pStages                = shaderStages,
        .pVertexInputState      = &vertexInputInfo,
        .pInputAssemblyState    = &inputAssembly,
        .pTessellationState     = nullptr,
        .pViewportState         = &viewportState,
        .pRasterizationState    = &rasterizationState,
        .pMultisampleState      = &multisampleState,
        .pDepthStencilState     = &depthStencilState,
        .pColorBlendState       = &colorBlending,
        .pDynamicState          = &dynamicState,
        .layout                 = pipeline->layout,
        .renderPass             = info->pass->handle,
        .subpass                = info->subpassIndex, // @TODO : safe cast
        .basePipelineHandle     = VK_NULL_HANDLE,
        .basePipelineIndex      = -1,
    };
    se_vk_check(vkCreateGraphicsPipelines(logicalHandle, VK_NULL_HANDLE, 1, &pipelineCreateInfo, callbacks, &pipeline->handle));
}

void se_vk_pipeline_compute_construct(SeVkPipeline* pipeline, SeVkComputePipelineInfo* info)
{
    SeVkDevice* device = info->device;
    VkDevice logicalHandle = se_vk_device_get_logical_handle(device);
    SeVkMemoryManager* memoryManager = &device->memoryManager;
    VkAllocationCallbacks* callbacks = se_vk_memory_manager_get_callbacks(memoryManager);
    AllocatorBindings frameAllocator = app_allocators::frame();
    
    SeVkProgram* program = info->program.program;
    *pipeline =
    {
        .object                     = { SE_VK_TYPE_GRAPHICS_PIPELINE, g_pipelineIndex++ },
        .device                     = device,
        .bindPoint                  = VK_PIPELINE_BIND_POINT_COMPUTE,
        .handle                     = VK_NULL_HANDLE,
        .layout                     = VK_NULL_HANDLE,
        .descriptorSetLayouts       = { },
        .numDescriptorSetLayouts    = 0,
        .dependencies               = { .compute = { program } },
    };

    const SimpleSpirvReflection* reflection = &program->reflection;
    se_assert(!reflection->pushConstantType && "Push constants are not supported");
    
    se_vk_pipeline_create_descriptor_sets_and_layout(pipeline, &reflection, 1);
    VkComputePipelineCreateInfo pipelineCreateInfo
    {
        .sType              = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .pNext              = nullptr,
        .flags              = 0,
        .stage              = se_vk_program_get_shader_stage_create_info(device, &info->program, frameAllocator),
        .layout             = pipeline->layout,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex  = -1,
    };
    se_vk_check(vkCreateComputePipelines(logicalHandle, VK_NULL_HANDLE, 1, &pipelineCreateInfo, callbacks, &pipeline->handle));
}

void se_vk_pipeline_destroy(SeVkPipeline* pipeline)
{
    SeVkDevice* device = pipeline->device;
    VkAllocationCallbacks* callbacks = se_vk_memory_manager_get_callbacks(&device->memoryManager);
    VkDevice logicalHandle = se_vk_device_get_logical_handle(device);
    
    for (size_t layoutIt = 0; layoutIt < pipeline->numDescriptorSetLayouts; layoutIt++)
    {
        SeVkDescriptorSetLayout* layout = &pipeline->descriptorSetLayouts[layoutIt];
        vkDestroyDescriptorSetLayout(logicalHandle, layout->handle, callbacks);
    }
    vkDestroyPipeline(logicalHandle, pipeline->handle, callbacks);
    vkDestroyPipelineLayout(logicalHandle, pipeline->layout, callbacks);
}

VkDescriptorPool se_vk_pipeline_create_descriptor_pool(SeVkPipeline* pipeline, size_t set)
{
    VkAllocationCallbacks* callbacks = se_vk_memory_manager_get_callbacks(&pipeline->device->memoryManager);
    VkDevice logicalHandle = se_vk_device_get_logical_handle(pipeline->device);
    VkDescriptorPool pool = VK_NULL_HANDLE;
    se_vk_check(vkCreateDescriptorPool(logicalHandle, &pipeline->descriptorSetLayouts[set].poolCreateInfo, callbacks, &pool));
    return pool;
}

size_t se_vk_pipeline_get_biggest_set_index(const SeVkPipeline* pipeline)
{
    return pipeline->numDescriptorSetLayouts;
}

size_t se_vk_pipeline_get_biggest_binding_index(const SeVkPipeline* pipeline, size_t set)
{
    return pipeline->descriptorSetLayouts[set].numBindings;
}

VkDescriptorSetLayout se_vk_pipeline_get_descriptor_set_layout(SeVkPipeline* pipeline, size_t set)
{
    return pipeline->descriptorSetLayouts[set].handle;
}

SeVkDescriptorSetBindingInfo se_vk_pipeline_get_binding_info(const SeVkPipeline* pipeline, size_t set, size_t binding)
{
    return pipeline->descriptorSetLayouts[set].bindingInfos[binding];
}
