#include "gl_framebuffer.h"
#include "gl_context.h"
#include "gl_formats.h"

static GLuint maxFramebufferId = 1;

static GLFramebuffer* createNamedFramebuffer(GLuint id) {
    GLFramebuffer* framebuffer = calloc(1, sizeof(GLFramebuffer));
    framebuffer->ownerId = currentRenderer->contextId;
    framebuffer->id = id;
    SparseArray_put(currentRenderer->clientState.framebuffers, id, framebuffer);
    return framebuffer;
}

GLuint GLFramebuffer_create() {
    GLX_CONTEXT_LOCK();
    GLuint id = maxFramebufferId++;
    createNamedFramebuffer(id);
    GLX_CONTEXT_UNLOCK();
    return id;
}

static void assignFBAttachment(GLenum target, GLuint attachment, FBAttachmentInfo* attachmentInfo) {
    if (attachmentInfo->type == 0) return;
    if (attachmentInfo->type == GL_RENDERBUFFER) {
        glFramebufferRenderbuffer(target, attachment, attachmentInfo->type, attachmentInfo->id);
    }
    else glFramebufferTexture2D(target, attachment, attachmentInfo->type, attachmentInfo->id, attachmentInfo->level);
}

static void recreateFramebuffer(GLenum target, GLFramebuffer* framebuffer) {
    glBindFramebuffer(target, framebuffer->id);

    for (int i = 0; i < MAX_FB_COLOR_ATTACHMENTS; i++) {
        assignFBAttachment(target, GL_COLOR_ATTACHMENT0 + i, &framebuffer->colorAttachment[i]);
    }

    assignFBAttachment(target, GL_DEPTH_ATTACHMENT, &framebuffer->depthAttachment);
    assignFBAttachment(target, GL_STENCIL_ATTACHMENT, &framebuffer->stencilAttachment);

    if (framebuffer->flags & FLAG_READ_BUFFER_NONE) glReadBuffer(GL_NONE);
    for (int i = 0, j = GETEXP(FLAG_READ_BUFFER_COLOR_ATTACHMENT); i < MAX_FB_COLOR_ATTACHMENTS; i++, j++) {
        if (framebuffer->flags & (1<<j)) glReadBuffer(GL_COLOR_ATTACHMENT0 + i);
    }

    if (framebuffer->flags & FLAG_DRAW_BUFFER_NONE) {
        GLenum none = GL_NONE;
        glDrawBuffers(1, &none);
    }

    GLenum drawBufs[MAX_FB_COLOR_ATTACHMENTS];
    int numDrawBufs = 0;
    for (int i = 0, j = GETEXP(FLAG_DRAW_BUFFER_COLOR_ATTACHMENT); i < MAX_FB_COLOR_ATTACHMENTS; i++, j++) {
        if (framebuffer->flags & (1<<j)) drawBufs[numDrawBufs++] = GL_COLOR_ATTACHMENT0 + i;
    }
    if (numDrawBufs > 0) glDrawBuffers(numDrawBufs, drawBufs);
}

void GLFramebuffer_bind(GLenum target, GLuint id) {
    GLX_CONTEXT_LOCK();
    // 槽位记录的是客户端可见的绑定 id：0 就是"默认帧缓冲"。displayBuffer 只是
    // 服务端对 0 的实现细节，【绝不】写进槽——客户端(wined3d)的状态缓存认为
    // 0=默认缓冲、不会重绑，槽里若存 displayBuffer 私有 id，一旦它被销毁/重建
    // （swap 失败、转屏 resize），槽位就悬垂指向表外对象，后续 setReadBuffer/
    // setDrawBuffers/setAttachment 查询即 SIGSEGV（fault addr 0x80 == offsetof(flags)）。
    // 0→displayBuffer 的翻译只在"真实层"这一侧做。
    GLuint clientId = id;
    if (id == 0) id = currentRenderer->displayBuffer;

    if (id == 0) {
        // displayBuffer 尚未就绪（首次建窗前，或销毁与重建之间的瞬时窗口，只可能
        // 出现在本请求线程内部）。真实层绑 0=EGL 默认帧缓冲；缓存记 0。不能
        // createNamedFramebuffer(0)：id=0 是 GL 保留名，建条目只会留下永不被删除
        // 的空壳，污染后续 getBound。
        if (target == GL_FRAMEBUFFER) {
            ARRAYS_FILL(currentRenderer->clientState.framebuffer, MAX_FRAMEBUFFER_TARGETS, 0);
        }
        else currentRenderer->clientState.framebuffer[indexOfGLTarget(target)] = 0;
        glBindFramebuffer(target, 0);
        GLX_CONTEXT_UNLOCK();
        return;
    }

    GLFramebuffer* framebuffer = SparseArray_get(currentRenderer->clientState.framebuffers, id);
    if (!framebuffer) framebuffer = createNamedFramebuffer(id);

    if (target == GL_FRAMEBUFFER) {
        ARRAYS_FILL(currentRenderer->clientState.framebuffer, MAX_FRAMEBUFFER_TARGETS, clientId);
    }
    else currentRenderer->clientState.framebuffer[indexOfGLTarget(target)] = clientId;

    if (currentRenderer->contextId != framebuffer->ownerId) {
        recreateFramebuffer(target, framebuffer);
        framebuffer->ownerId = currentRenderer->contextId;
    }
    else glBindFramebuffer(target, framebuffer->id);
    GLX_CONTEXT_UNLOCK();
}

GLFramebuffer* GLFramebuffer_getBound(GLenum target) {
    GLX_CONTEXT_LOCK();
    GLuint id = currentRenderer->clientState.framebuffer[indexOfGLTarget(target)];
    if (id == 0) id = currentRenderer->displayBuffer;  // 0=默认帧缓冲=displayBuffer
    GLFramebuffer* framebuffer = id ? SparseArray_get(currentRenderer->clientState.framebuffers, id) : NULL;
    GLX_CONTEXT_UNLOCK();
    // 槽位 0 且 displayBuffer 未就绪，或槽位指向表外 id（跨 context 共享删除的
    // 残余）时返回 NULL——由调用方容错（跳过缓存记录、真实层照常执行）。
    return framebuffer;
}

void GLFramebuffer_setAttachment(GLenum target, GLenum attachment, GLenum objectType, GLuint objectId, uint8_t level) {
    GLFramebuffer* framebuffer = GLFramebuffer_getBound(target);
    if (!framebuffer) {
        // 目标对象不在表中（displayBuffer 未就绪 / 跨 context 悬垂）：缓存无从记录，
        // 真实层照常附加即可——不崩溃是第一优先，recreate 时该 attachment 可能丢失
        // 属可接受代价（该场景本就异常）。
        FBAttachmentInfo info;
        info.type = objectType;
        info.id = objectId;
        info.level = level;
        assignFBAttachment(target, attachment, &info);
        return;
    }

    if (attachment >= GL_COLOR_ATTACHMENT0 && attachment <= GL_COLOR_ATTACHMENT31) {
        int index = attachment - GL_COLOR_ATTACHMENT0;
        framebuffer->colorAttachment[index].type = objectType;
        framebuffer->colorAttachment[index].id = objectId;
        framebuffer->colorAttachment[index].level = level;
        assignFBAttachment(target, attachment, &framebuffer->colorAttachment[index]);
    }
    else {
        if (attachment == GL_DEPTH_ATTACHMENT ||
            attachment == GL_DEPTH_STENCIL_ATTACHMENT) {
            framebuffer->depthAttachment.type = objectType;
            framebuffer->depthAttachment.id = objectId;
            framebuffer->depthAttachment.level = level;
            assignFBAttachment(target, attachment, &framebuffer->depthAttachment);
        }
        if (attachment == GL_STENCIL_ATTACHMENT ||
            attachment == GL_DEPTH_STENCIL_ATTACHMENT) {
            framebuffer->stencilAttachment.type = objectType;
            framebuffer->stencilAttachment.id = objectId;
            framebuffer->stencilAttachment.level = level;
            assignFBAttachment(target, attachment, &framebuffer->stencilAttachment);
        }
    }
}

void GLFramebuffer_delete(GLuint id) {
    // 槽位清理：slot1/slot2（DRAW/READ）对应真实绑定点，被删对象占有时按 GL 语义
    // 回退默认帧缓冲——bind(槽, 0) 表达这一语义（0 经映射绑 displayBuffer，缓存记 0，
    // 与客户端"0=默认缓冲"的认知一致，displayBuffer 重建后无需客户端重绑）。
    // slot0（GL_FRAMEBUFFER）不是真实绑定点，只是"客户端最后一次合并绑定"的缓存
    // 记录：命中时绝不能调 bind(GL_FRAMEBUFFER, 0)——其 ARRAYS_FILL 会把 slot1/slot2
    // 的客户端记录一并覆盖成 0（若客户端随后只重绑一个目标，另一槽就与真实层错位），
    // 只改写记录为 0 即可。0 是客户端可见的合法值（默认帧缓冲），不是私有 id。
    static const GLenum targetsBySlot[MAX_FRAMEBUFFER_TARGETS] = {
        GL_FRAMEBUFFER, GL_DRAW_FRAMEBUFFER, GL_READ_FRAMEBUFFER
    };
    for (int i = 0; i < MAX_FRAMEBUFFER_TARGETS; i++) {
        if (id == currentRenderer->clientState.framebuffer[i]) {
            if (i == 0) currentRenderer->clientState.framebuffer[0] = 0;
            else GLFramebuffer_bind(targetsBySlot[i], 0);
        }
    }

    GLX_CONTEXT_LOCK();
    GLFramebuffer* framebuffer = SparseArray_get(currentRenderer->clientState.framebuffers, id);
    if (framebuffer && framebuffer->ownerId == currentRenderer->contextId) {
        glDeleteFramebuffers(1, &framebuffer->id);
        SparseArray_remove(currentRenderer->clientState.framebuffers, id);
        free(framebuffer);
    }
    GLX_CONTEXT_UNLOCK();
}

void GLFramebuffer_setReadBuffer(GLenum src) {
    // glReadBuffer 作用于当前 READ 帧缓冲：flags 必须记在 READ 槽对象上。旧实现查
    // slot0（GL_FRAMEBUFFER 合并槽）——客户端单独 bind(GL_READ_FRAMEBUFFER) 后
    // slot0 与 READ 槽不一致，flags 会记错对象（recreate 时 read buffer 状态丢失），
    // 且 slot0 悬垂时即使 READ 槽有效也会误崩。
    bool success = src == GL_NONE || (src >= GL_COLOR_ATTACHMENT0 && src <= GL_COLOR_ATTACHMENT31);
    if (!success) return;  // GL_BACK/GL_FRONT 等默认帧缓冲选择在 GLES3 非法，原样忽略

    GLFramebuffer* framebuffer = GLFramebuffer_getBound(GL_READ_FRAMEBUFFER);
    if (framebuffer) {
        BITMASK_UNSET(framebuffer->flags, FLAG_READ_BUFFER_NONE);
        for (int i = 0, j = GETEXP(FLAG_READ_BUFFER_COLOR_ATTACHMENT); i < MAX_FB_COLOR_ATTACHMENTS; i++, j++) {
            BITMASK_UNSET(framebuffer->flags, (1<<j));
        }
        if (src == GL_NONE) {
            BITMASK_SET(framebuffer->flags, FLAG_READ_BUFFER_NONE);
        }
        else {
            int index = (src - GL_COLOR_ATTACHMENT0) + GETEXP(FLAG_READ_BUFFER_COLOR_ATTACHMENT);
            BITMASK_SET(framebuffer->flags, (1<<index));
        }
    }
    glReadBuffer(src);  // 对象缺失时缓存无从记录，真实层照常执行
}

void GLFramebuffer_setDrawBuffers(GLuint count, GLenum* dst) {
    // glDrawBuffers 作用于当前 DRAW 帧缓冲：flags 记在 DRAW 槽对象上（理由同上）。
    bool success = false;
    for (int i = 0; i < count; i++) {
        if (dst[i] == GL_NONE || (dst[i] >= GL_COLOR_ATTACHMENT0 && dst[i] <= GL_COLOR_ATTACHMENT31)) {
            success = true;
            break;
        }
    }
    if (!success) return;

    GLFramebuffer* framebuffer = GLFramebuffer_getBound(GL_DRAW_FRAMEBUFFER);
    if (framebuffer) {
        BITMASK_UNSET(framebuffer->flags, FLAG_DRAW_BUFFER_NONE);
        for (int i = 0, j = GETEXP(FLAG_DRAW_BUFFER_COLOR_ATTACHMENT); i < MAX_FB_COLOR_ATTACHMENTS; i++, j++) {
            BITMASK_UNSET(framebuffer->flags, (1<<j));
        }
        for (int i = 0; i < count; i++) {
            if (dst[i] == GL_NONE) {
                BITMASK_SET(framebuffer->flags, FLAG_DRAW_BUFFER_NONE);
            }
            else if (dst[i] >= GL_COLOR_ATTACHMENT0 && dst[i] <= GL_COLOR_ATTACHMENT31) {
                int index = (dst[i] - GL_COLOR_ATTACHMENT0) + GETEXP(FLAG_DRAW_BUFFER_COLOR_ATTACHMENT);
                BITMASK_SET(framebuffer->flags, (1<<index));
            }
        }
    }
    glDrawBuffers(count, dst);  // 对象缺失时缓存无从记录，真实层照常执行
}

void GLFramebuffer_getParamsv(GLenum target, GLenum attachment, GLenum pname, GLint *params) {
    *params = 0;
    if (attachment == GL_FRONT_LEFT ||
        attachment == GL_FRONT_RIGHT ||
        attachment == GL_BACK_LEFT ||
        attachment == GL_BACK_RIGHT ||
        attachment == GL_DEPTH ||
        attachment == GL_STENCIL) {

        GLFormatInfo* framebufferFormat = GLFormats_queryInternalformat(GL_TEXTURE_2D, PREFERRED_FRAMEBUFFER_FORMAT, 0, 0, NULL);
        GLFormatInfo* renderbufferFormat = GLFormats_queryInternalformat(GL_RENDERBUFFER, PREFERRED_RENDERBUFFER_FORMAT, 0, 0, NULL);

        switch (pname) {
            case  GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE:
                *params = framebufferFormat->redSize;
                break;
            case  GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE:
                *params = framebufferFormat->greenSize;
                break;
            case  GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE:
                *params = framebufferFormat->blueSize;
                break;
            case  GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE:
                *params = framebufferFormat->alphaSize;
                break;
            case  GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE:
                *params = renderbufferFormat->stencilSize;
                break;
            case  GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE:
                *params = renderbufferFormat->depthSize;
                break;
        }
    }
    else glGetFramebufferAttachmentParameteriv(target, attachment, pname, params);
}