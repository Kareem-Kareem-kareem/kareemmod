#pragma once

#include "PluginSDK.h"

#include <string>
#include <unordered_map>

namespace rlmk {

class Console;

// Concrete implementation of the SDK's IServices interface. One instance is
// shared with every loaded plugin. Draw calls are buffered and flushed onto the
// active ImGui foreground draw list each frame by the Renderer.
class Services : public IServices {
public:
    explicit Services(Console* console);

    // --- IServices: logging ------------------------------------------------
    void Log(const std::string& message) override;
    void LogWarning(const std::string& message) override;
    void LogError(const std::string& message) override;

    // --- IServices: commands ----------------------------------------------
    void RegisterCommand(const std::string& name,
                         const std::string& description,
                         CommandHandler handler) override;

    // --- IServices: cvars --------------------------------------------------
    void SetCvar(const std::string& key, const std::string& value) override;
    std::string GetCvar(const std::string& key, const std::string& fallback) override;

    // --- IServices: game state --------------------------------------------
    const GameState& GetGameState() const override { return m_state; }

    // --- IServices: overlay drawing ---------------------------------------
    void DrawText(float x, float y, const std::string& text, uint32_t rgba) override;
    void DrawLine(float x1, float y1, float x2, float y2, uint32_t rgba, float thickness) override;
    void DrawRect(float x, float y, float w, float h, uint32_t rgba, bool filled) override;

    // --- Core-side access --------------------------------------------------
    GameState& MutableState() { return m_state; }

    // Persist / load cvars to disk (simple key=value file).
    void LoadCvars(const std::string& path);
    void SaveCvars(const std::string& path) const;

private:
    Console* m_console;              // not owned
    GameState m_state{};
    std::unordered_map<std::string, std::string> m_cvars;
    std::string m_cvarPath;
};

} // namespace rlmk
