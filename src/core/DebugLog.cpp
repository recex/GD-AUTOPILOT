#include "core/DebugLog.hpp"

#include <chrono>
#include <cstdio>

#ifndef GD_AUTOPILOT_STANDALONE
#include <Geode/loader/Log.hpp>
#endif

namespace gdautopilot {

namespace {
double nowSeconds() {
    using clock = std::chrono::steady_clock;
    static const auto start = clock::now();
    auto elapsed = std::chrono::duration<double>(clock::now() - start);
    return elapsed.count();
}
} // namespace

DebugLog& DebugLog::get() {
    static DebugLog instance;
    return instance;
}

void DebugLog::setVerbose(bool enabled) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_verbose = enabled;
}

bool DebugLog::isVerbose() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_verbose;
}

void DebugLog::log(LogLevel level, std::string const& module, std::string const& message) {
    if (level == LogLevel::Debug && !isVerbose()) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_entries.push_back(LogEntry{
            .level = level,
            .module = module,
            .message = message,
            .timestampSec = nowSeconds()
        });
        while (m_entries.size() > m_maxEntries) {
            m_entries.pop_front();
        }
    }

#ifndef GD_AUTOPILOT_STANDALONE
    switch (level) {
        case LogLevel::Debug:
            geode::log::debug("[{}] {}", module, message);
            break;
        case LogLevel::Info:
            geode::log::info("[{}] {}", module, message);
            break;
        case LogLevel::Warn:
            geode::log::warn("[{}] {}", module, message);
            break;
        case LogLevel::Error:
            geode::log::error("[{}] {}", module, message);
            break;
    }
#else
    if (level == LogLevel::Error) {
        std::fprintf(stderr, "[ERROR][%s] %s\n", module.c_str(), message.c_str());
    }
#endif
}

void DebugLog::debug(std::string const& module, std::string const& message) {
    log(LogLevel::Debug, module, message);
}

void DebugLog::info(std::string const& module, std::string const& message) {
    log(LogLevel::Info, module, message);
}

void DebugLog::warn(std::string const& module, std::string const& message) {
    log(LogLevel::Warn, module, message);
}

void DebugLog::error(std::string const& module, std::string const& message) {
    log(LogLevel::Error, module, message);
}

std::vector<LogEntry> DebugLog::getRecentEntries(size_t maxCount) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<LogEntry> out;
    if (m_entries.empty()) return out;

    size_t count = std::min(maxCount, m_entries.size());
    out.reserve(count);
    auto beginIt = m_entries.end() - static_cast<std::ptrdiff_t>(count);
    for (auto it = beginIt; it != m_entries.end(); ++it) {
        out.push_back(*it);
    }
    return out;
}

void DebugLog::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_entries.clear();
}

} // namespace gdautopilot
