#pragma once
#include <windows.h>
#include <vector>
#include <string>

class AntiEDR
{
public:
    static bool Initialize();
    static void Cleanup();

    static bool CheckDebugger();
    static bool CheckSandbox();
    static void ApplyAntiDebug();

    static std::vector<std::wstring> DetectEDRs();

    static bool InstallAmsiEtwBypass();
    static void RemoveAmsiEtwBypass();

    static bool UnhookNtdll();
    static bool UnhookCLR(); // Restaura nLoadImage no clr.dll

    static bool StompModule(const wchar_t* moduleName, const BYTE* shellcode, SIZE_T size);
    static void RestoreStompedModule();

    static PVOID AllocateCodeCave(SIZE_T size);
    static void FreeCodeCave(PVOID address);

    static void ApplyAll(); // Aplica todas as técnicas
};