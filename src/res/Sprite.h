#pragma once

#include "Res.h"
#include "Texture.h"
#include <vector>

struct SprImg {
    int width;
    int height;
    int isHalfW;
    int isHalfH;
    CTexture* tex;
    std::vector<unsigned char> indices;
    std::vector<unsigned int> rgba;
};

class CSprRes : public CRes {
public:
    std::vector<SprImg*> m_sprites[2];
    unsigned int m_pal[256];
    int m_count;

    CSprRes();
    virtual ~CSprRes();

    unsigned char* HalfImage(unsigned char* src, int w, int h, int dw, int dh);
    unsigned char* ZeroCompression(unsigned char* src, int w, int h, unsigned short& outSize);
    unsigned char* ZeroDecompression(unsigned char* src, int w, int h);

    virtual bool LoadFromBuffer(const char* fName, const unsigned char* buffer, int size) override;
    virtual bool Load(const char* fName) override;
    virtual CRes* Clone() override;
    virtual void Reset() override;

    const SprImg* GetSprite(int clipType, int index) const;
};
