#ifndef _SE_RENDER_ABSTRACTION_INTERFACE_H_
#define _SE_RENDER_ABSTRACTION_INTERFACE_H_

#include <inttypes.h>
#include <stdbool.h>

#define SE_MAX_SPECIALIZATION_CONSTANTS 8

typedef enum SePassRenderTargetLoadOp
{
    SE_PASS_RENDER_TARGET_LOAD_OP_LOAD,
    SE_PASS_RENDER_TARGET_LOAD_OP_CLEAR,
    SE_PASS_RENDER_TARGET_LOAD_OP_DONT_CARE,
} SePassRenderTargetLoadOp;

typedef enum SeTextureFormat
{
    SE_TEXTURE_FORMAT_DEPTH_STENCIL,
    SE_TEXTURE_FORMAT_RGBA_8,
    SE_TEXTURE_FORMAT_RGBA_32F,
} SeTextureFormat;

typedef enum SePipelinePolygonMode
{
    SE_PIPELINE_POLYGON_FILL_MODE_FILL,
    SE_PIPELINE_POLYGON_FILL_MODE_LINE,
    SE_PIPELINE_POLYGON_FILL_MODE_POINT,
} SePipelinePolygonMode;

typedef enum SePipelineCullMode
{
    SE_PIPELINE_CULL_MODE_NONE,
    SE_PIPELINE_CULL_MODE_FRONT,
    SE_PIPELINE_CULL_MODE_BACK,
    SE_PIPELINE_CULL_MODE_FRONT_BACK,
} SePipelineCullMode;

typedef enum SePipelineFrontFace
{
    SE_PIPELINE_FRONT_FACE_CLOCKWISE,
    SE_PIPELINE_FRONT_FACE_COUNTER_CLOCKWISE,
} SePipelineFrontFace;

typedef enum SeSamplingType
{
    SE_SAMPLING_1  = 0x00000001,
    SE_SAMPLING_2  = 0x00000002,
    SE_SAMPLING_4  = 0x00000004,
    SE_SAMPLING_8  = 0x00000008,
    SE_SAMPLING_16 = 0x00000010,
    SE_SAMPLING_32 = 0x00000020,
    SE_SAMPLING_64 = 0x00000040,
} SeSamplingType;

typedef enum SeStencilOp
{
    SE_STENCIL_OP_KEEP,
    SE_STENCIL_OP_ZERO,
    SE_STENCIL_OP_REPLACE,
    SE_STENCIL_OP_INCREMENT_AND_CLAMP,
    SE_STENCIL_OP_DECREMENT_AND_CLAMP,
    SE_STENCIL_OP_INVERT,
    SE_STENCIL_OP_INCREMENT_AND_WRAP,
    SE_STENCIL_OP_DECREMENT_AND_WRAP,
} SeStencilOp;

typedef enum SeCompareOp
{
    SE_COMPARE_OP_NEVER,
    SE_COMPARE_OP_LESS,
    SE_COMPARE_OP_EQUAL,
    SE_COMPARE_OP_LESS_OR_EQUAL,
    SE_COMPARE_OP_GREATER,
    SE_COMPARE_OP_NOT_EQUAL,
    SE_COMPARE_OP_GREATER_OR_EQUAL,
    SE_COMPARE_OP_ALWAYS,
} SeCompareOp;

typedef enum SeSamplerFilter
{
    SE_SAMPLER_FILTER_NEAREST,
    SE_SAMPLER_FILTER_LINEAR,
} SeSamplerFilter;

typedef enum SeSamplerAddressMode
{
    SE_SAMPLER_ADDRESS_MODE_REPEAT,
    SE_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
    SE_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    SE_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
} SeSamplerAddressMode;

typedef enum SeSamplerMipmapMode
{
    SE_SAMPLER_MIPMAP_MODE_NEAREST,
    SE_SAMPLER_MIPMAP_MODE_LINEAR,
} SeSamplerMipmapMode;

typedef void* SeDeviceHandle;

typedef uint64_t SeRenderRef;

typedef struct SeRenderDeviceCreateInfo
{
    struct SeWindowHandle* window;
} SeRenderDeviceCreateInfo;

#define SE_MAX_PASS_DEPENDENCIES 64
#define se_pass_dependency(id) (1ull << id)
typedef uint64_t SePassDependencies;

typedef struct SePassRenderTarget
{
    SeRenderRef texture;
    SePassRenderTargetLoadOp loadOp;
} SePassRenderTarget;

typedef struct SeBeginPassInfo
{
    uint64_t            id;
    SePassDependencies  dependencies;
    SeRenderRef         pipeline;
    SePassRenderTarget  renderTargets[8];
    size_t              numRenderTargets;
    SePassRenderTarget  depthStencilTarget;
    bool                hasDepthStencil;
} SeBeginPassInfo;

typedef struct SeProgramInfo
{
    const uint32_t* bytecode;
    size_t          bytecodeSize;
} SeProgramInfo;

typedef struct SeRenderProgramComputeWorkGroupSize
{
    uint32_t x;
    uint32_t y;
    uint32_t z;
} SeRenderProgramComputeWorkGroupSize;

typedef struct SeTextureInfo
{
    SeDeviceHandle  device;
    size_t          width;
    size_t          height;
    SeTextureFormat format;
} SeTextureInfo;

typedef struct SeSamplerCreateInfo
{
    SeDeviceHandle          device;
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
} SeSamplerInfo;

typedef struct SeStencilOpState
{
    bool        isEnabled;
    uint32_t    compareMask;
    uint32_t    writeMask;
    uint32_t    reference;
    SeStencilOp failOp;
    SeStencilOp passOp;
    SeStencilOp depthFailOp;
    SeCompareOp compareOp;
} SeStencilOpState;

typedef struct SeDepthState
{
    bool isTestEnabled;
    bool isWriteEnabled;
} SeDepthState;

typedef struct SeSpecializationConstant
{
    uint32_t constantId;
    union
    {
        int32_t     asInt;
        uint32_t    asUint;
        float       asFloat;
        bool        asBool;
    };
} SeSpecializationConstant;

typedef struct SeProgramWithConstants
{
    SeRenderRef                 program;
    SeSpecializationConstant    specializationConstants[SE_MAX_SPECIALIZATION_CONSTANTS];
    size_t                      numSpecializationConstants;
} SeProgramWithConstants;

typedef struct SeGraphicsPipelineInfo
{
    SeDeviceHandle          device;
    SeProgramWithConstants  vertexProgram;
    SeProgramWithConstants  fragmentProgram;
    SeStencilOpState        frontStencilOpState;
    SeStencilOpState        backStencilOpState;
    SeDepthState            depthState;
    SePipelinePolygonMode   polygonMode;
    SePipelineCullMode      cullMode;
    SePipelineFrontFace     frontFace;
    SeSamplingType          samplingType;
} SeGraphicsPipelineInfo;

typedef struct SeComputePipelineInfo
{
    SeDeviceHandle          device;
    SeProgramWithConstants  program;
} SeComputePipelineInfo;

typedef struct SeMemoryBufferInfo
{
    SeDeviceHandle  device;
    size_t          size;
    const void*     data;
} SeMemoryBufferInfo;

typedef struct SeBinding
{
    uint32_t binding;
    SeRenderRef object;
    SeRenderRef sampler;
} SeBinding;

typedef struct SeCommandBindInfo
{
    SeDeviceHandle  device;
    uint32_t        set;
    SeBinding       bindings[8];
    uint32_t        numBindings;
} SeCommandBindInfo;

typedef struct SeCommandDrawInfo
{
    SeDeviceHandle  device;
    uint32_t        numVertices;
    uint32_t        numInstances;
} SeCommandDrawInfo;

typedef struct SeCommandDispatchInfo
{
    SeDeviceHandle  device;
    uint32_t        groupCountX;
    uint32_t        groupCountY;
    uint32_t        groupCountZ;
} SeCommandDispatchInfo;

typedef struct SeRenderAbstractionSubsystemInterface
{
    SeDeviceHandle                      (*device_create)                        (SeRenderDeviceCreateInfo* createInfo);
    void                                (*device_destroy)                       (SeDeviceHandle device);
    void                                (*begin_frame)                          (SeDeviceHandle device);
    void                                (*end_frame)                            (SeDeviceHandle device);
    void                                (*begin_pass)                           (SeDeviceHandle device, SeBeginPassInfo* info);
    void                                (*end_pass)                             (SeDeviceHandle device);

    SeRenderRef                         (*program)                              (SeDeviceHandle device, SeProgramInfo* info);
    SeRenderRef                         (*texture)                              (SeDeviceHandle device, SeTextureInfo* info);
    SeRenderRef                         (*swap_chain_texture)                   (SeDeviceHandle device);
    SeRenderRef                         (*graphics_pipeline)                    (SeDeviceHandle device, SeGraphicsPipelineInfo* info);
    SeRenderRef                         (*compute_pipeline)                     (SeDeviceHandle device, SeComputePipelineInfo* info);
    SeRenderRef                         (*memory_buffer)                        (SeDeviceHandle device, SeMemoryBufferInfo* info);
    SeRenderRef                         (*sampler)                              (SeDeviceHandle device, SeSamplerInfo* info);
    void                                (*bind)                                 (SeDeviceHandle device, SeCommandBindInfo* info);
    void                                (*draw)                                 (SeDeviceHandle device, SeCommandDrawInfo* info);
    void                                (*dispatch)                             (SeDeviceHandle device, SeCommandDispatchInfo* info);
    void                                (*perspective_projection_matrix)        (struct SeFloat4x4* result, float fovDeg, float aspect, float nearPlane, float farPlane);
} SeRenderAbstractionSubsystemInterface;

#endif
