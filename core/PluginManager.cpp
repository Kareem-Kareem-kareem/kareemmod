#include "PluginManager.h"
#include "Services.h"
#include "Logger.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <filesystem>

namespace fs = std::filesystem;

namespace rlmk {

using CreatePluginFn = IPlugin* (*)();
using GetSdkVersionFn = int (*)();

PluginManager::PluginManager(Services* services) : m_services(services) {}

PluginManager::~PluginManager() {
    UnloadAll();
}

void PluginManager::LoadAll(const std::string& directory) {
    m_directory = directory;
    std::error_code ec;
    if (!fs::exists(directory, ec)) {
        fs::create_directories(directory, ec);
        Logger::Get().Info("Created plugin directory: " + directory);
        return;
    }

    for (const auto& entry : fs::directory_iterator(directory, ec)) {
        if (entry.is_regular_file() && entry.path().extension() == ".dll") {
            Load(entry.path().string());
        }
    }
}

bool PluginManager::Load(const std::string& path) {
    const std::string fileName = fs::path(path).filename().string();

    HMODULE module = LoadLibraryA(path.c_str());
    if (!module) {
        Logger::Get().Error("Failed to LoadLibrary plugin: " + fileName +
                            " (err " + std::to_string(GetLastError()) + ")");
        return false;
    }

    auto getVersion = reinterpret_cast<GetSdkVersionFn>(
        GetProcAddress(module, "GetPluginSdkVersion"));
    auto create = reinterpret_cast<CreatePluginFn>(
        GetProcAddress(module, "CreatePlugin"));

    if (!create) {
        Logger::Get().Warning("Skipping " + fileName + ": no CreatePlugin export.");
        FreeLibrary(module);
        return false;
    }

    if (getVersion && getVersion() != RLMODKIT_SDK_VERSION) {
        Logger::Get().Error("Plugin " + fileName + " built against SDK v" +
                            std::to_string(getVersion()) + ", core is v" +
                            std::to_string(RLMODKIT_SDK_VERSION) + ". Not loaded.");
        FreeLibrary(module);
        return false;
    }

    IPlugin* instance = create();
    if (!instance) {
        Logger::Get().Error("CreatePlugin() returned null for " + fileName);
        FreeLibrary(module);
        return false;
    }

    instance->OnLoad(m_services);
    m_plugins.push_back({path, fileName, module, instance, true});

    Logger::Get().Info(std::string("Loaded plugin: ") + instance->Name() +
                       " v" + instance->Version() + " by " + instance->Author());
    return true;
}

bool PluginManager::Unload(const std::string& fileName) {
    for (auto it = m_plugins.begin(); it != m_plugins.end(); ++it) {
        if (it->fileName == fileName) {
            if (it->instance) it->instance->OnUnload();
            // The instance is allocated inside the DLL; freeing the library
            // runs its destructors. We drop our pointer first.
            if (it->module) FreeLibrary(static_cast<HMODULE>(it->module));
            Logger::Get().Info("Unloaded plugin: " + fileName);
            m_plugins.erase(it);
            return true;
        }
    }
    Logger::Get().Warning("Unload: plugin not found: " + fileName);
    return false;
}

bool PluginManager::Reload(const std::string& fileName) {
    std::string path;
    for (const auto& p : m_plugins) {
        if (p.fileName == fileName) { path = p.path; break; }
    }
    if (path.empty()) {
        // Maybe not loaded yet - try the plugin directory.
        path = (fs::path(m_directory) / fileName).string();
    }
    Unload(fileName);
    return Load(path);
}

void PluginManager::UnloadAll() {
    for (auto& p : m_plugins) {
        if (p.instance) p.instance->OnUnload();
        if (p.module) FreeLibrary(static_cast<HMODULE>(p.module));
    }
    m_plugins.clear();
}

void PluginManager::Tick(const GameState& state) {
    for (auto& p : m_plugins) {
        if (p.healthy && p.instance && p.instance->IsEnabled()) {
            p.instance->OnTick(state);
        }
    }
}

void PluginManager::Render() {
    for (auto& p : m_plugins) {
        if (p.healthy && p.instance && p.instance->IsEnabled()) {
            p.instance->OnRender();
        }
    }
}

IPlugin* PluginManager::Find(const std::string& name) {
    for (auto& p : m_plugins) {
        if (p.instance && name == p.instance->Name()) return p.instance;
    }
    return nullptr;
}

} // namespace rlmk
