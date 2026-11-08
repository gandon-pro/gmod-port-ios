#include "glua_runtime.h"
#include <cassert>
#include <fstream>
#include <iterator>
#include <iostream>
#include <cmath>
struct Host {
    int quads=0, texts=0, positions=0; double time=0, x=0,y=0;
    static int Invoke(lua_State *L){
        auto &h=*static_cast<Host *>(lua_touserdata(L,lua_upvalueindex(1)));
        std::string op=luaL_checkstring(L,1);
        if(op=="Time"){lua_pushnumber(L,h.time);return 1;}
        if(op=="Mode"){lua_pushliteral(L,"weld");return 1;}
        if(op=="GetTextureID"){assert(std::string(luaL_checkstring(L,2))=="models/weapons/v_toolgun/screen_bg");lua_pushinteger(L,7);return 1;}
        if(op=="CreateFont"){
            assert(std::string(luaL_checkstring(L,2))=="GModToolScreen");
            lua_getfield(L,3,"size");assert(lua_tointeger(L,-1)==60);lua_pop(L,1);
            lua_getfield(L,3,"weight");assert(lua_tointeger(L,-1)==900);lua_pop(L,1);return 0;
        }
        if(op=="GetTextSize"){assert(std::string(luaL_checkstring(L,2))=="#tool.weld.name");lua_pushinteger(L,100);lua_pushinteger(L,60);return 2;}
        if(op=="DrawTexturedRect"){assert(lua_tonumber(L,2)==0&&lua_tonumber(L,3)==0&&lua_tonumber(L,4)==256&&lua_tonumber(L,5)==256);h.quads++;return 0;}
        if(op=="SetTextPos"){
            double x=luaL_checknumber(L,2),y=luaL_checknumber(L,3);assert(std::isfinite(x)&&std::isfinite(y));
            double expected=-std::fmod(h.time*250,164)+(h.positions/2)*164;
            assert(std::abs(x-(expected+(h.positions%2==0?3:0)))<0.001);
            assert(y==74+(h.positions%2==0?3:0));h.positions++;return 0;
        }
        if(op=="DrawText"){assert(std::string(luaL_checkstring(L,2))=="#tool.weld.name");h.texts++;return 0;}
        if(op=="SetFont"||op=="SetTextColor"||op=="SetDrawColor"||op=="SetTexture")return 0;
        return luaL_error(L,"Unexpected operation: %s",op.c_str());
    }
};
int main(int argc,char **argv){
    if(argc!=3)return 2;Host h;std::string corpus=argv[1],port=argv[2];
    glua::RuntimeConfig cfg{glua::REALM_CLIENT,[&](const std::string &p,std::string &out){
        const bool native=p.rfind("gamemodes/",0)==0;
        std::ifstream f(native?corpus+"/"+p:port+"/"+(p.rfind("port/",0)==0?p.substr(5):p),std::ios::binary);
        if(!f)return false;out.assign(std::istreambuf_iterator<char>(f),{});return true;
    },[](const std::string &s){std::cerr<<s<<'\n';},[&](lua_State *L){lua_newtable(L);lua_pushlightuserdata(L,&h);lua_pushcclosure(L,Host::Invoke,1);lua_setfield(L,-2,"Invoke");lua_setglobal(L,"toolscreen");}};
    glua::Runtime vm(cfg);assert(vm.File("port/tool_screen.lua"));
    for(double time:{0.0,0.25,1.0,1234.5}){
        h.time=time;h.positions=h.texts=0;assert(vm.String("toolscreen.Paint()","tool screen frame"));
        assert(h.texts>=4&&h.texts%2==0&&h.positions==h.texts);assert(lua_gettop(vm.State())==0);
    }
    assert(h.quads==4);assert(!vm.String("render.PopRenderTarget()","underflow"));assert(lua_gettop(vm.State())==0);
    std::cout<<"PASS: unmodified cl_viewscreen.lua, 256x256 background, font, 250px/s scrolling, 3px shadows, balanced RT/camera and Lua stack\n";
}
