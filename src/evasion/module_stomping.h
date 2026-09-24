#pragma once
#include <windows.h>

class ModuleStomping
{
public:
    static bool Stomp(const wchar_t* moduleName, const BYTE* shellcode, SIZE_T size);
    static void Restore();
};