#pragma once
#if defined(SOURCE_IOS) && defined(GMP_USE_ANGLE)
extern void* GMPAngleCreate(SDL_Window*);
extern void GMPAngleDelete(void*);
extern int GMPAngleCurrent(SDL_Window*,void*);
extern void GMPAngleSwap(SDL_Window*);
extern int GMPAngleInterval(int);
extern void GMPAngleShutdown();
#define SDL_GL_CreateContext GMPAngleCreate
#define SDL_GL_DeleteContext GMPAngleDelete
#define SDL_GL_MakeCurrent GMPAngleCurrent
#define SDL_GL_SwapWindow GMPAngleSwap
#define SDL_GL_SetSwapInterval GMPAngleInterval
#define SDL_GL_LoadLibrary(path) 0
#define SDL_GL_UnloadLibrary GMPAngleShutdown
#undef SDL_WINDOW_OPENGL
#define SDL_WINDOW_OPENGL SDL_WINDOW_METAL
#endif
