#include <lua.hpp>
#include <fstream>
#include <iostream>
#include <string>

static std::string Quote(const std::string& s) {
    std::string out="\"";
    for(unsigned char c:s) {
        if(c=='"'||c=='\\') { out+='\\';out+=static_cast<char>(c); }
        else if(c=='\n') out+="\\n";
        else if(c=='\r') out+="\\r";
        else if(c=='\t') out+="\\t";
        else if(c<32) out+='?';
        else out+=static_cast<char>(c);
    }
    return out+'"';
}
int main(int argc,char** argv) {
    if(argc!=3) { std::cerr<<"usage: gluavm_verify --compile-manifest|--run-manifest FILE\n";return 2; }
    bool execute=std::string(argv[1])=="--run-manifest";
    if(!execute && std::string(argv[1])!="--compile-manifest")return 2;
    std::ifstream list(argv[2]);
    if(!list)return 2;
    int count=0,failures=0;
    for(std::string path;std::getline(list,path);) {
        if(!path.empty()&&path.back()=='\r')path.pop_back();
        if(path.empty())continue;
        lua_State* L=luaL_newstate();
        if(!L)return 3;
        if(execute)luaL_openlibs(L);
        int status=luaL_loadfile(L,path.c_str());
        if(!status&&execute)status=lua_pcall(L,0,0,0);
        const char* error=status?lua_tostring(L,-1):"";
        std::cout<<"{\"file\":"<<Quote(path)<<",\"passed\":"<<(status?"false":"true")
                 <<",\"error\":"<<Quote(error?error:"")<<"}\n";
        ++count;if(status)++failures;
        lua_close(L);
    }
    std::cout<<"{\"files\":"<<count<<",\"failures\":"<<failures<<"}\n";
    return failures||count==0?1:0;
}
