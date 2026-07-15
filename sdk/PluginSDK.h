// ============================================================================
//  RLModKit - Public Plugin SDK
// ----------------------------------------------------------------------------
//  This header is the ONLY thing a minigame plugin needs to include in order to
//  be loaded by the RLModKit core. It is intentionally dependency-free (only
//  the C++ standard library + a couple of forward declared interfaces) so that
//  plugins stay small and ABI-stable.
//
//  A plugin is a normal Windows DLL that exports a single factory function:
//
//      extern "C" __declspec(dllexport) IPlugin* CreatePlugin();
//
//  The core will call CreatePlugin(), keep the returned pointer for the life of
//  the DLL, and call the lifecycle hooks below on the game's render thread.
//
//  IMPORTANT (educational / fair-play scope):
//  RLModKit only attaches while Rocket League is running in a NON-competitive,
//  NON-EAC-protected context (Freeplay, Training, Local/Exhibition matches).
//  Plugins are meant for local minigames, overlays and training tools only.
// ============================================================================
#pragma once

#include <string>
#include <vector>
#include <functional>
#include <cstdint>

#define RLMODKIT_SDK_VERSION 1

namespace rlmk {

// ----------------------------------------------------------------------------
//  Lightweight math types (no external math library required)
// ----------------------------------------------------------------------------
struct Vec3 {
    float x = 0.f, y = 0.f, z = 0.f;
};

struct Rotator {
    int pitch = 0, yaw = 0, roll = 0;
};

// ----------------------------------------------------------------------------
//  Read-only snapshot of the local game state the core exposes to plugins.
//  These values are populated by the core from the game each frame. Plugins
//  should treat them as read-only inputs for their own logic/overlays.
// ----------------------------------------------------------------------------
struct BallState {
    Vec3 location;
    Vec3 velocity;
    Vec3 angularVelocity;
    bool valid = false;
};

struct CarState {
    Vec3 location;
    Vec3 velocity;
    Rotator rotation;
    float boostAmount = 0.f;   // 0..100
    bool  isOnGround = true;
    bool  isSupersonic = false;
    bool  valid = false;
};

struct GameState {
    bool   inGame = false;         // true when a freeplay/training/local match is active
    bool   isReplay = false;
    float  timeSeconds = 0.f;      // game clock, seconds
    float  deltaTime = 0.f;        // frame delta, seconds
    CarState localCar;
    BallState ball;
};

// ----------------------------------------------------------------------------
//  Services handed to plugins by the core. This is the plugin's only channel
//  for talking back to the framework (logging, drawing, registering commands,
//  reading game state). Kept as an abstract interface so the core can evolve
//  without breaking already-compiled plugins.
// ----------------------------------------------------------------------------
class IServices {
public:
    virtual ~IServices() = default;

    // --- Logging -> shows up in the F1 console + log file ------------------
    virtual void Log(const std::string& message) = 0;
    virtual void LogWarning(const std::string& message) = 0;
    virtual void LogError(const std::string& message) = 0;

    // --- Console commands --------------------------------------------------
    // Register a command callable from the F1 console, e.g. "speedclock_reset".
    // args excludes the command name itself.
    using CommandHandler = std::function<void(const std::vector<std::string>& args)>;
    virtual void RegisterCommand(const std::string& name,
                                 const std::string& description,
                                 CommandHandler handler) = 0;

    // --- Cvars (persisted key/value settings) ------------------------------
    virtual void  SetCvar(const std::string& key, const std::string& value) = 0;
    virtual std::string GetCvar(const std::string& key, const std::string& fallback = "") = 0;

    // --- Game state --------------------------------------------------------
    virtual const GameState& GetGameState() const = 0;

    // --- Immediate-mode overlay drawing (screen space, pixels) -------------
    // These are safe to call only from IPlugin::OnRender.
    virtual void DrawText(float x, float y, const std::string& text,
                          uint32_t rgba = 0xFFFFFFFFu) = 0;
    virtual void DrawLine(float x1, float y1, float x2, float y2,
                          uint32_t rgba = 0xFFFFFFFFu, float thickness = 1.f) = 0;
    virtual void DrawRect(float x, float y, float w, float h,
                          uint32_t rgba = 0xFFFFFFFFu, bool filled = false) = 0;
};

// ----------------------------------------------------------------------------
//  The interface every plugin implements.
// ----------------------------------------------------------------------------
class IPlugin {
public:
    virtual ~IPlugin() = default;

    // Metadata ---------------------------------------------------------------
    virtual const char* Name() const = 0;
    virtual const char* Author() const = 0;
    virtual const char* Version() const = 0;

    // Lifecycle --------------------------------------------------------------
    // Called once after the plugin DLL is loaded. Register commands/cvars here.
    virtual void OnLoad(IServices* services) = 0;

    // Called once before the plugin DLL is unloaded. Clean up here.
    virtual void OnUnload() {}

    // Called every game tick with the latest state. Put gameplay/minigame
    // logic here. Do NOT do any drawing from here.
    virtual void OnTick(const GameState& state) { (void)state; }

    // Called every frame while the plugin is enabled. Do overlay drawing here
    // via the IServices draw calls. Kept separate from OnTick so drawing always
    // happens on the render thread.
    virtual void OnRender() {}

    // Called when the user draws this plugin's tab inside the F1 console.
    // Use ImGui here if you link against it, or leave empty. Optional.
    virtual void OnRenderSettings() {}

    // Enable/disable toggled from the console. Plugins may override to react.
    virtual void SetEnabled(bool enabled) { m_enabled = enabled; }
    virtual bool IsEnabled() const { return m_enabled; }

protected:
    bool m_enabled = true;
};

} // namespace rlmk

// ----------------------------------------------------------------------------
//  Factory export. Every plugin DLL must define exactly this.
// ----------------------------------------------------------------------------
extern "C" {
    // Return a heap-allocated plugin instance. The core takes ownership.
    __declspec(dllexport) rlmk::IPlugin* CreatePlugin();

    // Report which SDK version this plugin was built against so the core can
    // refuse to load incompatible plugins.
    __declspec(dllexport) int GetPluginSdkVersion();
}
