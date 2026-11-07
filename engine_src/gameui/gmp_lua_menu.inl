#include "../game/shared/gmod/glua_runtime.h"
#include "tier1/utlbuffer.h"
#include "html_panel.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <map>
#include <set>

static ConVar gmp_menu_language("gmod_language","en",FCVAR_ARCHIVE,"Original HTML menu language");
static bool GMP_MenuRead(const std::string &path,std::string &out){
    std::string normalized;if(!glua::Runtime::Normalize("",path,normalized))return false;
    CUtlBuffer data;if(!g_pFullFileSystem->ReadFile(normalized.c_str(),"GAME",data))return false;
    out.assign(static_cast<const char *>(data.Base()),data.TellPut());return true;
}
static bool GMP_MenuLuaRead(const std::string &path,std::string &out){return GMP_MenuRead("lua/"+path,out);}
static int GMP_HTMLAsset(const char *path,void **bytes,size_t *size){
    std::string data;if(!GMP_MenuRead(path,data))return 0;
    *size=data.size();*bytes=malloc(*size?*size:1);if(!*bytes)return 0;if(*size)memcpy(*bytes,data.data(),*size);return 1;
}
static bool GMP_MenuName(const char *s){
    if(!s||!*s)return false;
    for(;*s;++s)if(!((*s>='a'&&*s<='z')||(*s>='A'&&*s<='Z')||(*s>='0'&&*s<='9')||*s=='_'||*s=='-'))return false;
    return true;
}
static void GMP_MenuKV(lua_State *L,KeyValues *kv){
    lua_newtable(L);
    for(KeyValues *item=kv->GetFirstSubKey();item;item=item->GetNextKey()){
        if(item->GetFirstSubKey())GMP_MenuKV(L,item);else lua_pushstring(L,item->GetString());
        lua_setfield(L,-2,item->GetName());
    }
}
class CGMPMenuLua {
public:
    void Init(){
        m_Started=engine->Time();m_Failure.clear();
        if(!g_pFullFileSystem->FileExists("html/menu.html","GAME")){
            m_Failure="Missing original html/menu.html in installed game content";Warning("%s\n",m_Failure.c_str());m_Failed=true;return;
        }
        GMPHTML_SetAssetReader(GMP_HTMLAsset);
        glua::RuntimeConfig config{glua::REALM_MENU,GMP_MenuLuaRead,[](const std::string &line){Msg("GLua menu: %s\n",line.c_str());},[this](lua_State *L){
            lua_newtable(L);lua_pushlightuserdata(L,this);lua_pushcclosure(L,Invoke,1);lua_setfield(L,-2,"Invoke");lua_setglobal(L,"port");
        }};
        m_VM.reset(new glua::Runtime(config));
        m_Ready=m_VM->File("port/menu_bootstrap.lua");
        if(!m_Ready)Warning("GModPort original menu bootstrap failed: %s\n",m_VM->Error().c_str());
    }
    void Shutdown(){
        if(m_Ready)m_VM->String("port.MenuShutdown()","menu shutdown");
        m_Ready=false;m_VM.reset();m_Handles.clear();m_Main=0;m_Visible=false;m_DocumentReady=false;GMPHTML_Shutdown();
    }
    void Tick(bool visible){
        if(!m_VM&&!m_Failed)Init();
        if(m_Ready&&!m_DocumentReady&&engine->Time()-m_Started>12){m_Failure="Original HTML menu did not become ready within 12 seconds";m_Ready=false;Warning("%s\n",m_Failure.c_str());}
        if(m_Ready&&!m_VM->String("port.MenuTick()","menu frame")){m_Ready=false;Warning("GModPort menu stopped: %s\n",m_VM->Error().c_str());}
        const bool show=visible&&m_Ready&&!engine->IsDrawingLoadingImage();
        if(show!=m_Visible){m_Visible=show;for(uint64_t h:m_Handles)GMPHTML_Show(h,show);}
    }
    const char *Status()const{return !m_Failure.empty()?m_Failure.c_str():m_VM?m_Ready?m_DocumentReady?"original mainmenu.lua and HTML ready (menu-only bindings)":"waiting for original HTML controller":m_VM->Error().c_str():"not initialized";}
    bool DocumentReady()const{return m_Ready&&m_DocumentReady;}
    bool Ready()const{return m_Ready;}
private:
    static int Invoke(lua_State *L){return static_cast<CGMPMenuLua *>(lua_touserdata(L,lua_upvalueindex(1)))->Dispatch(L);}
    void Notify(const char *message){Warning("GModPort menu: %s\n",message);if(m_Main)GMPHTML_Notify(m_Main,message);}
    int Dispatch(lua_State *L){
        const char *op=luaL_checkstring(L,1);
        if(!strcmp(op,"Width")||!strcmp(op,"Height")){int w,h;engine->GetScreenSize(w,h);lua_pushinteger(L,!strcmp(op,"Width")?w:h);return 1;}
        if(!strcmp(op,"Time")){lua_pushnumber(L,engine->Time());return 1;}
        if(!strcmp(op,"Protocol")){lua_pushinteger(L,engine->GetProtocolVersion());return 1;}
        if(!strcmp(op,"Loading")){lua_pushboolean(L,engine->IsDrawingLoadingImage());return 1;}
        if(!strcmp(op,"ImageSize")){
            void *bytes=nullptr;size_t size=0;int w=0,h=0;
            if(GMP_HTMLAsset(luaL_checkstring(L,2),&bytes,&size))GMPHTML_ImageSize(bytes,size,&w,&h);
            free(bytes);lua_pushinteger(L,w);lua_pushinteger(L,h);return 2;
        }
        if(!strcmp(op,"InGame")){lua_pushboolean(L,GameUI().IsInLevel()&&!GameUI().IsInBackgroundLevel());return 1;}
        if(!strcmp(op,"ConVar")){ConVarRef cv(luaL_checkstring(L,2));if(cv.IsValid())lua_pushstring(L,cv.GetString());else lua_pushnil(L);return 1;}
        if(!strcmp(op,"Read")||!strcmp(op,"DataRead")){
            const char *path=luaL_checkstring(L,2);
            if(!strcmp(op,"DataRead")&&!GMP_MenuNameBase(path))return luaL_error(L,"Invalid menu data filename");
            std::string data;const std::string file=!strcmp(op,"DataRead")?"data/gmp_menu/"+std::string(path):path;
            if(GMP_MenuRead(file,data))lua_pushlstring(L,data.data(),data.size());else lua_pushnil(L);return 1;
        }
        if(!strcmp(op,"DataWrite")){
            const char *name=luaL_checkstring(L,2);size_t size;const char *data=luaL_checklstring(L,3,&size);
            if(!GMP_MenuNameBase(name)||size>1024*1024)return luaL_error(L,"Invalid menu data write");
            g_pFullFileSystem->CreateDirHierarchy("data/gmp_menu","MOD");
            std::string path="data/gmp_menu/"+std::string(name);CUtlBuffer buffer(0,0,0);buffer.Put(data,static_cast<int>(size));
            lua_pushboolean(L,g_pFullFileSystem->WriteFile(path.c_str(),"MOD",buffer));return 1;
        }
        if(!strcmp(op,"Exists")){std::string path;const char *input=luaL_checkstring(L,2);lua_pushboolean(L,glua::Runtime::Normalize("",input,path)&&g_pFullFileSystem->FileExists(path.c_str(),"GAME"));return 1;}
        if(!strcmp(op,"Find")){
            const char *pattern=luaL_checkstring(L,2);std::string safe;if(!glua::Runtime::Normalize("",pattern,safe))return luaL_error(L,"Invalid menu search path");
            std::set<std::string> files,dirs;FileFindHandle_t handle;
            const char *name=g_pFullFileSystem->FindFirstEx(safe.c_str(),"GAME",&handle);
            for(;name;name=g_pFullFileSystem->FindNext(handle))if(strcmp(name,".")&&strcmp(name,".."))(g_pFullFileSystem->FindIsDirectory(handle)?dirs:files).insert(name);
            if(handle!=FILESYSTEM_INVALID_FIND_HANDLE)g_pFullFileSystem->FindClose(handle);
            lua_newtable(L);int i=0;for(const auto &s:files){lua_pushstring(L,s.c_str());lua_rawseti(L,-2,++i);}
            lua_newtable(L);i=0;for(const auto &s:dirs){lua_pushstring(L,s.c_str());lua_rawseti(L,-2,++i);}return 2;
        }
        if(!strcmp(op,"KeyValues")){
            KeyValues *kv=new KeyValues("menu");if(!kv->LoadFromBuffer("menu",luaL_checkstring(L,2))){kv->deleteThis();lua_pushnil(L);return 1;}
            GMP_MenuKV(L,kv);kv->deleteThis();return 1;
        }
        if(!strcmp(op,"Phrase")){const char *key=luaL_checkstring(L,2);LoadLanguage();auto found=m_Phrases.find(key);lua_pushstring(L,found==m_Phrases.end()?key:found->second.c_str());return 1;}
        if(!strcmp(op,"Gamemodes")){
            lua_newtable(L);lua_newtable(L);
            lua_pushliteral(L,"sandbox");lua_setfield(L,-2,"name");lua_pushliteral(L,"Sandbox");lua_setfield(L,-2,"title");
            lua_pushliteral(L,"^gm_|^gmod_");lua_setfield(L,-2,"maps");lua_pushboolean(L,true);lua_setfield(L,-2,"menusystem");lua_rawseti(L,-2,1);return 1;
        }
        // No Steam mount/subscription provider exists in this Source port.
        if(!strcmp(op,"Addons")||!strcmp(op,"UserContent")||!strcmp(op,"Games")){lua_newtable(L);return 1;}
        if(!strcmp(op,"Command")){
            const char *name=luaL_checkstring(L,2);
            if(!GMP_MenuName(name))return luaL_error(L,"Invalid console command name");
            if(!strcmp(name,"gamemode")){if(strcmp(luaL_checkstring(L,3),"sandbox"))return luaL_error(L,"Only native sandbox is implemented");return 0;}
            if(!strcmp(name,"map")){
                const char *map=luaL_checkstring(L,3);if(!GMP_MenuName(map))return luaL_error(L,"Invalid map name");
                std::string path="maps/"+std::string(map)+".bsp";if(!g_pFullFileSystem->FileExists(path.c_str(),"GAME"))return luaL_error(L,"Map is not installed");
            }
            std::string command=name;
            for(int i=3;i<=lua_gettop(L);++i){const char *value=luaL_checkstring(L,i);if(strpbrk(value,"\"\r\n;"))return luaL_error(L,"Invalid console argument");command+=" \"";command+=value;command+='"';}
            command+='\n';engine->ClientCmd_Unrestricted(command.c_str());return 0;
        }
        if(!strcmp(op,"UICommand")){vgui::ivgui()->PostMessage(BasePanel()->GetVPanel(),new KeyValues("Command","command",luaL_checkstring(L,2)),NULL);return 0;}
        if(!strcmp(op,"Hide")){GameUI().HideGameUI();return 0;}
        if(!strcmp(op,"Activate")){GameUI().ActivateGameUI();return 0;}
        if(!strcmp(op,"ConsoleVisible")){lua_pushboolean(L,GameConsole().IsConsoleVisible());return 1;}
        if(!strcmp(op,"Console")){GameConsole().Activate();return 0;}
        if(!strcmp(op,"Sound")){vgui::surface()->PlaySound(luaL_checkstring(L,2));return 0;}
        if(!strcmp(op,"OpenURL")){GMPHTML_OpenURL(luaL_checkstring(L,2));return 0;}
        if(!strcmp(op,"OpenFolder")){Notify("Use Files > GModPort to open game folders on iOS.");return 0;}
        if(!strcmp(op,"Log")){Msg("GLua menu: %s\n",luaL_checkstring(L,2));return 0;}
        if(!strcmp(op,"Notify")){Notify(luaL_checkstring(L,2));return 0;}
        if(!strcmp(op,"HTMLCreate")){uint64_t h=GMPHTML_Create(luaL_checkstring(L,2));if(h){m_Handles.insert(h);m_Main=h;}lua_pushnumber(L,static_cast<lua_Number>(h));return 1;}
        if(!strcmp(op,"HTMLDestroy")){uint64_t h=static_cast<uint64_t>(luaL_checknumber(L,2));m_Handles.erase(h);GMPHTML_Destroy(h);if(m_Main==h)m_Main=0;return 0;}
        if(!strcmp(op,"BackgroundClear")){GMPHTML_Call(m_Main,"document.body.style.backgroundImage='none'");return 0;}
        if(!strcmp(op,"Background")){
            // Original ScreenshotScan may prefix ../ to address a path outside materials/.
            const char *input=luaL_checkstring(L,2);if(!strncmp(input,"../",3))input+=3;
            std::string path;if(!glua::Runtime::Normalize("",input,path))return luaL_error(L,"Invalid menu background");
            std::string js="document.body.style.backgroundImage='url(\"asset://garrysmod/";
            for(char c:path){if(c=='\''||c=='"'||c=='\\'||static_cast<unsigned char>(c)<32)return luaL_error(L,"Invalid background name");js+=c;}
            js+="\")';document.body.style.backgroundSize='cover';document.body.style.backgroundPosition='center';";
            GMPHTML_Call(m_Main,js.c_str());return 0;
        }
        const uint64_t h=static_cast<uint64_t>(luaL_checknumber(L,2));if(!m_Handles.count(h))return luaL_error(L,"Stale HTML panel");
        if(!strcmp(op,"BackgroundFrame")){GMPHTML_BackgroundFrame(h,luaL_checkstring(L,3),luaL_checkinteger(L,4),luaL_checkinteger(L,5));return 0;}
        if(!strcmp(op,"HTMLFunction")){GMPHTML_AddFunction(h,luaL_checkstring(L,3),luaL_checkstring(L,4));return 0;}
        if(!strcmp(op,"HTMLShow")){GMPHTML_Show(h,lua_toboolean(L,3)&&m_Visible);return 0;}
        if(!strcmp(op,"HTMLViewport")){GMPHTML_Viewport(h,luaL_checkinteger(L,3),luaL_checkinteger(L,4));return 0;}
        if(!strcmp(op,"HTMLCall")){GMPHTML_Call(h,luaL_checkstring(L,3));return 0;}
        if(!strcmp(op,"HTMLReply")){GMPHTML_Reply(h,static_cast<uint64_t>(luaL_checknumber(L,3)),luaL_checkstring(L,4));return 0;}
        if(!strcmp(op,"HTMLPoll")){char name[256],json[65536];int result=GMPHTML_Poll(h,name,sizeof(name),json,sizeof(json));if(result<0)return luaL_error(L,"HTML callback exceeds native queue limits");if(!result)return 0;if(!strcmp(name,"_gmp.Ready"))m_DocumentReady=true;lua_pushstring(L,name);lua_pushstring(L,json);return 2;}
        return luaL_error(L,"Unknown menu native operation: %s",op);
    }
    static bool GMP_MenuNameBase(const char *name){std::string normalized;return glua::Runtime::Normalize("",name,normalized)&&normalized==name&&normalized.find('/')==std::string::npos;}
    void LoadLanguage(){
        const char *requested=gmp_menu_language.GetString();if(m_Language==requested)return;
        m_Language=requested;m_Phrases.clear();
        for(const std::string &lang:{std::string("en"),m_Language}){
            if(!GMP_MenuName(lang.c_str()))continue;
            std::string data;if(!GMP_MenuRead("resource/localization/"+lang+"/main_menu.properties",data))continue;
            size_t at=0;while(at<data.size()){
                size_t end=data.find('\n',at);if(end==std::string::npos)end=data.size();std::string line=data.substr(at,end-at);at=end+1;
                if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty()||line[0]=='#')continue;
                size_t eq=line.find('=');if(eq!=std::string::npos)m_Phrases[line.substr(0,eq)]=line.substr(eq+1);
            }
        }
    }
    std::unique_ptr<glua::Runtime> m_VM;bool m_Ready=false,m_Visible=false,m_DocumentReady=false,m_Failed=false;double m_Started=0;std::string m_Failure;uint64_t m_Main=0;
    std::set<uint64_t> m_Handles;std::string m_Language;std::map<std::string,std::string> m_Phrases;
};
static CGMPMenuLua s_GMPMenuLua;
CON_COMMAND(gmp_glua_menu_status,"Print native menu GLua bootstrap status"){Msg("GLua menu: %s\n",s_GMPMenuLua.Status());}
void GMP_ShutdownMenuLua(){s_GMPMenuLua.Shutdown();}
