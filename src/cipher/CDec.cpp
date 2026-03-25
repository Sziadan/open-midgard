//===========================================================================
// CDec.cpp  –  DES block cipher implementation
// Clean C++17 rewrite.
//
// Algorithm: Gravity's custom simplified DES variant.
// - Single S-box instead of 8 standard DES S-boxes.
// - IP/FP (initial/final) permutations from standard DES tables.
// - 16 subkeys derived via the standard DES key schedule.
// - Block size: 8 bytes (64 bits).
// - DataDecrypt applies full DES to blocks 0..19, and every (mod'th) block
//   beyond that; other blocks get a byte-shuffle pass-through.
//===========================================================================
#include "CDec.h"
#include <cstring>
#include <algorithm>
#include <new>
#include <cstdint>

// ---------------------------------------------------------------------------
// DES permutation tables (standard DES)
// ---------------------------------------------------------------------------
static const uint8_t ip_table[64] = {
    58,50,42,34,26,18,10, 2, 60,52,44,36,28,20,12, 4,
    62,54,46,38,30,22,14, 6, 64,56,48,40,32,24,16, 8,
    57,49,41,33,25,17, 9, 1, 59,51,43,35,27,19,11, 3,
    61,53,45,37,29,21,13, 5, 63,55,47,39,31,23,15, 7
};

static const uint8_t fp_table[64] = {
    40, 8,48,16,56,24,64,32, 39, 7,47,15,55,23,63,31,
    38, 6,46,14,54,22,62,30, 37, 5,45,13,53,21,61,29,
    36, 4,44,12,52,20,60,28, 35, 3,43,11,51,19,59,27,
    34, 2,42,10,50,18,58,26, 33, 1,41, 9,49,17,57,25
};

static const uint8_t expand_table[48] = {
    32, 1, 2, 3, 4, 5, 4, 5, 6, 7, 8, 9,
     8, 9,10,11,12,13,12,13,14,15,16,17,
    16,17,18,19,20,21,20,21,22,23,24,25,
    24,25,26,27,28,29,28,29,30,31,32, 1
};

static const uint8_t pc1_table[56] = {
    57,49,41,33,25,17, 9, 1,58,50,42,34,26,18,
    10, 2,59,51,43,35,27,19,11, 3,60,52,44,36,
    63,55,47,39,31,23,15, 7,62,54,46,38,30,22,
    14, 6,61,53,45,37,29,21,13, 5,28,20,12, 4
};

static const uint8_t pc2_table[48] = {
    14,17,11,24, 1, 5, 3,28,15, 6,21,10,
    23,19,12, 4,26, 8,16, 7,27,20,13, 2,
    41,52,31,37,47,55,30,40,51,45,33,48,
    44,49,39,56,34,53,46,42,50,36,29,32
};

static const int key_shifts[16] = {
    1,1,2,2,2,2,2,2,1,2,2,2,2,2,2,1
};

// Gravity's custom single-entry S-box (replaces all 8 standard DES S-boxes)
static const uint8_t s_table[4][64] = {
{
    14, 4,13, 1, 2,15,11, 8, 3,10, 6,12, 5, 9, 0, 7,
     0,15, 7, 4,14, 2,13, 1,10, 6,12,11, 9, 5, 3, 8,
     4, 1,14, 8,13, 6, 2,11,15,12, 9, 7, 3,10, 5, 0,
    15,12, 8, 2, 4, 9, 1, 7, 5,11, 3,14,10, 0, 6,13
},
{
    15, 1, 8,14, 6,11, 3, 4, 9, 7, 2,13,12, 0, 5,10,
     3,13, 4, 7,15, 2, 8,14,12, 0, 1,10, 6, 9,11, 5,
     0,14, 7,11,10, 4,13, 1, 5, 8,12, 6, 9, 3, 2,15,
    13, 8,10, 1, 3,15, 4, 2,11, 6, 7,12, 0, 5,14, 9
},
{
    10, 0, 9,14, 6, 3,15, 5, 1,13,12, 7,11, 4, 2, 8,
    13, 7, 0, 9, 3, 4, 6,10, 2, 8, 5,14,12,11,15, 1,
    13, 6, 4, 9, 8,15, 3, 0,11, 1, 2,12, 5,10,14, 7,
     1,10,13, 0, 6, 9, 8, 7, 4,15,14, 3,11, 5, 2,12
},
{
     7,13,14, 3, 0, 6, 9,10, 1, 2, 8, 5,11,12, 4,15,
    13, 8,11, 5, 6,15, 0, 3, 4, 7, 2,12, 1,10,14, 9,
    10, 6, 9, 0,12,11, 7,13,15, 1, 3,14, 5, 2, 8, 4,
     3,15, 0, 6,10, 1,13, 8, 9, 4, 5,11,12, 7, 2,14
}
};

static const uint8_t tp_table[32] = {
    16, 7,20,21,29,12,28,17, 1,15,23,26, 5,18,31,10,
     2, 8,24,14,32,27, 3, 9,19,13,30, 6,22,11, 4,25
};

static const uint8_t mask[8] = {
    0x80,0x40,0x20,0x10,0x08,0x04,0x02,0x01
};

// ---------------------------------------------------------------------------
// Byte-swap table used in the non-DES pass-through blocks (block type 2)
// Key positions: special value permutation for byte[7] of each block.
// ---------------------------------------------------------------------------
static uint8_t swapByte7(uint8_t v)
{
    static const uint8_t tbl1[] = { 0x00,0x01,0x2B,0x48,0x60,0x68,0x6C };
    static const uint8_t tbl2[] = { 0x2B,0x68,0x00,0x77,0xFF,0x01,0x80 };
    static const uint8_t tbl3[] = { 0x80,0xB9,0xC0,0xEB,0xFE,0xFF };
    static const uint8_t tbl4[] = { 0x6C,0x40,0xB9,0x02,0xEB,0x60 };

    if (v < 0x78)
    {
        if (v == 0x77) return 0x48;
        for (int i = 0; i < 7; ++i)
            if (v == tbl1[i]) return tbl2[i];
    }
    else
    {
        for (int i = 0; i < 6; ++i)
            if (v == tbl3[i]) return tbl4[i];
    }
    return v;
}

// ---------------------------------------------------------------------------
// CDec implementation
// ---------------------------------------------------------------------------
CDec::CDec(const char* key)
{
    std::memset(m_key, 0, sizeof(m_key));
    if (key) MakeKey(key);
}

void CDec::IP(BIT64* text) const
{
    BIT64 t{};
    for (unsigned int i = 0; i < 64; ++i)
    {
        uint8_t srcBit = ip_table[i] - 1;
        if (text->b[srcBit >> 3] & mask[srcBit & 7])
            t.b[i >> 3] |= mask[i & 7];
    }
    *text = t;
}

void CDec::FP(BIT64* text) const
{
    BIT64 t{};
    for (unsigned int i = 0; i < 64; ++i)
    {
        uint8_t srcBit = fp_table[i] - 1;
        if (text->b[srcBit >> 3] & mask[srcBit & 7])
            t.b[i >> 3] |= mask[i & 7];
    }
    *text = t;
}

void CDec::MakeKey(const char* key)
{
    // Step 1: PC-1 permutation on the 64-bit key → 56-bit permuted key (C, D)
    uint64_t k64 = 0;
    for (int i = 0; i < 8; ++i)
        reinterpret_cast<uint8_t*>(&k64)[i] = static_cast<uint8_t>(key[i]);

    uint64_t cd = 0;
    for (int i = 0; i < 56; ++i)
    {
        uint8_t srcBit = pc1_table[i] - 1;
        uint64_t bit = (k64 >> (63 - srcBit)) & 1;
        cd |= (bit << (55 - i));
    }

    uint32_t C = static_cast<uint32_t>(cd >> 28) & 0x0FFFFFFFu;
    uint32_t D = static_cast<uint32_t>(cd)        & 0x0FFFFFFFu;

    // Step 2: Generate 16 subkeys
    for (int round = 0; round < 16; ++round)
    {
        int shift = key_shifts[round];
        C = ((C << shift) | (C >> (28 - shift))) & 0x0FFFFFFFu;
        D = ((D << shift) | (D >> (28 - shift))) & 0x0FFFFFFFu;

        uint64_t cd56 = (static_cast<uint64_t>(C) << 28) | D;

        // PC-2 → 48-bit subkey
        uint64_t subkey = 0;
        for (int i = 0; i < 48; ++i)
        {
            uint8_t srcBit = pc2_table[i] - 1;
            uint64_t bit = (cd56 >> (55 - srcBit)) & 1;
            subkey |= (bit << (47 - i));
        }

        // Store as BIT64 in the round-key array
        m_key[round].dw[0] = static_cast<uint32_t>(subkey >> 16);
        m_key[round].dw[1] = static_cast<uint32_t>(subkey) & 0xFFFF;
    }
}

// ---------------------------------------------------------------------------
// One DES decryption round on a single 8-byte block (using key index keyIdx)
// ---------------------------------------------------------------------------
static void desDecryptBlock(const BIT64 keys[16], BIT64* block)
{
    // IP
    BIT64 state{};
    for (unsigned i = 0; i < 64; ++i)
    {
        uint8_t src = ip_table[i] - 1;
        if (block->b[src >> 3] & mask[src & 7])
            state.b[i >> 3] |= mask[i & 7];
    }
    *block = state;

    // 16 Feistel rounds in REVERSE order (decryption)
    uint32_t L = block->dw[0];
    uint32_t R = block->dw[1];

    for (int r = 15; r >= 0; --r)
    {
        // Expand R (32→48 bits)
        uint64_t Rexp = 0;
        BIT64 tmpR;
        tmpR.dw[0] = R;
        tmpR.dw[1] = 0;
        for (int i = 0; i < 48; ++i)
        {
            uint8_t srcBit = expand_table[i] - 1;
            if (tmpR.b[srcBit >> 3] & mask[srcBit & 7])
                Rexp |= (uint64_t(1) << (47 - i));
        }

        // XOR with subkey
        uint64_t keyBits = (uint64_t(keys[r].dw[0]) << 16) | keys[r].dw[1];
        Rexp ^= keyBits;

        // S-box substitution (4 S-boxes, 6 bits each)
        uint32_t Sout = 0;
        for (int i = 0; i < 4; ++i)
        {
            uint8_t b6 = static_cast<uint8_t>((Rexp >> (42 - i * 6)) & 0x3F);
            int row = ((b6 & 0x20) >> 4) | (b6 & 0x01);
            int col = (b6 >> 1) & 0x0F;
            Sout = (Sout << 4) | s_table[i][row * 16 + col];
        }

        // P permutation
        uint32_t Pout = 0;
        for (int i = 0; i < 32; ++i)
        {
            uint8_t srcBit = tp_table[i] - 1;
            if ((Sout >> (31 - srcBit)) & 1)
                Pout |= (1u << (31 - i));
        }

        uint32_t newR = L ^ Pout;
        L = R;
        R = newR;
    }

    block->dw[0] = R;
    block->dw[1] = L;

    // FP
    state = BIT64{};
    for (unsigned i = 0; i < 64; ++i)
    {
        uint8_t src = fp_table[i] - 1;
        if (block->b[src >> 3] & mask[src & 7])
            state.b[i >> 3] |= mask[i & 7];
    }
    *block = state;
}

// ---------------------------------------------------------------------------
// Compute the effective modulus from the raw caller-supplied `mod`
// ---------------------------------------------------------------------------
static int calcMod(int mod)
{
    if (mod >= 7) return mod + 15;
    if (mod >= 5) return mod + 9;
    if (mod >= 3) return mod + 1;
    return 1;
}

int CDec::DataDecrypt(char* pIn, int inSize, char* pOut, int* outSize, int mod)
{
    if (!pIn || !pOut || !outSize) return 0;
    *outSize = inSize;

    int effectiveMod = calcMod(mod);
    int blockIdx     = 0;
    int shuffleK     = 0; // Counter within the 8-block shuffle sequence

    for (int offset = 0; offset + 8 <= inSize; offset += 8, ++blockIdx)
    {
        bool doFullDES = (blockIdx < 20) || (blockIdx % effectiveMod == 0);

        if (doFullDES)
        {
            BIT64 block;
            std::memcpy(&block, pIn + offset, 8);
            desDecryptBlock(m_key, &block);
            std::memcpy(pOut + offset, &block, 8);
            shuffleK = 0; // Reset shuffle sequence counter
        }
        else
        {
            // Non-DES pass-through with byte shuffle
            uint8_t* src = reinterpret_cast<uint8_t*>(pIn  + offset);
            uint8_t* dst = reinterpret_cast<uint8_t*>(pOut + offset);

            ++shuffleK;
            if (shuffleK == 8)
            {
                // Full shuffle: permute bytes and replace byte[7]
                dst[0] = src[3]; dst[1] = src[4]; dst[2] = src[5];
                dst[3] = src[6]; dst[4] = src[7];
                // Byte index 5 and 6 get original positions 0 and 1
                dst[5] = src[0]; dst[6] = src[1];
                dst[7] = swapByte7(src[2]);
                shuffleK = 1;
            }
            else
            {
                std::memcpy(dst, src, 8);
            }
        }
    }
    return 1;
}

int CDec::DataDecryptHeader(char* pIn, int inSize, char* pOut, int* outSize)
{
    if (!pIn || !pOut || !outSize) return 0;
    *outSize = inSize;
    std::memcpy(pOut, pIn, inSize);

    // Decrypt only the first min(20, inSize/8) blocks
    int nBlocks = std::min(20, inSize / 8);
    for (int i = 0; i < nBlocks; ++i)
    {
        BIT64 block;
        std::memcpy(&block, pOut + i * 8, 8);
        desDecryptBlock(m_key, &block);
        std::memcpy(pOut + i * 8, &block, 8);
    }
    return 1;
}

int CDec::DataEncrypt(char* pIn, int inSize, char* pOut, int* outSize, int mod)
{
    // Round up to block boundary
    int alignedSize = (inSize + 7) & ~7;
    *outSize = alignedSize;

    int effectiveMod = calcMod(mod);
    int blockIdx     = 0;
    int shuffleK     = 0;

    for (int offset = 0; offset + 8 <= alignedSize; offset += 8, ++blockIdx)
    {
        bool doFullDES = (blockIdx < 20) || (blockIdx % effectiveMod == 0);

        if (doFullDES)
        {
            BIT64 block{};
            int copyLen = std::min(8, inSize - offset);
            if (copyLen > 0) std::memcpy(&block, pIn + offset, copyLen);

            // Encrypt: run Feistel in FORWARD order
            // (re-use desDecryptBlock and key order 0..15 instead of 15..0)
            // For simplicity we implement encrypt as decrypt with reversed key order.
            // Since this is a Feistel cipher, reversing the subkeys inverts decryption.
            BIT64 revKeys[16];
            for (int k = 0; k < 16; ++k) revKeys[k] = m_key[15 - k];
            desDecryptBlock(revKeys, &block);

            std::memcpy(pOut + offset, &block, 8);
            shuffleK = 0;
        }
        else
        {
            uint8_t* src = reinterpret_cast<uint8_t*>(pIn  + offset);
            uint8_t* dst = reinterpret_cast<uint8_t*>(pOut + offset);
            ++shuffleK;
            if (shuffleK == 8)
            {
                dst[3] = src[0]; dst[4] = src[1]; dst[5] = src[2];
                dst[6] = src[3]; dst[7] = src[4];
                dst[0] = src[5]; dst[1] = src[6];
                dst[2] = swapByte7(src[7]);
                shuffleK = 1;
            }
            else
            {
                std::memcpy(dst, src, 8);
            }
        }
    }
    return 1;
}

// ---------------------------------------------------------------------------
// C-linkage wrappers for use from GPak.cpp
// ---------------------------------------------------------------------------
extern "C" {
    void CDec_Init(CDec* self, const char* key)  { new(self) CDec(key); }
    void CDec_Deinit(CDec* self)                  { self->~CDec(); }
    void CDec_DataDecrypt(CDec* self, const char* src, unsigned srcLen,
                          char* dst, int* dstLen, int rounds)
    {
        self->DataDecrypt(const_cast<char*>(src), static_cast<int>(srcLen),
                          dst, dstLen, rounds);
    }
}
