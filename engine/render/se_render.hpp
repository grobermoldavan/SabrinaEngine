#ifndef _SE_RENDER_INTERFACE_HPP_
#define _SE_RENDER_INTERFACE_HPP_

#include "engine/se_common_includes.hpp"
#include "engine/se_math.hpp"
#include "engine/se_data_providers.hpp"
#include "engine/subsystems/se_window.hpp"

constexpr size_t SE_MAX_SPECIALIZATION_CONSTANTS    = 8;
constexpr size_t SE_MAX_BINDINGS                    = 8;
constexpr size_t SE_MAX_PASS_DEPENDENCIES           = 64;
constexpr size_t SE_MAX_PASS_RENDER_TARGETS         = 8;

enum SePassRenderTargetLoadOp
{
    SE_PASS_RENDER_TARGET_LOAD_OP_LOAD,
    SE_PASS_RENDER_TARGET_LOAD_OP_CLEAR,
    SE_PASS_RENDER_TARGET_LOAD_OP_DONT_CARE,
};

enum SeTextureFormat
{
    SE_TEXTURE_FORMAT_DEPTH_STENCIL,
    SE_TEXTURE_FORMAT_R_8,
    SE_TEXTURE_FORMAT_RGBA_8,
    SE_TEXTURE_FORMAT_RGBA_32F,
};

enum SePipelinePolygonMode
{
    SE_PIPELINE_POLYGON_FILL_MODE_FILL,
    SE_PIPELINE_POLYGON_FILL_MODE_LINE,
    SE_PIPELINE_POLYGON_FILL_MODE_POINT,
};

enum SePipelineCullMode
{
    SE_PIPELINE_CULL_MODE_NONE,
    SE_PIPELINE_CULL_MODE_FRONT,
    SE_PIPELINE_CULL_MODE_BACK,
    SE_PIPELINE_CULL_MODE_FRONT_BACK,
};

enum SePipelineFrontFace
{
    SE_PIPELINE_FRONT_FACE_CLOCKWISE,
    SE_PIPELINE_FRONT_FACE_COUNTER_CLOCKWISE,
};

enum SeSamplingType
{
    SE_SAMPLING_1  = 0x00000001,
    SE_SAMPLING_2  = 0x00000002,
    SE_SAMPLING_4  = 0x00000004,
    SE_SAMPLING_8  = 0x00000008,
    SE_SAMPLING_16 = 0x00000010,
    SE_SAMPLING_32 = 0x00000020,
    SE_SAMPLING_64 = 0x00000040,
};

enum SeStencilOp
{
    SE_STENCIL_OP_KEEP,
    SE_STENCIL_OP_ZERO,
    SE_STENCIL_OP_REPLACE,
    SE_STENCIL_OP_INCREMENT_AND_CLAMP,
    SE_STENCIL_OP_DECREMENT_AND_CLAMP,
    SE_STENCIL_OP_INVERT,
    SE_STENCIL_OP_INCREMENT_AND_WRAP,
    SE_STENCIL_OP_DECREMENT_AND_WRAP,
};

enum SeCompareOp
{
    SE_COMPARE_OP_NEVER,
    SE_COMPARE_OP_LESS,
    SE_COMPARE_OP_EQUAL,
    SE_COMPARE_OP_LESS_OR_EQUAL,
    SE_COMPARE_OP_GREATER,
    SE_COMPARE_OP_NOT_EQUAL,
    SE_COMPARE_OP_GREATER_OR_EQUAL,
    SE_COMPARE_OP_ALWAYS,
};

enum SeSamplerFilter
{
    SE_SAMPLER_FILTER_NEAREST,
    SE_SAMPLER_FILTER_LINEAR,
};

enum SeSamplerAddressMode
{
    SE_SAMPLER_ADDRESS_MODE_REPEAT,
    SE_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
    SE_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    SE_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
};

enum SeSamplerMipmapMode
{
    SE_SAMPLER_MIPMAP_MODE_NEAREST,
    SE_SAMPLER_MIPMAP_MODE_LINEAR,
};

using SeRenderRef = uint64_t;

constexpr SeRenderRef NULL_RENDER_REF = 0;

#define se_pass_dependency(id) (1ull << id)
using SePassDependencies = uint64_t;

struct SePassRenderTarget
{
    SeRenderRef texture;
    SePassRenderTargetLoadOp loadOp;
};

struct SeBeginPassInfo
{
    SePassDependencies  dependencies;
    SeRenderRef         pipeline;
    SePassRenderTarget  renderTargets[SE_MAX_PASS_RENDER_TARGETS];
    size_t              numRenderTargets;
    SePassRenderTarget  depthStencilTarget;
    bool                hasDepthStencil;
};

struct SeProgramInfo
{
    DataProvider data;
};

struct SeRenderProgramComputeWorkGroupSize
{
    uint32_t x;
    uint32_t y;
    uint32_t z;
};

struct SeTextureInfo
{
    size_t          width;
    size_t          height;
    SeTextureFormat format;
    DataProvider    data;
};

struct SeSamplerInfo
{
    SeSamplerFilter         magFilter;
    SeSamplerFilter         minFilter;
    SeSamplerAddressMode    addressModeU;
    SeSamplerAddressMode    addressModeV;
    SeSamplerAddressMode    addressModeW;
    SeSamplerMipmapMode     mipmapMode;
    float                   mipLodBias;
    float                   minLod;
    float                   maxLod;
    bool                    anisotropyEnable;
    float                   maxAnisotropy;
    bool                    compareEnabled;
    SeCompareOp             compareOp;
};

struct SeStencilOpState
{
    bool        isEnabled;
    uint32_t    compareMask;
    uint32_t    writeMask;
    uint32_t    reference;
    SeStencilOp failOp;
    SeStencilOp passOp;
    SeStencilOp depthFailOp;
    SeCompareOp compareOp;
};

struct SeDepthState
{
    bool isTestEnabled;
    bool isWriteEnabled;
};

struct SeSpecializationConstant
{
    uint32_t constantId;
    union
    {
        int32_t     asInt;
        uint32_t    asUint;
        float       asFloat;
        bool        asBool;
    };
};

struct SeProgramWithConstants
{
    SeRenderRef                 program;
    SeSpecializationConstant    specializationConstants[SE_MAX_SPECIALIZATION_CONSTANTS];
    size_t                      numSpecializationConstants;
};

struct SeGraphicsPipelineInfo
{
    SeProgramWithConstants  vertexProgram;
    SeProgramWithConstants  fragmentProgram;
    SeStencilOpState        frontStencilOpState;
    SeStencilOpState        backStencilOpState;
    SeDepthState            depthState;
    SePipelinePolygonMode   polygonMode;
    SePipelineCullMode      cullMode;
    SePipelineFrontFace     frontFace;
    SeSamplingType          samplingType;
};

struct SeComputePipelineInfo
{
    SeProgramWithConstants program;
};

struct SeMemoryBufferInfo
{
    DataProvider data;
};

struct SeBinding
{
    uint32_t    binding;
    SeRenderRef object;
    SeRenderRef sampler;
};

struct SeCommandBindInfo
{
    uint32_t    set;
    SeBinding   bindings[SE_MAX_BINDINGS];
};

struct SeCommandDrawInfo
{
    uint32_t    numVertices;
    uint32_t    numInstances;
};

struct SeCommandDispatchInfo
{
    uint32_t    groupCountX;
    uint32_t    groupCountY;
    uint32_t    groupCountZ;
};

struct SeComputeWorkgroupSize
{
    uint32_t x;
    uint32_t y;
    uint32_t z;
};

struct SeTextureSize
{
    size_t x;
    size_t y;
    size_t z;
};

namespace render
{
    bool                    begin_frame                 ();
    void                    end_frame                   ();
    SePassDependencies      begin_pass                  (const SeBeginPassInfo& info);
    void                    end_pass                    ();
    SeRenderRef             program                     (const SeProgramInfo& info);
    SeRenderRef             texture                     (const SeTextureInfo& info);
    SeRenderRef             swap_chain_texture          ();
    SeTextureSize           texture_size                (SeRenderRef texture);
    SeRenderRef             graphics_pipeline           (const SeGraphicsPipelineInfo& info);
    SeRenderRef             compute_pipeline            (const SeComputePipelineInfo& info);
    SeRenderRef             memory_buffer               (const SeMemoryBufferInfo& info);
    SeRenderRef             sampler                     (const SeSamplerInfo& info);
    void                    bind                        (const SeCommandBindInfo& info);
    void                    draw                        (const SeCommandDrawInfo& info);
    void                    dispatch                    (const SeCommandDispatchInfo& info);
    SeFloat4x4              perspective                 (float fovDeg, float aspect, float nearPlane, float farPlane);
    SeFloat4x4              orthographic                (float left, float right, float bottom, float top, float nearPlane, float farPlane);
    SeComputeWorkgroupSize  workgroup_size              (SeRenderRef program);
    SePassDependencies      dependencies_for_texture    (SeRenderRef texture);

    namespace engine
    {
        void init();
        void terminate();
    }
};

using ColorPacked = uint32_t;
using ColorUnpacked = SeFloat4;

namespace col
{
    inline constexpr ColorPacked pack(const ColorUnpacked& color)
    {
        const uint32_t r = (uint32_t)(se_clamp(color.r, 0.0f, 1.0f) * 255.0f);
        const uint32_t g = (uint32_t)(se_clamp(color.g, 0.0f, 1.0f) * 255.0f);
        const uint32_t b = (uint32_t)(se_clamp(color.b, 0.0f, 1.0f) * 255.0f);
        const uint32_t a = (uint32_t)(se_clamp(color.a, 0.0f, 1.0f) * 255.0f);
        return r | (g << 8) | (b << 16) | (a << 24);
    }

    inline constexpr ColorUnpacked unpack(ColorPacked color)
    {
        return
        {
            ((float)(color       & 0xFF)) / 255.0f,
            ((float)(color >> 8  & 0xFF)) / 255.0f,
            ((float)(color >> 16 & 0xFF)) / 255.0f,
            ((float)(color >> 24 & 0xFF)) / 255.0f,
        };
    }
}

#endif
