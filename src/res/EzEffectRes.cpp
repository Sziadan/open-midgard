#include "EzEffectRes.h"

#include "core/File.h"
#include "render/Renderer.h"
#include "DebugLog.h"

#include <algorithm>
#include <cstring>

namespace {

constexpr u32 kAniClipVersion = 148u;

bool ReadBytes(const unsigned char* buffer, int size, size_t* offset, void* out, size_t count)
{
    if (!buffer || !offset || !out || *offset + count > static_cast<size_t>(size)) {
        return false;
    }
    std::memcpy(out, buffer + *offset, count);
    *offset += count;
    return true;
}

std::string ReadFixedString(const unsigned char* buffer, int size, size_t* offset, size_t fieldSize)
{
    std::string value;
    if (!buffer || !offset || *offset + fieldSize > static_cast<size_t>(size)) {
        return value;
    }

    const char* bytes = reinterpret_cast<const char*>(buffer + *offset);
    const size_t length = strnlen(bytes, fieldSize);
    value.assign(bytes, length);
    *offset += fieldSize;
    return value;
}

bool ResolveEffectTextureName(const std::string& rawName, std::string* outPath)
{
    if (!outPath) {
        return false;
    }

    outPath->clear();
    if (rawName.empty()) {
        return false;
    }

    std::string candidate = rawName;
    std::replace(candidate.begin(), candidate.end(), '/', '\\');
    if (candidate.find('.') == std::string::npos) {
        candidate += ".bmp";
    }

    const std::string prefixed = std::string("effect\\") + candidate;
    if (g_fileMgr.IsDataExist(prefixed.c_str())) {
        *outPath = prefixed;
        return true;
    }
    if (g_fileMgr.IsDataExist(candidate.c_str())) {
        *outPath = candidate;
        return true;
    }
    *outPath = prefixed;
    return true;
}

} // namespace

void KAC_LAYER::Reset()
{
    for (CTexture* texture : m_tex) {
        if (texture) {
            texture->Unlock();
        }
    }
    cTex = 0;
    iCurAniFrame = 0;
    cAniKey = 0;
    aAniKey = nullptr;
    m_tex.fill(nullptr);
    m_texName.fill(nullptr);
    m_ownedTexNames.clear();
    m_ownedAniKeys.clear();
}

CTexture* KAC_LAYER::GetTexture(int iTex)
{
    if (iTex < 0 || iTex >= cTex) {
        return &CTexMgr::s_dummy_texture;
    }

    CTexture*& texture = m_tex[static_cast<size_t>(iTex)];
    if (texture) {
        return texture;
    }

    const char* rawName = m_texName[static_cast<size_t>(iTex)];
    std::string path;
    if (!ResolveEffectTextureName(rawName ? rawName : "", &path)) {
        return &CTexMgr::s_dummy_texture;
    }

    texture = g_texMgr.GetTexture(path.c_str(), true);
    if (texture) {
        texture->Lock();
        return texture;
    }
    return &CTexMgr::s_dummy_texture;
}

void KANICLIP::Reset()
{
    nFPS = 0;
    cFrame = 0;
    cLayer = 0;
    cEndLayer = 0;
    for (KAC_LAYER& layer : aLayer) {
        layer.Reset();
    }
}

bool CEZeffectRes::LoadFromBuffer(const char* fName, const unsigned char* buffer, int size)
{
    Reset();

    if (!buffer || size < 28) {
        DbgLog("[EZeffect] invalid buffer file='%s' size=%d\n", fName ? fName : "(null)", size);
        return false;
    }

    size_t offset = 0;
    u32 signature = 0;
    u32 version = 0;
    if (!ReadBytes(buffer, size, &offset, &signature, sizeof(signature))
        || !ReadBytes(buffer, size, &offset, &version, sizeof(version))
        || !ReadBytes(buffer, size, &offset, &m_aniClips.cFrame, sizeof(m_aniClips.cFrame))
        || !ReadBytes(buffer, size, &offset, &m_aniClips.nFPS, sizeof(m_aniClips.nFPS))
        || !ReadBytes(buffer, size, &offset, &m_aniClips.cLayer, sizeof(m_aniClips.cLayer))) {
        DbgLog("[EZeffect] truncated header file='%s'\n", fName ? fName : "(null)");
        Reset();
        return false;
    }

    if (version != kAniClipVersion) {
        DbgLog("[EZeffect] unsupported version file='%s' version=%u\n",
            fName ? fName : "(null)",
            static_cast<unsigned>(version));
        Reset();
        return false;
    }

    if (m_aniClips.cLayer < 0 || m_aniClips.cLayer > static_cast<int>(m_aniClips.aLayer.size())) {
        DbgLog("[EZeffect] invalid layer count file='%s' layers=%d\n",
            fName ? fName : "(null)",
            m_aniClips.cLayer);
        Reset();
        return false;
    }

    offset += 16; // Reserved bytes in the original STR format.
    if (offset > static_cast<size_t>(size)) {
        Reset();
        return false;
    }

    for (int layerIndex = 0; layerIndex < m_aniClips.cLayer; ++layerIndex) {
        KAC_LAYER& layer = m_aniClips.aLayer[static_cast<size_t>(layerIndex)];
        if (!ReadBytes(buffer, size, &offset, &layer.cTex, sizeof(layer.cTex))) {
            Reset();
            return false;
        }

        if (layer.cTex < 0 || layer.cTex > static_cast<int>(layer.m_tex.size())) {
            DbgLog("[EZeffect] invalid texture count file='%s' layer=%d texCount=%d\n",
                fName ? fName : "(null)",
                layerIndex,
                layer.cTex);
            Reset();
            return false;
        }

        layer.m_ownedTexNames.reserve(static_cast<size_t>(layer.cTex));
        for (int texIndex = 0; texIndex < layer.cTex; ++texIndex) {
            layer.m_ownedTexNames.push_back(ReadFixedString(buffer, size, &offset, 128));
        }
        for (int texIndex = 0; texIndex < layer.cTex; ++texIndex) {
            layer.m_texName[static_cast<size_t>(texIndex)] = layer.m_ownedTexNames[static_cast<size_t>(texIndex)].c_str();
        }

        if (!ReadBytes(buffer, size, &offset, &layer.cAniKey, sizeof(layer.cAniKey))) {
            Reset();
            return false;
        }

        if (layer.cAniKey < 0) {
            Reset();
            return false;
        }

        layer.m_ownedAniKeys.resize(static_cast<size_t>(layer.cAniKey));
        if (layer.cAniKey > 0) {
            const size_t bytes = sizeof(KAC_KEYFRAME) * static_cast<size_t>(layer.cAniKey);
            if (!ReadBytes(buffer, size, &offset, layer.m_ownedAniKeys.data(), bytes)) {
                Reset();
                return false;
            }
            layer.aAniKey = layer.m_ownedAniKeys.data();
        }
    }

    m_nMaxLayer = m_aniClips.cLayer;
    m_aniClips.cEndLayer = m_aniClips.cLayer;
    return true;
}

CRes* CEZeffectRes::Clone()
{
    return new CEZeffectRes();
}

void CEZeffectRes::Reset()
{
    m_aniClips.Reset();
    m_nMaxLayer = 0;
}
