#include "../game/shared/gmod/glua_runtime.h"
#include "tier1/utlbuffer.h"
#include "html_panel.h"
#include "gmp_local_files.h"
#include "demofile/demoformat.h"
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
    // BasePanel can stay locally visible while its parent GameUI is hidden.
    // Engine activation events, including native Resume, own this overlay state.
    void SetActive(bool active){m_Active=active;if(!active)SetShown(false);}
    void SetShown(bool show){if(show==m_Visible)return;m_Visible=show;if(show)m_Started=engine->Time();for(uint64_t h:m_Handles)GMPHTML_Show(h,show);}
    void Tick(bool visible){
        const bool allowed=m_Active&&visible&&!engine->IsDrawingLoadingImage();
        // Do not create a hidden page whose original startup timers would expire
        // before the first real menu activation (e.g. direct map startup).
        if(!m_VM&&!m_Failed){if(!allowed)return;Init();}
        if(m_Ready&&!m_VM->String("port.MenuTick()","menu frame")){m_Ready=false;Warning("GModPort menu stopped: %s\n",m_VM->Error().c_str());}
        // Lua may call Hide during MenuTick. Read activation state after callbacks.
        SetShown(m_Active&&visible&&m_Ready&&!engine->IsDrawingLoadingImage());
        if(m_Visible&&m_Ready&&!m_DocumentReady&&engine->Time()-m_Started>12){m_Failure="Original HTML menu did not become ready within 12 visible seconds";m_Ready=false;SetShown(false);Warning("%s\n",m_Failure.c_str());}
    }

    const char *Status()const{return !m_Failure.empty()?m_Failure.c_str():m_VM?m_Ready?m_DocumentReady?"original mainmenu.lua and HTML ready (menu-only bindings)":"waiting for original HTML controller":m_VM->Error().c_str():"not initialized";}
    bool Visible()const{return m_Visible;}
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
        if(!strcmp(op,"LocalDelete")){
            const std::string path=luaL_checkstring(L,2);
            if(!gmp::LocalDeletable(path))return luaL_error(L,"Invalid local file deletion");
            if(engine->IsRecordingDemo()&&gmp::LocalFile(path,"demos",".dem"))return luaL_error(L,"Stop recording before deleting a demo");
            g_pFullFileSystem->RemoveFile(path.c_str(),"MOD");
            if(gmp::LocalFile(path,"dupes",".dupe")||gmp::LocalFile(path,"saves",".gms")||gmp::LocalFile(path,"demos",".dem")){
                const std::string stem=path.substr(0,path.find_last_of('.'));
                for(const char *extension:{".txt",".jpg",".png"})g_pFullFileSystem->RemoveFile((stem+extension).c_str(),"MOD");
            }
            lua_pushboolean(L,!g_pFullFileSystem->FileExists(path.c_str(),"MOD"));return 1;
        }
        if(!strcmp(op,"LocalAction")){
            const std::string action=luaL_checkstring(L,2),path=luaL_checkstring(L,3);
            std::string command;
            if(action=="DupeArm"||action=="SaveLoad"){
                const bool dupe=action=="DupeArm";
                if(!gmp::LocalFile(path,dupe?"dupes":"saves",dupe?".dupe":".gms")||!g_pFullFileSystem->FileExists(path.c_str(),"MOD"))return luaL_error(L,"Local file is missing or invalid");
                if(!GameUI().IsInLevel()||GameUI().IsInBackgroundLevel())return luaL_error(L,"Start a map before loading a dupe or save");
                const std::string argument=dupe?path:path.substr(6,path.size()-10);
                if(!dupe){
                    if(argument.size()>64)return luaL_error(L,"Save name is too long for this port");
                    for(unsigned char c:argument)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c==' '))return luaL_error(L,"Save name is not supported by the native loader");
                }
                // These existing server handlers consume ArgS, including spaces; do not add quotes to ArgS.
                command=(dupe?"gm_dupe_open ":"gm_load ")+argument+"\n";
            }else if(action=="DupeSave"){
                if(!GameUI().IsInLevel()||GameUI().IsInBackgroundLevel())return luaL_error(L,"Start a map and copy a contraption with the Duplicator first");
                command="gm_dupe_save\n";
            }else if(action=="DemoRecord"){
                if(!GameUI().IsInLevel()||GameUI().IsInBackgroundLevel()||engine->IsPlayingDemo())return luaL_error(L,"Start a playable map before recording");
                if(engine->IsRecordingDemo())return luaL_error(L,"Demo recording is already active");
                if(!GMP_MenuName(path.c_str()))return luaL_error(L,"Invalid recording name");
                std::string demo="demos/"+path+".dem";
                if(g_pFullFileSystem->FileExists(demo.c_str(),"MOD"))return luaL_error(L,"A demo with that name already exists");
                g_pFullFileSystem->CreateDirHierarchy("demos","MOD");command="record \"demos/"+path+"\"\n";
            }else if(action=="DemoStop"){
                if(!engine->IsRecordingDemo()){Notify("No demo recording is active.");return 0;}
                engine->ClientCmd_Unrestricted("stop\n");return 0;
            }else if(action=="DemoPlay"){
                if(!gmp::LocalFile(path,"demos",".dem"))return luaL_error(L,"Invalid local demo path");
                if(engine->IsRecordingDemo())return luaL_error(L,"Stop recording before starting playback");
                FileHandle_t file=g_pFullFileSystem->Open(path.c_str(),"rb","MOD");
                if(file==FILESYSTEM_INVALID_HANDLE)return luaL_error(L,"Demo is missing");
                demoheader_t header;int count=g_pFullFileSystem->Read(&header,sizeof(header),file);g_pFullFileSystem->Close(file);
                if(count!=sizeof(header)||memcmp(header.demofilestamp,DEMO_HEADER_ID,sizeof(header.demofilestamp)))return luaL_error(L,"Invalid or incomplete demo header");
                ByteSwap_demoheader_t(header);
                if(header.demoprotocol!=DEMO_PROTOCOL||header.networkprotocol!=engine->GetProtocolVersion())return luaL_error(L,"Demo protocol does not match this port build");
                command="playdemo \""+path+"\"\n";
            }else return luaL_error(L,"Unknown local library action");
            SetActive(false);GameUI().HideGameUI();engine->ClientCmd_Unrestricted(command.c_str());return 0;
        }
        if(!strcmp(op,"Find")){
            const char *pattern=luaL_checkstring(L,2);std::string safe;if(!glua::Runtime::Normalize("",pattern,safe))return luaL_error(L,"Invalid menu search path");
            const char *realm=luaL_optstring(L,3,"GAME");if(strcmp(realm,"GAME")&&strcmp(realm,"MOD"))return luaL_error(L,"Unsupported menu search path ID");
            const bool newest=!strcmp(luaL_optstring(L,4,"nameasc"),"datedesc");
            std::set<std::string> unique,dirs;FileFindHandle_t handle;
            const char *name=g_pFullFileSystem->FindFirstEx(safe.c_str(),realm,&handle);
            for(;name;name=g_pFullFileSystem->FindNext(handle))if(strcmp(name,".")&&strcmp(name,".."))(g_pFullFileSystem->FindIsDirectory(handle)?dirs:unique).insert(name);
            if(handle!=FILESYSTEM_INVALID_FIND_HANDLE)g_pFullFileSystem->FindClose(handle);
            std::vector<std::string> files(unique.begin(),unique.end());
            const size_t slash=safe.find_last_of('/');const std::string folder=slash==std::string::npos?"":safe.substr(0,slash+1);
            if(newest){
                std::map<std::string,long> times;
                for(const auto &file:files)times[file]=g_pFullFileSystem->GetFileTime((folder+file).c_str(),realm);
                std::sort(files.begin(),files.end(),[&](const std::string &a,const std::string &b){return times[a]==times[b]?a<b:times[a]>times[b];});
            }
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
            if(!strcmp(name,"gmod_language")){
                const char *value=luaL_checkstring(L,3);if(!GMP_MenuName(value))return luaL_error(L,"Invalid language name");
                gmp_menu_language.SetValue(value);return 0;
            }
            if(!strcmp(name,"gamemode")){if(strcmp(luaL_checkstring(L,3),"sandbox"))return luaL_error(L,"Only native sandbox is implemented");return 0;}
            if(!strcmp(name,"map")){
                const char *map=luaL_checkstring(L,3);if(!GMP_MenuName(map))return luaL_error(L,"Invalid map name");
                std::string path="maps/"+std::string(map)+".bsp";if(!g_pFullFileSystem->FileExists(path.c_str(),"GAME"))return luaL_error(L,"Map is not installed");
            }
            std::string command=name;
            for(int i=3;i<=lua_gettop(L);++i){const char *value=luaL_checkstring(L,i);if(strpbrk(value,"\"\r\n;"))return luaL_error(L,"Invalid console argument");command+=" \"";command+=value;command+='"';}
            command+='\n';if(!strcmp(name,"map"))SetActive(false);engine->ClientCmd_Unrestricted(command.c_str());return 0;
        }
        if(!strcmp(op,"UICommand")){vgui::ivgui()->PostMessage(BasePanel()->GetVPanel(),new KeyValues("Command","command",luaL_checkstring(L,2)),NULL);return 0;}
        if(!strcmp(op,"Hide")){SetActive(false);GameUI().HideGameUI();return 0;}
        if(!strcmp(op,"Activate")){SetActive(true);GameUI().ActivateGameUI();return 0;}
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
    std::unique_ptr<glua::Runtime> m_VM;bool m_Active=true,m_Ready=false,m_Visible=false,m_DocumentReady=false,m_Failed=false;double m_Started=0;std::string m_Failure;uint64_t m_Main=0;
    std::set<uint64_t> m_Handles;std::string m_Language;std::map<std::string,std::string> m_Phrases;
};
static CGMPMenuLua s_GMPMenuLua;
CON_COMMAND(gmp_glua_menu_status,"Print native menu GLua bootstrap status"){Msg("GLua menu: %s\n",s_GMPMenuLua.Status());}
void GMP_ShutdownMenuLua(){s_GMPMenuLua.Shutdown();}

void GMP_SetMenuActive(bool active){s_GMPMenuLua.SetActive(active);}
