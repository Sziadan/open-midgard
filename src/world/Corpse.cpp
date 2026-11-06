#include "Corpse.h"
#include "CorpseState.h"
#include "World.h"
#include "gamemode/GameMode.h"
#include "gamemode/Mode.h"
#include "gamemode/View.h"
#include "render/Renderer.h"
#include "res/Texture.h"
#include <algorithm>
#include <cmath>

CCorpse::CCorpse(const vector3d& position, CActRes* act, CSprRes* spr,
    int action, int motion, float rotation)
{
    m_pos = position;
    m_actRes = act;
    m_sprRes = spr;
    if (act) { act->Lock(); m_actName = act->m_fName.m_String; }
    if (spr) { spr->Lock(); m_sprName = spr->m_fName.m_String; }
    m_baseAction = m_curAction = action;
    m_curMotion = motion;
    m_roty = rotation;
    m_isVisible = m_isLieOnGround = 1;
    m_stateStartTick = timeGetTime();
}

CCorpse::~CCorpse()
{
    m_textures.clear();
    if (m_actRes) m_actRes->Unlock();
    if (m_sprRes) m_sprRes->Unlock();
}

u8 CCorpse::OnProcess()
{
    return ProcessAt(timeGetTime());
}

u8 CCorpse::ProcessAt(u32 now)
{
    const float frames = ro::corpse::ElapsedFrames(now, m_stateStartTick);
    m_curAction = m_baseAction + CGameObject::Get8Dir(m_roty);
    // The original GetMotionCount returns one for an empty/missing action.
    const int count = m_actRes ? m_actRes->GetMotionCount(m_curAction) : 0;
    m_curMotion = (std::max)(1, count) - 1;
    SetArgb(static_cast<int>(255.0 - frames), -1, -1, -1);
    return frames < 255.0;
}

CTexture* CCorpse::TextureFor(int clipType, int spriteIndex)
{
    const auto key = std::make_pair(clipType, spriteIndex);
    const auto found = m_textures.find(key);
    if (found != m_textures.end()) return found->second.get();
    const SprImg* image = m_sprRes ? m_sprRes->GetSprite(clipType, spriteIndex) : nullptr;
    if (!image || image->width <= 0 || image->height <= 0) return nullptr;
    const size_t count = static_cast<size_t>(image->width) * image->height;
    if ((clipType == 0 && image->indices.size() < count)
        || (clipType != 0 && image->rgba.size() < count)) return nullptr;
    std::vector<unsigned int> pixels(count);
    for (size_t i = 0; i < count; ++i) {
        if (clipType == 0) {
            const unsigned char index = image->indices[i];
            pixels[i] = index ? 0xFF000000u | (m_sprRes->m_pal[index] & 0xFFFFFFu) : 0;
        } else pixels[i] = image->rgba[i];
    }
    auto texture = std::make_unique<CTexture>();
    if (!texture->Create(image->width, image->height, PF_A8R8G8B8)) return nullptr;
    texture->Update(0, 0, image->width, image->height, pixels.data(), true,
        image->width * static_cast<int>(sizeof(unsigned int)));
    CTexture* result = texture.get();
    m_textures.emplace(key, std::move(texture));
    return result;
}

void CCorpse::Render(matrix* view)
{
    if (!view || !m_isVisible || !m_sprRes || !m_actRes) return;
    const CMotion* motion = m_actRes->GetMotion(m_curAction, m_curMotion);
    if (!motion) return;
    const double depth = static_cast<double>(view->m[0][2]) * m_pos.x
        + static_cast<double>(view->m[1][2]) * m_pos.y
        + static_cast<double>(view->m[2][2]) * m_pos.z + view->m[3][2];
    const float bodyDepth = static_cast<float>(depth); // Original RenderBody's tv.z storage.
    if (!(depth > 0.0) || !std::isfinite(depth)) return;
    const float oow = static_cast<float>(1.0 / depth);
    const float x = static_cast<float>((static_cast<double>(view->m[0][0]) * m_pos.x + static_cast<double>(view->m[1][0]) * m_pos.y
        + static_cast<double>(view->m[2][0]) * m_pos.z + view->m[3][0]) / depth * g_renderer.m_hpc + g_renderer.m_xoffset);
    const float y = static_cast<float>((static_cast<double>(view->m[0][1]) * m_pos.x + static_cast<double>(view->m[1][1]) * m_pos.y
        + static_cast<double>(view->m[2][1]) * m_pos.z + view->m[3][1]) * oow * g_renderer.m_vpc + g_renderer.m_yoffset);
    const float pixelRatio = oow * g_renderer.m_hpc * 0.14285715f;
    const float averagePixelRatio = g_renderer.m_height * 0.0017447917f;
    m_lastPixelRatio = averagePixelRatio > 0 ? pixelRatio / averagePixelRatio : 1.0f;
    CGameMode* mode = g_modeMgr.GetCurrentGameMode();
    const CView* camera = mode ? mode->m_view : nullptr;
    const float latitude = camera ? camera->GetCameraLatitude() : -45.0f;
    const float distance = camera ? camera->GetCameraDistance() : 300.0f;
    // CView::CalcView uses integer-degree trig tables for these depth planes.
    const double angle = static_cast<int>(-latitude) * 3.141592653589793 / 180.0;
    const double sine = std::sin(angle), cosine = std::cos(angle);
    const double screenY = g_renderer.m_screenYFactor > 0 ? g_renderer.m_screenYFactor : 1.0;
    const double shadowSlope = std::fabs(sine) > 1e-8 ? 0.16 * cosine / sine / screenY : 0.0;
    const double relativeDistance = distance > 0 ? 300.0 / distance : 1.0;
    const double layerOffset = sine / std::sin(3.141592653589793 / 4.0) * relativeDistance * 0.0000090000003;
    const double hoverOffset = relativeDistance * 0.000036000001 * m_lastPixelRatio;
    float lightR = 1, lightG = 1, lightB = 1;
    if (mode && mode->m_world && mode->m_world->m_ground)
        mode->m_world->m_ground->GetActorLightIntensity(m_pos.x, m_pos.z, lightR, lightG, lightB);

    for (size_t i = 0; i < motion->sprClips.size(); ++i) {
        const CSprClip& clip = motion->sprClips[i];
        const SprImg* image = m_sprRes->GetSprite(clip.clipType, clip.sprIndex);
        if (!image) continue;
        CTexture* texture = TextureFor(clip.clipType, clip.sprIndex);
        if (!texture) continue;
        const float scaleX = clip.zoomX * m_zoom * pixelRatio;
        const float scaleY = clip.zoomY * m_zoom * pixelRatio;
        const float top = clip.y * scaleY + y;
        const float bottom = (clip.y + image->height * (image->isHalfH + 1)) * scaleY + y + 1.0f;
        float left = clip.x * scaleX + x;
        float right = (clip.x + image->width * (image->isHalfW + 1)) * scaleX + x + 1.0f;
        if (clip.flags & 1) {
            left = (clip.x + image->width * (image->isHalfW + 1)) * scaleX + x;
            right = clip.x * scaleX + x + 1.0f;
        }
        const float shift = g_renderer.m_screenXFactor * m_sprShift * m_zoom * m_lastPixelRatio;
        left += shift; right += shift;
        const double bias = (static_cast<double>(m_offsetOow) + static_cast<double>(i) + 3.0) * layerOffset + hoverOffset;
        const float topOow = static_cast<float>(1.0 / (bodyDepth - (top - y) * shadowSlope) + bias);
        const float bottomOow = static_cast<float>(1.0 / (bodyDepth - (bottom - y) * shadowSlope) + bias);
        if (!std::isfinite(topOow) || !std::isfinite(bottomOow)) continue;
        const auto channel = [](double value) -> u32 { return static_cast<u32>(std::clamp(value, 0.0, 255.0)); };
        const COLOR& color = m_u28.m_sprColor;
        const u32 alpha = channel(clip.a * static_cast<double>(color.u.s.a) * 0.0039215689);
        const u32 tint = (alpha << 24)
            | (channel(clip.r * static_cast<double>(color.u.s.r) * lightR * 0.0039215689) << 16)
            | (channel(clip.g * static_cast<double>(color.u.s.g) * lightG * 0.0039215689) << 8)
            | channel(clip.b * static_cast<double>(color.u.s.b) * lightB * 0.0039215689);
        RPFace* face = g_renderer.BorrowNullRP();
        if (!face) continue;
        face->primType = D3DPT_TRIANGLESTRIP;
        face->verts = face->m_verts; face->numVerts = 4;
        face->indices = nullptr; face->numIndices = 0;
        face->tex = texture; face->mtPreset = kSpriteTextureMode;
        face->cullMode = D3DCULL_NONE;
        face->srcAlphaMode = D3DBLEND_SRCALPHA; face->destAlphaMode = D3DBLEND_INVSRCALPHA;
        face->alphaSortKey = topOow; // Original alpha sort key is vertex0.oow.
        const float radians = (clip.angle + m_sprAngle) * 3.14159265358979323846f / 180.0f;
        const float cs = std::cos(radians), sn = std::sin(radians);
        const float cx = (left + right) * 0.5f, cy = (top + bottom) * 0.5f;
        const float halfW = (right - left) * 0.5f, halfH = (bottom - top) * 0.5f;
        const float maxU = static_cast<float>(texture->m_surfaceUpdateWidth) / texture->m_w;
        const float maxV = static_cast<float>(texture->m_surfaceUpdateHeight) / texture->m_h;
        for (int corner = 0; corner < 4; ++corner) {
            const float dx = (corner & 1) ? halfW : -halfW;
            const float dy = (corner & 2) ? halfH : -halfH;
            tlvertex3d& vertex = face->m_verts[corner];
            vertex.x = cx + dx * cs - dy * sn + 0.5f;
            vertex.y = cy + dx * sn + dy * cs + 0.5f;
            vertex.oow = (corner & 2) ? bottomOow : topOow;
            vertex.z = ro::corpse::DeviceDepth(vertex.oow);
            vertex.tu = (corner & 1) ? maxU : 0;
            vertex.tv = (corner & 2) ? maxV : 0;
            vertex.color = tint; vertex.specular = 0xFF000000u;
        }
        g_renderer.AddRP(face, alpha == 255 ? 257 : 1);
    }
}
