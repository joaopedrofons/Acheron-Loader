#include "code_cave.h"
#include "../utils/logger.h"
#include <vector>

struct CodeCave
{
    BYTE* address;
    SIZE_T size;
    bool used;
};

static std::vector<CodeCave> g_caves;

bool CodeCaveManager::Initialize()
{
    g_caves.clear();

    HMODULE modules[] = {
        GetModuleHandleW(L"kernel32.dll"),
        GetModuleHandleW(L"ntdll.dll"),
        GetModuleHandleW(L"user32.dll")
    };

    for (auto hMod : modules)
    {
        if (!hMod) continue;
        PIMAGE_DOS_HEADER pDos = (PIMAGE_DOS_HEADER)hMod;
        PIMAGE_NT_HEADERS pNt = (PIMAGE_NT_HEADERS)((BYTE*)hMod + pDos->e_lfanew);
        PIMAGE_SECTION_HEADER pSection = IMAGE_FIRST_SECTION(pNt);

        for (WORD i = 0; i < pNt->FileHeader.NumberOfSections; i++)
        {
            if (memcmp(pSection[i].Name, ".text", 5) == 0)
            {
                BYTE* start = (BYTE*)hMod + pSection[i].VirtualAddress;
                BYTE* end = start + pSection[i].Misc.VirtualSize;

                SIZE_T caveSize = 0;
                for (BYTE* p = start; p < end - 32; p++)
                {
                    if (*p == 0x00)
                    {
                        caveSize++;
                        if (caveSize >= 32)
                        {
                            g_caves.push_back({ p - caveSize + 1, caveSize, false });
                            caveSize = 0;
                        }
                    }
                    else
                        caveSize = 0;
                }
            }
        }
    }

    LOG_INFO(L"Code caves: %zu encontrados", g_caves.size());
    return true;
}

PVOID CodeCaveManager::Allocate(SIZE_T size)
{
    for (auto& cave : g_caves)
    {
        if (!cave.used && cave.size >= size)
        {
            cave.used = true;
            return cave.address;
        }
    }
    return nullptr;
}

void CodeCaveManager::Free(PVOID address)
{
    for (auto& cave : g_caves)
    {
        if (cave.address == address)
        {
            cave.used = false;
            return;
        }
    }
}