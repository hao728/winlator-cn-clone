#include <jni.h>

#include "winlator.h"

// The @CriticalNative methods below are implemented with the critical native ABI (no JNIEnv*/jclass
// parameters). The built-in dynamic JNI linking only resolves them on Android 12+; on Android 8-11
// they must be registered explicitly with RegisterNatives, otherwise the ABI mismatch crashes.

extern jint JNICALL Java_com_winlator_core_GPUHelper_vkGetApiVersion(void);

extern void JNICALL Java_com_winlator_xconnector_XConnectorEpoll_closeFd(jint fd);

extern jbyte JNICALL Java_com_winlator_xconnector_XInputStream_readByte(jlong nativePtr);
extern jshort JNICALL Java_com_winlator_xconnector_XInputStream_readShort(jlong nativePtr);
extern jint JNICALL Java_com_winlator_xconnector_XInputStream_readInt(jlong nativePtr);
extern jlong JNICALL Java_com_winlator_xconnector_XInputStream_readLong(jlong nativePtr);
extern void JNICALL Java_com_winlator_xconnector_XInputStream_skip(jlong nativePtr, jint length);
extern jint JNICALL Java_com_winlator_xconnector_XInputStream_available(jlong nativePtr);
extern jint JNICALL Java_com_winlator_xconnector_XInputStream_getActivePosition(jlong nativePtr);
extern void JNICALL Java_com_winlator_xconnector_XInputStream_setActivePosition(jlong nativePtr, jint activePosition);
extern jint JNICALL Java_com_winlator_xconnector_XInputStream_getAncillaryFd(jlong nativePtr);

extern void JNICALL Java_com_winlator_xconnector_XOutputStream_setAncillaryFd(jlong nativePtr, jint ancillaryFd);
extern void JNICALL Java_com_winlator_xconnector_XOutputStream_writeByte(jlong nativePtr, jbyte value);
extern void JNICALL Java_com_winlator_xconnector_XOutputStream_writeShort(jlong nativePtr, jshort value);
extern void JNICALL Java_com_winlator_xconnector_XOutputStream_writeInt(jlong nativePtr, jint value);
extern void JNICALL Java_com_winlator_xconnector_XOutputStream_writeLong(jlong nativePtr, jlong value);
extern void JNICALL Java_com_winlator_xconnector_XOutputStream_writePad(jlong nativePtr, jint length);
extern jint JNICALL Java_com_winlator_xconnector_XOutputStream_length(jlong nativePtr);

static const JNINativeMethod GPU_HELPER_METHODS[] = {
    {"vkGetApiVersion", "()I", (void*)Java_com_winlator_core_GPUHelper_vkGetApiVersion},
};

static const JNINativeMethod XCONNECTOR_EPOLL_METHODS[] = {
    {"closeFd", "(I)V", (void*)Java_com_winlator_xconnector_XConnectorEpoll_closeFd},
};

static const JNINativeMethod XINPUT_STREAM_METHODS[] = {
    {"readByte", "(J)B", (void*)Java_com_winlator_xconnector_XInputStream_readByte},
    {"readShort", "(J)S", (void*)Java_com_winlator_xconnector_XInputStream_readShort},
    {"readInt", "(J)I", (void*)Java_com_winlator_xconnector_XInputStream_readInt},
    {"readLong", "(J)J", (void*)Java_com_winlator_xconnector_XInputStream_readLong},
    {"skip", "(JI)V", (void*)Java_com_winlator_xconnector_XInputStream_skip},
    {"available", "(J)I", (void*)Java_com_winlator_xconnector_XInputStream_available},
    {"getActivePosition", "(J)I", (void*)Java_com_winlator_xconnector_XInputStream_getActivePosition},
    {"setActivePosition", "(JI)V", (void*)Java_com_winlator_xconnector_XInputStream_setActivePosition},
    {"getAncillaryFd", "(J)I", (void*)Java_com_winlator_xconnector_XInputStream_getAncillaryFd},
};

static const JNINativeMethod XOUTPUT_STREAM_METHODS[] = {
    {"setAncillaryFd", "(JI)V", (void*)Java_com_winlator_xconnector_XOutputStream_setAncillaryFd},
    {"writeByte", "(JB)V", (void*)Java_com_winlator_xconnector_XOutputStream_writeByte},
    {"writeShort", "(JS)V", (void*)Java_com_winlator_xconnector_XOutputStream_writeShort},
    {"writeInt", "(JI)V", (void*)Java_com_winlator_xconnector_XOutputStream_writeInt},
    {"writeLong", "(JJ)V", (void*)Java_com_winlator_xconnector_XOutputStream_writeLong},
    {"writePad", "(JI)V", (void*)Java_com_winlator_xconnector_XOutputStream_writePad},
    {"length", "(J)I", (void*)Java_com_winlator_xconnector_XOutputStream_length},
};

static int registerCriticalNatives(JNIEnv* env, const char* className, const JNINativeMethod* methods, int numMethods) {
    jclass clazz = (*env)->FindClass(env, className);
    if (clazz == NULL) return 0;

    int success = (*env)->RegisterNatives(env, clazz, methods, numMethods) == JNI_OK;
    (*env)->DeleteLocalRef(env, clazz);
    return success;
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    JNIEnv* env;
    if ((*vm)->GetEnv(vm, (void**)&env, JNI_VERSION_1_6) != JNI_OK) return JNI_ERR;

    if (!registerCriticalNatives(env, "com/winlator/core/GPUHelper", GPU_HELPER_METHODS, ARRAY_SIZE(GPU_HELPER_METHODS))) return JNI_ERR;
    if (!registerCriticalNatives(env, "com/winlator/xconnector/XConnectorEpoll", XCONNECTOR_EPOLL_METHODS, ARRAY_SIZE(XCONNECTOR_EPOLL_METHODS))) return JNI_ERR;
    if (!registerCriticalNatives(env, "com/winlator/xconnector/XInputStream", XINPUT_STREAM_METHODS, ARRAY_SIZE(XINPUT_STREAM_METHODS))) return JNI_ERR;
    if (!registerCriticalNatives(env, "com/winlator/xconnector/XOutputStream", XOUTPUT_STREAM_METHODS, ARRAY_SIZE(XOUTPUT_STREAM_METHODS))) return JNI_ERR;

    return JNI_VERSION_1_6;
}
