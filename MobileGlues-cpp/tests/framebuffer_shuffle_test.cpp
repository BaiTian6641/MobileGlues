// Drives the REAL gl/framebuffer.cpp against a fake driver that records which
// texture is physically attached at each colour attachment point. The question
// the shuffle has to answer correctly is only ever this one: when the
// application draws through draw-buffer slot j, does the texture it named in
// bufs[j] actually receive the pixels?
#include <cstdio>
#include <vector>
#include <map>
#include <array>
#include "gl/framebuffer.h"
#include "gles/loader.h"
#include "gl/mg.h"
#include "config/settings.h"
#include "gl/FSR3ScenePlan.h"

// --- the externals framebuffer.cpp reaches for -------------------------------
struct gles_func_t g_gles_func{};
namespace FSR1_Context { GLuint g_renderFBO = 0; bool g_dirty = false; }
void set_gl_state_current_draw_fbo(GLuint v) { gl_state->current_draw_fbo = v; }
void mg_set_gl_error(GLenum) {}
static std::array<GLint, 4> driver_viewport{0, 0, 2048, 2048};
static unsigned viewport_calls = 0;
static unsigned scene_viewport_calls = 0; // conversions the planner accepted
static unsigned adjust_probe_calls = 0;   // times the bind path asked
static unsigned native_viewport_calls = 0;
static bool fsr3_final_scope_active = false;
static GLuint bound_draw_fb = 0;
static GLuint bound_read_fb = 0;
// The bind path must run the same planner as the explicit glViewport/glScissor
// paths. The real FSR3AdjustViewport asks the driver for COLOR_ATTACHMENT0's
// texture extent and the EGL surface size; this harness fakes the driver, so it
// models the target extent per bound framebuffer instead.
static std::map<GLuint, std::array<int, 2>> target_extent;
bool FSR3AdjustViewport(int& x, int& y, int& width, int& height) {
    ++adjust_probe_calls;
    const auto it = target_extent.find(bound_draw_fb);
    if (it == target_extent.end()) return false;
    const FSR3ViewportPlan plan = FSR3PlanViewport(2880, 1585, it->second[0], it->second[1],
                                                   x, y, width, height);
    if (!plan.adjust) return false;
    x = plan.x;
    y = plan.y;
    width = plan.width;
    height = plan.height;
    ++scene_viewport_calls;
    return true;
}
bool FSR3FinalScopeActive() { return fsr3_final_scope_active; }
void FSR3NativeViewport() {
    ++native_viewport_calls;
    driver_viewport = {0, 0, 2880, 1585};
}
int __android_log_print(int, const char*, const char*, ...) { return 0; }
extern "C" void write_log(const char*, ...) {}
global_settings_t global_settings{};
gl_state_s g_default_gl_state{};
thread_local gl_state_t gl_state = &g_default_gl_state;

// --- fake driver ----------------------------------------------------------
static std::map<int, GLuint> physical;   // attachment index -> texture name
static std::vector<GLenum>   draw_list;  // what the driver was last told
static std::vector<GLenum>   invalidated;

static void fake_bind_fb(GLenum target, GLuint fb) {
    if (target != GL_READ_FRAMEBUFFER) bound_draw_fb = fb;
    if (target != GL_DRAW_FRAMEBUFFER) bound_read_fb = fb;
}
static void fake_viewport(GLint x, GLint y, GLsizei width, GLsizei height) {
    ++viewport_calls;
    driver_viewport = {x, y, width, height};
}
static void fake_fbtex2d(GLenum, GLenum att, GLenum, GLuint tex, GLint) {
    physical[att - GL_COLOR_ATTACHMENT0] = tex;
}
static void fake_fbrb(GLenum, GLenum att, GLenum, GLuint rb) {
    physical[att - GL_COLOR_ATTACHMENT0] = rb;
}
static void fake_fblayer(GLenum, GLenum att, GLuint tex, GLint, GLint) {
    physical[att - GL_COLOR_ATTACHMENT0] = tex;
}
static void fake_fbtex(GLenum, GLenum att, GLuint tex, GLint) {
    physical[att - GL_COLOR_ATTACHMENT0] = tex;
}
static void fake_drawbuffers(GLsizei n, const GLenum* b) { draw_list.assign(b, b + n); }
static void fake_getintegerv(GLenum pname, GLint* v) {
    if (pname == GL_VIEWPORT) {
        std::copy(driver_viewport.begin(), driver_viewport.end(), v);
        return;
    }
    *v = (pname == GL_MAX_COLOR_ATTACHMENTS || pname == GL_MAX_DRAW_BUFFERS) ? 8 : 0;
}
static void fake_deletefb(GLsizei, const GLuint*) {}
static void fake_readbuffer(GLenum) {}
static GLenum fake_checkfb(GLenum) { return GL_FRAMEBUFFER_COMPLETE; }
static GLenum fake_geterror() { return GL_NO_ERROR; }
static void fake_blit(GLint,GLint,GLint,GLint,GLint,GLint,GLint,GLint,GLbitfield,GLenum) {}
static void fake_invalidate(GLenum, GLsizei n, const GLenum* a) { invalidated.assign(a, a + n); }
static void fake_invalidate_sub(GLenum, GLsizei n, const GLenum* a, GLint, GLint, GLsizei, GLsizei) {
    invalidated.assign(a, a + n);
}

static int fails = 0;
static void expect(const char* what, GLuint got, GLuint want) {
    if (got != want) { printf("  FAIL %-50s physical=%u expected=%u\n", what, got, want); ++fails; }
}

int main() {
    GLES.glBindFramebuffer       = fake_bind_fb;
    GLES.glViewport              = fake_viewport;
    GLES.glFramebufferTexture2D  = fake_fbtex2d;
    GLES.glFramebufferRenderbuffer = fake_fbrb;
    GLES.glFramebufferTextureLayer = fake_fblayer;
    GLES.glFramebufferTexture    = fake_fbtex;
    GLES.glDrawBuffers           = fake_drawbuffers;
    GLES.glGetIntegerv           = fake_getintegerv;
    GLES.glDeleteFramebuffers    = fake_deletefb;
    GLES.glReadBuffer            = fake_readbuffer;
    GLES.glCheckFramebufferStatus= fake_checkfb;
    GLES.glGetError              = fake_geterror;
    GLES.glBlitFramebuffer       = fake_blit;
    GLES.glInvalidateFramebuffer = fake_invalidate;
    GLES.glInvalidateSubFramebuffer = fake_invalidate_sub;

    glBindFramebuffer(GL_FRAMEBUFFER, 7);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 100, 0); // colortex0
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, 101, 0); // colortex1
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, 102, 0); // colortex2

    printf("1. identity order leaves everything alone\n");
    { GLenum b[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1}; glDrawBuffers(2, b); }
    expect("slot0 holds colortex0", physical[0], 100);
    expect("slot1 holds colortex1", physical[1], 101);

    printf("2. swap: slot0 must receive colortex1, slot1 colortex0\n");
    { GLenum b[] = {GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT0}; glDrawBuffers(2, b); }
    expect("slot0 holds colortex1", physical[0], 101);
    expect("slot1 holds colortex0", physical[1], 100);

    printf("3. back to identity -- the regression a75f818 half-fixed\n");
    { GLenum b[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1}; glDrawBuffers(2, b); }
    expect("slot0 back to colortex0", physical[0], 100);
    expect("slot1 back to colortex1", physical[1], 101);

    printf("4. single non-identity target -- the destination-victim case\n");
    { GLenum b[] = {GL_COLOR_ATTACHMENT1}; glDrawBuffers(1, b); }
    expect("slot0 holds colortex1", physical[0], 101);
    { GLenum b[] = {GL_COLOR_ATTACHMENT0}; glDrawBuffers(1, b); }
    expect("slot0 back to colortex0", physical[0], 100);
    expect("slot1 back to colortex1", physical[1], 101);

    printf("5. shuffle -> different shuffle, without an identity in between\n");
    { GLenum b[] = {GL_COLOR_ATTACHMENT2}; glDrawBuffers(1, b); }
    expect("slot0 holds colortex2", physical[0], 102);
    { GLenum b[] = {GL_COLOR_ATTACHMENT1}; glDrawBuffers(1, b); }
    expect("slot0 holds colortex1", physical[0], 101);
    expect("slot2 back to colortex2", physical[2], 102);

    printf("5b. invalidation follows the logical attachment through a shuffle\n");
    invalidated.clear();
    { GLenum a[] = {GL_COLOR_ATTACHMENT1}; glInvalidateFramebuffer(GL_DRAW_FRAMEBUFFER, 1, a); }
    expect("invalidate logical COLOR1 reaches physical COLOR0",
           invalidated.empty() ? 0xffffu : static_cast<GLuint>(invalidated[0] - GL_COLOR_ATTACHMENT0), 0);
    invalidated.clear();
    { GLenum a[] = {GL_COLOR_ATTACHMENT1}; glInvalidateSubFramebuffer(GL_DRAW_FRAMEBUFFER, 1, a, 0, 0, 1, 1); }
    expect("sub-invalidate logical COLOR1 reaches physical COLOR0",
           invalidated.empty() ? 0xffffu : static_cast<GLuint>(invalidated[0] - GL_COLOR_ATTACHMENT0), 0);

    printf("6. a renderbuffer attachment survives a call that must not move it\n");
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, 900);
    { GLenum b[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1}; glDrawBuffers(2, b); }
    expect("slot0 still the renderbuffer", physical[0], 900);

    printf("7. a record survives the table growing under it\n");
    // Every scenario above uses one framebuffer, so until here nothing had ever
    // made the table grow. Sixty-four more names take it through several
    // rehashes and then come back to fbo 7, which must still know its home
    // attachments.
    //
    // This does not reproduce a reference held across an insert -- the case the
    // unique_ptr in the table exists for. It passes with or without it, because
    // no path in framebuffer.cpp currently keeps a framebuffer_t& while asking
    // for a different name; the pointer there is precaution, not load-bearing.
    // What this does catch is a record being lost or corrupted by the growth
    // itself, which nothing else covers.
    for (GLuint extra = 100; extra < 164; ++extra) {
        glBindFramebuffer(GL_FRAMEBUFFER, extra);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 7000 + extra, 0);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 7);
    physical.clear();
    { GLenum b[] = {GL_COLOR_ATTACHMENT1}; glDrawBuffers(1, b); }
    expect("slot0 holds colortex1 after 64 inserts", physical[0], 101);
    { GLenum b[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1}; glDrawBuffers(2, b); }
    // Scenario 6 left a renderbuffer on attachment 0, so that is what home means
    // here -- and getting it back is the whole point: the record still knows.
    expect("slot0 restored to the renderbuffer", physical[0], 900);
    expect("slot1 restored to colortex1", physical[1], 101);

    printf("8. deleting the framebuffer drops its record\n");
    { GLuint fb = 7; glDeleteFramebuffers(1, &fb); }
    glBindFramebuffer(GL_FRAMEBUFFER, 7);          // driver recycles the name
    physical.clear();
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, 555);
    { GLenum b[] = {GL_COLOR_ATTACHMENT0}; glDrawBuffers(1, b); }
    expect("recycled name did not inherit colortex0", physical[0], 555);

    printf("9. republishing a replaced draw binding moves both trackers\n");
    // FSR1's RecreateFSRTargets deletes its render FBO and creates a new one
    // behind this file's back. gl_state->current_draw_fbo and the per-context
    // draw binding must both name the replacement; if only one does, later
    // attachment and draw-buffer calls index a deleted name.
    FSR1_Context::g_renderFBO = 0;
    FSR1_Context::g_dirty = false;
    glBindFramebuffer(GL_FRAMEBUFFER, 0); // no FSR target: this is the surface
    expect("bind 0 without an FSR target does not prime", FSR1_Context::g_dirty ? 1u : 0u, 0u);

    FSR1_Context::g_renderFBO = 42;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);   // redirected to 42
    expect("redirect is the tracked draw binding", gl_state->current_draw_fbo, 42);
    expect("redirect primes the render target", FSR1_Context::g_dirty ? 1u : 0u, 1u);
    glDrawBuffer(GL_NONE); // mark 42's record: all colour attachments are NONE
    expect("record 42 is all-none", mg_draw_framebuffer_all_none() ? 1u : 0u, 1u);

    mg_framebuffer_republish_draw_binding(43);
    expect("republish updates gl_state's tracker", gl_state->current_draw_fbo, 43);
    // A stale per-context tracker would still find record 42 and answer true.
    expect("republish updates the per-context tracker", mg_draw_framebuffer_all_none() ? 1u : 0u, 0u);

    mg_framebuffer_republish_draw_binding(0); // the failure path
    expect("failed rebuild republishes the surface", gl_state->current_draw_fbo, 0);
    FSR1_Context::g_renderFBO = 0;
    FSR1_Context::g_dirty = false;

    printf("10. scene bind restores the pre-scaled viewport; other binds leave it alone\n");
    // Iris 1.8.14 binds its scaled scene target without an explicit glViewport,
    // so the binding still carries the native 2880x1585 viewport. Conversion on
    // that bind is the pre-c3cf54c behavior; a native target, a larger atlas and
    // a menu rectangle must not move.
    global_settings.fsr3_setting = FSR3_Mode::Experimental;
    viewport_calls = scene_viewport_calls = adjust_probe_calls = native_viewport_calls = 0;
    fsr3_final_scope_active = false;
    target_extent[91] = {1694, 932};   // scaled scene RenderTarget
    target_extent[92] = {2880, 1585};  // native-sized target
    target_extent[149] = {4096, 4096}; // atlas larger than the surface

    driver_viewport = {0, 0, 2880, 1585};
    glBindFramebuffer(GL_FRAMEBUFFER, 91);
    expect("scaled scene target converts the native viewport",
           driver_viewport == std::array<GLint, 4>{0, 0, 1694, 932}, true);
    expect("scene conversion issues one glViewport", viewport_calls, 1);
    expect("scene conversion was applied", scene_viewport_calls, 1);

    driver_viewport = {0, 0, 2880, 1585};
    glBindFramebuffer(GL_FRAMEBUFFER, 92);
    expect("native target keeps the native viewport",
           driver_viewport == std::array<GLint, 4>{0, 0, 2880, 1585}, true);

    driver_viewport = {0, 0, 2880, 1585};
    glBindFramebuffer(GL_FRAMEBUFFER, 149);
    expect("atlas target keeps the native viewport",
           driver_viewport == std::array<GLint, 4>{0, 0, 2880, 1585}, true);
    expect("native and atlas targets are probed but not converted", scene_viewport_calls, 1);

    driver_viewport = {30, 40, 128, 256};
    glBindFramebuffer(GL_FRAMEBUFFER, 91);
    expect("non-full-surface viewport (menu) is not scaled",
           driver_viewport == std::array<GLint, 4>{30, 40, 128, 256}, true);
    expect("menu viewport is not converted", scene_viewport_calls, 1);
    expect("each draw-target bind ran the planner", adjust_probe_calls, 4);

    // Closing a 26.2 render pass binds zero while a shadow viewport is current.
    // That must inherit the existing viewport: there is no FBO-0 native reset.
    driver_viewport = {0, 0, 2048, 2048};
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    expect("binding zero preserves the shadow viewport",
           driver_viewport == std::array<GLint, 4>{0, 0, 2048, 2048}, true);
    expect("binding zero issues no native reset", native_viewport_calls, 0);
    expect("binding zero is not a scene bind", scene_viewport_calls, 1);

    // The read target never owns the draw viewport.
    driver_viewport = {0, 0, 2880, 1585};
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 91);
    expect("read-target bind preserves the viewport",
           driver_viewport == std::array<GLint, 4>{0, 0, 2880, 1585}, true);
    expect("read-target bind does not convert", scene_viewport_calls, 1);

    // The bridge sets the final output extent explicitly and brackets it with
    // the final scope; a bind inside that scope must not override it.
    fsr3_final_scope_active = true;
    driver_viewport = {0, 0, 2880, 1585};
    glBindFramebuffer(GL_FRAMEBUFFER, 91);
    expect("final scope preserves the bridge viewport",
           driver_viewport == std::array<GLint, 4>{0, 0, 2880, 1585}, true);
    expect("final scope suppresses conversion", scene_viewport_calls, 1);
    fsr3_final_scope_active = false;

    // FSR1's redirect of 0 onto its render FBO is not a scene target; the
    // viewport must survive it.
    FSR1_Context::g_renderFBO = 42;
    driver_viewport = {3, 5, 2048, 2048};
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    expect("FSR1 redirect preserves viewport", driver_viewport == std::array<GLint, 4>{3, 5, 2048, 2048}, true);
    expect("FSR1 redirect keeps physical draw binding", bound_draw_fb, 42);
    expect("FSR1 redirect keeps original read binding", bound_read_fb, 0);
    expect("binding issues no native viewport reset", native_viewport_calls, 0);
    expect("only the scene bind converted in this scenario", scene_viewport_calls, 1);
    expect("only the scene bind called glViewport", viewport_calls, 1);
    expect("FSR1 redirect also ran the planner", adjust_probe_calls, 5);
    FSR1_Context::g_renderFBO = 0;

    printf("\n%s (%d failures)\n", fails ? "FAILED" : "all checks passed", fails);
    return fails != 0;
}
