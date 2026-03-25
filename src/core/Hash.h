#pragma once
//===========================================================================
// Hash.h  –  Filename hash used as a key inside .grf PAK archives
// Clean C++17 rewrite.
//===========================================================================
#include <string.h>

//===========================================================================
// CHash  –  Case-insensitive 256-byte string hash used for fast binary
//           search of PakPack arrays inside CGPak.
//===========================================================================
class CHash
{
public:
    static constexpr int StringLen = 256;

    CHash();
    explicit CHash(const char* filename);

    void SetString(const char* filename);

    bool operator==(const CHash& rhs) const;
    bool operator!=(const CHash& rhs) const;
    bool operator< (const CHash& rhs) const;

    // Public because CGPak needs direct access for binary search
    char m_String[StringLen] = {};
    unsigned int m_hashCode = 5381;

private:
    void CreateHashCode();
};
