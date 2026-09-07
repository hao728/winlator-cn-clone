#ifndef GLADIO_GL_DSA_H
#define GLADIO_GL_DSA_H

#include "gl_context.h"

// 缓存 id → 真实 GL 名（处理 0→displayBuffer 映射）。仅用于 DSA restore 的
// 真实层恢复，不修改任何缓存状态。
static inline GLuint realFramebufferId(GLuint id) {
    if (id == 0) id = currentRenderer->displayBuffer;
    GLFramebuffer* framebuffer = SparseArray_get(currentRenderer->clientState.framebuffers, id);
    return framebuffer ? framebuffer->id : id;
}

static inline void handleDSARequest(GLContext* context, short requestCode) {
    switch (requestCode) {
        case REQUEST_CODE_GL_DSA_SAVE_ACTIVE_TEXTURE: {
            GLuint unit = ArrayBuffer_getInt(&context->inputBuffer);
            GLuint oldUnit = GL_TEXTURE0 + currentRenderer->clientState.activeTexture;
            if (unit != oldUnit) {
                GLTexture_setActiveUnit(unit);
                context->savedDSAId = oldUnit;
            }
            break;
        }
        case REQUEST_CODE_GL_DSA_RESTORE_ACTIVE_TEXTURE:
            if (context->savedDSAId > 0)  {
                GLTexture_setActiveUnit(context->savedDSAId);
                context->savedDSAId = 0;
            }
            break;
        case REQUEST_CODE_GL_DSA_SAVE_BOUND_TEXTURE: {
            GLuint texture = ArrayBuffer_getInt(&context->inputBuffer);
            GLenum target = GLTexture_get(texture)->type;
            GLTexture* oldTexture = GLTexture_getBound(target);
            context->savedDSATarget = target;
            context->savedDSAId = oldTexture ? oldTexture->id : 0;
            GLTexture_bind(target, texture);
            break;
        }
        case REQUEST_CODE_GL_DSA_RESTORE_BOUND_TEXTURE:
            GLTexture_bind(context->savedDSATarget, context->savedDSAId);
            context->savedDSATarget = 0;
            context->savedDSAId = 0;
            break;
        case REQUEST_CODE_GL_DSA_SAVE_BOUND_BUFFER: {
            uint64_t requestData = ArrayBuffer_getLong(&context->inputBuffer);
            UNPACK32(requestData, target, buffer);
            GLBuffer* oldBuffer = GLBuffer_getBound(target);
            context->savedDSATarget = target;
            context->savedDSAId = oldBuffer ? oldBuffer->id : 0;
            GLBuffer_bind(target, buffer);
            break;
        }
        case REQUEST_CODE_GL_DSA_RESTORE_BOUND_BUFFER:
            GLBuffer_bind(context->savedDSATarget, context->savedDSAId);
            context->savedDSATarget = 0;
            context->savedDSAId = 0;
            break;
        case REQUEST_CODE_GL_DSA_SAVE_BOUND_ARB_PROGRAM: {
            GLuint program = ArrayBuffer_getInt(&context->inputBuffer);
            GLenum target = ARBProgram_get(program)->type;
            ARBProgram* oldProgram = ARBProgram_getBound(target);
            context->savedDSATarget = target;
            context->savedDSAId = oldProgram ? oldProgram->id : 0;
            ARBProgram_bind(target, program);
            break;
        }
        case REQUEST_CODE_GL_DSA_RESTORE_BOUND_ARB_PROGRAM:
            ARBProgram_bind(context->savedDSATarget, context->savedDSAId);
            context->savedDSATarget = 0;
            context->savedDSAId = 0;
            break;
        case REQUEST_CODE_GL_DSA_SAVE_BOUND_FRAMEBUFFER: {
            GLuint framebuffer = ArrayBuffer_getInt(&context->inputBuffer);
            // 分别记录 DRAW/READ 槽（save 时刻真实绑定的缓存值）。不能记 slot0
            // （GL_FRAMEBUFFER 合并槽）：它只是"客户端最后一次合并绑定"的缓存记录，
            // 与 DRAW/READ 各自绑定可能不一致，按它恢复会把真实绑定点拉偏。
            context->savedDSAId = currentRenderer->clientState.framebuffer[indexOfGLTarget(GL_DRAW_FRAMEBUFFER)];
            context->savedDSAId2 = currentRenderer->clientState.framebuffer[indexOfGLTarget(GL_READ_FRAMEBUFFER)];
            glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
            break;
        }
        case REQUEST_CODE_GL_DSA_RESTORE_BOUND_FRAMEBUFFER:
            // 只恢复真实层、不经 GLFramebuffer_bind：DSA 序列期间客户端可能已发新的
            // bind 命令更新缓存，把缓存拉回 save 时刻会与客户端状态缓存错位。
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, realFramebufferId(context->savedDSAId));
            glBindFramebuffer(GL_READ_FRAMEBUFFER, realFramebufferId(context->savedDSAId2));
            context->savedDSAId = 0;
            context->savedDSAId2 = 0;
            break;
        case REQUEST_CODE_GL_DSA_SAVE_BOUND_RENDERBUFFER: {
            GLuint renderbuffer = ArrayBuffer_getInt(&context->inputBuffer);
            context->savedDSAId = currentRenderer->clientState.renderbuffer;
            glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
            break;
        }
        case REQUEST_CODE_GL_DSA_RESTORE_BOUND_RENDERBUFFER:
            glBindRenderbuffer(GL_RENDERBUFFER, context->savedDSAId);
            context->savedDSAId = 0;
            break;
    }
}

#endif
