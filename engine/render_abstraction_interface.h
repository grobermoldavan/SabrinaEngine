#ifndef _SE_RENDER_ABSTRACTION_INTERFACE_H_
#define _SE_RENDER_ABSTRACTION_INTERFACE_H_

#include <inttypes.h>
#include <stdbool.h>

#define SE_RENDER_PASS_UNUSED_ATTACHMENT ~(0ul)
#define SE_RENDER_PASS_MAX_ATTACHMENTS 32

typedef enum SeRenderHandleType
{
    SE_RENDER_HANDLE_TYPE_UNITIALIZED,
    SE_RENDER_HANDLE_TYPE_DEVICE,
    SE_RENDER_HANDLE_TYPE_PROGRAM,
    SE_RENDER_HANDLE_TYPE_TEXTURE,
    SE_RENDER_HANDLE_TYPE_SAMPLER,
    SE_RENDER_HANDLE_TYPE_PASS,
    SE_RENDER_HANDLE_TYPE_PIPELINE,
    SE_RENDER_HANDLE_TYPE_FRAMEBUFFER,
    SE_RENDER_HANDLE_TYPE_RESOURCE_SET,
    SE_RENDER_HANDLE_TYPE_MEMORY_BUFFER,
    SE_RENDER_HANDLE_TYPE_COMMAND_BUFFER,
} SeRenderHandleType;

typedef enum SeTextureFormat
{
    SE_TEXTURE_FORMAT_DEPTH_STENCIL,
    SE_TEXTURE_FORMAT_SWAP_CHAIN,
    SE_TEXTURE_FORMAT_RGBA_8,
    SE_TEXTURE_FORMAT_RGBA_32F,
} SeTextureFormat;

typedef enum SeTextureUsageFlags
{
    SE_TEXTURE_USAGE_RENDER_PASS_ATTACHMENT = 0x00000001,
    SE_TEXTURE_USAGE_TEXTURE                = 0x00000002,
} SeTextureUsageFlags;
typedef uint32_t SeTextureUsage;

typedef enum SeAttachmentLoadOp
{
    SE_ATTACHMENT_LOAD_OP_NOTHING,
    SE_ATTACHMENT_LOAD_OP_CLEAR,
    SE_ATTACHMENT_LOAD_OP_LOAD,
} SeAttachmentLoadOp;

typedef enum SeAttachmentStoreOp
{
    SE_ATTACHMENT_STORE_OP_NOTHING,
    SE_ATTACHMENT_STORE_OP_STORE,
} SeAttachmentStoreOp;

typedef enum SePipelinePoligonMode
{
    SE_PIPELINE_POLIGON_FILL_MODE_FILL,
    SE_PIPELINE_POLIGON_FILL_MODE_LINE,
    SE_PIPELINE_POLIGON_FILL_MODE_POINT,
} SePipelinePoligonMode;

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
typedef uint32_t SeSamplingFlags;

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

typedef enum SeDepthOp
{
    SE_DEPTH_OP_NOTHING,
    SE_DEPTH_OP_READ_WRITE,
    SE_DEPTH_OP_READ,
} SeDepthOp;

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

typedef enum SeCommandBufferUsage
{
    SE_COMMAND_BUFFER_USAGE_GRAPHICS,
    SE_COMMAND_BUFFER_USAGE_COMPUTE,
    SE_COMMAND_BUFFER_USAGE_TRANSFER,
} SeCommandBufferUsage;

typedef enum SeMemoryBufferUsageFlags
{
    SE_MEMORY_BUFFER_USAGE_TRANSFER_SRC     = 0x00000001,
    SE_MEMORY_BUFFER_USAGE_TRANSFER_DST     = 0x00000002,
    SE_MEMORY_BUFFER_USAGE_UNIFORM_BUFFER   = 0x00000004,
    SE_MEMORY_BUFFER_USAGE_STORAGE_BUFFER   = 0x00000008,
} SeMemoryBufferUsageFlags;
typedef uint32_t SeMemoryBufferUsage;

typedef enum SeMemoryBufferVisibility
{
    SE_MEMORY_BUFFER_VISIBILITY_GPU,
    SE_MEMORY_BUFFER_VISIBILITY_GPU_AND_CPU,
} SeMemoryBufferVisibility;

typedef enum SeSizeParameterType
{
    SE_SIZE_PARAMETER_CONSTANT,
    SE_SIZE_PARAMETER_DYNAMIC,
} SeSizeParameterType;

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

typedef struct SeSizeParamater
{
    SeSizeParameterType type;
    union
    {
        size_t size;
        size_t (*dynamicSize)(size_t width, size_t height);
    };
} SeSizeParamater;

typedef void (*SeDestroyHandleFunc)(struct SeRenderObject* handle);

typedef struct SeRenderObject
{
    SeRenderHandleType handleType;
    SeDestroyHandleFunc destroy;
} SeRenderObject;

typedef struct SeRenderDeviceCreateInfo
{
    struct SeWindowHandle* window;
    struct SeAllocatorBindings* persistentAllocator;
    struct SeAllocatorBindings* frameAllocator;
    struct SePlatformInterface* platform;
} SeRenderDeviceCreateInfo;

typedef struct SeRenderProgramCreateInfo
{
    SeRenderObject* device;
    uint32_t* bytecode;
    size_t codeSizeBytes;
} SeRenderProgramCreateInfo;

typedef struct SeTextureCreateInfo
{
    SeRenderObject* device;
    SeSizeParamater width;
    SeSizeParamater height;
    SeTextureUsage usage;
    SeTextureFormat format;
} SeTextureCreateInfo;

typedef struct SeSamplerCreateInfo
{
    SeRenderObject* device;
    SeSamplerFilter magFilter;
    SeSamplerFilter minFilter;
    SeSamplerAddressMode addressModeU;
    SeSamplerAddressMode addressModeV;
    SeSamplerAddressMode addressModeW;
    SeSamplerMipmapMode mipmapMode;
    float mipLodBias;
    float minLod;
    float maxLod;
    bool anisotropyEnable;
    float maxAnisotropy;
    bool compareEnable;
    SeCompareOp compareOp;
} SeSamplerCreateInfo;

typedef struct SeRenderPassAttachment
{
    SeTextureFormat format;
    SeAttachmentLoadOp loadOp;
    SeAttachmentStoreOp storeOp;
    SeSamplingType sampling;
} SeRenderPassAttachment;

typedef struct SeRenderPassSubpass
{
    uint32_t* colorRefs;        // each value references colorAttachments array
    size_t numColorRefs;
    uint32_t* inputRefs;        // each value references colorAttachments array
    size_t numInputRefs;
    uint32_t* resolveRefs;      // each value references colorAttachments array
    size_t numResolveRefs;
    SeDepthOp depthOp;          // SE_DEPTH_OP_NOTHING means depth attachment is not used in subpass
} SeRenderPassSubpass;

typedef struct SeRenderPassCreateInfo
{
    SeRenderPassSubpass* subpasses;
    size_t numSubpasses;
    SeRenderPassAttachment* colorAttachments;
    size_t numColorAttachments;
    SeRenderPassAttachment* depthStencilAttachment;
    SeRenderObject* device;
} SeRenderPassCreateInfo;

typedef struct SeStencilOpState
{
    uint32_t compareMask;
    uint32_t writeMask;
    uint32_t reference;
    SeStencilOp failOp;
    SeStencilOp passOp;
    SeStencilOp depthFailOp;
    SeCompareOp compareOp;
} SeStencilOpState;

typedef struct SeDepthTestState
{
    bool isTestEnabled;
    bool isWriteEnabled;
} SeDepthTestState;

typedef struct SeSpecializationConstant
{
    uint32_t constantId;
    union
    {
        int32_t asInt;
        uint32_t asUint;
        float asFloat;
        bool asBool;
    };
} SeSpecializationConstant;

typedef struct SePipelineProgram
{
    SeRenderObject* program;
    SeSpecializationConstant* specializationConstants;
    size_t numSpecializationConstants;
} SePipelineProgram;

typedef struct SeGraphicsRenderPipelineCreateInfo
{
    SeRenderObject* device;
    SeRenderObject* renderPass;
    SePipelineProgram vertexProgram;
    SePipelineProgram fragmentProgram;
    SeStencilOpState* frontStencilOpState;
    SeStencilOpState* backStencilOpState;
    SeDepthTestState* depthTestState;
    size_t subpassIndex;
    SePipelinePoligonMode poligonMode;
    SePipelineCullMode cullMode;
    SePipelineFrontFace frontFace;
    SeSamplingType samplingType;
} SeGraphicsRenderPipelineCreateInfo;

typedef struct SeComputeRenderPipelineCreateInfo
{
    SeRenderObject* device;
    SePipelineProgram program;
} SeComputeRenderPipelineCreateInfo;

typedef struct SeFramebufferCreateInfo
{
    SeRenderObject** attachmentsPtr;
    size_t numAttachments;
    SeRenderObject* renderPass;
    SeRenderObject* device;
} SeFramebufferCreateInfo;

typedef struct SeMemoryBufferCreateInfo
{
    SeRenderObject* device;
    size_t size;
    SeMemoryBufferUsage usage;
    SeMemoryBufferVisibility visibility;
} SeMemoryBufferCreateInfo;

typedef struct SeResourceSetBinding
{
    SeRenderObject* object;
    SeRenderObject* sampler;
} SeResourceSetBinding;

typedef struct SeResourceSetRequestInfo
{
    SeRenderObject* device;
    SeRenderObject* pipeline;
    size_t set;
    SeResourceSetBinding* bindings;
    size_t numBindings;
} SeResourceSetRequestInfo;

typedef struct SeCommandBufferRequestInfo
{
    SeRenderObject* device;
    SeCommandBufferUsage usage;
} SeCommandBufferRequestInfo;

typedef struct SeCommandBindPipelineInfo
{
    SeRenderObject* pipeline;
    SeRenderObject* framebuffer;
} SeCommandBindPipelineInfo;

typedef struct SeCommandDrawInfo
{
    uint32_t numVertices;
    uint32_t numInstances;
} SeCommandDrawInfo;

typedef struct SeCommandBindResourceSetInfo
{
    SeRenderObject* resourceSet;
} SeCommandBindResourceSetInfo;

typedef struct SeRenderAbstractionSubsystemInterface
{
    SeRenderObject* (*device_create)                        (SeRenderDeviceCreateInfo* createInfo);
    void            (*device_wait)                          (SeRenderObject* device);
    size_t          (*get_swap_chain_textures_num)          (SeRenderObject* device);
    SeRenderObject* (*get_swap_chain_texture)               (SeRenderObject* device, size_t textureIndex);
    size_t          (*get_active_swap_chain_texture_index)  (SeRenderObject* device);
    SeSamplingFlags (*get_supported_sampling_types)         (SeRenderObject* device);
    void            (*begin_frame)                          (SeRenderObject* device);
    void            (*end_frame)                            (SeRenderObject* device);
    SeRenderObject* (*program_create)                       (SeRenderProgramCreateInfo* createInfo);
    SeRenderObject* (*texture_create)                       (SeTextureCreateInfo* createInfo);
    uint32_t        (*texture_get_width)                    (SeRenderObject* texture);
    uint32_t        (*texture_get_height)                   (SeRenderObject* texture);
    SeRenderObject* (*sampler_create)                       (SeSamplerCreateInfo* createInfo);
    SeRenderObject* (*render_pass_create)                   (SeRenderPassCreateInfo* createInfo);
    SeRenderObject* (*render_pipeline_graphics_create)      (SeGraphicsRenderPipelineCreateInfo* createInfo);
    SeRenderObject* (*render_pipeline_compute_create)       (SeComputeRenderPipelineCreateInfo* createInfo);
    SeRenderObject* (*framebuffer_create)                   (SeFramebufferCreateInfo* createInfo);
    SeRenderObject* (*resource_set_request)                 (SeResourceSetRequestInfo* requestInfo);
    SeRenderObject* (*memory_buffer_create)                 (SeMemoryBufferCreateInfo* createInfo);
    void*           (*memory_buffer_get_mapped_address)     (SeRenderObject* buffer);
    SeRenderObject* (*command_buffer_request)               (SeCommandBufferRequestInfo* requestInfo);
    void            (*command_buffer_submit)                (SeRenderObject* cmdBuffer);
    void            (*command_bind_pipeline)                (SeRenderObject* cmdBuffer, SeCommandBindPipelineInfo* commandInfo);
    void            (*command_draw)                         (SeRenderObject* cmdBuffer, SeCommandDrawInfo* commandInfo);
    void            (*command_bind_resource_set)            (SeRenderObject* cmdBuffer, SeCommandBindResourceSetInfo* commandInfo);
    void            (*perspective_projection_matrix)        (struct SeFloat4x4* result, float fovDeg, float aspect, float nearPlane, float farPlane);
} SeRenderAbstractionSubsystemInterface;

size_t se_size_parameter_dynamic_default_width(size_t windowWidth, size_t windowHeight)
{
    return windowWidth;
}

size_t se_size_parameter_dynamic_default_height(size_t windowWidth, size_t windowHeight)
{
    return windowHeight;
}

size_t se_size_parameter_get(SeSizeParamater* parameter, size_t width, size_t height)
{
    if (parameter->type == SE_SIZE_PARAMETER_CONSTANT) return parameter->size;
    else return parameter->dynamicSize(width, height);
}

#endif
