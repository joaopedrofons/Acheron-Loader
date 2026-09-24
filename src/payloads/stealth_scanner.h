#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <set>

class StealthScanner
{
public:
    bool Initialize();
    void ScanSensitiveData();
    bool ExportResults(const std::wstring& filePath);
    size_t GetFilesScanned() const { return m_filesScanned; }
    size_t GetCredentialsFound() const { return m_credentialsFound; }

private:
    struct FoundCredential
    {
        std::wstring filePath;
        std::wstring key;
        std::wstring value;
    };
    std::vector<FoundCredential> m_credentials;
    std::set<std::wstring> m_scannedDirs;
    size_t m_filesScanned = 0;
    size_t m_credentialsFound = 0;

    bool IsSensitiveFile(const std::wstring& name);
    void ExtractFromFile(const std::wstring& path);
    void ScanDirectory(const std::wstring& path);
    bool ScanDirectoryNt(const std::wstring& path);
};