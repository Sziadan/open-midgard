#include "GndRes.h"

#include <cstring>

namespace {
bool ReadU8(const unsigned char* buffer, size_t size, size_t* offset, u8* outValue)
{
    if (!offset || !outValue || *offset + sizeof(u8) > size) {
        return false;
    }
    *outValue = buffer[*offset];
    *offset += sizeof(u8);
    return true;
}

bool ReadI32(const unsigned char* buffer, size_t size, size_t* offset, int* outValue)
{
    if (!offset || !outValue || *offset + sizeof(int) > size) {
        return false;
    }
    std::memcpy(outValue, buffer + *offset, sizeof(int));
    *offset += sizeof(int);
    return true;
}

bool ReadF32(const unsigned char* buffer, size_t size, size_t* offset, float* outValue)
{
    if (!offset || !outValue || *offset + sizeof(float) > size) {
        return false;
    }
    std::memcpy(outValue, buffer + *offset, sizeof(float));
    *offset += sizeof(float);
    return true;
}

std::string ReadFixedString(const unsigned char* buffer, size_t length)
{
    size_t actualLength = 0;
    while (actualLength < length && buffer[actualLength] != 0) {
        ++actualLength;
    }
    return std::string(reinterpret_cast<const char*>(buffer), actualLength);
}
}

CGndRes::CGndRes()
{
    Reset();
}

CGndRes::~CGndRes() = default;

bool CGndRes::LoadFromBuffer(const char* fName, const unsigned char* buffer, int size)
{
    (void)fName;
    Reset();

    if (!buffer || size < 6) {
        return false;
    }

    if (std::memcmp(buffer, "GRGN", 4) != 0) {
        return false;
    }

    size_t offset = 4;
    if (!ReadU8(buffer, static_cast<size_t>(size), &offset, &m_verMajor)
        || !ReadU8(buffer, static_cast<size_t>(size), &offset, &m_verMinor)) {
        return false;
    }

    if (m_verMajor != 1 || m_verMinor <= 6) {
        return false;
    }

    if (!ReadI32(buffer, static_cast<size_t>(size), &offset, &m_width)
        || !ReadI32(buffer, static_cast<size_t>(size), &offset, &m_height)
        || !ReadF32(buffer, static_cast<size_t>(size), &offset, &m_zoom)
        || !ReadI32(buffer, static_cast<size_t>(size), &offset, &m_numTexture)) {
        return false;
    }

    int maxTexName = 0;
    if (!ReadI32(buffer, static_cast<size_t>(size), &offset, &maxTexName)
        || m_width <= 0 || m_height <= 0 || m_numTexture < 0 || maxTexName <= 0) {
        return false;
    }

    if (offset + static_cast<size_t>(m_numTexture) * static_cast<size_t>(maxTexName) > static_cast<size_t>(size)) {
        return false;
    }

    m_texNameTable.resize(static_cast<size_t>(m_numTexture));
    for (int i = 0; i < m_numTexture; ++i) {
        m_texNameTable[static_cast<size_t>(i)] = ReadFixedString(buffer + offset, static_cast<size_t>(maxTexName));
        offset += static_cast<size_t>(maxTexName);
    }

    int lightmapWidth = 0;
    int lightmapHeight = 0;
    int lightmapPf = 0;
    if (!ReadI32(buffer, static_cast<size_t>(size), &offset, &m_numLightmap)
        || !ReadI32(buffer, static_cast<size_t>(size), &offset, &lightmapWidth)
        || !ReadI32(buffer, static_cast<size_t>(size), &offset, &lightmapHeight)
        || !ReadI32(buffer, static_cast<size_t>(size), &offset, &lightmapPf)
        || m_numLightmap < 0) {
        return false;
    }

    if (m_verMinor == 7) {
        const size_t lmInfoBytes = static_cast<size_t>(m_numLightmap) * 256u;
        if (offset + lmInfoBytes > static_cast<size_t>(size)) {
            return false;
        }
        m_lminfoRaw.assign(buffer + offset, buffer + offset + lmInfoBytes);
        offset += lmInfoBytes;
    } else {
        const size_t lmIndexBytes = static_cast<size_t>(m_numLightmap) * sizeof(LMIndex);
        if (offset + lmIndexBytes > static_cast<size_t>(size)) {
            return false;
        }
        m_lmindex.resize(static_cast<size_t>(m_numLightmap));
        std::memcpy(m_lmindex.data(), buffer + offset, lmIndexBytes);
        offset += lmIndexBytes;

        int colorChannelCount = 0;
        if (!ReadI32(buffer, static_cast<size_t>(size), &offset, &colorChannelCount) || colorChannelCount < 0) {
            return false;
        }

        const size_t colorChannelBytes = static_cast<size_t>(colorChannelCount) * sizeof(ColorChannel);
        if (offset + colorChannelBytes > static_cast<size_t>(size)) {
            return false;
        }
        m_colorchannel.resize(static_cast<size_t>(colorChannelCount));
        std::memcpy(m_colorchannel.data(), buffer + offset, colorChannelBytes);
        offset += colorChannelBytes;
    }

    if (!ReadI32(buffer, static_cast<size_t>(size), &offset, &m_numSurface) || m_numSurface < 0) {
        return false;
    }

    const size_t surfaceBytes = static_cast<size_t>(m_numSurface) * sizeof(GndSurfaceFmt);
    if (offset + surfaceBytes > static_cast<size_t>(size)) {
        return false;
    }
    m_surface.resize(static_cast<size_t>(m_numSurface));
    std::memcpy(m_surface.data(), buffer + offset, surfaceBytes);
    offset += surfaceBytes;

    const size_t cellCount = static_cast<size_t>(m_width) * static_cast<size_t>(m_height);
    const size_t cellBytes = cellCount * sizeof(GndCellFmt17);
    if (offset + cellBytes > static_cast<size_t>(size)) {
        return false;
    }
    m_cells.resize(cellCount);
    std::memcpy(m_cells.data(), buffer + offset, cellBytes);

    m_newVer = true;
    return true;
}

CRes* CGndRes::Clone()
{
    return new CGndRes();
}

void CGndRes::Reset()
{
    m_newVer = false;
    m_verMajor = 0;
    m_verMinor = 0;
    m_width = 0;
    m_height = 0;
    m_zoom = 0.0f;
    m_numTexture = 0;
    m_texNameTable.clear();
    m_numLightmap = 0;
    m_lminfoRaw.clear();
    m_lmindex.clear();
    m_colorchannel.clear();
    m_numSurface = 0;
    m_surface.clear();
    m_cells.clear();
}

const GndCellFmt17* CGndRes::GetCell(int x, int y) const
{
    if (x < 0 || y < 0 || x >= m_width || y >= m_height || m_cells.empty()) {
        return nullptr;
    }
    return &m_cells[static_cast<size_t>(y) * static_cast<size_t>(m_width) + static_cast<size_t>(x)];
}

const GndSurfaceFmt* CGndRes::GetSurface(int index) const
{
    if (index < 0 || index >= m_numSurface || m_surface.empty()) {
        return nullptr;
    }
    return &m_surface[static_cast<size_t>(index)];
}

const char* CGndRes::GetTextureName(int index) const
{
    if (index < 0 || index >= m_numTexture || m_texNameTable.empty()) {
        return nullptr;
    }
    return m_texNameTable[static_cast<size_t>(index)].c_str();
}