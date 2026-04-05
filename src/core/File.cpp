//===========================================================================
// File.cpp  –  File and PAK resource management
// Clean C++17 rewrite.
//===========================================================================
#include "File.h"
#include "GPak.h"
#include "Types.h"
#include <string.h>
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include "../DebugLog.h"

#if !RO_PLATFORM_WINDOWS
#include <optional>
#endif

namespace {

#if !RO_PLATFORM_WINDOWS
// Windows is case-insensitive, so on non-windows platforms we can operate under the same assumption but must deal with possible filename mismatches due to case
namespace fs = std::filesystem;
bool iequals(const std::string& a, const std::string& b) {
    return a.size() == b.size() &&
    std::equal(a.begin(), a.end(), b.begin(),
                [](char a, char b) {
                    return std::tolower(a) == std::tolower(b);
                });
}

std::optional<fs::path> ResolveCaseInsensitive(const fs::path& relative, const fs::path& base = ".")
{
    fs::path current = base;
    for (const auto& part : relative) {
        if (!fs::exists(current) || !fs::is_directory(current)) {
            return std::nullopt;
        }
        bool found = false;
        for (const auto& entry : fs::directory_iterator(current)) {
            if (iequals(entry.path().filename().string(), part.string())) {
                current /= entry.path().filename();
                found = true;
                break;
            }
        }
        if (!found) {
            return std::nullopt;
        }
    }
    return current;
}

std::string NormalizePortablePath(const char* fileName)
{
    if (!fileName) {
        return {};
    }

    std::string normalized(fileName);
    std::replace(normalized.begin(), normalized.end(), '\\', '/');

    auto resolved = ResolveCaseInsensitive(std::filesystem::path(normalized.c_str()));

    return resolved.value_or("");
}

bool FileExistsPortable(const char* fileName)
{
    if (!fileName || !*fileName) {
        return false;
    }

    return !NormalizePortablePath(fileName).empty();
}

unsigned char* ReadWholeFilePortable(const char* fileName, int* outSize)
{
    if (outSize) {
        *outSize = 0;
    }
    if (!fileName || !*fileName) {
        return nullptr;
    }

    const std::string normalizedPath = NormalizePortablePath(fileName);
    FILE* file = std::fopen(normalizedPath.c_str(), "rb");
    if (!file) {
        return nullptr;
    }

    if (std::fseek(file, 0, SEEK_END) != 0) {
        std::fclose(file);
        return nullptr;
    }

    const long size = std::ftell(file);
    if (size < 0 || std::fseek(file, 0, SEEK_SET) != 0) {
        std::fclose(file);
        return nullptr;
    }

    unsigned char* buffer = new unsigned char[static_cast<size_t>(size) + 1];
    if (!buffer) {
        std::fclose(file);
        return nullptr;
    }

    const size_t bytesRead = std::fread(buffer, 1, static_cast<size_t>(size), file);
    std::fclose(file);
    if (bytesRead != static_cast<size_t>(size)) {
        delete[] buffer;
        return nullptr;
    }

    buffer[size] = 0;
    if (outSize) {
        *outSize = static_cast<int>(size);
    }
    return buffer;
}
#endif

} // namespace

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

#if RO_PLATFORM_WINDOWS
    if (GetFileAttributesA(fileName) != INVALID_FILE_ATTRIBUTES)
        return true;
#else
    if (FileExistsPortable(fileName))
        return true;
#endif

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

#if RO_PLATFORM_WINDOWS
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
#else
    if (unsigned char* buf = ReadWholeFilePortable(fileName, outSize)) {
        return buf;
    }
#endif
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
#if RO_PLATFORM_WINDOWS
        m_hFile = CreateFileA(fileName, GENERIC_WRITE, FILE_SHARE_READ,
                              nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (m_hFile == INVALID_HANDLE_VALUE) return false;
        m_size = GetFileSize(m_hFile, nullptr);
#else
        m_hFile = std::fopen(fileName, "wb");
        if (!m_hFile) return false;
        m_size = 0;
#endif
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
    if (m_hFile)
    {
#if RO_PLATFORM_WINDOWS
        DWORD bytesRead = 0;
        return ReadFile(m_hFile, buffer, (DWORD)count, &bytesRead, nullptr) != FALSE;
#else
        return std::fread(buffer, 1, count, static_cast<FILE*>(m_hFile)) == count;
#endif
    }
    if (!m_buf) return false;
    if (m_cursor + count > m_size) return false;

    std::memcpy(buffer, m_buf + m_cursor, count);
    m_cursor += count;
    return true;
}

bool CFile::Write(const void* buffer, u32 count)
{
    if (!m_hFile) return false;
#if RO_PLATFORM_WINDOWS
    DWORD written = 0;
    return WriteFile(m_hFile, buffer, (DWORD)count, &written, nullptr) != FALSE;
#else
    const size_t written = std::fwrite(buffer, 1, count, static_cast<FILE*>(m_hFile));
    m_size += static_cast<u32>(written);
    return written == count;
#endif
}

bool CFile::Seek(int offset, int origin)
{
    if (m_hFile)
    {
#if RO_PLATFORM_WINDOWS
        LARGE_INTEGER move{};
        move.QuadPart = static_cast<LONGLONG>(offset);
        return SetFilePointerEx(m_hFile, move, nullptr, static_cast<DWORD>(origin)) != FALSE;
#else
        return std::fseek(static_cast<FILE*>(m_hFile), offset, origin) == 0;
#endif
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
    if (m_hFile)
    {
#if RO_PLATFORM_WINDOWS
        CloseHandle(m_hFile);
#else
        std::fclose(static_cast<FILE*>(m_hFile));
#endif
        m_hFile = nullptr;
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
#if RO_PLATFORM_WINDOWS
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    m_dwAllocationGranularity = si.dwAllocationGranularity;
#else
    m_dwAllocationGranularity = 4096;
#endif
    m_hFile = nullptr;
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
#if RO_PLATFORM_WINDOWS
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
#else
    if (offset > m_dwFileSize || len > (m_dwFileSize - offset)) {
        return nullptr;
    }
    return m_pFileBuf.data() + offset;
#endif
}

bool CMemMapFile::open(const char* fileName)
{
#if RO_PLATFORM_WINDOWS
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
#else
    close();
    int fileSize = 0;
    unsigned char* data = ReadWholeFilePortable(fileName, &fileSize);
    if (!data) {
        return false;
    }

    m_pFileBuf.assign(data, data + fileSize);
    delete[] data;
    m_dwFileSize = static_cast<u32>(fileSize);
    m_dwOpenOffset = 0;
    m_dwOpenSize = m_dwFileSize;
    m_pFile = m_pFileBuf.data();
    return true;
#endif
}

void CMemMapFile::close()
{
#if RO_PLATFORM_WINDOWS
    if (m_pFile)    { UnmapViewOfFile(m_pFile); m_pFile = nullptr; }
    if (m_hFileMap) { CloseHandle(m_hFileMap);  m_hFileMap = nullptr; }
    if (m_hFile)
    {
        CloseHandle(m_hFile);
        m_hFile = nullptr;
    }
#else
    m_hFile = nullptr;
    m_hFileMap = nullptr;
    m_pFile = nullptr;
    m_pFileBuf.clear();
#endif
    m_dwFileSize = 0;
}
