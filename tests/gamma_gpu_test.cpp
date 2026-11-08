// Real GLES3 framebuffer test. Mesa surfaceless EGL on Linux; no mock GL calls.
#include <EGL/egl.h>
#include "gamma_pass.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <array>

static void Gradient(int width,int height){
    glBindFramebuffer(GL_FRAMEBUFFER,0);glDisable(GL_RASTERIZER_DISCARD);glEnable(GL_SCISSOR_TEST);glColorMask(1,1,1,1);
    for(int i=0;i<width;++i){float v=float(i)/float(width-1);glScissor(i,0,1,height);glClearColor(v,v,v,0.6f);glClear(GL_COLOR_BUFFER_BIT);}
    glDisable(GL_SCISSOR_TEST);
}
static void ReadAndCheck(int width,int height,bool identity){
    std::array<unsigned char,256*4*4> pixels{};
    glBindFramebuffer(GL_READ_FRAMEBUFFER,0);glReadBuffer(GL_BACK);glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
    for(int y=0;y<height;++y)for(int x=0;x<width;++x){
        int v=int(std::lround(255.0*x/(width-1)));int o=(y*width+x)*4;
        int expected[]={identity?v:255-v,identity?v:int(std::lround(double(v)*v/255)),v,153};
        for(int c=0;c<4;++c)if(std::abs(int(pixels[o+c])-expected[c])>1){
            std::fprintf(stderr,"Pixel mismatch (%d,%d) channel=%d actual=%d expected=%d\n",x,y,c,pixels[o+c],expected[c]);assert(false);
        }
    }
}
int main(){
    EGLDisplay display=eglGetPlatformDisplay(0x31DD /* EGL_PLATFORM_SURFACELESS_MESA */,EGL_DEFAULT_DISPLAY,nullptr);
    assert(display!=EGL_NO_DISPLAY&&eglInitialize(display,nullptr,nullptr));assert(eglBindAPI(EGL_OPENGL_ES_API));
    const EGLint attributes[]={EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT,
        EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_ALPHA_SIZE,8,EGL_NONE};
    EGLConfig config;EGLint count=0;assert(eglChooseConfig(display,attributes,&config,1,&count)&&count==1);
    const EGLint contextAttributes[]={EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};
    EGLContext context=eglCreateContext(display,config,EGL_NO_CONTEXT,contextAttributes);assert(context!=EGL_NO_CONTEXT);
    const EGLint size[]={EGL_WIDTH,256,EGL_HEIGHT,2,EGL_NONE};
    EGLSurface surface=eglCreatePbufferSurface(display,config,size);assert(surface!=EGL_NO_SURFACE);
    assert(eglMakeCurrent(display,surface,surface,context));
    std::printf("GLES renderer: %s\n",glGetString(GL_RENDERER));
    std::array<std::uint16_t,256> r,g,b,id;
    for(int i=0;i<256;++i){id[i]=i*257;r[i]=(255-i)*257;g[i]=std::lround(double(i)*i/65025*65535);b[i]=id[i];}
    gmp_angle::GammaPass gamma;assert(!gamma.SetRamp(nullptr,g.data(),b.data()));assert(gamma.SetRamp(r.data(),g.data(),b.data()));
    Gradient(256,2);
    GLuint fbo,tex,vao,pbo,samplers[2],sceneTextures[2];glGenFramebuffers(1,&fbo);glGenTextures(1,&tex);
    glBindTexture(GL_TEXTURE_2D,tex);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,8,8,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
    glBindFramebuffer(GL_FRAMEBUFFER,fbo);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,tex,0);
    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
    glGenVertexArrays(1,&vao);glBindVertexArray(vao);glViewport(3,7,40,17);glColorMask(0,1,0,1);
    glGenBuffers(1,&pbo);glBindBuffer(GL_PIXEL_UNPACK_BUFFER,pbo);glBufferData(GL_PIXEL_UNPACK_BUFFER,8192,nullptr,GL_STATIC_DRAW);
    glPixelStorei(GL_UNPACK_ALIGNMENT,8);glPixelStorei(GL_UNPACK_ROW_LENGTH,37);glPixelStorei(GL_UNPACK_SKIP_ROWS,2);glPixelStorei(GL_UNPACK_SKIP_PIXELS,2);
    glGenSamplers(2,samplers);glGenTextures(2,sceneTextures);
    for(int i=0;i<2;++i){glActiveTexture(GL_TEXTURE0+i);glBindTexture(GL_TEXTURE_2D,sceneTextures[i]);glBindSampler(i,samplers[i]);}
    glActiveTexture(GL_TEXTURE3);
    GLenum caps[]={GL_BLEND,GL_DEPTH_TEST,GL_STENCIL_TEST,GL_SCISSOR_TEST,GL_CULL_FACE,GL_RASTERIZER_DISCARD,
        GL_SAMPLE_ALPHA_TO_COVERAGE,GL_SAMPLE_COVERAGE,GL_DITHER};
    for(auto cap:caps)glEnable(cap);
    assert(glGetError()==GL_NO_ERROR);
    assert(gamma.Apply(256,2));assert(glGetError()==GL_NO_ERROR);
    GLint value;GLboolean mask[4];GLint viewport[4];
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&value);assert(value==int(fbo));glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&value);assert(value==int(fbo));
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&value);assert(value==int(vao));glGetIntegerv(GL_ACTIVE_TEXTURE,&value);assert(value==GL_TEXTURE3);
    glGetIntegerv(GL_VIEWPORT,viewport);assert(viewport[0]==3&&viewport[1]==7&&viewport[2]==40&&viewport[3]==17);
    glGetBooleanv(GL_COLOR_WRITEMASK,mask);assert(!mask[0]&&mask[1]&&!mask[2]&&mask[3]);
    glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING,&value);assert(value==int(pbo));glGetIntegerv(GL_UNPACK_ROW_LENGTH,&value);assert(value==37);
    glGetIntegerv(GL_UNPACK_SKIP_ROWS,&value);assert(value==2);glGetIntegerv(GL_UNPACK_SKIP_PIXELS,&value);assert(value==2);
    glGetIntegerv(GL_UNPACK_ALIGNMENT,&value);assert(value==8);
    for(int i=0;i<2;++i){glActiveTexture(GL_TEXTURE0+i);glGetIntegerv(GL_TEXTURE_BINDING_2D,&value);assert(value==int(sceneTextures[i]));glGetIntegerv(GL_SAMPLER_BINDING,&value);assert(value==int(samplers[i]));}
    for(auto cap:caps)assert(glIsEnabled(cap));
    ReadAndCheck(256,2,false);
    // A fresh neutral frame bypasses the pass after restoring the identity ramp.
    assert(gamma.SetRamp(id.data(),id.data(),id.data()));Gradient(256,2);assert(gamma.Apply(256,2));ReadAndCheck(256,2,true);
    // Reallocate the resolve texture after a drawable resize, then rebuild after cleanup.
    const EGLint resized[]={EGL_WIDTH,128,EGL_HEIGHT,4,EGL_NONE};EGLSurface newSurface=eglCreatePbufferSurface(display,config,resized);
    assert(newSurface!=EGL_NO_SURFACE&&eglMakeCurrent(display,newSurface,newSurface,context));eglDestroySurface(display,surface);
    for(auto cap:caps)glDisable(cap);
    assert(gamma.SetRamp(r.data(),g.data(),b.data()));Gradient(128,4);assert(gamma.Apply(128,4));ReadAndCheck(128,4,false);
    gamma.Release();Gradient(128,4);assert(gamma.Apply(128,4));ReadAndCheck(128,4,false);gamma.Release();
    assert(glGetError()==GL_NO_ERROR);
    eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);eglDestroyContext(display,context);eglDestroySurface(display,newSurface);eglTerminate(display);
    std::puts("Gamma GPU: per-channel LUT, alpha, identity, GL-state restoration, resize and cleanup passed");
}
