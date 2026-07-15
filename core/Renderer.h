#pragma once

namespace rlmk {

class Console;
class PluginManager;

// Owns the DirectX11 Present() hook and the ImGui context. Each hooked frame it:
//   1. Ensures ImGui is initialized against the game's swapchain/device.
//   2. Starts an ImGui frame.
//   3. Lets plugins draw overlays and draws the F1 console.
//   4. Renders ImGui on top of the game's back buffer.
//
// Input is captured via a WndProc subclass so the console can eat mouse/keyboard
// while it is open (and F1 always toggles it).
class Renderer {
public:
    static Renderer& Get();

    // Wire up dependencies before hooking.
    void Bind(Console* console, PluginManager* pluginManager);

    // Install the Present hook (via kiero). Returns false on failure.
    bool Hook();

    // Remove the hook and shut down ImGui. Safe to call on unload.
    void Unhook();

    // Set by dllmain: when true, the DLL is tearing down and the hook should
    // stop touching game/ImGui resources.
    void RequestShutdown() { m_shuttingDown = true; }
    bool IsShuttingDown() const { return m_shuttingDown; }

    Console* GetConsole() const { return m_console; }
    PluginManager* GetPluginManager() const { return m_pluginManager; }

    bool ImGuiInitialized() const { return m_imguiInit; }
    void SetImGuiInitialized(bool v) { m_imguiInit = v; }

private:
    Renderer() = default;

    Console* m_console = nullptr;
    PluginManager* m_pluginManager = nullptr;
    bool m_hooked = false;
    bool m_imguiInit = false;
    bool m_shuttingDown = false;
};

} // namespace rlmk
