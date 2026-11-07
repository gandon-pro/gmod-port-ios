// Native GLua states are owned by Source's game-system lifecycle.
#include "igamesystem.h"
#include "../gmod/glua_runtime.h"
#include "tier1/utlbuffer.h"
#include <memory>
static bool GMP_ReadLua(const std::string &path,std::string &out){
    CUtlBuffer data;
    std::string file=path.compare(0,10,"gamemodes/")==0?path:"lua/"+path;
    if(!filesystem->ReadFile(file.c_str(),"GAME",data))return false;
    out.assign(static_cast<const char *>(data.Base()),data.TellPut());return true;
}
class CGMPGLuaSystem : public CAutoGameSystemPerFrame {
public:
    CGMPGLuaSystem():CAutoGameSystemPerFrame("GModPortGLua"),m_Ready(false){}
    bool Init() override {
#ifdef CLIENT_DLL
        const glua::Realm realm=glua::REALM_CLIENT;
#else
        const glua::Realm realm=glua::REALM_SERVER;
#endif
        glua::RuntimeConfig config{realm,GMP_ReadLua,[](const std::string &line){Msg("GLua: %s\n",line.c_str());},{}};
        m_VM.reset(new glua::Runtime(config));
        m_Ready=m_VM->File("includes/init.lua");
        if(!m_Ready)Warning("GModPort GLua bootstrap blocked: %s. Native engine bindings are incomplete; retaining the existing sandbox.\n",m_VM->Error().c_str());
        // A binding failure must not abort the existing playable Source client.
        return true;
    }
    void Shutdown() override {
#ifdef CLIENT_DLL
        GMP_QMenuShutdown();
#endif
if(m_Ready)m_VM->Hook("ShutDown");m_Ready=false;m_VM.reset();}
    void LevelInitPostEntity() override {if(m_Ready)m_VM->Hook("InitPostEntity");}
#ifdef CLIENT_DLL
    void Update(float) override {Tick();}
#else
    void FrameUpdatePostEntityThink() override {Tick();}
#endif
    const char *Status()const{return m_Ready?"includes/init.lua loaded":m_VM?m_VM->Error().c_str():"not initialized";}
private:
    void Tick(){if(m_Ready&&!m_VM->Hook("Think"))m_Ready=false;}
    std::unique_ptr<glua::Runtime> m_VM;bool m_Ready;
};
static CGMPGLuaSystem s_GMPGLuaSystem;
#ifdef CLIENT_DLL
CON_COMMAND(gmp_glua_client_status,"Print native client GLua bootstrap status"){Msg("GLua client: %s\n",s_GMPGLuaSystem.Status());}
#else
CON_COMMAND(gmp_glua_server_status,"Print native server GLua bootstrap status"){Msg("GLua server: %s\n",s_GMPGLuaSystem.Status());}
#endif
