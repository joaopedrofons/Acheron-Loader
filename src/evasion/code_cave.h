#pragma once
#include <windows.h>

class CodeCaveManager
{
public:
    static bool Initialize();
    static PVOID Allocate(SIZE_T size);
    static void Free(PVOID address);
};