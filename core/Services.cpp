#include "Services.h"
#include "Console.h"
#include "Logger.h"

#include "imgui.h"

#include <fstream>
#include <sstream>

namespace rlmk {

Services::Services(Console* console) : m_console(console) {}

void Services::Log(const std::string& message)        { Logger::Get().Info(message); }
void Services::LogWarning(const std::string& message) { Logger::Get().Warning(message); }
void Services::LogError(const std::string& message)   { Logger::Get().Error(message); }

void Services::RegisterCommand(const std::string& name,
                               const std::string& description,
                               CommandHandler handler) {
    if (m_console) {
        m_console->RegisterCommand(name, description, std::move(handler));
    }
}

void Services::SetCvar(const std::string& key, const std::string& value) {
    m_cvars[key] = value;
    if (!m_cvarPath.empty()) SaveCvars(m_cvarPath);
}

std::string Services::GetCvar(const std::string& key, const std::string& fallback) {
    auto it = m_cvars.find(key);
    return it != m_cvars.end() ? it->second : fallback;
}

// ImGui uses ABGR packed color; the SDK exposes RGBA for convenience.
static ImU32 RgbaToImU32(uint32_t rgba) {
    const uint8_t r = (rgba >> 24) & 0xFF;
    const uint8_t g = (rgba >> 16) & 0xFF;
    const uint8_t b = (rgba >> 8) & 0xFF;
    const uint8_t a = (rgba) & 0xFF;
    return IM_COL32(r, g, b, a);
}

void Services::DrawText(float x, float y, const std::string& text, uint32_t rgba) {
    ImGui::GetForegroundDrawList()->AddText(ImVec2(x, y), RgbaToImU32(rgba), text.c_str());
}

void Services::DrawLine(float x1, float y1, float x2, float y2, uint32_t rgba, float thickness) {
    ImGui::GetForegroundDrawList()->AddLine(ImVec2(x1, y1), ImVec2(x2, y2),
                                            RgbaToImU32(rgba), thickness);
}

void Services::DrawRect(float x, float y, float w, float h, uint32_t rgba, bool filled) {
    auto* dl = ImGui::GetForegroundDrawList();
    if (filled) {
        dl->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h), RgbaToImU32(rgba));
    } else {
        dl->AddRect(ImVec2(x, y), ImVec2(x + w, y + h), RgbaToImU32(rgba));
    }
}

void Services::LoadCvars(const std::string& path) {
    m_cvarPath = path;
    std::ifstream in(path);
    if (!in) return;
    std::string line;
    while (std::getline(in, line)) {
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        m_cvars[line.substr(0, eq)] = line.substr(eq + 1);
    }
}

void Services::SaveCvars(const std::string& path) const {
    std::ofstream out(path, std::ios::trunc);
    if (!out) return;
    for (const auto& [k, v] : m_cvars) {
        out << k << '=' << v << '\n';
    }
}

} // namespace rlmk
