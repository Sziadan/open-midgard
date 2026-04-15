#include "Renderer.h"
#include "render3d/RenderDevice.h"
#include "render3d/Device.h"
#include "render3d/D3dutil.h"
#include "res/Bitmap.h"
#include "res/Texture.h"
#include "core/Globals.h"
#include "core/File.h"
#include "res/Res.h"
#include "DebugLog.h"
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <cctype>
#include <string>

namespace {

constexpr DWORD kLmFvf = D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_SPECULAR | D3DFVF_TEX2;
constexpr bool kLogTexture = false;
constexpr unsigned int kEffectBlackKeyThreshold = 9u;
constexpr unsigned int kMagentaMaskMinChannel = 0xF8u;
constexpr unsigned int kMagentaMaskMaxGreen = 0x18u;

enum class TextureLoadMode {
    ColorKey,
    BlackKey,
    MagentaMask,
};

bool ShouldLogGroundTextureName(const char* name)
{
    if (!name || !*name) {
        return false;
    }

    std::string lowerName(name);
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });

    return lowerName.find("pron-dun") != std::string::npos
        || lowerName.find("backside.bmp") != std::string::npos;
}

void LogUploadedTerrainSurfaceSamples(CTexture* tex, const char* name)
{
#if !RO_PLATFORM_WINDOWS
    (void)tex;
    (void)name;
    return;
#else
    if (!tex || !tex->m_pddsSurface) {
        return;
    }

    DDSURFACEDESC2 desc{};
    desc.dwSize = sizeof(desc);
    if (FAILED(tex->m_pddsSurface->Lock(nullptr, &desc, DDLOCK_WAIT | DDLOCK_READONLY, nullptr))) {
        if constexpr (kLogTexture) {
            DbgLog("[Texture] readback failed name='%s'\n", name ? name : "(null)");
        }
        return;
    }

    if (desc.ddpfPixelFormat.dwRGBBitCount == 32 && desc.lpSurface && desc.lPitch >= 4) {
        const int width = static_cast<int>(tex->m_updateWidth > 0 ? tex->m_updateWidth : tex->m_w);
        const int height = static_cast<int>(tex->m_updateHeight > 0 ? tex->m_updateHeight : tex->m_h);
        if (width > 0 && height > 0) {
            const unsigned char* base = static_cast<const unsigned char*>(desc.lpSurface);
            const int maxX = width - 1;
            const int maxY = height - 1;
            const int centerX = width / 2;
            const int centerY = height / 2;

            auto readPixel = [&](int x, int y) -> u32 {
                const u32* row = reinterpret_cast<const u32*>(base + static_cast<size_t>(y) * static_cast<size_t>(desc.lPitch));
                return row[x];
            };

            if constexpr (kLogTexture) {
                DbgLog("[Texture] terrain-readback name='%s' size=%dx%d samples=(tl:%08X tr:%08X bl:%08X br:%08X c:%08X)\n",
                    name ? name : "(null)",
                    width,
                    height,
                    readPixel(0, 0),
                    readPixel(maxX, 0),
                    readPixel(0, maxY),
                    readPixel(maxX, maxY),
                    readPixel(centerX, centerY));
            }
        }
    } else {
        if constexpr (kLogTexture) {
            DbgLog("[Texture] terrain-readback unsupported name='%s' bits=%lu pitch=%ld\n",
                name ? name : "(null)",
                desc.ddpfPixelFormat.dwRGBBitCount,
                desc.lPitch);
        }
    }

    tex->m_pddsSurface->Unlock(nullptr);
#endif
}

std::string ToLowerAsciiTexture(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        if (ch >= 0x80u) {
            return static_cast<char>(ch);
        }
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string CollapseUtf8Latin1ToBytes(const std::string& value)
{
    std::string out;
    out.reserve(value.size());
    bool changed = false;
    for (size_t i = 0; i < value.size(); ++i) {
        const unsigned char ch = static_cast<unsigned char>(value[i]);
        if (ch == 0xC2 && i + 1 < value.size()) {
            out.push_back(value[i + 1]);
            ++i;
            changed = true;
            continue;
        }
        if (ch == 0xC3 && i + 1 < value.size()) {
            out.push_back(static_cast<char>(static_cast<unsigned char>(value[i + 1]) + 0x40u));
            ++i;
            changed = true;
            continue;
        }
        out.push_back(static_cast<char>(ch));
    }
    return changed ? out : value;
}

std::string NormalizeTexturePath(std::string value)
{
    std::replace(value.begin(), value.end(), '/', '\\');
    return value;
}

std::string BaseNameOfTexturePath(const std::string& path)
{
    const std::string normalized = NormalizeTexturePath(path);
    const size_t slash = normalized.find_last_of('\\');
    if (slash == std::string::npos) {
        return normalized;
    }
    return normalized.substr(slash + 1);
}

const std::map<std::string, std::string>& GetTextureAliases()
{
    static bool s_loaded = false;
    static std::map<std::string, std::string> s_aliases;
    if (s_loaded) {
        return s_aliases;
    }

    s_loaded = true;

    int size = 0;
    unsigned char* bytes = g_fileMgr.GetData("data\\resnametable.txt", &size);
    if (!bytes || size <= 0) {
        delete[] bytes;
        return s_aliases;
    }

    std::string text(reinterpret_cast<const char*>(bytes), static_cast<size_t>(size));
    delete[] bytes;

    size_t lineStart = 0;
    while (lineStart < text.size()) {
        size_t lineEnd = text.find_first_of("\r\n", lineStart);
        if (lineEnd == std::string::npos) {
            lineEnd = text.size();
        }

        std::string line = text.substr(lineStart, lineEnd - lineStart);
        lineStart = text.find_first_not_of("\r\n", lineEnd);
        if (lineStart == std::string::npos) {
            lineStart = text.size();
        }

        if (line.empty() || (line.size() >= 2 && line[0] == '/' && line[1] == '/')) {
            continue;
        }

        const size_t firstHash = line.find('#');
        const size_t secondHash = firstHash == std::string::npos ? std::string::npos : line.find('#', firstHash + 1);
        if (firstHash == std::string::npos || secondHash == std::string::npos || firstHash == 0 || secondHash <= firstHash + 1) {
            continue;
        }

        std::string key = NormalizeTexturePath(line.substr(0, firstHash));
        std::string value = NormalizeTexturePath(line.substr(firstHash + 1, secondHash - firstHash - 1));
        if (!key.empty() && !value.empty()) {
            s_aliases[ToLowerAsciiTexture(key)] = value;
        }
    }

    return s_aliases;
}

std::string ResolveTextureAlias(const std::string& candidate)
{
    if (candidate.empty()) {
        return std::string();
    }

    const auto& aliases = GetTextureAliases();
    const std::string normalized = ToLowerAsciiTexture(NormalizeTexturePath(candidate));
    const auto it = aliases.find(normalized);
    if (it != aliases.end()) {
        return it->second;
    }

    const std::string candidateBase = ToLowerAsciiTexture(BaseNameOfTexturePath(normalized));
    for (const auto& entry : aliases) {
        if (ToLowerAsciiTexture(BaseNameOfTexturePath(entry.first)) == candidateBase) {
            return entry.second;
        }
    }

    return std::string();
}

std::string ResolveExistingTexturePath(const std::string& candidate, const std::vector<std::string>& directPrefixes)
{
    if (candidate.empty()) {
        return std::string();
    }

    const std::string normalized = NormalizeTexturePath(candidate);
    if (g_fileMgr.IsDataExist(normalized.c_str())) {
        return normalized;
    }

    for (const std::string& prefix : directPrefixes) {
        const std::string prefixed = NormalizeTexturePath(prefix + normalized);
        if (g_fileMgr.IsDataExist(prefixed.c_str())) {
            return prefixed;
        }
    }

    return std::string();
}

const std::vector<std::string>& GetTextureDataNamesByExtension(const char* ext)
{
    static std::map<std::string, std::vector<std::string>> s_byExt;
    const std::string key = ToLowerAsciiTexture(ext ? ext : "");
    auto it = s_byExt.find(key);
    if (it != s_byExt.end()) {
        return it->second;
    }

    std::vector<std::string> names;
    g_fileMgr.CollectDataNamesByExtension(key.c_str(), names);
    auto inserted = s_byExt.emplace(key, std::move(names));
    return inserted.first->second;
}

std::string ResolveTextureDataPath(const std::string& fileName, const char* ext, const std::vector<std::string>& directPrefixes)
{
    if (fileName.empty()) {
        return std::string();
    }

    const std::string normalizedName = NormalizeTexturePath(fileName);
    for (const std::string& prefix : directPrefixes) {
        const std::string candidate = NormalizeTexturePath(prefix + normalizedName);
        if (g_fileMgr.IsDataExist(candidate.c_str())) {
            return candidate;
        }

        const std::string alias = ResolveTextureAlias(candidate);
        if (!alias.empty()) {
            const std::string resolvedAlias = ResolveExistingTexturePath(alias, directPrefixes);
            if (!resolvedAlias.empty()) {
                return resolvedAlias;
            }
        }
    }

    const std::string directAlias = ResolveTextureAlias(normalizedName);
    if (!directAlias.empty()) {
        const std::string resolvedDirectAlias = ResolveExistingTexturePath(directAlias, directPrefixes);
        if (!resolvedDirectAlias.empty()) {
            return resolvedDirectAlias;
        }
    }

    const std::string wantedBase = ToLowerAsciiTexture(BaseNameOfTexturePath(normalizedName));
    const std::string wantedStem = wantedBase.rfind('.') != std::string::npos
        ? wantedBase.substr(0, wantedBase.rfind('.'))
        : wantedBase;
    const auto& knownNames = GetTextureDataNamesByExtension(ext);
    for (const std::string& known : knownNames) {
        if (ToLowerAsciiTexture(BaseNameOfTexturePath(known)) == wantedBase) {
            return known;
        }
    }

    for (const std::string& known : knownNames) {
        const std::string knownLower = ToLowerAsciiTexture(known);
        if (knownLower.find(wantedStem) != std::string::npos) {
            return known;
        }
    }

    for (const std::string& prefix : directPrefixes) {
        const std::string alias = ResolveTextureAlias(prefix + normalizedName);
        if (!alias.empty()) {
            const std::string aliasBase = ToLowerAsciiTexture(BaseNameOfTexturePath(alias));
            if (aliasBase == wantedBase || aliasBase.find(wantedStem) != std::string::npos) {
                const std::string resolvedAlias = ResolveExistingTexturePath(alias, directPrefixes);
                if (!resolvedAlias.empty()) {
                    return resolvedAlias;
                }
            }
        }
    }

    return std::string();
}

std::string ResolveTexturePath(const char* name)
{
    if (!name || !*name) {
        return std::string();
    }

    const std::string normalizedName = NormalizeTexturePath(CollapseUtf8Latin1ToBytes(name));
    const std::vector<std::string> prefixes = {
        "",
        "data\\",
        "texture\\",
        "data\\texture\\"
    };

    const char* dot = std::strrchr(normalizedName.c_str(), '.');
    if (dot) {
        const std::string resolvedWithExistingExt = ResolveTextureDataPath(normalizedName, dot, prefixes);
        if (!resolvedWithExistingExt.empty()) {
            return resolvedWithExistingExt;
        }
    }

    static const char* const kTextureExts[] = { ".bmp", ".jpg", ".jpeg", ".tga", ".png" };
    for (const char* ext : kTextureExts) {
        const std::string candidate = dot ? normalizedName : normalizedName + ext;
        const std::string resolved = ResolveTextureDataPath(candidate, ext, prefixes);
        if (!resolved.empty()) {
            return resolved;
        }
    }

    return std::string();
}

bool IsMaskedMagentaPixel(unsigned int pixel)
{
    if ((pixel & 0x00FFFFFFu) == 0x00FF00FFu) {
        return true;
    }

    const unsigned int red = (pixel >> 16) & 0xFFu;
    const unsigned int green = (pixel >> 8) & 0xFFu;
    const unsigned int blue = pixel & 0xFFu;
    return red >= kMagentaMaskMinChannel
        && blue >= kMagentaMaskMinChannel
        && green <= kMagentaMaskMaxGreen;
}

vector3d SubtractVec3(const vector3d& a, const vector3d& b)
{
    return vector3d{ a.x - b.x, a.y - b.y, a.z - b.z };
}

float DotVec3(const vector3d& a, const vector3d& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

vector3d CrossVec3(const vector3d& a, const vector3d& b)
{
    return vector3d{
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

vector3d NormalizeVec3(const vector3d& value)
{
    const float lengthSq = DotVec3(value, value);
    if (lengthSq <= 1.0e-12f) {
        return vector3d{ 0.0f, 1.0f, 0.0f };
    }

    const float invLength = 1.0f / std::sqrt(lengthSq);
    return vector3d{ value.x * invLength, value.y * invLength, value.z * invLength };
}

DWORD PackRenderColor(const vector3d& color)
{
    const auto toByte = [](float value) -> DWORD {
        const float clamped = (std::max)(0.0f, (std::min)(1.0f, value));
        return static_cast<DWORD>(clamped * 255.0f + 0.5f);
    };

    const DWORD r = toByte(color.x);
    const DWORD g = toByte(color.y);
    const DWORD b = toByte(color.z);
    return 0xFF000000u | (r << 16) | (g << 8) | b;
}

void BuildLookAtMatrix(const vector3d& eye, const vector3d& at, const vector3d& up, D3DMATRIX* outMatrix)
{
    if (!outMatrix) {
        return;
    }

    const vector3d zaxis = NormalizeVec3(SubtractVec3(at, eye));
    const vector3d xaxis = NormalizeVec3(CrossVec3(up, zaxis));
    const vector3d yaxis = CrossVec3(zaxis, xaxis);

    std::memset(outMatrix, 0, sizeof(D3DMATRIX));
    outMatrix->_11 = xaxis.x;
    outMatrix->_12 = yaxis.x;
    outMatrix->_13 = zaxis.x;
    outMatrix->_21 = xaxis.y;
    outMatrix->_22 = yaxis.y;
    outMatrix->_23 = zaxis.y;
    outMatrix->_31 = xaxis.z;
    outMatrix->_32 = yaxis.z;
    outMatrix->_33 = zaxis.z;
    outMatrix->_41 = -DotVec3(xaxis, eye);
    outMatrix->_42 = -DotVec3(yaxis, eye);
    outMatrix->_43 = -DotVec3(zaxis, eye);
    outMatrix->_44 = 1.0f;
}

} // namespace

CRenderer g_renderer;
CTexMgr g_texMgr;

CacheSurface::~CacheSurface() = default;

// --- CTexMgr Implementation ---

CTexMgr::CTexMgr() {
}

CTexMgr::~CTexMgr() {
    DestroyAllTexture();
}

void CTexMgr::DestroyAllTexture() {
    for (auto& pair : m_texTable) {
        if (pair.second) {
            delete pair.second;
        }
    }
    m_texTable.clear();
    m_missingTexTable.clear();
}

CTexture* LoadManagedTexture(CTexMgr& texMgr, const char* name, TextureLoadMode mode) {
    const std::string baseName = name ? name : "";
    const char* cachePrefix = mode == TextureLoadMode::BlackKey
        ? "@bk:"
        : (mode == TextureLoadMode::MagentaMask ? "@mk:" : "@ck:");
    const char* modeName = mode == TextureLoadMode::BlackKey
        ? "blackkey"
        : (mode == TextureLoadMode::MagentaMask ? "masked" : "colorkey");
    const std::string cacheKey = std::string(cachePrefix) + baseName;
    auto it = texMgr.m_texTable.find(cacheKey.c_str());
    if (it != texMgr.m_texTable.end()) {
        it->second->m_timeStamp = GetTickCount();
        return it->second;
    }
    if (texMgr.m_missingTexTable.find(cacheKey) != texMgr.m_missingTexTable.end()) {
        return &CTexMgr::s_dummy_texture;
    }

    const std::string texturePath = ResolveTexturePath(name);
    if (texturePath.empty()) {
        static std::map<std::string, bool> loggedMissingTextures;
        const std::string logKey = std::string(modeName) + ":" + baseName;
        if (loggedMissingTextures.size() < 24 && loggedMissingTextures.emplace(logKey, true).second) {
            DbgLog("[Texture] unresolved name='%s' mode=%s\n", name ? name : "(null)", modeName);
        }
        texMgr.m_missingTexTable.insert(cacheKey);
        return &CTexMgr::s_dummy_texture;
    }

    CBitmapRes* bitmap = g_resMgr.GetAs<CBitmapRes>(texturePath.c_str());
    if (!bitmap || !bitmap->m_data || bitmap->m_width <= 0 || bitmap->m_height <= 0) {
        static int failedTextureLoadLogCount = 0;
        if (failedTextureLoadLogCount < 16) {
            ++failedTextureLoadLogCount;
            if constexpr (kLogTexture) {
                DbgLog("[Texture] load failed name='%s' resolved='%s' mode=%s\n",
                    name ? name : "(null)",
                    texturePath.c_str(),
                    modeName);
            }
        }
        texMgr.m_missingTexTable.insert(cacheKey);
        return &CTexMgr::s_dummy_texture;
    }

    const unsigned int* textureData = reinterpret_cast<const unsigned int*>(bitmap->m_data);
    std::vector<unsigned int> transformedPixels;
    bool skipColorKey = false;
    if (mode == TextureLoadMode::BlackKey) {
        const size_t pixelCount = static_cast<size_t>(bitmap->m_width) * static_cast<size_t>(bitmap->m_height);
        transformedPixels.assign(textureData, textureData + pixelCount);
        for (unsigned int& pixel : transformedPixels) {
            if ((pixel & 0x00FFFFFFu) == 0x00FF00FFu) {
                pixel = 0x00000000u;
                continue;
            }
            const unsigned int red = (pixel >> 16) & 0xFFu;
            const unsigned int green = (pixel >> 8) & 0xFFu;
            const unsigned int blue = pixel & 0xFFu;
            if (bitmap->m_isAlpha == 0) {
                const unsigned int alpha = (std::max)(red, (std::max)(green, blue));
                if (alpha <= kEffectBlackKeyThreshold) {
                    pixel = 0x00000000u;
                } else {
                    pixel = (alpha << 24) | (pixel & 0x00FFFFFFu);
                }
            } else if ((pixel & 0x00FFFFFFu) == 0x00000000u) {
                pixel = 0x00000000u;
            }
        }
        textureData = transformedPixels.data();
        skipColorKey = true;
    } else if (mode == TextureLoadMode::MagentaMask) {
        const size_t pixelCount = static_cast<size_t>(bitmap->m_width) * static_cast<size_t>(bitmap->m_height);
        transformedPixels.assign(textureData, textureData + pixelCount);
        for (unsigned int& pixel : transformedPixels) {
            if (IsMaskedMagentaPixel(pixel)) {
                pixel = 0x00000000u;
            }
        }
        textureData = transformedPixels.data();
        skipColorKey = true;
    }

    CTexture* tex = new CTexture();
    if (tex) {
        std::strncpy(tex->m_texName, cacheKey.c_str(), sizeof(tex->m_texName) - 1);
        tex->m_texName[sizeof(tex->m_texName) - 1] = '\0';
        if (!tex->Create(static_cast<unsigned int>(bitmap->m_width), static_cast<unsigned int>(bitmap->m_height), PF_A8R8G8B8)) {
            delete tex;
            tex = nullptr;
        } else {
            tex->Update(
                0,
                0,
                bitmap->m_width,
                bitmap->m_height,
                const_cast<unsigned int*>(reinterpret_cast<const unsigned int*>(textureData)),
                skipColorKey,
                bitmap->m_width * static_cast<int>(sizeof(unsigned int)));
        }
    }
    if (!tex) {
        static int failedTextureCreateLogCount = 0;
        if (failedTextureCreateLogCount < 16) {
            ++failedTextureCreateLogCount;
            if constexpr (kLogTexture) {
                DbgLog("[Texture] create failed name='%s' resolved='%s' size=%dx%d mode=%s\n",
                    name ? name : "(null)",
                    texturePath.c_str(),
                    bitmap->m_width,
                    bitmap->m_height,
                    modeName);
            }
        }
        texMgr.m_missingTexTable.insert(cacheKey);
        return &CTexMgr::s_dummy_texture;
    }

    static int loadedTextureLogCount = 0;
    if (loadedTextureLogCount < 16) {
        ++loadedTextureLogCount;
        if constexpr (kLogTexture) {
            DbgLog("[Texture] loaded name='%s' resolved='%s' size=%dx%d mode=%s surface=%p\n",
                name ? name : "(null)",
                texturePath.c_str(),
                bitmap->m_width,
                bitmap->m_height,
                modeName,
                static_cast<void*>(tex->m_pddsSurface));
        }
    }

    if (ShouldLogGroundTextureName(name)) {
        LogUploadedTerrainSurfaceSamples(tex, name);
    }

    tex->m_timeStamp = GetTickCount();
    texMgr.m_texTable[tex->m_texName] = tex;
    return tex;
}

CTexture* CTexMgr::GetTexture(const char* name, bool b) {
    return LoadManagedTexture(*this, name, b ? TextureLoadMode::BlackKey : TextureLoadMode::ColorKey);
}

CTexture* CTexMgr::GetMaskedTexture(const char* name) {
    return LoadManagedTexture(*this, name, TextureLoadMode::MagentaMask);
}

CTexture* CTexMgr::CreateTexture(int w, int h, unsigned int* data, PixelFormat format, bool b) {
    CTexture* tex = new CTexture();
    if (!tex) {
        return nullptr;
    }
    if (!tex->Create(static_cast<unsigned int>(w), static_cast<unsigned int>(h), format)) {
        delete tex;
        return nullptr;
    }
    tex->Update(0, 0, w, h, reinterpret_cast<unsigned int*>(data), b,
        w * static_cast<int>(sizeof(unsigned int)));
    return tex;
}

CTexture* CTexMgr::CreateTexture(unsigned long w, unsigned long h, PixelFormat format, IDirectDrawSurface7* pSurface) {
    CTexture* tex = new CTexture();
    if (!tex) {
        return nullptr;
    }
    tex->m_w = w;
    tex->m_h = h;
    tex->m_pf = format;
    tex->m_pddsSurface = pSurface;
    tex->m_backendTextureObject = nullptr;
    tex->m_backendTextureView = nullptr;
    tex->m_backendTextureUpload = nullptr;
    if (pSurface) {
#if RO_PLATFORM_WINDOWS
        pSurface->AddRef();
#endif
    }
    return tex;
}

CTexture CTexMgr::s_dummy_texture;

// --- CRenderer Implementation ---

CRenderer::CRenderer() {
    m_renderDevice = nullptr;
    m_oldTexture = nullptr;
    m_oldLmapTexture = nullptr;
    m_curFrame = 0;
    m_fpsFrameCount = 0;
    m_fpsStartTick = 0;
    m_isVertexFog = false;
    m_isFoggy = false;
    m_fogChanged = false;
    m_nClearColor = 0;
    m_lpSurfacePtr = nullptr;
    m_lPitch = 0;
    m_bRGBBitCount = 0;
    m_eyePos = vector3d{ 0.0f, 0.0f, 0.0f };
    m_eyeAt = vector3d{ 0.0f, 0.0f, 1.0f };
    m_eyeUp = vector3d{ 0.0f, 1.0f, 0.0f };
    m_eyeRight = vector3d{ 1.0f, 0.0f, 0.0f };
    m_eyeForward = vector3d{ 0.0f, 0.0f, 1.0f };
}

CRenderer::~CRenderer() {
}

void CRenderer::Init() {
    m_renderDevice = &GetRenderDevice();
    m_rpNullFaceListIter = m_rpNullFaceList.begin();
    m_rpQuadFaceListIter = m_rpQuadFaceList.begin();
    m_rpLmQuadFaceListIter = m_rpLmQuadFaceList.begin();
}

void CRenderer::SetSize(int cx, int cy) {
    m_width = cx;
    m_height = cy;
    m_halfWidth = cx / 2;
    m_halfHeight = cy / 2;
    m_aspectRatio = (float)cy / (float)cx;
    
    float fFOV = 15.0f * 0.017453292f; // 15 degrees in radians
    m_hpc = (float)m_halfWidth / tanf(fFOV * 0.5f);
    m_vpc = (float)m_halfHeight / tanf(fFOV * 0.5f);
    
    // Adjusted based on aspect ratio as seen in ref
    m_hpc = m_aspectRatio * m_hpc;
    
    m_xoffset = m_halfWidth;
    m_yoffset = m_halfHeight;
    m_hratio = 1.0f;
    m_vratio = 1.0f;
    
    m_screenXFactor = (float)cx * 0.0015625f; // cx / 640
    m_screenYFactor = (float)cy * 0.0020833334f; // cy / 480

    if (m_renderDevice) {
        D3DMATRIX projection{};
        if (D3DUtil_SetProjectionMatrix(&projection, 15.0f * 0.017453292f, m_aspectRatio, 10.0f, 5000.0f) == 0) {
            m_renderDevice->SetTransform(D3DTRANSFORMSTATE_PROJECTION, &projection);
        }
    }
}

void CRenderer::SetPixelFormat(PixelFormat pf) {
    m_pf = pf;
}

void CRenderer::Clear(int color) {
    if (color != 0) {
        GetRenderDevice().ClearColor(0xFF000000 | color);
    } else {
        GetRenderDevice().ClearDepth();
    }
}

void CRenderer::ClearBackground() {
    GetRenderDevice().ClearColor(m_nClearColor);
}

bool CRenderer::DrawScene() {
    if (!m_renderDevice || !m_renderDevice->BeginScene())
        return false;

    FlushRenderList();

    m_renderDevice->EndScene();
    return true;
}

void CRenderer::Flip(bool vertSync) {
    m_curFrame++;
    m_fpsFrameCount++;
    
    GetRenderDevice().Present(vertSync);
}

void CRenderer::AddRP(RPFace* face, int renderFlag) {
    if (renderFlag & 1) { // Alpha
        float sortKey = face->alphaSortKey;
        if (sortKey <= 0.0f && face->verts && face->numVerts > 0) {
            sortKey = face->verts[0].oow;
            for (int index = 1; index < face->numVerts; ++index) {
                sortKey = (std::min)(sortKey, face->verts[index].oow);
            }
        }

        if ((renderFlag & 8) && (renderFlag & 4)) {
            m_rpAlphaOPNoDepthList.push_back({sortKey, face});
        } else if (renderFlag & 4) {
            m_rpAlphaOPList.push_back(face);
        } else if (renderFlag & 2) { // Emissive
            m_rpEmissiveList.push_back({sortKey, face});
        } else if (renderFlag & 8) {
            m_rpAlphaNoDepthList.push_back({sortKey, face});
        } else {
            m_rpAlphaList.push_back({sortKey, face});
        }
    } else {
        if (renderFlag & 0x800) {
            m_rpLMGroundList.push_back(face);
        } else if (renderFlag & 0x1000) {
            m_rpLMLightList.push_back(face);
        } else {
            m_rpFaceList.push_back(face);
        }
    }
}

void CRenderer::AddLmRP(RPLmFace* face, int renderFlag) {
    m_rpLmList.push_back(face);
}

void CRenderer::AddRawRP(RPRaw* face, int renderFlag) {
    if (renderFlag & 1) {
        m_rpRawAlphaList.push_back(face);
    } else {
        m_rpRawList.push_back(face);
    }
}

void CRenderer::SetLmapTexture(CTexture* tex) {
    if (m_oldLmapTexture != tex) {
        if (m_renderDevice) {
            m_renderDevice->BindTexture(1, tex);
        }
        m_oldLmapTexture = tex;
    }
}

void CRenderer::SetMultiTextureMode(int nMode) {
    if (!m_renderDevice) {
        return;
    }

    switch (nMode) {
    case 0:
        m_renderDevice->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
        m_renderDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        m_renderDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
        m_renderDevice->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
        m_renderDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
        m_renderDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
        m_renderDevice->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
        m_renderDevice->SetTextureStageState(0, D3DTSS_MINFILTER, D3DTFN_LINEAR);
        m_renderDevice->SetTextureStageState(0, D3DTSS_MAGFILTER, D3DTFG_LINEAR);
        m_renderDevice->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
        m_renderDevice->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
        m_renderDevice->BindTexture(1, nullptr);
        break;
    case 1:
    case 2:
        m_renderDevice->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
        m_renderDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        m_renderDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
        m_renderDevice->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
        m_renderDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
        m_renderDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
        m_renderDevice->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
        m_renderDevice->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
        m_renderDevice->BindTexture(1, nullptr);
        break;
    case 3:
        m_renderDevice->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
        m_renderDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        m_renderDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
        m_renderDevice->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
        m_renderDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
        m_renderDevice->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
        m_renderDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
        m_renderDevice->SetTextureStageState(0, D3DTSS_MINFILTER, D3DTFN_POINT);
        m_renderDevice->SetTextureStageState(0, D3DTSS_MAGFILTER, D3DTFG_POINT);
        m_renderDevice->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
        m_renderDevice->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
        m_renderDevice->BindTexture(1, nullptr);
        break;
    default:
        m_renderDevice->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
        m_renderDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        m_renderDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
        m_renderDevice->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
        m_renderDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
        m_renderDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
        m_renderDevice->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
        m_renderDevice->SetTextureStageState(0, D3DTSS_MINFILTER, D3DTFN_LINEAR);
        m_renderDevice->SetTextureStageState(0, D3DTSS_MAGFILTER, D3DTFG_LINEAR);
        m_renderDevice->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
        m_renderDevice->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
        m_renderDevice->BindTexture(1, nullptr);
        break;
    }
}

void CRenderer::FlushRenderList() {
    if (!m_renderDevice) {
        return;
    }

    // 1. Sort lists
    std::sort(m_rpFaceList.begin(), m_rpFaceList.end(), [](RPFace* a, RPFace* b) {
        return a->tex < b->tex;
    });
    std::sort(m_rpLMGroundList.begin(), m_rpLMGroundList.end(), [](RPFace* a, RPFace* b) {
        return a->tex < b->tex;
    });
    
    // 2. Opaque faces
    SetMultiTextureMode(0);
    m_renderDevice->SetRenderState(D3DRENDERSTATE_ALPHAREF, 207);
    m_renderDevice->SetRenderState(D3DRENDERSTATE_ALPHATESTENABLE, TRUE);
    m_renderDevice->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, FALSE);
    m_renderDevice->SetRenderState(D3DRENDERSTATE_ZENABLE, D3DZB_TRUE);
    m_renderDevice->SetRenderState(D3DRENDERSTATE_ZWRITEENABLE, TRUE);
    m_renderDevice->SetRenderState(D3DRENDERSTATE_CULLMODE, D3DCULL_CW);
    m_oldTexture = nullptr;
    m_oldLmapTexture = nullptr;
    int colorKeyEnabled = TRUE;
    int activeMtPreset = 0;
    D3DCULL activeCullMode = D3DCULL_CW;
    for (auto face : m_rpFaceList) {
        if (face->mtPreset != activeMtPreset) {
            SetMultiTextureMode(face->mtPreset);
            activeMtPreset = face->mtPreset;
        }

        const D3DCULL wantCullMode = face->cullMode == 0 ? D3DCULL_CW : face->cullMode;
        if (wantCullMode != activeCullMode) {
            m_renderDevice->SetRenderState(D3DRENDERSTATE_CULLMODE, wantCullMode);
            activeCullMode = wantCullMode;
        }

        const int wantColorKey = (face->mtPreset == 1 || face->mtPreset == 2) ? FALSE : TRUE;
        if (wantColorKey != colorKeyEnabled) {
            m_renderDevice->SetRenderState(D3DRENDERSTATE_COLORKEYENABLE, wantColorKey);
            colorKeyEnabled = wantColorKey;
        }
        SetTexture(face->tex);
        RPFace::DrawPri(face, *m_renderDevice);
    }
    SetMultiTextureMode(0);
    if (!colorKeyEnabled) {
        m_renderDevice->SetRenderState(D3DRENDERSTATE_COLORKEYENABLE, TRUE);
    }
    if (activeCullMode != D3DCULL_CW) {
        m_renderDevice->SetRenderState(D3DRENDERSTATE_CULLMODE, D3DCULL_CW);
    }
    
    // 3. Raw primitives
    for (auto face : m_rpRawList) {
        SetTexture(face->tex);
        RPRaw::DrawPri(face, *m_renderDevice);
    }

    // 4. Ground list (reference client routes terrain here)
    activeMtPreset = 0;
    SetMultiTextureMode(0);
    m_renderDevice->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, FALSE);
    m_renderDevice->SetRenderState(D3DRENDERSTATE_SRCBLEND, D3DBLEND_ONE);
    m_renderDevice->SetRenderState(D3DRENDERSTATE_DESTBLEND, D3DBLEND_ZERO);
    m_renderDevice->SetRenderState(D3DRENDERSTATE_ALPHATESTENABLE, FALSE);
    m_renderDevice->SetRenderState(D3DRENDERSTATE_COLORKEYENABLE, FALSE);
    colorKeyEnabled = FALSE;
    static bool loggedGroundDraw = false;
    for (auto face : m_rpLMGroundList) {
        if (face->mtPreset != activeMtPreset) {
            SetMultiTextureMode(face->mtPreset);
            activeMtPreset = face->mtPreset;
        }
        SetTexture(face->tex);
        if (!loggedGroundDraw) {
            loggedGroundDraw = true;
            DbgLog("[Renderer] ground-draw tex=%p pdds=%p name='%s' verts=%d prim=%d mtPreset=%d\n",
                static_cast<void*>(face->tex),
                face->tex ? static_cast<void*>(face->tex->m_pddsSurface) : nullptr,
                face->tex ? face->tex->m_texName : "(null)",
                face->numVerts,
                static_cast<int>(face->primType),
                face->mtPreset);
        }
        RPFace::DrawPri(face, *m_renderDevice);
    }
    SetMultiTextureMode(0);
    m_renderDevice->SetRenderState(D3DRENDERSTATE_COLORKEYENABLE, TRUE);
    m_renderDevice->SetRenderState(D3DRENDERSTATE_ALPHATESTENABLE, TRUE);
    colorKeyEnabled = TRUE;

    if (!m_rpLmList.empty()) {
        std::sort(m_rpLmList.begin(), m_rpLmList.end(), [](RPLmFace* a, RPLmFace* b) {
            if (a->tex != b->tex) {
                return a->tex < b->tex;
            }
            return a->tex2 < b->tex2;
        });

        m_renderDevice->SetRenderState(D3DRENDERSTATE_COLORKEYENABLE, FALSE);
        m_renderDevice->SetRenderState(D3DRENDERSTATE_ALPHATESTENABLE, FALSE);
        m_renderDevice->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
        m_renderDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        m_renderDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
        m_renderDevice->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
        m_renderDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
        m_renderDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
        m_renderDevice->SetTextureStageState(1, D3DTSS_TEXCOORDINDEX, 1);
        m_renderDevice->SetTextureStageState(1, D3DTSS_COLORARG1, D3DTA_TEXTURE | D3DTA_ALPHAREPLICATE);
        m_renderDevice->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_MODULATE);
        m_renderDevice->SetTextureStageState(1, D3DTSS_COLORARG2, D3DTA_CURRENT);
        m_renderDevice->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
        m_oldTexture = nullptr;
        m_oldLmapTexture = nullptr;
        for (RPLmFace* face : m_rpLmList) {
            SetTexture(face->tex);
            SetLmapTexture(face->tex2);
            RPLmFace::DrawPri(face, *m_renderDevice);
        }
        SetMultiTextureMode(0);
        m_renderDevice->SetRenderState(D3DRENDERSTATE_COLORKEYENABLE, TRUE);
        m_renderDevice->SetRenderState(D3DRENDERSTATE_ALPHATESTENABLE, TRUE);
        m_oldLmapTexture = nullptr;
    }
    
    // 5. Alpha blended (sorted by depth)
    m_renderDevice->SetRenderState(D3DRENDERSTATE_CULLMODE, D3DCULL_NONE);
    m_renderDevice->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, TRUE);
    m_renderDevice->SetRenderState(D3DRENDERSTATE_SRCBLEND, D3DBLEND_SRCALPHA);
    m_renderDevice->SetRenderState(D3DRENDERSTATE_DESTBLEND, D3DBLEND_INVSRCALPHA);
    m_renderDevice->SetRenderState(D3DRENDERSTATE_ALPHATESTENABLE, FALSE);
    m_renderDevice->SetRenderState(D3DRENDERSTATE_COLORKEYENABLE, FALSE);
    m_renderDevice->SetRenderState(D3DRENDERSTATE_ZWRITEENABLE, FALSE);
    
    struct BlendedFaceEntry {
        float sortKey;
        RPFace* face;
        D3DBLEND srcBlend;
        D3DBLEND destBlend;
    };

    std::vector<BlendedFaceEntry> blendedFaces;
    blendedFaces.reserve(m_rpAlphaList.size() + m_rpEmissiveList.size());
    for (auto& pair : m_rpAlphaList) {
        blendedFaces.push_back({ pair.first, pair.second, D3DBLEND_SRCALPHA, D3DBLEND_INVSRCALPHA });
    }
    for (auto& pair : m_rpEmissiveList) {
        blendedFaces.push_back({ pair.first, pair.second, D3DBLEND_ONE, D3DBLEND_ONE });
    }
    std::stable_sort(blendedFaces.begin(), blendedFaces.end(), [](const BlendedFaceEntry& a, const BlendedFaceEntry& b) {
        return a.sortKey < b.sortKey; // Sort back-to-front using oow: smaller is farther.
    });

    D3DBLEND activeSrcBlend = D3DBLEND_SRCALPHA;
    D3DBLEND activeDestBlend = D3DBLEND_INVSRCALPHA;
    for (const BlendedFaceEntry& entry : blendedFaces) {
        if (entry.srcBlend != activeSrcBlend) {
            m_renderDevice->SetRenderState(D3DRENDERSTATE_SRCBLEND, entry.srcBlend);
            activeSrcBlend = entry.srcBlend;
        }
        if (entry.destBlend != activeDestBlend) {
            m_renderDevice->SetRenderState(D3DRENDERSTATE_DESTBLEND, entry.destBlend);
            activeDestBlend = entry.destBlend;
        }
        SetTexture(entry.face->tex);
        RPFace::DrawPri(entry.face, *m_renderDevice);
    }

    if (activeSrcBlend != D3DBLEND_SRCALPHA) {
        m_renderDevice->SetRenderState(D3DRENDERSTATE_SRCBLEND, D3DBLEND_SRCALPHA);
    }
    if (activeDestBlend != D3DBLEND_INVSRCALPHA) {
        m_renderDevice->SetRenderState(D3DRENDERSTATE_DESTBLEND, D3DBLEND_INVSRCALPHA);
    }

    auto resolveAlphaSortKey = [](RPFace* face) {
        if (!face) {
            return 0.0f;
        }

        float sortKey = face->alphaSortKey;
        if (sortKey <= 0.0f && face->verts && face->numVerts > 0) {
            sortKey = face->verts[0].oow;
            for (int index = 1; index < face->numVerts; ++index) {
                sortKey = (std::min)(sortKey, face->verts[index].oow);
            }
        }
        return sortKey;
    };

    // STR AlphaOP layers still need colorkeying here, but alpha test can
    // reject the whole layer for textures that do not carry usable alpha.
    m_renderDevice->SetRenderState(D3DRENDERSTATE_COLORKEYENABLE, TRUE);

    std::vector<std::pair<float, RPFace*>> alphaOpFaces;
    alphaOpFaces.reserve(m_rpAlphaOPList.size());
    for (RPFace* face : m_rpAlphaOPList) {
        alphaOpFaces.push_back({ resolveAlphaSortKey(face), face });
    }
    std::stable_sort(alphaOpFaces.begin(), alphaOpFaces.end(), [](const std::pair<float, RPFace*>& a, const std::pair<float, RPFace*>& b) {
        return a.first < b.first;
    });

    int activeAlphaOpMtPreset = 0;
    for (const auto& pair : alphaOpFaces) {
        RPFace* face = pair.second;
        if (!face) {
            continue;
        }
        if (face->mtPreset != activeAlphaOpMtPreset) {
            SetMultiTextureMode(face->mtPreset);
            activeAlphaOpMtPreset = face->mtPreset;
        }
        SetTexture(face->tex);
        if (face->srcAlphaMode != activeSrcBlend) {
            m_renderDevice->SetRenderState(D3DRENDERSTATE_SRCBLEND, face->srcAlphaMode);
            activeSrcBlend = face->srcAlphaMode;
        }
        if (face->destAlphaMode != activeDestBlend) {
            m_renderDevice->SetRenderState(D3DRENDERSTATE_DESTBLEND, face->destAlphaMode);
            activeDestBlend = face->destAlphaMode;
        }
        RPFace::DrawPri(face, *m_renderDevice);
    }
    SetMultiTextureMode(0);

    // Reference parity: no-depth alpha draws run with the standard alpha blend
    // state, not whatever blend the last AlphaOP face happened to use.
    m_renderDevice->SetRenderState(D3DRENDERSTATE_SRCBLEND, D3DBLEND_SRCALPHA);
    m_renderDevice->SetRenderState(D3DRENDERSTATE_DESTBLEND, D3DBLEND_INVSRCALPHA);

    std::stable_sort(m_rpAlphaNoDepthList.begin(), m_rpAlphaNoDepthList.end(), [](const std::pair<float, RPFace*>& a, const std::pair<float, RPFace*>& b) {
        return a.first < b.first;
    });

    if (!m_rpAlphaNoDepthList.empty() && !m_renderDevice->PrepareOverlayPass()) {
        m_rpFaceList.clear();
        m_rpLMGroundList.clear();
        m_rpLMLightList.clear();
        m_rpRawList.clear();
        m_rpAlphaList.clear();
        m_rpAlphaNoDepthList.clear();
        m_rpEmissiveList.clear();
        m_rpEmissiveNoDepthList.clear();
        m_rpRawAlphaList.clear();
        m_rpLmList.clear();
        m_rpAlphaOPList.clear();
        m_rpAlphaOPNoDepthList.clear();
        m_vertBuffer.clear();
        m_rpNullFaceListIter = m_rpNullFaceList.begin();
        m_rpQuadFaceListIter = m_rpQuadFaceList.begin();
        m_rpLmQuadFaceListIter = m_rpLmQuadFaceList.begin();
        return;
    }

    m_renderDevice->SetRenderState(D3DRENDERSTATE_ZENABLE, D3DZB_FALSE);
    if (!m_rpAlphaOPNoDepthList.empty()) {
        m_renderDevice->SetRenderState(D3DRENDERSTATE_COLORKEYENABLE, TRUE);
        std::stable_sort(m_rpAlphaOPNoDepthList.begin(), m_rpAlphaOPNoDepthList.end(), [](const std::pair<float, RPFace*>& a, const std::pair<float, RPFace*>& b) {
            return a.first < b.first;
        });

        D3DBLEND activeSrcBlend = D3DBLEND_SRCALPHA;
        D3DBLEND activeDestBlend = D3DBLEND_INVSRCALPHA;
        int activeAlphaOpMtPreset = 0;
        for (auto& pair : m_rpAlphaOPNoDepthList) {
            RPFace* face = pair.second;
            if (!face) {
                continue;
            }

            if (face->mtPreset != activeAlphaOpMtPreset) {
                SetMultiTextureMode(face->mtPreset);
                activeAlphaOpMtPreset = face->mtPreset;
            }

            if (face->srcAlphaMode != activeSrcBlend) {
                m_renderDevice->SetRenderState(D3DRENDERSTATE_SRCBLEND, face->srcAlphaMode);
                activeSrcBlend = face->srcAlphaMode;
            }
            if (face->destAlphaMode != activeDestBlend) {
                m_renderDevice->SetRenderState(D3DRENDERSTATE_DESTBLEND, face->destAlphaMode);
                activeDestBlend = face->destAlphaMode;
            }
            SetTexture(face->tex);
            RPFace::DrawPri(face, *m_renderDevice);
        }

        if (activeSrcBlend != D3DBLEND_SRCALPHA) {
            m_renderDevice->SetRenderState(D3DRENDERSTATE_SRCBLEND, D3DBLEND_SRCALPHA);
        }
        if (activeDestBlend != D3DBLEND_INVSRCALPHA) {
            m_renderDevice->SetRenderState(D3DRENDERSTATE_DESTBLEND, D3DBLEND_INVSRCALPHA);
        }
        SetMultiTextureMode(0);
    }

    m_renderDevice->SetRenderState(D3DRENDERSTATE_COLORKEYENABLE, FALSE);

    for (auto& pair : m_rpAlphaNoDepthList) {
        SetTexture(pair.second->tex);
        RPFace::DrawPri(pair.second, *m_renderDevice);
    }

    if (!m_rpEmissiveNoDepthList.empty()) {
        m_renderDevice->SetRenderState(D3DRENDERSTATE_SRCBLEND, D3DBLEND_ONE);
        m_renderDevice->SetRenderState(D3DRENDERSTATE_DESTBLEND, D3DBLEND_ONE);
        std::stable_sort(m_rpEmissiveNoDepthList.begin(), m_rpEmissiveNoDepthList.end(), [](const std::pair<float, RPFace*>& a, const std::pair<float, RPFace*>& b) {
            return a.first < b.first;
        });

        for (auto& pair : m_rpEmissiveNoDepthList) {
            SetTexture(pair.second->tex);
            RPFace::DrawPri(pair.second, *m_renderDevice);
        }

        m_renderDevice->SetRenderState(D3DRENDERSTATE_SRCBLEND, D3DBLEND_SRCALPHA);
        m_renderDevice->SetRenderState(D3DRENDERSTATE_DESTBLEND, D3DBLEND_INVSRCALPHA);
    }
    m_renderDevice->SetRenderState(D3DRENDERSTATE_ZENABLE, D3DZB_TRUE);
    
    m_renderDevice->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, FALSE);
    m_renderDevice->SetRenderState(D3DRENDERSTATE_ALPHATESTENABLE, TRUE);
    m_renderDevice->SetRenderState(D3DRENDERSTATE_COLORKEYENABLE, TRUE);
    m_renderDevice->SetRenderState(D3DRENDERSTATE_ZWRITEENABLE, TRUE);
    
    // 6. Clear lists for next frame
    m_rpFaceList.clear();
    m_rpLMGroundList.clear();
    m_rpLMLightList.clear();
    m_rpRawList.clear();
    m_rpAlphaList.clear();
    m_rpAlphaNoDepthList.clear();
    m_rpEmissiveList.clear();
    m_rpRawAlphaList.clear();
    m_rpLmList.clear();
    m_rpAlphaOPList.clear();
    m_rpAlphaOPNoDepthList.clear();
    m_vertBuffer.clear();
    
    m_rpNullFaceListIter = m_rpNullFaceList.begin();
    m_rpQuadFaceListIter = m_rpQuadFaceList.begin();
    m_rpLmQuadFaceListIter = m_rpLmQuadFaceList.begin();
}

void CRenderer::SetTexture(CTexture* tex) {
    if (m_oldTexture != tex) {
        if (m_renderDevice) {
            m_renderDevice->BindTexture(0, tex);
        }
        m_oldTexture = tex;
    }
}

void CRenderer::SetLookAt(vector3d& eye, vector3d& at, vector3d& up) {
    if (!m_renderDevice) {
        return;
    }

    m_eyePos = eye;
    m_eyeAt = at;
    m_eyeUp = NormalizeVec3(up);
    m_eyeForward = NormalizeVec3(SubtractVec3(at, eye));
    m_eyeRight = NormalizeVec3(CrossVec3(m_eyeUp, m_eyeForward));
    m_eyeUp = CrossVec3(m_eyeForward, m_eyeRight);

    D3DMATRIX view{};
    BuildLookAtMatrix(eye, at, up, &view);
    m_renderDevice->SetTransform(D3DTRANSFORMSTATE_VIEW, &view);
}

void CRenderer::SetLight(vector3d* dir, vector3d* diffuse, vector3d* ambient) {
    if (!dir || !diffuse || !ambient || !m_renderDevice) {
        return;
    }

    m_renderDevice->SetRenderState(D3DRENDERSTATE_AMBIENT, PackRenderColor(*ambient));
    m_renderDevice->SetRenderState(D3DRENDERSTATE_LIGHTING, TRUE);

    IDirect3DDevice7* legacyDevice = m_renderDevice->GetLegacyDevice();
    if (!legacyDevice) {
        return;
    }

    D3DLIGHT7 light{};
    light.dltType = D3DLIGHT_DIRECTIONAL;
    light.dcvDiffuse.a = 255.0f;
    light.dcvAmbient.a = 255.0f;
    light.dvDirection.x = -dir->x;
    light.dvDirection.y = -dir->y;
    light.dvDirection.z = -dir->z;
    light.dcvDiffuse.r = diffuse->x;
    light.dcvDiffuse.g = diffuse->y;
    light.dcvDiffuse.b = diffuse->z;
    light.dcvAmbient.r = ambient->x;
    light.dcvAmbient.g = ambient->y;
    light.dcvAmbient.b = ambient->z;
    legacyDevice->SetLight(0, &light);
    legacyDevice->LightEnable(0, TRUE);
}

tlvertex3d* CRenderer::BorrowVerts(unsigned int vertCount) {
    size_t oldSize = m_vertBuffer.size();
    m_vertBuffer.resize(oldSize + vertCount);
    return &m_vertBuffer[oldSize];
}

RPFace* CRenderer::BorrowNullRP() {
    if (m_rpNullFaceListIter == m_rpNullFaceList.end()) {
        m_rpNullFaceList.push_back({});
        m_rpNullFaceListIter = m_rpNullFaceList.begin();
        // Move to the newly created element
        m_rpNullFaceListIter = prev(m_rpNullFaceList.end());
        return &(*m_rpNullFaceListIter++);
    }
    return &(*m_rpNullFaceListIter++);
}

RPQuadFace* CRenderer::BorrowQuadRP() {
    if (m_rpQuadFaceListIter == m_rpQuadFaceList.end()) {
        m_rpQuadFaceList.push_back({});
        m_rpQuadFaceListIter = prev(m_rpQuadFaceList.end());
        return &(*m_rpQuadFaceListIter++);
    }
    return &(*m_rpQuadFaceListIter++);
}

RPLmQuadFace* CRenderer::BorrowLmQuadRP() {
    if (m_rpLmQuadFaceListIter == m_rpLmQuadFaceList.end()) {
        m_rpLmQuadFaceList.push_back({});
        m_rpLmQuadFaceListIter = prev(m_rpLmQuadFaceList.end());
        return &(*m_rpLmQuadFaceListIter++);
    }
    return &(*m_rpLmQuadFaceListIter++);
}

void RPFace::DrawPri(RPFace* face, IRenderDevice& renderDevice) {
    if (face->indices && face->numIndices > 0) {
        renderDevice.DrawIndexedPrimitive(face->primType, D3DFVF_TLVERTEX, face->verts, face->numVerts, face->indices, face->numIndices, 0);
    } else {
        renderDevice.DrawPrimitive(face->primType, D3DFVF_TLVERTEX, face->verts, face->numVerts, 0);
    }
}

void RPLmFace::DrawPri(RPLmFace* face, IRenderDevice& renderDevice) {
    if (face->indices && face->numIndices > 0) {
        renderDevice.DrawIndexedPrimitive(face->primType, kLmFvf, face->lmverts, face->numVerts, face->indices, face->numIndices, 0);
    } else {
        renderDevice.DrawPrimitive(face->primType, kLmFvf, face->lmverts, face->numVerts, 0);
    }
}

void RPRaw::DrawPri(RPRaw* face, IRenderDevice& renderDevice) {
    if (face->indices && face->numIndices > 0) {
        renderDevice.DrawIndexedPrimitive(face->primType, D3DFVF_TLVERTEX, face->verts, face->numVerts, face->indices, face->numIndices, 0);
    } else {
        renderDevice.DrawPrimitive(face->primType, D3DFVF_TLVERTEX, face->verts, face->numVerts, 0);
    }
}
