#include "EacGuard.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#include <array>
#include <cwchar>

namespace rlmk {

bool EacGuard::AntiCheatPresent() {
    static const std::array<const wchar_t*, 5> kAntiCheat = {
        L"EasyAntiCheat.dll",
        L"EasyAntiCheat_x86.dll",
        L"EasyAntiCheat_x64.dll",
        L"EasyAntiCheat_EOS.dll",
        L"EasyAntiCheatSys.sys",
    };

    HMODULE modules[1024];
    DWORD needed = 0;
    HANDLE self = GetCurrentProcess();
    if (!EnumProcessModulesEx(self, modules, sizeof(modules), &needed, LIST_MODULES_ALL)) {
        return false;
    }

    const DWORD count = needed / sizeof(HMODULE);
    for (DWORD i = 0; i < count; ++i) {
        wchar_t name[MAX_PATH];
        if (GetModuleBaseNameW(self, modules[i], name, MAX_PATH)) {
            for (const auto* ac : kAntiCheat) {
                if (_wcsicmp(name, ac) == 0) {
                    return true;
                }
            }
        }
    }
    return false;
}

} // namespace rlmk
