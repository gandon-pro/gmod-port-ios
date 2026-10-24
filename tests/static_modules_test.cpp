#include "static_modules.h"
#include <cassert>
#include <cstring>

static int engineObject, clientObject;
static void* Engine(const char* name, int* status) {
    const bool match = std::strcmp(name, "SameInterface001") == 0;
    if (status) *status = match ? 0 : 1;
    return match ? &engineObject : nullptr;
}
static void* Client(const char* name, int* status) {
    const bool match = std::strcmp(name, "SameInterface001") == 0;
    if (status) *status = match ? 0 : 1;
    return match ? &clientObject : nullptr;
}
int main() {
    using namespace gmodport;
    const Module invalid[] = {{"engine", Engine}, {"libengine.so", Client}};
    assert(!InstallModules(invalid, 2));
    const Module table[] = {{"engine", Engine}, {"client", Client}};
    assert(InstallModules(table, 2));
    assert(!InstallModules(table, 2));
    assert(!LoadModule("engine", true));
    auto* engine = LoadModule("/Documents/garrysmod/bin/LIBENGINE.DYLIB");
    auto* client = LoadModule("bin\\client.dll");
    assert(engine && client);
    assert(GetFactory(engine)("SameInterface001", nullptr) == &engineObject);
    assert(GetFactory(client)("SameInterface001", nullptr) == &clientObject);
    assert(LoadModule("engine.so", true) == engine);
    assert(ReferenceCount(engine) == 1);
    assert(LoadModule("engine") == engine);
    assert(ReferenceCount(engine) == 2);
    UnloadModule(engine); UnloadModule(engine);
    assert(!GetFactory(engine) && !GetFactory("engine"));
    assert(!LoadModule("engine", true));
    assert(!LoadModule("some_user_module.dll"));
    assert(std::strstr(LastModuleError(), "not linked"));
    assert(!GetFactory(reinterpret_cast<ModuleHandle*>(1)));
    UnloadModule(reinterpret_cast<ModuleHandle*>(1));
    return 0;
}
