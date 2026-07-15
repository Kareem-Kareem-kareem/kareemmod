// ============================================================================
//  SpeedClock - example RLModKit minigame plugin
// ----------------------------------------------------------------------------
//  A tiny training overlay that demonstrates the full plugin SDK surface:
//    - metadata + lifecycle (OnLoad / OnTick / OnRender / OnRenderSettings)
//    - registering a console command
//    - reading/writing cvars
//    - drawing a screen-space overlay
//
//  Gameplay: a stopwatch you start/stop from the console. While running it
//  shows elapsed time and your (placeholder) speed as a HUD in the corner - a
//  scaffold you can turn into a real "fastest lap" style minigame once the RL
//  SDK state adapter is wired up.
// ============================================================================
#include "PluginSDK.h"

#include <string>
#include <cstdio>
#include <cmath>

using namespace rlmk;

// ImGui is available to plugins for their settings tab if they choose to link
// it. To keep this example dependency-free we only use it if the macro is set.
#if defined(RLMK_PLUGIN_USE_IMGUI)
#include "imgui.h"
#endif

class SpeedClockPlugin final : public IPlugin {
public:
    const char* Name()    const override { return "Speed Clock"; }
    const char* Author()  const override { return "RLModKit"; }
    const char* Version() const override { return "1.0.0"; }

    void OnLoad(IServices* services) override {
        m_services = services;

        // Restore persisted HUD position.
        m_hudX = std::stof(services->GetCvar("speedclock_x", "40"));
        m_hudY = std::stof(services->GetCvar("speedclock_y", "40"));

        services->RegisterCommand("speedclock", "speedclock <start|stop|reset>",
            [this](const std::vector<std::string>& args) {
                if (args.empty()) { Help(); return; }
                if (args[0] == "start") { m_running = true;  m_services->Log("Speed Clock started."); }
                else if (args[0] == "stop") { m_running = false; m_services->Log("Speed Clock stopped."); }
                else if (args[0] == "reset") { m_elapsed = 0.f; m_services->Log("Speed Clock reset."); }
                else Help();
            });

        services->Log("Speed Clock loaded. Try 'speedclock start' then press F1 to close the console.");
    }

    void OnUnload() override {
        if (m_services) {
            m_services->SetCvar("speedclock_x", std::to_string(m_hudX));
            m_services->SetCvar("speedclock_y", std::to_string(m_hudY));
        }
    }

    void OnTick(const GameState& state) override {
        if (m_running && state.inGame) {
            m_elapsed += state.deltaTime;
            // Speed magnitude from car velocity (uu/s). 0 until the RL SDK
            // adapter populates real values.
            const auto& v = state.localCar.velocity;
            m_speed = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
        }
    }

    void OnRender() override {
        char line1[64];
        std::snprintf(line1, sizeof(line1), "SPEED CLOCK  %06.2fs", m_elapsed);
        char line2[64];
        std::snprintf(line2, sizeof(line2), "Speed: %.0f uu/s", m_speed);

        const uint32_t accent = m_running ? 0x35D07AFFu : 0xC8C8C8FFu; // green / grey (RGBA)

        m_services->DrawRect(m_hudX - 12, m_hudY - 10, 230, 62, 0x101418C8u, true);
        m_services->DrawRect(m_hudX - 12, m_hudY - 10, 230, 62, accent, false);
        m_services->DrawText(m_hudX, m_hudY, line1, accent);
        m_services->DrawText(m_hudX, m_hudY + 24, line2, 0xFFFFFFFFu);
    }

    void OnRenderSettings() override {
#if defined(RLMK_PLUGIN_USE_IMGUI)
        ImGui::SliderFloat("HUD X", &m_hudX, 0.f, 1920.f, "%.0f");
        ImGui::SliderFloat("HUD Y", &m_hudY, 0.f, 1080.f, "%.0f");
        ImGui::Text("Elapsed: %.2fs", m_elapsed);
        if (ImGui::Button(m_running ? "Stop" : "Start")) m_running = !m_running;
        ImGui::SameLine();
        if (ImGui::Button("Reset")) m_elapsed = 0.f;
#else
        // No ImGui linked: nothing to draw in the settings tab.
#endif
    }

private:
    void Help() {
        m_services->Log("usage: speedclock <start|stop|reset>");
    }

    IServices* m_services = nullptr;
    bool  m_running = false;
    float m_elapsed = 0.f;
    float m_speed = 0.f;
    float m_hudX = 40.f;
    float m_hudY = 40.f;
};

// ----------------------------------------------------------------------------
//  Required SDK exports.
// ----------------------------------------------------------------------------
extern "C" __declspec(dllexport) rlmk::IPlugin* CreatePlugin() {
    return new SpeedClockPlugin();
}

extern "C" __declspec(dllexport) int GetPluginSdkVersion() {
    return RLMODKIT_SDK_VERSION;
}
