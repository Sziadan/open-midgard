#include "CursorRenderer.h"

#include "DebugLog.h"
#include "GameMode.h"
#include "Mode.h"
#include "main/WinMain.h"
#include "res/ActRes.h"
#include "res/Sprite.h"
#include "session/Session.h"
#include "ui/UIShopCommon.h"

#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>

bool DrawModeCursorAtToHdc(HDC hdc, int x, int y, int cursorActNum, u32 mouseAnimStartTick);

namespace {

struct CursorResCache
{
    bool resolved;
    std::string sprName;
    std::string actName;
};

constexpr int kDragPreviewSize = 24;
constexpr int kDragPreviewOffsetX = 0;
constexpr int kDragPreviewOffsetY = 0;

struct DragPreviewPixels
{
    int width = 0;
    int height = 0;
    std::vector<unsigned int> pixels;
};

unsigned int PremultiplyCursorColor(unsigned int color);
void AlphaBlendCursorPixel(unsigned int& dst, unsigned int src);

CursorResCache ResolveCursorResources()
{
    CursorResCache cache{};
    cache.resolved = false;

    static const struct { const char* spr; const char* act; } kCandidates[] = {
        { "data\\sprite\\cursors.spr", "data\\sprite\\cursors.act" },
        { "data\\sprite\\interface\\cursors.spr", "data\\sprite\\interface\\cursors.act" },
        { nullptr, nullptr }
    };

    for (int i = 0; kCandidates[i].spr != nullptr; ++i) {
        DbgLog("[Cursor] Trying candidate %d: spr=%s\n", i, kCandidates[i].spr);
        CSprRes* spr = g_resMgr.GetAs<CSprRes>(kCandidates[i].spr);
        DbgLog("[Cursor]   spr result=%p\n", (void*)spr);
        DbgLog("[Cursor]   Trying act=%s\n", kCandidates[i].act);
        CActRes* act = g_resMgr.GetAs<CActRes>(kCandidates[i].act);
        DbgLog("[Cursor]   act result=%p\n", (void*)act);
        if (spr && act) {
            cache.resolved = true;
            cache.sprName = kCandidates[i].spr;
            cache.actName = kCandidates[i].act;
            DbgLog("[Cursor] RESOLVED: spr=%s act=%s\n", kCandidates[i].spr, kCandidates[i].act);
            return cache;
        }
        DbgLog("[Cursor] Not found: spr=%s act=%s\n", kCandidates[i].spr, kCandidates[i].act);
    }

    DbgLog("[Cursor] FAILED to resolve cursor assets (using fallback arrow).\n");
    return cache;
}

void DrawFallbackCursorAt(HDC hdc, int x, int y)
{
    if (!hdc) {
        return;
    }

    HCURSOR cursor = LoadCursor(nullptr, IDC_ARROW);
    if (cursor) {
        DrawIconEx(hdc, x, y, cursor, 0, 0, 0, nullptr, DI_NORMAL);
    }
}

bool DrawFallbackCursorToArgb(unsigned int* dest, int destW, int destH, int x, int y)
{
    if (!dest || destW <= 0 || destH <= 0) {
        return false;
    }

    static const char* const kFallbackCursorMask[] = {
        "X.............",
        "XX............",
        "XOX...........",
        "XOOX..........",
        "XOOOX.........",
        "XOOOOX........",
        "XOOOOOX.......",
        "XOOOOOOX......",
        "XOOOOOOOX.....",
        "XOOOOOOOOX....",
        "XOOOOOOOOOX...",
        "XOOOOOOOOOOX..",
        "XOOOOXXXXXXXX.",
        "XOOXOX........",
        "XOX..OX.......",
        "XX....OX......",
        "X......OX.....",
        "........X....."
    };

    constexpr int kMaskHeight = static_cast<int>(sizeof(kFallbackCursorMask) / sizeof(kFallbackCursorMask[0]));
    constexpr unsigned int kOutlineColor = 0xFF000000u;
    constexpr unsigned int kFillColor = 0xFFF6F6F6u;

    for (int row = 0; row < kMaskHeight; ++row) {
        const char* maskRow = kFallbackCursorMask[row];
        for (int col = 0; maskRow[col] != '\0'; ++col) {
            const int destX = x + col;
            const int destY = y + row;
            if (destX < 0 || destX >= destW || destY < 0 || destY >= destH) {
                continue;
            }

            unsigned int srcColor = 0u;
            if (maskRow[col] == 'X') {
                srcColor = kOutlineColor;
            } else if (maskRow[col] == 'O') {
                srcColor = kFillColor;
            } else {
                continue;
            }

            AlphaBlendCursorPixel(
                dest[static_cast<size_t>(destY) * static_cast<size_t>(destW) + static_cast<size_t>(destX)],
                PremultiplyCursorColor(srcColor));
        }
    }

    return true;
}

bool GetClientCursorPosInternal(POINT* outPoint)
{
    if (!outPoint || !g_hMainWnd) {
        return false;
    }

    POINT pt{};
    if (!GetCursorPos(&pt) || !ScreenToClient(g_hMainWnd, &pt)) {
        return false;
    }

    *outPoint = pt;
    return true;
}

std::string ResolveShortcutDragSkillIconPath(int skillId)
{
    g_skillMgr.EnsureLoaded();
    const SkillMetadata* metadata = g_skillMgr.GetSkillMetadata(skillId);
    if (metadata && !metadata->skillIdName.empty()) {
        const std::string lowered = shopui::ToLowerAscii(metadata->skillIdName);
        const std::string direct = "texture\\\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\item\\" + lowered + ".bmp";
        const std::string dataPath = "data\\" + direct;
        if (g_fileMgr.IsDataExist(direct.c_str())) {
            return direct;
        }
        if (g_fileMgr.IsDataExist(dataPath.c_str())) {
            return dataPath;
        }
        return direct;
    }
    return g_skillMgr.GetSkillIconPath(skillId);
}

bool ResolveDragPreviewCacheKey(const DRAG_INFO& dragInfo, unsigned long long* outKey)
{
    if (!outKey) {
        return false;
    }

    if (dragInfo.type == static_cast<int>(DragType::ShortcutItem) && dragInfo.itemId != 0) {
        *outKey = (1ull << 32) | static_cast<unsigned long long>(dragInfo.itemId);
        return true;
    }
    if (dragInfo.type == static_cast<int>(DragType::ShortcutSkill) && dragInfo.skillId != 0) {
        *outKey = (2ull << 32) | static_cast<unsigned long long>(static_cast<unsigned int>(dragInfo.skillId));
        return true;
    }

    *outKey = 0;
    return false;
}

HBITMAP LoadDragPreviewBitmap(const DRAG_INFO& dragInfo)
{
    static std::unordered_map<unsigned long long, HBITMAP> s_dragPreviewCache;

    unsigned long long cacheKey = 0;
    if (!ResolveDragPreviewCacheKey(dragInfo, &cacheKey)) {
        return nullptr;
    }

    const auto found = s_dragPreviewCache.find(cacheKey);
    if (found != s_dragPreviewCache.end()) {
        return found->second;
    }

    HBITMAP bitmap = nullptr;
    if (dragInfo.type == static_cast<int>(DragType::ShortcutItem) && dragInfo.itemId != 0) {
        ITEM_INFO item{};
        item.SetItemId(dragInfo.itemId);
        item.m_isIdentified = 1;
        for (const std::string& candidate : shopui::BuildItemIconCandidates(item)) {
            if (!g_fileMgr.IsDataExist(candidate.c_str())) {
                continue;
            }
            bitmap = shopui::LoadBitmapFromGameData(candidate);
            if (bitmap) {
                break;
            }
        }
    } else if (dragInfo.type == static_cast<int>(DragType::ShortcutSkill) && dragInfo.skillId != 0) {
        const std::string path = ResolveShortcutDragSkillIconPath(dragInfo.skillId);
        if (!path.empty() && g_fileMgr.IsDataExist(path.c_str())) {
            bitmap = shopui::LoadBitmapFromGameData(path);
        }
    }

    s_dragPreviewCache[cacheKey] = bitmap;
    return bitmap;
}

bool LoadDragPreviewPixels(const DRAG_INFO& dragInfo, DragPreviewPixels* outPreview)
{
    if (!outPreview) {
        return false;
    }

    static std::unordered_map<unsigned long long, DragPreviewPixels> s_dragPreviewCache;

    unsigned long long cacheKey = 0;
    if (!ResolveDragPreviewCacheKey(dragInfo, &cacheKey)) {
        return false;
    }

    const auto found = s_dragPreviewCache.find(cacheKey);
    if (found != s_dragPreviewCache.end()) {
        *outPreview = found->second;
        return !outPreview->pixels.empty();
    }

    std::string bitmapPath;
    if (dragInfo.type == static_cast<int>(DragType::ShortcutItem) && dragInfo.itemId != 0) {
        ITEM_INFO item{};
        item.SetItemId(dragInfo.itemId);
        item.m_isIdentified = 1;
        for (const std::string& candidate : shopui::BuildItemIconCandidates(item)) {
            if (g_fileMgr.IsDataExist(candidate.c_str())) {
                bitmapPath = candidate;
                break;
            }
        }
    } else if (dragInfo.type == static_cast<int>(DragType::ShortcutSkill) && dragInfo.skillId != 0) {
        const std::string path = ResolveShortcutDragSkillIconPath(dragInfo.skillId);
        if (!path.empty() && g_fileMgr.IsDataExist(path.c_str())) {
            bitmapPath = path;
        }
    }

    DragPreviewPixels preview{};
    if (!bitmapPath.empty()) {
        u32* pixels = nullptr;
        if (LoadBgraPixelsFromGameData(bitmapPath.c_str(), &pixels, &preview.width, &preview.height)
            && pixels
            && preview.width > 0
            && preview.height > 0) {
            preview.pixels.assign(
                pixels,
                pixels + static_cast<size_t>(preview.width) * static_cast<size_t>(preview.height));
        }
        delete[] pixels;
    }

    s_dragPreviewCache[cacheKey] = preview;
    *outPreview = preview;
    return !outPreview->pixels.empty();
}

void DrawDragPreviewAt(HDC hdc, int x, int y)
{
    if (!hdc) {
        return;
    }

    const CGameMode* gameMode = g_modeMgr.GetCurrentGameMode();
    if (!gameMode || gameMode->m_dragType == static_cast<int>(DragType::None)) {
        return;
    }

    HBITMAP bitmap = LoadDragPreviewBitmap(gameMode->m_dragInfo);
    if (!bitmap) {
        return;
    }

    RECT dst{
        x + kDragPreviewOffsetX - (kDragPreviewSize / 2),
        y + kDragPreviewOffsetY - (kDragPreviewSize / 2),
        x + kDragPreviewOffsetX + (kDragPreviewSize / 2),
        y + kDragPreviewOffsetY + (kDragPreviewSize / 2)
    };
    shopui::DrawBitmapTransparent(hdc, bitmap, dst);
}

bool DrawDragPreviewToArgb(unsigned int* dest, int destW, int destH, int x, int y)
{
    if (!dest || destW <= 0 || destH <= 0) {
        return false;
    }

    const CGameMode* gameMode = g_modeMgr.GetCurrentGameMode();
    if (!gameMode || gameMode->m_dragType == static_cast<int>(DragType::None)) {
        return false;
    }

    DragPreviewPixels preview{};
    if (!LoadDragPreviewPixels(gameMode->m_dragInfo, &preview)
        || preview.width <= 0
        || preview.height <= 0
        || preview.pixels.empty()) {
        return false;
    }

    const int destLeft = x + kDragPreviewOffsetX - (kDragPreviewSize / 2);
    const int destTop = y + kDragPreviewOffsetY - (kDragPreviewSize / 2);
    for (int dy = 0; dy < kDragPreviewSize; ++dy) {
        const int destY = destTop + dy;
        if (destY < 0 || destY >= destH) {
            continue;
        }

        const int srcY = dy * preview.height / kDragPreviewSize;
        for (int dx = 0; dx < kDragPreviewSize; ++dx) {
            const int destX = destLeft + dx;
            if (destX < 0 || destX >= destW) {
                continue;
            }

            const int srcX = dx * preview.width / kDragPreviewSize;
            unsigned int srcColor = preview.pixels[static_cast<size_t>(srcY) * static_cast<size_t>(preview.width) + static_cast<size_t>(srcX)];
            if ((srcColor & 0x00FFFFFFu) == 0x00FF00FFu) {
                continue;
            }
            if (((srcColor >> 24) & 0xFFu) == 0u) {
                srcColor |= 0xFF000000u;
            }
            AlphaBlendCursorPixel(
                dest[static_cast<size_t>(destY) * static_cast<size_t>(destW) + static_cast<size_t>(destX)],
                PremultiplyCursorColor(srcColor));
        }
    }

    return true;
}

u32 GetDragPreviewVisualToken()
{
    const CGameMode* gameMode = g_modeMgr.GetCurrentGameMode();
    if (!gameMode) {
        return 0u;
    }

    u32 token = static_cast<u32>(gameMode->m_dragType & 0xFF);
    if (gameMode->m_dragType == static_cast<int>(DragType::ShortcutItem)) {
        token = (token * 16777619u) ^ static_cast<u32>(gameMode->m_dragInfo.itemId);
    } else if (gameMode->m_dragType == static_cast<int>(DragType::ShortcutSkill)) {
        token = (token * 16777619u) ^ static_cast<u32>(gameMode->m_dragInfo.skillId);
        token = (token * 16777619u) ^ static_cast<u32>(gameMode->m_dragInfo.skillLevel);
    }
    return token;
}

unsigned int PremultiplyCursorColor(unsigned int color)
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

unsigned int ModulateCursorColor(unsigned int srcColor, const CSprClip& clip, unsigned int globalColor)
{
    const unsigned int srcA = (srcColor >> 24) & 0xFFu;
    const unsigned int srcR = (srcColor >> 16) & 0xFFu;
    const unsigned int srcG = (srcColor >> 8) & 0xFFu;
    const unsigned int srcB = srcColor & 0xFFu;

    const unsigned int clipA = static_cast<unsigned int>(clip.a) * ((globalColor >> 24) & 0xFFu) / 255u;
    const unsigned int clipR = static_cast<unsigned int>(clip.r) * ((globalColor >> 16) & 0xFFu) / 255u;
    const unsigned int clipG = static_cast<unsigned int>(clip.g) * ((globalColor >> 8) & 0xFFu) / 255u;
    const unsigned int clipB = static_cast<unsigned int>(clip.b) * (globalColor & 0xFFu) / 255u;

    const unsigned int outA = srcA * clipA / 255u;
    const unsigned int outR = srcR * clipR / 255u;
    const unsigned int outG = srcG * clipG / 255u;
    const unsigned int outB = srcB * clipB / 255u;
    return (outA << 24) | (outR << 16) | (outG << 8) | outB;
}

void AlphaBlendCursorPixel(unsigned int& dst, unsigned int src)
{
    const unsigned int srcA = (src >> 24) & 0xFFu;
    if (srcA == 0u) {
        return;
    }
    if (srcA == 0xFFu) {
        dst = src;
        return;
    }

    const unsigned int dstA = (dst >> 24) & 0xFFu;
    const unsigned int srcR = (src >> 16) & 0xFFu;
    const unsigned int srcG = (src >> 8) & 0xFFu;
    const unsigned int srcB = src & 0xFFu;
    const unsigned int dstR = (dst >> 16) & 0xFFu;
    const unsigned int dstG = (dst >> 8) & 0xFFu;
    const unsigned int dstB = dst & 0xFFu;

    const unsigned int invA = 255u - srcA;
    const unsigned int outA = srcA + (dstA * invA) / 255u;
    const unsigned int outR = srcR + (dstR * invA) / 255u;
    const unsigned int outG = srcG + (dstG * invA) / 255u;
    const unsigned int outB = srcB + (dstB * invA) / 255u;
    dst = (outA << 24) | (outR << 16) | (outG << 8) | outB;
}

bool ComputeCursorMotionBounds(CSprRes* sprRes, const CMotion* motion, RECT* outBounds)
{
    if (!sprRes || !motion || !outBounds) {
        return false;
    }

    bool hasBounds = false;
    float minX = 0.0f;
    float minY = 0.0f;
    float maxX = 0.0f;
    float maxY = 0.0f;
    for (const CSprClip& clip : motion->sprClips) {
        const SprImg* image = sprRes->GetSprite(clip.clipType, clip.sprIndex);
        if (!image || image->width <= 0 || image->height <= 0) {
            continue;
        }

        const float clipZoomX = clip.zoomX > 0.0f ? clip.zoomX : 1.0f;
        const float clipZoomY = clip.zoomY > 0.0f ? clip.zoomY : 1.0f;
        const float drawWidth = static_cast<float>(image->width * (image->isHalfW + 1));
        const float drawHeight = static_cast<float>(image->height * (image->isHalfH + 1));
        float x1 = static_cast<float>(clip.x) * clipZoomX;
        float x2 = (drawWidth + static_cast<float>(clip.x) - 1.0f) * clipZoomX;
        if ((clip.flags & 1) != 0) {
            std::swap(x1, x2);
        }
        const float y1 = static_cast<float>(clip.y) * clipZoomY;
        const float y2 = (drawHeight + static_cast<float>(clip.y) - 1.0f) * clipZoomY;

        const float clipMinX = (std::min)(x1, x2);
        const float clipMaxX = (std::max)(x1, x2) + 1.0f;
        const float clipMinY = (std::min)(y1, y2);
        const float clipMaxY = (std::max)(y1, y2) + 1.0f;
        if (!hasBounds) {
            minX = clipMinX;
            minY = clipMinY;
            maxX = clipMaxX;
            maxY = clipMaxY;
            hasBounds = true;
        } else {
            minX = (std::min)(minX, clipMinX);
            minY = (std::min)(minY, clipMinY);
            maxX = (std::max)(maxX, clipMaxX);
            maxY = (std::max)(maxY, clipMaxY);
        }
    }

    if (!hasBounds) {
        return false;
    }

    outBounds->left = static_cast<LONG>(std::floor(minX));
    outBounds->top = static_cast<LONG>(std::floor(minY));
    outBounds->right = static_cast<LONG>(std::ceil(maxX));
    outBounds->bottom = static_cast<LONG>(std::ceil(maxY));
    return outBounds->right > outBounds->left && outBounds->bottom > outBounds->top;
}

void BlitCursorMotionToArgb(unsigned int* dest,
    int destW,
    int destH,
    int baseX,
    int baseY,
    CSprRes* sprRes,
    const CMotion* motion,
    unsigned int* palette,
    unsigned int globalColor)
{
    if (!dest || !sprRes || !motion || !palette) {
        return;
    }

    for (const CSprClip& clip : motion->sprClips) {
        const SprImg* image = sprRes->GetSprite(clip.clipType, clip.sprIndex);
        if (!image || image->width <= 0 || image->height <= 0) {
            continue;
        }

        const float clipZoomX = clip.zoomX > 0.0f ? clip.zoomX : 1.0f;
        const float clipZoomY = clip.zoomY > 0.0f ? clip.zoomY : 1.0f;
        const int logicalWidth = image->width * (image->isHalfW + 1);
        const int logicalHeight = image->height * (image->isHalfH + 1);
        if (logicalWidth <= 0 || logicalHeight <= 0) {
            continue;
        }

        float x1 = static_cast<float>(clip.x) * clipZoomX;
        float x2 = (static_cast<float>(logicalWidth + clip.x) - 1.0f) * clipZoomX;
        const float y1 = static_cast<float>(clip.y) * clipZoomY;
        const float y2 = (static_cast<float>(logicalHeight + clip.y) - 1.0f) * clipZoomY;
        const bool flipX = (clip.flags & 1) != 0;
        if (flipX) {
            std::swap(x1, x2);
        }

        const float minX = (std::min)(x1, x2);
        const float minY = (std::min)(y1, y2);
        const int drawLeft = static_cast<int>(std::floor(minX));
        const int drawTop = static_cast<int>(std::floor(minY));
        const int drawRight = static_cast<int>(std::ceil((std::max)(x1, x2) + 1.0f));
        const int drawBottom = static_cast<int>(std::ceil((std::max)(y1, y2) + 1.0f));

        for (int dy = drawTop; dy < drawBottom; ++dy) {
            const int destY = baseY + dy;
            if (destY < 0 || destY >= destH) {
                continue;
            }

            const int logicalY = static_cast<int>(std::floor((static_cast<float>(dy) + 0.5f - minY) / clipZoomY));
            if (logicalY < 0 || logicalY >= logicalHeight) {
                continue;
            }
            const int srcY = logicalY / (image->isHalfH + 1);

            for (int dx = drawLeft; dx < drawRight; ++dx) {
                const int destX = baseX + dx;
                if (destX < 0 || destX >= destW) {
                    continue;
                }

                int logicalX = static_cast<int>(std::floor((static_cast<float>(dx) + 0.5f - minX) / clipZoomX));
                if (logicalX < 0 || logicalX >= logicalWidth) {
                    continue;
                }
                if (flipX) {
                    logicalX = logicalWidth - 1 - logicalX;
                }
                const int srcX = logicalX / (image->isHalfW + 1);

                unsigned int srcColor = 0u;
                if (clip.clipType == 0) {
                    const unsigned char index = image->indices[static_cast<size_t>(srcY) * static_cast<size_t>(image->width) + static_cast<size_t>(srcX)];
                    if (index == 0u) {
                        continue;
                    }
                    srcColor = 0xFF000000u | (palette[index] & 0x00FFFFFFu);
                } else {
                    srcColor = image->rgba[static_cast<size_t>(srcY) * static_cast<size_t>(image->width) + static_cast<size_t>(srcX)];
                    if (((srcColor >> 24) & 0xFFu) == 0u) {
                        continue;
                    }
                }

                srcColor = ModulateCursorColor(srcColor, clip, globalColor);
                srcColor = PremultiplyCursorColor(srcColor);
                AlphaBlendCursorPixel(dest[static_cast<size_t>(destY) * static_cast<size_t>(destW) + static_cast<size_t>(destX)], srcColor);
            }
        }
    }
}

bool DrawCursorMotionToHdc(HDC hdc, int x, int y, CSprRes* sprRes, const CMotion* motion, unsigned int* palette, unsigned int globalColor)
{
    RECT bounds{};
    if (!hdc || !ComputeCursorMotionBounds(sprRes, motion, &bounds)) {
        return false;
    }

    const int width = (std::max)(1, static_cast<int>(bounds.right - bounds.left));
    const int height = (std::max)(1, static_cast<int>(bounds.bottom - bounds.top));
    std::vector<unsigned int> pixels(static_cast<size_t>(width) * static_cast<size_t>(height), 0u);
    BlitCursorMotionToArgb(pixels.data(), width, height, -bounds.left, -bounds.top, sprRes, motion, palette, globalColor);

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* dibBits = nullptr;
    HBITMAP dib = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &dibBits, nullptr, 0);
    if (!dib || !dibBits) {
        if (dib) {
            DeleteObject(dib);
        }
        return false;
    }

    std::memcpy(dibBits, pixels.data(), pixels.size() * sizeof(unsigned int));

    HDC memDc = CreateCompatibleDC(hdc);
    if (!memDc) {
        DeleteObject(dib);
        return false;
    }

    BLENDFUNCTION blend{};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;

    // Cursor ACT motions are authored around a hotspot-relative local origin.
    // When the motion bounds start entirely in positive space, placing the
    // bitmap at `x + bounds.left/y + bounds.top` shifts the visible cursor
    // away from the real OS cursor. Clamp that authored positive offset back
    // out so the software cursor lands on the same hotspot.
    const int hotspotOffsetX = (std::max)(0, static_cast<int>(bounds.left));
    const int hotspotOffsetY = (std::max)(0, static_cast<int>(bounds.top));

    HGDIOBJ oldBitmap = SelectObject(memDc, dib);
    AlphaBlend(hdc,
        x + bounds.left - hotspotOffsetX,
        y + bounds.top - hotspotOffsetY,
        width,
        height,
        memDc,
        0,
        0,
        width,
        height,
        blend);
    SelectObject(memDc, oldBitmap);
    DeleteDC(memDc);
    DeleteObject(dib);
    return true;
}

bool DrawCursorMotionToArgb(unsigned int* dest,
    int destW,
    int destH,
    int x,
    int y,
    CSprRes* sprRes,
    const CMotion* motion,
    unsigned int* palette,
    unsigned int globalColor)
{
    RECT bounds{};
    if (!dest || destW <= 0 || destH <= 0 || !ComputeCursorMotionBounds(sprRes, motion, &bounds)) {
        return false;
    }

    const int hotspotOffsetX = (std::max)(0, static_cast<int>(bounds.left));
    const int hotspotOffsetY = (std::max)(0, static_cast<int>(bounds.top));
    BlitCursorMotionToArgb(dest,
        destW,
        destH,
        x + bounds.left - hotspotOffsetX,
        y + bounds.top - hotspotOffsetY,
        sprRes,
        motion,
        palette,
        globalColor);
    return true;
}

bool DrawResolvedCursorActionAt(HDC hdc, int x, int y, int cursorActNum, u32 mouseAnimStartTick, const CursorResCache& cache)
{
    CActRes* actRes = cache.resolved ? g_resMgr.GetAs<CActRes>(cache.actName.c_str()) : nullptr;
    CSprRes* sprRes = cache.resolved ? g_resMgr.GetAs<CSprRes>(cache.sprName.c_str()) : nullptr;
    if (!actRes || !sprRes) {
        return false;
    }

    int action = cursorActNum;
    if (action < 0 || action >= static_cast<int>(actRes->actions.size())) {
        action = 0;
    }

    const int motionCount = actRes->GetMotionCount(action);
    if (motionCount <= 0) {
        return false;
    }

    const unsigned int elapsed = GetTickCount() - mouseAnimStartTick;
    const float stateTicks = static_cast<float>(elapsed) * 0.041666668f;
    const float motionDelay = (std::max)(0.0001f, actRes->GetDelay(action));
    const int motionIndex = static_cast<int>(stateTicks / motionDelay) % motionCount;
    const CMotion* motion = actRes->GetMotion(action, motionIndex);
    return motion ? DrawCursorMotionToHdc(hdc, x, y, sprRes, motion, sprRes->m_pal, 0xFAFFFFFFu) : false;
}

bool DrawResolvedCursorActionToArgb(unsigned int* dest,
    int destW,
    int destH,
    int x,
    int y,
    int cursorActNum,
    u32 mouseAnimStartTick,
    const CursorResCache& cache)
{
    CSprRes* sprRes = cache.resolved ? g_resMgr.GetAs<CSprRes>(cache.sprName.c_str()) : nullptr;
    CActRes* actRes = cache.resolved ? g_resMgr.GetAs<CActRes>(cache.actName.c_str()) : nullptr;
    if (!actRes || !sprRes) {
        return false;
    }

    int action = cursorActNum;
    if (action < 0 || action >= static_cast<int>(actRes->actions.size())) {
        action = 0;
    }

    const int motionCount = actRes->GetMotionCount(action);
    if (motionCount <= 0) {
        return false;
    }

    const unsigned int elapsed = GetTickCount() - mouseAnimStartTick;
    const float stateTicks = static_cast<float>(elapsed) * 0.041666668f;
    const float motionDelay = (std::max)(0.0001f, actRes->GetDelay(action));
    const int motionIndex = static_cast<int>(stateTicks / motionDelay) % motionCount;
    const CMotion* motion = actRes->GetMotion(action, motionIndex);
    return motion ? DrawCursorMotionToArgb(dest, destW, destH, x, y, sprRes, motion, sprRes->m_pal, 0xFAFFFFFFu) : false;
}

u32 GetCursorActionVisualFrame(int cursorActNum, u32 mouseAnimStartTick, const CursorResCache& cache)
{
    CActRes* actRes = cache.resolved ? g_resMgr.GetAs<CActRes>(cache.actName.c_str()) : nullptr;
    if (!actRes) {
        return 0u;
    }

    int action = cursorActNum;
    if (action < 0 || action >= static_cast<int>(actRes->actions.size())) {
        action = 0;
    }

    const int motionCount = actRes->GetMotionCount(action);
    if (motionCount <= 0) {
        return static_cast<u32>(action & 0xFFFF);
    }

    const unsigned int elapsed = GetTickCount() - mouseAnimStartTick;
    const float stateTicks = static_cast<float>(elapsed) * 0.041666668f;
    const float motionDelay = (std::max)(0.0001f, actRes->GetDelay(action));
    const int motionIndex = static_cast<int>(stateTicks / motionDelay) % motionCount;
    return (static_cast<u32>(action & 0xFFFF) << 16) | static_cast<u32>(motionIndex & 0xFFFF);
}

} // namespace

bool DrawModeCursorToHdc(HDC hdc, int cursorActNum, u32 mouseAnimStartTick)
{
    POINT pt{};
    if (!GetClientCursorPosInternal(&pt)) {
        return false;
    }
    return DrawModeCursorAtToHdc(hdc, pt.x, pt.y, cursorActNum, mouseAnimStartTick);
}

bool DrawModeCursorAtToHdc(HDC hdc, int x, int y, int cursorActNum, u32 mouseAnimStartTick)
{
    if (!g_hMainWnd || !hdc) {
        return false;
    }

    static bool s_cacheInit = false;
    static CursorResCache s_cache{};
    if (!s_cacheInit) {
        DbgLog("[Cursor] Resolving cursor resources...\n");
        s_cache = ResolveCursorResources();
        DbgLog("[Cursor] resolved=%d spr='%s' act='%s'\n",
            (int)s_cache.resolved, s_cache.sprName.c_str(), s_cache.actName.c_str());
        s_cacheInit = true;
    }

    bool drewCustomCursor = false;
    if (cursorActNum == static_cast<int>(CursorAction::Forbidden)) {
        drewCustomCursor = DrawResolvedCursorActionAt(hdc, x, y, static_cast<int>(CursorAction::Arrow), mouseAnimStartTick, s_cache);
        drewCustomCursor = DrawResolvedCursorActionAt(hdc, x, y, cursorActNum, mouseAnimStartTick, s_cache) || drewCustomCursor;
    } else {
        drewCustomCursor = DrawResolvedCursorActionAt(hdc, x, y, cursorActNum, mouseAnimStartTick, s_cache);
    }

    if (!drewCustomCursor) {
        DrawFallbackCursorAt(hdc, x, y);
    }

    DrawDragPreviewAt(hdc, x, y);

    return drewCustomCursor;
}

bool DrawModeCursorAtToArgb(unsigned int* dest, int destW, int destH, int x, int y, int cursorActNum, u32 mouseAnimStartTick)
{
    if (!dest || destW <= 0 || destH <= 0 || !g_hMainWnd) {
        return false;
    }

    static bool s_cacheInit = false;
    static CursorResCache s_cache{};
    if (!s_cacheInit) {
        s_cache = ResolveCursorResources();
        s_cacheInit = true;
    }

    bool drewCustomCursor = false;
    if (cursorActNum == static_cast<int>(CursorAction::Forbidden)) {
        drewCustomCursor = DrawResolvedCursorActionToArgb(dest, destW, destH, x, y, static_cast<int>(CursorAction::Arrow), mouseAnimStartTick, s_cache);
        drewCustomCursor = DrawResolvedCursorActionToArgb(dest, destW, destH, x, y, cursorActNum, mouseAnimStartTick, s_cache) || drewCustomCursor;
    } else {
        drewCustomCursor = DrawResolvedCursorActionToArgb(dest, destW, destH, x, y, cursorActNum, mouseAnimStartTick, s_cache);
    }

    if (!drewCustomCursor) {
        drewCustomCursor = DrawFallbackCursorToArgb(dest, destW, destH, x, y);
    }

    DrawDragPreviewToArgb(dest, destW, destH, x, y);

    return drewCustomCursor;
}

bool GetModeCursorClientPos(POINT* outPoint)
{
    return GetClientCursorPosInternal(outPoint);
}

u32 GetModeCursorVisualFrame(int cursorActNum, u32 mouseAnimStartTick)
{
    static bool s_cacheInit = false;
    static CursorResCache s_cache{};
    if (!s_cacheInit) {
        s_cache = ResolveCursorResources();
        s_cacheInit = true;
    }

    if (cursorActNum == static_cast<int>(CursorAction::Forbidden)) {
        const u32 arrowFrame = GetCursorActionVisualFrame(static_cast<int>(CursorAction::Arrow), mouseAnimStartTick, s_cache);
        const u32 forbiddenFrame = GetCursorActionVisualFrame(cursorActNum, mouseAnimStartTick, s_cache);
        return ((arrowFrame * 16777619u) ^ forbiddenFrame) ^ GetDragPreviewVisualToken();
    }

    return GetCursorActionVisualFrame(cursorActNum, mouseAnimStartTick, s_cache) ^ GetDragPreviewVisualToken();
}

void DrawModeCursor(int cursorActNum, u32 mouseAnimStartTick)
{
    if (!g_hMainWnd) {
        return;
    }

    HDC hdc = GetDC(g_hMainWnd);
    if (!hdc) {
        return;
    }

    DrawModeCursorToHdc(hdc, cursorActNum, mouseAnimStartTick);

    ReleaseDC(g_hMainWnd, hdc);
}