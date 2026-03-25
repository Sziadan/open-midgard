#pragma once
//===========================================================================
// GPak.h  –  GRF/PAK archive reader
// Clean C++17 rewrite.
//===========================================================================
#include "Hash.h"
#include <vector>
#include <string>
#include <cstdint>

//===========================================================================
// PakPack  –  Per-file entry inside a .grf archive
//===========================================================================
struct PakPack
{
    CHash        m_fName;           // Hashed filename key
    unsigned int m_compressSize;    // Compressed size after encryption strip
    unsigned int m_dataSize;        // Size of the encrypted/compressed block
    unsigned int m_size;            // Original (uncompressed) size
    uint8_t      m_type;            // Bit flags: 0x2=encrypted, 0x4=never-encrypt
    unsigned int m_Offset;          // Byte offset inside the archive
};

// Comparator used for binary search (sort by CHash)
struct PakPrtLess
{
    bool operator()(const PakPack& a, const PakPack& b) const
    {
        return a.m_fName < b.m_fName;
    }
};

class CMemFile;
//===========================================================================
// CGPak  –  Reader for Gravity's .grf archive format (versions 0x100–0x103)
//===========================================================================
class CGPak
{
public:
    CGPak();
    virtual ~CGPak();

    // Attach a CMemFile data source and populate m_PakPack index.
    bool Open(CMemFile* memFile);

    // Populate `out` with metadata for the file named by `key`.
    // Returns false if not found.
    bool GetInfo(const CHash& key, PakPack* out) const;

    // Decompress and decrypt the entry described by `pack` into `buffer`.
    // `buffer` must be >= pack.m_size bytes.
    bool GetData(const PakPack& pack, void* buffer);

    // Collect every archived file path that ends with .ext (case-insensitive).
    void CollectFileNamesByExtension(const char* ext, std::vector<std::string>& out) const;

protected:
    void Init();

    bool OpenPak01();   // GRF version 0x100–0x103 (DES encrypted index)
    bool OpenPak02();   // GRF version 0x200 (zlib compressed index)

    // Bit-swap all nibbles in a filename to obfuscate it
    static char ChangeLHBit_BYTE(char ch)
    {
        return static_cast<char>((ch << 4) | ((ch >> 4) & 0x0F));
    }
    static void  ModifyString(char* dst, const char* src, int len = -1);
    char*        MakeSeed(const char* fileName, char* seed) const;
    bool         IsNeverEncrypt(const char* fileName) const;

    CMemFile*             m_memFile        = nullptr;
    unsigned int          m_FileVer        = 0;
    unsigned int          m_FileCount      = 0;
    unsigned int          m_PakInfoOffset  = 0;
    unsigned int          m_PakInfoSize    = 0;
    std::vector<PakPack>  m_PakPack;        // sorted by CHash for binary search
    std::vector<uint8_t>  m_pDecBuf;        // scratch decompress buffer
};
