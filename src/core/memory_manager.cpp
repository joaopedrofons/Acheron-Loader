#include "memory_manager.h"
#include "../utils/logger.h"
#include <algorithm>

MemoryManager& MemoryManager::Instance()
{
    static MemoryManager inst;
    return inst;
}

bool MemoryManager::Initialize()
{
    if (m_initialized) return true;
    m_initialized = true;
    LOG_INFO(L"MemoryManager inicializado");
    return true;
}

void MemoryManager::Shutdown()
{
    if (!m_initialized) return;
    FreeAll();
    m_initialized = false;
}

void* MemoryManager::Allocate(SIZE_T size, DWORD protect, const std::string& tag)
{
    if (size == 0) return nullptr;
    void* ptr = VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, protect);
    if (!ptr)
    {
        LOG_ERROR(L"VirtualAlloc falhou (size=%zu, err=0x%X)", size, GetLastError());
        return nullptr;
    }
    m_regions.push_back({ ptr, size, protect, tag });
    m_total += size;
    return ptr;
}

bool MemoryManager::Free(void* address)
{
    if (!address) return false;

    auto it = std::find_if(m_regions.begin(), m_regions.end(),
        [address](const MemoryRegion& r) { return r.address == address; });

    if (it == m_regions.end()) return VirtualFree(address, 0, MEM_RELEASE) != 0;

    if (VirtualFree(address, 0, MEM_RELEASE))
    {
        m_total -= it->size;
        m_regions.erase(it);
        return true;
    }
    return false;
}

bool MemoryManager::FreeAll()
{
    for (auto it = m_regions.begin(); it != m_regions.end(); )
    {
        if (VirtualFree(it->address, 0, MEM_RELEASE))
            it = m_regions.erase(it);
        else
            ++it;
    }
    m_total = 0;
    return true;
}

bool MemoryManager::Protect(void* address, SIZE_T size, DWORD newProtect)
{
    if (!address || size == 0) return false;
    DWORD old = 0;
    return VirtualProtect(address, size, newProtect, &old) != 0;
}