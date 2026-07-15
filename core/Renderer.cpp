#include "Renderer.h"
#include "Console.h"
#include "PluginManager.h"
#include "Logger.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>

#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx11.h"
#include "kiero.h"

// Forward decl of ImGui's Win32 message handler.
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace rlmk {

namespace {
    // Cached DirectX resources discovered on the first hooked frame.
    ID3D11Device*           g_device = nullptr;
    ID3D11DeviceContext*    g_context = nullptr;
    ID3D11RenderTargetView* g_rtv = nullptr;
    HWND                    g_window = nullptr;
    WNDPROC                 g_originalWndProc = nullptr;

    // Present() vtable index for IDXGISwapChain is 8.
    constexpr int kPresentIndex = 8;

    using PresentFn = HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT);
    PresentFn g_originalPresent = nullptr;

    void CreateRenderTarget(IDXGISwapChain* swapChain) {
        ID3D11Texture2D* backBuffer = nullptr;
        if (SUCCEEDED(swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D),
                                           reinterpret_cast<void**>(&backBuffer))) && backBuffer) {
            g_device->CreateRenderTargetView(backBuffer, nullptr, &g_rtv);
            backBuffer->Release();
        }
    }

    // Our window procedure: feed input to ImGui, toggle console on F1, and eat
    // input while the console is visible so it doesn't leak into the game.
    LRESULT WINAPI HookedWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        auto& renderer = Renderer::Get();

        if (msg == WM_KEYDOWN && wParam == VK_F1) {
            if (renderer.GetConsole()) renderer.GetConsole()->Toggle();
            return 0;
        }

        if (ImGui::GetCurrentContext()) {
            ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
            const bool consoleOpen = renderer.GetConsole() && renderer.GetConsole()->IsVisible();
            if (consoleOpen) {
                ImGuiIO& io = ImGui::GetIO();
                // Swallow input the console is using.
                if (io.WantCaptureMouse &&
                    (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP ||
                     msg == WM_RBUTTONDOWN || msg == WM_RBUTTONUP ||
                     msg == WM_MBUTTONDOWN || msg == WM_MBUTTONUP ||
                     msg == WM_MOUSEWHEEL  || msg == WM_MOUSEMOVE)) {
                    return true;
                }
                if (io.WantCaptureKeyboard &&
                    (msg == WM_KEYDOWN || msg == WM_KEYUP || msg == WM_CHAR)) {
                    return true;
                }
            }
        }

        return CallWindowProc(g_originalWndProc, hWnd, msg, wParam, lParam);
    }

    void InitImGui(IDXGISwapChain* swapChain) {
        DXGI_SWAP_CHAIN_DESC desc{};
        swapChain->GetDesc(&desc);
        g_window = desc.OutputWindow;

        if (FAILED(swapChain->GetDevice(__uuidof(ID3D11Device),
                                        reinterpret_cast<void**>(&g_device)))) {
            Logger::Get().Error("Renderer: failed to get D3D11 device from swapchain.");
            return;
        }
        g_device->GetImmediateContext(&g_context);

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;            // don't write imgui.ini into the game folder
        ImGui::StyleColorsDark();

        ImGui_ImplWin32_Init(g_window);
        ImGui_ImplDX11_Init(g_device, g_context);

        CreateRenderTarget(swapChain);

        // Subclass the game window so we receive input.
        g_originalWndProc = reinterpret_cast<WNDPROC>(
            SetWindowLongPtr(g_window, GWLP_WNDPROC,
                             reinterpret_cast<LONG_PTR>(HookedWndProc)));

        Renderer::Get().SetImGuiInitialized(true);
        Logger::Get().Info("Renderer: ImGui initialized, F1 opens the console.");
    }

    HRESULT __stdcall HookedPresent(IDXGISwapChain* swapChain, UINT syncInterval, UINT flags) {
        auto& renderer = Renderer::Get();

        if (renderer.IsShuttingDown()) {
            return g_originalPresent(swapChain, syncInterval, flags);
        }

        if (!renderer.ImGuiInitialized()) {
            InitImGui(swapChain);
        }
        if (!g_rtv) {
            CreateRenderTarget(swapChain);
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // Plugin overlays (they draw to the foreground draw list via Services).
        if (renderer.GetPluginManager()) renderer.GetPluginManager()->Render();

        // The F1 console window.
        if (renderer.GetConsole()) renderer.GetConsole()->Render();

        ImGui::Render();
        if (g_rtv) {
            g_context->OMSetRenderTargets(1, &g_rtv, nullptr);
        }
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        return g_originalPresent(swapChain, syncInterval, flags);
    }
} // namespace

Renderer& Renderer::Get() {
    static Renderer instance;
    return instance;
}

void Renderer::Bind(Console* console, PluginManager* pluginManager) {
    m_console = console;
    m_pluginManager = pluginManager;
}

bool Renderer::Hook() {
    if (m_hooked) return true;

    if (kiero::init(kiero::RenderType::D3D11) != kiero::Status::Success) {
        Logger::Get().Error("Renderer: kiero failed to init D3D11 (is the game using DX11?).");
        return false;
    }

    if (kiero::bind(kPresentIndex,
                    reinterpret_cast<void**>(&g_originalPresent),
                    reinterpret_cast<void*>(HookedPresent)) != kiero::Status::Success) {
        Logger::Get().Error("Renderer: kiero failed to bind Present hook.");
        kiero::shutdown();
        return false;
    }

    m_hooked = true;
    Logger::Get().Info("Renderer: Present() hooked.");
    return true;
}

void Renderer::Unhook() {
    m_shuttingDown = true;

    if (m_hooked) {
        kiero::shutdown();      // restores the original vtable entry
        m_hooked = false;
    }

    if (g_window && g_originalWndProc) {
        SetWindowLongPtr(g_window, GWLP_WNDPROC,
                         reinterpret_cast<LONG_PTR>(g_originalWndProc));
        g_originalWndProc = nullptr;
    }

    if (m_imguiInit) {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        m_imguiInit = false;
    }

    if (g_rtv)     { g_rtv->Release();     g_rtv = nullptr; }
    if (g_context) { g_context->Release(); g_context = nullptr; }
    if (g_device)  { g_device->Release();  g_device = nullptr; }

    Logger::Get().Info("Renderer: unhooked and cleaned up.");
}

} // namespace rlmk
