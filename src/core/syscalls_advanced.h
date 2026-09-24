#pragma once
#include <windows.h>
#include <vector>
#include <string>

class SyscallManager
{
public:
    struct SyscallEntry
    {
        std::string name;
        DWORD ssn;
        PVOID address;
    };

    static bool Initialize();
    static void Shutdown();

    static DWORD GetSSN(const char* name);
    static PVOID GetOriginalAddress(const char* name);

    static NTSTATUS ExecuteSyscall(DWORD ssn, ...);
    static NTSTATUS ExecuteSyscallEx(DWORD ssn, const std::vector<ULONG_PTR>& args);

    static bool CreateThreadEx(PHANDLE hThread, PVOID startAddress, PVOID parameter = nullptr);

private:
    static std::vector<SyscallEntry> m_syscalls;
    static bool m_initialized;
    static CRITICAL_SECTION m_cs;

    static bool ResolveSyscalls();
    static DWORD ExtractSSN(PVOID funcAddr);
};