#include "static_modules.h"
#include <cctype>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

namespace gmodport {
struct ModuleHandle { std::string name; Factory factory; unsigned references; };
static std::vector<ModuleHandle> modules;
static std::mutex moduleMutex;
static bool installed = false;
static thread_local std::string lastError;

static std::string CanonicalName(const char* path) {
    if (!path || !*path) return {};
    std::string name(path);
    const auto slash = name.find_last_of("/\\");
    if (slash != std::string::npos) name.erase(0, slash + 1);
    for (char& c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    for (const char* suffix : {".dll", ".dylib", ".so", ".a"}) {
        const auto length = std::strlen(suffix);
        if (name.size() > length && name.compare(name.size() - length, length, suffix) == 0) {
            name.resize(name.size() - length);
            break;
        }
    }
    if (name.compare(0, 3, "lib") == 0) name.erase(0, 3);
    return name;
}

bool InstallModules(const Module* table, std::size_t count) {
    std::lock_guard<std::mutex> lock(moduleMutex);
    lastError.clear();
    if (installed || !table || count == 0) { lastError = "invalid or repeated module table installation"; return false; }
    std::vector<ModuleHandle> checked;
    checked.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        auto name = CanonicalName(table[i].name);
        if (name.empty() || !table[i].factory) { lastError = "invalid static module entry"; return false; }
        for (const auto& previous : checked) {
            if (previous.name == name) { lastError = "duplicate static module: " + name; return false; }
        }
        checked.push_back({name, table[i].factory, 0});
    }
    modules = std::move(checked);
    installed = true;
    return true;
}

ModuleHandle* LoadModule(const char* path, bool noLoad) {
    std::lock_guard<std::mutex> lock(moduleMutex);
    lastError.clear();
    const auto name = CanonicalName(path);
    for (auto& module : modules) {
        if (module.name != name) continue;
        if (noLoad && module.references == 0) { lastError = "module is not logically loaded: " + name; return nullptr; }
        if (!noLoad) ++module.references;
        return &module;
    }
    lastError = "module not linked into this application: " + name;
    return nullptr;  // No dlopen/dlsym fallback on iOS.
}

static ModuleHandle* Checked(ModuleHandle* handle) {
    for (auto& module : modules) if (&module == handle) return &module;
    return nullptr;
}

void UnloadModule(ModuleHandle* handle) {
    std::lock_guard<std::mutex> lock(moduleMutex);
    auto* module = Checked(handle);
    if (module && module->references) --module->references;
    // Static code and factory registrations stay resident. This is not a
    // replacement for IAppSystem::Shutdown/Disconnect in the engine lifecycle.
}

Factory GetFactory(ModuleHandle* handle) {
    std::lock_guard<std::mutex> lock(moduleMutex);
    auto* module = Checked(handle);
    return module && module->references ? module->factory : nullptr;
}

Factory GetFactory(const char* path) {
    std::lock_guard<std::mutex> lock(moduleMutex);
    const auto name = CanonicalName(path);
    for (const auto& module : modules) if (module.name == name && module.references) return module.factory;
    return nullptr;
}

unsigned ReferenceCount(ModuleHandle* handle) {
    std::lock_guard<std::mutex> lock(moduleMutex);
    auto* module = Checked(handle);
    return module ? module->references : 0;
}
const char* LastModuleError() { return lastError.c_str(); }
}
