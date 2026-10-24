// Shared replacements for tier1/interface.cpp's dynamic loader portion.
// InterfaceReg/CreateInterface/Sys_GetFactoryThis live in per-module factories.
#include "tier1/interface.h"
#include "static_modules.h"

CSysModule* Sys_LoadModule(const char* name, Sys_Flags flags) {
    return reinterpret_cast<CSysModule*>(gmodport::LoadModule(name, (flags & SYS_NOLOAD) != 0));
}
void Sys_UnloadModule(CSysModule* handle) {
    gmodport::UnloadModule(reinterpret_cast<gmodport::ModuleHandle*>(handle));
}
CreateInterfaceFn Sys_GetFactory(CSysModule* handle) {
    return gmodport::GetFactory(reinterpret_cast<gmodport::ModuleHandle*>(handle));
}
CreateInterfaceFn Sys_GetFactory(const char* name) { return gmodport::GetFactory(name); }
bool Sys_LoadInterface(const char* name, const char* version, CSysModule** outModule, void** outInterface) {
    if (!outInterface) return false;
    if (outModule) *outModule = nullptr;
    *outInterface = nullptr;
    auto* module = Sys_LoadModule(name);
    const auto factory = Sys_GetFactory(module);
    if (!factory) { Sys_UnloadModule(module); return false; }
    void* object = factory(version, nullptr);
    if (!object) { Sys_UnloadModule(module); return false; }
    if (outModule) *outModule = module;
    *outInterface = object;
    return true;
}
bool Sys_IsDebuggerPresent() { return false; }
CDllDemandLoader::CDllDemandLoader(const char* name)
    : m_pchModuleName(name), m_hModule(nullptr), m_bLoadAttempted(false) {}
CDllDemandLoader::~CDllDemandLoader() { Unload(); }
CreateInterfaceFn CDllDemandLoader::GetFactory() {
    if (!m_bLoadAttempted) { m_hModule = Sys_LoadModule(m_pchModuleName); m_bLoadAttempted = true; }
    return Sys_GetFactory(m_hModule);
}
void CDllDemandLoader::Unload() {
    Sys_UnloadModule(m_hModule);
    m_hModule = nullptr;
    m_bLoadAttempted = false;
}
