#include "glua_api.h"
namespace glua {
static RegList *s_Head=nullptr;
static char s_RealmKey;
static PushObjectFn s_EntityPusher=nullptr;
RegList::RegList(const Reg *regs):m_pRegs(regs),m_pNext(s_Head){s_Head=this;}
Realm GetRealm(lua_State *L){lua_pushlightuserdata(L,&s_RealmKey);lua_rawget(L,LUA_REGISTRYINDEX);int value=lua_tointeger(L,-1);lua_pop(L,1);return static_cast<Realm>(value);}
void Install(lua_State *L,Realm realm){
    lua_pushlightuserdata(L,&s_RealmKey);lua_pushinteger(L,realm);lua_rawset(L,LUA_REGISTRYINDEX);
    for(RegList *list=s_Head;list;list=list->m_pNext)for(const Reg *r=list->m_pRegs;r->name;++r){
        if(!(r->realms&realm))continue;
        if(!r->lib){lua_pushcfunction(L,r->fn);lua_setglobal(L,r->name);continue;}
        if(r->lib[0]==':')luaL_newmetatable(L,r->lib+1);
        else {lua_getglobal(L,r->lib);if(lua_isnil(L,-1)){lua_pop(L,1);lua_newtable(L);lua_pushvalue(L,-1);lua_setglobal(L,r->lib);}}
        lua_pushcfunction(L,r->fn);lua_setfield(L,-2,r->name);lua_pop(L,1);
    }
}
void SetEntityPusher(PushObjectFn fn){s_EntityPusher=fn;}
void PushEntity(lua_State *L,void *entity){if(s_EntityPusher)s_EntityPusher(L,entity);else if(!entity)lua_pushnil(L);else luaL_error(L,"Entity binding is not installed");}
}
