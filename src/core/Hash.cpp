//===========================================================================
// Hash.cpp  –  Case-insensitive filename hash for GRF PAK archive lookup
// Clean C++17 rewrite.
//
// Algorithm: djb2 hash over the lower-cased filename.
// Comparison: hash code first (fast), then lexicographic strcmp (fallback).
//===========================================================================
#include "Hash.h"
#include <cstring>

// ---------------------------------------------------------------------------
// djb2 hash over the already-lowercased m_String
// ---------------------------------------------------------------------------
void CHash::CreateHashCode()
{
    unsigned int hashVal = 5381;
    unsigned int len     = static_cast<unsigned int>(std::strlen(m_String));
    for (unsigned int i = 0; i < len; ++i)
        hashVal = hashVal * 33 + static_cast<unsigned char>(m_String[i]);
    m_hashCode = hashVal;
}

CHash::CHash()
{
    m_String[0] = '\0';
    m_hashCode  = 5381;
}

CHash::CHash(const char* filename)
{
    SetString(filename);
}

void CHash::SetString(const char* filename)
{
    if (!filename)
    {
        m_String[0] = '\0';
        m_hashCode  = 5381;
        return;
    }
    // Canonicalize path bytes for stable archive keys:
    // - unify '/' and '\\'
    // - strip leading ".\\"
    // - lowercase ASCII only (preserve non-ASCII bytes as-is)
    std::size_t dst = 0;
    const unsigned char* src = reinterpret_cast<const unsigned char*>(filename);

    if (src[0] == '.' && (src[1] == '\\' || src[1] == '/')) {
        src += 2;
    }

    while (*src && dst < static_cast<std::size_t>(StringLen - 1)) {
        unsigned char ch = *src++;
        if (ch == '/') {
            ch = '\\';
        }
        if (ch >= 'A' && ch <= 'Z') {
            ch = static_cast<unsigned char>(ch - 'A' + 'a');
        }
        m_String[dst++] = static_cast<char>(ch);
    }
    m_String[dst] = '\0';
    CreateHashCode();
}

bool CHash::operator==(const CHash& rhs) const
{
    return m_hashCode == rhs.m_hashCode &&
           strcmp(m_String, rhs.m_String) == 0;
}

bool CHash::operator!=(const CHash& rhs) const
{
    return !(*this == rhs);
}

bool CHash::operator<(const CHash& rhs) const
{
    if (m_hashCode != rhs.m_hashCode)
        return m_hashCode < rhs.m_hashCode;
    return strcmp(m_String, rhs.m_String) < 0;
}
