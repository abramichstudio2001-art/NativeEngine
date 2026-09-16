#include "engine/core/Log.hpp"
#include <iomanip>
#include <chrono>

namespace NativeEngine::Core {

Logger& Logger::GetInstance() {
    static Logger instance;
    return instance;
}

void Logger::Initialize() {
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_LogBuffer.clear();
    m_LogBuffer.reserve(m_MaxBufferEntries);
}

void Logger::Shutdown() {
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_LogBuffer.clear();
}

void Logger::Log(LogLevel level, std::string_view category, std::string_view message) {
    auto now = std::chrono::system_clock::now();
    auto nowUs = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();

    auto timeT = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &timeT);
#else
    localtime_r(&timeT, &tm);
#endif

    std::ostringstream ss;
    ss << std::put_time(&tm, "%H:%M:%S") << "." << std::setfill('0') << std::setw(6) << (nowUs % 1000000);
    std::string timestampStr = ss.str();

    const char* levelStr = "INFO";
    const char* colorCode = "\033[0m";

    switch (level) {
        case LogLevel::Trace:    levelStr = "TRACE"; colorCode = "\033[90m"; break;
        case LogLevel::Debug:    levelStr = "DEBUG"; colorCode = "\033[36m"; break;
        case LogLevel::Info:     levelStr = "INFO "; colorCode = "\033[32m"; break;
        case LogLevel::Warn:     levelStr = "WARN "; colorCode = "\033[33m"; break;
        case LogLevel::Error:    levelStr = "ERROR"; colorCode = "\033[31m"; break;
        case LogLevel::Critical: levelStr = "CRIT "; colorCode = "\033[35m"; break;
    }

    LogEntry entry{
        .level = level,
        .timestamp = timestampStr,
        .timestampUs = static_cast<uint64_t>(nowUs),
        .category = std::string(category),
        .message = std::string(message)
    };

    std::cout << colorCode << "[" << timestampStr << "] [" << levelStr << "] [" << category << "] " << message << "\033[0m\n";

    std::lock_guard<std::mutex> lock(m_Mutex);
    if (m_LogBuffer.size() >= m_MaxBufferEntries) {
        m_LogBuffer.erase(m_LogBuffer.begin());
    }
    m_LogBuffer.push_back(std::move(entry));
}

std::vector<LogEntry> Logger::GetRecentEntries(size_t maxCount) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    if (m_LogBuffer.size() <= maxCount) {
        return m_LogBuffer;
    }
    return std::vector<LogEntry>(m_LogBuffer.end() - maxCount, m_LogBuffer.end());
}

void Logger::Clear() {
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_LogBuffer.clear();
}

} // namespace NativeEngine::Core
