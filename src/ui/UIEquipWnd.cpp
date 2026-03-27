#include "UIEquipWnd.h"

#include "gamemode/GameMode.h"
#include "gamemode/Mode.h"
#include "UIWindowMgr.h"
#include "core/File.h"
#include "item/Item.h"
#include "main/WinMain.h"
#include "render/DC.h"
#include "render3d/Device.h"
#include "res/ActRes.h"
#include "res/ImfRes.h"
#include "res/PaletteRes.h"
#include "res/Sprite.h"
#include "res/Texture.h"
#include "session/Session.h"
#include "world/GameActor.h"
#include "world/World.h"

#include <gdiplus.h>
#include <windows.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <list>
#include <string>
#include <vector>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "msimg32.lib")

namespace {

constexpr int kWindowWidth = 280;
constexpr int kWindowHeight = 206;
constexpr int kMiniHeight = 34;
constexpr int kTitleBarHeight = 17;
constexpr int kButtonIdBase = 134;
constexpr int kButtonIdClose = 135;
constexpr int kButtonIdMini = 136;
constexpr const char* kUiKorPrefix =
    "texture\\"
    "\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA"
    "\\";

constexpr int kSlotIconSize = 24;
constexpr int kCenterPanelLeft = 98;
constexpr int kCenterPanelRight = 182;
constexpr int kCenterPanelTop = 32;
constexpr int kCenterPanelBottom = 188;

struct EquipSlotDefLocal {
    int wearMask;
    int iconX;
    int iconY;
};

constexpr std::array<EquipSlotDefLocal, 10> kEquipSlots = {{
    { 256, 8, 19 },    // head upper
    { 1, 8, 45 },      // head lower
    { 4, 8, 97 },      // garment
    { 8, 8, 123 },     // accessory left
    { 512, 248, 19 },  // head mid
    { 16, 248, 45 },   // armor
    { 32, 248, 71 },   // shield
    { 64, 248, 97 },   // shoes
    { 128, 248, 123 }, // accessory right
    { 2, 8, 71 },      // weapon
}};

ULONG_PTR EnsureGdiplusStarted()
{
    static ULONG_PTR s_token = 0;
    static bool s_started = false;
    if (!s_started) {
        Gdiplus::GdiplusStartupInput startupInput;
        if (Gdiplus::GdiplusStartup(&s_token, &startupInput, nullptr) == Gdiplus::Ok) {
            s_started = true;
        }
    }
    return s_token;
}

std::string ToLowerAscii(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        if (ch >= 'A' && ch <= 'Z') {
            return static_cast<char>(ch - 'A' + 'a');
        }
        return static_cast<char>(ch);
    });
    return value;
}

std::string NormalizeSlash(std::string value)
{
    std::replace(value.begin(), value.end(), '/', '\\');
    return value;
}

void AddUniqueCandidate(std::vector<std::string>& out, const std::string& raw)
{
    if (raw.empty()) {
        return;
    }

    const std::string normalized = NormalizeSlash(raw);
    const std::string lowered = ToLowerAscii(normalized);
    for (const std::string& existing : out) {
        if (ToLowerAscii(existing) == lowered) {
            return;
        }
    }
    out.push_back(normalized);
}

std::vector<std::string> BuildUiAssetCandidates(const char* fileName)
{
    std::vector<std::string> out;
    if (!fileName || !*fileName) {
        return out;
    }

    const char* prefixes[] = {
        "",
        "skin\\default\\",
        "skin\\default\\basic_interface\\",
        "texture\\",
        "texture\\interface\\",
        "texture\\interface\\basic_interface\\",
        "data\\",
        "data\\texture\\",
        "data\\texture\\interface\\",
        "data\\texture\\interface\\basic_interface\\",
        nullptr
    };

    std::string base = NormalizeSlash(fileName);
    AddUniqueCandidate(out, base);

    std::string filenameOnly = base;
    const size_t slashPos = filenameOnly.find_last_of('\\');
    if (slashPos != std::string::npos && slashPos + 1 < filenameOnly.size()) {
        filenameOnly = filenameOnly.substr(slashPos + 1);
    }

    for (int index = 0; prefixes[index]; ++index) {
        AddUniqueCandidate(out, std::string(prefixes[index]) + filenameOnly);
    }

    return out;
}

std::string ResolveUiAssetPath(const char* fileName)
{
    for (const std::string& candidate : BuildUiAssetCandidates(fileName)) {
        if (g_fileMgr.IsDataExist(candidate.c_str())) {
            return candidate;
        }
    }
    return NormalizeSlash(fileName ? fileName : "");
}

HBITMAP LoadBitmapFromGameData(const std::string& path)
{
    if (path.empty() || !EnsureGdiplusStarted()) {
        return nullptr;
    }

    int size = 0;
    unsigned char* bytes = g_fileMgr.GetData(path.c_str(), &size);
    if (!bytes || size <= 0) {
        delete[] bytes;
        return nullptr;
    }

    HBITMAP outBitmap = nullptr;
    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, static_cast<SIZE_T>(size));
    if (mem) {
        void* dst = GlobalLock(mem);
        if (dst) {
            std::memcpy(dst, bytes, static_cast<size_t>(size));
            GlobalUnlock(mem);

            IStream* stream = nullptr;
            if (CreateStreamOnHGlobal(mem, TRUE, &stream) == S_OK) {
                auto* bitmap = Gdiplus::Bitmap::FromStream(stream, FALSE);
                if (bitmap && bitmap->GetLastStatus() == Gdiplus::Ok) {
                    bitmap->GetHBITMAP(RGB(0, 0, 0), &outBitmap);
                }
                delete bitmap;
                stream->Release();
            } else {
                GlobalFree(mem);
            }
        } else {
            GlobalFree(mem);
        }
    }

    delete[] bytes;
    return outBitmap;
}

void DrawBitmapTransparent(HDC target, HBITMAP bitmap, const RECT& dst)
{
    if (!target || !bitmap) {
        return;
    }

    BITMAP bm{};
    if (!GetObjectA(bitmap, sizeof(bm), &bm) || bm.bmWidth <= 0 || bm.bmHeight <= 0) {
        return;
    }

    HDC srcDC = CreateCompatibleDC(target);
    if (!srcDC) {
        return;
    }

    HGDIOBJ oldBitmap = SelectObject(srcDC, bitmap);
    TransparentBlt(target,
        dst.left,
        dst.top,
        dst.right - dst.left,
        dst.bottom - dst.top,
        srcDC,
        0,
        0,
        bm.bmWidth,
        bm.bmHeight,
        RGB(255, 0, 255));
    SelectObject(srcDC, oldBitmap);
    DeleteDC(srcDC);
}

void DrawBitmapAlpha(HDC target, HBITMAP bitmap, const RECT& dst)
{
    if (!target || !bitmap || dst.right <= dst.left || dst.bottom <= dst.top) {
        return;
    }

    BITMAP bm{};
    if (!GetObjectA(bitmap, sizeof(bm), &bm) || bm.bmWidth <= 0 || bm.bmHeight <= 0) {
        return;
    }

    HDC srcDC = CreateCompatibleDC(target);
    if (!srcDC) {
        return;
    }

    HGDIOBJ oldBitmap = SelectObject(srcDC, bitmap);

    BLENDFUNCTION blend{};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;

    const BOOL ok = AlphaBlend(target,
        dst.left,
        dst.top,
        dst.right - dst.left,
        dst.bottom - dst.top,
        srcDC,
        0,
        0,
        bm.bmWidth,
        bm.bmHeight,
        blend);

    if (!ok) {
        DrawBitmapTransparent(target, bitmap, dst);
    }

    SelectObject(srcDC, oldBitmap);
    DeleteDC(srcDC);
}

void DrawBitmapSegmentTransparent(HDC target, HBITMAP bitmap, const RECT& dst, int srcX, int srcY, int srcW, int srcH)
{
    if (!target || !bitmap || srcW <= 0 || srcH <= 0 || dst.right <= dst.left || dst.bottom <= dst.top) {
        return;
    }

    HDC srcDC = CreateCompatibleDC(target);
    if (!srcDC) {
        return;
    }
    HGDIOBJ oldBitmap = SelectObject(srcDC, bitmap);
    TransparentBlt(target,
        dst.left,
        dst.top,
        dst.right - dst.left,
        dst.bottom - dst.top,
        srcDC,
        srcX,
        srcY,
        srcW,
        srcH,
        RGB(255, 0, 255));
    SelectObject(srcDC, oldBitmap);
    DeleteDC(srcDC);
}

void FillRectColor(HDC hdc, const RECT& rect, COLORREF color)
{
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(hdc, &rect, brush);
    DeleteObject(brush);
}

void FrameRectColor(HDC hdc, const RECT& rect, COLORREF color)
{
    HBRUSH brush = CreateSolidBrush(color);
    FrameRect(hdc, &rect, brush);
    DeleteObject(brush);
}

void DrawWindowText(HDC hdc, int x, int y, const std::string& text, COLORREF color, UINT format = DT_LEFT | DT_TOP | DT_SINGLELINE)
{
    if (!hdc || text.empty()) {
        return;
    }

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    RECT rect{ x, y, x + 260, y + 18 };
    HGDIOBJ oldFont = SelectObject(hdc, GetStockObject(DEFAULT_GUI_FONT));
    DrawTextA(hdc, text.c_str(), -1, &rect, format);
    SelectObject(hdc, oldFont);
}

void DrawWindowTextRect(HDC hdc, const RECT& rect, const std::string& text, COLORREF color, UINT format)
{
    if (!hdc || text.empty() || rect.right <= rect.left || rect.bottom <= rect.top) {
        return;
    }

    static HFONT s_sharpUiFont = nullptr;
    if (!s_sharpUiFont) {
        s_sharpUiFont = CreateFontA(
            -11,
            0,
            0,
            0,
            FW_NORMAL,
            FALSE,
            FALSE,
            FALSE,
            DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,
            NONANTIALIASED_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE,
            "MS Sans Serif");
    }

    RECT drawRect = rect;
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    HGDIOBJ oldFont = SelectObject(hdc, s_sharpUiFont ? s_sharpUiFont : GetStockObject(DEFAULT_GUI_FONT));
    DrawTextA(hdc, text.c_str(), -1, &drawRect, format);
    SelectObject(hdc, oldFont);
}

bool IsEquipTabType(int itemType)
{
    switch (itemType) {
    case 4:
    case 5:
    case 8:
    case 9:
    case 11:
    case 12:
    case 13:
    case 14:
    case 15:
        return true;
    default:
        return false;
    }
}

bool BuildEquipPreviewPaletteOverride(const std::string& paletteName, std::array<unsigned int, 256>& outPalette)
{
    if (paletteName.empty()) {
        return false;
    }

    CPaletteRes* palRes = g_resMgr.GetAs<CPaletteRes>(paletteName.c_str());
    if (!palRes) {
        return false;
    }

    g_3dDevice.ConvertPalette(outPalette.data(), palRes->m_pal, 256);
    return true;
}

POINT GetEquipPreviewLayerPoint(int layerPriority,
    int resolvedLayer,
    CImfRes* imfRes,
    const CMotion* motion,
    const std::string& bodyActName,
    int curAction,
    int curMotion)
{
    POINT point = imfRes->GetPoint(resolvedLayer, curAction, curMotion);
    if (layerPriority != 1 || !motion || motion->attachInfo.empty()) {
        return point;
    }

    CActRes* bodyActRes = g_resMgr.GetAs<CActRes>(bodyActName.c_str());
    if (!bodyActRes) {
        return point;
    }

    const CMotion* bodyMotion = bodyActRes->GetMotion(curAction, curMotion);
    if (!bodyMotion || bodyMotion->attachInfo.empty()) {
        return point;
    }

    const CAttachPointInfo& headAttach = motion->attachInfo.front();
    const CAttachPointInfo& bodyAttach = bodyMotion->attachInfo.front();
    if (headAttach.attr != bodyAttach.attr) {
        return point;
    }

    point.x += bodyAttach.x - headAttach.x;
    point.y += bodyAttach.y - headAttach.y;
    return point;
}

bool DrawEquipPreviewLayer(HDC hdc,
    int drawX,
    int drawY,
    int layerIndex,
    int curAction,
    int curMotion,
    const std::string& actName,
    const std::string& sprName,
    const std::string& imfName,
    const std::string& bodyActName,
    const std::string& paletteName)
{
    CActRes* actRes = g_resMgr.GetAs<CActRes>(actName.c_str());
    CSprRes* sprRes = g_resMgr.GetAs<CSprRes>(sprName.c_str());
    CImfRes* imfRes = g_resMgr.GetAs<CImfRes>(imfName.c_str());
    if (!actRes || !sprRes || !imfRes) {
        return false;
    }

    int resolvedLayer = imfRes->GetLayer(layerIndex, curAction, curMotion);
    if (resolvedLayer < 0) {
        resolvedLayer = layerIndex;
    }

    const CMotion* motion = actRes->GetMotion(curAction, curMotion);
    if (!motion || resolvedLayer >= static_cast<int>(motion->sprClips.size())) {
        return false;
    }

    const POINT point = GetEquipPreviewLayerPoint(layerIndex, resolvedLayer, imfRes, motion, bodyActName, curAction, curMotion);

    std::array<unsigned int, 256> paletteOverride{};
    unsigned int* palette = sprRes->m_pal;
    if (!paletteName.empty() && BuildEquipPreviewPaletteOverride(paletteName, paletteOverride)) {
        palette = paletteOverride.data();
    }

    CMotion singleLayerMotion{};
    singleLayerMotion.sprClips.push_back(motion->sprClips[resolvedLayer]);
    return DrawActMotionToHdc(hdc, drawX + point.x, drawY + point.y, sprRes, &singleLayerMotion, palette);
}

bool DrawEquipPreviewPlayerSprite(HDC hdc, int drawX, int drawY)
{
    char bodyAct[260] = {};
    char bodySpr[260] = {};
    char headAct[260] = {};
    char headSpr[260] = {};
    char imfName[260] = {};
    char bodyPalette[260] = {};
    char headPalette[260] = {};

    const int sex = g_session.GetSex();
    int head = g_session.m_playerHead;
    const int curAction = g_session.m_playerDir & 7;
    const int curMotion = 0;

    const std::string bodyActName = g_session.GetJobActName(g_session.m_playerJob, sex, bodyAct);
    const std::string bodySprName = g_session.GetJobSprName(g_session.m_playerJob, sex, bodySpr);
    const std::string headActName = g_session.GetHeadActName(g_session.m_playerJob, &head, sex, headAct);
    const std::string headSprName = g_session.GetHeadSprName(g_session.m_playerJob, &head, sex, headSpr);
    const std::string imfPath = g_session.GetImfName(g_session.m_playerJob, head, sex, imfName);
    const std::string bodyPaletteName = g_session.m_playerBodyPalette > 0
        ? g_session.GetBodyPaletteName(g_session.m_playerJob, sex, g_session.m_playerBodyPalette, bodyPalette)
        : std::string();
    const std::string headPaletteName = g_session.m_playerHeadPalette > 0
        ? g_session.GetHeadPaletteName(head, g_session.m_playerJob, sex, g_session.m_playerHeadPalette, headPalette)
        : std::string();

    bool drew = false;
    drew |= DrawEquipPreviewLayer(hdc, drawX, drawY, 0, curAction, curMotion, bodyActName, bodySprName, imfPath, bodyActName, bodyPaletteName);
    drew |= DrawEquipPreviewLayer(hdc, drawX, drawY, 1, curAction, curMotion, headActName, headSprName, imfPath, bodyActName, headPaletteName);
    return drew;
}

bool FindOpaqueBounds(const unsigned int* pixels, int width, int height, RECT* outBounds)
{
    if (!pixels || width <= 0 || height <= 0 || !outBounds) {
        return false;
    }

    int minX = width;
    int minY = height;
    int maxX = -1;
    int maxY = -1;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const unsigned int pixel = pixels[static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)];
            if (((pixel >> 24) & 0xFFu) == 0u) {
                continue;
            }
            minX = (std::min)(minX, x);
            minY = (std::min)(minY, y);
            maxX = (std::max)(maxX, x);
            maxY = (std::max)(maxY, y);
        }
    }

    if (maxX < minX || maxY < minY) {
        return false;
    }

    outBounds->left = minX;
    outBounds->top = minY;
    outBounds->right = maxX + 1;
    outBounds->bottom = maxY + 1;
    return true;
}

bool DrawEquipPreviewPlayerSpriteFitted(HDC hdc, const RECT& previewArea)
{
    if (!hdc || previewArea.right <= previewArea.left || previewArea.bottom <= previewArea.top) {
        return false;
    }

    constexpr int kComposeWidth = 160;
    constexpr int kComposeHeight = 180;

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = kComposeWidth;
    bmi.bmiHeader.biHeight = -kComposeHeight;
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

    std::memset(dibBits, 0, static_cast<size_t>(kComposeWidth) * static_cast<size_t>(kComposeHeight) * sizeof(unsigned int));

    HDC memDc = CreateCompatibleDC(hdc);
    if (!memDc) {
        DeleteObject(dib);
        return false;
    }

    HGDIOBJ oldBitmap = SelectObject(memDc, dib);
    const bool drew = DrawEquipPreviewPlayerSprite(memDc, kComposeWidth / 2, kComposeHeight - 14);
    RECT srcBounds{};
    const bool hasBounds = FindOpaqueBounds(static_cast<const unsigned int*>(dibBits), kComposeWidth, kComposeHeight, &srcBounds);

    if (drew && hasBounds) {
        const int srcW = srcBounds.right - srcBounds.left;
        const int srcH = srcBounds.bottom - srcBounds.top;
        const int areaW = previewArea.right - previewArea.left;
        const int areaH = previewArea.bottom - previewArea.top;

        int drawW = srcW;
        int drawH = srcH;
        if (drawW > areaW || drawH > areaH) {
            const float scaleX = static_cast<float>(areaW) / static_cast<float>((std::max)(1, srcW));
            const float scaleY = static_cast<float>(areaH) / static_cast<float>((std::max)(1, srcH));
            const float scale = (std::min)(scaleX, scaleY);
            drawW = (std::max)(1, static_cast<int>(static_cast<float>(srcW) * scale));
            drawH = (std::max)(1, static_cast<int>(static_cast<float>(srcH) * scale));
        }

        const int dstX = previewArea.left + (areaW - drawW) / 2;
        const int dstY = previewArea.top + (areaH - drawH) / 2;

        BLENDFUNCTION blend{};
        blend.BlendOp = AC_SRC_OVER;
        blend.SourceConstantAlpha = 255;
        blend.AlphaFormat = AC_SRC_ALPHA;
        AlphaBlend(hdc,
            dstX,
            dstY,
            drawW,
            drawH,
            memDc,
            srcBounds.left,
            srcBounds.top,
            srcW,
            srcH,
            blend);
    }

    SelectObject(memDc, oldBitmap);
    DeleteDC(memDc);
    DeleteObject(dib);
    return drew && hasBounds;
}

std::vector<std::string> BuildItemIconCandidates(const ITEM_INFO& item)
{
    std::vector<std::string> out;
    const std::string resource = item.GetResourceName();
    if (resource.empty()) {
        return out;
    }

    std::string stem = NormalizeSlash(resource);
    std::string filenameOnly = stem;
    const size_t slashPos = filenameOnly.find_last_of('\\');
    if (slashPos != std::string::npos && slashPos + 1 < filenameOnly.size()) {
        filenameOnly = filenameOnly.substr(slashPos + 1);
    }

    AddUniqueCandidate(out, stem);
    AddUniqueCandidate(out, stem + ".bmp");
    AddUniqueCandidate(out, filenameOnly);
    AddUniqueCandidate(out, filenameOnly + ".bmp");

    const char* prefixes[] = {
        kUiKorPrefix,
        "texture\\\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\item\\",
        "data\\texture\\\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\",
        "data\\texture\\\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\item\\",
        "skin\\default\\",
        "skin\\default\\item\\",
        "item\\",
        "texture\\item\\",
        "texture\\interface\\item\\",
        "data\\item\\",
        "data\\texture\\item\\",
        "data\\texture\\interface\\item\\",
        nullptr
    };

    for (int index = 0; prefixes[index]; ++index) {
        AddUniqueCandidate(out, std::string(prefixes[index]) + stem);
        AddUniqueCandidate(out, std::string(prefixes[index]) + stem + ".bmp");
        AddUniqueCandidate(out, std::string(prefixes[index]) + filenameOnly);
        AddUniqueCandidate(out, std::string(prefixes[index]) + filenameOnly + ".bmp");
    }

    return out;
}

} // namespace

UIEquipWnd::UIEquipWnd()
    : m_controlsCreated(false),
      m_fullHeight(kWindowHeight),
      m_systemButtons{ nullptr, nullptr, nullptr },
      m_backgroundLeft(nullptr),
      m_backgroundMid(nullptr),
      m_backgroundRight(nullptr),
      m_backgroundFull(nullptr),
      m_titleBarLeft(nullptr),
      m_titleBarMid(nullptr),
      m_titleBarRight(nullptr)
{
    Create(kWindowWidth, kWindowHeight);
    Move(281, 121);
}

UIEquipWnd::~UIEquipWnd()
{
    ReleaseAssets();
}

void UIEquipWnd::SetShow(int show)
{
    UIWindow::SetShow(show);
    if (show != 0) {
        EnsureCreated();
        LayoutChildren();
    }
}

void UIEquipWnd::Move(int x, int y)
{
    UIWindow::Move(x, y);
    if (m_controlsCreated) {
        LayoutChildren();
    }
}

bool UIEquipWnd::IsUpdateNeed()
{
    return true;
}

int UIEquipWnd::SendMsg(UIWindow* sender, int msg, int wparam, int lparam, int extra)
{
    (void)sender;
    (void)lparam;
    (void)extra;

    if (msg != 6) {
        return 0;
    }

    switch (wparam) {
    case kButtonIdBase:
        SetMiniMode(false);
        return 1;
    case kButtonIdMini:
        SetMiniMode(true);
        return 1;
    case kButtonIdClose:
        SetShow(0);
        return 1;
    default:
        return 1;
    }
}

void UIEquipWnd::OnCreate(int x, int y)
{
    (void)x;
    (void)y;
    if (m_controlsCreated) {
        return;
    }

    m_controlsCreated = true;
    LoadAssets();

    if (m_backgroundFull) {
        BITMAP bgInfo{};
        if (GetObjectA(m_backgroundFull, sizeof(bgInfo), &bgInfo) && bgInfo.bmWidth > 0 && bgInfo.bmHeight > 0) {
            m_fullHeight = kTitleBarHeight + static_cast<int>(bgInfo.bmHeight);
            Resize(static_cast<int>(bgInfo.bmWidth), m_fullHeight);
        }
    }

    struct ButtonSpec {
        const char* offName;
        const char* onName;
        int id;
        const char* tooltip;
    };

    const std::array<ButtonSpec, 3> specs = {{
        { "sys_base_off.bmp", "sys_base_on.bmp", kButtonIdBase, "Base" },
        { "sys_mini_off.bmp", "sys_mini_on.bmp", kButtonIdMini, "Mini" },
        { "sys_close_off.bmp", "sys_close_on.bmp", kButtonIdClose, "Close" },
    }};

    for (size_t index = 0; index < specs.size(); ++index) {
        auto* button = new UIBitmapButton();
        button->SetBitmapName(ResolveUiAssetPath(specs[index].offName).c_str(), 0);
        button->SetBitmapName(ResolveUiAssetPath(specs[index].onName).c_str(), 1);
        button->SetBitmapName(ResolveUiAssetPath(specs[index].onName).c_str(), 2);
        button->Create(button->m_bitmapWidth, button->m_bitmapHeight);
        button->m_id = specs[index].id;
        button->SetToolTip(specs[index].tooltip);
        AddChild(button);
        m_systemButtons[index] = button;
    }

    LayoutChildren();
}

void UIEquipWnd::OnDestroy()
{
}

void UIEquipWnd::OnDraw()
{
    if (m_show == 0) {
        return;
    }

    EnsureCreated();

    HDC hdc = UIWindow::GetSharedDrawDC();
    const bool useShared = hdc != nullptr;
    if (!hdc && g_hMainWnd) {
        hdc = GetDC(g_hMainWnd);
    }
    if (!hdc) {
        return;
    }

    RECT windowRect{ m_x, m_y, m_x + m_w, m_y + m_h };
    FillRectColor(hdc, windowRect, RGB(255, 255, 255));

    const int bodyTop = m_y + kTitleBarHeight;
    if (m_backgroundFull) {
        BITMAP bgInfo{};
        if (GetObjectA(m_backgroundFull, sizeof(bgInfo), &bgInfo) && bgInfo.bmWidth > 0 && bgInfo.bmHeight > 0) {
            RECT bgRect{
                m_x,
                bodyTop,
                m_x + bgInfo.bmWidth,
                bodyTop + bgInfo.bmHeight
            };
            DrawBitmapTransparent(hdc, m_backgroundFull, bgRect);
        }
    } else {
        RECT bodyRect{ m_x, bodyTop, m_x + m_w, m_y + m_h };
        FillRectColor(hdc, bodyRect, RGB(255, 255, 255));

        for (int yPos = bodyTop; yPos < m_y + m_h; yPos += 8) {
            RECT leftRect{ m_x, yPos, m_x + 20, std::min(yPos + 8, m_y + m_h) };
            RECT rightRect{ m_x + m_w - 20, yPos, m_x + m_w, std::min(yPos + 8, m_y + m_h) };
            DrawBitmapTransparent(hdc, m_backgroundLeft, leftRect);
            DrawBitmapTransparent(hdc, m_backgroundRight, rightRect);
        }
    }

    const RECT titleStrip{ m_x, m_y, m_x + m_w, m_y + kTitleBarHeight };
    if (m_titleBarLeft && m_titleBarMid && m_titleBarRight) {
        BITMAP leftInfo{};
        BITMAP midInfo{};
        BITMAP rightInfo{};
        GetObjectA(m_titleBarLeft, sizeof(leftInfo), &leftInfo);
        GetObjectA(m_titleBarMid, sizeof(midInfo), &midInfo);
        GetObjectA(m_titleBarRight, sizeof(rightInfo), &rightInfo);

        const int leftW = (std::max)(0, static_cast<int>(leftInfo.bmWidth));
        const int rightW = (std::max)(0, static_cast<int>(rightInfo.bmWidth));
        const int midW = (std::max)(0, static_cast<int>(titleStrip.right - titleStrip.left - leftW - rightW));
        if (leftW > 0) {
            RECT dst{ titleStrip.left, titleStrip.top, titleStrip.left + leftW, titleStrip.bottom };
            DrawBitmapSegmentTransparent(hdc, m_titleBarLeft, dst, 0, 0, static_cast<int>(leftInfo.bmWidth), (std::max)(1, static_cast<int>(leftInfo.bmHeight)));
        }
        if (midW > 0) {
            RECT dst{ titleStrip.left + leftW, titleStrip.top, titleStrip.left + leftW + midW, titleStrip.bottom };
            DrawBitmapSegmentTransparent(hdc, m_titleBarMid, dst, 0, 0, (std::max)(1, static_cast<int>(midInfo.bmWidth)), (std::max)(1, static_cast<int>(midInfo.bmHeight)));
        }
        if (rightW > 0) {
            RECT dst{ titleStrip.right - rightW, titleStrip.top, titleStrip.right, titleStrip.bottom };
            DrawBitmapSegmentTransparent(hdc, m_titleBarRight, dst, 0, 0, static_cast<int>(rightInfo.bmWidth), (std::max)(1, static_cast<int>(rightInfo.bmHeight)));
        }
    } else if (m_backgroundMid) {
        DrawBitmapTransparent(hdc, m_backgroundMid, titleStrip);
    }
    DrawWindowText(hdc, m_x + 18, m_y + 3, "Equipment", RGB(255, 255, 255));
    DrawWindowText(hdc, m_x + 17, m_y + 2, "Equipment", RGB(0, 0, 0));

    if (m_h > kMiniHeight) {
        RECT centerPanel{
            m_x + kCenterPanelLeft,
            m_y + kCenterPanelTop,
            m_x + (std::min)(kCenterPanelRight, m_w - 98),
            m_y + (std::min)(kCenterPanelBottom, m_h - 10)
        };

        if (!DrawEquipPreviewPlayerSpriteFitted(hdc, centerPanel)) {
            DrawWindowText(hdc, centerPanel.left + 14, centerPanel.top + 68, "No Preview", RGB(90, 90, 90));
        }

        const std::vector<const ITEM_INFO*> slotItems = BuildSlotAssignments();
        for (size_t i = 0; i < kEquipSlots.size(); ++i) {
            RECT slotRect{
                m_x + kEquipSlots[i].iconX,
                m_y + kEquipSlots[i].iconY,
                m_x + kEquipSlots[i].iconX + kSlotIconSize,
                m_y + kEquipSlots[i].iconY + kSlotIconSize
            };
            if (slotItems[i]) {
                if (HBITMAP icon = GetItemIcon(*slotItems[i])) {
                    DrawBitmapTransparent(hdc, icon, slotRect);
                }

                const bool leftColumn = kEquipSlots[i].iconX < kCenterPanelLeft;
                RECT textRect{};
                if (leftColumn) {
                    textRect.left = slotRect.right + 4;
                    textRect.right = m_x + kCenterPanelLeft + 24;
                } else {
                    textRect.left = m_x + kCenterPanelRight - 24;
                    textRect.right = slotRect.left - 4;
                }
                textRect.top = slotRect.top + 2;
                textRect.bottom = slotRect.bottom;

                const std::string itemText = slotItems[i]->GetEquipDisplayName();
                const UINT textFormat = (leftColumn ? DT_LEFT : DT_RIGHT) | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS;
                DrawWindowTextRect(hdc, textRect, itemText, RGB(0, 0, 0), textFormat);
            }
        }
    }

    DrawChildren();
    if (!useShared) {
        ReleaseDC(g_hMainWnd, hdc);
    }
}

void UIEquipWnd::OnLBtnDblClk(int x, int y)
{
    if (y >= m_y && y < m_y + kTitleBarHeight) {
        SetMiniMode(m_h != kMiniHeight);
        return;
    }

    if (m_h > kMiniHeight) {
        const std::vector<const ITEM_INFO*> slotItems = BuildSlotAssignments();
        for (size_t i = 0; i < kEquipSlots.size(); ++i) {
            if (!slotItems[i]) {
                continue;
            }

            RECT slotRect{
                m_x + kEquipSlots[i].iconX,
                m_y + kEquipSlots[i].iconY,
                m_x + kEquipSlots[i].iconX + kSlotIconSize,
                m_y + kEquipSlots[i].iconY + kSlotIconSize
            };
            if (x >= slotRect.left && x < slotRect.right && y >= slotRect.top && y < slotRect.bottom) {
                if (g_modeMgr.SendMsg(
                        CGameMode::GameMsg_RequestUnequipInventoryItem,
                        static_cast<int>(slotItems[i]->m_itemIndex),
                        0,
                        0) != 0) {
                    return;
                }
            }
        }
    }

    UIFrameWnd::OnLBtnDblClk(x, y);
}

void UIEquipWnd::StoreInfo()
{
}

void UIEquipWnd::EnsureCreated()
{
    if (!m_controlsCreated) {
        OnCreate(0, 0);
    }
}

void UIEquipWnd::LayoutChildren()
{
    if (!m_controlsCreated) {
        return;
    }

    if (m_systemButtons[0]) {
        m_systemButtons[0]->Move(m_x + 247, m_y + 3);
        m_systemButtons[0]->SetShow(m_h == kMiniHeight ? 1 : 0);
    }
    if (m_systemButtons[1]) {
        m_systemButtons[1]->Move(m_x + 247, m_y + 3);
        m_systemButtons[1]->SetShow(m_h != kMiniHeight ? 1 : 0);
    }
    if (m_systemButtons[2]) {
        m_systemButtons[2]->Move(m_x + 265, m_y + 3);
        m_systemButtons[2]->SetShow(1);
    }
}

void UIEquipWnd::LoadAssets()
{
    m_backgroundLeft = LoadBitmapFromGameData(ResolveUiAssetPath("itemwin_left.bmp"));
    m_backgroundMid = LoadBitmapFromGameData(ResolveUiAssetPath("itemwin_mid.bmp"));
    m_backgroundRight = LoadBitmapFromGameData(ResolveUiAssetPath("itemwin_right.bmp"));
    m_backgroundFull = LoadBitmapFromGameData(ResolveUiAssetPath("equipwin_bg.bmp"));
    m_titleBarLeft = LoadBitmapFromGameData(ResolveUiAssetPath("titlebar_left.bmp"));
    m_titleBarMid = LoadBitmapFromGameData(ResolveUiAssetPath("titlebar_mid.bmp"));
    m_titleBarRight = LoadBitmapFromGameData(ResolveUiAssetPath("titlebar_right.bmp"));
}

void UIEquipWnd::ReleaseAssets()
{
    if (m_backgroundLeft) {
        DeleteObject(m_backgroundLeft);
        m_backgroundLeft = nullptr;
    }
    if (m_backgroundMid) {
        DeleteObject(m_backgroundMid);
        m_backgroundMid = nullptr;
    }
    if (m_backgroundRight) {
        DeleteObject(m_backgroundRight);
        m_backgroundRight = nullptr;
    }
    if (m_backgroundFull) {
        DeleteObject(m_backgroundFull);
        m_backgroundFull = nullptr;
    }
    if (m_titleBarLeft) {
        DeleteObject(m_titleBarLeft);
        m_titleBarLeft = nullptr;
    }
    if (m_titleBarMid) {
        DeleteObject(m_titleBarMid);
        m_titleBarMid = nullptr;
    }
    if (m_titleBarRight) {
        DeleteObject(m_titleBarRight);
        m_titleBarRight = nullptr;
    }
    for (auto& entry : m_iconCache) {
        if (entry.second) {
            DeleteObject(entry.second);
        }
    }
    m_iconCache.clear();
}

void UIEquipWnd::SetMiniMode(bool miniMode)
{
    Resize(kWindowWidth, miniMode ? kMiniHeight : m_fullHeight);
    LayoutChildren();
}

std::vector<const ITEM_INFO*> UIEquipWnd::BuildSlotAssignments() const
{
    std::vector<const ITEM_INFO*> out(kEquipSlots.size(), nullptr);
    std::vector<const ITEM_INFO*> equipped;
    const std::list<ITEM_INFO>& items = g_session.GetInventoryItems();
    for (const ITEM_INFO& item : items) {
        if (item.m_wearLocation == 0 || !IsEquipTabType(item.m_itemType)) {
            continue;
        }
        equipped.push_back(&item);
    }

    // Primary-slot assignment to avoid duplicate rendering (e.g. two-hand weapons).
    const auto assignByMask = [&](int slotMask) {
        for (const ITEM_INFO* item : equipped) {
            if (!item) {
                continue;
            }
            if ((item->m_wearLocation & slotMask) == 0) {
                continue;
            }
            for (const ITEM_INFO* existing : out) {
                if (existing == item) {
                    return;
                }
            }
            for (size_t i = 0; i < kEquipSlots.size(); ++i) {
                if (kEquipSlots[i].wearMask == slotMask && !out[i]) {
                    out[i] = item;
                    return;
                }
            }
        }
    };

    // Ref-like priority order.
    assignByMask(256);
    assignByMask(512);
    assignByMask(1);
    assignByMask(4);
    assignByMask(8);
    assignByMask(2);
    assignByMask(32);
    assignByMask(16);
    assignByMask(64);
    assignByMask(128);

    // Fallback fill for uncommon masks that still overlap known slots.
    for (const ITEM_INFO* item : equipped) {
        bool alreadyPlaced = false;
        for (const ITEM_INFO* existing : out) {
            if (existing == item) {
                alreadyPlaced = true;
                break;
            }
        }
        if (alreadyPlaced) {
            continue;
        }
        for (size_t i = 0; i < kEquipSlots.size(); ++i) {
            if (out[i]) {
                continue;
            }
            if ((item->m_wearLocation & kEquipSlots[i].wearMask) != 0) {
                out[i] = item;
                break;
            }
        }
    }
    return out;
}


HBITMAP UIEquipWnd::GetItemIcon(const ITEM_INFO& item)
{
    const unsigned int itemId = item.GetItemId();
    const auto found = m_iconCache.find(itemId);
    if (found != m_iconCache.end()) {
        return found->second;
    }

    HBITMAP bitmap = nullptr;
    for (const std::string& candidate : BuildItemIconCandidates(item)) {
        if (!g_fileMgr.IsDataExist(candidate.c_str())) {
            continue;
        }
        bitmap = LoadBitmapFromGameData(candidate);
        if (bitmap) {
            break;
        }
    }

    m_iconCache[itemId] = bitmap;
    return bitmap;
}

