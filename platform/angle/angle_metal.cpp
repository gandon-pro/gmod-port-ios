#include "angle_metal.h"
#include <SDL.h>
#include <SDL_metal.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <EGL/eglext_angle.h>
#include <cstdio>

namespace gmp_angle {
namespace {
EGLDisplay display=EGL_NO_DISPLAY;
EGLSurface surface=EGL_NO_SURFACE;
EGLConfig config=nullptr;
struct Context { EGLContext handle; EGLSurface target; bool ownsSurface; };
SDL_MetalView view=nullptr;
thread_local char error[256]{};
bool Fail(const char* operation) {
    std::snprintf(error,sizeof(error),"%s failed (EGL 0x%04x)",operation,eglGetError());
    SDL_Log("ANGLE: %s",error); return false;
}
}
const char* Error() { return error; }
void Shutdown() {
    if (display!=EGL_NO_DISPLAY) {
        eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
        if (surface!=EGL_NO_SURFACE) eglDestroySurface(display,surface);
        eglTerminate(display);
    }
    surface=EGL_NO_SURFACE; display=EGL_NO_DISPLAY; config=nullptr;
    if (view) SDL_Metal_DestroyView(view);
    view=nullptr;
}
bool Initialize(SDL_Window* window) {
    if (display!=EGL_NO_DISPLAY) { SDL_strlcpy(error,"ANGLE already initialized",sizeof(error)); return false; }
    if (!window || !(SDL_GetWindowFlags(window)&SDL_WINDOW_METAL)) {
        SDL_strlcpy(error,"ANGLE requires an SDL Metal window",sizeof(error)); return false;
    }
    view=SDL_Metal_CreateView(window);
    if (!view) { SDL_strlcpy(error,SDL_GetError(),sizeof(error)); return false; }
    auto getDisplay=reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplayEXT"));
    if (!getDisplay) { Fail("eglGetPlatformDisplayEXT"); Shutdown(); return false; }
    const EGLint attributes[]={EGL_PLATFORM_ANGLE_TYPE_ANGLE,EGL_PLATFORM_ANGLE_TYPE_METAL_ANGLE,EGL_NONE};
    display=getDisplay(EGL_PLATFORM_ANGLE_ANGLE,EGL_DEFAULT_DISPLAY,attributes);
    if (display==EGL_NO_DISPLAY || !eglInitialize(display,nullptr,nullptr)) { Fail("eglInitialize Metal"); Shutdown(); return false; }
    if (!eglBindAPI(EGL_OPENGL_ES_API)) { Fail("eglBindAPI"); Shutdown(); return false; }
    const EGLint configAttributes[]={EGL_SURFACE_TYPE,EGL_WINDOW_BIT|EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT,
        EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_ALPHA_SIZE,8,EGL_DEPTH_SIZE,24,EGL_STENCIL_SIZE,8,EGL_NONE};
    EGLint count=0;
    if (!eglChooseConfig(display,configAttributes,&config,1,&count) || count!=1) { Fail("eglChooseConfig ES3"); Shutdown(); return false; }
    surface=eglCreateWindowSurface(display,config,reinterpret_cast<EGLNativeWindowType>(SDL_Metal_GetLayer(view)),nullptr);
    if (surface==EGL_NO_SURFACE) { Fail("eglCreateWindowSurface"); Shutdown(); return false; }
    error[0]=0; return true;
}
void* CreateContext(void* share) {
    if (display==EGL_NO_DISPLAY) { SDL_strlcpy(error,"ANGLE is not initialized",sizeof(error)); return nullptr; }
    // eglBindAPI is thread-local; worker contexts must bind the ES API too.
    if (!eglBindAPI(EGL_OPENGL_ES_API)) { Fail("eglBindAPI worker"); return nullptr; }
    const EGLint attributes[]={EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};
    EGLContext context=eglCreateContext(display,config,share?static_cast<Context*>(share)->handle:EGL_NO_CONTEXT,attributes);
    if (context==EGL_NO_CONTEXT) { Fail("eglCreateContext"); return nullptr; }
    EGLSurface target=surface;
    if (share) {
        const EGLint size[]={EGL_WIDTH,1,EGL_HEIGHT,1,EGL_NONE};
        target=eglCreatePbufferSurface(display,config,size);
        if (target==EGL_NO_SURFACE) { Fail("eglCreatePbufferSurface"); eglDestroyContext(display,context); return nullptr; }
    }
    return new Context{context,target,share!=nullptr};
}
bool MakeCurrent(void* context) {
    Context* ctx=static_cast<Context*>(context);
    const EGLSurface target=ctx?ctx->target:EGL_NO_SURFACE;
    return eglMakeCurrent(display,target,target,ctx?ctx->handle:EGL_NO_CONTEXT) || Fail("eglMakeCurrent");
}
void DestroyContext(void* context) {
    Context* ctx=static_cast<Context*>(context);
    if (!ctx) return;
    if (display!=EGL_NO_DISPLAY) {
        if (eglGetCurrentContext()==ctx->handle) MakeCurrent(nullptr);
        eglDestroyContext(display,ctx->handle);
        if (ctx->ownsSurface) eglDestroySurface(display,ctx->target);
    }
    delete ctx;
}
bool Present() { return eglSwapBuffers(display,surface) || Fail("eglSwapBuffers"); }
bool SwapInterval(int interval) { return eglSwapInterval(display,interval) || Fail("eglSwapInterval"); }
void* ProcAddress(const char* name) { return reinterpret_cast<void*>(eglGetProcAddress(name)); }
void DrawableSize(int* width,int* height) {
    EGLint w=0,h=0;
    if (display!=EGL_NO_DISPLAY && surface!=EGL_NO_SURFACE) {
        eglQuerySurface(display,surface,EGL_WIDTH,&w); eglQuerySurface(display,surface,EGL_HEIGHT,&h);
    }
    *width=w; *height=h;
}
}


// Source's SDL manager has one presentation context and shared worker contexts.
namespace { void* gmpPrimaryContext=nullptr; }
void* GMPAngleCreate(SDL_Window* window) {
    if (!gmpPrimaryContext) {
        if (!gmp_angle::Initialize(window)) { SDL_SetError("%s",gmp_angle::Error()); return nullptr; }
        gmpPrimaryContext=gmp_angle::CreateContext(nullptr);
        if (!gmpPrimaryContext) gmp_angle::Shutdown();
        return gmpPrimaryContext;
    }
    return gmp_angle::CreateContext(gmpPrimaryContext);
}
void GMPAngleDelete(void* context) {
    gmp_angle::DestroyContext(context);
    if (context==gmpPrimaryContext) gmpPrimaryContext=nullptr;
}
int GMPAngleCurrent(SDL_Window*,void* context) { return gmp_angle::MakeCurrent(context)?0:-1; }
void GMPAngleSwap(SDL_Window*) { if (!gmp_angle::Present()) SDL_Log("%s",gmp_angle::Error()); }
int GMPAngleInterval(int interval) { return gmp_angle::SwapInterval(interval)?0:-1; }
void GMPAngleShutdown() { gmp_angle::Shutdown(); gmpPrimaryContext=nullptr; }
