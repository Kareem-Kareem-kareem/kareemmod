# Building RLModKit

## Prerequisites

- **Windows 10/11** (the target and toolchain are Windows-only).
- **Visual Studio 2022** with the *Desktop development with C++* workload,
  or the standalone *Build Tools for Visual Studio*.
- **CMake 3.21+** (bundled with recent VS installs).
- **Git** — CMake fetches ImGui, MinHook and kiero automatically.

## Why 32-bit (Win32)?

`RocketLeague.exe` is a 32-bit process. A DLL can only be injected into a
process of the same architecture, so the core DLL **and** the example plugin
must be compiled as **x86 / Win32**. The injector is built x86 too for
simplicity.

Always configure with `-A Win32`:

```powershell
cmake -A Win32 -B build
cmake --build build --config Release
```

If you accidentally configure for x64, delete the `build/` folder and
re-run the configure step with `-A Win32`.

## Output layout

```
build/bin/
├── RLModKit.exe          # injector
├── RLModKitCore.dll      # injected core
├── rlmodkit.log          # created at runtime next to the core
├── cvars.cfg             # created at runtime (persisted settings)
└── plugins/
    └── SpeedClock.dll     # example plugin, auto-loaded
```

The injector looks for `RLModKitCore.dll` in its own directory by default, so
keep the exe and dll together. You can also pass an explicit path:

```powershell
RLModKit.exe C:\path\to\RLModKitCore.dll
```

## Common issues

| Symptom | Fix |
|---------|-----|
| `kiero failed to init D3D11` | Make sure the game window has rendered at least one frame before injecting, and that RL is using the DX11 renderer. |
| Injector says *anti-cheat detected* | Launch RL without EAC (`-noeac`) and use a local/offline mode. |
| `OpenProcess failed` | Run the injector as administrator. |
| Console (F1) doesn't appear | Check `rlmodkit.log`. The overlay only initializes after the first hooked Present frame. |
| Plugin not loading | Confirm it exports `CreatePlugin` and `GetPluginSdkVersion`, and was built x86 against the same SDK version. |

## Editing on non-Windows

You can browse and edit the sources anywhere, but the DLL/injector only build
on Windows/MSVC because they depend on the Win32 API and DirectX. The CMake
configure step will warn if run on another platform.
