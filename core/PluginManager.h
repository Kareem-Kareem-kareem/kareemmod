#pragma once

#include "PluginSDK.h"

#include <string>
#include <vector>
#include <memory>

namespace rlmk {

class Services;

// Loads plugin DLLs from a directory, owns their lifetime, and fans out the
// lifecycle callbacks (tick/render). Supports hot (re)loading at runtime so you
// can iterate on a minigame plugin without restarting the game.
class PluginManager {
public:
    struct LoadedPlugin {
        std::string path;
        std::string fileName;
        void* module = nullptr;        // HMODULE
        IPlugin* instance = nullptr;    // owned by the DLL, freed on unload
        bool healthy = true;
    };

    explicit PluginManager(Services* services);
    ~PluginManager();

    // Scan directory for *.dll and load any that expose CreatePlugin().
    void LoadAll(const std::string& directory);
    void UnloadAll();

    // Load / unload / reload a single plugin by file name.
    bool Load(const std::string& path);
    bool Unload(const std::string& fileName);
    bool Reload(const std::string& fileName);

    // Lifecycle fan-out (called by the core each frame).
    void Tick(const GameState& state);
    void Render();

    const std::vector<LoadedPlugin>& Plugins() const { return m_plugins; }
    IPlugin* Find(const std::string& name);

private:
    Services* m_services;              // not owned
    std::string m_directory;
    std::vector<LoadedPlugin> m_plugins;
};

} // namespace rlmk
