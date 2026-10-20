// MobileGlues - upscaling/mg_fsr_jni.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header
// Extracted source with substituted JNI transport; see README.md.
#include <jni.h>
#include <algorithm>
#include <chrono>
#include <mutex>
#include <cstdio>
#include <iterator>
struct Floats : _jfloatArray { float data[16]{}; };
struct Doubles : _jdoubleArray { double data[3]{}; };
Floats matrices;
Doubles position;
jsize arrayLength(JNIEnv*, jarray a) { return a == &position ? 3 : 16; }
void floatRegion(JNIEnv*, jfloatArray a, jsize start, jsize n, jfloat* out) {
  std::copy_n(static_cast<Floats*>(a)->data + start, n, out);
}
void doubleRegion(JNIEnv*, jdoubleArray a, jsize start, jsize n, jdouble* out) {
  std::copy_n(static_cast<Doubles*>(a)->data + start, n, out);
}
struct CameraState {
    bool valid = false;
    bool has_previous = false;
    float model_view[16]{};
    float projection[16]{};
    double position[3]{};
    float previous_model_view[16]{};
    float previous_projection[16]{};
    double previous_position[3]{};
    std::chrono::steady_clock::time_point last_capture{};
};
CameraState g_camera;
std::mutex g_camera_mutex;
extern "C" JNIEXPORT void JNICALL
Java_com_googlebook_fsrbridge_Fsr3Bridge_nativeCameraInvalid(JNIEnv*, jclass) {
    std::lock_guard<std::mutex> lock(g_camera_mutex);
    g_camera.valid = false;
    g_camera.has_previous = false;
}

extern "C" JNIEXPORT void JNICALL
Java_com_googlebook_fsrbridge_Fsr3Bridge_nativeCamera(JNIEnv* env, jclass, jfloatArray mv, jfloatArray proj,
                                                       jdoubleArray position) {
    if (!mv || !proj || !position) return;
    if (env->GetArrayLength(mv) < 16 || env->GetArrayLength(proj) < 16 ||
        env->GetArrayLength(position) < 3) return;
    float current_mv[16], current_proj[16];
    double current_pos[3];
    env->GetFloatArrayRegion(mv, 0, 16, current_mv);
    env->GetFloatArrayRegion(proj, 0, 16, current_proj);
    env->GetDoubleArrayRegion(position, 0, 3, current_pos);
    std::lock_guard<std::mutex> lock(g_camera_mutex);
    const auto now = std::chrono::steady_clock::now();
    if (g_camera.valid && g_camera.last_capture.time_since_epoch().count() != 0 &&
        std::chrono::duration<float>(now - g_camera.last_capture).count() > 0.25f) {
        g_camera.has_previous = false;
    }
    bool keep_previous = g_camera.valid && g_camera.has_previous;
    if (g_camera.valid) {
        const double dx = current_pos[0] - g_camera.position[0];
        const double dy = current_pos[1] - g_camera.position[1];
        const double dz = current_pos[2] - g_camera.position[2];
        if (dx * dx + dy * dy + dz * dz > 64.0 * 64.0) keep_previous = false;
        std::copy(std::begin(g_camera.model_view), std::end(g_camera.model_view), std::begin(g_camera.previous_model_view));
        std::copy(std::begin(g_camera.projection), std::end(g_camera.projection), std::begin(g_camera.previous_projection));
        std::copy(std::begin(g_camera.position), std::end(g_camera.position), std::begin(g_camera.previous_position));
    }
    g_camera.has_previous = keep_previous;
    std::copy(std::begin(current_mv), std::end(current_mv), std::begin(g_camera.model_view));
    std::copy(std::begin(current_proj), std::end(current_proj), std::begin(g_camera.projection));
    std::copy(std::begin(current_pos), std::end(current_pos), std::begin(g_camera.position));
    g_camera.last_capture = now;
    g_camera.valid = true;
}


int main() {
  JNINativeInterface_ table{};
  table.GetArrayLength = arrayLength;
  table.GetFloatArrayRegion = floatRegion;
  table.GetDoubleArrayRegion = doubleRegion;
  JNIEnv env{&table};
  for (int i=0;i<16;i+=5) matrices.data[i]=1.0f;
  Java_com_googlebook_fsrbridge_Fsr3Bridge_nativeCameraInvalid(&env, nullptr);
  bool mismatch=false;
  for (int i=0;i<3;++i) {
    position.data[0]=0.01*i;
    Java_com_googlebook_fsrbridge_Fsr3Bridge_nativeCamera(&env,nullptr,&matrices,&matrices,&position);
    bool expected=i>0;
    std::printf("capture=%d valid=%d has_previous=%d expected_previous=%d\n",i+1,g_camera.valid,g_camera.has_previous,expected);
    mismatch |= g_camera.has_previous != expected;
  }
  std::printf("continuity_contract=%s\n",mismatch?"FAIL":"PASS");
  return mismatch?1:0;
}
