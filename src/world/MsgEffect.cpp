#include "MsgEffect.h"

#include "World.h"
#include "DebugLog.h"
#include "main/WinMain.h"
#include "render/DC.h"
#include "render/DrawUtil.h"
#include "render/Renderer.h"
#include "render3d/Device.h"
#include "res/ActRes.h"
#include "res/Sprite.h"
#include "res/Res.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace {

constexpr float kMsgEffectNearPlane = 10.0f;
constexpr float kMsgEffectSubmitNearPlane = 80.0f;
constexpr const char* kDamageNumberSpritePath = "data\\sprite\\\xC0\xCC\xC6\xD1\xC6\xAE\\\xBC\xFD\xC0\xDA.spr";
constexpr const char* kDamageNumberActPath = "data\\sprite\\\xC0\xCC\xC6\xD1\xC6\xAE\\\xBC\xFD\xC0\xDA.act";

struct QueuedMsgEffectDraw {
    int screenX = 0;
    int screenY = 0;
    int digit = 0;
    u32 colorArgb = 0xFFFFFFFFu;
    int alpha = 255;
    float zoom = 1.0f;
    int sprShift = 0;
};

std::vector<QueuedMsgEffectDraw> g_queuedMsgEffects;

bool ProjectMsgEffectPoint(const matrix& viewMatrix, const vector3d& point, tlvertex3d* outVertex)
{
    if (!outVertex) {
        return false;
    }

    const float clipZ = point.x * viewMatrix.m[0][2]
        + point.y * viewMatrix.m[1][2]
        + point.z * viewMatrix.m[2][2]
        + viewMatrix.m[3][2];
    if (!std::isfinite(clipZ) || clipZ <= kMsgEffectSubmitNearPlane) {
        return false;
    }

    const float oow = 1.0f / clipZ;
    const float projectedX = point.x * viewMatrix.m[0][0]
        + point.y * viewMatrix.m[1][0]
        + point.z * viewMatrix.m[2][0]
        + viewMatrix.m[3][0];
    const float projectedY = point.x * viewMatrix.m[0][1]
        + point.y * viewMatrix.m[1][1]
        + point.z * viewMatrix.m[2][1]
        + viewMatrix.m[3][1];

    outVertex->x = g_renderer.m_xoffset + projectedX * g_renderer.m_hpc * oow;
    outVertex->y = g_renderer.m_yoffset + projectedY * g_renderer.m_vpc * oow;
    outVertex->z = (1500.0f / (1500.0f - kMsgEffectNearPlane)) * ((1.0f / oow) - kMsgEffectNearPlane) * oow;
    outVertex->oow = oow;
    outVertex->specular = 0xFF000000u;
    return std::isfinite(outVertex->x) && std::isfinite(outVertex->y) && std::isfinite(outVertex->z);
}

COLORREF ScaleColorByAlpha(u32 argb, int alpha)
{
    const int effectiveAlpha = (std::max)(0, (std::min)(255, alpha));
    const int red = static_cast<int>((argb >> 16) & 0xFFu) * effectiveAlpha / 255;
    const int green = static_cast<int>((argb >> 8) & 0xFFu) * effectiveAlpha / 255;
    const int blue = static_cast<int>(argb & 0xFFu) * effectiveAlpha / 255;
    return RGB(red, green, blue);
}

CSprRes* GetDamageCountSprite()
{
    static CSprRes* sprite = nullptr;
    static bool attemptedLoad = false;
    if (!attemptedLoad) {
        attemptedLoad = true;
        sprite = g_resMgr.GetAs<CSprRes>(kDamageNumberSpritePath);
        if (!sprite) {
            DbgLog("[MsgEffect] Failed to load damage number sprite '%s'\n", kDamageNumberSpritePath);
        }
    }
    return sprite;
}

CActRes* GetDamageCountAct()
{
    static CActRes* act = nullptr;
    static bool attemptedLoad = false;
    if (!attemptedLoad) {
        attemptedLoad = true;
        act = g_resMgr.GetAs<CActRes>(kDamageNumberActPath);
        if (!act) {
            DbgLog("[MsgEffect] Failed to load damage number act '%s'\n", kDamageNumberActPath);
        }
    }
    return act;
}

float ResolveDigitScale(float zoom)
{
    const float scale = 1.8f + zoom * 0.6f;
    return (std::max)(1.8f, (std::min)(4.8f, scale));
}

int ResolveDigitShiftPixels(const QueuedMsgEffectDraw& draw)
{
    return draw.sprShift;
}

int CountDamageDigits(int value)
{
    if (value >= 100000) {
        return 6;
    }
    if (value >= 10000) {
        return 5;
    }
    if (value >= 1000) {
        return 4;
    }
    if (value >= 100) {
        return 3;
    }
    if (value >= 10) {
        return 2;
    }
    return 1;
}

int ResolveDigitAtIndex(int value, int index)
{
    for (int i = 0; i < index; ++i) {
        value /= 10;
    }
    return value % 10;
}

bool ResolveDigitClipBox(int digit, RECT* outClipBox)
{
    if (!outClipBox) {
        return false;
    }

    CSprRes* sprite = GetDamageCountSprite();
    CActRes* act = GetDamageCountAct();
    if (!sprite || !act) {
        return false;
    }

    const int digitIndex = (std::max)(0, (std::min)(9, digit));
    const CMotion* motion = act->GetMotion(0, digitIndex);
    if (!motion) {
        motion = act->GetMotion(0, 0);
    }
    if (!motion) {
        return false;
    }

    RECT clipBox{};
    bool hasClip = false;
    for (const CSprClip& clip : motion->sprClips) {
        const SprImg* image = sprite->GetSprite(clip.clipType, clip.sprIndex);
        if (!image || image->width <= 0 || image->height <= 0) {
            continue;
        }

        const int drawX = clip.x - image->width / 2;
        const int drawY = clip.y - image->height / 2;
        RECT current = { drawX, drawY, drawX + image->width, drawY + image->height };
        if (!hasClip) {
            clipBox = current;
            hasClip = true;
        } else {
            clipBox.left = (std::min)(clipBox.left, current.left);
            clipBox.top = (std::min)(clipBox.top, current.top);
            clipBox.right = (std::max)(clipBox.right, current.right);
            clipBox.bottom = (std::max)(clipBox.bottom, current.bottom);
        }
    }

    if (!hasClip) {
        return false;
    }

    *outClipBox = clipBox;
    return true;
}

int ResolveDigitOutputWidth(int digit, float zoom)
{
    RECT clipBox{};
    if (!ResolveDigitClipBox(digit, &clipBox)) {
        return (std::max)(1, static_cast<int>(std::lround(8.0f * ResolveDigitScale(zoom))));
    }

    const int nativeWidth = (std::max)(1, static_cast<int>(clipBox.right - clipBox.left));
    return (std::max)(1, static_cast<int>(std::lround(static_cast<float>(nativeWidth) * ResolveDigitScale(zoom))));
}

unsigned int PremultiplyArgb(unsigned int color)
{
    const unsigned int alpha = (color >> 24) & 0xFFu;
    if (alpha == 0u || alpha == 0xFFu) {
        return color;
    }

    const unsigned int red = ((color >> 16) & 0xFFu) * alpha / 255u;
    const unsigned int green = ((color >> 8) & 0xFFu) * alpha / 255u;
    const unsigned int blue = (color & 0xFFu) * alpha / 255u;
    return (alpha << 24) | (red << 16) | (green << 8) | blue;
}

unsigned int ModulateDigitColor(unsigned int srcRgb, unsigned int alpha, unsigned int tintRed, unsigned int tintGreen, unsigned int tintBlue)
{
    const unsigned int srcRed = (srcRgb >> 16) & 0xFFu;
    const unsigned int srcGreen = (srcRgb >> 8) & 0xFFu;
    const unsigned int srcBlue = srcRgb & 0xFFu;
    const unsigned int maxChannel = (std::max)(srcRed, (std::max)(srcGreen, srcBlue));

    if (maxChannel <= 24u) {
        return PremultiplyArgb(alpha << 24);
    }

    const unsigned int modulatedRed = srcRed * tintRed / 255u;
    const unsigned int modulatedGreen = srcGreen * tintGreen / 255u;
    const unsigned int modulatedBlue = srcBlue * tintBlue / 255u;
    return PremultiplyArgb((alpha << 24) | (modulatedRed << 16) | (modulatedGreen << 8) | modulatedBlue);
}

bool DrawDigitSprite(HDC hdc, const QueuedMsgEffectDraw& draw)
{
    if (!hdc) {
        return false;
    }

    CSprRes* sprite = GetDamageCountSprite();
    CActRes* act = GetDamageCountAct();
    if (!sprite || !act) {
        return false;
    }

    const int digitIndex = (std::max)(0, (std::min)(9, draw.digit));
    const CMotion* motion = act->GetMotion(0, digitIndex);
    if (!motion) {
        motion = act->GetMotion(0, 0);
    }
    if (!motion) {
        return false;
    }

    RECT clipBox{};
    if (!ResolveDigitClipBox(draw.digit, &clipBox)) {
        return false;
    }

    const float scale = ResolveDigitScale(draw.zoom);
    const int nativeWidth = (std::max)(1, static_cast<int>(clipBox.right - clipBox.left));
    const int nativeHeight = (std::max)(1, static_cast<int>(clipBox.bottom - clipBox.top));
    const int outWidth = (std::max)(1, static_cast<int>(std::lround(static_cast<float>(nativeWidth) * scale)));
    const int outHeight = (std::max)(1, static_cast<int>(std::lround(static_cast<float>(nativeHeight) * scale)));
    const int originX = draw.screenX + ResolveDigitShiftPixels(draw) - outWidth / 2;
    const int originY = draw.screenY - outHeight / 2;

    const unsigned int tintAlpha = (draw.colorArgb >> 24) & 0xFFu;
    const unsigned int totalAlpha = static_cast<unsigned int>((std::max)(0, (std::min)(255, draw.alpha))) * tintAlpha / 255u;
    const unsigned int tintRed = (draw.colorArgb >> 16) & 0xFFu;
    const unsigned int tintGreen = (draw.colorArgb >> 8) & 0xFFu;
    const unsigned int tintBlue = draw.colorArgb & 0xFFu;

    std::vector<unsigned int> srcPixels(static_cast<size_t>(nativeWidth) * nativeHeight, 0u);
    for (const CSprClip& clip : motion->sprClips) {
        const SprImg* image = sprite->GetSprite(clip.clipType, clip.sprIndex);
        if (!image || image->width <= 0 || image->height <= 0) {
            continue;
        }

        const bool flipX = (clip.flags & 1) != 0;
        const int destLeft = clip.x - image->width / 2 - clipBox.left;
        const int destTop = clip.y - image->height / 2 - clipBox.top;

        for (int sy = 0; sy < image->height; ++sy) {
            const int dy = destTop + sy;
            if (dy < 0 || dy >= nativeHeight) {
                continue;
            }

            for (int sx = 0; sx < image->width; ++sx) {
                const int sourceX = flipX ? (image->width - 1 - sx) : sx;
                const int dx = destLeft + sx;
                if (dx < 0 || dx >= nativeWidth) {
                    continue;
                }

                unsigned int pixel = 0u;
                if (clip.clipType == 0) {
                    const unsigned char paletteIndex = image->indices[static_cast<size_t>(sy) * image->width + sourceX];
                    if (paletteIndex == 0) {
                        continue;
                    }
                    const unsigned int paletteColor = sprite->m_pal[paletteIndex] & 0x00FFFFFFu;
                    const unsigned int clipAlpha = static_cast<unsigned int>(clip.a) * totalAlpha / 255u;
                    pixel = ModulateDigitColor(
                        paletteColor,
                        clipAlpha,
                        tintRed * clip.r / 255u,
                        tintGreen * clip.g / 255u,
                        tintBlue * clip.b / 255u);
                } else {
                    const unsigned int rgba = image->rgba[static_cast<size_t>(sy) * image->width + sourceX];
                    const unsigned int srcAlpha = (rgba >> 24) & 0xFFu;
                    if (srcAlpha == 0u) {
                        continue;
                    }
                    const unsigned int clipAlpha = srcAlpha * clip.a * totalAlpha / (255u * 255u);
                    pixel = ModulateDigitColor(
                        rgba & 0x00FFFFFFu,
                        clipAlpha,
                        tintRed * clip.r / 255u,
                        tintGreen * clip.g / 255u,
                        tintBlue * clip.b / 255u);
                }

                const size_t destIndex = static_cast<size_t>(dy) * nativeWidth + dx;
                const unsigned int srcAlpha = (pixel >> 24) & 0xFFu;
                if (srcAlpha == 0xFFu || (srcPixels[destIndex] >> 24) == 0u) {
                    srcPixels[destIndex] = pixel;
                } else if (srcAlpha != 0u) {
                    const unsigned int dst = srcPixels[destIndex];
                    const unsigned int dstAlpha = (dst >> 24) & 0xFFu;
                    const unsigned int invAlpha = 255u - srcAlpha;
                    const unsigned int outAlpha = srcAlpha + dstAlpha * invAlpha / 255u;
                    const unsigned int outRed = ((pixel >> 16) & 0xFFu) + (((dst >> 16) & 0xFFu) * invAlpha / 255u);
                    const unsigned int outGreen = ((pixel >> 8) & 0xFFu) + (((dst >> 8) & 0xFFu) * invAlpha / 255u);
                    const unsigned int outBlue = (pixel & 0xFFu) + ((dst & 0xFFu) * invAlpha / 255u);
                    srcPixels[destIndex] = (outAlpha << 24) | (outRed << 16) | (outGreen << 8) | outBlue;
                }
            }
        }
    }

    BITMAPINFO srcBmi{};
    srcBmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    srcBmi.bmiHeader.biWidth = nativeWidth;
    srcBmi.bmiHeader.biHeight = -nativeHeight;
    srcBmi.bmiHeader.biPlanes = 1;
    srcBmi.bmiHeader.biBitCount = 32;
    srcBmi.bmiHeader.biCompression = BI_RGB;

    void* srcBits = nullptr;
    HBITMAP srcDib = CreateDIBSection(hdc, &srcBmi, DIB_RGB_COLORS, &srcBits, nullptr, 0);
    if (!srcDib || !srcBits) {
        if (srcDib) {
            DeleteObject(srcDib);
        }
        return false;
    }
    std::memcpy(srcBits, srcPixels.data(), srcPixels.size() * sizeof(unsigned int));

    HDC memDc = CreateCompatibleDC(hdc);
    if (!memDc) {
        DeleteObject(srcDib);
        return false;
    }

    BLENDFUNCTION blend{};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;

    HGDIOBJ oldBitmap = SelectObject(memDc, srcDib);
    AlphaBlend(hdc, originX, originY, outWidth, outHeight, memDc, 0, 0, nativeWidth, nativeHeight, blend);
    SelectObject(memDc, oldBitmap);
    DeleteDC(memDc);
    DeleteObject(srcDib);
    return true;
}

void DrawOutlinedDigit(HDC hdc, int x, int y, const char* text, COLORREF color, int fontHeight)
{
    if (!hdc || !text || !*text) {
        return;
    }

    DrawDC drawDc(hdc);
    drawDc.SetFont(FONT_DEFAULT, fontHeight, 0);
    SetBkMode(hdc, TRANSPARENT);

    const int textLen = static_cast<int>(std::strlen(text));
    drawDc.SetTextColor(RGB(0, 0, 0));
    drawDc.TextOutA(x - 1, y, text, textLen);
    drawDc.TextOutA(x + 1, y, text, textLen);
    drawDc.TextOutA(x, y - 1, text, textLen);
    drawDc.TextOutA(x, y + 1, text, textLen);

    drawDc.SetTextColor(color);
    drawDc.TextOutA(x, y, text, textLen);
}

void ResolveLateralOffset(float rotationDegrees, float* outX, float* outZ)
{
    if (!outX || !outZ) {
        return;
    }

    *outX = 0.0f;
    *outZ = 0.0f;

    int dir = 0;
    int normalized = static_cast<int>(rotationDegrees);
    while (normalized < 0) {
        normalized += 360;
    }
    while (normalized >= 360) {
        normalized -= 360;
    }

    if (normalized < 90) {
        dir = 0;
    } else if (normalized < 180) {
        dir = 2;
    } else if (normalized < 270) {
        dir = 4;
    } else {
        dir = 6;
    }

    switch (dir) {
    case 0:
        *outX = -0.8f;
        *outZ = 0.8f;
        break;
    case 2:
        *outX = -0.8f;
        *outZ = -0.8f;
        break;
    case 4:
        *outX = 0.8f;
        *outZ = -0.8f;
        break;
    default:
        *outX = 0.8f;
        *outZ = 0.8f;
        break;
    }
}

} // namespace

CMsgEffect::CMsgEffect()
    : m_msgEffectType(0)
    , m_digit(0)
    , m_numberValue(0)
    , m_sprShift(0)
    , m_alpha(255)
    , m_masterGid(0)
    , m_colorArgb(0xFFFFFFFFu)
    , m_stateStartTick(timeGetTime())
    , m_pos{}
    , m_orgPos{}
    , m_destPos{}
    , m_destPos2{}
    , m_zoom(1.0f)
    , m_orgZoom(1.0f)
    , m_masterActor(nullptr)
    , m_isVisible(0)
    , m_isDisappear(0)
    , m_removedFromOwner(0)
{
}

CMsgEffect::~CMsgEffect()
{
    if (!m_removedFromOwner && m_masterActor) {
        m_masterActor->DeleteMatchingEffect(this);
    }
}

u8 CMsgEffect::OnProcess()
{
    if (m_isDisappear) {
        if (!m_removedFromOwner && m_masterActor) {
            m_masterActor->DeleteMatchingEffect(this);
            m_removedFromOwner = 1;
        }
        return 0;
    }

    const float stateCount = static_cast<float>(timeGetTime() - m_stateStartTick) * 0.041666668f;
    switch (m_msgEffectType) {
    case 14:
    case 21: {
        if (m_destPos.x == 0.0f && m_masterActor) {
            ResolveLateralOffset(m_masterActor->m_roty, &m_destPos.x, &m_destPos.z);
        }

        m_pos.x = m_orgPos.x + (stateCount * 0.33333334f + 3.0f) * m_destPos.x + m_destPos2.x;
        m_pos.z = m_orgPos.z + (stateCount * 0.33333334f + 3.0f) * m_destPos.z + m_destPos2.z;
        m_pos.y = m_orgPos.y + 8.0f - (2.0f - stateCount * 0.033333335f) * stateCount + m_destPos.y;
        m_zoom = stateCount >= 0.0f ? (std::max)(1.2f, m_orgZoom - stateCount * 0.23999999f) : m_orgZoom;
        m_alpha = (std::max)(0, static_cast<int>(250.0f - stateCount * 3.4f));
        if (stateCount >= 70.0f) {
            m_isDisappear = 1;
        }
        break;
    }
    case 16: {
        if (m_destPos.x == 0.0f && m_masterActor) {
            ResolveLateralOffset(m_masterActor->m_roty, &m_destPos.x, &m_destPos.z);
            m_destPos.x *= 0.625f;
            m_destPos.z *= 0.625f;
        }

        m_pos.x = m_orgPos.x + (stateCount * 0.33333334f + 3.0f) * m_destPos.x + m_destPos2.x;
        m_pos.z = m_orgPos.z + (stateCount * 0.33333334f + 3.0f) * m_destPos.z + m_destPos2.z;
        m_pos.y = m_orgPos.y + 8.0f - (2.0f - stateCount * 0.033333335f) * stateCount + m_destPos.y;
        m_zoom = stateCount >= 0.0f ? (std::max)(1.3f, m_orgZoom - stateCount * 0.23999999f) : m_orgZoom;
        m_alpha = (std::max)(0, static_cast<int>(250.0f - stateCount * 3.4f));
        if (stateCount >= 70.0f) {
            m_isDisappear = 1;
        }
        break;
    }
    case 22:
        m_pos.y = m_orgPos.y - stateCount * 0.1f;
        if (m_zoom < 3.0f) {
            m_zoom += stateCount * 0.18f;
            if (m_zoom > 3.0f) {
                m_zoom = 3.0f;
            }
        }
        m_alpha = stateCount > 90.0f
            ? (std::max)(0, 250 - static_cast<int>((stateCount - 90.0f) * 8.0f))
            : 250;
        if (stateCount >= 120.0f) {
            m_isDisappear = 1;
        }
        break;
    case 114:
    default:
        if (m_masterActor) {
            m_pos.x = m_masterActor->m_pos.x;
            m_pos.z = m_masterActor->m_pos.z;
        }
        m_pos.y = m_orgPos.y - stateCount * 0.54f;
        m_zoom = 1.0f;
        m_alpha = (std::max)(0, static_cast<int>(250.0f - stateCount * 3.0f));
        if (stateCount >= 80.0f) {
            m_isDisappear = 1;
        }
        break;
    }

    if (m_isDisappear && !m_removedFromOwner && m_masterActor) {
        m_masterActor->DeleteMatchingEffect(this);
        m_removedFromOwner = 1;
    }
    return m_isDisappear ? 0 : 1;
}

void CMsgEffect::SendMsg(CGameObject* sender, int msg, int par1, int par2, int par3)
{
    switch (msg) {
    case 22:
        m_masterGid = static_cast<u32>(par1);
        m_msgEffectType = par2;
        return;
    case 49:
        m_masterActor = nullptr;
        return;
    case 50:
        m_masterActor = dynamic_cast<CGameActor*>(sender);
        if (m_masterActor && m_msgEffectType == 114) {
            m_pos.x = m_masterActor->m_pos.x;
            m_pos.z = m_masterActor->m_pos.z;
        }
        return;
    case 53:
        m_isDisappear = 1;
        return;
    case 64: {
        const vector3d* pos = reinterpret_cast<const vector3d*>(static_cast<intptr_t>(par1));
        if (!pos) {
            m_isDisappear = 1;
            return;
        }

        m_isVisible = 1;
        m_stateStartTick = timeGetTime();
        m_numberValue = (std::max)(0, par2);
        m_digit = (std::max)(0, (std::min)(9, m_numberValue % 10));
        m_sprShift = par3;
        m_pos = *pos;
        m_orgPos = *pos;

        if (m_msgEffectType == 114) {
            m_pos.y -= 12.0f;
            m_orgPos.y = m_pos.y;
            m_zoom = 5.0f;
            m_orgZoom = 5.0f;
        } else {
            m_pos.y -= 15.0f;
            m_orgPos.y = m_pos.y;
            m_zoom = 5.0f;
            m_orgZoom = 5.0f;
        }
        return;
    }
    default:
        return;
    }
}

void CMsgEffect::Render(matrix* viewMatrix)
{
    if (!m_isVisible || !viewMatrix) {
        return;
    }

    tlvertex3d projected{};
    if (!ProjectMsgEffectPoint(*viewMatrix, m_pos, &projected)) {
        return;
    }

    const int digitCount = CountDamageDigits(m_numberValue);
    int digits[6] = {};
    int digitWidths[6] = {};
    int totalWidth = 0;
    const int digitGap = -2;

    for (int index = 0; index < digitCount; ++index) {
        digits[index] = ResolveDigitAtIndex(m_numberValue, digitCount - 1 - index);
        digitWidths[index] = ResolveDigitOutputWidth(digits[index], m_zoom);
        totalWidth += digitWidths[index];
    }
    if (digitCount > 1) {
        totalWidth += digitGap * (digitCount - 1);
    }

    int currentLeft = -totalWidth / 2;
    for (int index = 0; index < digitCount; ++index) {
        QueuedMsgEffectDraw draw{};
        draw.screenX = static_cast<int>(std::lround(projected.x));
        draw.screenY = static_cast<int>(std::lround(projected.y));
        draw.digit = digits[index];
        draw.colorArgb = m_colorArgb;
        draw.alpha = m_alpha;
        draw.zoom = m_zoom;
        draw.sprShift = currentLeft + digitWidths[index] / 2;
        g_queuedMsgEffects.push_back(draw);
        currentLeft += digitWidths[index] + digitGap;
    }
}

void DrawQueuedMsgEffects(HDC hdc)
{
    if (!hdc) {
        g_queuedMsgEffects.clear();
        return;
    }

    for (const QueuedMsgEffectDraw& draw : g_queuedMsgEffects) {
        if (!DrawDigitSprite(hdc, draw)) {
            const int fontHeight = (std::max)(14, (std::min)(32, static_cast<int>(std::lround(8.0f + draw.zoom * 4.0f))));
            char text[2] = {
                static_cast<char>('0' + (std::max)(0, (std::min)(9, draw.digit))),
                '\0'
            };
            DrawOutlinedDigit(hdc,
                draw.screenX + ResolveDigitShiftPixels(draw) - fontHeight / 4,
                draw.screenY - fontHeight / 2,
                text,
                ScaleColorByAlpha(draw.colorArgb, draw.alpha),
                fontHeight);
        }
    }

    g_queuedMsgEffects.clear();
}

void ClearQueuedMsgEffects()
{
    g_queuedMsgEffects.clear();
}

bool HasQueuedMsgEffects()
{
    return !g_queuedMsgEffects.empty();
}