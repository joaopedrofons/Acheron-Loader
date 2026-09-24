#include "syscalls_advanced.h"
#include "../utils/logger.h"
#include <psapi.h>
#include <cstdarg>

#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
#endif
#ifndef STATUS_UNSUCCESSFUL
#define STATUS_UNSUCCESSFUL ((NTSTATUS)0xC0000001L)
#endif

std::vector<SyscallManager::SyscallEntry> SyscallManager::m_syscalls;
bool SyscallManager::m_initialized = false;
CRITICAL_SECTION SyscallManager::m_cs;

// ======================================================================
// Gera um stub em memória para syscall (usando syscall; ret)
// ======================================================================
static PVOID GenerateStub(DWORD ssn)
{
    BYTE stub[] = {
        0x49, 0x89, 0xCA,               // mov r10, rcx
        0xB8, 0x00,0x00,0x00,0x00,      // mov eax, SSN
        0x0F, 0x05,                     // syscall
        0xC3                            // ret
    };
    *(DWORD*)(stub + 4) = ssn;

    PVOID mem = VirtualAlloc(nullptr, sizeof(stub), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!mem) return nullptr;
    memcpy(mem, stub, sizeof(stub));

    DWORD old;
    VirtualProtect(mem, sizeof(stub), PAGE_EXECUTE_READ, &old);
    return mem;
}

// ======================================================================
// Inicialização
// ======================================================================
bool SyscallManager::Initialize()
{
    if (m_initialized) return true;
    InitializeCriticalSection(&m_cs);
    EnterCriticalSection(&m_cs);

    if (!ResolveSyscalls())
    {
        LeaveCriticalSection(&m_cs);
        return false;
    }
    m_initialized = true;
    LeaveCriticalSection(&m_cs);
    LOG_INFO(L"SyscallManager: %zu syscalls resolvidos", m_syscalls.size());
    return true;
}

void SyscallManager::Shutdown()
{
    if (!m_initialized) return;
    DeleteCriticalSection(&m_cs);
    m_initialized = false;
}

// ======================================================================
// Extrai o SSN de uma função Nt*
// ======================================================================
DWORD SyscallManager::ExtractSSN(PVOID funcAddr)
{
    BYTE* b = (BYTE*)funcAddr;
    if (b[0] == 0x4C && b[1] == 0x8B && b[2] == 0xD1) return b[4];
    if (b[0] == 0xB8) return b[1];
    return 0xFFFFFFFF;
}

// ======================================================================
// Resolve todos os SSNs do ntdll
// ======================================================================
bool SyscallManager::ResolveSyscalls()
{
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll) return false;

    PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)ntdll;
    PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)((BYTE*)ntdll + dos->e_lfanew);
    DWORD rva = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
    if (!rva) return false;

    PIMAGE_EXPORT_DIRECTORY exp = (PIMAGE_EXPORT_DIRECTORY)((BYTE*)ntdll + rva);
    DWORD* names = (DWORD*)((BYTE*)ntdll + exp->AddressOfNames);
    WORD* ords = (WORD*)((BYTE*)ntdll + exp->AddressOfNameOrdinals);
    DWORD* funcs = (DWORD*)((BYTE*)ntdll + exp->AddressOfFunctions);

    for (DWORD i = 0; i < exp->NumberOfNames; ++i)
    {
        const char* name = (const char*)((BYTE*)ntdll + names[i]);
        if (strncmp(name, "Nt", 2) == 0 || strncmp(name, "Zw", 2) == 0)
        {
            PVOID addr = (BYTE*)ntdll + funcs[ords[i]];
            DWORD ssn = ExtractSSN(addr);
            if (ssn != 0xFFFFFFFF)
                m_syscalls.push_back({ name, ssn, addr });
        }
    }
    return !m_syscalls.empty();
}

// ======================================================================
// Getters
// ======================================================================
DWORD SyscallManager::GetSSN(const char* name)
{
    EnterCriticalSection(&m_cs);
    for (auto& e : m_syscalls)
        if (strcmp(e.name.c_str(), name) == 0) { LeaveCriticalSection(&m_cs); return e.ssn; }
    LeaveCriticalSection(&m_cs);
    return 0xFFFFFFFF;
}

PVOID SyscallManager::GetOriginalAddress(const char* name)
{
    EnterCriticalSection(&m_cs);
    for (auto& e : m_syscalls)
        if (strcmp(e.name.c_str(), name) == 0) { LeaveCriticalSection(&m_cs); return e.address; }
    LeaveCriticalSection(&m_cs);
    return nullptr;
}

// ======================================================================
// Executa syscall com até 4 argumentos (via stub gerado em memória)
// ======================================================================
NTSTATUS SyscallManager::ExecuteSyscall(DWORD ssn, ...)
{
    PVOID stub = GenerateStub(ssn);
    if (!stub) return STATUS_UNSUCCESSFUL;

    va_list a; va_start(a, ssn);
    ULONG_PTR a1 = va_arg(a, ULONG_PTR);
    ULONG_PTR a2 = va_arg(a, ULONG_PTR);
    ULONG_PTR a3 = va_arg(a, ULONG_PTR);
    ULONG_PTR a4 = va_arg(a, ULONG_PTR);
    va_end(a);

    typedef NTSTATUS(WINAPI* Fn)(ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR);
    NTSTATUS s = ((Fn)stub)(a1, a2, a3, a4);
    VirtualFree(stub, 0, MEM_RELEASE);
    return s;
}

// ======================================================================
// Executa syscall com MAIS de 4 argumentos (fallback via GetProcAddress)
// ======================================================================
NTSTATUS SyscallManager::ExecuteSyscallEx(DWORD ssn, const std::vector<ULONG_PTR>& args)
{
    if (args.empty()) return STATUS_UNSUCCESSFUL;

    std::string funcName;
    for (auto& e : m_syscalls)
        if (e.ssn == ssn) { funcName = e.name; break; }
    if (funcName.empty()) return STATUS_UNSUCCESSFUL;

    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll) return STATUS_UNSUCCESSFUL;

    FARPROC pFunc = GetProcAddress(ntdll, funcName.c_str());
    if (!pFunc) return STATUS_UNSUCCESSFUL;

    typedef NTSTATUS(WINAPI* Fn)(...);
    Fn fn = (Fn)pFunc;

    size_t n = args.size();
    switch (n)
    {
    case 1:  return fn(args[0]);
    case 2:  return fn(args[0], args[1]);
    case 3:  return fn(args[0], args[1], args[2]);
    case 4:  return fn(args[0], args[1], args[2], args[3]);
    case 5:  return fn(args[0], args[1], args[2], args[3], args[4]);
    case 6:  return fn(args[0], args[1], args[2], args[3], args[4], args[5]);
    case 7:  return fn(args[0], args[1], args[2], args[3], args[4], args[5], args[6]);
    case 8:  return fn(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7]);
    case 9:  return fn(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8]);
    case 10: return fn(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8], args[9]);
    case 11: return fn(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8], args[9], args[10]);
    case 12: return fn(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8], args[9], args[10], args[11]);
    default: return STATUS_UNSUCCESSFUL;
    }
}

// ======================================================================
// Wrapper para NtCreateThreadEx
// ======================================================================
bool SyscallManager::CreateThreadEx(PHANDLE hThread, PVOID startAddress, PVOID parameter)
{
    DWORD ssn = GetSSN("NtCreateThreadEx");
    if (ssn == 0xFFFFFFFF) return false;

    std::vector<ULONG_PTR> args = {
        (ULONG_PTR)hThread,
        (ULONG_PTR)THREAD_ALL_ACCESS,
        (ULONG_PTR)nullptr,
        (ULONG_PTR)GetCurrentProcess(),
        (ULONG_PTR)startAddress,
        (ULONG_PTR)parameter,
        (ULONG_PTR)0, (ULONG_PTR)0, (ULONG_PTR)0, (ULONG_PTR)0, (ULONG_PTR)nullptr
    };
    return ExecuteSyscallEx(ssn, args) == 0;
}