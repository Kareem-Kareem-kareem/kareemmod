#include "Console.h"
#include "PluginManager.h"
#include "Logger.h"

#include "imgui.h"

#include <sstream>
#include <algorithm>

namespace rlmk {

Console::Console() {
    RegisterBuiltins();
}

// ----------------------------------------------------------------------------
//  Command registry
// ----------------------------------------------------------------------------
void Console::RegisterCommand(const std::string& name,
                              const std::string& description,
                              IServices::CommandHandler handler) {
    m_commands[name] = {description, std::move(handler)};
    Logger::Get().Info("Registered command: " + name);
}

static std::vector<std::string> Tokenize(const std::string& line) {
    std::vector<std::string> tokens;
    std::istringstream iss(line);
    std::string tok;
    while (iss >> tok) tokens.push_back(tok);
    return tokens;
}

void Console::Execute(const std::string& line) {
    if (line.empty()) return;

    m_history.push_back(line);
    m_historyPos = -1;
    Logger::Get().Info("> " + line);

    auto tokens = Tokenize(line);
    if (tokens.empty()) return;

    const std::string& name = tokens[0];
    auto it = m_commands.find(name);
    if (it == m_commands.end()) {
        Logger::Get().Warning("Unknown command: " + name + " (type 'help')");
        return;
    }

    std::vector<std::string> args(tokens.begin() + 1, tokens.end());
    try {
        it->second.handler(args);
    } catch (const std::exception& e) {
        Logger::Get().Error(std::string("Command threw: ") + e.what());
    } catch (...) {
        Logger::Get().Error("Command threw an unknown exception.");
    }
}

// ----------------------------------------------------------------------------
//  Built-in commands
// ----------------------------------------------------------------------------
void Console::RegisterBuiltins() {
    RegisterCommand("help", "List all registered commands.",
        [this](const std::vector<std::string>&) {
            Logger::Get().Info("Available commands:");
            for (const auto& [name, cmd] : m_commands) {
                Logger::Get().Info("  " + name + "  -  " + cmd.description);
            }
        });

    RegisterCommand("clear", "Clear the console log.",
        [](const std::vector<std::string>&) { Logger::Get().Clear(); });

    RegisterCommand("plugin_list", "List loaded plugins.",
        [this](const std::vector<std::string>&) {
            if (!m_pluginManager) return;
            const auto& plugins = m_pluginManager->Plugins();
            if (plugins.empty()) { Logger::Get().Info("No plugins loaded."); return; }
            for (const auto& p : plugins) {
                const char* state = p.instance && p.instance->IsEnabled() ? "on" : "off";
                Logger::Get().Info("  " + p.fileName + " [" + state + "]");
            }
        });

    RegisterCommand("plugin_load", "plugin_load <path.dll> - load a plugin.",
        [this](const std::vector<std::string>& args) {
            if (!m_pluginManager || args.empty()) {
                Logger::Get().Warning("usage: plugin_load <path.dll>"); return;
            }
            m_pluginManager->Load(args[0]);
        });

    RegisterCommand("plugin_unload", "plugin_unload <file.dll> - unload a plugin.",
        [this](const std::vector<std::string>& args) {
            if (!m_pluginManager || args.empty()) {
                Logger::Get().Warning("usage: plugin_unload <file.dll>"); return;
            }
            m_pluginManager->Unload(args[0]);
        });

    RegisterCommand("plugin_reload", "plugin_reload <file.dll> - hot-reload a plugin.",
        [this](const std::vector<std::string>& args) {
            if (!m_pluginManager || args.empty()) {
                Logger::Get().Warning("usage: plugin_reload <file.dll>"); return;
            }
            m_pluginManager->Reload(args[0]);
        });

    RegisterCommand("plugin_toggle", "plugin_toggle <name> - enable/disable a plugin.",
        [this](const std::vector<std::string>& args) {
            if (!m_pluginManager || args.empty()) {
                Logger::Get().Warning("usage: plugin_toggle <plugin name>"); return;
            }
            if (IPlugin* p = m_pluginManager->Find(args[0])) {
                p->SetEnabled(!p->IsEnabled());
                Logger::Get().Info(std::string(p->Name()) + " -> " +
                                   (p->IsEnabled() ? "enabled" : "disabled"));
            } else {
                Logger::Get().Warning("Plugin not found: " + args[0]);
            }
        });
}

// ----------------------------------------------------------------------------
//  Rendering
// ----------------------------------------------------------------------------
void Console::Render() {
    if (!m_visible) return;

    ImGui::SetNextWindowSize(ImVec2(760, 460), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("RLModKit Console  (F1 to toggle)", &m_visible)) {
        ImGui::End();
        return;
    }

    if (ImGui::BeginTabBar("##tabs")) {
        if (ImGui::BeginTabItem("Console")) { DrawConsoleTab(); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Plugins")) { DrawPluginsTab(); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("About"))   { DrawAboutTab();   ImGui::EndTabItem(); }
        ImGui::EndTabBar();
    }

    ImGui::End();
}

void Console::DrawConsoleTab() {
    ImGui::Checkbox("Auto-scroll", &m_autoScroll);
    ImGui::SameLine();
    if (ImGui::Button("Clear")) Logger::Get().Clear();
    ImGui::Separator();

    const float footer = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
    ImGui::BeginChild("##scroll", ImVec2(0, -footer), true,
                      ImGuiWindowFlags_HorizontalScrollbar);

    for (const auto& entry : Logger::Get().Snapshot()) {
        ImVec4 color(0.85f, 0.85f, 0.85f, 1.0f);
        if (entry.level == LogLevel::Warning) color = ImVec4(1.0f, 0.80f, 0.35f, 1.0f);
        if (entry.level == LogLevel::Error)   color = ImVec4(1.0f, 0.45f, 0.45f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::TextUnformatted(entry.text.c_str());
        ImGui::PopStyleColor();
    }
    if (m_scrollToBottom || (m_autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())) {
        ImGui::SetScrollHereY(1.0f);
    }
    m_scrollToBottom = false;
    ImGui::EndChild();

    ImGui::Separator();
    ImGui::PushItemWidth(-1);
    const ImGuiInputTextFlags flags =
        ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_EscapeClearsAll;
    if (ImGui::InputTextWithHint("##input", "type a command and press Enter (try 'help')",
                                 m_inputBuffer, sizeof(m_inputBuffer), flags)) {
        Execute(m_inputBuffer);
        m_inputBuffer[0] = '\0';
        m_scrollToBottom = true;
        ImGui::SetKeyboardFocusHere(-1);
    }
    ImGui::PopItemWidth();
}

void Console::DrawPluginsTab() {
    if (!m_pluginManager) { ImGui::TextUnformatted("Plugin manager unavailable."); return; }

    const auto& plugins = m_pluginManager->Plugins();
    ImGui::Text("Loaded plugins: %d", static_cast<int>(plugins.size()));
    ImGui::Separator();

    if (plugins.empty()) {
        ImGui::TextWrapped("No plugins loaded. Drop compiled plugin DLLs into the "
                           "'plugins' folder next to the core, then run 'plugin_load' "
                           "or restart the game.");
        return;
    }

    for (const auto& p : plugins) {
        if (!p.instance) continue;
        ImGui::PushID(p.fileName.c_str());

        bool enabled = p.instance->IsEnabled();
        if (ImGui::Checkbox("##enabled", &enabled)) p.instance->SetEnabled(enabled);
        ImGui::SameLine();
        ImGui::Text("%s  v%s", p.instance->Name(), p.instance->Version());
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 70);
        if (ImGui::SmallButton("Reload")) m_pluginManager->Reload(p.fileName);

        ImGui::Indent();
        ImGui::TextDisabled("by %s  -  %s", p.instance->Author(), p.fileName.c_str());
        p.instance->OnRenderSettings();   // plugin-provided settings UI
        ImGui::Unindent();
        ImGui::Separator();

        ImGui::PopID();
    }
}

void Console::DrawAboutTab() {
    ImGui::TextUnformatted("RLModKit - educational plugin framework");
    ImGui::Separator();
    ImGui::TextWrapped(
        "A BakkesMod-style loader for LOCAL, non-competitive Rocket League modes "
        "(Freeplay, Training, Exhibition). It attaches only when Easy Anti-Cheat "
        "is not active and is intended for building minigame plugins and training "
        "overlays.");
    ImGui::Spacing();
    ImGui::BulletText("Press F1 to toggle this console.");
    ImGui::BulletText("Type 'help' for a list of commands.");
    ImGui::BulletText("Drop plugin DLLs in the 'plugins' folder.");
}

} // namespace rlmk
