#include "../game/shared/gmod/glua_runtime.h"
#include "tier1/utlbuffer.h"
#include "html_panel.h"
#include <memory>
static bool GMP_MenuLuaRead(const std::string &path,std::string &out){CUtlBuffer data;const std::string file="lua/"+path;if(!g_pFullFileSystem->ReadFile(file.c_str(),"GAME",data))return false;out.assign(static_cast<const char *>(data.Base()),data.TellPut());return true;}
static int GMP_HTMLAsset(const char *path,void **bytes,size_t *size){
    std::string normalized;if(!glua::Runtime::Normalize("",path,normalized))return 0;
    CUtlBuffer data;if(!g_pFullFileSystem->ReadFile(normalized.c_str(),"GAME",data))return 0;
    *size=data.TellPut();*bytes=malloc(*size?*size:1);if(!*bytes)return 0;if(*size)memcpy(*bytes,data.Base(),*size);return 1;
}
class CGMPMenuLua {
public:
    void Init(){
        GMPHTML_SetAssetReader(GMP_HTMLAsset);
        glua::RuntimeConfig config{glua::REALM_MENU,GMP_MenuLuaRead,[](const std::string &line){Msg("GLua menu: %s\n",line.c_str());},{}};
        m_VM.reset(new glua::Runtime(config));
        m_Ready=m_VM->File("includes/init.lua")&&m_VM->File("menu/menu.lua");
        if(!m_Ready)Warning("GModPort original menu bootstrap blocked: %s. vgui/DHTML and engine bindings must be installed before the original menu can run.\n",m_VM->Error().c_str());
    }
    void Shutdown(){if(m_Ready)m_VM->Hook("ShutDown");m_Ready=false;m_VM.reset();GMPHTML_Shutdown();}
    void Tick(){if(!m_VM)Init();if(m_Ready&&!m_VM->Hook("Think"))m_Ready=false;}
    const char *Status()const{return m_VM?m_Ready?"original menu loaded":m_VM->Error().c_str():"not initialized";}
private:
    std::unique_ptr<glua::Runtime> m_VM;bool m_Ready=false;
};
static CGMPMenuLua s_GMPMenuLua;
CON_COMMAND(gmp_glua_menu_status,"Print native menu GLua bootstrap status"){Msg("GLua menu: %s\n",s_GMPMenuLua.Status());}

void GMP_ShutdownMenuLua(){s_GMPMenuLua.Shutdown();}
