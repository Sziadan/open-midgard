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
}

std::string ResolveTexturePath(const char* name)
{
    if (!name || !*name) {
        return std::string();
    }

    const char* candidates[] = {
        name,
        "texture\\",
        "data\\texture\\"
    };

    for (const char* prefix : candidates) {
        std::string path = prefix == name ? std::string(name) : std::string(prefix) + name;
        if (g_fileMgr.IsDataExist(path.c_str())) {
            return path;
        }
    }

    const char* dot = std::strrchr(name, '.');
    if (!dot) {
        const char* exts[] = { ".bmp", ".jpg", ".jpeg", ".tga" };
        for (const char* prefix : candidates) {
            const std::string base = prefix == name ? std::string(name) : std::string(prefix) + name;
            for (const char* ext : exts) {
                const std::string path = base + ext;
                if (g_fileMgr.IsDataExist(path.c_str())) {
                    return path;
                }
            }
        }
    }

    return std::string();
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
}

CTexture* CTexMgr::GetTexture(const char* name, bool b) {
    (void)b;
    auto it = m_texTable.find(name);
    if (it != m_texTable.end()) {
        it->second->m_timeStamp = GetTickCount();
        return it->second;
    }

    const std::string texturePath = ResolveTexturePath(name);
    if (texturePath.empty()) {
        static int missingTextureLogCount = 0;
        if (missingTextureLogCount < 16) {
            ++missingTextureLogCount;
            if constexpr (kLogTexture) {
                DbgLog("[Texture] missing '%s'\n", name ? name : "(null)");
            }
        }
        return &s_dummy_texture;
    }

    CBitmapRes* bitmap = g_resMgr.GetAs<CBitmapRes>(texturePath.c_str());
    if (!bitmap || !bitmap->m_data || bitmap->m_width <= 0 || bitmap->m_height <= 0) {
        static int failedTextureLoadLogCount = 0;
        if (failedTextureLoadLogCount < 16) {
            ++failedTextureLoadLogCount;
            if constexpr (kLogTexture) {
                DbgLog("[Texture] load failed name='%s' resolved='%s'\n", name ? name : "(null)", texturePath.c_str());
            }
        }
        return &s_dummy_texture;
    }

    CTexture* tex = CreateTexture(bitmap->m_width, bitmap->m_height,
        reinterpret_cast<unsigned long*>(bitmap->m_data), PF_A8R8G8B8, false);
    if (!tex) {
        static int failedTextureCreateLogCount = 0;
        if (failedTextureCreateLogCount < 16) {
            ++failedTextureCreateLogCount;
            if constexpr (kLogTexture) {
                DbgLog("[Texture] create failed name='%s' resolved='%s' size=%dx%d\n",
                    name ? name : "(null)",
                    texturePath.c_str(),
                    bitmap->m_width,
                    bitmap->m_height);
            }
        }
        return &s_dummy_texture;
    }

    static int loadedTextureLogCount = 0;
    if (loadedTextureLogCount < 16) {
        ++loadedTextureLogCount;
        if constexpr (kLogTexture) {
            DbgLog("[Texture] loaded name='%s' resolved='%s' size=%dx%d surface=%p\n",
                name ? name : "(null)",
                texturePath.c_str(),
                bitmap->m_width,
                bitmap->m_height,
                static_cast<void*>(tex->m_pddsSurface));
        }
    }

    if (ShouldLogGroundTextureName(name)) {
        LogUploadedTerrainSurfaceSamples(tex, name);
    }

    std::strncpy(tex->m_texName, name, sizeof(tex->m_texName) - 1);
    tex->m_texName[sizeof(tex->m_texName) - 1] = '\0';
    tex->m_timeStamp = GetTickCount();
    m_texTable[tex->m_texName] = tex;
    return tex;
}

CTexture* CTexMgr::CreateTexture(int w, int h, unsigned long* data, PixelFormat format, bool b) {
    CTexture* tex = new CTexture();
    if (!tex) {
        return nullptr;
    }
    if (!tex->Create(static_cast<unsigned int>(w), static_cast<unsigned int>(h), format)) {
        delete tex;
        return nullptr;
    }
    tex->Update(0, 0, w, h, reinterpret_cast<unsigned int*>(data), b,
        w * static_cast<int>(sizeof(unsigned long)));
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
        pSurface->AddRef();
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
            m_rpAlphaOPList.push_back({sortKey, face});
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
        m_renderDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
        m_renderDevice->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
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
    blendedFaces.reserve(m_rpAlphaList.size() + m_rpEmissiveList.size() + m_rpAlphaOPList.size());
    for (auto& pair : m_rpAlphaList) {
        blendedFaces.push_back({ pair.first, pair.second, D3DBLEND_SRCALPHA, D3DBLEND_INVSRCALPHA });
    }
    for (auto& pair : m_rpEmissiveList) {
        blendedFaces.push_back({ pair.first, pair.second, D3DBLEND_ONE, D3DBLEND_ONE });
    }
    for (auto& pair : m_rpAlphaOPList) {
        blendedFaces.push_back({ pair.first, pair.second, pair.second->srcAlphaMode, pair.second->destAlphaMode });
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
        std::stable_sort(m_rpAlphaOPNoDepthList.begin(), m_rpAlphaOPNoDepthList.end(), [](const std::pair<float, RPFace*>& a, const std::pair<float, RPFace*>& b) {
            return a.first < b.first;
        });

        D3DBLEND activeSrcBlend = D3DBLEND_SRCALPHA;
        D3DBLEND activeDestBlend = D3DBLEND_INVSRCALPHA;
        for (auto& pair : m_rpAlphaOPNoDepthList) {
            RPFace* face = pair.second;
            if (!face) {
                continue;
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
    }

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
    (void)dir;
    (void)diffuse;
    (void)ambient;
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
