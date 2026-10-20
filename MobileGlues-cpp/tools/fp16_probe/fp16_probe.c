#include <EGL/egl.h>
#include <GLES3/gl32.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
static GLuint build(const char* src,int mediump){
  GLuint s=glCreateShader(GL_COMPUTE_SHADER); glShaderSource(s,1,&src,NULL); glCompileShader(s);
  GLint ok=0; glGetShaderiv(s,GL_COMPILE_STATUS,&ok); if(!ok){char log[2048];glGetShaderInfoLog(s,sizeof log,NULL,log);printf("compile fail %s: %s\n",mediump?"mediump":"highp",log);return 0;}
  GLuint p=glCreateProgram(); glAttachShader(p,s); glLinkProgram(p); glGetProgramiv(p,GL_LINK_STATUS,&ok); if(!ok){printf("link fail\n");return 0;} glDeleteShader(s); return p;
}
static double run(GLuint p,GLuint ssbo){
  glUseProgram(p); glBindBufferBase(GL_SHADER_STORAGE_BUFFER,0,ssbo); glFinish();
  double t=now(); for(int i=0;i<20;i++){ glDispatchCompute(1,1,1); glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT); } glFinish(); return (now()-t)/20.0;
}
/*
 * X1E FP16 ALU probe. Compares the same dependent vec4 loop in highp
 * (FP32) and mediump (FP16) through the system EGL/GLES provider.
 * This is an ALU-rate probe, not a shaderpack benchmark.
 */
int main(){
  EGLDisplay d=eglGetDisplay(EGL_DEFAULT_DISPLAY);eglInitialize(d,NULL,NULL);
  EGLint ca[]={EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT,EGL_NONE};EGLConfig c;EGLint n;eglChooseConfig(d,ca,&c,1,&n);
  EGLint xa[]={EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};EGLContext x=eglCreateContext(d,c,EGL_NO_CONTEXT,xa);EGLint pb[]={EGL_WIDTH,16,EGL_HEIGHT,16,EGL_NONE};EGLSurface s=eglCreatePbufferSurface(d,c,pb);eglMakeCurrent(d,s,s,x);
  const char* hi="#version 310 es\nlayout(local_size_x=1) in;\nlayout(std430,binding=0) buffer O{vec4 v;}o;\nvoid main(){highp vec4 x=vec4(1.0);for(int i=0;i<200000;i++){x=x*1.000001+vec4(0.00001);}o.v=x;}\n";
  const char* me="#version 310 es\nprecision mediump float;\nlayout(local_size_x=1) in;\nlayout(std430,binding=0) buffer O{vec4 v;}o;\nvoid main(){mediump vec4 x=vec4(1.0);for(int i=0;i<200000;i++){x=x*1.000001+vec4(0.00001);}o.v=x;}\n";
  GLuint ph=build(hi,0), pm=build(me,1); GLuint b; glGenBuffers(1,&b); glBindBuffer(GL_SHADER_STORAGE_BUFFER,b); glBufferData(GL_SHADER_STORAGE_BUFFER,16,NULL,GL_DYNAMIC_COPY);
  if(ph) if(ph) printf("highp_ms=%.3f\n",run(ph,b)*1000.0);
  if(pm) printf("mediump_ms=%.3f\n",run(pm,b)*1000.0);
  return 0;
}
