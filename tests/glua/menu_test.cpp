#include "glua_runtime.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <deque>
#include <set>
#include <sstream>
namespace fs=std::filesystem;
struct Host {
 fs::path corpus,port;
 double time=0;
 std::deque<std::pair<std::string,std::string>> incoming;
 std::vector<std::string> js,commands,errors,functions;
 std::map<std::string,std::string> data;
 bool destroyed=false,inGame=false;
 bool read(const std::string &p,std::string &out){
  fs::path file=p.rfind("port/",0)==0?port/p.substr(5):corpus/"lua"/p;
  std::ifstream f(file,std::ios::binary);if(!f)return false;out.assign(std::istreambuf_iterator<char>(f),{});return true;
 }
 static int Call(lua_State *L){return static_cast<Host *>(lua_touserdata(L,lua_upvalueindex(1)))->call(L);}
 int call(lua_State *L){
  const std::string op=luaL_checkstring(L,1);auto str=[&](int i){return std::string(luaL_checkstring(L,i));};
  if(op=="Width"||op=="Height"){lua_pushinteger(L,op=="Width"?1280:720);return 1;}
  if(op=="Time"){lua_pushnumber(L,time);return 1;}
  if(op=="Protocol"){lua_pushinteger(L,24);return 1;} // Test host value, not a target-build claim.
  if(op=="InGame"){lua_pushboolean(L,inGame);return 1;}
  if(op=="Hide"){commands.push_back("gameui_hide");return 0;}
  if(op=="Activate"){commands.push_back("gameui_activate");return 0;}
  if(op=="ConsoleVisible"||op=="Loading"){lua_pushboolean(L,false);return 1;}
  if(op=="ConVar"){lua_pushstring(L,str(2)=="gmod_language"?"en":"0");return 1;}
  if(op=="Read"){std::ifstream f(corpus/str(2),std::ios::binary);if(!f)lua_pushnil(L);else{std::string s((std::istreambuf_iterator<char>(f)),{});lua_pushlstring(L,s.data(),s.size());}return 1;}
  if(op=="Exists"){const std::string path=str(2);lua_pushboolean(L,fs::exists(corpus/path)||path=="maps/gm_flatgrass.bsp"||path=="maps/gm_construct.bsp");return 1;}
  if(op=="DataRead"){auto it=data.find(str(2));if(it==data.end())lua_pushnil(L);else lua_pushstring(L,it->second.c_str());return 1;}
  if(op=="DataWrite"){data[str(2)]=str(3);lua_pushboolean(L,true);return 1;}
  if(op=="Find"){
   lua_newtable(L);int n=0;auto add=[&](const std::string &s){lua_pushstring(L,s.c_str());lua_rawseti(L,-2,++n);};
   if(str(2)=="maps/*.bsp"){add("gm_construct.bsp");add("gm_flatgrass.bsp");}
   if(str(2)=="resource/localization/*.png")add("en.png");
   lua_newtable(L);return 2;
  }
  if(op=="KeyValues"){lua_newtable(L);return 1;}
  if(op=="Gamemodes"){
   lua_newtable(L);lua_newtable(L);for(const auto &p:std::map<std::string,std::string>{{"name","sandbox"},{"title","Sandbox"},{"maps","^gm_|^gmod_"}}){lua_pushstring(L,p.second.c_str());lua_setfield(L,-2,p.first.c_str());}lua_pushboolean(L,true);lua_setfield(L,-2,"menusystem");lua_rawseti(L,-2,1);return 1;
  }
  if(op=="Addons"||op=="Games"||op=="UserContent"){lua_newtable(L);return 1;}
  if(op=="ImageSize"){lua_pushinteger(L,1280);lua_pushinteger(L,720);return 2;}
  if(op=="HTMLCreate"){lua_pushinteger(L,1);return 1;}
  if(op=="HTMLDestroy"){destroyed=true;return 0;}
  if(op=="HTMLPoll"){if(incoming.empty())return 0;auto p=incoming.front();incoming.pop_front();lua_pushstring(L,p.first.c_str());lua_pushstring(L,p.second.c_str());return 2;}
  if(op=="BackgroundFrame"){js.push_back("window.__gmpPaintBackground("+str(3)+",1280,720)");return 0;}
  if(op=="HTMLCall"){js.push_back(str(3));return 0;}
  if(op=="HTMLReply"){std::ostringstream out;out<<"window.__gmpComplete("<<lua_tonumber(L,3)<<","<<str(4)<<")";js.push_back(out.str());return 0;}
  if(op=="Command"){if(str(2)=="map")inGame=true;std::string s;for(int i=2;i<=lua_gettop(L);++i){if(i>2)s+=' ';s+=str(i);}commands.push_back(s);return 0;}
  if(op=="Phrase"){
   std::ifstream f(corpus/"resource/localization/en/main_menu.properties");std::string line,key=str(2),value=key;
   while(std::getline(f,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.rfind(key+"=",0)==0){value=line.substr(key.size()+1);break;}}
   lua_pushstring(L,value.c_str());return 1;
  }
  if(op=="HTMLFunction"){functions.push_back(str(3)+"."+str(4));return 0;}
  if(op=="Notify"){errors.push_back(str(2));return 0;}
  if(op=="HTMLFunction"||op=="HTMLViewport"||op=="HTMLShow"||op=="Log"||op=="Background"||op=="BackgroundClear"||op=="BackgroundFrame"||op=="Sound"||op=="UICommand")return 0;
  return luaL_error(L,"Unexpected test-host operation: %s",op.c_str());
 }
};
static std::string quote(const std::string &s){std::string out="\"";for(unsigned char c:s){if(c=='"'||c=='\\'){out+='\\';out+=c;}else if(c=='\n')out+="\\n";else if(c=='\r')out+="\\r";else if(c=='\t')out+="\\t";else if(c<32){const char *hex="0123456789abcdef";out+="\\u00";out+=hex[c>>4];out+=hex[c&15];}else out+=c;}return out+'"';}
static void output(Host &h){std::cout<<"{";bool first=true;for(auto pair:{std::make_pair("js",&h.js),std::make_pair("commands",&h.commands),std::make_pair("errors",&h.errors),std::make_pair("functions",&h.functions)}){if(!first)std::cout<<",";first=false;std::cout<<quote(pair.first)<<":[";bool item=false;for(const auto &s:*pair.second){if(item)std::cout<<",";item=true;std::cout<<quote(s);}std::cout<<"]";pair.second->clear();}std::cout<<"}"<<std::endl;}
int main(int argc,char **argv){
 if(argc<3)return 2;
 Host host;host.corpus=argv[1];host.port=argv[2];
 glua::RuntimeConfig cfg{glua::REALM_MENU,[&](const std::string &p,std::string &s){return host.read(p,s);},[&](const std::string &s){std::cerr<<s<<'\n';},[&](lua_State *L){lua_newtable(L);lua_pushlightuserdata(L,&host);lua_pushcclosure(L,Host::Call,1);lua_setfield(L,-2,"Invoke");lua_setglobal(L,"port");}};
 glua::Runtime vm(cfg);
 if(!vm.File("port/menu_bootstrap.lua")){std::cerr<<vm.Error()<<'\n';return 1;}
 if(!vm.String("port.MenuTick()","first frame")){std::cerr<<vm.Error()<<'\n';return 1;}
 if(argc>3&&std::string(argv[3])=="--serve"){
  output(host);std::string line;
  while(std::getline(std::cin,line)){auto tab=line.find('\t');if(tab==std::string::npos)return 2;host.incoming.push_back({line.substr(0,tab),line.substr(tab+1)});host.time+=0.01;if(!vm.String("port.MenuTick()","HTML frame"))return 1;output(host);}return 0;
 }
 assert(vm.String("assert(IsValid(pnlMainMenu)); assert(pnlMainMenu.HTML.handle==1); assert(not pnlMainMenu.HTML.ready)","created"));
 host.incoming.push_back({"_gmp.Ready","{\"args\":[],\"request\":null}"});host.time=0.2;
 assert(vm.String("port.MenuTick()","ready frame"));assert(host.errors.empty());
 assert(vm.String("assert(pnlMainMenu.HTML.ready and pnlMainMenu.menuLoaded); assert(GetMapList().Sandbox[1]=='gm_construct')","original map logic"));
 host.incoming.push_back({"language.Update","{\"args\":[\"new_game\"],\"request\":1}"});
 host.incoming.push_back({"lua.Run","{\"args\":[\"RunConsoleCommand(\\\"map\\\", %s)\",\"gm_construct\"],\"request\":null}"});
 assert(vm.String("port.MenuTick()","callbacks"));assert(host.commands.size()==1&&host.commands[0]=="map gm_construct");
 host.incoming.push_back({"lua.Run","{\"args\":[\"gui.HideGameUI()\"],\"request\":null}"});
 assert(vm.String("port.MenuTick()","resume callback"));assert(host.commands.back()=="gameui_hide");
 bool translated=false;for(auto &s:host.js)if(s.find("Start New Game")!=std::string::npos)translated=true;assert(translated);
 assert(vm.String("ToggleFavourite('gm_construct'); assert(GetMapList().Favourites[1]=='gm_construct'); SaveLastMap('gm_construct','Sandbox')","original persistence"));
 assert(!host.data["menu_cookies.json"].empty());
 assert(vm.String(R"lua(local j=include('port/json.lua'); assert(j.decode('[false,null,"\\ud83d\\ude00"]')[1]==false); assert(j.decode(j.encode({x='a\n"b',n=-12.5})).x=='a\n"b'); for _,bad in ipairs({'01','1.','1e','[1,]','{"a":1,}','"\\ud800"','NaN','true false'}) do assert(not pcall(j.decode,bad),bad) end)lua","JSON strictness"));
 assert(vm.String("port.MenuShutdown(); assert(not IsValid(pnlMainMenu))","shutdown"));assert(host.destroyed);assert(host.errors.empty());assert(lua_gettop(vm.State())==0);
 std::cout<<"PASS: unchanged Facepunch mainmenu/getmaps, DHTML callbacks, JSON, map start, favourites, persistence and cleanup\n";
}
