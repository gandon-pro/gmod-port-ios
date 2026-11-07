#include "glua_runtime.h"
#include <cassert>
#include <map>
#include <iostream>
static int Realm(lua_State *L){lua_pushinteger(L,glua::GetRealm(L));return 1;}
static const glua::Reg registry[]={{"engine","realm",Realm,glua::REALM_ALL},{nullptr,nullptr,nullptr,0}};
GLUA_REGISTER(registry);
int main(){
 std::map<std::string,std::string> files={
 {"includes/init.lua","state=include('nested/a.lua'); assert(state==23)"},
 {"includes/nested/a.lua","return include('../value.lua')"},
 {"includes/value.lua","return 23"},
 {"bad.lua","error('real bootstrap failure')"}
 };
 for(auto realm:{glua::REALM_CLIENT,glua::REALM_SERVER,glua::REALM_MENU}){
  glua::RuntimeConfig cfg{realm,[&](const std::string &path,std::string &out){auto i=files.find(path);if(i==files.end())return false;out=i->second;return true;},[](const std::string &){},nullptr};
  glua::Runtime vm(cfg);assert(vm.String("assert(not pcall(require, 'ffi')); assert(jit==nil or jit.status()==false)","interpreter-only"));assert(vm.File("includes/init.lua"));assert(lua_gettop(vm.State())==0);
  assert(vm.String("assert(engine.realm()>0); local n=0; for i=1,5 do if i==3 then continue end n=n+i end assert(n!=0 && !false && n==12) // native GLua syntax\n/* multiline */","dialect"));
  assert(!vm.File("bad.lua"));assert(vm.Error().find("real bootstrap failure")!=std::string::npos);assert(lua_gettop(vm.State())==0);
  assert(!vm.File("../../outside.lua"));assert(lua_gettop(vm.State())==0);
  for(int i=0;i<100;++i){assert(!vm.String("error('x')","failure"));assert(lua_gettop(vm.State())==0);}
  assert(!vm.Hook("Think"));assert(vm.String("hook={Run=function(name) assert(name=='Think'); count=(count or 0)+1 end}","hook"));assert(vm.Hook("Think"));
  assert(vm.String("assert(count==1)","verify"));
 }
 glua::RuntimeConfig cfg{glua::REALM_CLIENT,{}, {}, {}};glua::Runtime fresh(cfg);assert(fresh.String("assert(count==nil and SERVER==false and CLIENT==true and MENU_DLL==false)","isolation"));
 std::string path;assert(glua::Runtime::Normalize("includes/","a/../b.lua",path)&&path=="includes/b.lua");assert(!glua::Runtime::Normalize("","/a",path));assert(!glua::Runtime::Normalize("","a:evil",path));
 std::cout<<"PASS: native GLua syntax, per-realm registration, isolation, includes, errors, hook dispatch and stack balance\n";
}
