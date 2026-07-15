#pragma once

namespace rlmk {

// Runtime self-check performed from inside the injected DLL. Even though the
// injector already refuses to attach under EAC, we re-verify from within the
// process on startup and periodically. If an anti-cheat module ever appears in
// our own process, we detach cleanly (unload the core) rather than continue.
//
// This keeps RLModKit strictly a non-competitive / local tool.
class EacGuard {
public:
    // Scans currently loaded modules of THIS process for known anti-cheat DLLs.
    // Returns true if any are present.
    static bool AntiCheatPresent();
};

} // namespace rlmk
