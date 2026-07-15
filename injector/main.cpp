// ============================================================================
//  RLModKit Injector
// ----------------------------------------------------------------------------
//  Standalone launcher/injector. Its job:
//    1. Find the running RocketLeague.exe process.
//    2. HARD-REFUSE to attach if Easy Anti-Cheat is active. This tool is for
//       local, non-competitive modes only (Freeplay, Training, Exhibition).
//    3. Load RLModKitCore.dll into the game via the standard, fully documented
//       LoadLibrary + CreateRemoteThread technique (same approach BakkesMod and
//       most single-player mod loaders use).
//
//  This is intentionally simple and transparent - there is no attempt to hide
//  the module, evade detection, or tamper with anti-cheat. If EAC is present,
//  we bail out.
// ============================================================================
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>

#include <string>
#include <vector>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

namespace {

constexpr wchar_t kGameProcess[] = L"RocketLeague.exe";
constexpr char    kCoreDll[]     = "RLModKitCore.dll";

// Modules that indicate an anti-cheat protected session. If any are present in
// the target process (or running system-wide as the EAC service), we refuse.
const std::vector<std::wstring> kAntiCheatModules = {
    L"EasyAntiCheat.dll",
    L"EasyAntiCheat_x86.dll",
    L"EasyAntiCheat_x64.dll",
    L"EasyAntiCheat_EOS.dll",
    L"EasyAntiCheatSys.sys",
};

void PrintBanner() {
    std::cout <<
        "==============================================================\n"
        "  RLModKit Injector (educational, non-EAC modes only)\n"
        "  Attaches only to local Freeplay / Training / Exhibition.\n"
        "==============================================================\n";
}

DWORD FindProcessId(const std::wstring& name) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    DWORD pid = 0;

    if (Process32FirstW(snap, &entry)) {
        do {
            if (name == entry.szExeFile) {
                pid = entry.th32ProcessID;
                break;
            }
        } while (Process32NextW(snap, &entry));
    }
    CloseHandle(snap);
    return pid;
}

// Returns true if any anti-cheat module is loaded in the target process.
bool ProcessHasAntiCheat(HANDLE process) {
    HMODULE modules[1024];
    DWORD needed = 0;
    if (!EnumProcessModulesEx(process, modules, sizeof(modules), &needed, LIST_MODULES_ALL)) {
        // If we cannot enumerate, fail safe: assume protected.
        std::cerr << "[!] Could not enumerate target modules; assuming protected.\n";
        return true;
    }

    const DWORD count = needed / sizeof(HMODULE);
    for (DWORD i = 0; i < count; ++i) {
        wchar_t modName[MAX_PATH];
        if (GetModuleBaseNameW(process, modules[i], modName, MAX_PATH)) {
            for (const auto& ac : kAntiCheatModules) {
                if (_wcsicmp(modName, ac.c_str()) == 0) {
                    return true;
                }
            }
        }
    }
    return false;
}

// Returns true if the EAC service/launcher is running anywhere on the system.
bool SystemHasAntiCheatProcess() {
    static const std::vector<std::wstring> kAcProcesses = {
        L"EasyAntiCheat.exe",
        L"EasyAntiCheat_EOS.exe",
        L"EasyAntiCheat_Setup.exe",
        L"start_protected_game.exe",
    };

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    bool found = false;

    if (Process32FirstW(snap, &entry)) {
        do {
            for (const auto& ac : kAcProcesses) {
                if (_wcsicmp(entry.szExeFile, ac.c_str()) == 0) {
                    found = true;
                    break;
                }
            }
        } while (!found && Process32NextW(snap, &entry));
    }
    CloseHandle(snap);
    return found;
}

bool InjectDll(HANDLE process, const std::string& dllPath) {
    const SIZE_T size = dllPath.size() + 1;

    LPVOID remoteMem = VirtualAllocEx(process, nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remoteMem) {
        std::cerr << "[!] VirtualAllocEx failed: " << GetLastError() << "\n";
        return false;
    }

    if (!WriteProcessMemory(process, remoteMem, dllPath.c_str(), size, nullptr)) {
        std::cerr << "[!] WriteProcessMemory failed: " << GetLastError() << "\n";
        VirtualFreeEx(process, remoteMem, 0, MEM_RELEASE);
        return false;
    }

    // LoadLibraryA lives in kernel32, which is mapped at the same base in every
    // process, so its address in our process is valid in the target too.
    auto loadLibrary = reinterpret_cast<LPTHREAD_START_ROUTINE>(
        GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryA"));
    if (!loadLibrary) {
        std::cerr << "[!] Could not resolve LoadLibraryA.\n";
        VirtualFreeEx(process, remoteMem, 0, MEM_RELEASE);
        return false;
    }

    HANDLE thread = CreateRemoteThread(process, nullptr, 0, loadLibrary, remoteMem, 0, nullptr);
    if (!thread) {
        std::cerr << "[!] CreateRemoteThread failed: " << GetLastError() << "\n";
        VirtualFreeEx(process, remoteMem, 0, MEM_RELEASE);
        return false;
    }

    WaitForSingleObject(thread, 10000);

    DWORD exitCode = 0;
    GetExitCodeThread(thread, &exitCode);   // non-zero == HMODULE of loaded DLL (x86)

    CloseHandle(thread);
    VirtualFreeEx(process, remoteMem, 0, MEM_RELEASE);

    if (exitCode == 0) {
        std::cerr << "[!] LoadLibraryA returned NULL inside the target.\n";
        return false;
    }
    return true;
}

} // namespace

int main(int argc, char** argv) {
    PrintBanner();

    // Resolve the core DLL path (same directory as this exe by default).
    fs::path exeDir = fs::absolute(fs::path(argv[0])).parent_path();
    fs::path dllPath = exeDir / kCoreDll;
    if (argc > 1) {
        dllPath = fs::absolute(argv[1]);
    }

    if (!fs::exists(dllPath)) {
        std::cerr << "[!] Core DLL not found: " << dllPath.string() << "\n";
        std::cerr << "    Build the 'core' target first, or pass the path as arg 1.\n";
        return 1;
    }

    // --- Safety gate: never touch an anti-cheat protected session -----------
    if (SystemHasAntiCheatProcess()) {
        std::cerr << "\n[X] Easy Anti-Cheat is running on this system.\n"
                     "    RLModKit will NOT attach. Launch Rocket League WITHOUT EAC\n"
                     "    (e.g. Steam launch option \"-noeac\" for local play) and retry.\n";
        return 2;
    }

    std::cout << "[*] Looking for RocketLeague.exe...\n";
    DWORD pid = FindProcessId(kGameProcess);
    if (pid == 0) {
        std::cerr << "[!] RocketLeague.exe is not running. Start the game first.\n";
        return 3;
    }
    std::cout << "[*] Found game (PID " << pid << ").\n";

    HANDLE process = OpenProcess(
        PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
        PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
        FALSE, pid);
    if (!process) {
        std::cerr << "[!] OpenProcess failed: " << GetLastError()
                  << " (try running as administrator).\n";
        return 4;
    }

    if (ProcessHasAntiCheat(process)) {
        std::cerr << "\n[X] Anti-cheat module detected inside RocketLeague.exe.\n"
                     "    Refusing to inject. Use a local, non-EAC session only.\n";
        CloseHandle(process);
        return 5;
    }

    std::cout << "[*] No anti-cheat detected. Injecting core...\n";
    const bool ok = InjectDll(process, dllPath.string());
    CloseHandle(process);

    if (!ok) {
        std::cerr << "[!] Injection failed.\n";
        return 6;
    }

    std::cout << "[+] RLModKitCore loaded. Press F1 in-game to open the console.\n";
    return 0;
}
