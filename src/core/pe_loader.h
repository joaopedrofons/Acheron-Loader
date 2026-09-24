#pragma once
#include <windows.h>
#include <string>
#include <vector>

class PELoader
{
public:
    PELoader() = default;
    ~PELoader();

    bool LoadFromFile(const std::wstring& path);
    bool MapToMemory();
    void* GetImageBase() const { return m_imageBase; }
    void* GetEntryPoint() const;
    void Cleanup();

    // NOVOS MÉTODOS
    bool ResolveImports();
    bool ApplyRelocations(ULONGLONG imageDelta);
    bool SetMemoryProtections();

private:
    bool Validate() const;

    std::vector<uint8_t> m_fileData;
    void* m_imageBase = nullptr;
    PIMAGE_NT_HEADERS64 m_nt = nullptr;
    bool m_mapped = false;
};