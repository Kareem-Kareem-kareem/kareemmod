#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <cstdint>

namespace rlmk {

enum class LogLevel { Info, Warning, Error };

struct LogEntry {
    LogLevel level;
    std::string text;
};

// Thread-safe ring buffer of log lines. The console reads from it; plugins and
// core systems write to it. Also mirrors everything to a rotating file.
class Logger {
public:
    static Logger& Get();

    void Init(const std::string& logFilePath);
    void Shutdown();

    void Info(const std::string& msg);
    void Warning(const std::string& msg);
    void Error(const std::string& msg);

    // Snapshot copy for rendering (avoids holding the lock while drawing).
    std::vector<LogEntry> Snapshot();
    void Clear();

private:
    void Push(LogLevel level, const std::string& msg);

    std::mutex m_mutex;
    std::vector<LogEntry> m_entries;
    size_t m_maxEntries = 2000;
    void* m_file = nullptr;   // FILE* kept opaque to avoid <cstdio> in header
};

} // namespace rlmk
