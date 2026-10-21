// Executes the production conversion shader on a host GLES provider. This is
// separate from run.sh because it requires an EGL/GLES 3.2 runtime, and does
// not qualify Android, MobileGlues wrapper state or Minecraft rendering.
// Compile this source with -I MobileGlues-cpp -lEGL -lGLESv2, then run it.
#define EGL_EGLEXT_PROTOTYPES 1
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl32.h>
#include "upscaling/fsr3_input_shader.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

static void require(bool value, const char* what) {
    if (!value) { std::fprintf(stderr, "FAIL %s (GL error 0x%x)\n", what, glGetError()); std::exit(1); }
}

int main() {
    EGLDisplay display = eglGetPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, nullptr);
    require(display != EGL_NO_DISPLAY && eglInitialize(display,nullptr,nullptr), "host EGL initialization");
    const EGLint config_attributes[] = {EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT,EGL_NONE};
    EGLConfig config;
    EGLint count = 0;
    require(eglChooseConfig(display,config_attributes,&config,1,&count) && count==1, "EGL config");
    const EGLint surface_attributes[] = {EGL_WIDTH,1,EGL_HEIGHT,1,EGL_NONE};
    EGLSurface surface = eglCreatePbufferSurface(display,config,surface_attributes);
    require(eglBindAPI(EGL_OPENGL_ES_API), "bind GLES API");
    const EGLint context_attributes[] = {EGL_CONTEXT_MAJOR_VERSION,3,EGL_CONTEXT_MINOR_VERSION,2,EGL_NONE};
    EGLContext context = eglCreateContext(display,config,EGL_NO_CONTEXT,context_attributes);
    require(context!=EGL_NO_CONTEXT && surface!=EGL_NO_SURFACE &&
            eglMakeCurrent(display,surface,surface,context), "GLES 3.2 context");
    std::printf("Host provider: %s; %s; %s\n", glGetString(GL_VENDOR), glGetString(GL_RENDERER), glGetString(GL_VERSION));

    GLuint shader = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(shader,1,&mg_fsr3_depth::kInputShader,nullptr);
    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);
    if (!ok) { char log[4096]{}; glGetShaderInfoLog(shader,sizeof(log),nullptr,log); std::fprintf(stderr,"%s\n",log); }
    require(ok,"production shader compile");
    GLuint program = glCreateProgram();
    glAttachShader(program,shader); glLinkProgram(program); glDeleteShader(shader);
    glGetProgramiv(program,GL_LINK_STATUS,&ok);
    require(ok,"production shader link");
    glUseProgram(program);
    const float identity[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    glUniformMatrix4fv(glGetUniformLocation(program,"uInvVP"),1,GL_FALSE,identity);
    glUniformMatrix4fv(glGetUniformLocation(program,"uPrevVP"),1,GL_FALSE,identity);
    glUniform2f(glGetUniformLocation(program,"uSize"),5,1);
    glUniform3f(glGetUniformLocation(program,"uCameraDelta"),0,0,0);
    glUniform1i(glGetUniformLocation(program,"uHasPrev"),0);
    glUniform1i(glGetUniformLocation(program,"uDepth"),0);

    GLuint textures[3]{};
    glGenTextures(3,textures);
    for (int i=0;i<3;++i) {
        glBindTexture(GL_TEXTURE_2D,textures[i]);
        glTexStorage2D(GL_TEXTURE_2D,1,i==2 ? GL_RGBA16F : GL_R32F,5,1);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    }
    const float samples[5] = {0,.125f,.5f,.875f,1};
    glBindTexture(GL_TEXTURE_2D,textures[0]);
    glTexSubImage2D(GL_TEXTURE_2D,0,0,0,5,1,GL_RED,GL_FLOAT,samples);
    glBindImageTexture(0,textures[2],0,GL_FALSE,0,GL_WRITE_ONLY,GL_RGBA16F);
    glBindImageTexture(1,textures[1],0,GL_FALSE,0,GL_WRITE_ONLY,GL_R32F);
    GLuint framebuffer = 0;
    glGenFramebuffers(1,&framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);
    for (int reverse=0;reverse<=1;++reverse) {
        glUniform1i(glGetUniformLocation(program,"uReverseDepth"),reverse);
        glDispatchCompute(1,1,1);
        glMemoryBarrier(GL_FRAMEBUFFER_BARRIER_BIT);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,textures[1],0);
        require(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"depth read framebuffer");
        float depth[5]{};
        glReadPixels(0,0,5,1,GL_RED,GL_FLOAT,depth);
        for (int i=0;i<5;++i) require(std::fabs(depth[i]-(reverse ? 1-samples[i] : samples[i]))<1e-6f,
                                     "forward/reverse depth conversion endpoints and intermediates");
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,textures[2],0);
        require(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"motion read framebuffer");
        float motion[20]{};
        glReadPixels(0,0,5,1,GL_RGBA,GL_FLOAT,motion);
        for (float component : motion) require(component==0.0f,"reset-only motion is zero");
        require(glGetError()==GL_NO_ERROR,"dispatch and readback errors");
    }
    glDeleteFramebuffers(1,&framebuffer);
    glDeleteTextures(3,textures);
    glDeleteProgram(program);
    eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
    eglDestroyContext(display,context); eglDestroySurface(display,surface); eglTerminate(display);
    std::puts("fsr3_depth_gpu: all conversion and zero-motion checks passed");
}
