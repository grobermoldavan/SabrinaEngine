#ifndef _SE_VULKAN_RENDER_SUBSYSTEM_RENDER_PROGRAM_H_
#define _SE_VULKAN_RENDER_SUBSYSTEM_RENDER_PROGRAM_H_

#include "se_vulkan_render_subsystem_base.hpp"

struct SeVkProgramInfo
{
    SeVkDevice*  device;
    DataProvider data;
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

VkPipelineShaderStageCreateInfo se_vk_program_get_shader_stage_create_info(SeVkDevice* device, SeVkProgramWithConstants* pipelineProgram, const AllocatorBindings& allocator);

template<>
void se_vk_destroy<SeVkProgram>(SeVkProgram* res)
{
    se_vk_program_destroy(res);
}

namespace hash_value
{
    namespace builder
    {
        template<>
        void absorb<SeVkProgramInfo>(HashValueBuilder& builder, const SeVkProgramInfo& value)
        {
            hash_value::builder::absorb(builder, value.data);
        }

        template<>
        void absorb<SeVkProgram>(HashValueBuilder& builder, const SeVkProgram& value)
        {
            hash_value::builder::absorb_raw(builder, { (void*)&value.object, sizeof(value.object) });
        }

        template<>
        void absorb<SeVkProgramWithConstants>(HashValueBuilder& builder, const SeVkProgramWithConstants& value)
        {
            hash_value::builder::absorb(builder, *value.program);
            for (size_t it = 0; it < value.numSpecializationConstants; it++)
                hash_value::builder::absorb(builder, value.constants[it]);
        }
    }

    template<>
    HashValue generate<SeVkProgramInfo>(const SeVkProgramInfo& value)
    {
        return hash_value::generate(value.data);
    }

    template<>
    HashValue generate<SeVkProgram>(const SeVkProgram& value)
    {
        return hash_value::generate_raw({ (void*)&value.object, sizeof(value.object) });
    }

    template<>
    HashValue generate<SeVkProgramWithConstants>(const SeVkProgramWithConstants& value)
    {
        HashValueBuilder builder = hash_value::builder::begin();
        hash_value::builder::absorb(builder, value);
        return hash_value::builder::end(builder);
    }
}

#endif
