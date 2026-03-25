#pragma once
//===========================================================================
// File.h  –  File and PAK resource management classes
// Clean C++17 rewrite of the HighPriest 2008 client.
//===========================================================================
#include <list>
#include <vector>
#include <string>
#include <utility>
#include "Types.h"
#include <windows.h>

// Forward declarations
class CGPak;

//===========================================================================
// CFile  –  Thin file-access wrapper
//===========================================================================
class CFile
{
public:
    CFile();
    virtual ~CFile();

    // IDA-style OpenFlags (matches HighPriest.exe.h:3096)
    enum OpenFlags : u32
    {
        modeRead = 0x0,
        modeWrite = 0x1,
        modeReadWrite = 0x2,
        shareCompat = 0x0,
        shareExclusive = 0x10,
        shareDenyWrite = 0x20,
        shareDenyRead = 0x30,
        shareDenyNone = 0x40,
        modeNoInherit = 0x80,
        modeCreate = 0x1000,
        modeNoTruncate = 0x2000,
        typeText = 0x4000,
        typeBinary = 0x8000,
    };

    bool Open(const char* fileName, int mode = modeRead);
    bool Read(void* buffer, u32 count);
    bool Write(const void* buffer, u32 count);
    bool Seek(int offset, int origin);
    void Close();

    // Additional helpers for clean code
    int Tell();
    int Size();

protected:
    void*  m_hFile = nullptr;
    char*  m_buf   = nullptr;
    u32    m_cursor = 0;
    u32    m_size   = 0;
    char   m_fileName[128] = {};
};

//===========================================================================
// CMemFile – Abstract base for memory-mapped Grf/Pak file views
//===========================================================================
class CMemFile
{
public:
    CMemFile() = default;
    virtual ~CMemFile() = default;

    virtual u32   size() = 0;
    virtual const u8* read(u32 offset, u32 len) = 0;
};

class CMemMapFile : public CMemFile
{
public:
    CMemMapFile();
    virtual ~CMemMapFile() override;

    u32   size() override;
    const u8* read(u32 offset, u32 len) override;

    bool open(const char* fileName);
    void close();

protected:
    void* m_hFile = nullptr;
    void* m_hFileMap = nullptr;
    u32   m_dwFileSize = 0;
    u32   m_dwOpenOffset = 0;
    u32   m_dwOpenSize = 0;
    u32   m_dwFileMappingSize = 0;
    u32   m_dwAllocationGranularity = 0;
    const u8* m_pFile = nullptr;
    std::vector<u8> m_pFileBuf;
};

//===========================================================================
// CFileMgr – Manages PAK/GRF archives and file priority
//===========================================================================
class CFileMgr
{
public:
    CFileMgr();
    ~CFileMgr();

    void AddPak(const char* pakName);
    bool IsDataExist(const char* fileName);
    bool IsExist(const char* fileName); // Local alias for compatibility

    // Reads a file into a buffer. Returns the buffer (must be free'd by caller).
    // `outSize` is populated with the read byte count.
    unsigned char* GetData(const char* fileName, int* outSize);
    void CollectDataNamesByExtension(const char* ext, std::vector<std::string>& out);
    void* GetObj(const char* fileName, int* outSize) { return GetData(fileName, outSize); }

private:
    std::list<std::pair<CMemFile*, CGPak*>> m_pakList;
};

// ---------------------------------------------------------------------------
// Global file manager
// ---------------------------------------------------------------------------
extern CFileMgr g_fileMgr;
extern int      g_readFolderFirst;   // 0 = try PAK first; 1 = try disk first
extern char     g_baseDir[MAX_PATH];
