#ifndef _SE_VULKAN_RENDER_PROGRAM_H_
#define _SE_VULKAN_RENDER_PROGRAM_H_

#include "se_vulkan_base.hpp"

struct SeVkProgramInfo
{
    SeVkDevice*  device;
    SeDataProvider data;
};

struct SeVkProgram
{
    SeVkObject              object;
    SeVkDevice*             device;
    VkShaderModule          handle;
    SimpleSpirvReflection   reflection;
};

struct SeVkProgramWithConstants
{
    SeVkProgram*                program;
    SeSpecializationConstant    constants[SE_MAX_SPECIALIZATION_CONSTANTS];
    size_t                      numSpecializationConstants;
};

void se_vk_program_construct(SeVkProgram* program, SeVkProgramInfo* info);
void se_vk_program_destroy(SeVkProgram* program);

VkPipelineShaderStageCreateInfo se_vk_program_get_shader_stage_create_info(SeVkDevice* device, SeVkProgramWithConstants* pipelineProgram, const SeAllocatorBindings& allocator);

template<>
void se_vk_destroy<SeVkProgram>(SeVkProgram* res)
{
    se_vk_program_destroy(res);
}

template<>
void se_hash_value_builder_absorb<SeVkProgramInfo>(SeHashValueBuilder& builder, const SeVkProgramInfo& value)
{
    se_hash_value_builder_absorb(builder, value.data);
}

template<>
void se_hash_value_builder_absorb<SeVkProgram>(SeHashValueBuilder& builder, const SeVkProgram& value)
{
    se_hash_value_builder_absorb_raw(builder, { (void*)&value.object, sizeof(value.object) });
}

template<>
void se_hash_value_builder_absorb<SeVkProgramWithConstants>(SeHashValueBuilder& builder, const SeVkProgramWithConstants& value)
{
    se_hash_value_builder_absorb(builder, *value.program);
    for (size_t it = 0; it < value.numSpecializationConstants; it++)
        se_hash_value_builder_absorb(builder, value.constants[it]);
}

template<>
SeHashValue se_hash_value_generate<SeVkProgramInfo>(const SeVkProgramInfo& value)
{
    return se_hash_value_generate(value.data);
}

template<>
SeHashValue se_hash_value_generate<SeVkProgram>(const SeVkProgram& value)
{
    return se_hash_value_generate_raw({ (void*)&value.object, sizeof(value.object) });
}

template<>
SeHashValue se_hash_value_generate<SeVkProgramWithConstants>(const SeVkProgramWithConstants& value)
{
    SeHashValueBuilder builder = se_hash_value_builder_begin();
    se_hash_value_builder_absorb(builder, value);
    return se_hash_value_builder_end(builder);
}

#endif
