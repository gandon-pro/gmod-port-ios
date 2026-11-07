#include "glua_runtime.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
static std::string JSON(const std::string &s){std::string out="\"";for(unsigned char c:s){if(c=='"'||c=='\\'){out+='\\';out+=c;}else if(c=='\n')out+="\\n";else if(c=='\r')out+="\\r";else if(c=='\t')out+="\\t";else if(c>=32)out+=c;}return out+'"';}
int main(int argc,char **argv){
 if(argc!=2){std::cerr<<"usage: bootstrap_probe PATH_TO_GARRYSMOD_DIR\n";return 2;}
 bool all=true;
 for(auto realm:{glua::REALM_CLIENT,glua::REALM_SERVER,glua::REALM_MENU}){
  const std::string root=argv[1];
  glua::RuntimeConfig cfg{realm,[&](const std::string &path,std::string &out){std::ifstream file(root+"/lua/"+path,std::ios::binary);if(!file)return false;out.assign(std::istreambuf_iterator<char>(file),{});return true;},{},{}};
  glua::Runtime vm(cfg);bool ok=vm.File("includes/init.lua");all&=ok;
  std::cout<<"{\"realm\":"<<int(realm)<<",\"original_bootstrap_ready\":"<<(ok?"true":"false")<<",\"error\":"<<JSON(vm.Error())<<"}\n";
 }
 return all?0:1;
}
