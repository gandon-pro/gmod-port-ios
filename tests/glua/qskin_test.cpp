#include "glua_runtime.h"
#include <cassert>
#include <fstream>
#include <iterator>
#include <iostream>
#include <cmath>
struct SkinHost{
 int quads=0;double firstU=0,firstV=0;
 static int Invoke(lua_State *L){auto &self=*static_cast<SkinHost *>(lua_touserdata(L,lua_upvalueindex(1)));const std::string op=luaL_checkstring(L,1);
  if(op=="Width"||op=="Height"){lua_pushinteger(L,512);return 1;}
  if(op=="Pixel"){for(int i=0;i<4;i++)lua_pushinteger(L,255);return 4;}
  if(op=="Color")return 0;
  if(op=="Quad"){
   for(int i=2;i<=9;i++)assert(std::isfinite(luaL_checknumber(L,i)));
   double w=lua_tonumber(L,4),h=lua_tonumber(L,5);assert(w>=0&&h>=0);
   for(int i=6;i<=9;i++)assert(lua_tonumber(L,i)>=0&&lua_tonumber(L,i)<=1);
   if(self.quads==0){self.firstU=lua_tonumber(L,6);self.firstV=lua_tonumber(L,7);}self.quads++;return 0;
  }
  return luaL_error(L,"Unexpected skin operation");
 }
};
int main(int argc,char **argv){
 if(argc!=3)return 2;SkinHost host;std::string corpus=argv[1],port=argv[2];
 glua::RuntimeConfig cfg{glua::REALM_CLIENT,[&](const std::string &p,std::string &s){std::ifstream f(p.rfind("port/",0)==0?port+"/"+p.substr(5):corpus+"/lua/"+p,std::ios::binary);if(!f)return false;s.assign(std::istreambuf_iterator<char>(f),{});return true;},[](const std::string &s){std::cerr<<s<<'\n';},[&](lua_State *L){lua_newtable(L);lua_pushlightuserdata(L,&host);lua_pushcclosure(L,SkinHost::Invoke,1);lua_setfield(L,-2,"Invoke");lua_setglobal(L,"qskin");}};
 glua::Runtime vm(cfg);assert(vm.File("port/qskin.lua"));
 assert(vm.String("qskin.Draw('TabT_Active',0,0,100,28)","original tab"));assert(host.quads==9&&host.firstU==0&&host.firstV==384.0/512);
 for(const char *slot:{"TabT_Inactive","Tab_Control","Tree","CategoryList.Header","Selection"}){
  std::string call="qskin.Draw('"+std::string(slot)+"',0,0,5,5)";assert(vm.String(call,"small original skin"));
 }
 assert(!vm.String("qskin.Draw('missing',0,0,10,10)","unknown texture"));assert(lua_gettop(vm.State())==0);
 std::cout<<"PASS: original default.lua and derma_gwen.lua, nine-slice UVs, small-panel clamping and errors\n";
}
