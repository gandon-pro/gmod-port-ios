#pragma once
#include <GLES3/gl3.h>
#include <array>
#include <cstdint>
#include <mutex>

namespace gmp_angle {
// SetRamp may run before the GL context exists or on the engine thread.
// Apply and Release must run on the primary presentation context.
class GammaPass {
public:
    bool SetRamp(const std::uint16_t* red,const std::uint16_t* green,const std::uint16_t* blue);
    bool Apply(int width,int height);
    void Release();
    const char* Error()const{return error_.data();}
private:
    bool Init();
    bool Fail(const char* message);
    GLuint Compile(GLenum type,const char* source);
    std::mutex mutex_;
    std::array<float,256*4> pending_{};
    bool identity_=true,dirty_=true,failed_=false;
    GLuint program_=0,vao_=0,frame_=0,lut_=0,fbo_=0;
    int width_=0,height_=0;
    std::array<char,1024> error_{};
};
}
