#pragma once
#include "glua_api.h"
#include <functional>
#include <string>
#include <vector>
namespace glua {
struct RuntimeConfig {
    Realm realm;
    std::function<bool(const std::string &,std::string &)> read;
    std::function<void(const std::string &)> log;
    std::function<void(lua_State *)> install;
};
/// One state, exclusively owned and called by its engine module's game thread.
class Runtime {
public:
    explicit Runtime(RuntimeConfig config);
    ~Runtime();
    Runtime(const Runtime &)=delete;Runtime &operator=(const Runtime &)=delete;
    bool File(const std::string &path);
    bool String(const std::string &code,const std::string &name);
    bool Hook(const char *name);
    lua_State *State()const{return m_State;}
    const std::string &Error()const{return m_Error;}
    static bool Normalize(const std::string &base,const std::string &name,std::string &out);
private:
    static Runtime *Self(lua_State *L);
    static int Include(lua_State *L);
    static int Print(lua_State *L);
    int LoadFile(const std::string &name);
    bool Call(int arguments,int results);
    void ErrorFromStack();
    RuntimeConfig m_Config;lua_State *m_State=nullptr;std::vector<std::string> m_Files;std::string m_Error;
};
}
