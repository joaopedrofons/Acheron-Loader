#pragma once
#include <windows.h>
#include <vector>
#include <string>

struct MemoryRegion
{
    void* address = nullptr;
    SIZE_T size = 0;
    DWORD protect = PAGE_NOACCESS;
    std::string tag;
};

class MemoryManager
{
public:
    static MemoryManager& Instance();

    bool Initialize();
    void Shutdown();

    void* Allocate(SIZE_T size, DWORD protect = PAGE_READWRITE, const std::string& tag = "");
    bool Free(void* address);
    bool FreeAll();
    bool Protect(void* address, SIZE_T size, DWORD newProtect);

    size_t GetRegionCount() const { return m_regions.size(); }
    SIZE_T GetTotalAllocated() const { return m_total; }

private:
    MemoryManager() = default;
    ~MemoryManager() = default;
    MemoryManager(const MemoryManager&) = delete;
    MemoryManager& operator=(const MemoryManager&) = delete;

    std::vector<MemoryRegion> m_regions;
    bool m_initialized = false;
    SIZE_T m_total = 0;
};