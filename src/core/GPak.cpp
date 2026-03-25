//===========================================================================
// GPak.cpp  –  GRF archive reader implementation
// Clean C++17 rewrite.
//
// The GRF format has three defined versions:
//   0x100  – Original Gravity format (DES-based encryption, byte-swapped names)
//   0x101–0x103 – Extended format (DES encrypted, numeric seed key)
//   0x200  – Modern format (zlib compressed file table)
//
// Reference: open-source OpenRO / eAthena client documentation.
//===========================================================================
#include "GPak.h"
#include "Hash.h"
#include "Types.h"
#include "File.h"
#include <algorithm>
#include <string.h>
#include <cstdlib>  // itoa
#include <cassert>
#include <vector>
#include <windows.h>
#include "DllMgr.h"
#include "../DebugLog.h"

#include "../cipher/CDec.h"

// File extensions that are NOT DES-encrypted inside GRF archives
static const char* const k_noEncryptExt[] = {
    ".mp3", ".wav", ".bmp", ".jpg", ".tga", nullptr
};

// "Magic" marker found at the start of .grf files
static const char k_GrfHeader[16] = "Master of Magic";

// ---------------------------------------------------------------------------

CGPak::CGPak()  { Init(); }
CGPak::~CGPak() = default;

void CGPak::Init()
{
    m_FileVer       = 0;
    m_FileCount     = 0;
    m_PakInfoOffset = 0;
    m_PakInfoSize   = 0;
    m_PakPack.clear();
    m_pDecBuf.clear();
    m_memFile       = nullptr;
}

// ---------------------------------------------------------------------------
// Open  –  Entry point.  Read the GRF header, detect version, dispatch.
// ---------------------------------------------------------------------------
bool CGPak::Open(CMemFile* memFile)
{
    if (!memFile) return false;
    m_memFile = memFile;
    DbgLog("[GPak::Open] memFile size: %u\n", memFile->size());

    // Read the fixed 46-byte GRF header.
    // Field byte offsets verified from the original decompiled Open() at 0x51ffa0:
    //   [30] uint32 data_offset  -> m_PakInfoOffset = data_offset + 46
    //   [34] uint32 count_low   ]
    //   [38] uint32 count_high  ] -> m_FileCount = count_high - count_low - 7
    //   [42] uint32 version     -> m_FileVer
    const unsigned char* hdr = memFile->read(0, 46);
    if (!hdr) return false;
    if (std::strncmp(reinterpret_cast<const char*>(hdr), k_GrfHeader, 15) != 0) return false;

    m_PakInfoOffset = *reinterpret_cast<const uint32_t*>(hdr + 30) + 46;
    m_FileCount     = *reinterpret_cast<const uint32_t*>(hdr + 38)
                    - *reinterpret_cast<const uint32_t*>(hdr + 34) - 7;
    m_FileVer       = *reinterpret_cast<const uint32_t*>(hdr + 42);

    DbgLog("[GPak::Open] GRF version: 0x%X, files: %u, pakInfoOffset: %u\n",
           m_FileVer, m_FileCount, m_PakInfoOffset);

    m_PakPack.resize(m_FileCount);

    const unsigned int verClass = m_FileVer & 0xFF00u;
    bool ok = false;
    if (verClass == 0x100)
    {
        DbgLog("[GPak::Open] Dispatching to OpenPak01 (v0x%X)\n", m_FileVer);
        ok = OpenPak01();
    }
    else if (verClass == 0x200)
    {
        DbgLog("[GPak::Open] Dispatching to OpenPak02, uncompress=%p\n",
               (void*)g_dllExports.uncompress);
        ok = OpenPak02();
    }
    else
    {
        DbgLog("[GPak::Open] FAIL: unknown GRF version 0x%X\n", m_FileVer);
    }

    if (!ok)
    {
        m_FileVer = m_FileCount = m_PakInfoOffset = m_PakInfoSize = 0;
        m_PakPack.clear();
        m_pDecBuf.clear();
        m_memFile = nullptr;
    }
    return ok;
}

// ---------------------------------------------------------------------------
// IsNeverEncrypt  –  Returns true for audio/image extensions that are stored
//                   plain (unencrypted) even inside an encrypted archive.
// ---------------------------------------------------------------------------
bool CGPak::IsNeverEncrypt(const char* fileName) const
{
    const char* ext = std::strrchr(fileName, '.');
    if (!ext) return false;
    for (int i = 0; k_noEncryptExt[i]; ++i)
        if (_stricmp(ext, k_noEncryptExt[i]) == 0) return true;
    return false;
}

// ---------------------------------------------------------------------------
// MakeSeed  –  Derive the 8-byte DES key from the filename base.
// Key format: 'n'  [first 6 chars of basename]  'k'
// ---------------------------------------------------------------------------
char* CGPak::MakeSeed(const char* fileName, char* seed) const
{
    const char* base = fileName;
    const char* end  = fileName + std::strlen(fileName);
    for (const char* p = fileName; p < end; ++p)
        if (*p == '\\' || *p == '/') base = p + 1;

    int baseLen = static_cast<int>(end - base);

    seed[0] = 'n';
    seed[7] = 'k';
    if (baseLen >= 6)
    {
        std::memcpy(seed + 1, base, 6);
    }
    else
    {
        std::memcpy(seed + 1, base, baseLen);
        std::memcpy(seed + 1 + baseLen, base, 6 - baseLen);
    }
    return seed;
}

// ---------------------------------------------------------------------------
// ModifyString  –  Nibble-swap each byte of src into dst (GRF obfuscation)
// ---------------------------------------------------------------------------
/*static*/ void CGPak::ModifyString(char* dst, const char* src, int len)
{
    int n = (len < 0) ? static_cast<int>(std::strlen(src) + 1) : len;
    for (int i = 0; i < n; ++i)
        dst[i] = ChangeLHBit_BYTE(src[i]);
}

// ---------------------------------------------------------------------------
// OpenPak01  –  Parse GRF 0x100–0x103 file table (DES-encrypted names)
// ---------------------------------------------------------------------------
bool CGPak::OpenPak01()
{
    // The file entry table starts at m_PakInfoOffset.
    // Each entry: [nameLen:4][encryptedName:nameLen][meta:17]
    // meta layout: compressSize:4  dataSize:4  size:4  type:1  offset:4

    const unsigned char* iter = m_memFile->read(m_PakInfoOffset,
        m_memFile->size() - m_PakInfoOffset);
    if (!iter) return false;

    unsigned int decKeyVal = 95001;
    unsigned int keySeq    = 1;

    std::vector<char> nameBuf(256);
    std::vector<char> decBuf(256);

    unsigned maxDecBufSize = 0;

    for (unsigned int i = 0; i < m_FileCount && i < m_PakPack.size(); ++i)
    {
        unsigned int nameLen = *reinterpret_cast<const uint32_t*>(iter);
        iter += 4;

        if (m_FileVer >= 0x101)
        {
            // The name is DES-encrypted with a numeric derived key
            iter += 2;  // skip 2-byte flags
            int plainLen = static_cast<int>(nameLen) - 6;

            unsigned int seedNum;
            if (m_FileVer == 0x101)
                seedNum = decKeyVal;
            else
                seedNum = 3 * (31669 - (keySeq >> 1));
            if (seedNum < 10000) seedNum += 85000;

            char strbuf[12];
            _itoa_s(seedNum, strbuf, sizeof(strbuf), 10);
            char seed[9] = "Pr";
            std::memcpy(seed + 2, strbuf, 4);
            seed[6] = strbuf[4];
            seed[7] = 'e';
            seed[8] = '\0';

            // DES decrypt into decBuf
            int decLen = 0;
            if (nameBuf.size() < static_cast<size_t>(plainLen + 1))
                nameBuf.resize(plainLen + 1);
            ModifyString(nameBuf.data(), reinterpret_cast<const char*>(iter), plainLen);

            CDec dec;
            dec.MakeKey(seed);
            dec.DataDecrypt(nameBuf.data(), plainLen,
                             decBuf.data(), &decLen, 1);

            decBuf[decLen] = '\0';
            nameLen -= 2;
        }
        else
        {
            // 0x100: nibble-swap only
            int rawLen = static_cast<int>(std::strlen(reinterpret_cast<const char*>(iter)) + 1);
            if (nameBuf.size() < static_cast<size_t>(rawLen))
                nameBuf.resize(rawLen);
            ModifyString(nameBuf.data(), reinterpret_cast<const char*>(iter), rawLen);
            decBuf = nameBuf;
        }

        // Skip past the (encrypted) name data
        const unsigned char* meta = iter + nameLen;
        iter = meta + 17;

        PakPack& pp = m_PakPack[i];
        pp.m_compressSize = *reinterpret_cast<const uint32_t*>(meta)     - *reinterpret_cast<const uint32_t*>(meta + 8) - 715;
        pp.m_dataSize     = *reinterpret_cast<const uint32_t*>(meta + 4) - 37579;
        pp.m_size         = *reinterpret_cast<const uint32_t*>(meta + 8);
        pp.m_type         = meta[12];
        pp.m_Offset       = *reinterpret_cast<const uint32_t*>(meta + 13);
        pp.m_fName.SetString(decBuf.data());

        // Mark encryption type
        if (!IsNeverEncrypt(decBuf.data()))
            pp.m_type |= 0x02;
        else
            pp.m_type |= 0x04;

        if (maxDecBufSize < pp.m_dataSize)
            maxDecBufSize = pp.m_dataSize;

        decKeyVal -= 2;
        keySeq    += 5;
    }

    // Sort by hash for binary search
    std::sort(m_PakPack.begin(), m_PakPack.end(), PakPrtLess{});
    m_pDecBuf.resize(maxDecBufSize);
    return true;
}

// ---------------------------------------------------------------------------
// OpenPak02  –  Parse GRF 0x200 (zlib-compressed file table)
// ---------------------------------------------------------------------------
bool CGPak::OpenPak02()
{
    // Header at m_PakInfoOffset: [compressedSize:4][plaintextSize:4][data...]
    const unsigned char* header = m_memFile->read(m_PakInfoOffset, 8);
    if (!header) return false;

    uint32_t compSz  = *reinterpret_cast<const uint32_t*>(header);
    uint32_t plainSz = *reinterpret_cast<const uint32_t*>(header + 4);

    const unsigned char* compressed = m_memFile->read(m_PakInfoOffset + 8, compSz);
    if (!compressed) return false;

    std::vector<uint8_t> plain(plainSz + 8);
    u32 destLen = plainSz + 8; // Changed to match u32* signature
    u32 sourceLen = compSz;
    if (!g_dllExports.uncompress || g_dllExports.uncompress(plain.data(), &destLen, compressed, sourceLen) != 0)
        return false;

    const unsigned char* iter = plain.data();
    unsigned maxDecBufSize = 0;

    for (unsigned int i = 0; i < m_FileCount && i < m_PakPack.size(); ++i)
    {
        const char* name = reinterpret_cast<const char*>(iter);
        size_t nameLen   = std::strlen(name) + 1;

        const unsigned char* meta = iter + nameLen;
        iter = meta + 17;

        PakPack& pp = m_PakPack[i];
        pp.m_compressSize = *reinterpret_cast<const uint32_t*>(meta);
        pp.m_dataSize     = *reinterpret_cast<const uint32_t*>(meta + 4);
        pp.m_size         = *reinterpret_cast<const uint32_t*>(meta + 8);
        pp.m_type         = meta[12];
        pp.m_Offset       = *reinterpret_cast<const uint32_t*>(meta + 13);
        pp.m_fName.SetString(name);

        if (maxDecBufSize < pp.m_dataSize)
            maxDecBufSize = pp.m_dataSize;
    }

    std::sort(m_PakPack.begin(), m_PakPack.end(), PakPrtLess{});
    m_pDecBuf.resize(maxDecBufSize);
    return true;
}

// ---------------------------------------------------------------------------
// GetInfo  –  Binary search for a filename hash
// ---------------------------------------------------------------------------
bool CGPak::GetInfo(const CHash& key, PakPack* out) const
{
    // Build a dummy PakPack with just the key set for comparison
    PakPack probe{};
    probe.m_fName = key;

    auto it = std::lower_bound(m_PakPack.begin(), m_PakPack.end(), probe, PakPrtLess{});
    if (it == m_PakPack.end() || it->m_fName != key)
        return false;

    if (out) *out = *it;
    return true;
}

void CGPak::CollectFileNamesByExtension(const char* ext, std::vector<std::string>& out) const
{
    if (!ext || !*ext) {
        return;
    }

    char normalizedExt[32] = {};
    if (ext[0] == '.') {
        std::strncpy(normalizedExt, ext, sizeof(normalizedExt) - 1);
    } else {
        normalizedExt[0] = '.';
        std::strncpy(normalizedExt + 1, ext, sizeof(normalizedExt) - 2);
    }

    for (const PakPack& pack : m_PakPack) {
        const char* name = pack.m_fName.m_String;
        if (!name || !*name) {
            continue;
        }

        const char* dot = std::strrchr(name, '.');
        if (!dot) {
            continue;
        }
        if (_stricmp(dot, normalizedExt) == 0) {
            out.emplace_back(name);
        }
    }
}

// ---------------------------------------------------------------------------
// GetData  –  Read, decrypt and decompress one file entry
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// GetData  –  Read, decrypt and decompress one file entry
//
// Three-branch logic per the reference decompiled binary (0x520150):
//   type & 0x02 -> full DES (DataDecrypt), then check type & 0x01 for zlib
//   type & 0x04 -> header DES (DataDecryptHeader), then check type & 0x01
//   else        -> raw bytes, then check type & 0x01 for zlib
// ---------------------------------------------------------------------------
bool CGPak::GetData(const PakPack& pack, void* buffer)
{
    const unsigned char* raw = m_memFile->read(pack.m_Offset + 46, pack.m_dataSize);
    if (!raw) return false;

    // Ensure decrypt scratch buffer is large enough
    if (m_pDecBuf.size() < pack.m_dataSize + 8)
        m_pDecBuf.resize(pack.m_dataSize + 8);

    unsigned char*       dst   = static_cast<unsigned char*>(buffer);
    const uint8_t        mtype = pack.m_type;
    const unsigned char* First = raw;
    int dataLen = static_cast<int>(pack.m_compressSize);

    if (mtype & 0x02)
    {
        // Full DES block cipher decryption
        int rounds = 1;
        if (pack.m_compressSize > 0)
        {
            unsigned int tmp = pack.m_compressSize;
            rounds = 0;
            while (tmp) { ++rounds; tmp /= 10; }
            if (rounds <= 0) rounds = 1;
        }
        char seed[8];
        MakeSeed(pack.m_fName.m_String, seed);
        CDec dec;
        dec.MakeKey(seed);
        int decLen = 0;
        dec.DataDecrypt(
            reinterpret_cast<char*>(const_cast<unsigned char*>(raw)),
            static_cast<int>(pack.m_dataSize),
            reinterpret_cast<char*>(m_pDecBuf.data()),
            &decLen, rounds);
        First   = m_pDecBuf.data();
        dataLen = decLen;
    }
    else if (mtype & 0x04)
    {
        // Header-only DES encryption (images/sounds in v1 GRF)
        char seed[8];
        MakeSeed(pack.m_fName.m_String, seed);
        CDec dec;
        dec.MakeKey(seed);
        int decLen = 0;
        dec.DataDecryptHeader(
            reinterpret_cast<char*>(const_cast<unsigned char*>(raw)),
            static_cast<int>(pack.m_dataSize),
            reinterpret_cast<char*>(m_pDecBuf.data()),
            &decLen);
        First   = m_pDecBuf.data();
        dataLen = decLen;
    }
    // else: First = raw, dataLen = compressSize (set above)

    if (mtype & 0x01)
    {
        // zlib-compressed: decompress First[dataLen] -> dst[pack.m_size]
        u32 outLen = pack.m_size;
        u32 srcLen = static_cast<u32>(dataLen);
        if (!g_dllExports.uncompress ||
            g_dllExports.uncompress(dst, &outLen, First, srcLen) != 0)
            return false;
    }
    else
    {
        std::memcpy(dst, First, pack.m_size);
    }

    return true;
}
