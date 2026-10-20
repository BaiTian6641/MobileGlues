// JNI helper: resolve the MobileGlues core already loaded by the renderer plugin
// and call its bridge entry points. This avoids loading a second copy with a
// separate EGL/GL state in the Minecraft class loader.
#include <jni.h>
#include <dlfcn.h>
#include <cstdio>

namespace {
void* g_core = nullptr;
using AvailableFn = jboolean (*)(JNIEnv*, jclass);
using SubmitFn = jint (*)(JNIEnv*, jclass);
using ScaleFn = jfloat (*)(JNIEnv*, jclass);
using ViewportFn = void (*)(JNIEnv*, jclass);
using CameraFn = void (*)(JNIEnv*, jclass, jfloatArray, jfloatArray, jdoubleArray);
using CameraInvalidFn = void (*)(JNIEnv*, jclass);
using BridgeModeFn = void (*)(JNIEnv*, jclass);
using TraceFn = jstring (*)(JNIEnv*, jclass, jint, jint, jint, jint);
using FinalScopeFn = void (*)(JNIEnv*, jclass, jboolean);
using SceneDepthFn = void (*)(JNIEnv*, jclass, jint);
AvailableFn g_available = nullptr;
SubmitFn g_submit = nullptr;
ScaleFn g_scale = nullptr;
ViewportFn g_viewport = nullptr;
CameraFn g_camera = nullptr;
CameraInvalidFn g_camera_invalid = nullptr;
BridgeModeFn g_bridge_mode = nullptr;
TraceFn g_trace = nullptr;
FinalScopeFn g_final_scope = nullptr;
SceneDepthFn g_scene_depth = nullptr;
} // namespace

extern "C" JNIEXPORT jboolean JNICALL
Java_com_googlebook_fsrbridge_NativeBridge_init(JNIEnv* env, jclass, jstring path) {
    if (g_core) return JNI_TRUE;
    const char* p = path ? env->GetStringUTFChars(path, nullptr) : nullptr;
    if (!p) return JNI_FALSE;
    g_core = dlopen(p, RTLD_NOW | RTLD_NOLOAD);
    if (!g_core) g_core = dlopen(p, RTLD_NOW);
    env->ReleaseStringUTFChars(path, p);
    if (!g_core) return JNI_FALSE;
    g_available = reinterpret_cast<AvailableFn>(
        dlsym(g_core, "Java_com_googlebook_fsrbridge_Fsr3Bridge_nativeAvailable"));
    g_submit = reinterpret_cast<SubmitFn>(
        dlsym(g_core, "Java_com_googlebook_fsrbridge_Fsr3Bridge_nativeSubmit"));
    g_scale = reinterpret_cast<ScaleFn>(
        dlsym(g_core, "Java_com_googlebook_fsrbridge_Fsr3Bridge_nativeScale"));
    g_viewport = reinterpret_cast<ViewportFn>(
        dlsym(g_core, "Java_com_googlebook_fsrbridge_Fsr3Bridge_nativeViewport"));
    g_camera = reinterpret_cast<CameraFn>(
        dlsym(g_core, "Java_com_googlebook_fsrbridge_Fsr3Bridge_nativeCamera"));
    g_camera_invalid = reinterpret_cast<CameraInvalidFn>(
        dlsym(g_core, "Java_com_googlebook_fsrbridge_Fsr3Bridge_nativeCameraInvalid"));
    g_bridge_mode = reinterpret_cast<BridgeModeFn>(
        dlsym(g_core, "Java_com_googlebook_fsrbridge_Fsr3Bridge_nativeBridgeMode"));
    g_trace = reinterpret_cast<TraceFn>(
        dlsym(g_core, "Java_com_googlebook_fsrbridge_Fsr3Bridge_nativeTrace"));
    g_final_scope = reinterpret_cast<FinalScopeFn>(
        dlsym(g_core, "Java_com_googlebook_fsrbridge_Fsr3Bridge_nativeFinalScope"));
    g_scene_depth = reinterpret_cast<SceneDepthFn>(
        dlsym(g_core, "Java_com_googlebook_fsrbridge_Fsr3Bridge_nativeSetSceneDepth"));
    return (g_available && g_submit) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_googlebook_fsrbridge_NativeBridge_available(JNIEnv* env, jclass clazz) {
    if (!g_available) return JNI_FALSE;
    jclass target = env->FindClass("com/googlebook/fsrbridge/Fsr3Bridge");
    if (!target) return JNI_FALSE;
    jboolean result = g_available(env, target);
    env->DeleteLocalRef(target);
    return result;
}

extern "C" JNIEXPORT jfloat JNICALL
Java_com_googlebook_fsrbridge_NativeBridge_scale(JNIEnv* env, jclass clazz) {
    if (!g_scale) return 1.0f;
    jclass target = env->FindClass("com/googlebook/fsrbridge/Fsr3Bridge");
    if (!target) return 1.0f;
    jfloat result = g_scale(env, target);
    env->DeleteLocalRef(target);
    return result;
}

extern "C" JNIEXPORT void JNICALL
Java_com_googlebook_fsrbridge_NativeBridge_viewport(JNIEnv* env, jclass clazz) {
    if (!g_viewport) return;
    jclass target = env->FindClass("com/googlebook/fsrbridge/Fsr3Bridge");
    if (!target) return;
    g_viewport(env, target);
    env->DeleteLocalRef(target);
}

extern "C" JNIEXPORT void JNICALL
Java_com_googlebook_fsrbridge_NativeBridge_bridgeMode(JNIEnv* env, jclass clazz) {
    if (!g_bridge_mode) return;
    jclass target = env->FindClass("com/googlebook/fsrbridge/Fsr3Bridge");
    if (!target) return;
    g_bridge_mode(env, target);
    env->DeleteLocalRef(target);
}

extern "C" JNIEXPORT void JNICALL
Java_com_googlebook_fsrbridge_NativeBridge_invalidateCamera(JNIEnv* env, jclass clazz) {
    if (!g_camera_invalid) return;
    jclass target = env->FindClass("com/googlebook/fsrbridge/Fsr3Bridge");
    if (!target) return;
    g_camera_invalid(env, target);
    env->DeleteLocalRef(target);
}

extern "C" JNIEXPORT void JNICALL
Java_com_googlebook_fsrbridge_NativeBridge_camera(JNIEnv* env, jclass clazz,
                                                  jfloatArray modelView, jfloatArray projection,
                                                  jdoubleArray cameraPosition) {
    if (!g_camera) return;
    jclass target = env->FindClass("com/googlebook/fsrbridge/Fsr3Bridge");
    if (!target) return;
    g_camera(env, target, modelView, projection, cameraPosition);
    env->DeleteLocalRef(target);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_googlebook_fsrbridge_NativeBridge_submit(JNIEnv* env, jclass clazz) {
    if (!g_submit) return -1;
    jclass target = env->FindClass("com/googlebook/fsrbridge/Fsr3Bridge");
    if (!target) return -1;
    jint result = g_submit(env, target);
    env->DeleteLocalRef(target);
    return result;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_googlebook_fsrbridge_NativeBridge_trace(JNIEnv* env, jclass clazz, jint op, jint role,
                                                 jint frame, jint event) {
    if (!g_trace) return nullptr;
    jclass target = env->FindClass("com/googlebook/fsrbridge/Fsr3Bridge");
    if (!target) return nullptr;
    jstring result = g_trace(env, target, op, role, frame, event);
    env->DeleteLocalRef(target);
    return result;
}

extern "C" JNIEXPORT void JNICALL
Java_com_googlebook_fsrbridge_NativeBridge_finalScope(JNIEnv* env, jclass clazz, jboolean active) {
    if (!g_final_scope) return;
    jclass target = env->FindClass("com/googlebook/fsrbridge/Fsr3Bridge");
    if (!target) return;
    g_final_scope(env, target, active);
    env->DeleteLocalRef(target);
}

extern "C" JNIEXPORT void JNICALL
Java_com_googlebook_fsrbridge_NativeBridge_setSceneDepth(JNIEnv* env, jclass clazz, jint texture) {
    if (!g_scene_depth) return;
    jclass target = env->FindClass("com/googlebook/fsrbridge/Fsr3Bridge");
    if (!target) return;
    g_scene_depth(env, target, texture);
    env->DeleteLocalRef(target);
}
