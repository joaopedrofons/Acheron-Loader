#include "logger.h"
#include <cstdarg>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <vector>
#include <cstdio>

Logger& Logger::Instance()
{
    static Logger inst;
    return inst;
}

bool Logger::Initialize(const std::wstring& logPath, LogLevel minLevel, bool consoleOutput)
{
    if (m_initialized) return true;

    m_minLevel = minLevel;
    m_consoleOutput = consoleOutput;
    m_file.open(logPath, std::ios::out | std::ios::app);
    m_initialized = true;

    if (m_file.is_open())
    {
        m_file << L"[INFO] Logger inicializado: " << logPath << L'\n';
        m_file.flush();
    }
    if (m_consoleOutput)
    {
        std::wstring msg = L"[INFO] Logger inicializado: " + logPath + L"\n";
        fputws(msg.c_str(), stdout);
    }
    return true;
}

void Logger::Shutdown()
{
    if (!m_initialized) return;
    if (m_file.is_open()) { m_file.flush(); m_file.close(); }
    m_initialized = false;
}

void Logger::SetMinLevel(LogLevel level) { m_minLevel = level; }
void Logger::SetConsoleOutput(bool enabled) { m_consoleOutput = enabled; }

std::wstring Logger::GetTimestamp()
{
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    std::tm tm{};
    localtime_s(&tm, &t);
    std::wostringstream oss;
    oss << std::put_time(&tm, L"%Y-%m-%d %H:%M:%S")
        << L'.' << std::setfill(L'0') << std::setw(3) << ms.count();
    return oss.str();
}

const wchar_t* Logger::LevelToString(LogLevel level)
{
    switch (level)
    {
    case LogLevel::Debug: return L"DEBUG";
    case LogLevel::Info:  return L"INFO";
    case LogLevel::Warn:  return L"WARN";
    case LogLevel::Error: return L"ERROR";
    case LogLevel::Fatal: return L"FATAL";
    default:              return L"?";
    }
}

// ======================================================================
// MÉTODO PRINCIPAL DE LOG (SEM BUFFER FIXO)
// ======================================================================
void Logger::Log(LogLevel level, const wchar_t* format, ...)
{
    if (!m_initialized || level < m_minLevel) return;
    if (format == nullptr) return;

    va_list args;
    va_start(args, format);

    // Calcula o tamanho necessário (com alocação dinâmica)
    int len = _vscwprintf(format, args);
    if (len < 0) { va_end(args); return; }

    std::wstring buffer(len + 1, L'\0');
    va_start(args, format); // reinicia a lista (importante)
    vswprintf_s(&buffer[0], buffer.size(), format, args);
    va_end(args);

    buffer.resize(len);
    std::wstring full = L"[" + GetTimestamp() + L"] [" + LevelToString(level) + L"] " + buffer;

    if (m_file.is_open())
    {
        m_file << full << L'\n';
        m_file.flush();
    }
    if (m_consoleOutput)
    {
        // Usa fputws (seguro para wide strings)
        std::wstring line = full + L"\n";
        fputws(line.c_str(), stdout);
    }
}

// ======================================================================
// WRAPPERS
// ======================================================================
void Logger::Debug(const wchar_t* format, ...)
{
    va_list a; va_start(a, format);
    int len = _vscwprintf(format, a);
    if (len < 0) { va_end(a); return; }
    std::wstring buffer(len + 1, L'\0');
    va_start(a, format);
    vswprintf_s(&buffer[0], buffer.size(), format, a);
    va_end(a);
    buffer.resize(len);
    Log(LogLevel::Debug, L"%ls", buffer.c_str());
}

void Logger::Info(const wchar_t* format, ...)
{
    va_list a; va_start(a, format);
    int len = _vscwprintf(format, a);
    if (len < 0) { va_end(a); return; }
    std::wstring buffer(len + 1, L'\0');
    va_start(a, format);
    vswprintf_s(&buffer[0], buffer.size(), format, a);
    va_end(a);
    buffer.resize(len);
    Log(LogLevel::Info, L"%ls", buffer.c_str());
}

void Logger::Warn(const wchar_t* format, ...)
{
    va_list a; va_start(a, format);
    int len = _vscwprintf(format, a);
    if (len < 0) { va_end(a); return; }
    std::wstring buffer(len + 1, L'\0');
    va_start(a, format);
    vswprintf_s(&buffer[0], buffer.size(), format, a);
    va_end(a);
    buffer.resize(len);
    Log(LogLevel::Warn, L"%ls", buffer.c_str());
}

void Logger::Error(const wchar_t* format, ...)
{
    va_list a; va_start(a, format);
    int len = _vscwprintf(format, a);
    if (len < 0) { va_end(a); return; }
    std::wstring buffer(len + 1, L'\0');
    va_start(a, format);
    vswprintf_s(&buffer[0], buffer.size(), format, a);
    va_end(a);
    buffer.resize(len);
    Log(LogLevel::Error, L"%ls", buffer.c_str());
}

void Logger::Fatal(const wchar_t* format, ...)
{
    va_list a; va_start(a, format);
    int len = _vscwprintf(format, a);
    if (len < 0) { va_end(a); return; }
    std::wstring buffer(len + 1, L'\0');
    va_start(a, format);
    vswprintf_s(&buffer[0], buffer.size(), format, a);
    va_end(a);
    buffer.resize(len);
    Log(LogLevel::Fatal, L"%ls", buffer.c_str());
}