#include "tier1/interface.h"
#include "static_modules.h"
#include <cassert>
extern "C" void* gp_test_engine_CreateInterface(const char*, int*);
extern "C" void* gp_test_client_CreateInterface(const char*, int*);
int main() {
    const gmodport::Module table[] = {
        {"engine", gp_test_engine_CreateInterface}, {"client", gp_test_client_CreateInterface}
    };
    assert(gmodport::InstallModules(table, 2));
    CSysModule* engine = nullptr;
    void* object = nullptr;
    assert(Sys_LoadInterface("bin/engine.dll", "SharedTestInterface001", &engine, &object));
    assert(object && *static_cast<int*>(object) == 11);
    auto* client = Sys_LoadModule("libclient.so");
    assert(client && *static_cast<int*>(Sys_GetFactory(client)("SharedTestInterface001", nullptr)) == 22);
    assert(Sys_GetFactory("engine") == Sys_GetFactory(engine));
    Sys_UnloadModule(engine);
    assert(!Sys_GetFactory("engine"));
    assert(!Sys_LoadModule("engine", SYS_NOLOAD));
    CSysModule* failed = reinterpret_cast<CSysModule*>(1);
    object = reinterpret_cast<void*>(1);
    assert(!Sys_LoadInterface("engine", "WrongVersion001", &failed, &object));
    assert(!failed && !object);
    CDllDemandLoader loader("engine");
    assert(loader.GetFactory());
    loader.Unload();
    assert(!Sys_LoadModule("engine", SYS_NOLOAD));
}
