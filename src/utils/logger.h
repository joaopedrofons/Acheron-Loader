#pragma once
#include <windows.h>
#include <string>
#include <fstream>

enum class LogLevel { Debug = 0, Info, Warn, Error, Fatal };

class Logger
{
public:
    static Logger& Instance();

    bool Initialize(const std::wstring& logPath = L"acheron.log",
        LogLevel minLevel = LogLevel::Info,
        bool consoleOutput = true);
    void Shutdown();
    void SetMinLevel(LogLevel level);
    void SetConsoleOutput(bool enabled);

    void Log(LogLevel level, const wchar_t* format, ...);
    void Debug(const wchar_t* format, ...);
    void Info(const wchar_t* format, ...);
    void Warn(const wchar_t* format, ...);
    void Error(const wchar_t* format, ...);
    void Fatal(const wchar_t* format, ...);

    static std::wstring GetTimestamp();
    static const wchar_t* LevelToString(LogLevel level);

private:
    Logger() = default;
    ~Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    std::wofstream m_file;
    LogLevel       m_minLevel = LogLevel::Info;
    bool           m_consoleOutput = true;
    bool           m_initialized = false;
};

#define LOG_DEBUG(...) Logger::Instance().Debug(__VA_ARGS__)
#define LOG_INFO(...)  Logger::Instance().Info (__VA_ARGS__)
#define LOG_WARN(...)  Logger::Instance().Warn (__VA_ARGS__)
#define LOG_ERROR(...) Logger::Instance().Error(__VA_ARGS__)
#define LOG_FATAL(...) Logger::Instance().Fatal(__VA_ARGS__)