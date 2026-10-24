#pragma once
#include <cstddef>

namespace gmodport {
using Factory = void* (*)(const char*, int*);
struct Module {
    const char* name;
    Factory factory;
};
struct ModuleHandle;

// The generated table is installed before LauncherMain, not from unordered
// cross-module static constructors. InterfaceReg constructors may run earlier.
bool InstallModules(const Module* modules, std::size_t count);
ModuleHandle* LoadModule(const char* path, bool noLoad = false);
void UnloadModule(ModuleHandle* handle);
Factory GetFactory(ModuleHandle* handle);
Factory GetFactory(const char* path);
unsigned ReferenceCount(ModuleHandle* handle);
const char* LastModuleError();
}
