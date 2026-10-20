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
AvailableFn g_available = nullptr;
SubmitFn g_submit = nullptr;
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

extern "C" JNIEXPORT jint JNICALL
Java_com_googlebook_fsrbridge_NativeBridge_submit(JNIEnv* env, jclass clazz) {
    if (!g_submit) return -1;
    jclass target = env->FindClass("com/googlebook/fsrbridge/Fsr3Bridge");
    if (!target) return -1;
    jint result = g_submit(env, target);
    env->DeleteLocalRef(target);
    return result;
}
