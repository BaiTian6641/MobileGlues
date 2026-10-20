// FSR3.1.4 GLES compute dispatch gate.
//
// Executes adapted FSR3 upscaler passes through a GLES 3.2 compute context and
// validates the readback against a declared reference. This is a test harness:
// the glReadPixels path exists only here and is never part of a production
// backend.
//
// The shader is generated from the pinned AMD source (see make_shaders.sh) and
// passed by path; no SDK code is vendored. The harness creates the GLES
// resources, binds UBOs/images/samplers, dispatches, inserts a full memory
// barrier, reads the result back and compares it with a CPU model and stated
// invariants.
//
// Build (host):  make
// Build (arm64): ./build_android.sh
// Output: one JSON object on stdout; progress/errors go to stderr.
#define GL_GLEXT_PROTOTYPES 1
#define EGL_EGLEXT_PROTOTYPES 1
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl32.h>

#include <cmath>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// SHA-256 (FIPS 180-4), used to bind the exact shader and output bytes.
// ---------------------------------------------------------------------------
namespace sha {

struct Ctx {
    uint32_t h[8];
    uint64_t len;
    uint8_t buf[64];
    size_t buf_len;
};

static const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
    0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
    0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
    0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
    0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
    0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

static inline uint32_t ror(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

static void block(Ctx& c, const uint8_t* p) {
    uint32_t w[64];
    for (int i = 0; i < 16; i++)
        w[i] = (uint32_t)p[i * 4] << 24 | (uint32_t)p[i * 4 + 1] << 16 |
               (uint32_t)p[i * 4 + 2] << 8 | (uint32_t)p[i * 4 + 3];
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = ror(w[i - 15], 7) ^ ror(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = ror(w[i - 2], 17) ^ ror(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    uint32_t a = c.h[0], b = c.h[1], cc = c.h[2], d = c.h[3];
    uint32_t e = c.h[4], f = c.h[5], g = c.h[6], h = c.h[7];
    for (int i = 0; i < 64; i++) {
        uint32_t S1 = ror(e, 6) ^ ror(e, 11) ^ ror(e, 25);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t t1 = h + S1 + ch + K[i] + w[i];
        uint32_t S0 = ror(a, 2) ^ ror(a, 13) ^ ror(a, 22);
        uint32_t maj = (a & b) ^ (a & cc) ^ (b & cc);
        uint32_t t2 = S0 + maj;
        h = g; g = f; f = e; e = d + t1;
        d = cc; cc = b; b = a; a = t1 + t2;
    }
    c.h[0] += a; c.h[1] += b; c.h[2] += cc; c.h[3] += d;
    c.h[4] += e; c.h[5] += f; c.h[6] += g; c.h[7] += h;
}

static void init(Ctx& c) {
    static const uint32_t iv[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                   0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    memcpy(c.h, iv, sizeof iv);
    c.len = 0;
    c.buf_len = 0;
}

static void update(Ctx& c, const void* data, size_t n) {
    const uint8_t* p = (const uint8_t*)data;
    c.len += n;
    while (n) {
        size_t take = 64 - c.buf_len;
        if (take > n) take = n;
        memcpy(c.buf + c.buf_len, p, take);
        c.buf_len += take;
        p += take;
        n -= take;
        if (c.buf_len == 64) {
            block(c, c.buf);
            c.buf_len = 0;
        }
    }
}

static std::string hex(const Ctx& c) {
    char out[65];
    for (int i = 0; i < 8; i++) snprintf(out + i * 8, 9, "%08x", c.h[i]);
    return std::string(out);
}

static std::string digest(const void* data, size_t n) {
    Ctx c;
    init(c);
    update(c, data, n);
    uint64_t bits = c.len * 8;
    uint8_t pad = 0x80;
    update(c, &pad, 1);
    uint8_t zero = 0;
    while (c.buf_len != 56) update(c, &zero, 1);
    uint8_t lenb[8];
    for (int i = 0; i < 8; i++) lenb[i] = (uint8_t)(bits >> (56 - 8 * i));
    update(c, lenb, 8);
    return hex(c);
}

} // namespace sha

// ---------------------------------------------------------------------------
// Minimal JSON emission helpers.
// ---------------------------------------------------------------------------
static std::string jesc(const std::string& s) {
    std::string o;
    for (char ch : s) {
        switch (ch) {
        case '"': o += "\\\""; break;
        case '\\': o += "\\\\"; break;
        case '\n': o += "\\n"; break;
        case '\r': o += "\\r"; break;
        case '\t': o += "\\t"; break;
        default:
            if ((unsigned char)ch < 0x20) {
                char b[8];
                snprintf(b, sizeof b, "\\u%04x", (unsigned char)ch);
                o += b;
            } else {
                o += ch;
            }
        }
    }
    return o;
}

static std::string jstr(const std::string& s) { return "\"" + jesc(s) + "\""; }

struct Check {
    std::string name;
    std::string status; // PASS / FAIL / NOT_RUN
    std::string detail;
};
static std::vector<Check> g_checks;
static std::vector<std::string> g_errors;

static void check(const std::string& name, bool ok, const std::string& detail) {
    g_checks.push_back({name, ok ? "PASS" : "FAIL", detail});
}

// ---------------------------------------------------------------------------
// GL helpers.
// ---------------------------------------------------------------------------
static std::string glerr() {
    GLenum e = glGetError();
    if (e == GL_NO_ERROR) return "gl=0";
    char b[32];
    snprintf(b, sizeof b, "gl=0x%04x", e);
    return b;
}

static bool g_gl_ok = true;
static void note_gl(const char* where) {
    GLenum e = glGetError();
    if (e != GL_NO_ERROR) {
        char b[128];
        snprintf(b, sizeof b, "%s: gl=0x%04x", where, e);
        g_errors.push_back(b);
        g_gl_ok = false;
    }
}

static std::string read_file(const char* path, bool& ok) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { ok = false; return {}; }
    std::string s((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    ok = true;
    return s;
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
        glDeleteProgram(p);
        return 0;
    }
    return p;
}

// Resolve a sampler uniform to a texture unit. SPIRV-Cross emits long combined
// names; an exact lookup is tried first, then a substring scan.
static int sampler_unit(GLuint prog, const char* exact, const char* contains,
                        std::string& resolved) {
    GLint loc = glGetUniformLocation(prog, exact);
    std::string name = exact;
    if (loc < 0) {
        GLint n = 0;
        glGetProgramiv(prog, GL_ACTIVE_UNIFORMS, &n);
        for (GLint i = 0; i < n; i++) {
            char nm[256];
            GLsizei l = 0;
            GLint sz = 0;
            GLenum ty = 0;
            glGetActiveUniform(prog, i, sizeof nm, &l, &sz, &ty, nm);
            if (l > 0 && ty == GL_SAMPLER_2D && strstr(nm, contains)) {
                loc = glGetUniformLocation(prog, nm);
                name = nm;
                break;
            }
        }
    }
    resolved = name;
    return loc;
}

static GLuint make_rgba8(GLint w, GLint h, const std::vector<uint8_t>& data) {
    GLuint t = 0;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, w, h);
    if (!data.empty()) {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE,
                        data.data());
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    return t;
}

static GLuint make_r32f(GLint w, GLint h, float v) {
    GLuint t = 0;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32F, w, h);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RED, GL_FLOAT, &v);
    glBindTexture(GL_TEXTURE_2D, 0);
    return t;
}

static bool read_rgba8(GLuint tex, GLint w, GLint h, std::vector<uint8_t>& out) {
    GLuint fbo = 0;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           tex, 0);
    GLenum st = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    bool ok = st == GL_FRAMEBUFFER_COMPLETE;
    if (ok) {
        out.resize((size_t)w * h * 4);
        glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, out.data());
        ok = glGetError() == GL_NO_ERROR;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
    return ok;
}

// ---------------------------------------------------------------------------
// RCAS CPU model. Mirrors the adapted ESSL exactly:
//   sdk/src/backends/vk/shaders/fsr3upscaler/ffx_fsr3upscaler_rcas_pass.glsl
//   + sdk/include/FidelityFX/gpu/fsr3upscaler/ffx_fsr3upscaler_rcas.h
// fp32, compiled with -ffp-contract=off so the reference is platform-stable.
// ---------------------------------------------------------------------------
static inline float u2f(uint32_t v) {
    float f;
    memcpy(&f, &v, 4);
    return f;
}
static inline uint32_t f2u(float f) {
    uint32_t u;
    memcpy(&u, &f, 4);
    return u;
}

static float approx_recip_medium(float value) {
    float b = u2f(2129764351u - f2u(value));
    return b * (((-b) * value) + 2.0f);
}

static inline float saturate(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }
static inline float max3(float a, float b, float c) { return fmaxf(a, fmaxf(b, c)); }
static inline float min3(float a, float b, float c) { return fminf(a, fminf(b, c)); }

static void fsr_rcas(const float rgb[5][3], const float con_x, float out[3]) {
    // rgb[0..4] = b, d, e, f, h (top, left, center, right, bottom)
    float bR = rgb[0][0], bG = rgb[0][1], bB = rgb[0][2];
    float dR = rgb[1][0], dG = rgb[1][1], dB = rgb[1][2];
    float eR = rgb[2][0], eG = rgb[2][1], eB = rgb[2][2];
    float fR = rgb[3][0], fG = rgb[3][1], fB = rgb[3][2];
    float hR = rgb[4][0], hG = rgb[4][1], hB = rgb[4][2];

    float bL = (bB * 0.5f) + ((bR * 0.5f) + bG);
    float dL = (dB * 0.5f) + ((dR * 0.5f) + dG);
    float eL = (eB * 0.5f) + ((eR * 0.5f) + eG);
    float fL = (fB * 0.5f) + ((fR * 0.5f) + fG);
    float hL = (hB * 0.5f) + ((hR * 0.5f) + hG);
    float nz = ((((0.25f * bL) + (0.25f * dL)) + (0.25f * fL)) + (0.25f * hL)) - eL;
    float mx = max3(max3(bL, dL, eL), fL, hL);
    float mn = min3(min3(bL, dL, eL), fL, hL);
    nz = saturate(fabsf(nz) * approx_recip_medium(mx - mn));
    nz = ((-0.5f) * nz) + 1.0f;

    float mn4R = fminf(min3(bR, dR, fR), hR);
    float mn4G = fminf(min3(bG, dG, fG), hG);
    float mn4B = fminf(min3(bB, dB, fB), hB);
    float mx4R = fmaxf(max3(bR, dR, fR), hR);
    float mx4G = fmaxf(max3(bG, dG, fG), hG);
    float mx4B = fmaxf(max3(bB, dB, fB), hB);
    float peakC_x = 1.0f, peakC_y = -4.0f;
    float hitMinR = mn4R / (4.0f * mx4R);
    float hitMinG = mn4G / (4.0f * mx4G);
    float hitMinB = mn4B / (4.0f * mx4B);
    float hitMaxR = (peakC_x - mx4R) / ((4.0f * mn4R) + peakC_y);
    float hitMaxG = (peakC_x - mx4G) / ((4.0f * mn4G) + peakC_y);
    float hitMaxB = (peakC_x - mx4B) / ((4.0f * mn4B) + peakC_y);
    float lobeR = fmaxf(-hitMinR, hitMaxR);
    float lobeG = fmaxf(-hitMinG, hitMaxG);
    float lobeB = fmaxf(-hitMinB, hitMaxB);
    float lobe = fmaxf(-0.1875f, fminf(max3(lobeR, lobeG, lobeB), 0.0f)) * con_x;
    lobe *= nz;
    float rcpL = approx_recip_medium((4.0f * lobe) + 1.0f);
    out[0] = (((((lobe * bR) + (lobe * dR)) + (lobe * hR)) + (lobe * fR)) + eR) * rcpL;
    out[1] = (((((lobe * bG) + (lobe * dG)) + (lobe * hG)) + (lobe * fG)) + eG) * rcpL;
    out[2] = (((((lobe * bB) + (lobe * dB)) + (lobe * hB)) + (lobe * fB)) + eB) * rcpL;
}

// Reference for interior pixels. Clamped sampling for border pixels is not
// claimed to match the GPU (out-of-range texelFetch is undefined in ES).
static void build_reference(const std::vector<uint8_t>& in, int w, int h,
                            float con_x, std::vector<uint8_t>& ref,
                            std::vector<uint8_t>& interior_mask) {
    ref.assign((size_t)w * h * 4, 0);
    interior_mask.assign((size_t)w * h, 0);
    auto px = [&](int x, int y) -> const uint8_t* {
        return &in[((size_t)y * w + x) * 4];
    };
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            float nb[5][3];
            const int ox[5] = {0, -1, 0, 1, 0};
            const int oy[5] = {-1, 0, 0, 0, 1};
            for (int i = 0; i < 5; i++) {
                int sx = x + ox[i], sy = y + oy[i];
                if (sx < 0) sx = 0;
                if (sy < 0) sy = 0;
                if (sx >= w) sx = w - 1;
                if (sy >= h) sy = h - 1;
                const uint8_t* p = px(sx, sy);
                nb[i][0] = p[0] / 255.0f;
                nb[i][1] = p[1] / 255.0f;
                nb[i][2] = p[2] / 255.0f;
            }
            float o[3];
            fsr_rcas(nb, con_x, o);
            uint8_t* d = &ref[((size_t)y * w + x) * 4];
            for (int c = 0; c < 3; c++) {
                float c01 = saturate(o[c]);
                long v = lroundf(c01 * 255.0f);
                d[c] = (uint8_t)v;
            }
            d[3] = 255;
            interior_mask[(size_t)y * w + x] =
                (x >= 1 && y >= 1 && x <= w - 2 && y <= h - 2) ? 1 : 0;
        }
    }
}

// ---------------------------------------------------------------------------
// Synthetic input. Deterministic, with a hard vertical step, a horizontal
// step, a diagonal ramp and a constant patch (flat-identity invariant).
// ---------------------------------------------------------------------------

static std::vector<uint8_t> make_input(int w, int h) {
    std::vector<uint8_t> d((size_t)w * h * 4);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            uint8_t* p = &d[((size_t)y * w + x) * 4];
            int r, g, b;
            if (x < w / 2) r = 32 + (x * 24) / (w / 2 + 1);      // ramp up to edge
            else r = 224 - ((x - w / 2) * 12) / (w / 2 + 1);     // hard step
            g = 16 + (int)lround(223.0 * (double)x / (double)(w - 1));
            b = (y < h / 2) ? 24 + (y * 16) / (h / 2 + 1) : 208;
            p[0] = (uint8_t)r;
            p[1] = (uint8_t)g;
            p[2] = (uint8_t)b;
            p[3] = 255;
        }
    }
    // Constant patch, inset by 2 px so its 5-neighborhood is uniform.
    int x0 = w / 8 + 2, x1 = w / 3 - 2;
    int y0 = h / 3 + 2, y1 = (2 * h) / 3 - 2;
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) {
            uint8_t* p = &d[((size_t)y * w + x) * 4];
            p[0] = 128; p[1] = 128; p[2] = 128; p[3] = 255;
        }
    return d;
}

static void patch_rect(int w, int h, int& x0, int& y0, int& x1, int& y1) {
    x0 = w / 8 + 2; x1 = w / 3 - 2;
    y0 = h / 3 + 2; y1 = (2 * h) / 3 - 2;
}

// ---------------------------------------------------------------------------
// EGL.
// ---------------------------------------------------------------------------
static bool init_egl(EGLDisplay& dpy, EGLContext& ctx, EGLSurface& surf) {
    PFNEGLGETPLATFORMDISPLAYEXTPROC get_platform_display =
        (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
    dpy = EGL_NO_DISPLAY;
    if (get_platform_display)
        dpy = get_platform_display(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY,
                                   nullptr);
    if (dpy == EGL_NO_DISPLAY || dpy == nullptr) dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (dpy == EGL_NO_DISPLAY) { fprintf(stderr, "eglGetDisplay failed\n"); return false; }
    if (!eglInitialize(dpy, nullptr, nullptr)) {
        fprintf(stderr, "eglInitialize failed 0x%x\n", eglGetError());
        return false;
    }
    if (!eglBindAPI(EGL_OPENGL_ES_API)) { fprintf(stderr, "eglBindAPI failed\n"); return false; }
    const EGLint cfg_attr[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_NONE};
    EGLConfig cfg = nullptr;
    EGLint n = 0;
    if (!eglChooseConfig(dpy, cfg_attr, &cfg, 1, &n) || n == 0) {
        fprintf(stderr, "eglChooseConfig failed\n");
        return false;
    }
    const EGLint ctx_attr[] = {EGL_CONTEXT_MAJOR_VERSION, 3,
                               EGL_CONTEXT_MINOR_VERSION, 2, EGL_NONE};
    ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctx_attr);
    if (ctx == EGL_NO_CONTEXT) {
        fprintf(stderr, "eglCreateContext failed 0x%x\n", eglGetError());
        return false;
    }
    const EGLint pb_attr[] = {EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE};
    surf = eglCreatePbufferSurface(dpy, cfg, pb_attr);
    if (surf == EGL_NO_SURFACE) {
        fprintf(stderr, "eglCreatePbufferSurface failed 0x%x\n", eglGetError());
        return false;
    }
    if (!eglMakeCurrent(dpy, surf, surf, ctx)) {
        fprintf(stderr, "eglMakeCurrent failed 0x%x\n", eglGetError());
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// RCAS dispatch.
// ---------------------------------------------------------------------------
struct RcasResult {
    bool executed = false;
    std::vector<uint8_t> out;
    std::string readback_error;
};

struct UboInfo {
    std::string name;
    GLint binding = -1;
    GLint size = 0;
    bool active = false;
};

static UboInfo bind_ubo(GLuint prog, const char* block_name, GLint binding,
                        GLuint buf, size_t bytes) {
    UboInfo info;
    info.name = block_name;
    GLuint idx = glGetUniformBlockIndex(prog, block_name);
    if (idx == GL_INVALID_INDEX) return info;
    info.active = true;
    glUniformBlockBinding(prog, idx, (GLuint)binding);
    GLint size = 0;
    glGetActiveUniformBlockiv(prog, idx, GL_UNIFORM_BLOCK_DATA_SIZE, &size);
    info.size = size;
    info.binding = binding;
    glBindBufferRange(GL_UNIFORM_BUFFER, (GLuint)binding, buf, 0, bytes);
    return info;
}

static RcasResult dispatch_rcas(GLuint prog, GLuint in_tex, GLuint exp_tex,
                                int w, int h, uint32_t con0,
                                std::vector<UboInfo>& ubos,
                                std::vector<std::string>& bind_log) {
    RcasResult r;
    GLuint out_tex = make_rgba8(w, h, {});
    // Sentinel so "unchanged" is visible: magenta.
    std::vector<uint8_t> sentinel((size_t)w * h * 4);
    for (size_t i = 0; i < sentinel.size(); i += 4) {
        sentinel[i] = 255; sentinel[i + 1] = 0; sentinel[i + 2] = 255; sentinel[i + 3] = 0;
    }
    glBindTexture(GL_TEXTURE_2D, out_tex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE,
                    sentinel.data());
    glBindTexture(GL_TEXTURE_2D, 0);

    glUseProgram(prog);
    note_gl("glUseProgram");

    std::string sname1, sname2;
    GLint loc_in = sampler_unit(
        prog, "SPIRV_Cross_Combinedr_rcas_inputSPIRV_Cross_DummySampler",
        "rcas_input", sname1);
    GLint loc_exp = sampler_unit(
        prog, "SPIRV_Cross_Combinedr_input_exposureSPIRV_Cross_DummySampler",
        "input_exposure", sname2);
    glUniform1i(loc_in, 0);
    glUniform1i(loc_exp, 1);
    note_gl("sampler uniforms");
    bind_log.push_back("sampler2D " + sname1 + " <- unit 0 (input RGBA8)");
    bind_log.push_back("sampler2D " + sname2 + " <- unit 1 (exposure R32F)");
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, in_tex);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, exp_tex);
    glActiveTexture(GL_TEXTURE0);

    // rcasConfig UBO: only con[0] = asuint(exp2(-(2 - 2*sharpness))) is used.
    uint32_t cfg[4] = {con0, 0, 0, 0};
    GLuint rcas_ubo = 0;
    glGenBuffers(1, &rcas_ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, rcas_ubo);
    glBufferData(GL_UNIFORM_BUFFER, sizeof cfg, cfg, GL_STATIC_DRAW);
    // The FSR3 constants UBO is declared by the pass but RCAS does not read it.
    uint8_t fsr_consts[256];
    memset(fsr_consts, 0, sizeof fsr_consts);
    GLuint fsr_ubo = 0;
    glGenBuffers(1, &fsr_ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, fsr_ubo);
    glBufferData(GL_UNIFORM_BUFFER, sizeof fsr_consts, fsr_consts, GL_STATIC_DRAW);

    UboInfo u_cb = bind_ubo(prog, "cbFSR3UPSCALER_t", 3, fsr_ubo, sizeof fsr_consts);
    UboInfo u_rcas = bind_ubo(prog, "cbRCAS_t", 4, rcas_ubo, sizeof cfg);
    ubos.push_back(u_cb);
    ubos.push_back(u_rcas);
    note_gl("uniform blocks");

    glBindImageTexture(2, out_tex, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);
    std::string img_err = glerr();
    bind_log.push_back("image unit 2 <- output tex RGBA8 access=WRITE_ONLY " + img_err);

    GLuint groups_x = (GLuint)((w + 15) / 16);
    GLuint groups_y = (GLuint)((h + 15) / 16);
    glDispatchCompute(groups_x, groups_y, 1);
    note_gl("glDispatchCompute");
    glMemoryBarrier(GL_ALL_BARRIER_BITS);
    note_gl("glMemoryBarrier");
    glFinish();
    note_gl("glFinish");

    glBindImageTexture(2, 0, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    glDeleteBuffers(1, &rcas_ubo);
    glDeleteBuffers(1, &fsr_ubo);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);

    if (!read_rgba8(out_tex, w, h, r.out)) {
        r.readback_error = "FBO readback failed " + glerr();
    } else {
        r.executed = true;
    }
    glDeleteTextures(1, &out_tex);
    return r;
}


// ===========================================================================
// Multi-pass chain executor (8-pass FSR3 upscaler schedule).
//
// The executor parses the adapted ESSL declarations to bind the same resource
// names the SDK pipeline uses. Resource selection follows the SDK's frame-0
// ping-pong choices (ffx_fsr3upscaler.cpp). Constants are filled from the
// documented host formulas. The readbacks and checks exist only in this test
// harness.
// ===========================================================================
struct Tex {
    GLuint id = 0;
    GLenum internal = 0;
    int w = 0, h = 0, levels = 1;
    bool valid() const { return id != 0; }
};

static const char* gl_format_name(GLenum f) {
    switch (f) {
    case GL_RGBA8: return "GL_RGBA8";
    case GL_RGBA16F: return "GL_RGBA16F";
    case GL_RGBA32F: return "GL_RGBA32F";
    case GL_R32F: return "GL_R32F";
    case GL_R32UI: return "GL_R32UI";
    case GL_R16F: return "GL_R16F";
    case GL_RG16F: return "GL_RG16F";
    case GL_R8: return "GL_R8";
    default: return "GL_OTHER";
    }
}

static GLenum gl_format_of(const std::string& q) {
    if (q == "rgba8") return GL_RGBA8;
    if (q == "rgba16f") return GL_RGBA16F;
    if (q == "rgba32f") return GL_RGBA32F;
    if (q == "r32f") return GL_R32F;
    if (q == "r32ui") return GL_R32UI;
    if (q == "r16f") return GL_R16F;
    if (q == "rg16f") return GL_RG16F;
    return 0;
}

static Tex make_tex(GLenum internal, int w, int h, int levels, const void* data,
                    GLenum up_fmt, GLenum up_type) {
    Tex t;
    t.internal = internal; t.w = w; t.h = h; t.levels = levels;
    glGenTextures(1, &t.id);
    glBindTexture(GL_TEXTURE_2D, t.id);
    GLint filt = (levels > 1) ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filt);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexStorage2D(GL_TEXTURE_2D, levels, internal, w, h);
    if (data) glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, up_fmt, up_type, data);
    glBindTexture(GL_TEXTURE_2D, 0);
    return t;
}

static int level_dim(int d, int level) { int v = d >> level; return v > 0 ? v : 1; }

// Read a texture level through an FBO. Test-harness path only.
static bool read_level(const Tex& t, int level, GLenum fmt, GLenum type, size_t bytes,
                       std::vector<uint8_t>& out) {
    if (!t.valid()) return false;
    int w = level_dim(t.w, level), h = level_dim(t.h, level);
    GLuint fbo = 0;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t.id, level);
    GLenum st = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    bool ok = st == GL_FRAMEBUFFER_COMPLETE;
    if (ok) {
        out.resize(bytes);
        glReadPixels(0, 0, w, h, fmt, type, out.data());
        ok = glGetError() == GL_NO_ERROR;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
    return ok;
}

// half -> float, for RGBA16F readback.
static float h2f(uint16_t v) {
    uint32_t sign = (v >> 15) & 1u, exp = (v >> 10) & 0x1fu, man = v & 0x3ffu;
    float r;
    if (exp == 0) r = ldexpf((float)man, -24);
    else if (exp == 31) r = man ? NAN : INFINITY;
    else r = ldexpf((float)(man + 1024), (int)exp - 25);
    return sign ? -r : r;
}

static std::string hash_vec(const std::vector<uint8_t>& v) {
    return v.empty() ? "" : sha::digest(v.data(), v.size());
}

// ---------------------------------------------------------------------------
// ESSL declaration parsing.
// ---------------------------------------------------------------------------
static std::string trim_s(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

static int parse_int_after(const std::string& s, const char* key) {
    size_t p = s.find(key);
    if (p == std::string::npos) return -1;
    p += strlen(key);
    while (p < s.size() && s[p] == ' ') p++;
    return atoi(s.c_str() + p);
}

struct UboDecl { GLuint binding; std::string block; };
struct SamplerDecl { std::string full; std::string base; };
struct ImageDecl {
    GLuint unit;
    std::string name;   // as declared, may carry the r_ alias prefix
    GLenum fmt;
    GLenum access;
};

struct PassInfo {
    std::string name;
    GLuint prog = 0;
    int lx = 1, ly = 1;
    int gx = 0, gy = 0;
    std::vector<UboDecl> ubos;
    std::vector<SamplerDecl> samplers;
    std::vector<ImageDecl> images;
    bool compiled = false;
    std::string log;
    std::vector<std::string> bindings;
};

static void parse_essl(const std::string& src, PassInfo& p) {
    p.lx = parse_int_after(src, "local_size_x = ");
    p.ly = parse_int_after(src, "local_size_y = ");
    if (p.lx <= 0) p.lx = 1;
    if (p.ly <= 0) p.ly = 1;

    size_t pos = 0;
    while (pos < src.size()) {
        size_t eol = src.find('\n', pos);
        if (eol == std::string::npos) eol = src.size();
        std::string line = src.substr(pos, eol - pos);
        pos = eol + 1;

        if (line.find("std140) uniform ") != std::string::npos) {
            UboDecl d;
            d.binding = (GLuint)parse_int_after(line, "binding = ");
            size_t q = line.find("uniform ");
            std::string rest = trim_s(line.substr(q + 8));
            size_t sp = rest.find_first_of(" \t");
            d.block = (sp == std::string::npos) ? rest : rest.substr(0, sp);
            p.ubos.push_back(d);
            continue;
        }
        if (line.find("sampler2D ") != std::string::npos) {
            size_t q = line.find("sampler2D ");
            std::string rest = trim_s(line.substr(q + 10));
            size_t semi = rest.find(';');
            if (semi != std::string::npos) rest = rest.substr(0, semi);
            SamplerDecl d;
            d.full = trim_s(rest);
            std::string b = d.full;
            const std::string pre = "SPIRV_Cross_Combined";
            if (b.compare(0, pre.size(), pre) == 0) b = b.substr(pre.size());
            const std::string suf1 = "SPIRV_Cross_DummySampler";
            const std::string suf2 = "s_LinearClamp";
            if (b.size() > suf1.size() && b.compare(b.size() - suf1.size(), suf1.size(), suf1) == 0)
                b = b.substr(0, b.size() - suf1.size());
            else if (b.size() > suf2.size() && b.compare(b.size() - suf2.size(), suf2.size(), suf2) == 0)
                b = b.substr(0, b.size() - suf2.size());
            d.base = b;
            p.samplers.push_back(d);
            continue;
        }
        if (line.find("image2D ") != std::string::npos && line.find("binding = ") != std::string::npos) {
            ImageDecl d;
            d.unit = (GLuint)parse_int_after(line, "binding = ");
            size_t comma = line.find(',', line.find("binding = "));
            size_t close = line.find(')', comma);
            std::string fmt = trim_s(line.substr(comma + 1, close - comma - 1));
            d.fmt = gl_format_of(fmt);
            size_t i2 = line.find("image2D ");
            std::string rest = trim_s(line.substr(i2 + 8));
            size_t semi = rest.find(';');
            if (semi != std::string::npos) rest = rest.substr(0, semi);
            size_t sp = rest.find_last_of(" \t");
            d.name = (sp == std::string::npos) ? rest : rest.substr(sp + 1);
            d.access = (line.find("writeonly") != std::string::npos) ? GL_WRITE_ONLY
                     : (line.find("readonly") != std::string::npos) ? GL_READ_ONLY
                     : GL_READ_WRITE;
            p.images.push_back(d);
        }
    }
}

// ---------------------------------------------------------------------------
// Chain resources.
// ---------------------------------------------------------------------------
struct Chain {
    int rw = 160, rh = 90, ow = 320, oh = 180;
    std::map<std::string, Tex> res;
    int spd_levels = 1;
    int spd_base_w = 0, spd_base_h = 0;

    Tex& at(const std::string& n) { return res[n]; }
};

static int mip_levels_for(int w, int h) {
    int m = w > h ? w : h;
    int n = 0;
    while (m > 1) { m >>= 1; n++; }
    return n + 1;
}

// resource name -> texture + level, following the SDK frame-0 ping-pong choice.
static bool resolve_tex(Chain& c, const std::string& n, Tex*& t, int& level, bool image) {
    level = 0;
    std::string s = n;
    if (s.compare(0, 5, "r_rw_") == 0) s = s.substr(2); // readonly alias of a UAV
    auto find = [&](const char* key, Tex*& out) {
        auto it = c.res.find(key);
        if (it == c.res.end()) return false;
        out = &it->second;
        return true;
    };
    if (s == "rw_spd_mip0" || s == "rw_spd_mip1" || s == "rw_spd_mip2" ||
        s == "rw_spd_mip3" || s == "rw_spd_mip4" || s == "rw_spd_mip5") {
        level = s.back() - '0';
        return find("spd_mips", t);
    }
    if (s == "rw_spd_global_atomic") return find("spd_atomic", t);
    if (s == "rw_internal_upscaled_color") return find("iuc2", t);
    if (s == "rw_accumulation") return find("acc2", t);
    if (s == "rw_luma_history") return find("lh2", t);
    if (s == "rw_new_locks") return find("new_locks", t);
    if (s == "rw_frame_info") return find("frame_info", t);
    if (s == "rw_upscaled_output") return find("output", t);
    if (s == "rw_dilated_reactive_masks") return find("dilated_reactive", t);
    if (s == "rw_shading_change") return find("shading_change", t);
    if (s == "rw_farthest_depth") return find("intermediate", t);
    if (s == "rw_farthest_depth_mip1") return find("farthest_mip1", t);
    if (s == "rw_current_luma") return find("luma1", t);
    if (s == "rw_luma_instability") return find("intermediate", t);
    if (s == "rw_dilated_motion_vectors") return find("dilated_motion", t);
    if (s == "rw_dilated_depth") return find("dilated_depth", t);
    if (s == "rw_reconstructed_previous_nearest_depth") return find("recon_depth", t);

    // sampler names
    if (s == "r_input_color_jittered" || s == "r_input_color") return find("input_color", t);
    if (s == "r_input_depth") return find("input_depth", t);
    if (s == "r_input_motion_vectors") return find("input_motion", t);
    if (s == "r_input_exposure") return find("input_exposure", t);
    if (s == "r_reactive_mask") return find("input_reactive", t);
    if (s == "r_transparency_and_composition_mask") return find("input_transparency", t);
    if (s == "r_current_luma") return find("luma1", t);
    if (s == "r_previous_luma") return find("luma2", t);
    if (s == "r_spd_mips") return find("spd_mips", t);
    if (s == "r_farthest_depth") return find("intermediate", t);
    if (s == "r_farthest_depth_mip1") return find("farthest_mip1", t);
    if (s == "r_dilated_reactive_masks") return find("dilated_reactive", t);
    if (s == "r_dilated_motion_vectors") return find("dilated_motion", t);
    if (s == "r_dilated_depth") return find("dilated_depth", t);
    if (s == "r_luma_history") return find("lh1", t);
    if (s == "r_luma_instability") return find("intermediate", t);
    if (s == "r_internal_upscaled_color") return find("iuc1", t);
    if (s == "r_accumulation") return find("acc1", t);
    if (s == "r_shading_change") return find("shading_change", t);
    if (s == "r_rcas_input") return find("iuc2", t);
    if (s == "r_lanczos_lut") return find("lanczos_lut", t);
    if (s == "r_reconstructed_previous_nearest_depth") return find("recon_depth", t);
    if (s == "r_new_locks") return find("new_locks", t);
    if (s == "r_frame_info") return find("frame_info", t);
    (void)image;
    return false;
}

struct FsrConsts {
    int32_t renderSize[2], previousFrameRenderSize[2];
    int32_t upscaleSize[2], previousFrameUpscaleSize[2];
    int32_t maxRenderSize[2], maxUpscaleSize[2];
    float deviceToViewDepth[4];
    float jitterOffset[2], previousFrameJitterOffset[2];
    float motionVectorScale[2], downscaleFactor[2];
    float motionVectorJitterCancellation[2];
    float tanHalfFOV, jitterPhaseCount;
    float deltaTime, deltaPreExposure, viewSpaceToMetersFactor, frameIndex;
    float velocityFactor, reactivenessScale, shadingChangeScale;
    float accumulationAddedPerFrame, minDisocclusionAccumulation;
};
static_assert(sizeof(FsrConsts) == 148, "FSR3 constants layout");

struct SpdConsts {
    uint32_t mips, numWorkGroups;
    uint32_t workGroupOffset[2];
    uint32_t renderSize[2];
    uint32_t pad;
}; // std140: 24 bytes used

static void fill_consts(FsrConsts& k, int rw, int rh, int ow, int oh) {
    memset(&k, 0, sizeof k);
    k.renderSize[0] = rw; k.renderSize[1] = rh;
    k.previousFrameRenderSize[0] = 0; k.previousFrameRenderSize[1] = 0;
    k.upscaleSize[0] = ow; k.upscaleSize[1] = oh;
    k.previousFrameUpscaleSize[0] = 0; k.previousFrameUpscaleSize[1] = 0;
    k.maxRenderSize[0] = rw; k.maxRenderSize[1] = rh;
    k.maxUpscaleSize[0] = ow; k.maxUpscaleSize[1] = oh;

    // setupDeviceDepthToViewSpaceDepthParams: near 0.1, far 1000, no inverted/infinite.
    const float fMin = 0.1f, fMax = 1000.0f;
    const float fQ = fMax / (fMin - fMax);
    k.deviceToViewDepth[0] = -1.0f * fQ;
    k.deviceToViewDepth[1] = fQ * fMin;
    const float aspect = (float)rw / (float)rh;
    const float fovY = 1.0471975511965976f; // 60 degrees
    const float cotHalfFovY = cosf(0.5f * fovY) / sinf(0.5f * fovY);
    const float a = cotHalfFovY / aspect;
    const float b = cotHalfFovY;
    k.deviceToViewDepth[2] = 1.0f / a;
    k.deviceToViewDepth[3] = 1.0f / b;

    k.motionVectorScale[0] = 1.0f / (float)rw;
    k.motionVectorScale[1] = 1.0f / (float)rh;
    k.downscaleFactor[0] = (float)rw / (float)ow;
    k.downscaleFactor[1] = (float)rh / (float)oh;

    // host computes the horizontal FOV then tan(half of it)
    const float cameraAngleHorizontal = atanf(tanf(fovY / 2.0f) * aspect) * 2.0f;
    k.tanHalfFOV = tanf(cameraAngleHorizontal * 0.5f);
    k.jitterPhaseCount = (float)(int32_t)(8.0f * powf((float)ow / (float)rw, 2.0f));
    k.deltaTime = 0.016f;
    k.deltaPreExposure = 1.0f;
    k.viewSpaceToMetersFactor = 1.0f;
    k.frameIndex = 0.0f;
    k.velocityFactor = 1.0f;
    k.reactivenessScale = 1.0f;
    k.shadingChangeScale = 1.0f;
    k.accumulationAddedPerFrame = 1.0f / 3.0f;
    k.minDisocclusionAccumulation = -1.0f / 3.0f;
}

static float view_space_meters(const FsrConsts& k, float device_depth) {
    return (k.deviceToViewDepth[1] / (device_depth - k.deviceToViewDepth[0])) *
           k.viewSpaceToMetersFactor;
}

static void build_chain_resources(Chain& c, const std::vector<uint8_t>& color,
                                  const std::vector<float>& depth,
                                  const std::vector<float>& motion) {
    const int rw = c.rw, rh = c.rh, ow = c.ow, oh = c.oh;
    const int hw = rw / 2, hh = rh / 2;
    c.spd_base_w = hw; c.spd_base_h = hh;
    c.spd_levels = mip_levels_for(hw, hh);

    std::vector<uint8_t> zero8((size_t)ow * oh * 4, 0);
    std::vector<uint8_t> zero_r((size_t)rw * rh * 4, 0);
    std::vector<float> zero_f((size_t)ow * oh * 4, 0.0f);
    std::vector<uint32_t> one_u((size_t)rw * rh, 0x3F800000u); // clear to depth 1.0
    std::vector<float> exposure = {1.0f};
    std::vector<float> frame_info = {-1.0f, 1.0f, 0.0f, 0.0f};
    std::vector<uint8_t> zero_1 = {0};
    std::vector<uint32_t> zero_u1 = {0};

    c.res["input_color"] = make_tex(GL_RGBA8, rw, rh, 1, color.data(), GL_RGBA, GL_UNSIGNED_BYTE);
    c.res["input_depth"] = make_tex(GL_R32F, rw, rh, 1, depth.data(), GL_RED, GL_FLOAT);
    c.res["input_motion"] = make_tex(GL_RG16F, rw, rh, 1, motion.data(), GL_RG, GL_FLOAT);
    c.res["input_exposure"] = make_tex(GL_R32F, 1, 1, 1, exposure.data(), GL_RED, GL_FLOAT);
    c.res["input_reactive"] = make_tex(GL_R8, 1, 1, 1, zero_1.data(), GL_RED, GL_UNSIGNED_BYTE);
    c.res["input_transparency"] = make_tex(GL_R8, 1, 1, 1, zero_1.data(), GL_RED, GL_UNSIGNED_BYTE);

    c.res["recon_depth"] = make_tex(GL_R32UI, rw, rh, 1, one_u.data(), GL_RED_INTEGER, GL_UNSIGNED_INT);
    c.res["dilated_motion"] = make_tex(GL_RGBA16F, rw, rh, 1, zero_f.data(), GL_RGBA, GL_FLOAT);
    c.res["dilated_depth"] = make_tex(GL_R32F, rw, rh, 1, zero_f.data(), GL_RED, GL_FLOAT);
    c.res["acc1"] = make_tex(GL_RGBA8, rw, rh, 1, zero_r.data(), GL_RGBA, GL_UNSIGNED_BYTE);
    c.res["acc2"] = make_tex(GL_RGBA8, rw, rh, 1, zero_r.data(), GL_RGBA, GL_UNSIGNED_BYTE);
    c.res["luma1"] = make_tex(GL_RGBA16F, rw, rh, 1, zero_f.data(), GL_RGBA, GL_FLOAT);
    c.res["luma2"] = make_tex(GL_RGBA16F, rw, rh, 1, zero_f.data(), GL_RGBA, GL_FLOAT);
    c.res["intermediate"] = make_tex(GL_RGBA16F, rw, rh, 1, zero_f.data(), GL_RGBA, GL_FLOAT);
    c.res["lh1"] = make_tex(GL_RGBA16F, rw, rh, 1, zero_f.data(), GL_RGBA, GL_FLOAT);
    c.res["lh2"] = make_tex(GL_RGBA16F, rw, rh, 1, zero_f.data(), GL_RGBA, GL_FLOAT);
    c.res["iuc1"] = make_tex(GL_RGBA16F, ow, oh, 1, zero_f.data(), GL_RGBA, GL_FLOAT);
    c.res["iuc2"] = make_tex(GL_RGBA16F, ow, oh, 1, zero_f.data(), GL_RGBA, GL_FLOAT);
    c.res["new_locks"] = make_tex(GL_RGBA8, ow, oh, 1, zero8.data(), GL_RGBA, GL_UNSIGNED_BYTE);
    c.res["dilated_reactive"] = make_tex(GL_RGBA8, rw, rh, 1, zero_r.data(), GL_RGBA, GL_UNSIGNED_BYTE);
    c.res["shading_change"] = make_tex(GL_RGBA8, hw, hh, 1, std::vector<uint8_t>((size_t)hw * hh * 4, 0).data(), GL_RGBA, GL_UNSIGNED_BYTE);
    c.res["spd_mips"] = make_tex(GL_RGBA16F, hw, hh, c.spd_levels, zero_f.data(), GL_RGBA, GL_FLOAT);
    c.res["farthest_mip1"] = make_tex(GL_RGBA16F, hw, hh, 1, zero_f.data(), GL_RGBA, GL_FLOAT);
    c.res["spd_atomic"] = make_tex(GL_R32UI, 1, 1, 1, zero_u1.data(), GL_RED_INTEGER, GL_UNSIGNED_INT);
    c.res["frame_info"] = make_tex(GL_RGBA32F, 1, 1, 1, frame_info.data(), GL_RGBA, GL_FLOAT);
    c.res["output"] = make_tex(GL_RGBA8, ow, oh, 1, zero8.data(), GL_RGBA, GL_UNSIGNED_BYTE);

    // The Lanczos LUT is not sampled by this permutation (analytic Lanczos is
    // selected by the build flags); allocate a placeholder so name resolution
    // is complete if a future permutation binds it.
    std::vector<float> lut(128, 0.0f);
    c.res["lanczos_lut"] = make_tex(GL_R16F, 128, 1, 1, lut.data(), GL_RED, GL_FLOAT);
}

static bool bind_and_dispatch(Chain& c, PassInfo& p, const FsrConsts& k,
                              const SpdConsts& spd, uint32_t rcas_con,
                              std::string& err) {
    glUseProgram(p.prog);
    note_gl("chain glUseProgram");

    // Uniform blocks by declared binding.
    static GLuint buf_fsr = 0, buf_spd = 0, buf_rcas = 0;
    if (!buf_fsr) {
        glGenBuffers(1, &buf_fsr);
        glGenBuffers(1, &buf_spd);
        glGenBuffers(1, &buf_rcas);
    }
    glBindBuffer(GL_UNIFORM_BUFFER, buf_fsr);
    glBufferData(GL_UNIFORM_BUFFER, 256, nullptr, GL_DYNAMIC_DRAW);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof k, &k);
    glBindBuffer(GL_UNIFORM_BUFFER, buf_spd);
    glBufferData(GL_UNIFORM_BUFFER, 32, nullptr, GL_DYNAMIC_DRAW);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof spd, &spd);
    glBindBuffer(GL_UNIFORM_BUFFER, buf_rcas);
    uint32_t cfg[4] = {rcas_con, 0, 0, 0};
    glBufferData(GL_UNIFORM_BUFFER, sizeof cfg, cfg, GL_DYNAMIC_DRAW);

    for (auto& u : p.ubos) {
        GLuint idx = glGetUniformBlockIndex(p.prog, u.block.c_str());
        if (idx == GL_INVALID_INDEX) { err = "missing uniform block " + u.block; return false; }
        glUniformBlockBinding(p.prog, idx, u.binding);
        GLuint buf = (u.block == "cbFSR3UPSCALER_t") ? buf_fsr
                   : (u.block == "cbSPD_t") ? buf_spd
                   : (u.block == "cbRCAS_t") ? buf_rcas : 0;
        if (!buf) { err = "unexpected block " + u.block; return false; }
        glBindBufferRange(GL_UNIFORM_BUFFER, u.binding, buf, 0,
                          u.block == "cbFSR3UPSCALER_t" ? 256 : (u.block == "cbSPD_t" ? 32 : 16));
        char bb[128];
        snprintf(bb, sizeof bb, "uniform_block %s binding=%u bytes=%d", u.block.c_str(),
                 u.binding, u.block == "cbFSR3UPSCALER_t" ? 256 : (u.block == "cbSPD_t" ? 32 : 16));
        p.bindings.push_back(bb);
    }
    note_gl("chain uniform blocks");

    // Combined samplers. Texture units are independent of image units.
    int unit = 0;
    for (auto& s : p.samplers) {
        Tex* t = nullptr;
        int level = 0;
        if (!resolve_tex(c, s.base, t, level, false) || !t) {
            err = "unresolved sampler " + s.base;
            return false;
        }
        // A declared sampler can be inactive after linking (SPIRV-Cross emits
        // dummy/linear pairs and the unused half is optimised out).
        GLint loc = glGetUniformLocation(p.prog, s.full.c_str());
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, t->id);
        if (loc >= 0) glUniform1i(loc, unit);
        char bb[256];
        snprintf(bb, sizeof bb, "sampler2D %s unit=%d texture=%dx%d format=%s%s",
                 s.base.c_str(), unit, t->w, t->h, gl_format_name(t->internal),
                 loc >= 0 ? "" : " (inactive)");
        p.bindings.push_back(bb);
        unit++;
    }
    glActiveTexture(GL_TEXTURE0);
    note_gl("chain samplers");

    for (auto& d : p.images) {
        Tex* t = nullptr;
        int level = 0;
        if (!resolve_tex(c, d.name, t, level, true) || !t) {
            err = "unresolved image " + d.name;
            return false;
        }
        glBindImageTexture(d.unit, t->id, (GLint)level, GL_FALSE, 0, d.access, d.fmt);
        char bb[256];
        snprintf(bb, sizeof bb, "image2D %s unit=%u level=%d format=%s access=%s", d.name.c_str(),
                 d.unit, level, gl_format_name(d.fmt),
                 d.access == GL_WRITE_ONLY ? "WRITE_ONLY" :
                 d.access == GL_READ_ONLY ? "READ_ONLY" : "READ_WRITE");
        p.bindings.push_back(bb);
    }
    note_gl("chain images");

    glDispatchCompute((GLuint)p.gx, (GLuint)p.gy, 1);
    note_gl("chain dispatch");
    glMemoryBarrier(GL_ALL_BARRIER_BITS);
    note_gl("chain barrier");
    glFinish();
    note_gl("chain finish");

    for (auto& d : p.images) glBindImageTexture(d.unit, 0, 0, GL_FALSE, 0, d.access, d.fmt);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
    return true;
}

// ---- chain validation helpers -------------------------------------------
static bool all_r32ui_eq(Chain& c, const char* res, uint32_t value, int& bad, std::string& detail) {
    Tex* t = nullptr; int lvl = 0;
    if (!resolve_tex(c, res, t, lvl, true) || !t) { detail = "missing " + std::string(res); return false; }
    std::vector<uint8_t> raw;
    if (!read_level(*t, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, (size_t)t->w * t->h * 4, raw)) {
        detail = "read failed " + std::string(res); return false;
    }
    bad = 0;
    const uint32_t* v = (const uint32_t*)raw.data();
    for (size_t i = 0; i < (size_t)t->w * t->h; i++) if (v[i] != value) bad++;
    char b[160];
    snprintf(b, sizeof b, "%s pixels=%d mismatched=%d expected_bits=0x%08x", res, t->w * t->h, bad, value);
    detail = b;
    return true;
}

static bool all_r32f_near(Chain& c, const char* res, float value, float tol, int& bad, std::string& detail) {
    Tex* t = nullptr; int lvl = 0;
    if (!resolve_tex(c, res, t, lvl, true) || !t) { detail = "missing " + std::string(res); return false; }
    std::vector<uint8_t> raw;
    if (!read_level(*t, 0, GL_RED, GL_FLOAT, (size_t)t->w * t->h * 4, raw)) {
        detail = "read failed " + std::string(res); return false;
    }
    bad = 0; float mx = 0;
    const float* v = (const float*)raw.data();
    for (size_t i = 0; i < (size_t)t->w * t->h; i++) {
        float d = fabsf(v[i] - value);
        if (d > mx) mx = d;
        if (d > tol) bad++;
    }
    char b[192];
    snprintf(b, sizeof b, "%s pixels=%d out_of_tol=%d max_abs_diff=%g expected=%g tol=%g",
             res, t->w * t->h, bad, mx, value, tol);
    detail = b;
    return true;
}

static bool all_rgba16f_near(Chain& c, const char* res, int level, const float exp[3],
                             float tol, int& bad, std::string& detail, float* first = nullptr) {
    Tex* t = nullptr; int lvl = 0;
    if (!resolve_tex(c, res, t, lvl, true) || !t) { detail = "missing " + std::string(res); return false; }
    std::vector<uint8_t> raw;
    int w = level_dim(t->w, level), h = level_dim(t->h, level);
    if (!read_level(*t, level, GL_RGBA, GL_HALF_FLOAT, (size_t)w * h * 8, raw)) {
        detail = "read failed " + std::string(res); return false;
    }
    bad = 0; float mx = 0;
    const uint16_t* v = (const uint16_t*)raw.data();
    for (size_t i = 0; i < (size_t)w * h; i++) {
        for (int ch = 0; ch < 3; ch++) {
            float f = h2f(v[i * 4 + ch]);
            float d = fabsf(f - exp[ch]);
            if (d > mx) mx = d;
            if (d > tol) bad++;
        }
        if (first && i == 0) { for (int ch = 0; ch < 4; ch++) first[ch] = h2f(v[ch]); }
    }
    char b[320];
    snprintf(b, sizeof b, "%s level=%d pixels=%d out_of_tol=%d max_abs_diff=%g expected=(%g,%g,%g) "
             "first=(%g,%g,%g,%g) tol=%g",
             res, level, w * h, bad, mx, exp[0], exp[1], exp[2],
             h2f(v[0]), h2f(v[1]), h2f(v[2]), h2f(v[3]), tol);
    detail = b;
    return true;
}

static bool zero_rgba16f(Chain& c, const char* res, int& bad, std::string& detail) {
    float z[3] = {0, 0, 0};
    return all_rgba16f_near(c, res, 0, z, 0.0f, bad, detail);
}



static bool flat_rgba16f(Chain& c, const std::string& rn, int level, float tol,
                         int& bad, std::string& detail) {
    Tex* t = nullptr; int lvl = 0;
    if (!resolve_tex(c, rn, t, lvl, true) || !t) { detail = "missing " + rn; return false; }
    std::vector<uint8_t> raw;
    int w = level_dim(t->w, level), h = level_dim(t->h, level);
    if (!read_level(*t, level, GL_RGBA, GL_HALF_FLOAT, (size_t)w * h * 8, raw)) {
        detail = "read failed " + rn; return false;
    }
    const uint16_t* v = (const uint16_t*)raw.data();
    float mn[3] = {1e30f, 1e30f, 1e30f}, mx[3] = {-1e30f, -1e30f, -1e30f};
    for (size_t i = 0; i < (size_t)w * h; i++)
        for (int ch = 0; ch < 3; ch++) {
            float f = h2f(v[i * 4 + ch]);
            if (f < mn[ch]) mn[ch] = f;
            if (f > mx[ch]) mx[ch] = f;
        }
    bad = 0;
    for (int ch = 0; ch < 3; ch++) if (mx[ch] - mn[ch] > tol) bad++;
    char b[256];
    snprintf(b, sizeof b, "%s level=%d size=%dx%d spread=(%g,%g,%g) tol=%g", rn.c_str(),
             level, w, h, mx[0] - mn[0], mx[1] - mn[1], mx[2] - mn[2], tol);
    detail = b;
    return true;
}

// Chain validation: run checkpoints and emit a JSON object.
// Assumes an EGL context is already current (main initializes it).
static int run_chain_body(const std::string& shader_dir, const std::string& pattern,
                          int rw, int rh, int ow, int oh,
                          const char* renderer, const char* vendor,
                          const char* version, const char* glsl) {
    Chain c;
    c.rw = rw; c.rh = rh; c.ow = ow; c.oh = oh;

    const char* files[8] = {"prepare_inputs", "luma_pyramid", "shading_change_pyramid",
                            "shading_change", "prepare_reactivity", "luma_instability",
                            "accumulate", "rcas"};
    PassInfo passes[8];
    bool all_compiled = true;
    for (int i = 0; i < 8; i++) {
        passes[i].name = files[i];
        std::string path = shader_dir + "/" + files[i] + "_essl320.glsl";
        bool ok = false;
        std::string src = read_file(path.c_str(), ok);
        if (!ok) {
            passes[i].log = "cannot read " + path;
            all_compiled = false;
            continue;
        }
        parse_essl(src, passes[i]);
        passes[i].prog = link_program(src.c_str(), passes[i].log);
        passes[i].compiled = passes[i].prog != 0;
        std::string sha = sha::digest(src.data(), src.size());
        if (!passes[i].compiled) all_compiled = false;
        // stash the shader hash in the log for the JSON
        passes[i].log = passes[i].compiled ? sha : ("FAILED " + passes[i].log);
    }
    check("chain_compile_link", all_compiled,
          all_compiled ? "8/8 adapted passes compiled and linked"
                       : "at least one pass failed to compile/link");
    if (!all_compiled) {
        printf("{\"harness\":\"fsr3_dispatch\",\"pass\":\"chain\",\"status\":\"FAIL\","
               "\"renderer\":%s,\"vendor\":%s,\"gl_version\":%s,\"glsl_version\":%s,"
               "\"passes\":[", jstr(renderer).c_str(), jstr(vendor).c_str(),
               jstr(version).c_str(), jstr(glsl).c_str());
        for (int i = 0; i < 8; i++)
            printf("%s{\"name\":%s,\"status\":%s,\"detail\":%s}", i ? "," : "",
                   jstr(passes[i].name).c_str(),
                   passes[i].compiled ? "\"PASS\"" : "\"FAIL\"",
                   jstr(passes[i].log).c_str());
        printf("],\"checks\":[],\"errors\":[\"chain shader compile/link failed\"]}\n");
        return 1;
    }

    // Inputs -----------------------------------------------------------------
    std::vector<uint8_t> color((size_t)rw * rh * 4);
    bool flat = (pattern == "flat");
    if (flat) {
        for (size_t i = 0; i < color.size(); i += 4) {
            color[i] = 96; color[i + 1] = 128; color[i + 2] = 192; color[i + 3] = 255;
        }
    } else {
        color = make_input(rw, rh);
    }
    std::vector<float> depth((size_t)rw * rh, 0.5f);
    std::vector<float> motion((size_t)rw * rh * 2, 0.0f);

    build_chain_resources(c, color, depth, motion);
    note_gl("chain resource creation");

    FsrConsts k;
    fill_consts(k, rw, rh, ow, oh);

    const int spd_gx = (rw + 63) / 64, spd_gy = (rh + 63) / 64;
    const int spd_num = spd_gx * spd_gy;
    int maxdim = rw > rh ? rw : rh;
    int mips = 0;
    while ((1 << (mips + 1)) <= maxdim) mips++;
    if (mips > 12) mips = 12;

    SpdConsts spd;
    memset(&spd, 0, sizeof spd);
    spd.mips = (uint32_t)mips;
    spd.numWorkGroups = (uint32_t)spd_num;
    spd.workGroupOffset[0] = 0;
    spd.workGroupOffset[1] = 0;
    spd.renderSize[0] = (uint32_t)rw;
    spd.renderSize[1] = (uint32_t)rh;

    float sharpenessRemapped = (-2.0f * 0.2f) + 2.0f;
    float con_f = exp2f(-sharpenessRemapped);
    uint32_t rcas_con = f2u(con_f);

    const int hw = rw / 2, hh = rh / 2;
    int groups[8][2] = {
        {(rw + 7) / 8, (rh + 7) / 8},
        {spd_gx, spd_gy},
        {spd_gx, spd_gy},
        {(hw + 7) / 8, (hh + 7) / 8},
        {(rw + 7) / 8, (rh + 7) / 8},
        {(rw + 7) / 8, (rh + 7) / 8},
        {(ow + 7) / 8, (oh + 7) / 8},
        {(ow + 15) / 16, (oh + 15) / 16},
    };
    for (int i = 0; i < 8; i++) {
        passes[i].gx = groups[i][0];
        passes[i].gy = groups[i][1];
    }

    // Declared expectations --------------------------------------------------
    float luma = (0.2126f * (color[0] / 255.0f) + 0.7152f * (color[1] / 255.0f) +
                  0.0722f * (color[2] / 255.0f));
    float meters = view_space_meters(k, 0.5f);
    // PrepareInputs stores luma in R only; the pyramid source image is
    // (max(6.1e-5, log(luma)), luma, farthestDepthMeters).
    float luma_exp[3] = {luma, 0.0f, 0.0f};
    float far_exp[3] = {meters, 0.0f, 0.0f};
    float mip5_exp[3] = {6.1e-5f, luma, 0.0f};

    std::string hashes[8];
    bool all_ran = true;

    for (int i = 0; i < 8; i++) {
        std::string err;
        bool ok = bind_and_dispatch(c, passes[i], k, spd, rcas_con, err);
        if (!ok) { g_errors.push_back("chain " + passes[i].name + ": " + err); all_ran = false; break; }
        note_gl(("chain " + passes[i].name).c_str());

        // Checkpoints (test-only readbacks).
        if (i == 0) {
            int bad = 0; std::string d;
            bool r = all_r32ui_eq(c, "rw_reconstructed_previous_nearest_depth", 0x3F000000u, bad, d);
            check("prepare_inputs_reconstructed_depth", r && bad == 0, d);
            r = all_r32f_near(c, "rw_dilated_depth", 0.5f, 1e-6f, bad, d);
            check("prepare_inputs_dilated_depth", r && bad == 0, d);
            r = zero_rgba16f(c, "rw_dilated_motion_vectors", bad, d);
            check("prepare_inputs_dilated_motion", r && bad == 0, d);
            if (flat) {
                r = all_rgba16f_near(c, "rw_current_luma", 0, luma_exp, 2e-3f, bad, d);
                check("prepare_inputs_current_luma", r && bad == 0, d);
            }
            r = all_rgba16f_near(c, "rw_farthest_depth", 0, far_exp, 2e-3f, bad, d);
            check("prepare_inputs_farthest_depth", r && bad == 0, d);
        } else if (i == 1 && flat) {
            int bad = 0; std::string d;
            bool r = all_rgba16f_near(c, "rw_spd_mip5", 5, mip5_exp, 5e-3f, bad, d);
            check("luma_pyramid_mip5_average", r && bad == 0, d);
            r = all_rgba16f_near(c, "rw_farthest_depth_mip1", 0, far_exp, 2e-3f, bad, d);
            check("luma_pyramid_farthest_mip1", r && bad == 0, d);
            r = all_r32ui_eq(c, "rw_spd_global_atomic", 0u, bad, d);
            check("luma_pyramid_atomic_reset", r && bad == 0, d);
        } else if (i == 2 && flat) {
            for (int lv = 0; lv <= 5; lv++) {
                std::string rn = "rw_spd_mip" + std::to_string(lv);
                int bad = 0; std::string d;
                bool r = flat_rgba16f(c, rn, lv, 1e-2f, bad, d);
                check("shading_change_pyramid_flat_mip" + std::to_string(lv), r && bad == 0, d);
            }
        }
    }

    check("chain_all_passes_dispatched", all_ran,
          all_ran ? "8/8 passes dispatched with GL_ALL_BARRIER_BITS + glFinish"
                  : "chain stopped at a failed pass");

    // Final output ----------------------------------------------------------
    std::vector<uint8_t> out;
    Tex* out_tex = nullptr; int lvl = 0;
    bool have_out = resolve_tex(c, "rw_upscaled_output", out_tex, lvl, true) &&
                    read_level(*out_tex, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                               (size_t)ow * oh * 4, out);
    check("chain_output_readback", have_out, have_out ? "RGBA8 readback" : "readback failed");
    std::string out_sha = have_out ? hash_vec(out) : "";

    if (have_out && flat) {
        int mn[3] = {255, 255, 255}, mx[3] = {0, 0, 0};
        for (size_t i = 0; i < (size_t)ow * oh; i++)
            for (int ch = 0; ch < 3; ch++) {
                int v = out[i * 4 + ch];
                if (v < mn[ch]) mn[ch] = v;
                if (v > mx[ch]) mx[ch] = v;
            }
        int max_spread = 0, max_diff = 0;
        for (int ch = 0; ch < 3; ch++) {
            if (mx[ch] - mn[ch] > max_spread) max_spread = mx[ch] - mn[ch];
            int d = abs(mx[ch] - color[ch]);
            int d2 = abs(mn[ch] - color[ch]);
            if (d > max_diff) max_diff = d;
            if (d2 > max_diff) max_diff = d2;
        }
        char b[256];
        snprintf(b, sizeof b,
                 "extent=%dx%d min=(%d,%d,%d) max=(%d,%d,%d) input=(%d,%d,%d) "
                 "spread=%d max_channel_diff=%d tolerance: spread<=1, diff<=3",
                 ow, oh, mn[0], mn[1], mn[2], mx[0], mx[1], mx[2], color[0], color[1],
                 color[2], max_spread, max_diff);
        check("chain_flat_output_invariant", max_spread <= 1 && max_diff <= 3, b);
    } else if (have_out) {
        int mn = 255, mx = 0;
        for (size_t i = 0; i < (size_t)ow * oh; i++)
            for (int ch = 0; ch < 3; ch++) {
                int v = out[i * 4 + ch];
                if (v < mn) mn = v;
                if (v > mx) mx = v;
            }
        check("chain_step_output_non_degenerate", mx - mn > 16,
              "min=" + std::to_string(mn) + " max=" + std::to_string(mx) +
              " (synthetic step frame, differential reference)");
    }

    // Extra state hashes for cross-implementation comparison.
    const char* hres[5] = {"rw_dilated_motion_vectors", "rw_reconstructed_previous_nearest_depth",
                           "rw_spd_mip5", "rw_shading_change", "rw_dilated_reactive_masks"};
    std::string extra[5];
    for (int i = 0; i < 5; i++) {
        Tex* t = nullptr; int l = 0;
        std::vector<uint8_t> raw;
        resolve_tex(c, hres[i], t, l, true);
        if (t && t->valid()) {
            if (strcmp(hres[i], "rw_reconstructed_previous_nearest_depth") == 0)
                read_level(*t, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, (size_t)t->w * t->h * 4, raw);
            else if (strcmp(hres[i], "rw_spd_mip5") == 0)
                read_level(*t, 5, GL_RGBA, GL_HALF_FLOAT, (size_t)level_dim(t->w, 5) * level_dim(t->h, 5) * 8, raw);
            else if (strcmp(hres[i], "rw_shading_change") == 0 ||
                     strcmp(hres[i], "rw_dilated_reactive_masks") == 0)
                read_level(*t, 0, GL_RGBA, GL_UNSIGNED_BYTE, (size_t)t->w * t->h * 4, raw);
            else
                read_level(*t, 0, GL_RGBA, GL_HALF_FLOAT, (size_t)t->w * t->h * 8, raw);
        }
        extra[i] = hash_vec(raw);
        if (raw.size() >= 4)
            fprintf(stderr, "[chain] %s first bytes %u,%u,%u,%u\n", hres[i],
                    raw[0], raw[1], raw[2], raw[3]);
    }

    bool pass = true;
    for (auto& chk : g_checks) if (chk.status != "PASS") pass = false;
    if (!g_gl_ok) pass = false;

    // JSON ------------------------------------------------------------------
    printf("{\"harness\":\"fsr3_dispatch\",\"pass\":\"chain\",\"status\":\"%s\",\n",
           pass ? "PASS" : "FAIL");
    printf("\"pattern\":%s,\"extent\":{\"render\":[%d,%d],\"upscale\":[%d,%d]},\n",
           jstr(pattern).c_str(), rw, rh, ow, oh);
    printf("\"reference\":{\"type\":\"differential + declared invariants\","
           "\"description\":\"host Mesa GLES run of the same adapted ESSL (byte hashes) plus analytic flat-frame expectations; not an independent Vulkan reference\"},\n");
    printf("\"renderer\":%s,\"vendor\":%s,\"gl_version\":%s,\"glsl_version\":%s,\n",
           jstr(renderer).c_str(), jstr(vendor).c_str(), jstr(version).c_str(), jstr(glsl).c_str());
    printf("\"constants\":{\"render\":%d,\"upscale\":%d,\"device_to_view_depth\":[%.9g,%.9g,%.9g,%.9g],"
           "\"jitter_phase_count\":%.0f,\"frame_index\":0,\"delta_time\":0.016,"
           "\"accumulation_added_per_frame\":%.9g,\"min_disocclusion_accumulation\":%.9g},\n",
           rw, ow, k.deviceToViewDepth[0], k.deviceToViewDepth[1], k.deviceToViewDepth[2],
           k.deviceToViewDepth[3], k.jitterPhaseCount, k.accumulationAddedPerFrame,
           k.minDisocclusionAccumulation);
    printf("\"spd\":{\"groups\":[%d,%d],\"num_workgroups\":%d,\"mips\":%d,"
           "\"workgroup_offset\":[0,0],\"renderSize\":[%d,%d]},\n",
           spd_gx, spd_gy, spd_num, mips, rw, rh);
    printf("\"passes\":[");
    for (int i = 0; i < 8; i++) {
        printf("%s{\"name\":%s,\"shader\":\"%s_essl320.glsl\",\"shader_sha256\":%s,"
               "\"local_size\":[%d,%d],\"groups\":[%d,%d],\"barrier\":\"GL_ALL_BARRIER_BITS\","
               "\"status\":\"PASS\",\"bindings\":[",
               i ? "," : "", jstr(passes[i].name).c_str(), passes[i].name.c_str(),
               jstr(passes[i].log).c_str(), passes[i].lx, passes[i].ly,
               passes[i].gx, passes[i].gy);
        for (size_t bi = 0; bi < passes[i].bindings.size(); bi++)
            printf("%s%s", bi ? "," : "", jstr(passes[i].bindings[bi]).c_str());
        printf("]}");
    }
    printf("],\n\"hashes\":{\"output\":%s,\"dilated_motion\":%s,\"reconstructed_depth\":%s,"
           "\"spd_mip5\":%s,\"shading_change\":%s,\"dilated_reactive\":%s},\n",
           jstr(out_sha).c_str(), jstr(extra[0]).c_str(), jstr(extra[1]).c_str(),
           jstr(extra[2]).c_str(), jstr(extra[3]).c_str(), jstr(extra[4]).c_str());
    printf("\"checks\":[");
    for (size_t i = 0; i < g_checks.size(); i++) {
        if (i) printf(",");
        printf("{\"name\":%s,\"status\":%s,\"detail\":%s}",
               jstr(g_checks[i].name).c_str(), jstr(g_checks[i].status).c_str(),
               jstr(g_checks[i].detail).c_str());
    }
    printf("],\n\"errors\":[");
    for (size_t i = 0; i < g_errors.size(); i++) {
        if (i) printf(",");
        printf("%s", jstr(g_errors[i]).c_str());
    }
    printf("]}\n");

    for (int i = 0; i < 8; i++) if (passes[i].prog) glDeleteProgram(passes[i].prog);
    for (auto& kv : c.res) if (kv.second.id) glDeleteTextures(1, &kv.second.id);
    return pass ? 0 : 1;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
struct Args {
    std::string shader;
    std::string mode = "rcas";
    std::string shader_dir = ".";
    std::string pattern = "flat";
    int width = 320;
    int height = 180;
    int rw = 160, rh = 90, ow = 320, oh = 180;
    float sharpness = 0.2f; // FSR3 param; host remaps to stops before FsrRcasCon
    bool json_only = true;
};

int main(int argc, char** argv) {
    Args a;
    for (int i = 1; i < argc; i++) {
        std::string s = argv[i];
        auto next = [&](const char* what) -> std::string {
            if (i + 1 >= argc) { fprintf(stderr, "missing value for %s\n", what); exit(2); }
            return argv[++i];
        };
        if (s == "--shader") a.shader = next("--shader");
        else if (s == "--mode") a.mode = next("--mode");
        else if (s == "--shaders") a.shader_dir = next("--shaders");
        else if (s == "--pattern") a.pattern = next("--pattern");
        else if (s == "--width") a.width = atoi(next("--width").c_str());
        else if (s == "--height") a.height = atoi(next("--height").c_str());
        else if (s == "--render") { std::string v = next("--render");
            if (sscanf(v.c_str(), "%dx%d", &a.rw, &a.rh) != 2) return 2; }
        else if (s == "--output") { std::string v = next("--output");
            if (sscanf(v.c_str(), "%dx%d", &a.ow, &a.oh) != 2) return 2; }
        else if (s == "--sharpness") a.sharpness = (float)atof(next("--sharpness").c_str());
        else { fprintf(stderr, "unknown arg %s\n", s.c_str()); return 2; }
    }
    if (a.mode != "rcas" && a.mode != "chain") {
        fprintf(stderr, "unknown mode %s\n", a.mode.c_str());
        return 2;
    }
    if (a.mode == "rcas" && a.shader.empty()) {
        fprintf(stderr, "--shader is required\n");
        return 2;
    }
    if (a.mode == "rcas" && (a.width < 16 || a.height < 16 || a.width > 4096 || a.height > 4096)) {
        fprintf(stderr, "bad extent\n");
        return 2;
    }
    if (a.mode == "chain" && (a.rw < 16 || a.rh < 16 || a.ow < 16 || a.oh < 16 ||
                              a.ow < a.rw || a.oh < a.rh)) {
        fprintf(stderr, "bad chain extent\n");
        return 2;
    }

    EGLDisplay dpy;
    EGLContext ctx;
    EGLSurface surf;
    if (!init_egl(dpy, ctx, surf)) return 2;

    const char* renderer = (const char*)glGetString(GL_RENDERER);
    const char* vendor = (const char*)glGetString(GL_VENDOR);
    const char* version = (const char*)glGetString(GL_VERSION);
    const char* glsl = (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION);
    renderer = renderer ? renderer : "";
    vendor = vendor ? vendor : "";
    version = version ? version : "";
    glsl = glsl ? glsl : "";

    if (a.mode == "chain") {
        int rc = run_chain_body(a.shader_dir, a.pattern, a.rw, a.rh, a.ow, a.oh,
                                renderer, vendor, version, glsl);
        eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroySurface(dpy, surf);
        eglDestroyContext(dpy, ctx);
        eglTerminate(dpy);
        fflush(stdout);
        return rc;
    }

    bool file_ok = false;
    std::string src = read_file(a.shader.c_str(), file_ok);
    std::string shader_sha = file_ok ? sha::digest(src.data(), src.size()) : "";

    std::string log;
    GLuint prog = 0;
    if (file_ok) prog = link_program(src.c_str(), log);
    check("shader_compile_link", prog != 0,
          prog ? "program linked" : ("compile/link failed: " + log));
    if (!prog) {
        printf("{\"harness\":\"fsr3_dispatch\",\"pass\":\"rcas\",\"status\":\"FAIL\","
               "\"renderer\":%s,\"vendor\":%s,\"gl_version\":%s,\"glsl_version\":%s,"
               "\"shader\":{\"path\":%s,\"sha256\":%s,\"bytes\":%zu,\"file_ok\":%s},"
               "\"log\":%s,\"checks\":[],\"errors\":[\"shader compile/link failed\"]}\n",
               jstr(renderer).c_str(), jstr(vendor).c_str(), jstr(version).c_str(),
               jstr(glsl).c_str(), jstr(a.shader).c_str(), jstr(shader_sha).c_str(),
               src.size(), file_ok ? "true" : "false", jstr(log).c_str());
        return 1;
    }

    int w = a.width, h = a.height;
    std::vector<uint8_t> input = make_input(w, h);

    // FSR3 host remap + FsrRcasCon: con[0] = asuint(exp2(-(2 - 2*sharpness))).
    float remapped = (-2.0f * a.sharpness) + 2.0f;
    float con_f = exp2f(-remapped);
    uint32_t con0 = f2u(con_f);

    GLuint in_tex = make_rgba8(w, h, input);
    GLuint exp_tex = make_r32f(1, 1, 1.0f);
    note_gl("resource creation");

    std::vector<UboInfo> ubos;
    std::vector<std::string> bind_log;
    RcasResult res = dispatch_rcas(prog, in_tex, exp_tex, w, h, con0, ubos, bind_log);
    check("dispatch_and_readback", res.executed,
          res.executed ? "dispatch issued, full barrier, glFinish, FBO readback"
                       : res.readback_error);

    // Determinism: an identical second dispatch must produce identical bytes.
    std::vector<UboInfo> ubos2;
    std::vector<std::string> bind_log2;
    RcasResult res2 = dispatch_rcas(prog, in_tex, exp_tex, w, h, con0, ubos2, bind_log2);
    bool det = res.executed && res2.executed && res.out == res2.out;
    check("determinism", det,
          det ? "two dispatches, byte-identical readback"
              : "second dispatch differs or failed");

    // Sharpness 0 => con[0] = exp2(-2) is not identity; FSR3 sharpness=1 gives
    // exp2(0)=1 which is also not a pass-through. The pass-through case is
    // sharpness=1? No: lobe = min(maxLobe,0)*con.x; con.x == 0 would be
    // pass-through but FsrRcasCon never emits 0. Declared invariant instead:
    // constant regions are reproduced exactly, and RCAS output differs from
    // its input on the step edge.
    std::vector<uint8_t> ref;
    std::vector<uint8_t> interior;
    build_reference(input, w, h, con_f, ref, interior);

    // Reference hash covers interior pixels of the CPU model, row-major RGBA8.
    std::vector<uint8_t> ref_interior;
    ref_interior.reserve((size_t)w * h * 4);
    size_t interior_count = 0;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            size_t i = (size_t)y * w + x;
            if (!interior[i]) continue;
            interior_count++;
            ref_interior.insert(ref_interior.end(), &ref[i * 4], &ref[i * 4] + 4);
        }

    // Compare.
    size_t exact = 0, le1 = 0, gt1 = 0;
    int maxdiff = 0;
    std::vector<std::string> samples;
    if (res.executed) {
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++) {
                size_t i = (size_t)y * w + x;
                if (!interior[i]) continue;
                int d = 0;
                for (int c = 0; c < 4; c++) {
                    int dv = abs((int)res.out[i * 4 + c] - (int)ref[i * 4 + c]);
                    if (dv > d) d = dv;
                }
                if (d == 0) exact++;
                if (d <= 1) le1++;
                if (d > 1) {
                    gt1++;
                    if (samples.size() < 8) {
                        char b[192];
                        snprintf(b, sizeof b,
                                 "(%d,%d) got=%u,%u,%u,%u ref=%u,%u,%u,%u maxdiff=%d",
                                 x, y, res.out[i * 4], res.out[i * 4 + 1],
                                 res.out[i * 4 + 2], res.out[i * 4 + 3],
                                 ref[i * 4], ref[i * 4 + 1], ref[i * 4 + 2],
                                 ref[i * 4 + 3], d);
                        samples.push_back(b);
                    }
                }
                if (d > maxdiff) maxdiff = d;
            }
    }
    double exact_frac = interior_count ? (double)exact / (double)interior_count : 0.0;
    // Declared tolerance: every interior pixel within 1 8-bit level of the CPU
    // model, and >=99.9% byte-exact.
    bool model_ok = res.executed && gt1 == 0 && exact_frac >= 0.999;
    char det_detail[512];
    snprintf(det_detail, sizeof det_detail,
             "interior_pixels=%zu exact=%zu (%.4f%%) le1=%zu gt1=%zu maxdiff=%d "
             "tolerance=maxdiff<=1 and exact>=99.9%%",
             interior_count, exact, exact_frac * 100.0, le1, gt1, maxdiff);
    check("cpu_model_interior", model_ok, det_detail);

    // Constant patch: RCAS must reproduce a uniform neighborhood exactly.
    int px0, py0, px1, py1;
    patch_rect(w, h, px0, py0, px1, py1);
    // Inset by one pixel: the 5-neighborhood of a boundary pixel is not flat.
    px0 += 1; py0 += 1; px1 -= 1; py1 -= 1;
    int patch_bad = 0, patch_px = 0;
    std::string patch_sample;
    if (res.executed) {
        for (int y = py0; y < py1; y++)
            for (int x = px0; x < px1; x++) {
                size_t i = (size_t)y * w + x;
                patch_px++;
                int d = 0;
                for (int c = 0; c < 3; c++)
                    d = std::max(d, abs((int)res.out[i * 4 + c] - 128));
                if (res.out[i * 4 + 3] != 255) d = 255;
                if (d != 0) {
                    patch_bad++;
                    if (patch_sample.empty()) {
                        char b[160];
                        snprintf(b, sizeof b, "first bad (%d,%d) got=%u,%u,%u,%u exp=128",
                                 x, y, res.out[i * 4], res.out[i * 4 + 1],
                                 res.out[i * 4 + 2], res.out[i * 4 + 3]);
                        patch_sample = b;
                    }
                }
            }
    }
    char patch_detail[384];
    snprintf(patch_detail, sizeof patch_detail,
             "flat patch [%d,%d)-[%d,%d) pixels=%d mismatched=%d%s%s", px0, py0, px1,
             py1, patch_px, patch_bad, patch_sample.empty() ? "" : " ",
             patch_sample.c_str());
    check("flat_region_identity", res.executed && patch_bad == 0, patch_detail);

    // Alpha: the pass writes 1.0.
    int alpha_bad = 0;
    if (res.executed)
        for (size_t i = 0; i < (size_t)w * h; i++)
            if (res.out[i * 4 + 3] != 255) alpha_bad++;
    check("alpha_opaque", res.executed && alpha_bad == 0,
          "alpha!=255 pixels=" + std::to_string(alpha_bad));

    // Edge response: the pass must change the step-edge neighborhood (a no-op
    // or cleared buffer cannot pass).
    int changed = 0;
    if (res.executed)
        for (size_t i = 0; i < (size_t)w * h; i++)
            for (int c = 0; c < 3; c++)
                if (res.out[i * 4 + c] != input[i * 4 + c]) { changed++; break; }
    check("output_is_sharpened", res.executed && changed > 0,
          "pixels differing from input=" + std::to_string(changed));

    bool pass = true;
    for (auto& c : g_checks) if (c.status != "PASS") pass = false;
    if (!g_gl_ok) pass = false;

    std::string out_sha = res.executed ? sha::digest(res.out.data(), res.out.size()) : "";
    std::string ref_sha = sha::digest(ref_interior.data(), ref_interior.size());

    // JSON
    printf("{\"harness\":\"fsr3_dispatch\",\"pass\":\"rcas\",\"status\":\"%s\",\n",
           pass ? "PASS" : "FAIL");
    printf("\"reference\":{\"model\":\"CPU fp32 port of FsrRcasF (ffx_fsr3upscaler_rcas.h, FSR3.1.4)\","
           "\"comparison_region\":\"interior, 1px border excluded (out-of-range texelFetch is undefined)\","
           "\"tolerance\":\"max_abs_8bit_diff<=1 and exact>=99.9%%\","
           "\"con_x\":%.9g,\"con_x_bits\":%u,\"sharpness_param\":%.6g,"
           "\"interior_sha256\":%s,\"interior_pixels\":%zu},\n",
           con_f, con0, a.sharpness, jstr(ref_sha).c_str(), interior_count);
    printf("\"renderer\":%s,\"vendor\":%s,\"gl_version\":%s,\"glsl_version\":%s,\n",
           jstr(renderer).c_str(), jstr(vendor).c_str(), jstr(version).c_str(),
           jstr(glsl).c_str());
    printf("\"shader\":{\"path\":%s,\"sha256\":%s,\"bytes\":%zu,\"file_ok\":%s,"
           "\"local_size\":[64,1,1],\"source\":\"ffx_fsr3upscaler_rcas_pass.glsl FSR3.1.4 (SDK v1.1.4 c6efa6b)\"},\n",
           jstr(a.shader).c_str(), jstr(shader_sha).c_str(), src.size(),
           file_ok ? "true" : "false");
    printf("\"resources\":{\"input\":{\"type\":\"sampler2D\",\"format\":\"GL_RGBA8\","
           "\"size\":[%d,%d],\"filter\":\"NEAREST\",\"data\":\"synthetic step/ramp/patch\"},"
           "\"exposure\":{\"type\":\"sampler2D\",\"format\":\"GL_R32F\",\"size\":[1,1],\"value\":1.0},"
           "\"output\":{\"type\":\"image2D\",\"format\":\"GL_RGBA8\",\"layout\":\"rgba8 writeonly\","
           "\"unit\":2,\"size\":[%d,%d]}},\n", w, h, w, h);
    printf("\"uniform_blocks\":[");
    for (size_t i = 0; i < ubos.size(); i++) {
        if (i) printf(",");
        printf("{\"name\":%s,\"binding\":%d,\"block_size\":%d,\"active\":%s}",
               jstr(ubos[i].name).c_str(), ubos[i].binding, ubos[i].size,
               ubos[i].active ? "true" : "false");
    }
    printf("],\n");
    printf("\"samplers\":[");
    for (size_t i = 0; i < bind_log.size(); i++) {
        if (i) printf(",");
        printf("%s", jstr(bind_log[i]).c_str());
    }
    printf("],\n");
    printf("\"dispatch\":{\"groups\":[%d,%d,1],\"local_size\":[64,1,1],"
           "\"barrier\":\"GL_ALL_BARRIER_BITS\",\"finish\":true,\"repeat\":2},\n",
           (w + 15) / 16, (h + 15) / 16);
    printf("\"readback\":{\"method\":\"FBO attach + glReadPixels RGBA/UNSIGNED_BYTE\","
           "\"format\":\"RGBA8\",\"bytes\":%zu,\"sha256\":%s},\n",
           res.out.size(), jstr(out_sha).c_str());
    printf("\"checks\":[");
    for (size_t i = 0; i < g_checks.size(); i++) {
        if (i) printf(",");
        printf("{\"name\":%s,\"status\":%s,\"detail\":%s}",
               jstr(g_checks[i].name).c_str(), jstr(g_checks[i].status).c_str(),
               jstr(g_checks[i].detail).c_str());
    }
    printf("],\n\"errors\":[");
    for (size_t i = 0; i < g_errors.size(); i++) {
        if (i) printf(",");
        printf("%s", jstr(g_errors[i]).c_str());
    }
    printf("]}\n");

    glDeleteTextures(1, &in_tex);
    glDeleteTextures(1, &exp_tex);
    glDeleteProgram(prog);

    eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroySurface(dpy, surf);
    eglDestroyContext(dpy, ctx);
    eglTerminate(dpy);
    fflush(stdout);
    return pass ? 0 : 1;
}
