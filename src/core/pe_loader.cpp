#include "pe_loader.h"
#include "memory_manager.h"
#include "../utils/logger.h"
#include <fstream>
#include <algorithm>

PELoader::~PELoader() { Cleanup(); }

bool PELoader::LoadFromFile(const std::wstring& path)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) { LOG_ERROR(L"Falha ao abrir %ls", path.c_str()); return false; }

    auto size = file.tellg();
    if (size <= 0) return false;

    file.seekg(0);
    m_fileData.resize(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(m_fileData.data()), size))
        return false;

    if (!Validate()) { m_fileData.clear(); return false; }

    LOG_INFO(L"PE carregado (%zu bytes)", m_fileData.size());
    return true;
}

bool PELoader::Validate() const
{
    if (m_fileData.size() < sizeof(IMAGE_DOS_HEADER)) return false;

    auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(m_fileData.data());
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;

    if (static_cast<size_t>(dos->e_lfanew) + sizeof(IMAGE_NT_HEADERS64) > m_fileData.size())
        return false;

    auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(m_fileData.data() + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return false;
    if (nt->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64) return false;

    return true;
}

bool PELoader::MapToMemory()
{
    if (m_fileData.empty() || m_mapped) return false;

    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(m_fileData.data());
    m_nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(m_fileData.data() + dos->e_lfanew);

    SIZE_T imageSize = m_nt->OptionalHeader.SizeOfImage;
    m_imageBase = MemoryManager::Instance().Allocate(imageSize, PAGE_READWRITE, "PEImage");
    if (!m_imageBase) return false;

    // Headers
    memcpy(m_imageBase, m_fileData.data(), m_nt->OptionalHeader.SizeOfHeaders);

    // Sections
    auto* sec = IMAGE_FIRST_SECTION(m_nt);
    for (WORD i = 0; i < m_nt->FileHeader.NumberOfSections; ++i, ++sec)
    {
        if (sec->SizeOfRawData == 0) continue;

        void* dst = static_cast<uint8_t*>(m_imageBase) + sec->VirtualAddress;
        const void* src = m_fileData.data() + sec->PointerToRawData;
        size_t sz = (std::min)(static_cast<size_t>(sec->SizeOfRawData), static_cast<size_t>(sec->Misc.VirtualSize));
        memcpy(dst, src, sz);
    }

    m_mapped = true;
    LOG_INFO(L"Imagem mapeada em 0x%p", m_imageBase);
    return true;
}

void* PELoader::GetEntryPoint() const
{
    if (!m_imageBase || !m_nt) return nullptr;
    return static_cast<uint8_t*>(m_imageBase) + m_nt->OptionalHeader.AddressOfEntryPoint;
}

void PELoader::Cleanup()
{
    if (m_imageBase)
    {
        MemoryManager::Instance().Free(m_imageBase);
        m_imageBase = nullptr;
    }
    m_fileData.clear();
    m_nt = nullptr;
    m_mapped = false;
}

// ======================================================================
// NOVAS FUNÇÕES PARA IMPORTS, RELOCATIONS E PROTEÇÕES
// ======================================================================
bool PELoader::ResolveImports()
{
    if (!m_imageBase || !m_nt) return false;

    DWORD importRVA = m_nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    if (!importRVA) return true;

    IMAGE_IMPORT_DESCRIPTOR* pDesc = (IMAGE_IMPORT_DESCRIPTOR*)((BYTE*)m_imageBase + importRVA);
    while (pDesc->Name && pDesc->FirstThunk)
    {
        const char* dllName = (const char*)((BYTE*)m_imageBase + pDesc->Name);
        HMODULE hMod = GetModuleHandleA(dllName);
        if (!hMod) hMod = LoadLibraryA(dllName);
        if (!hMod) return false;

        IMAGE_THUNK_DATA64* pThunk = (IMAGE_THUNK_DATA64*)((BYTE*)m_imageBase + pDesc->FirstThunk);
        IMAGE_THUNK_DATA64* pIAT = pThunk;

        while (pThunk->u1.AddressOfData)
        {
            FARPROC pFunc = nullptr;
            if (pThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG64)
                pFunc = GetProcAddress(hMod, (LPCSTR)(pThunk->u1.Ordinal & ~IMAGE_ORDINAL_FLAG64));
            else
            {
                IMAGE_IMPORT_BY_NAME* pName = (IMAGE_IMPORT_BY_NAME*)((BYTE*)m_imageBase + pThunk->u1.AddressOfData);
                pFunc = GetProcAddress(hMod, pName->Name);
            }
            if (!pFunc) return false;
            pIAT->u1.Function = (ULONGLONG)pFunc;
            pThunk++;
            pIAT++;
        }
        pDesc++;
    }
    return true;
}

bool PELoader::ApplyRelocations(ULONGLONG imageDelta)
{
    if (!m_imageBase || !m_nt) return false;

    DWORD relocRVA = m_nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress;
    if (!relocRVA || imageDelta == 0) return true;

    IMAGE_BASE_RELOCATION* pReloc = (IMAGE_BASE_RELOCATION*)((BYTE*)m_imageBase + relocRVA);
    while (pReloc->VirtualAddress && pReloc->SizeOfBlock)
    {
        WORD* pEntries = (WORD*)(pReloc + 1);
        DWORD numEntries = (pReloc->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
        for (DWORD i = 0; i < numEntries; i++)
        {
            WORD type = pEntries[i] >> 12;
            WORD offset = pEntries[i] & 0x0FFF;
            if (type == IMAGE_REL_BASED_DIR64)
            {
                ULONGLONG* pAddr = (ULONGLONG*)((BYTE*)m_imageBase + pReloc->VirtualAddress + offset);
                *pAddr += imageDelta;
            }
        }
        pReloc = (IMAGE_BASE_RELOCATION*)((BYTE*)pReloc + pReloc->SizeOfBlock);
    }
    return true;
}

bool PELoader::SetMemoryProtections()
{
    if (!m_imageBase || !m_nt) return false;

    IMAGE_SECTION_HEADER* pSection = IMAGE_FIRST_SECTION(m_nt);
    for (WORD i = 0; i < m_nt->FileHeader.NumberOfSections; i++)
    {
        DWORD protect = PAGE_READWRITE;
        if (pSection[i].Characteristics & IMAGE_SCN_MEM_EXECUTE)
        {
            if (pSection[i].Characteristics & IMAGE_SCN_MEM_READ)
                protect = (pSection[i].Characteristics & IMAGE_SCN_MEM_WRITE) ?
                          PAGE_EXECUTE_READWRITE : PAGE_EXECUTE_READ;
            else
                protect = PAGE_EXECUTE;
        }
        else
        {
            if (pSection[i].Characteristics & IMAGE_SCN_MEM_READ)
                protect = (pSection[i].Characteristics & IMAGE_SCN_MEM_WRITE) ?
                          PAGE_READWRITE : PAGE_READONLY;
            else
                protect = PAGE_NOACCESS;
        }
        if (protect == PAGE_EXECUTE_READWRITE)
            protect = PAGE_EXECUTE_READ;

        MemoryManager::Instance().Protect((BYTE*)m_imageBase + pSection[i].VirtualAddress,
                                          pSection[i].Misc.VirtualSize,
                                          protect);
    }
    return true;
}