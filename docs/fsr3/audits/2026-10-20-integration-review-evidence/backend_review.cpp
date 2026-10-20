#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl32.h>
#include "gles/gles.h"
#include "upscaling/fsr3/fsr3_backend.h"
#include <cstdio>
#include <vector>
#include <string>
struct gles_func_t g_gles_func{};
static int pass=0;
static void traceDispatch(GLuint x, GLuint y, GLuint z) {
  GLint p=0,valid=0; glGetIntegerv(GL_CURRENT_PROGRAM,&p);
  glValidateProgram(p); glGetProgramiv(p,GL_VALIDATE_STATUS,&valid);
  if (!valid || pass % 8 == 4) {
    char log[2048]{}; glGetProgramInfoLog(p,sizeof(log),nullptr,log);
    printf("pass=%d program_validation=%d log=%s\n",pass,valid,log);
    GLint count=0; glGetProgramiv(p,GL_ACTIVE_UNIFORMS,&count);
    for (int i=0;i<count;++i) {
      char n[256]{}; GLenum type=0; GLint size=0;
      glGetActiveUniform(p,i,sizeof(n),nullptr,&size,&type,n);
      if(type==GL_SAMPLER_2D || type==GL_UNSIGNED_INT_SAMPLER_2D) {
        GLint unit=0; glGetUniformiv(p,glGetUniformLocation(p,n),&unit);
        GLint oldActive=0,bound=0,fmt=0; glGetIntegerv(GL_ACTIVE_TEXTURE,&oldActive); glActiveTexture(GL_TEXTURE0+unit); glGetIntegerv(GL_TEXTURE_BINDING_2D,&bound); if(bound) glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_INTERNAL_FORMAT,&fmt); glActiveTexture(oldActive); printf("  sampler type=0x%x unit=%d texture=%d format=0x%x %s\n",type,unit,bound,fmt,n);
      }
    }
  }
  printf("dispatch pass=%d groups=%u,%u,%u\n",pass++,x,y,z);
  glDispatchCompute(x,y,z);
}
static GLuint texture(GLenum fmt, GLenum datafmt, GLenum type, const void* data) {
 GLuint t=0; glGenTextures(1,&t);glBindTexture(GL_TEXTURE_2D,t);
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
 glTexStorage2D(GL_TEXTURE_2D,1,fmt,160,90);
 glTexSubImage2D(GL_TEXTURE_2D,0,0,0,160,90,datafmt,type,data);
 return t;
}
int main(){
 auto platform=reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplayEXT"));
 EGLDisplay d=platform(EGL_PLATFORM_SURFACELESS_MESA,EGL_DEFAULT_DISPLAY,nullptr);
 if(!eglInitialize(d,nullptr,nullptr)){printf("EGL init failed 0x%x\n",eglGetError());return 2;}
 eglBindAPI(EGL_OPENGL_ES_API);
 EGLint attrs[]={EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT,EGL_NONE};
 EGLConfig config;EGLint n=0;eglChooseConfig(d,attrs,&config,1,&n);
 EGLint ca[]={EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};
 EGLContext ctx=eglCreateContext(d,config,EGL_NO_CONTEXT,ca);
 if(!eglMakeCurrent(d,EGL_NO_SURFACE,EGL_NO_SURFACE,ctx)){printf("make current failed 0x%x\n",eglGetError());return 2;}
 printf("provider=%s\nversion=%s\n",glGetString(GL_RENDERER),glGetString(GL_VERSION));
    GLES.glActiveTexture=reinterpret_cast<decltype(GLES.glActiveTexture)>(eglGetProcAddress("glActiveTexture"));
    GLES.glAttachShader=reinterpret_cast<decltype(GLES.glAttachShader)>(eglGetProcAddress("glAttachShader"));
    GLES.glBindBuffer=reinterpret_cast<decltype(GLES.glBindBuffer)>(eglGetProcAddress("glBindBuffer"));
    GLES.glBindBufferBase=reinterpret_cast<decltype(GLES.glBindBufferBase)>(eglGetProcAddress("glBindBufferBase"));
    GLES.glBindBufferRange=reinterpret_cast<decltype(GLES.glBindBufferRange)>(eglGetProcAddress("glBindBufferRange"));
    GLES.glBindFramebuffer=reinterpret_cast<decltype(GLES.glBindFramebuffer)>(eglGetProcAddress("glBindFramebuffer"));
    GLES.glBindImageTexture=reinterpret_cast<decltype(GLES.glBindImageTexture)>(eglGetProcAddress("glBindImageTexture"));
    GLES.glBindSampler=reinterpret_cast<decltype(GLES.glBindSampler)>(eglGetProcAddress("glBindSampler"));
    GLES.glBindTexture=reinterpret_cast<decltype(GLES.glBindTexture)>(eglGetProcAddress("glBindTexture"));
    GLES.glBufferData=reinterpret_cast<decltype(GLES.glBufferData)>(eglGetProcAddress("glBufferData"));
    GLES.glBufferSubData=reinterpret_cast<decltype(GLES.glBufferSubData)>(eglGetProcAddress("glBufferSubData"));
    GLES.glCheckFramebufferStatus=reinterpret_cast<decltype(GLES.glCheckFramebufferStatus)>(eglGetProcAddress("glCheckFramebufferStatus"));
    GLES.glClearBufferfv=reinterpret_cast<decltype(GLES.glClearBufferfv)>(eglGetProcAddress("glClearBufferfv"));
    GLES.glClearBufferuiv=reinterpret_cast<decltype(GLES.glClearBufferuiv)>(eglGetProcAddress("glClearBufferuiv"));
    GLES.glClearColor=reinterpret_cast<decltype(GLES.glClearColor)>(eglGetProcAddress("glClearColor"));
    GLES.glColorMask=reinterpret_cast<decltype(GLES.glColorMask)>(eglGetProcAddress("glColorMask"));
    GLES.glCompileShader=reinterpret_cast<decltype(GLES.glCompileShader)>(eglGetProcAddress("glCompileShader"));
    GLES.glCreateProgram=reinterpret_cast<decltype(GLES.glCreateProgram)>(eglGetProcAddress("glCreateProgram"));
    GLES.glCreateShader=reinterpret_cast<decltype(GLES.glCreateShader)>(eglGetProcAddress("glCreateShader"));
    GLES.glCullFace=reinterpret_cast<decltype(GLES.glCullFace)>(eglGetProcAddress("glCullFace"));
    GLES.glDeleteBuffers=reinterpret_cast<decltype(GLES.glDeleteBuffers)>(eglGetProcAddress("glDeleteBuffers"));
    GLES.glDeleteFramebuffers=reinterpret_cast<decltype(GLES.glDeleteFramebuffers)>(eglGetProcAddress("glDeleteFramebuffers"));
    GLES.glDeleteProgram=reinterpret_cast<decltype(GLES.glDeleteProgram)>(eglGetProcAddress("glDeleteProgram"));
    GLES.glDeleteShader=reinterpret_cast<decltype(GLES.glDeleteShader)>(eglGetProcAddress("glDeleteShader"));
    GLES.glDeleteTextures=reinterpret_cast<decltype(GLES.glDeleteTextures)>(eglGetProcAddress("glDeleteTextures"));
    GLES.glDepthFunc=reinterpret_cast<decltype(GLES.glDepthFunc)>(eglGetProcAddress("glDepthFunc"));
    GLES.glDepthMask=reinterpret_cast<decltype(GLES.glDepthMask)>(eglGetProcAddress("glDepthMask"));
    GLES.glDisable=reinterpret_cast<decltype(GLES.glDisable)>(eglGetProcAddress("glDisable"));
    GLES.glDispatchCompute=reinterpret_cast<decltype(GLES.glDispatchCompute)>(eglGetProcAddress("glDispatchCompute"));
    GLES.glDrawBuffers=reinterpret_cast<decltype(GLES.glDrawBuffers)>(eglGetProcAddress("glDrawBuffers"));
    GLES.glEnable=reinterpret_cast<decltype(GLES.glEnable)>(eglGetProcAddress("glEnable"));
    GLES.glFramebufferTexture2D=reinterpret_cast<decltype(GLES.glFramebufferTexture2D)>(eglGetProcAddress("glFramebufferTexture2D"));
    GLES.glGenBuffers=reinterpret_cast<decltype(GLES.glGenBuffers)>(eglGetProcAddress("glGenBuffers"));
    GLES.glGenFramebuffers=reinterpret_cast<decltype(GLES.glGenFramebuffers)>(eglGetProcAddress("glGenFramebuffers"));
    GLES.glGenTextures=reinterpret_cast<decltype(GLES.glGenTextures)>(eglGetProcAddress("glGenTextures"));
    GLES.glGetActiveUniform=reinterpret_cast<decltype(GLES.glGetActiveUniform)>(eglGetProcAddress("glGetActiveUniform"));
    GLES.glGetBooleanv=reinterpret_cast<decltype(GLES.glGetBooleanv)>(eglGetProcAddress("glGetBooleanv"));
    GLES.glGetError=reinterpret_cast<decltype(GLES.glGetError)>(eglGetProcAddress("glGetError"));
    GLES.glGetFloatv=reinterpret_cast<decltype(GLES.glGetFloatv)>(eglGetProcAddress("glGetFloatv"));
    GLES.glGetIntegeri_v=reinterpret_cast<decltype(GLES.glGetIntegeri_v)>(eglGetProcAddress("glGetIntegeri_v"));
    GLES.glGetIntegerv=reinterpret_cast<decltype(GLES.glGetIntegerv)>(eglGetProcAddress("glGetIntegerv"));
    GLES.glGetProgramInfoLog=reinterpret_cast<decltype(GLES.glGetProgramInfoLog)>(eglGetProcAddress("glGetProgramInfoLog"));
    GLES.glGetProgramiv=reinterpret_cast<decltype(GLES.glGetProgramiv)>(eglGetProcAddress("glGetProgramiv"));
    GLES.glGetShaderInfoLog=reinterpret_cast<decltype(GLES.glGetShaderInfoLog)>(eglGetProcAddress("glGetShaderInfoLog"));
    GLES.glGetShaderiv=reinterpret_cast<decltype(GLES.glGetShaderiv)>(eglGetProcAddress("glGetShaderiv"));
    GLES.glGetUniformBlockIndex=reinterpret_cast<decltype(GLES.glGetUniformBlockIndex)>(eglGetProcAddress("glGetUniformBlockIndex"));
    GLES.glGetUniformLocation=reinterpret_cast<decltype(GLES.glGetUniformLocation)>(eglGetProcAddress("glGetUniformLocation"));
    GLES.glLinkProgram=reinterpret_cast<decltype(GLES.glLinkProgram)>(eglGetProcAddress("glLinkProgram"));
    GLES.glMemoryBarrier=reinterpret_cast<decltype(GLES.glMemoryBarrier)>(eglGetProcAddress("glMemoryBarrier"));
    GLES.glScissor=reinterpret_cast<decltype(GLES.glScissor)>(eglGetProcAddress("glScissor"));
    GLES.glShaderSource=reinterpret_cast<decltype(GLES.glShaderSource)>(eglGetProcAddress("glShaderSource"));
    GLES.glTexParameteri=reinterpret_cast<decltype(GLES.glTexParameteri)>(eglGetProcAddress("glTexParameteri"));
    GLES.glTexStorage2D=reinterpret_cast<decltype(GLES.glTexStorage2D)>(eglGetProcAddress("glTexStorage2D"));
    GLES.glTexSubImage2D=reinterpret_cast<decltype(GLES.glTexSubImage2D)>(eglGetProcAddress("glTexSubImage2D"));
    GLES.glUniform1i=reinterpret_cast<decltype(GLES.glUniform1i)>(eglGetProcAddress("glUniform1i"));
    GLES.glUniformBlockBinding=reinterpret_cast<decltype(GLES.glUniformBlockBinding)>(eglGetProcAddress("glUniformBlockBinding"));
    GLES.glUseProgram=reinterpret_cast<decltype(GLES.glUseProgram)>(eglGetProcAddress("glUseProgram"));
    GLES.glViewport=reinterpret_cast<decltype(GLES.glViewport)>(eglGetProcAddress("glViewport"));
 GLES.glDispatchCompute=traceDispatch;
 GLuint shader=glCreateShader(GL_COMPUTE_SHADER);
 const char* src="#version 320 es\nlayout(local_size_x=1) in; void main() {}";
 glShaderSource(shader,1,&src,nullptr);glCompileShader(shader);
 GLuint originalProgram=glCreateProgram();glAttachShader(originalProgram,shader);glLinkProgram(originalProgram);glUseProgram(originalProgram);
 GLuint originalTexture=0,originalUbo=0;glGenTextures(1,&originalTexture);glBindTexture(GL_TEXTURE_2D,originalTexture);
 glGenBuffers(1,&originalUbo);glBindBuffer(GL_UNIFORM_BUFFER,originalUbo);glBufferData(GL_UNIFORM_BUFFER,160,nullptr,GL_DYNAMIC_DRAW);
 mg_fsr3::Backend backend;
 bool init=backend.initialize(160,90,320,180);
 GLint actualP=0,actualT=0,actualU=0;glGetIntegerv(GL_CURRENT_PROGRAM,&actualP);glGetIntegerv(GL_TEXTURE_BINDING_2D,&actualT);glGetIntegerv(GL_UNIFORM_BUFFER_BINDING,&actualU);
 printf("initialize=%s error=%s\n",init?"PASS":"FAIL",backend.error().c_str());
 printf("state program %u->%d texture %u->%d ubo %u->%d\n",originalProgram,actualP,originalTexture,actualT,originalUbo,actualU);
 if(!init)return 3;
 std::vector<unsigned char> color(160*90*4,128); for(size_t i=3;i<color.size();i+=4) color[i]=255;
 std::vector<float> depth(160*90,0.5f),motion(160*90*2,0.0f);
 GLuint ct=texture(GL_RGBA8,GL_RGBA,GL_UNSIGNED_BYTE,color.data());
 GLuint dt=texture(GL_R32F,GL_RED,GL_FLOAT,depth.data());
 GLuint mt=texture(GL_RG16F,GL_RG,GL_FLOAT,motion.data());
 mg_fsr3::Inputs in{};in.color.handle=ct;in.depth.handle=dt;in.motion.handle=mt;
 in.render_width=160;in.render_height=90;in.output_width=320;in.output_height=180;
 in.depth_valid=true;in.motion_valid=true;in.jitter_x=0.25f;in.jitter_y=-0.25f;in.reset_history=true;
 bool result=backend.dispatch(in);
 printf("dispatch_result=%s error=%s passes=%d remaining_gl_error=0x%x\n",result?"PASS":"FAIL",backend.error().c_str(),pass,glGetError());
 backend.destroy();return result?0:4;
}
