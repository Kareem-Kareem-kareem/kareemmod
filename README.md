# RLModKit

An educational, BakkesMod-style **plugin framework** for Rocket League, built for
**local, non-competitive modes only** (Freeplay, Training, Local/Exhibition
matches). It attaches a developer console + overlay to the game so you can build
and run your own **minigame plugins**.

Press **F1** in-game to open the console.

> ### Scope & fair play
> RLModKit is designed for offline/local experimentation and learning about game
> tooling. It **refuses to attach when Easy Anti-Cheat is active** — both the
> injector and the injected DLL check for EAC and bail out, and a runtime
> watchdog detaches the toolkit if anti-cheat ever appears. Do not use it in
> ranked/online play. Modding your game may violate the game's Terms of Service;
> use at your own risk on your own machine.

---

## What's in the box

| Component | Output | Description |
|-----------|--------|-------------|
| **Injector** | `RLModKit.exe` | Finds `RocketLeague.exe`, verifies no EAC, injects the core. |
| **Core** | `RLModKitCore.dll` | Hooks DirectX 11, renders the ImGui F1 console/overlay, loads plugins. |
| **SDK** | `sdk/PluginSDK.h` | Header-only API your minigame plugins implement. |
| **Example plugin** | `plugins/example` | `SpeedClock`, a training overlay showing how the SDK works. |

### Architecture

```
RLModKit.exe ──inject──▶ RocketLeague.exe
                              │
                        RLModKitCore.dll
                          ├── Renderer   (DX11 Present hook + ImGui, F1 toggle)
                          ├── Console    (command line + plugin manager UI)
                          ├── Services   (logging, cvars, drawing, game state)
                          └── PluginManager
                                └── loads ./plugins/*.dll  (your minigames)
```

---

## Building (Windows + Visual Studio)

Rocket League is a **32-bit** process, so everything must be built for **x86 (Win32)**.

**Requirements**
- Windows 10/11
- Visual Studio 2022 (Desktop C++ workload) or Build Tools
- CMake 3.21+
- Git (dependencies are fetched automatically)

```powershell
# from the repo root
cmake -A Win32 -B build
cmake --build build --config Release
```

Artifacts land in `build/bin/`:

```
build/bin/
├── RLModKit.exe          # the injector
├── RLModKitCore.dll      # the core
└── plugins/
    └── SpeedClock.dll     # example plugin (auto-loaded)
```

Dependencies (Dear ImGui, MinHook, kiero) are pulled via CMake `FetchContent`
on first configure — no manual setup needed.

---

## Running

1. Launch Rocket League **without Easy Anti-Cheat** (e.g. the Steam launch
   option `-noeac`, used for local/freeplay play). If EAC is running, the
   injector will refuse.
2. Enter **Freeplay**, **Training**, or a **Local/Exhibition** match.
3. Run the injector (as administrator if needed):
   ```powershell
   build\bin\RLModKit.exe
   ```
4. Press **F1** in-game to open the console.

Type `help` in the console for the full command list.

### Built-in console commands

| Command | Description |
|---------|-------------|
| `help` | List all commands. |
| `clear` | Clear the console log. |
| `plugin_list` | List loaded plugins. |
| `plugin_load <path.dll>` | Load a plugin at runtime. |
| `plugin_unload <file.dll>` | Unload a plugin. |
| `plugin_reload <file.dll>` | Hot-reload a plugin (great while developing). |
| `plugin_toggle <name>` | Enable/disable a plugin. |
| `unload` | Detach RLModKit from the game. |

---

## Writing your own minigame plugin

A plugin is just a DLL that implements `rlmk::IPlugin` and exports a factory.
Start by copying `plugins/example`.

```cpp
#include "PluginSDK.h"
using namespace rlmk;

class MyMinigame final : public IPlugin {
public:
    const char* Name()    const override { return "My Minigame"; }
    const char* Author()  const override { return "you"; }
    const char* Version() const override { return "0.1.0"; }

    void OnLoad(IServices* s) override {
        m_s = s;
        s->RegisterCommand("mygame", "start my minigame",
            [this](const std::vector<std::string>&){ m_s->Log("hello!"); });
    }

    void OnTick(const GameState& st) override { /* game logic */ }
    void OnRender() override { m_s->DrawText(60, 120, "My Minigame HUD"); }

private:
    IServices* m_s = nullptr;
};

extern "C" __declspec(dllexport) IPlugin* CreatePlugin() { return new MyMinigame(); }
extern "C" __declspec(dllexport) int GetPluginSdkVersion() { return RLMODKIT_SDK_VERSION; }
```

Add it to the build:

```cmake
add_library(MyMinigame SHARED MyMinigame.cpp)
set_target_properties(MyMinigame PROPERTIES OUTPUT_NAME "MyMinigame" PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY "${RLMK_PLUGIN_DIR}")
target_link_libraries(MyMinigame PRIVATE rlmk_sdk)
```

Rebuild, then use `plugin_reload MyMinigame.dll` in the console to iterate
without restarting the game.

See [`docs/BUILDING.md`](docs/BUILDING.md) for details and
[`docs/PLUGINS.md`](docs/PLUGINS.md) for the full SDK reference.

---

## Note on reading live game state

The `GameState` passed to plugins exposes timing and a placeholder car/ball
state. Populating **real** ball/car values requires Rocket League SDK offsets
that are game-version specific and are intentionally **not** bundled here. The
framework, console, overlay, and plugin system are fully functional; wiring a
game-state adapter is left as the next step.
