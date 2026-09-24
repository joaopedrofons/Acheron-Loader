#pragma once
#include <windows.h>
#include <string>
#include "logger.h"

enum class ExecutionMode { NORMAL, STEALTH, DEBUG, RECON_ONLY };

class Config
{
public:
    static Config& Instance();

    bool ParseCommandLine(int argc, wchar_t* argv[]);
    void ShowHelp() const;

    ExecutionMode GetMode() const { return m_mode; }
    const std::wstring& GetLogPath() const { return m_logPath; }
    const std::wstring& GetResultsPath() const { return m_resultsPath; }
    const std::wstring& GetPayloadPath() const { return m_payloadPath; }
    LogLevel GetMinLevel() const;
    bool IsPayloadEnabled() const { return m_payloadEnabled; }
    bool IsScannerEnabled() const { return m_scannerEnabled; }
    bool IsConsoleOutput() const { return m_consoleOutput; }
    DWORD GetMaxExecutionTime() const { return m_maxExecutionTime; } // ADICIONADO!

    void SetPayloadPath(const std::wstring& path) { m_payloadPath = path; }
    void SetLogPath(const std::wstring& path) { m_logPath = path; }
    void SetResultsPath(const std::wstring& path) { m_resultsPath = path; }
    void SetMaxExecutionTime(DWORD seconds) { m_maxExecutionTime = seconds; }

private:
    Config() = default;
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;

    std::wstring m_logPath = L"acheron.log";
    std::wstring m_resultsPath = L"recon_results.json";
    std::wstring m_payloadPath = L"payloads\\example_payload.dll";
    ExecutionMode m_mode = ExecutionMode::NORMAL;
    DWORD m_maxExecutionTime = 300;
    bool m_payloadEnabled = true;
    bool m_scannerEnabled = true;
    bool m_consoleOutput = true;
};