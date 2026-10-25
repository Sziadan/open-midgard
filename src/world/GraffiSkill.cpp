#include "world/GameActor.h"

#include "gamemode/GameMode.h"
#include "gamemode/Mode.h"
#include "render/Renderer.h"
#include "res/Bitmap.h"
#include "res/Res.h"
#include "world/World.h"

#include <cmath>
#include <cstring>
#include <vector>

namespace {

constexpr int kGraffitiSize = 384;
constexpr int kGraffitiSpan = 18;
constexpr char kAlphabetPath[] = "effect\\alpabet.bmp";

int GlyphWidth(unsigned char ch, int& glyph, bool& draw)
{
    draw = true;
    if (ch >= 'a' && ch <= 'z') {
        glyph = static_cast<int>(ch) - 71;
        return 32;
    }
    if (ch >= 'A' && ch <= 'Z') {
        glyph = static_cast<int>(ch) - 65;
        return 32;
    }
    if (ch >= '0' && ch <= '9') {
        glyph = static_cast<int>(ch) + 6;
        return 32;
    }
    if (ch == '?') {
        glyph = 53;
        return 16;
    }
    if (ch == '!') {
        glyph = 54;
        return 16;
    }
    glyph = 99;
    draw = false;
    return ch == ' ' ? 32 : 16;
}

void BlitGlyph(std::vector<u32>& dest, const u32* src, int srcW, int srcH, int glyph, int dx, int dy, int dw)
{
    if (src == nullptr || dw <= 0 || glyph < 0) {
        return;
    }
    const int sx = 32 * (glyph & 7);
    const int sy = 32 * (glyph >> 3);
    for (int y = 0; y < 32; ++y) {
        const int destY = dy + y;
        if (destY < 0 || destY >= kGraffitiSize) {
            continue;
        }
        const int sampleY = sy + y;
        if (sampleY < 0 || sampleY >= srcH) {
            continue;
        }
        for (int x = 0; x < dw; ++x) {
            const int destX = dx + x;
            if (destX < 0 || destX >= kGraffitiSize) {
                continue;
            }
            const int sampleX = sx + x * 32 / dw;
            if (sampleX < 0 || sampleX >= srcW) {
                continue;
            }
            dest[static_cast<size_t>(destY) * kGraffitiSize + static_cast<size_t>(destX)] =
                src[static_cast<size_t>(sampleY) * static_cast<size_t>(srcW) + static_cast<size_t>(sampleX)];
        }
    }
}

} // namespace

CGraffiSkill::CGraffiSkill()
{
    m_strMsg[0] = '\0';
}

CGraffiSkill::~CGraffiSkill()
{
    delete m_tex;
    m_tex = nullptr;
}

u8 CGraffiSkill::OnProcess()
{
    return 1;
}

void CGraffiSkill::OnInit()
{
    m_offsX = 0;
    m_offsY = 0;
    delete m_tex;
    m_tex = nullptr;

    const int length = static_cast<int>(std::strlen(m_strMsg));
    m_offsY += 36 * ((144 - length) / 24);

    const std::string fontPath = ResolveTexturePath(kAlphabetPath);
    CBitmapRes* font = fontPath.empty() ? nullptr : g_resMgr.GetAs<CBitmapRes>(fontPath.c_str());
    const u32* fontPixels = (font != nullptr) ? font->m_data : nullptr;
    const int fontW = (font != nullptr) ? font->m_width : 0;
    const int fontH = (font != nullptr) ? font->m_height : 0;

    std::vector<u32> pixels(static_cast<size_t>(kGraffitiSize) * kGraffitiSize, 0u);
    for (int index = 0; index < length; ++index) {
        const unsigned char ch = static_cast<unsigned char>(m_strMsg[index]);
        if (ch == ' ') {
            int projected = m_offsX;
            const char* next = m_strMsg + index + 1;
            if (*next != '\0' && *next != ' ') {
                for (const char* word = next; *word != '\0' && *word != ' '; ++word) {
                    projected += 32;
                }
            }
            if (projected >= 512) {
                m_offsX = 0;
                m_offsY += 36;
            }
        }

        int glyph = 99;
        bool draw = false;
        const int width = GlyphWidth(ch, glyph, draw);
        if (m_offsX + width >= 512) {
            m_offsX = 0;
            m_offsY += 36;
        }
        if (draw) {
            BlitGlyph(pixels, fontPixels, fontW, fontH, glyph, m_offsX, m_offsY, width);
        }
        m_offsX += width;
    }

    m_tex = g_texMgr.CreateTexture(kGraffitiSize, kGraffitiSize, pixels.data(), PF_A8R8G8B8, false);

    CGameMode* gameMode = g_modeMgr.GetCurrentGameMode();
    if (gameMode != nullptr && gameMode->m_world != nullptr && gameMode->m_world->m_player != nullptr) {
        m_rot = gameMode->m_world->m_player->m_roty;
    } else {
        m_rot = 0.0f;
    }
}

void CGraffiSkill::Render(matrix* viewMatrix)
{
    if (viewMatrix == nullptr || m_tex == nullptr) {
        return;
    }
    CGameMode* gameMode = g_modeMgr.GetCurrentGameMode();
    if (gameMode == nullptr || gameMode->m_world == nullptr || gameMode->m_world->m_ground == nullptr) {
        return;
    }

    const float angle = m_rot * 3.141592f * 0.0055555557f;
    const float sinR = std::sin(angle);
    const float cosR = std::cos(angle);
    const int originX = m_groundPosX - 9;
    const int originY = m_groundPosY - 9;
    constexpr float kHalf = 9.0f;
    constexpr float kInvSpan = 1.0f / 18.0f;

    for (int row = 0; row < kGraffitiSpan; ++row) {
        const float v0 = static_cast<float>(row) - kHalf;
        const float v1 = static_cast<float>(row + 1) - kHalf;
        for (int column = 0; column < kGraffitiSpan; ++column) {
            const float u0 = static_cast<float>(column) - kHalf;
            const float u1 = static_cast<float>(column + 1) - kHalf;
            vector2d uv[4];
            const float corners[4][2] = {
                { u0, v0 },
                { u1, v0 },
                { u0, v1 },
                { u1, v1 },
            };
            for (int corner = 0; corner < 4; ++corner) {
                const float u = corners[corner][0];
                const float v = corners[corner][1];
                const float rotatedX = u * cosR + v * sinR;
                const float rotatedY = v * cosR - u * sinR;
                uv[corner].x = 1.0f - (rotatedX + kHalf) * kInvSpan;
                uv[corner].y = (rotatedY + kHalf) * kInvSpan;
            }
            gameMode->m_world->m_ground->RenderGroundTile(*viewMatrix,
                originX + column,
                originY + row,
                0xFFFFFFFFu,
                m_tex,
                uv);
        }
    }
}

void CGraffiSkill::SendMsg(CGameObject* sender, int message, msgparam_t arg1, msgparam_t arg2, msgparam_t arg3)
{
    switch (message) {
    case 22:
        m_aid = static_cast<u32>(arg1);
        break;
    case 115:
        m_lastingTime = static_cast<u32>(arg1);
        break;
    case 117:
        if (arg1 != 0) {
            std::strncpy(m_strMsg, reinterpret_cast<const char*>(arg1), sizeof(m_strMsg) - 1);
            m_strMsg[sizeof(m_strMsg) - 1] = '\0';
        } else {
            m_strMsg[0] = '\0';
        }
        m_groundPosX = static_cast<int>(arg2);
        m_groundPosY = static_cast<int>(arg3);
        OnInit();
        break;
    default:
        CGameActor::SendMsg(sender, message, arg1, arg2, arg3);
        break;
    }
}
