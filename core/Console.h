#pragma once

#include "PluginSDK.h"

#include <string>
#include <vector>
#include <map>

namespace rlmk {

class PluginManager;

// The F1 developer console + plugin manager UI, drawn with ImGui.
class Console {
public:
    struct Command {
        std::string description;
        IServices::CommandHandler handler;
    };

    Console();

    void SetPluginManager(PluginManager* pm) { m_pluginManager = pm; }

    // Toggle / query visibility (bound to F1 by the Renderer).
    void Toggle()            { m_visible = !m_visible; }
    void SetVisible(bool v)  { m_visible = v; }
    bool IsVisible() const   { return m_visible; }

    // Command registry.
    void RegisterCommand(const std::string& name,
                         const std::string& description,
                         IServices::CommandHandler handler);
    // Parse a raw input line ("cmd arg1 arg2") and dispatch it.
    void Execute(const std::string& line);

    // Draw the whole console window. Call once per frame inside an ImGui frame.
    void Render();

private:
    void RegisterBuiltins();
    void DrawConsoleTab();
    void DrawPluginsTab();
    void DrawAboutTab();

    bool m_visible = false;
    PluginManager* m_pluginManager = nullptr;

    std::map<std::string, Command> m_commands;   // sorted for help output
    std::vector<std::string> m_history;
    int  m_historyPos = -1;
    char m_inputBuffer[512] = {0};
    bool m_scrollToBottom = true;
    bool m_autoScroll = true;
};

} // namespace rlmk
