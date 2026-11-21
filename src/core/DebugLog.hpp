#pragma once

#include <deque>
#include <mutex>
#include <string>
#include <vector>

namespace gdautopilot {

enum class LogLevel {
    Debug,
    Info,
    Warn,
    Error
};

struct LogEntry {
    LogLevel level = LogLevel::Info;
    std::string module;
    std::string message;
    double timestampSec = 0.0;
};

class DebugLog {
public:
    static DebugLog& get();

    void setVerbose(bool enabled);
    [[nodiscard]] bool isVerbose() const;

    void log(LogLevel level, std::string const& module, std::string const& message);
    void debug(std::string const& module, std::string const& message);
    void info(std::string const& module, std::string const& message);
    void warn(std::string const& module, std::string const& message);
    void error(std::string const& module, std::string const& message);

    [[nodiscard]] std::vector<LogEntry> getRecentEntries(size_t maxCount = 60) const;
    void clear();

private:
    DebugLog() = default;

    mutable std::mutex m_mutex;
    std::deque<LogEntry> m_entries;
    size_t m_maxEntries = 250;
    bool m_verbose = false;
};

} // namespace gdautopilot
