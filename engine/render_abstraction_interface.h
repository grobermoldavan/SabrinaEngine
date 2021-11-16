#ifndef _SE_RENDER_ABSTRACTION_INTERFACE_H_
#define _SE_RENDER_ABSTRACTION_INTERFACE_H_

#include <inttypes.h>
#include <stdbool.h>

#define SE_RENDER_PASS_UNUSED_ATTACHMENT ~(0ul)
#define SE_RENDER_PASS_MAX_ATTACHMENTS 32

typedef enum SeRenderHandleType
{
    SE_RENDER_DEVICE,
    SE_RENDER_PROGRAM,
    SE_RENDER_TEXTURE,
    SE_RENDER_PASS,
    SE_RENDER_PIPELINE,
    SE_RENDER_FRAMEBUFFER,
    SE_RENDER_COMMAND_BUFFER,
} SeRenderHandleType;

typedef enum SeTextureFormat
{
    SE_TEXTURE_FORMAT_DEPTH_STENCIL,
    SE_TEXTURE_FORMAT_SWAP_CHAIN,
    SE_TEXTURE_FORMAT_RGBA_8,
    SE_TEXTURE_FORMAT_RGBA_32F,
} SeTextureFormat;

typedef enum SeAttachmentLoadOp
{
    SE_LOAD_NOTHING,
    SE_LOAD_CLEAR,
    SE_LOAD_LOAD,
} SeAttachmentLoadOp;

typedef enum SeAttachmentStoreOp
{
    SE_STORE_NOTHING,
    SE_STORE_STORE,
} SeAttachmentStoreOp;

typedef enum SePipelinePoligonMode
{
    SE_POLIGON_FILL,
    SE_POLIGON_LINE,
    SE_POLIGON_POINT,
} SePipelinePoligonMode;

typedef enum SePipelineCullMode
{
    SE_CULL_NONE,
    SE_CULL_FRONT,
    SE_CULL_BACK,
    SE_CULL_FRONT_BACK,
} SePipelineCullMode;

typedef enum SePipelineFrontFace
{
    SE_CLOCKWISE,
    SE_COUNTER_CLOCKWISE,
} SePipelineFrontFace;

typedef enum SeMultisamplingType
{
    SE_SAMPLE_1,
    SE_SAMPLE_2,
    SE_SAMPLE_4,
    SE_SAMPLE_8,
} SeMultisamplingType;

typedef enum SeStencilOp
{
    SE_STENCIL_KEEP,
    SE_STENCIL_ZERO,
    SE_STENCIL_REPLACE,
    SE_STENCIL_INCREMENT_AND_CLAMP,
    SE_STENCIL_DECREMENT_AND_CLAMP,
    SE_STENCIL_INVERT,
    SE_STENCIL_INCREMENT_AND_WRAP,
    SE_STENCIL_DECREMENT_AND_WRAP,
} SeStencilOp;

typedef enum SeDepthOp
{
    SE_DEPTH_NOTHING,
    SE_DEPTH_READ_WRITE,
    SE_DEPTH_READ,
} SeDepthOp;

typedef enum SeCompareOp
{
    SE_COMPARE_NEVER,
    SE_COMPARE_LESS,
    SE_COMPARE_EQUAL,
    SE_COMPARE_LESS_OR_EQUAL,
    SE_COMPARE_GREATER,
    SE_COMPARE_NOT_EQUAL,
    SE_COMPARE_GREATER_OR_EQUAL,
    SE_COMPARE_ALWAYS,
} SeCompareOp;

typedef enum SeCommandBufferUsage
{
    SE_USAGE_GRAPHICS,
    SE_USAGE_COMPUTE,
    SE_USAGE_TRANSFER,
} SeCommandBufferUsage;

typedef enum SeProgramStageFlags
{
    SE_STAGE_VERTEX   = 0x00000001,
    SE_STAGE_FRAGMENT = 0x00000002,
    SE_STAGE_COMPUTE  = 0x00000004,
} SeProgramStageFlags;

typedef enum SeMemoryBufferUsage
{
    SE_BUFFER_TRANSFER_SRC      = 0x00000001,
    SE_BUFFER_TRANSFER_DST      = 0x00000002,
    SE_BUFFER_UNIFORM_TEXEL     = 0x00000004,
    SE_BUFFER_STORAGE_TEXEL     = 0x00000008,
    SE_BUFFER_UNIFORM_BUFFER    = 0x00000010,
    SE_BUFFER_STORAGE_BUFFER    = 0x00000020,
    SE_BUFFER_INDEX_BUFFER      = 0x00000040,
    SE_BUFFER_VERTEX_BUFFER     = 0x00000080,
    SE_BUFFER_INDIRECT_BUFFER   = 0x00000100,
} SeMemoryBufferUsage;

typedef void (*SeDestroyHandleFunc)(struct SeRenderObject* handle);

typedef struct SeRenderObject
{
    uint64_t handleType;
    SeDestroyHandleFunc destroy;
} SeRenderObject;

typedef struct SeRenderDeviceCreateInfo
{
    struct SeWindowHandle* window;
    struct SeAllocatorBindings* persistentAllocator;
    struct SeAllocatorBindings* frameAllocator;
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
} SeTextureCreateInfo;

typedef struct SeRenderPassAttachment
{
    SeTextureFormat format;
    SeAttachmentLoadOp loadOp;
    SeAttachmentStoreOp storeOp;
    SeMultisamplingType samples;
} SeRenderPassAttachment;

typedef struct SeRenderPassSubpass
{
    uint32_t* colorRefs;        // each value references colorAttachments array
    size_t numColorRefs;
    uint32_t* inputRefs;        // each value references colorAttachments array
    size_t numInputRefs;
    uint32_t* resolveRefs;      // each value references colorAttachments array
    size_t numResolveRefs;
    SeDepthOp depthOp;     // SE_DEPTH_NOTHING means depth attachment is not used in subpass
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
    float minDepthBounds;
    float maxDepthBounds;
    bool isTestEnabled;
    bool isWriteEnabled;
    bool isBoundsTestEnabled;
    SeCompareOp compareOp;
} SeDepthTestState;

typedef struct SeGraphicsRenderPipelineCreateInfo
{
    SeRenderObject* device;
    SeRenderObject* renderPass;
    SeRenderObject* vertexProgram;
    SeRenderObject* fragmentProgram;
    SeStencilOpState* frontStencilOpState;
    SeStencilOpState* backStencilOpState;
    SeDepthTestState* depthTestState;
    size_t subpassIndex;
    SePipelinePoligonMode poligonMode;
    SePipelineCullMode cullMode;
    SePipelineFrontFace frontFace;
    SeMultisamplingType multisamplingType;
} SeGraphicsRenderPipelineCreateInfo;

typedef struct SeFramebufferCreateInfo
{
    SeRenderObject** attachmentsPtr;
    size_t numAttachments;
    SeRenderObject* renderPass;
    SeRenderObject* device;
} SeFramebufferCreateInfo;

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
    uint32_t vertexCount;
} SeCommandDrawInfo;

typedef struct SeRenderAbstractionSubsystemInterface
{
    SeRenderObject* (*device_create)                        (SeRenderDeviceCreateInfo* createInfo);
    void            (*device_wait)                          (SeRenderObject* device);
    size_t          (*get_swap_chain_textures_num)          (SeRenderObject* device);
    SeRenderObject* (*get_swap_chain_texture)               (SeRenderObject* device, size_t textureIndex);
    size_t          (*get_active_swap_chain_texture_index)  (SeRenderObject* device);
    void            (*begin_frame)                          (SeRenderObject* device);
    void            (*end_frame)                            (SeRenderObject* device);
    SeRenderObject* (*program_create)                       (SeRenderProgramCreateInfo* createInfo);
    SeRenderObject* (*texture_create)                       (SeTextureCreateInfo* createInfo);
    SeRenderObject* (*render_pass_create)                   (SeRenderPassCreateInfo* createInfo);
    SeRenderObject* (*render_pipeline_graphics_create)      (SeGraphicsRenderPipelineCreateInfo* createInfo);
    SeRenderObject* (*framebuffer_create)                   (SeFramebufferCreateInfo* createInfo);
    SeRenderObject* (*command_buffer_request)               (SeCommandBufferRequestInfo* requestInfo);
    void            (*command_buffer_submit)                (SeRenderObject* buffer);
    void            (*command_bind_pipeline)                (SeRenderObject* buffer, SeCommandBindPipelineInfo* commandInfo);
    void            (*command_draw)                         (SeRenderObject* buffer, SeCommandDrawInfo* commandInfo);
} SeRenderAbstractionSubsystemInterface;

#endif
