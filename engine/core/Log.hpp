#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <chrono>
#include <iostream>
#include <sstream>
#include <format>
#include <string_view>

namespace NativeEngine::Core {

enum class LogLevel {
    Trace,
    Debug,
    Info,
    Warn,
    Error,
    Critical
};

struct LogEntry {
    LogLevel level;
    std::string timestamp;
    uint64_t timestampUs;
    std::string category;
    std::string message;
};

class Logger {
public:
    static Logger& GetInstance();

    void Initialize();
    void Shutdown();

    void Log(LogLevel level, std::string_view category, std::string_view message);

    template<typename... Args>
    void Trace(std::string_view category, std::format_string<Args...> fmt, Args&&... args) {
        Log(LogLevel::Trace, category, std::format(fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    void Debug(std::string_view category, std::format_string<Args...> fmt, Args&&... args) {
        Log(LogLevel::Debug, category, std::format(fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    void Info(std::string_view category, std::format_string<Args...> fmt, Args&&... args) {
        Log(LogLevel::Info, category, std::format(fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    void Warn(std::string_view category, std::format_string<Args...> fmt, Args&&... args) {
        Log(LogLevel::Warn, category, std::format(fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    void Error(std::string_view category, std::format_string<Args...> fmt, Args&&... args) {
        Log(LogLevel::Error, category, std::format(fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    void Critical(std::string_view category, std::format_string<Args...> fmt, Args&&... args) {
        Log(LogLevel::Critical, category, std::format(fmt, std::forward<Args>(args)...));
    }

    std::vector<LogEntry> GetRecentEntries(size_t maxCount = 100);
    void Clear();

private:
    Logger() = default;
    ~Logger() = default;

    std::mutex m_Mutex;
    std::vector<LogEntry> m_LogBuffer;
    size_t m_MaxBufferEntries = 5000;
};

} // namespace NativeEngine::Core

#define NE_LOG_TRACE(category, ...) ::NativeEngine::Core::Logger::GetInstance().Trace(category, __VA_ARGS__)
#define NE_LOG_DEBUG(category, ...) ::NativeEngine::Core::Logger::GetInstance().Debug(category, __VA_ARGS__)
#define NE_LOG_INFO(category, ...)  ::NativeEngine::Core::Logger::GetInstance().Info(category, __VA_ARGS__)
#define NE_LOG_WARN(category, ...)  ::NativeEngine::Core::Logger::GetInstance().Warn(category, __VA_ARGS__)
#define NE_LOG_ERROR(category, ...) ::NativeEngine::Core::Logger::GetInstance().Error(category, __VA_ARGS__)
#define NE_LOG_CRITICAL(category, ...) ::NativeEngine::Core::Logger::GetInstance().Critical(category, __VA_ARGS__)
