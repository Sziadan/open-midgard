#pragma once

#include "Res.h"

#include <string>
#include <vector>

#pragma pack(push, 1)
struct LMIndex {
    int a;
    int r;
    int g;
    int b;
};

struct ColorChannel {
    unsigned char m_buffer[40];
};

struct GndSurfaceFmt {
    float u[4];
    float v[4];
    u16 textureId;
    u16 lightmapId;
    u32 color;
};

struct GndCellFmt17 {
    float height[4];
    int topSurfaceId;
    int frontSurfaceId;
    int rightSurfaceId;
};
#pragma pack(pop)

static_assert(sizeof(LMIndex) == 16, "LMIndex size mismatch");
static_assert(sizeof(ColorChannel) == 40, "ColorChannel size mismatch");
static_assert(sizeof(GndSurfaceFmt) == 40, "GndSurfaceFmt size mismatch");
static_assert(sizeof(GndCellFmt17) == 28, "GndCellFmt17 size mismatch");

class CGndRes : public CRes {
public:
    CGndRes();
    ~CGndRes() override;

    bool LoadFromBuffer(const char* fName, const unsigned char* buffer, int size) override;
    CRes* Clone() override;
    void Reset() override;

    const GndCellFmt17* GetCell(int x, int y) const;
    const GndSurfaceFmt* GetSurface(int index) const;
    const char* GetTextureName(int index) const;

    bool m_newVer;
    u8 m_verMajor;
    u8 m_verMinor;
    int m_width;
    int m_height;
    float m_zoom;
    int m_numTexture;
    std::vector<std::string> m_texNameTable;
    int m_numLightmap;
    std::vector<unsigned char> m_lminfoRaw;
    std::vector<LMIndex> m_lmindex;
    std::vector<ColorChannel> m_colorchannel;
    int m_numSurface;
    std::vector<GndSurfaceFmt> m_surface;
    std::vector<GndCellFmt17> m_cells;
};