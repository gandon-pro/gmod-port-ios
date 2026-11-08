#include "gamma_pass.h"
#include <cstdio>

namespace gmp_angle {
namespace {
// The engine caches GL state. Restore every state touched by this pass.
struct SavedState {
    GLint program,vao,readFbo,drawFbo,activeTexture,unpackBuffer,unpackAlignment,unpackRowLength,unpackSkipRows,unpackSkipPixels;
    GLint viewport[4],textures[2],samplers[2];
    GLboolean mask[4];
    static constexpr GLenum caps[]={GL_BLEND,GL_DEPTH_TEST,GL_STENCIL_TEST,GL_SCISSOR_TEST,GL_CULL_FACE,
        GL_RASTERIZER_DISCARD,GL_SAMPLE_ALPHA_TO_COVERAGE,GL_SAMPLE_COVERAGE,GL_DITHER};
    GLboolean enabled[sizeof(caps)/sizeof(caps[0])];
    SavedState(){
        glGetIntegerv(GL_CURRENT_PROGRAM,&program);glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&vao);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&readFbo);glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&drawFbo);
        glGetIntegerv(GL_ACTIVE_TEXTURE,&activeTexture);glGetIntegerv(GL_VIEWPORT,viewport);
        glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING,&unpackBuffer);
        glGetIntegerv(GL_UNPACK_ALIGNMENT,&unpackAlignment);glGetIntegerv(GL_UNPACK_ROW_LENGTH,&unpackRowLength);
        glGetIntegerv(GL_UNPACK_SKIP_ROWS,&unpackSkipRows);glGetIntegerv(GL_UNPACK_SKIP_PIXELS,&unpackSkipPixels);
        glGetBooleanv(GL_COLOR_WRITEMASK,mask);
        for(int i=0;i<2;++i){glActiveTexture(GL_TEXTURE0+i);glGetIntegerv(GL_TEXTURE_BINDING_2D,&textures[i]);glGetIntegerv(GL_SAMPLER_BINDING,&samplers[i]);}
        for(unsigned i=0;i<sizeof(caps)/sizeof(caps[0]);++i){enabled[i]=glIsEnabled(caps[i]);glDisable(caps[i]);}
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);glPixelStorei(GL_UNPACK_ALIGNMENT,1);glPixelStorei(GL_UNPACK_ROW_LENGTH,0);
        glPixelStorei(GL_UNPACK_SKIP_ROWS,0);glPixelStorei(GL_UNPACK_SKIP_PIXELS,0);
        glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
    }
    ~SavedState(){
        glUseProgram(program);glBindVertexArray(vao);
        glBindFramebuffer(GL_READ_FRAMEBUFFER,readFbo);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,drawFbo);
        glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);glColorMask(mask[0],mask[1],mask[2],mask[3]);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER,unpackBuffer);glPixelStorei(GL_UNPACK_ALIGNMENT,unpackAlignment);
        glPixelStorei(GL_UNPACK_ROW_LENGTH,unpackRowLength);glPixelStorei(GL_UNPACK_SKIP_ROWS,unpackSkipRows);glPixelStorei(GL_UNPACK_SKIP_PIXELS,unpackSkipPixels);
        for(int i=0;i<2;++i){glActiveTexture(GL_TEXTURE0+i);glBindTexture(GL_TEXTURE_2D,textures[i]);glBindSampler(i,samplers[i]);}
        glActiveTexture(activeTexture);
        for(unsigned i=0;i<sizeof(caps)/sizeof(caps[0]);++i)if(enabled[i])glEnable(caps[i]);
    }
};
// Default-framebuffer read/draw selections belong to framebuffer zero, not to
// the engine's currently bound FBOs. Preserve them separately.
struct DefaultBuffers {
    GLint read,draw;
    DefaultBuffers(){
        glBindFramebuffer(GL_FRAMEBUFFER,0);glGetIntegerv(GL_READ_BUFFER,&read);glGetIntegerv(GL_DRAW_BUFFER0,&draw);
        glReadBuffer(GL_BACK);const GLenum back=GL_BACK;glDrawBuffers(1,&back);
    }
    ~DefaultBuffers(){glBindFramebuffer(GL_FRAMEBUFFER,0);glReadBuffer(read);const GLenum selected=draw;glDrawBuffers(1,&selected);}
};
const char* vertex=R"(#version 300 es
void main(){vec2 p=vec2(float((gl_VertexID<<1)&2),float(gl_VertexID&2));gl_Position=vec4(p*2.0-1.0,0.0,1.0);}
)";
const char* fragment=R"(#version 300 es
precision highp float;
uniform highp sampler2D frameTexture;
uniform highp sampler2D rampTexture;
out vec4 result;
float ramp(float v,int channel){float p=clamp(v,0.0,1.0)*255.0;int lo=int(floor(p));int hi=min(lo+1,255);
return mix(texelFetch(rampTexture,ivec2(lo,0),0)[channel],texelFetch(rampTexture,ivec2(hi,0),0)[channel],fract(p));}
void main(){vec4 c=texelFetch(frameTexture,ivec2(gl_FragCoord.xy),0);result=vec4(ramp(c.r,0),ramp(c.g,1),ramp(c.b,2),c.a);}
)";
}
bool GammaPass::SetRamp(const std::uint16_t* red,const std::uint16_t* green,const std::uint16_t* blue){
    if(!red||!green||!blue)return false;
    std::lock_guard<std::mutex> guard(mutex_);
    identity_=true;
    for(int i=0;i<256;++i){
        pending_[i*4]=red[i]/65535.0f;pending_[i*4+1]=green[i]/65535.0f;pending_[i*4+2]=blue[i]/65535.0f;pending_[i*4+3]=1.0f;
        identity_=identity_&&red[i]==i*257&&green[i]==i*257&&blue[i]==i*257;
    }
    dirty_=true;failed_=false;return true;
}
bool GammaPass::Fail(const char* message){std::snprintf(error_.data(),error_.size(),"%s",message);std::lock_guard<std::mutex> guard(mutex_);failed_=true;return false;}
GLuint GammaPass::Compile(GLenum type,const char* source){
    GLuint shader=glCreateShader(type);if(!shader){Fail("Cannot allocate gamma shader");return 0;}
    glShaderSource(shader,1,&source,nullptr);glCompileShader(shader);GLint okay=0;glGetShaderiv(shader,GL_COMPILE_STATUS,&okay);
    if(!okay){glGetShaderInfoLog(shader,error_.size(),nullptr,error_.data());glDeleteShader(shader);return 0;}return shader;
}
bool GammaPass::Init(){
    GLuint vs=Compile(GL_VERTEX_SHADER,vertex);if(!vs)return false;
    GLuint fs=Compile(GL_FRAGMENT_SHADER,fragment);if(!fs){glDeleteShader(vs);return false;}
    GLuint program=glCreateProgram();glAttachShader(program,vs);glAttachShader(program,fs);glLinkProgram(program);
    glDeleteShader(vs);glDeleteShader(fs);GLint okay=0;glGetProgramiv(program,GL_LINK_STATUS,&okay);
    if(!okay){glGetProgramInfoLog(program,error_.size(),nullptr,error_.data());glDeleteProgram(program);return false;}
    program_=program;glGenVertexArrays(1,&vao_);glGenFramebuffers(1,&fbo_);glGenTextures(1,&frame_);glGenTextures(1,&lut_);
    glUseProgram(program_);glUniform1i(glGetUniformLocation(program_,"frameTexture"),0);glUniform1i(glGetUniformLocation(program_,"rampTexture"),1);
    for(int unit=0;unit<2;++unit){glActiveTexture(GL_TEXTURE0+unit);glBindSampler(unit,0);glBindTexture(GL_TEXTURE_2D,unit?lut_:frame_);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);}
    return true;
}
bool GammaPass::Apply(int width,int height){
    if(width<=0||height<=0)return true;
    std::array<float,256*4> upload;
    bool dirty=false;
    {
        std::lock_guard<std::mutex> guard(mutex_);
        if(identity_||failed_)return true;
        dirty=dirty_||!program_;if(dirty){upload=pending_;dirty_=false;}
    }
    // Preserve error evidence instead of attributing an older engine error to this pass.
    for(int n=0;n<16;++n){GLenum prior=glGetError();if(prior==GL_NO_ERROR)break;std::fprintf(stderr,"GMP gamma: preceding engine GL error 0x%04x\n",prior);}
    SavedState saved;DefaultBuffers buffers;
    GLint encoding=0;glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,GL_BACK,GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING,&encoding);
    if(encoding!=GL_LINEAR)return Fail("Gamma pass needs a linear EGL presentation surface; refusing a double sRGB conversion");
    if(!program_&&!Init()){std::lock_guard<std::mutex> guard(mutex_);failed_=true;return false;}
    glActiveTexture(GL_TEXTURE0);glBindSampler(0,0);glBindTexture(GL_TEXTURE_2D,frame_);
    if(width!=width_||height!=height_){
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER,fbo_);glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,frame_,0);
        if(glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)return Fail("Incomplete gamma resolve framebuffer");
        width_=width;height_=height;
    }
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,fbo_);
    glBlitFramebuffer(0,0,width,height,0,0,width,height,GL_COLOR_BUFFER_BIT,GL_NEAREST);
    glActiveTexture(GL_TEXTURE1);glBindSampler(1,0);glBindTexture(GL_TEXTURE_2D,lut_);
    if(dirty)glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,256,1,0,GL_RGBA,GL_FLOAT,upload.data());
    GLenum problem=glGetError();
    if(problem!=GL_NO_ERROR){char message[96];std::snprintf(message,sizeof(message),"Gamma preparation GL error 0x%04x",problem);return Fail(message);}
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,0);glViewport(0,0,width,height);glUseProgram(program_);glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLES,0,3);
    problem=glGetError();
    if(problem!=GL_NO_ERROR){char message[96];std::snprintf(message,sizeof(message),"Gamma draw GL error 0x%04x",problem);return Fail(message);}
    if(dirty)std::fprintf(stderr,"GMP gamma: presentation LUT applied at %dx%d\n",width,height);
    return true;
}
void GammaPass::Release(){
    if(program_)glDeleteProgram(program_);
    if(vao_)glDeleteVertexArrays(1,&vao_);
    if(fbo_)glDeleteFramebuffers(1,&fbo_);
    if(frame_)glDeleteTextures(1,&frame_);
    if(lut_)glDeleteTextures(1,&lut_);
    program_=vao_=fbo_=frame_=lut_=0;width_=height_=0;
    std::lock_guard<std::mutex> guard(mutex_);dirty_=true;failed_=false;
}
}
