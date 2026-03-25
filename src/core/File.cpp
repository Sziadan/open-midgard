//===========================================================================
// File.cpp  –  File and PAK resource management
// Clean C++17 rewrite.
//===========================================================================
#include "File.h"
#include "GPak.h"
#include "Types.h"
#include <string.h>
#include <algorithm>
#include "../DebugLog.h"

// ---------------------------------------------------------------------------
// CFileMgr
// ---------------------------------------------------------------------------
CFileMgr::CFileMgr() = default;

CFileMgr::~CFileMgr()
{
    for (auto& [memFile, pak] : m_pakList)
    {
        delete memFile;
        delete pak;
    }
    m_pakList.clear();
}

void CFileMgr::AddPak(const char* pakName)
{
    DbgLog("[AddPak] Trying to open: %s\n", pakName ? pakName : "(null)");

    auto* pak     = new CGPak();
    auto* memFile = new CMemMapFile();

    if (!memFile->open(pakName))
    {
        DbgLog("[AddPak] FAIL: CMemMapFile::open failed for '%s' (last error: %lu)\n",
               pakName, GetLastError());
        delete pak;
        delete memFile;
        return;
    }
    DbgLog("[AddPak] memFile opened OK (size: %u bytes)\n", memFile->size());

    if (!pak->Open(memFile))
    {
        DbgLog("[AddPak] FAIL: CGPak::Open failed for '%s'\n", pakName);
        delete pak;
        delete memFile;
        return;
    }
    DbgLog("[AddPak] SUCCESS: '%s' loaded into pak list\n", pakName);

    m_pakList.emplace_front(memFile, pak);
}

bool CFileMgr::IsDataExist(const char* fileName)
{
    CHash key(fileName);
    for (const auto& [memFile, pak] : m_pakList)
    {
        if (pak->GetInfo(key, nullptr))
            return true;
    }

    if (GetFileAttributesA(fileName) != INVALID_FILE_ATTRIBUTES)
        return true;

    return false;
}

bool CFileMgr::IsExist(const char* fileName)
{
    return IsDataExist(fileName);
}

unsigned char* CFileMgr::GetData(const char* fileName, int* outSize)
{
    CHash key(fileName);
    for (auto& [memFile, pak] : m_pakList)
    {
        PakPack pack{};
        if (!pak->GetInfo(key, &pack))
            continue;

        if (outSize) *outSize = (int)pack.m_size;
        unsigned char* buf = new unsigned char[pack.m_size + 1];
        if (!buf) continue;

        if (pak->GetData(pack, buf))
        {
            buf[pack.m_size] = 0;
            return buf;
        }
        delete[] buf;
    }

    HANDLE hFile = CreateFileA(fileName, GENERIC_READ, FILE_SHARE_READ,
                               nullptr, OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile != INVALID_HANDLE_VALUE)
    {
        DWORD fileSize = GetFileSize(hFile, nullptr);
        if (outSize) *outSize = (int)fileSize;

        unsigned char* buf = new unsigned char[fileSize + 1];
        if (buf)
        {
            DWORD bytesRead = 0;
            ReadFile(hFile, buf, fileSize, &bytesRead, nullptr);
            buf[fileSize] = 0;
            CloseHandle(hFile);
            return buf;
        }
        CloseHandle(hFile);
    }
    return nullptr;
}

void CFileMgr::CollectDataNamesByExtension(const char* ext, std::vector<std::string>& out)
{
    for (const auto& [memFile, pak] : m_pakList)
    {
        (void)memFile;
        if (pak) {
            pak->CollectFileNamesByExtension(ext, out);
        }
    }
}

// ---------------------------------------------------------------------------
// CFile
// ---------------------------------------------------------------------------
CFile::CFile() = default;

CFile::~CFile()
{
    Close();
}

bool CFile::Open(const char* fileName, int mode)
{
    Close();
    if (!fileName) return false;
    std::strncpy(m_fileName, fileName, 127);

    if (mode & modeWrite)
    {
        m_hFile = CreateFileA(fileName, GENERIC_WRITE, FILE_SHARE_READ,
                              nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (m_hFile == INVALID_HANDLE_VALUE) return false;
        m_size = GetFileSize(m_hFile, nullptr);
        return true;
    }

    int dataSize = 0;
    unsigned char* data = g_fileMgr.GetData(fileName, &dataSize);
    if (!data) return false;

    m_buf = (char*)data;
    m_size = (u32)dataSize;
    m_cursor = 0;
    return true;
}

bool CFile::Read(void* buffer, u32 count)
{
    if (m_hFile != INVALID_HANDLE_VALUE)
    {
        DWORD bytesRead = 0;
        return ReadFile(m_hFile, buffer, (DWORD)count, &bytesRead, nullptr) != FALSE;
    }
    if (!m_buf) return false;
    if (m_cursor + count > m_size) return false;

    std::memcpy(buffer, m_buf + m_cursor, count);
    m_cursor += count;
    return true;
}

bool CFile::Write(const void* buffer, u32 count)
{
    if (m_hFile == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    return WriteFile(m_hFile, buffer, (DWORD)count, &written, nullptr) != FALSE;
}

bool CFile::Seek(int offset, int origin)
{
    if (m_hFile != INVALID_HANDLE_VALUE)
    {
        SetFilePointer(m_hFile, offset, nullptr, (DWORD)origin);
        return true;
    }
    if (!m_buf) return false;

    u32 newPos;
    switch (origin)
    {
    case 0: newPos = (u32)offset; break;
    case 1: newPos = m_cursor + (u32)offset; break;
    case 2: newPos = m_size + (u32)offset; break;
    default: return false;
    }
    if (newPos > m_size) return false;
    m_cursor = newPos;
    return true;
}

void CFile::Close()
{
    if (m_hFile != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_hFile);
        m_hFile = INVALID_HANDLE_VALUE;
    }
    if (m_buf)
    {
        delete[] m_buf;
        m_buf = nullptr;
    }
    m_size   = 0;
    m_cursor = 0;
}

int CFile::Tell() { return (int)m_cursor; }
int CFile::Size() { return (int)m_size; }

// ---------------------------------------------------------------------------
// CMemMapFile
// ---------------------------------------------------------------------------
CMemMapFile::CMemMapFile()
{
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    m_dwAllocationGranularity = si.dwAllocationGranularity;
    m_hFile = INVALID_HANDLE_VALUE;
    m_hFileMap = nullptr;
    m_pFile = nullptr;
    m_dwFileSize = 0;
    m_dwOpenOffset = 0;
    m_dwOpenSize = 0;
    m_dwFileMappingSize = 4 * 1024 * 1024;
}

CMemMapFile::~CMemMapFile()
{
    close();
}

u32 CMemMapFile::size()
{
    return m_dwFileSize;
}

const u8* CMemMapFile::read(u32 offset, u32 len)
{
    if (offset < m_dwOpenOffset || (offset + len) > (m_dwOpenOffset + m_dwOpenSize))
    {
        if (m_pFile) UnmapViewOfFile(m_pFile);

        u32 aligned = offset & ~(m_dwAllocationGranularity - 1);
        u32 viewSize = (std::min)(m_dwFileSize - aligned, m_dwFileMappingSize);

        m_pFile = static_cast<const u8*>(
            MapViewOfFile(m_hFileMap, FILE_MAP_READ, 0, aligned, viewSize));
        if (!m_pFile) return nullptr;

        m_dwOpenOffset = aligned;
        m_dwOpenSize   = viewSize;
    }
    return m_pFile + (offset - m_dwOpenOffset);
}

bool CMemMapFile::open(const char* fileName)
{
    m_hFile = CreateFileA(fileName, GENERIC_READ, FILE_SHARE_READ,
                          nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (m_hFile == INVALID_HANDLE_VALUE) return false;

    m_dwFileSize = GetFileSize(m_hFile, nullptr);
    m_hFileMap = CreateFileMappingA(m_hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!m_hFileMap)
    {
        close();
        return false;
    }

    u32 viewSize = (std::min)(m_dwFileSize, m_dwFileMappingSize);
    m_pFile = static_cast<const u8*>(
        MapViewOfFile(m_hFileMap, FILE_MAP_READ, 0, 0, viewSize));

    if (!m_pFile)
    {
        close();
        return false;
    }
    m_dwOpenOffset = 0;
    m_dwOpenSize = viewSize;
    return true;
}

void CMemMapFile::close()
{
    if (m_pFile)    { UnmapViewOfFile(m_pFile); m_pFile = nullptr; }
    if (m_hFileMap) { CloseHandle(m_hFileMap);  m_hFileMap = nullptr; }
    if (m_hFile != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_hFile);
        m_hFile = INVALID_HANDLE_VALUE;
    }
    m_dwFileSize = 0;
}
