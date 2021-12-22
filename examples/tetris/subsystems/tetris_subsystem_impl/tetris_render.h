#ifndef _TETRIS_RENDER_H_
#define _TETRIS_RENDER_H_

#include "engine/containers.h"

typedef struct TetrisRenderInitInfo
{
    struct TetrisRenderState* renderState;
    struct SeWindowHandle* window;
    struct SabrinaEngine* engine;
} TetrisRenderInitInfo;

typedef struct TetrisRenderState
{
    struct SePlatformInterface* platformInterface;
    struct SeRenderAbstractionSubsystemInterface* renderInterface;
    struct SeApplicationAllocatorsSubsystemInterface* allocatorsInterface;
    struct SeWindowSubsystemInterface* windowInterface;

    struct SeRenderObject* renderDevice;

    struct SeRenderObject* frameDataBuffer;
    struct SeRenderObject* cubeVerticesBuffer;
    struct SeRenderObject* cubeIndicesBuffer;
    se_sbuffer(struct SeRenderObject*) cubeInstanceBuffers;

    struct SeRenderObject* gridVerticesBuffer;
    struct SeRenderObject* gridIndicesBuffer;
    struct SeRenderObject* gridInstanceBuffer;

    struct SeRenderObject* drawRenderPass;
    struct SeRenderObject* drawVs;
    struct SeRenderObject* drawFs;
    struct SeRenderObject* drawRenderPipeline;
    struct SeRenderObject* drawColorTexture;
    struct SeRenderObject* drawDepthTexture;
    struct SeRenderObject* drawFramebuffer;

    struct SeRenderObject* presentRenderPass;
    struct SeRenderObject* presentVs;
    struct SeRenderObject* presentFs;
    struct SeRenderObject* presentRenderPipeline;
    se_sbuffer(struct SeRenderObject*) presentFramebuffers;
} TetrisRenderState;

void tetris_render_init(TetrisRenderInitInfo* initInfo);
void tetris_render_terminate();
void tetris_render_update(TetrisRenderState* renderState, struct TetrisState* state, const struct SeWindowSubsystemInput* input, float dt);

#endif
