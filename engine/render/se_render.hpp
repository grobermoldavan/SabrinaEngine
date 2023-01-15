#ifndef _SE_RENDER_INTERFACE_HPP_
#define _SE_RENDER_INTERFACE_HPP_

#include "engine/se_common_includes.hpp"
#include "engine/se_math.hpp"
#include "engine/se_data_providers.hpp"
#include "engine/subsystems/se_window.hpp"

#include "se_color.hpp"

constexpr size_t SE_MAX_SPECIALIZATION_CONSTANTS    = 8;
constexpr size_t SE_MAX_BINDINGS                    = 8;
constexpr size_t SE_MAX_PASS_DEPENDENCIES           = 64;
constexpr size_t SE_MAX_PASS_RENDER_TARGETS         = 8;

enum struct SeRenderTargetLoadOp : uint32_t
{
    UNDEFINED,
    LOAD,
    CLEAR,
    DONT_CARE,
};

enum struct SeTextureFormat : uint32_t
{
    DEPTH_STENCIL,
    R_8_UNORM,
    R_8_SRGB,
    RGBA_8_UNORM,
    RGBA_8_SRGB,
};

enum struct SePipelinePolygonMode : uint32_t
{
    FILL,
    LINE,
    POINT,
};

enum struct SePipelineCullMode : uint32_t
{
    NONE,
    FRONT,
    BACK,
    FRONT_BACK,
};

enum struct SePipelineFrontFace : uint32_t
{
    CLOCKWISE,
    COUNTER_CLOCKWISE,
};

enum struct SeSamplingType : uint32_t
{
    _1  = 0x00000001,
    _2  = 0x00000002,
    _4  = 0x00000004,
    _8  = 0x00000008,
    _16 = 0x00000010,
    _32 = 0x00000020,
    _64 = 0x00000040,
};

enum struct SeStencilOp : uint32_t
{
    KEEP,
    ZERO,
    REPLACE,
    INCREMENT_AND_CLAMP,
    DECREMENT_AND_CLAMP,
    INVERT,
    INCREMENT_AND_WRAP,
    DECREMENT_AND_WRAP,
};

enum struct SeCompareOp : uint32_t
{
    NEVER,
    LESS,
    EQUAL,
    LESS_OR_EQUAL,
    GREATER,
    NOT_EQUAL,
    GREATER_OR_EQUAL,
    ALWAYS,
};

enum struct SeSamplerFilter : uint32_t
{
    NEAREST,
    LINEAR,
};

enum struct SeSamplerAddressMode : uint32_t
{
    REPEAT,
    MIRRORED_REPEAT,
    CLAMP_TO_EDGE,
    CLAMP_TO_BORDER,
};

enum struct SeSamplerMipmapMode : uint32_t
{
    NEAREST,
    LINEAR,
};

enum struct SeRenderRefType : uint32_t
{
    PROGRAM,
    BUFFER,
    SAMPLER,
};

template<SeRenderRefType type> 
struct SeRenderRef
{
    uint32_t index;
    uint32_t generation;
    operator bool () const { return generation != 0; }
};

using SeProgramRef = SeRenderRef<SeRenderRefType::PROGRAM>;
using SeSamplerRef = SeRenderRef<SeRenderRefType::SAMPLER>;

struct SeBufferRef
{
    uint32_t index;
    uint32_t generation : 31;
    uint32_t isScratch : 1;
    operator bool () const { return (index != 0) | (generation != 0) | (isScratch != 0); }
};

struct SeTextureRef
{
    uint32_t index;
    uint32_t generation : 31;
    uint32_t isSwapChain : 1;
    operator bool () const { return (index != 0) | (generation != 0) | (isSwapChain != 0); }
};

using SePassDependencies = uint64_t;

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

struct SeProgramInfo
{
    DataProvider data;
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
    SeProgramRef                program;
    SeSpecializationConstant    specializationConstants[SE_MAX_SPECIALIZATION_CONSTANTS];
    size_t                      numSpecializationConstants;
};

struct SePassRenderTarget
{
    SeTextureRef texture;
    SeRenderTargetLoadOp loadOp;
    SeColorUnpacked clearColor;
    operator bool () const { return loadOp != SeRenderTargetLoadOp::UNDEFINED; }
};

struct SeGraphicsPassInfo
{
    SePassDependencies      dependencies;
    SeProgramWithConstants  vertexProgram;
    SeProgramWithConstants  fragmentProgram;
    SeStencilOpState        frontStencilOpState;
    SeStencilOpState        backStencilOpState;
    SeDepthState            depthState;
    SePipelinePolygonMode   polygonMode;
    SePipelineCullMode      cullMode;
    SePipelineFrontFace     frontFace;
    SeSamplingType          samplingType;
    SePassRenderTarget      renderTargets[SE_MAX_PASS_RENDER_TARGETS];
    SePassRenderTarget      depthStencilTarget;
};

struct SeComputePassInfo
{
    SePassDependencies      dependencies;
    SeProgramWithConstants  program;
};

struct SeRenderProgramComputeWorkGroupSize
{
    uint32_t x;
    uint32_t y;
    uint32_t z;
};

struct SeTextureInfo
{
    SeTextureFormat format;
    uint32_t        width;
    uint32_t        height;
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

struct SeMemoryBufferInfo
{
    DataProvider data;
};

struct SeMemoryBufferWriteInfo
{
    SeBufferRef buffer;
    DataProvider data;
    size_t offset;
};

struct SeBinding
{
    enum Type : uint32_t
    {
        __UNDEFINED,
        TEXTURE,
        BUFFER,
    };
    uint32_t binding;
    Type type;
    union
    {
        struct
        {
            SeTextureRef texture;
            SeSamplerRef sampler;
        } texture;
        struct
        {
            SeBufferRef buffer;
            size_t offset;
            size_t size;
        } buffer;
    };
    operator bool () const { return type != __UNDEFINED; }
};

struct SeCommandBindInfo
{
    uint32_t    set;
    SeBinding   bindings[SE_MAX_BINDINGS];
};

struct SeCommandDrawInfo
{
    uint32_t numVertices;
    uint32_t numInstances;
};

struct SeCommandDispatchInfo
{
    uint32_t groupCountX;
    uint32_t groupCountY;
    uint32_t groupCountZ;
};

struct SeComputeWorkgroupSize
{
    uint32_t x;
    uint32_t y;
    uint32_t z;
};

struct SeTextureSize
{
    size_t width;
    size_t height;
    size_t depth;
};

namespace render
{
    bool                    begin_frame                 ();
    void                    end_frame                   ();

    SePassDependencies      begin_graphics_pass         (const SeGraphicsPassInfo& info);
    SePassDependencies      begin_compute_pass          (const SeComputePassInfo& info);
    void                    end_pass                    ();

    SeProgramRef            program                     (const SeProgramInfo& info);
    SeTextureRef            texture                     (const SeTextureInfo& info);
    SeTextureRef            swap_chain_texture          ();
    SeBufferRef             memory_buffer               (const SeMemoryBufferInfo& info);
    SeBufferRef             scratch_memory_buffer       (const SeMemoryBufferInfo& info);
    SeSamplerRef            sampler                     (const SeSamplerInfo& info);

    void                    bind                        (const SeCommandBindInfo& info);
    void                    draw                        (const SeCommandDrawInfo& info);
    void                    dispatch                    (const SeCommandDispatchInfo& info);
    void                    write                       (const SeMemoryBufferWriteInfo& info);

    SeFloat4x4              perspective                 (float fovDeg, float aspect, float nearPlane, float farPlane);
    SeFloat4x4              orthographic                (float left, float right, float bottom, float top, float nearPlane, float farPlane);
    SeTextureSize           texture_size                (SeTextureRef texture);
    SeComputeWorkgroupSize  workgroup_size              (SeProgramRef program);

    void                    destroy                     (SeProgramRef ref);
    void                    destroy                     (SeSamplerRef ref);
    void                    destroy                     (SeBufferRef ref);
    void                    destroy                     (SeTextureRef ref);

    namespace engine
    {
        void init(const SeSettings& settings);
        void terminate();
        void update();
    }
};

#endif
