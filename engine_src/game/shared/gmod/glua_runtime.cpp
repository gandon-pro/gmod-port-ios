#include "glua_runtime.h"
extern "C" {
#include "lualib.h"
}
#include <utility>
namespace glua {
static char s_RuntimeKey;
Runtime::Runtime(RuntimeConfig config):m_Config(std::move(config)){
    m_State=luaL_newstate();if(!m_State){m_Error="Cannot allocate Lua state";return;}
    luaL_openlibs(m_State);
    // Binary Lua modules cannot be loaded as iOS dylibs; native APIs use RegList.
    lua_getglobal(m_State,"package");lua_pushliteral(m_State,"");lua_setfield(m_State,-2,"cpath");lua_pop(m_State,1);
    lua_pushlightuserdata(m_State,&s_RuntimeKey);lua_pushlightuserdata(m_State,this);lua_rawset(m_State,LUA_REGISTRYINDEX);
    lua_pushcfunction(m_State,Include);lua_setglobal(m_State,"include");
    lua_pushcfunction(m_State,Print);lua_setglobal(m_State,"print");
    const char *names[]={"CLIENT","SERVER","MENU_DLL"};const Realm realms[]={REALM_CLIENT,REALM_SERVER,REALM_MENU};
    for(int i=0;i<3;++i){lua_pushboolean(m_State,m_Config.realm==realms[i]);lua_setglobal(m_State,names[i]);}
    Install(m_State,m_Config.realm);if(m_Config.install)m_Config.install(m_State);
}
Runtime::~Runtime(){if(m_State)lua_close(m_State);}
Runtime *Runtime::Self(lua_State *L){lua_pushlightuserdata(L,&s_RuntimeKey);lua_rawget(L,LUA_REGISTRYINDEX);Runtime *p=static_cast<Runtime *>(lua_touserdata(L,-1));lua_pop(L,1);return p;}
bool Runtime::Normalize(const std::string &base,const std::string &name,std::string &out){
    if(name.empty()||name[0]=='/'||name.find('\\')!=std::string::npos||name.find(':')!=std::string::npos||name.find('\0')!=std::string::npos)return false;
    std::string joined=base+name;std::vector<std::string> parts;
    for(size_t start=0;start<=joined.size();){size_t end=joined.find('/',start);if(end==std::string::npos)end=joined.size();std::string p=joined.substr(start,end-start);
        if(p==".."){if(parts.empty())return false;parts.pop_back();}else if(!p.empty()&&p!=".")parts.push_back(p);
        if(end==joined.size())break;
        start=end+1;
    }
    out.clear();for(const auto &p:parts){if(!out.empty())out+='/';out+=p;}return !out.empty();
}
int Runtime::LoadFile(const std::string &name){
    std::string path,code;std::string base;
    if(!m_Files.empty()){const auto slash=m_Files.back().find_last_of('/');if(slash!=std::string::npos)base=m_Files.back().substr(0,slash+1);}
    if(!Normalize(base,name,path)){lua_pushliteral(m_State,"Invalid Lua include path");return LUA_ERRFILE;}
    if(!m_Config.read||!m_Config.read(path,code)){
        if(!Normalize("",name,path)||!m_Config.read||!m_Config.read(path,code)){lua_pushfstring(m_State,"Lua file not found: %s",name.c_str());return LUA_ERRFILE;}
    }
    if(m_Files.size()>=128){lua_pushliteral(m_State,"Lua include depth exceeds 128");return LUA_ERRRUN;}
    int status=luaL_loadbuffer(m_State,code.data(),code.size(),("@"+path).c_str());if(status)return status;
    m_Files.push_back(path);status=lua_pcall(m_State,0,LUA_MULTRET,0);m_Files.pop_back();return status;
}
int Runtime::Include(lua_State *L){
    const char *name=luaL_checkstring(L,1);Runtime *self=Self(L);const int top=lua_gettop(L);
    // All C++ objects and include-stack guards are gone before lua_error longjmps.
    int status=self->LoadFile(name);if(status)return lua_error(L);return lua_gettop(L)-top;
}
int Runtime::Print(lua_State *L){Runtime *self=Self(L);std::string text;for(int i=1;i<=lua_gettop(L);++i){if(i>1)text+='\t';const char *s=lua_tostring(L,i);text+=s?s:lua_typename(L,lua_type(L,i));}if(self->m_Config.log)self->m_Config.log(text);return 0;}
void Runtime::ErrorFromStack(){const char *error=lua_tostring(m_State,-1);m_Error=error?error:"Non-string Lua error";if(m_Config.log)m_Config.log(m_Error);}
bool Runtime::File(const std::string &path){if(!m_State)return false;const int top=lua_gettop(m_State);int status=LoadFile(path);if(status)ErrorFromStack();else m_Error.clear();lua_settop(m_State,top);return status==0;}
bool Runtime::Call(int arguments,int results){int status=lua_pcall(m_State,arguments,results,0);if(status)ErrorFromStack();else m_Error.clear();return status==0;}
bool Runtime::String(const std::string &code,const std::string &name){if(!m_State)return false;const int top=lua_gettop(m_State);int status=luaL_loadbuffer(m_State,code.data(),code.size(),name.c_str());bool ok=false;if(status)ErrorFromStack();else ok=Call(0,0);lua_settop(m_State,top);return ok;}
bool Runtime::Hook(const char *name){if(!m_State)return false;int top=lua_gettop(m_State);lua_getglobal(m_State,"hook");if(!lua_istable(m_State,-1)){lua_settop(m_State,top);return false;}lua_getfield(m_State,-1,"Run");if(!lua_isfunction(m_State,-1)){lua_settop(m_State,top);return false;}lua_pushstring(m_State,name);bool ok=Call(1,0);lua_settop(m_State,top);return ok;}
}
