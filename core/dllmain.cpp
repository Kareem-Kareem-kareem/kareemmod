// ============================================================================
//  RLModKitCore - injected DLL entry point
// ----------------------------------------------------------------------------
//  Boots the framework on a dedicated thread (never do heavy work in DllMain):
//    - initialize logging
//    - self-verify no anti-cheat is present in this process
//    - construct Console / PluginManager / Services and wire them together
//    - hook DirectX and load plugins from the ./plugins folder
//
//  A lightweight watchdog thread periodically re-checks for anti-cheat and
//  triggers a clean unload if it ever appears, keeping this strictly a local /
//  non-competitive tool.
// ============================================================================
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "Logger.h"
#include "EacGuard.h"
#include "Console.h"
#include "PluginManager.h"
#include "Services.h"
#include "Renderer.h"

#include <memory>
#include <string>
#include <filesystem>
#include <chrono>
#include <thread>

namespace fs = std::filesystem;

namespace {

HMODULE g_selfModule = nullptr;
std::unique_ptr<rlmk::Console> g_console;
std::unique_ptr<rlmk::PluginManager> g_pluginManager;
std::unique_ptr<rlmk::Services> g_services;
volatile bool g_running = false;

fs::path SelfDirectory() {
    wchar_t buf[MAX_PATH]{};
    GetModuleFileNameW(g_selfModule, buf, MAX_PATH);
    return fs::path(buf).parent_path();
}

void Shutdown() {
    g_running = false;
    rlmk::Renderer::Get().Unhook();

    if (g_pluginManager) g_pluginManager->UnloadAll();

    g_pluginManager.reset();
    g_services.reset();
    g_console.reset();

    rlmk::Logger::Get().Info("RLModKit shutting down.");
    rlmk::Logger::Get().Shutdown();
}

// Periodic anti-cheat watchdog. Detaches the whole toolkit if EAC shows up.
void WatchdogThread() {
    using namespace std::chrono_literals;
    while (g_running) {
        if (rlmk::EacGuard::AntiCheatPresent()) {
            rlmk::Logger::Get().Error("Anti-cheat detected at runtime. Detaching RLModKit.");
            Shutdown();
            FreeLibraryAndExitThread(g_selfModule, 0);
        }
        std::this_thread::sleep_for(3s);
    }
}

// Minimal game-state pump. Reading real ball/car state requires Rocket League
// SDK offsets which are game-version specific and intentionally NOT bundled
// here. This keeps timing/inGame fields fresh so plugins and overlays work; a
// full RL SDK adapter can populate the rest later.
void StatePumpThread() {
    using clock = std::chrono::steady_clock;
    auto last = clock::now();
    while (g_running) {
        auto now = clock::now();
        float dt = std::chrono::duration<float>(now - last).count();
        last = now;

        if (g_services) {
            auto& s = g_services->MutableState();
            s.deltaTime = dt;
            s.timeSeconds += dt;
            // Without the RL SDK we can't confirm match type, so default to the
            // safe assumption that this is a local session (the injector already
            // guaranteed no EAC).
            s.inGame = true;
            g_pluginManager->Tick(s);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(8)); // ~120 Hz
    }
}

DWORD WINAPI Bootstrap(LPVOID) {
    const fs::path dir = SelfDirectory();

    rlmk::Logger::Get().Init((dir / "rlmodkit.log").string());
    rlmk::Logger::Get().Info("RLModKit core injected.");

    if (rlmk::EacGuard::AntiCheatPresent()) {
        rlmk::Logger::Get().Error("Easy Anti-Cheat present in process. Refusing to run.");
        rlmk::Logger::Get().Shutdown();
        FreeLibraryAndExitThread(g_selfModule, 0);
        return 0;
    }

    g_console = std::make_unique<rlmk::Console>();
    g_services = std::make_unique<rlmk::Services>(g_console.get());
    g_pluginManager = std::make_unique<rlmk::PluginManager>(g_services.get());
    g_console->SetPluginManager(g_pluginManager.get());

    g_services->LoadCvars((dir / "cvars.cfg").string());

    rlmk::Renderer::Get().Bind(g_console.get(), g_pluginManager.get());
    if (!rlmk::Renderer::Get().Hook()) {
        rlmk::Logger::Get().Error("Failed to hook renderer; unloading.");
        Shutdown();
        FreeLibraryAndExitThread(g_selfModule, 0);
        return 0;
    }

    g_running = true;

    // Auto-load any plugins shipped next to the core.
    g_pluginManager->LoadAll((dir / "plugins").string());

    // Register a couple of core-level convenience commands.
    g_services->RegisterCommand("unload", "Detach RLModKit from the game.",
        [](const std::vector<std::string>&) {
            Shutdown();
            FreeLibraryAndExitThread(g_selfModule, 0);
        });

    std::thread(StatePumpThread).detach();
    std::thread(WatchdogThread).detach();

    rlmk::Logger::Get().Info("RLModKit ready. Press F1 in-game.");
    return 0;
}

} // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    switch (reason) {
        case DLL_PROCESS_ATTACH:
            g_selfModule = module;
            DisableThreadLibraryCalls(module);
            CreateThread(nullptr, 0, Bootstrap, nullptr, 0, nullptr);
            break;
        case DLL_PROCESS_DETACH:
            // If the process is exiting, don't touch hooked resources.
            break;
    }
    return TRUE;
}
