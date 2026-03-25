#pragma once
//===========================================================================
// CDec.h  –  DES block cipher used to decrypt GRF archive entries
// Clean C++17 rewrite.
//
// This is a simplified, single-table DES variant used by Gravity to
// encrypt the name table and individual file data in .grf archives.
// It is NOT standard DES (the S-box is a single table, not 8).
//===========================================================================
#include <cstdint>

union BIT64
{
    uint64_t    qw;
    uint32_t    dw[2];
    uint8_t     b[8];

    BIT64() : qw(0) {}
    explicit BIT64(uint64_t v) : qw(v) {}
    BIT64& operator=(uint64_t v) { qw = v; return *this; }
    operator uint64_t() const { return qw; }
};

//===========================================================================
// CDec  –  Gravity's single-round DES-derived cipher
//
// Usage:
//   CDec dec("mykey12");          // 8-byte key (char[8], NOT null terminated)
//   dec.DataDecrypt(in, inLen, out, &outLen, rounds);
//===========================================================================
class CDec
{
public:
    // Construct with an 8-byte key string (copied into m_rawKey for MakeKey)
    explicit CDec(const char* key = nullptr);
    ~CDec() = default;

    // Set key (must be called before any Encrypt/Decrypt call)
    void MakeKey(const char* key);

    // Decrypt pIn[inSize] → pOut.  *outSize = inSize on return.
    // mod controls which 8-byte blocks are actually decrypted vs pass-through:
    //   Every block whose index (0-based) < 20 OR whose index % effectiveMod == 0
    //   is DES-decrypted; other blocks are passed through with byte-shuffle applied.
    int DataDecrypt(char* pIn, int inSize, char* pOut, int* outSize, int mod);

    // Decrypt only the first ≤20 8-byte blocks (used for filename decryption).
    int DataDecryptHeader(char* pIn, int inSize, char* pOut, int* outSize);

    // Encrypt pIn[inSize] → pOut (mirror of DataDecrypt).
    int DataEncrypt(char* pIn, int inSize, char* pOut, int* outSize, int mod);

private:
    void IP(BIT64* text) const;   // Initial permutation
    void FP(BIT64* text) const;   // Final permutation

    // 16 48-bit subkeys derived from the 64-bit master key
    BIT64 m_key[16];
};
