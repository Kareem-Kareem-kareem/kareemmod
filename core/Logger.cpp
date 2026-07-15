#include "Logger.h"

#include <cstdio>
#include <ctime>

namespace rlmk {

Logger& Logger::Get() {
    static Logger instance;
    return instance;
}

void Logger::Init(const std::string& logFilePath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_file) {
        m_file = std::fopen(logFilePath.c_str(), "w");
    }
}

void Logger::Shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_file) {
        std::fclose(static_cast<std::FILE*>(m_file));
        m_file = nullptr;
    }
    m_entries.clear();
}

static const char* LevelTag(LogLevel level) {
    switch (level) {
        case LogLevel::Warning: return "WARN";
        case LogLevel::Error:   return "ERR ";
        default:                return "INFO";
    }
}

void Logger::Push(LogLevel level, const std::string& msg) {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_entries.push_back({level, msg});
    if (m_entries.size() > m_maxEntries) {
        m_entries.erase(m_entries.begin(),
                        m_entries.begin() + (m_entries.size() - m_maxEntries));
    }

    if (m_file) {
        std::time_t t = std::time(nullptr);
        std::tm tm{};
        localtime_s(&tm, &t);
        char stamp[16];
        std::strftime(stamp, sizeof(stamp), "%H:%M:%S", &tm);
        std::fprintf(static_cast<std::FILE*>(m_file), "[%s][%s] %s\n",
                     stamp, LevelTag(level), msg.c_str());
        std::fflush(static_cast<std::FILE*>(m_file));
    }
}

void Logger::Info(const std::string& msg)    { Push(LogLevel::Info, msg); }
void Logger::Warning(const std::string& msg) { Push(LogLevel::Warning, msg); }
void Logger::Error(const std::string& msg)   { Push(LogLevel::Error, msg); }

std::vector<LogEntry> Logger::Snapshot() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_entries;
}

void Logger::Clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_entries.clear();
}

} // namespace rlmk
