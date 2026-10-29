#pragma once
struct SDL_Window;
namespace gmp_angle {
// Window must have SDL_WINDOW_METAL; call from the render thread.
bool Initialize(SDL_Window* window);
void Shutdown();
void* CreateContext(void* share);
bool MakeCurrent(void* context);
void DestroyContext(void* context);
bool Present();
bool SwapInterval(int interval);
void* ProcAddress(const char* name);
void DrawableSize(int* width, int* height);
const char* Error();
}
