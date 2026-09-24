#include "config.h"
#include <iostream>

Config& Config::Instance()
{
    static Config instance;
    return instance;
}

bool Config::ParseCommandLine(int argc, wchar_t* argv[])
{
    for (int i = 1; i < argc; i++)
    {
        std::wstring arg = argv[i];
        if (arg == L"--help" || arg == L"-h") { ShowHelp(); return false; }
        else if (arg == L"--debug") { m_mode = ExecutionMode::DEBUG; m_consoleOutput = true; }
        else if (arg == L"--stealth") { m_mode = ExecutionMode::STEALTH; m_consoleOutput = false; }
        else if (arg == L"--recon-only") { m_mode = ExecutionMode::RECON_ONLY; m_payloadEnabled = false; }
        else if (arg == L"--no-console") { m_consoleOutput = false; }
        else if (arg == L"--payload" && i + 1 < argc) { m_payloadPath = argv[++i]; }
        else if (arg == L"--log" && i + 1 < argc) { m_logPath = argv[++i]; }
        else if (arg == L"--results" && i + 1 < argc) { m_resultsPath = argv[++i]; }
        else if (arg == L"--timeout" && i + 1 < argc) { m_maxExecutionTime = _wtoi(argv[++i]); }
    }
    return true;
}

void Config::ShowHelp() const
{
    std::wcout << L"Acheron-Loader Elite v2.0\n"
               << L"Uso: acheron.exe [opções]\n"
               << L"  --debug              Modo debug\n"
               << L"  --stealth            Modo stealth\n"
               << L"  --recon-only         Apenas scanner\n"
               << L"  --payload <path>     Payload personalizado\n"
               << L"  --log <path>         Arquivo de log\n"
               << L"  --results <path>     Arquivo de resultados\n";
}

LogLevel Config::GetMinLevel() const
{
    switch (m_mode)
    {
    case ExecutionMode::DEBUG: return LogLevel::Debug;
    case ExecutionMode::STEALTH: return LogLevel::Warn;
    default: return LogLevel::Info;
    }
}