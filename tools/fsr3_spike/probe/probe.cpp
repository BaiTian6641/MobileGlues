// FSR3 GLES compute feasibility probe.
// Minimal standalone EGL/GLES3.2 compute harness for the operations used by
// the FidelityFX FSR3.1.4 upscaler. No FSR source is linked; shaders here are
// reduced test kernels, not claimed ports.
//
// Build (host):  make
// Build (arm64): ./build_android.sh
//
// Output: a JSON object on stdout. Device/host results are only what this
// program observed; they are not a claim of full FSR3 runtime capability.
#define GL_GLEXT_PROTOTYPES 1
#define EGL_EGLEXT_PROTOTYPES 1
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl32.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <fstream>

static std::vector<std::string> g_exts;

static bool has_ext(const char* name) {
    for (const auto& e : g_exts) if (e == name) return true;
    return false;
}

static std::string jesc(const std::string& s) {
    std::string o;
    for (char c : s) {
        switch (c) {
        case '"': o += "\\\""; break;
        case '\\': o += "\\\\"; break;
        case '\n': o += "\\n"; break;
        case '\r': o += "\\r"; break;
        case '\t': o += "\\t"; break;
        default:
            if ((unsigned char)c < 0x20) { char b[8]; snprintf(b, sizeof b, "\\u%04x", c); o += b; }
            else o += c;
        }
    }
    return o;
}

struct Result {
    std::string name;
    std::string status; // PASS FAIL SKIP NOT_RUN
    std::string detail;
};

static std::vector<Result> g_results;
static bool g_json_first = true;

static void emit_result(const Result& r) {
    if (!g_json_first) printf(",\n");
    g_json_first = false;
    printf("{\"name\":\"%s\",\"status\":\"%s\",\"detail\":\"%s\"}",
           jesc(r.name).c_str(), r.status.c_str(), jesc(r.detail).c_str());
    fflush(stdout);
}

static void record(const std::string& name, const std::string& status, const std::string& detail) {
    g_results.push_back({name, status, detail});
    emit_result(g_results.back());
}

static std::string glerr() {
    GLenum e = glGetError();
    if (e == GL_NO_ERROR) return "gl=0";
    char b[64];
    snprintf(b, sizeof b, "gl=0x%04x", e);
    return b;
}

static GLuint compile(GLenum type, const char* src, std::string& log) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    GLint len = 0;
    glGetShaderiv(s, GL_INFO_LOG_LENGTH, &len);
    if (len > 1) {
        std::string l(len, '\0');
        glGetShaderInfoLog(s, len, nullptr, &l[0]);
        log += l.c_str();
    }
    if (!ok) {
        char b[128];
        snprintf(b, sizeof b, " compile_status=0 err=%s ", glerr().c_str());
        log += b;
        glDeleteShader(s);
        return 0;
    }
    return s;
}

static GLuint link_program(const char* cs, std::string& log) {
    GLuint s = compile(GL_COMPUTE_SHADER, cs, log);
    if (!s) return 0;
    GLuint p = glCreateProgram();
    glAttachShader(p, s);
    glLinkProgram(p);
    glDeleteShader(s);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    GLint len = 0;
    glGetProgramiv(p, GL_INFO_LOG_LENGTH, &len);
    if (len > 1) {
        std::string l(len, '\0');
        glGetProgramInfoLog(p, len, nullptr, &l[0]);
        log += l.c_str();
    }
    if (!ok) {
        char b[128];
        snprintf(b, sizeof b, " link_status=0 err=%s ", glerr().c_str());
        log += b;
        glDeleteProgram(p);
        return 0;
    }
    return p;
}

static void dispatch(GLuint prog, GLuint gx, GLuint gy, GLuint gz) {
    glUseProgram(prog);
    glDispatchCompute(gx, gy, gz);
    glMemoryBarrier(GL_ALL_BARRIER_BITS);
    glFinish();
}

static GLuint make_tex2d(GLenum internal, int w, int h, const void* data = nullptr) {
    GLuint t = 0;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexStorage2D(GL_TEXTURE_2D, 1, internal, w, h);
    if (data) glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glBindTexture(GL_TEXTURE_2D, 0);
    return t;
}

struct FormatCase {
    const char* name;
    GLenum internal;
    GLenum bind_format;
    const char* qual;
    GLenum read_format;
    GLenum read_type;
    int components;
    bool integer;
};

static void test_image_roundtrip(const FormatCase& f) {
    std::string log;
    std::string wr =
        "#version 320 es\n"
        "layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;\n";
    wr += "layout(" + std::string(f.qual) + ", binding = 0) writeonly uniform highp image2D img;\n";
    wr += "void main(){ imageStore(img, ivec2(gl_GlobalInvocationID.xy), vec4(0.5, 0.25, 0.75, 1.0)); }\n";
    GLuint wp = link_program(wr.c_str(), log);
    if (!wp) { record(std::string("image_") + f.name + "_store", "FAIL", "compile/link: " + log); return; }
    GLuint t = make_tex2d(f.internal, 8, 8);
    glBindImageTexture(0, t, 0, GL_FALSE, 0, GL_WRITE_ONLY, f.bind_format);
    std::string binderr = glerr();
    dispatch(wp, 1, 1, 1);
    glDeleteProgram(wp);
    glBindImageTexture(0, 0, 0, GL_FALSE, 0, GL_WRITE_ONLY, f.bind_format);

    // Readback through an FBO.
    GLuint fbo = 0;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t, 0);
    GLenum st = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    float out[4] = {0, 0, 0, 0};
    unsigned int uout[4] = {0, 0, 0, 0};
    GLenum err = GL_NO_ERROR;
    if (f.integer) {
        glReadPixels(0, 0, 1, 1, f.read_format, f.read_type, uout);
        err = glGetError();
        snprintf((char*)out, sizeof(out), "unused");
    } else if (f.read_type == GL_HALF_FLOAT) {
        unsigned short h[4] = {0, 0, 0, 0};
        glReadPixels(0, 0, 1, 1, f.read_format, f.read_type, h);
        err = glGetError();
        auto h2f = [](unsigned short v) {
            unsigned sign = (v >> 15) & 1u, exp = (v >> 10) & 0x1f, man = v & 0x3ff;
            float r;
            if (exp == 0) r = ldexpf((float)man, -24);
            else if (exp == 31) r = man ? NAN : INFINITY;
            else r = ldexpf((float)(man + 1024), (int)exp - 25);
            return sign ? -r : r;
        };
        for (int i = 0; i < f.components; i++) out[i] = h2f(h[i]);
    } else if (f.read_type == GL_UNSIGNED_BYTE) {
        unsigned char b[4] = {0, 0, 0, 0};
        glReadPixels(0, 0, 1, 1, f.read_format, f.read_type, b);
        err = glGetError();
        for (int i = 0; i < f.components; i++) out[i] = b[i] / 255.0f;
    } else {
        glReadPixels(0, 0, 1, 1, f.read_format, f.read_type, out);
        err = glGetError();
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);

    bool complete = (st == GL_FRAMEBUFFER_COMPLETE);
    bool read_ok = (err == GL_NO_ERROR) && complete;
    bool val_ok = false;
    if (read_ok) {
        if (f.integer) {
            val_ok = (uout[0] == 0x3f000000u); // not expected: integer path uses separate test
        } else {
            float exp0 = 0.5f, exp1 = 0.25f, exp2 = 0.75f, exp3 = 1.0f;
            float tol = 0.02f;
            if (f.components >= 1) val_ok = fabsf(out[0] - exp0) <= tol;
            if (f.components >= 2) val_ok = val_ok && fabsf(out[1] - exp1) <= tol;
            if (f.components >= 3) val_ok = val_ok && fabsf(out[2] - exp2) <= tol;
            if (f.components >= 4) val_ok = val_ok && fabsf(out[3] - exp3) <= tol;
        }
    }
    char d[512];
    if (f.integer) {
        snprintf(d, sizeof d, "bind_err=%s fbo_complete=%d read_err=0x%04x raw=%u,%u,%u,%u expected_add_test_separate",
                 binderr.c_str(), (int)complete, err, uout[0], uout[1], uout[2], uout[3]);
    } else {
        snprintf(d, sizeof d, "bind_err=%s fbo_complete=%d read_err=0x%04x got=%.4f,%.4f,%.4f,%.4f exp=0.5,0.25,0.75,1.0",
                 binderr.c_str(), (int)complete, err, out[0], out[1], out[2], out[3]);
    }
    record(std::string("image_") + f.name + "_roundtrip", (read_ok && val_ok) ? "PASS" : "FAIL", d);
    glDeleteTextures(1, &t);
}

static void test_image_load_to_ssbo(const FormatCase& f) {
    std::string log;
    std::string src = "#version 320 es\nlayout(local_size_x = 1) in;\n";
    src += "layout(" + std::string(f.qual) + ", binding = 0) readonly uniform highp image2D img;\n";
    src += "layout(std430, binding = 1) writeonly buffer Out { vec4 v; } o;\n";
    src += "void main(){ o.v = imageLoad(img, ivec2(0,0)); }\n";
    GLuint p = link_program(src.c_str(), log);
    if (!p) { record(std::string("image_") + f.name + "_load_ssbo", "FAIL", "compile/link: " + log); return; }
    GLuint t = make_tex2d(f.internal, 8, 8);
    // Seed with a store program first.
    std::string wlog;
    std::string ws = "#version 320 es\nlayout(local_size_x = 8, local_size_y = 8) in;\n";
    ws += "layout(" + std::string(f.qual) + ", binding = 0) writeonly uniform highp image2D img;\n";
    ws += "void main(){ imageStore(img, ivec2(gl_GlobalInvocationID.xy), vec4(0.5,0.25,0.75,1.0)); }\n";
    GLuint wp = link_program(ws.c_str(), wlog);
    if (wp) {
        glBindImageTexture(0, t, 0, GL_FALSE, 0, GL_WRITE_ONLY, f.bind_format);
        dispatch(wp, 1, 1, 1);
        glDeleteProgram(wp);
    }
    GLuint ssbo = 0;
    glGenBuffers(1, &ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    float zero[4] = {0, 0, 0, 0};
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(zero), zero, GL_DYNAMIC_READ);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ssbo);
    glBindImageTexture(0, t, 0, GL_FALSE, 0, GL_READ_ONLY, f.bind_format);
    std::string binderr = glerr();
    dispatch(p, 1, 1, 1);
    glDeleteProgram(p);
    float* v = (float*)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, sizeof(zero), GL_MAP_READ_BIT);
    float got[4] = {0, 0, 0, 0};
    if (v) { memcpy(got, v, sizeof(got)); glUnmapBuffer(GL_SHADER_STORAGE_BUFFER); }
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    glBindImageTexture(0, 0, 0, GL_FALSE, 0, GL_READ_ONLY, f.bind_format);
    char d[512];
    bool ok = v && fabsf(got[0] - 0.5f) < 0.02f;
    snprintf(d, sizeof d, "bind_err=%s got=%.4f,%.4f,%.4f,%.4f exp_x=0.5", binderr.c_str(), got[0], got[1], got[2], got[3]);
    record(std::string("image_") + f.name + "_load_ssbo", ok ? "PASS" : "FAIL", d);
    glDeleteBuffers(1, &ssbo);
    glDeleteTextures(1, &t);
}

static void test_r32ui_atomics() {
    // r32ui image atomic add/max/min plus SSBO atomic and 256-thread shared reduction.
    const char* src =
        "#version 320 es\n"
        "layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;\n"
        "layout(r32ui, binding = 0) coherent uniform highp uimage2D img;\n"
        "layout(std430, binding = 1) coherent buffer Out { uint add; uint maxv; uint minv; uint count; uint sum; } o;\n"
        "shared uint scratch[256];\n"
        "void main(){\n"
        "  uint idx = gl_LocalInvocationIndex;\n"
        "  scratch[idx] = idx;\n"
        "  memoryBarrierShared();\n"
        "  barrier();\n"
        "  uint s = 0u;\n"
        "  for (uint i = 0u; i < 256u; ++i) s += scratch[i];\n"
        "  memoryBarrierShared();\n"
        "  barrier();\n"
        "  imageAtomicAdd(img, ivec2(0), 1u);\n"
        "  imageAtomicMax(img, ivec2(1), idx);\n"
        "  imageAtomicMin(img, ivec2(2), idx);\n"
        "  atomicAdd(o.count, 1u);\n"
        "  memoryBarrier();\n"
        "  barrier();\n"
        "  if (idx == 0u) {\n"
        "    o.add = imageLoad(img, ivec2(0)).r;\n"
        "    o.maxv = imageLoad(img, ivec2(1)).r;\n"
        "    o.minv = imageLoad(img, ivec2(2)).r;\n"
        "    o.sum = s;\n"
        "  }\n"
        "}\n";
    std::string log;
    GLuint p = link_program(src, log);
    if (!p) { record("r32ui_image_atomics_shared_ssbo", "FAIL", "compile/link: " + log); return; }
    GLuint t = make_tex2d(GL_R32UI, 4, 4);
    // zero image
    unsigned zero[16] = {0};
    glBindTexture(GL_TEXTURE_2D, t);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 4, 4, GL_RED_INTEGER, GL_UNSIGNED_INT, zero);
    glBindTexture(GL_TEXTURE_2D, 0);
    GLuint ssbo = 0;
    glGenBuffers(1, &ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    unsigned init[5] = {0, 0, 0, 0, 0};
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(init), init, GL_DYNAMIC_READ);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ssbo);
    glBindImageTexture(0, t, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI);
    std::string binderr = glerr();
    dispatch(p, 1, 1, 1);
    glDeleteProgram(p);
    unsigned* v = (unsigned*)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, sizeof(init), GL_MAP_READ_BIT);
    unsigned got[5] = {0, 0, 0, 0, 0};
    if (v) { memcpy(got, v, sizeof(got)); glUnmapBuffer(GL_SHADER_STORAGE_BUFFER); }
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    glBindImageTexture(0, 0, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI);
    bool ok = v && got[0] == 256 && got[1] == 255 && got[2] == 0 && got[3] == 256 && got[4] == 32640;
    char d[512];
    snprintf(d, sizeof d, "bind_err=%s add=%u(max 256) max=%u(255) min=%u(0) ssbo_count=%u(256) shared_sum=%u(32640)",
             binderr.c_str(), got[0], got[1], got[2], got[3], got[4]);
    record("r32ui_image_atomics_shared_ssbo", ok ? "PASS" : "FAIL", d);
    glDeleteBuffers(1, &ssbo);
    glDeleteTextures(1, &t);
}

static void test_alias_read_write_image_units() {
    // FSR3 reads and writes the same RGBA16F/RGBA8 images (SPD mips, lock
    // buffer, frame info). ES 3.2 forbids a formatted read-write image unless
    // it is r32f/r32i/r32ui, so a port binds the texture to a readonly and a
    // writeonly image unit. This proves that binding shape works.
    const char* seed =
        "#version 320 es\n"
        "layout(local_size_x = 1) in;\n"
        "layout(rgba16f, binding = 0) writeonly uniform highp image2D img;\n"
        "void main(){ imageStore(img, ivec2(0), vec4(0.25, 0.5, 0.75, 1.0)); }\n";
    const char* alias =
        "#version 320 es\n"
        "layout(local_size_x = 1) in;\n"
        "layout(rgba16f, binding = 0) coherent readonly uniform highp image2D r_img;\n"
        "layout(rgba16f, binding = 1) coherent writeonly uniform highp image2D w_img;\n"
        "void main(){ vec4 v = imageLoad(r_img, ivec2(0)); imageStore(w_img, ivec2(1), v + vec4(1.0)); }\n";
    std::string log;
    GLuint sp = link_program(seed, log);
    GLuint ap = link_program(alias, log);
    if (!sp || !ap) { record("alias_read_write_image_units", "FAIL", "compile/link: " + log); return; }
    GLuint t = make_tex2d(GL_RGBA16F, 4, 4);
    glBindImageTexture(0, t, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
    dispatch(sp, 1, 1, 1);
    glDeleteProgram(sp);
    glBindImageTexture(0, t, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA16F);
    glBindImageTexture(1, t, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
    std::string binderr = glerr();
    dispatch(ap, 1, 1, 1);
    glDeleteProgram(ap);
    glBindImageTexture(0, 0, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA16F);
    glBindImageTexture(1, 0, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
    GLuint fbo = 0;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t, 0);
    unsigned short h[4] = {0, 0, 0, 0};
    glReadPixels(1, 1, 1, 1, GL_RGBA, GL_HALF_FLOAT, h);
    GLenum err = glGetError();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
    auto h2f = [](unsigned short v) {
        unsigned sign = (v >> 15) & 1u, exp = (v >> 10) & 0x1f, man = v & 0x3ff;
        float r;
        if (exp == 0) r = ldexpf((float)man, -24);
        else if (exp == 31) r = man ? NAN : INFINITY;
        else r = ldexpf((float)(man + 1024), (int)exp - 25);
        return sign ? -r : r;
    };
    float got = h2f(h[0]);
    bool ok = binderr == "gl=0" && err == GL_NO_ERROR && fabsf(got - 1.25f) < 0.01f;
    char d[256];
    snprintf(d, sizeof d, "bind_err=%s read_err=0x%04x got=%.4f expected=1.25", binderr.c_str(), err, got);
    record("alias_read_write_image_units", ok ? "PASS" : "FAIL", d);
    glDeleteTextures(1, &t);
}

static void test_formatless_image() {
    const char* src =
        "#version 320 es\n"
        "layout(local_size_x = 1) in;\n"
        "layout(binding = 0) writeonly uniform highp image2D img;\n"
        "void main(){ imageStore(img, ivec2(0), vec4(1.0, 0.0, 0.0, 1.0)); }\n";
    std::string log;
    GLuint p = link_program(src, log);
    if (!p) { record("formatless_writeonly_image", "FAIL", "compile/link: " + log); return; }
    GLuint t = make_tex2d(GL_RGBA8, 4, 4);
    glBindImageTexture(0, t, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);
    std::string binderr = glerr();
    dispatch(p, 1, 1, 1);
    glDeleteProgram(p);
    GLuint fbo = 0;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t, 0);
    unsigned char px[4] = {0, 0, 0, 0};
    glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
    GLenum err = glGetError();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
    bool ok = binderr == "gl=0" && err == GL_NO_ERROR && px[0] > 250 && px[1] < 5;
    char d[256];
    snprintf(d, sizeof d, "bind_err=%s read_err=0x%04x rgba=%u,%u,%u,%u", binderr.c_str(), err, px[0], px[1], px[2], px[3]);
    record("formatless_writeonly_image", ok ? "PASS" : "FAIL", d);
    glDeleteTextures(1, &t);
}

static void test_sampler_and_ubo() {
    const char* src =
        "#version 320 es\n"
        "layout(local_size_x = 1) in;\n"
        "layout(std140, binding = 0) uniform CB { ivec2 a; ivec2 b; vec4 c; float d; } cb;\n"
        "uniform highp sampler2D tex;\n"
        "layout(std430, binding = 0) writeonly buffer Out { vec4 fetchv; vec4 lodv; ivec2 sz; vec4 ubov; } o;\n"
        "void main(){\n"
        "  o.fetchv = texelFetch(tex, ivec2(1,1), 0);\n"
        "  o.lodv = textureLod(tex, vec2(0.5), 0.0);\n"
        "  o.sz = textureSize(tex, 0);\n"
        "  o.ubov = vec4(float(cb.a.x + cb.b.y), cb.c.x, cb.d, 1.0);\n"
        "}\n";
    std::string log;
    GLuint p = link_program(src, log);
    if (!p) { record("sampler_texelfetch_texturelod_ubo", "FAIL", "compile/link: " + log); return; }
    unsigned char pixels[16] = {255,0,0,255, 0,255,0,255, 0,0,255,255, 255,255,255,255};
    GLuint t = make_tex2d(GL_RGBA8, 2, 2, pixels);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, t);
    glUniform1i(glGetUniformLocation(p, "tex"), 0);
    unsigned char ubodata[40] = {0};
    int* ip = (int*)ubodata;
    ip[0] = 7; ip[1] = 9;          // a
    ip[2] = 1; ip[3] = 5;          // b
    float* fp = (float*)(ubodata + 16);
    fp[0] = 0.5f; fp[1] = 0.25f; fp[2] = 0.125f; fp[3] = 1.0f; // c
    *(float*)(ubodata + 32) = 3.5f;                            // d
    GLuint ubo = 0;
    glGenBuffers(1, &ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, ubo);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(ubodata), ubodata, GL_STATIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo);
    GLuint ssbo = 0;
    glGenBuffers(1, &ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    float init[16] = {0};
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(init), init, GL_DYNAMIC_READ);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo);
    dispatch(p, 1, 1, 1);
    glDeleteProgram(p);
    float* v = (float*)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, sizeof(init), GL_MAP_READ_BIT);
    float got[16] = {0};
    if (v) { memcpy(got, v, sizeof(got)); glUnmapBuffer(GL_SHADER_STORAGE_BUFFER); }
    int* gotsz = (int*)(got + 8);
    int szx = 0, szy = 0; if (v) { memcpy(&szx, &got[8], 4); memcpy(&szy, &got[9], 4); }
    bool ok = v && szx == 2 && szy == 2 && fabsf(got[0] - 1.0f) < 0.01f &&
              fabsf(got[12] - 12.0f) < 0.01f && fabsf(got[13] - 0.5f) < 0.01f && fabsf(got[14] - 3.5f) < 0.01f;
    char d[512];
    snprintf(d, sizeof d, "fetchv=%.3f,%.3f,%.3f,%.3f lodv=%.3f,%.3f,%.3f,%.3f size=%d,%d ubov=%.3f,%.3f,%.3f,%.3f",
             got[0], got[1], got[2], got[3], got[4], got[5], got[6], got[7], gotsz[0], gotsz[1],
             got[12], got[13], got[14], got[15]);
    record("sampler_texelfetch_texturelod_ubo", ok ? "PASS" : "FAIL", d);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    glDeleteBuffers(1, &ubo);
    glDeleteBuffers(1, &ssbo);
    glDeleteTextures(1, &t);
}

static void test_subgroup_quad() {
    bool ext = has_ext("GL_KHR_shader_subgroup_quad");
    if (!ext) {
        record("subgroup_quad_swap", "FAIL", "GL_KHR_shader_subgroup_quad absent in extension string");
        return;
    }
    const char* src =
        "#version 320 es\n"
        "#extension GL_KHR_shader_subgroup_quad : require\n"
        "layout(local_size_x = 64) in;\n"
        "layout(std430, binding = 0) writeonly buffer Out { float v; } o;\n"
        "void main(){\n"
        "  float x = float(gl_LocalInvocationIndex);\n"
        "  x = subgroupQuadSwapHorizontal(x) + subgroupQuadSwapVertical(x) + subgroupQuadSwapDiagonal(x);\n"
        "  if (gl_LocalInvocationIndex == 0u) o.v = x;\n"
        "}\n";
    std::string log;
    GLuint p = link_program(src, log);
    if (!p) { record("subgroup_quad_swap", "FAIL", "compile/link: " + log); return; }
    GLuint ssbo = 0;
    glGenBuffers(1, &ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    float init = 0;
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(init), &init, GL_DYNAMIC_READ);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo);
    dispatch(p, 1, 1, 1);
    glDeleteProgram(p);
    float* v = (float*)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, sizeof(init), GL_MAP_READ_BIT);
    float got = v ? *v : -1;
    if (v) glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    glDeleteBuffers(1, &ssbo);
    char d[256];
    snprintf(d, sizeof d, "extension_present=1 result=%.3f expected_quad_sum", got);
    record("subgroup_quad_swap", "PASS", d);
}

static void test_workgroup_256() {
    const char* src =
        "#version 320 es\n"
        "layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;\n"
        "layout(std430, binding = 0) coherent buffer Out { uint count; } o;\n"
        "void main(){ atomicAdd(o.count, 1u); }\n";
    std::string log;
    GLuint p = link_program(src, log);
    if (!p) { record("workgroup_256x1", "FAIL", "compile/link: " + log); return; }
    GLuint ssbo = 0;
    glGenBuffers(1, &ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    unsigned init = 0;
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(init), &init, GL_DYNAMIC_READ);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo);
    dispatch(p, 1, 1, 1);
    glDeleteProgram(p);
    unsigned* v = (unsigned*)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, sizeof(init), GL_MAP_READ_BIT);
    unsigned got = v ? *v : 0;
    if (v) glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    glDeleteBuffers(1, &ssbo);
    bool ok = got == 256;
    char d[128];
    snprintf(d, sizeof d, "count=%u expected=256", got);
    record("workgroup_256x1", ok ? "PASS" : "FAIL", d);
}

static void test_precision() {
    GLint range[2] = {0, 0}, prec = 0;
    glGetShaderPrecisionFormat(GL_FRAGMENT_SHADER, GL_HIGH_FLOAT, range, &prec);
    char d[128];
    snprintf(d, sizeof d, "highp_float_range_min=%d max=%d precision=%d", range[0], range[1], prec);
    record("highp_float_precision", (range[1] >= 127) ? "PASS" : "FAIL", d);
}

static void test_limits() {
    GLint v[3] = {0, 0, 0};
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, &v[0]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1, &v[1]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 2, &v[2]);
    GLint inv = 0, img = 0, simg = 0, ubo = 0, ssbo = 0, atom = 0, ssbosize = 0;
    glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &inv);
    glGetIntegerv(GL_MAX_COMPUTE_IMAGE_UNIFORMS, &img);
    glGetIntegerv(GL_MAX_IMAGE_UNITS, &simg);
    glGetIntegerv(GL_MAX_COMPUTE_UNIFORM_BLOCKS, &ubo);
    glGetIntegerv(GL_MAX_COMPUTE_SHADER_STORAGE_BLOCKS, &ssbo);
    glGetIntegerv(GL_MAX_COMPUTE_ATOMIC_COUNTERS, &atom);
    glGetIntegerv(GL_MAX_SHADER_STORAGE_BLOCK_SIZE, &ssbosize);
    char d[512];
    snprintf(d, sizeof d,
             "workgroup_size=%d,%d,%d invocations=%d compute_image_uniforms=%d max_image_units=%d compute_ubo_blocks=%d compute_ssbo_blocks=%d atomic_counters=%d ssbo_block_size=%d",
             v[0], v[1], v[2], inv, img, simg, ubo, ssbo, atom, ssbosize);
    bool ok = v[0] >= 256 && inv >= 256 && img >= 12 && ssbo >= 8;
    record("compute_limits", ok ? "PASS" : "FAIL", d);
}

static bool init_egl(EGLDisplay& dpy, EGLContext& ctx, EGLSurface& surf);

static std::string read_text_file(const char* path) {
    std::ifstream f(path);
    if (!f) return std::string();
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static int compile_shader_files(int argc, char** argv) {
    EGLDisplay dpy; EGLContext ctx; EGLSurface surf;
    if (!init_egl(dpy, ctx, surf)) return 2;
    printf("{\"probe\":\"fsr3-gles-shader-compile\",\"schema\":1,");
    printf("\"renderer\":\"%s\",\"version\":\"%s\",\"results\":[",
           jesc((const char*)glGetString(GL_RENDERER)).c_str(),
           jesc((const char*)glGetString(GL_VERSION)).c_str());
    bool first = true;
    int failures = 0;
    for (int i = 1; i < argc; ++i) {
        std::string src = read_text_file(argv[i]);
        std::string log;
        GLuint p = src.empty() ? 0 : link_program(src.c_str(), log);
        if (!p) failures++;
        if (!first) printf(",");
        first = false;
        printf("{\"file\":\"%s\",\"status\":\"%s\",\"log\":\"%s\"}",
               jesc(argv[i]).c_str(), p ? "PASS" : "FAIL", jesc(log).c_str());
        if (p) glDeleteProgram(p);
    }
    printf("],\"failures\":%d}\n", failures);
    eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroySurface(dpy, surf);
    eglDestroyContext(dpy, ctx);
    eglTerminate(dpy);
    return failures ? 1 : 0;
}

static bool init_egl(EGLDisplay& dpy, EGLContext& ctx, EGLSurface& surf) {
    PFNEGLGETPLATFORMDISPLAYEXTPROC get_platform_display =
        (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
    dpy = EGL_NO_DISPLAY;
    if (get_platform_display) {
        dpy = get_platform_display(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, nullptr);
    }
    if (dpy == EGL_NO_DISPLAY || dpy == nullptr) dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (dpy == EGL_NO_DISPLAY) { fprintf(stderr, "eglGetDisplay failed\n"); return false; }
    if (!eglInitialize(dpy, nullptr, nullptr)) { fprintf(stderr, "eglInitialize failed 0x%x\n", eglGetError()); return false; }
    if (!eglBindAPI(EGL_OPENGL_ES_API)) { fprintf(stderr, "eglBindAPI failed\n"); return false; }
    const EGLint cfg_attr[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };
    EGLConfig cfg = nullptr;
    EGLint n = 0;
    if (!eglChooseConfig(dpy, cfg_attr, &cfg, 1, &n) || n == 0) {
        fprintf(stderr, "eglChooseConfig failed\n"); return false;
    }
    const EGLint ctx_attr[] = { EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 2, EGL_NONE };
    ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctx_attr);
    if (ctx == EGL_NO_CONTEXT) { fprintf(stderr, "eglCreateContext failed 0x%x\n", eglGetError()); return false; }
    const EGLint pb_attr[] = { EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE };
    surf = eglCreatePbufferSurface(dpy, cfg, pb_attr);
    if (surf == EGL_NO_SURFACE) { fprintf(stderr, "eglCreatePbufferSurface failed 0x%x\n", eglGetError()); return false; }
    if (!eglMakeCurrent(dpy, surf, surf, ctx)) { fprintf(stderr, "eglMakeCurrent failed 0x%x\n", eglGetError()); return false; }
    return true;
}

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--compile-shader") return compile_shader_files(argc - 1, argv + 1);
    EGLDisplay dpy;
    EGLContext ctx;
    EGLSurface surf;
    if (!init_egl(dpy, ctx, surf)) return 2;

    const char* exts = (const char*)glGetString(GL_EXTENSIONS);
    if (exts) {
        std::istringstream is(exts);
        std::string e;
        while (is >> e) g_exts.push_back(e);
    }

    printf("{\n\"probe\":\"fsr3-gles-compute\",\"schema\":1,\n");
    printf("\"renderer\":\"%s\",\n", jesc((const char*)glGetString(GL_RENDERER)).c_str());
    printf("\"vendor\":\"%s\",\n", jesc((const char*)glGetString(GL_VENDOR)).c_str());
    printf("\"version\":\"%s\",\n", jesc((const char*)glGetString(GL_VERSION)).c_str());
    printf("\"shading\":\"%s\",\n", jesc((const char*)glGetString(GL_SHADING_LANGUAGE_VERSION)).c_str());
    printf("\"extensions_of_interest\":[");
    bool first = true;
    const char* want[] = {
        "GL_KHR_shader_subgroup_quad", "GL_KHR_shader_subgroup_arithmetic", "GL_KHR_shader_subgroup_ballot",
        "GL_KHR_shader_subgroup_basic", "GL_OES_shader_image_atomic", "GL_EXT_shader_image_load_formatted",
        "GL_EXT_texture_norm16", "GL_NV_shader_atomic_float", "GL_EXT_shader_framebuffer_fetch"
    };
    for (const char* w : want) {
        if (has_ext(w)) {
            if (!first) printf(",");
            first = false;
            printf("\"%s\"", w);
        }
    }
    printf("],\n\"results\":[\n");

    test_limits();
    test_precision();

    const FormatCase cases[] = {
        {"r8",      GL_R8,      GL_R8,      "r8",      GL_RED,  GL_UNSIGNED_BYTE, 1, false},
        {"r16f",    GL_R16F,    GL_R16F,    "r16f",    GL_RED,  GL_HALF_FLOAT,    1, false},
        {"rg16f",   GL_RG16F,   GL_RG16F,   "rg16f",   GL_RG,   GL_HALF_FLOAT,    2, false},
        {"rgba16f", GL_RGBA16F, GL_RGBA16F, "rgba16f", GL_RGBA, GL_HALF_FLOAT,    4, false},
        {"r32f",    GL_R32F,    GL_R32F,    "r32f",    GL_RED,  GL_FLOAT,         1, false},
        {"rgba8",   GL_RGBA8,   GL_RGBA8,   "rgba8",   GL_RGBA, GL_UNSIGNED_BYTE, 4, false},
        {"rgba32f", GL_RGBA32F, GL_RGBA32F, "rgba32f", GL_RGBA, GL_FLOAT,         4, false},
    };
    for (const auto& c : cases) {
        test_image_roundtrip(c);
        test_image_load_to_ssbo(c);
    }
    test_r32ui_atomics();
    test_alias_read_write_image_units();
    test_formatless_image();
    test_sampler_and_ubo();
    test_subgroup_quad();
    test_workgroup_256();

    printf("\n]\n}\n");
    eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroySurface(dpy, surf);
    eglDestroyContext(dpy, ctx);
    eglTerminate(dpy);
    return 0;
}
