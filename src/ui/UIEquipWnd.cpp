#include "UIEquipWnd.h"

#include "gamemode/GameMode.h"
#include "gamemode/Mode.h"
#include "UIItemWnd.h"
#include "UIShortCutWnd.h"
#include "UIWindowMgr.h"
#include "core/File.h"
#include "item/Item.h"
#include "main/WinMain.h"
#include "qtui/QtUiRuntime.h"
#include "render/DC.h"
#include "render3d/Device.h"
#include "res/ActRes.h"
#include "res/Bitmap.h"
#include "res/ImfRes.h"
#include "res/PaletteRes.h"
#include "res/Sprite.h"
#include "res/Texture.h"
#include "session/Session.h"
#include "world/GameActor.h"
#include "world/World.h"

#include <windows.h>

#if RO_ENABLE_QT6_UI
#include <QFont>
#include <QFontMetrics>
#include <QImage>
#include <QPainter>
#include <QString>
#endif

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <list>
#include <string>
#include <vector>

#pragma comment(lib, "msimg32.lib")

namespace {

constexpr int kWindowWidth = 280;
constexpr int kWindowHeight = 232;
constexpr int kMiniHeight = 34;
constexpr int kTitleBarHeight = 17;
constexpr int kQtButtonWidth = 12;
constexpr int kQtButtonHeight = 11;
constexpr int kButtonIdBase = 134;
constexpr int kButtonIdClose = 135;
constexpr int kButtonIdMini = 136;
constexpr const char* kUiKorPrefix =
    "texture\\"
    "\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA"
    "\\";

constexpr int kSlotIconSize = 32;
constexpr int kSlotVerticalSpacing = 5;
constexpr int kEquipSlotRowCount = 5;
constexpr int kEquipSlotBaseY = 19;
constexpr int kEquipSlotLegacyStep = 32;
constexpr int kCenterPanelLeft = 98;
constexpr int kCenterPanelRight = 182;
constexpr int kCenterPanelTop = 32;
constexpr int kCenterPanelBottom = 204;

struct EquipSlotDefLocal {
    int wearMask;
    int iconX;
    int iconY;
};

constexpr std::array<EquipSlotDefLocal, 10> kEquipSlots = {{
    { 256, 8, 19 },    // head upper
    { 1, 8, 51 },      // head lower
    { 4, 8, 115 },     // garment
    { 8, 8, 147 },     // accessory left
    { 512, 240, 19 },  // head mid
    { 16, 240, 51 },   // armor
    { 32, 240, 83 },   // shield
    { 64, 240, 115 },  // shoes
    { 128, 240, 147 }, // accessory right
    { 2, 8, 83 },      // weapon
}};

bool IsLeftEquipSlot(const EquipSlotDefLocal& slot)
{
    return slot.iconX < kCenterPanelLeft;
}

int GetEquipSlotX(const EquipSlotDefLocal& slot, int windowWidth)
{
    if (IsLeftEquipSlot(slot)) {
        return (std::max)(0, (kCenterPanelLeft - kSlotIconSize) / 2);
    }

    const int rightGutterWidth = (std::max)(0, windowWidth - kCenterPanelRight);
    return kCenterPanelRight + (std::max)(0, (rightGutterWidth - kSlotIconSize) / 2);
}

int GetEquipSlotRow(const EquipSlotDefLocal& slot)
{
    return (std::max)(0, (slot.iconY - kEquipSlotBaseY) / kEquipSlotLegacyStep);
}

int GetEquipSlotY(const EquipSlotDefLocal& slot, int windowHeight)
{
    const int contentHeight = (std::max)(0, windowHeight - kTitleBarHeight);
    const int totalSlotHeight = (kEquipSlotRowCount * kSlotIconSize)
        + ((kEquipSlotRowCount - 1) * kSlotVerticalSpacing);
    const int topOffset = kTitleBarHeight + (std::max)(0, (contentHeight - totalSlotHeight) / 2);
    return topOffset + (GetEquipSlotRow(slot) * (kSlotIconSize + kSlotVerticalSpacing));
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

void HashTokenValue(unsigned long long* hash, unsigned long long value)
{
    if (!hash) {
        return;
    }
    *hash ^= value;
    *hash *= 1099511628211ull;
}

void HashTokenString(unsigned long long* hash, const std::string& value)
{
    if (!hash) {
        return;
    }
    for (unsigned char ch : value) {
        HashTokenValue(hash, static_cast<unsigned long long>(ch));
    }
    HashTokenValue(hash, 0xFFull);
}

RECT MakeEquipRect(int x, int y, int left, int top, int width, int height)
{
    RECT rect{ x + left, y + top, x + left + width, y + top + height };
    return rect;
}

bool IsPointInRect(const RECT& rect, int x, int y)
{
    return x >= rect.left && x < rect.right && y >= rect.top && y < rect.bottom;
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

shopui::BitmapPixels LoadBitmapPixelsFromGameData(const std::string& path)
{
    return shopui::LoadBitmapPixelsFromGameData(path, true);
}

void DrawBitmapPixelsStretched(HDC target, const shopui::BitmapPixels& bitmap, const RECT& dst)
{
    if (!target || !bitmap.IsValid() || dst.right <= dst.left || dst.bottom <= dst.top) {
        return;
    }
    AlphaBlendArgbToHdc(target,
                        dst.left,
                        dst.top,
                        dst.right - dst.left,
                        dst.bottom - dst.top,
                        bitmap.pixels.data(),
                        bitmap.width,
                        bitmap.height);
}

void DrawBitmapPixelsSegmentTransparent(HDC target, const shopui::BitmapPixels& bitmap, const RECT& dst, int srcX, int srcY, int srcW, int srcH)
{
    if (!target || !bitmap.IsValid() || srcW <= 0 || srcH <= 0 || dst.right <= dst.left || dst.bottom <= dst.top) {
        return;
    }
    if (srcX < 0 || srcY < 0 || srcX + srcW > bitmap.width || srcY + srcH > bitmap.height) {
        return;
    }

    std::vector<unsigned int> cropped(static_cast<size_t>(srcW) * static_cast<size_t>(srcH));
    for (int row = 0; row < srcH; ++row) {
        const unsigned int* srcRow = bitmap.pixels.data() + static_cast<size_t>(srcY + row) * static_cast<size_t>(bitmap.width) + static_cast<size_t>(srcX);
        unsigned int* dstRow = cropped.data() + static_cast<size_t>(row) * static_cast<size_t>(srcW);
        std::copy(srcRow, srcRow + srcW, dstRow);
    }

    AlphaBlendArgbToHdc(target,
                        dst.left,
                        dst.top,
                        dst.right - dst.left,
                        dst.bottom - dst.top,
                        cropped.data(),
                        srcW,
                        srcH);
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

#if RO_ENABLE_QT6_UI
QFont BuildEquipWindowFontFromHdc(HDC hdc, const char* fallbackFamily = "MS Sans Serif", int fallbackPixelSize = 13)
{
    LOGFONTA logFont{};
    if (hdc) {
        if (HGDIOBJ fontObject = GetCurrentObject(hdc, OBJ_FONT)) {
            GetObjectA(fontObject, sizeof(logFont), &logFont);
        }
    }

    const QString family = logFont.lfFaceName[0] != '\0'
        ? QString::fromLocal8Bit(logFont.lfFaceName)
        : QString::fromLocal8Bit(fallbackFamily);
    QFont font(family);
    font.setPixelSize(logFont.lfHeight != 0 ? (std::max)(1, static_cast<int>(std::abs(logFont.lfHeight))) : fallbackPixelSize);
    font.setBold(logFont.lfWeight >= FW_BOLD);
    font.setStyleStrategy(QFont::NoAntialias);
    return font;
}

Qt::Alignment ToQtEquipTextAlignment(UINT format)
{
    Qt::Alignment alignment = Qt::AlignLeft | Qt::AlignTop;
    if (format & DT_CENTER) {
        alignment &= ~Qt::AlignLeft;
        alignment |= Qt::AlignHCenter;
    } else if (format & DT_RIGHT) {
        alignment &= ~Qt::AlignLeft;
        alignment |= Qt::AlignRight;
    }

    if (format & DT_VCENTER) {
        alignment &= ~Qt::AlignTop;
        alignment |= Qt::AlignVCenter;
    } else if (format & DT_BOTTOM) {
        alignment &= ~Qt::AlignTop;
        alignment |= Qt::AlignBottom;
    }

    return alignment;
}

void DrawEquipWindowTextQt(HDC hdc, const RECT& rect, const std::string& text, COLORREF color, UINT format)
{
    if (!hdc || text.empty() || rect.right <= rect.left || rect.bottom <= rect.top) {
        return;
    }

    QString label = QString::fromLocal8Bit(text.c_str());
    if (label.isEmpty()) {
        return;
    }

    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    const QFont font = BuildEquipWindowFontFromHdc(hdc);
    if (format & DT_END_ELLIPSIS) {
        const QFontMetrics metrics(font);
        label = metrics.elidedText(label, Qt::ElideRight, width);
    }

    std::vector<unsigned int> pixels(static_cast<size_t>(width) * static_cast<size_t>(height), 0u);
    QImage image(reinterpret_cast<uchar*>(pixels.data()), width, height, width * static_cast<int>(sizeof(unsigned int)), QImage::Format_ARGB32);
    if (image.isNull()) {
        return;
    }

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setRenderHint(QPainter::TextAntialiasing, false);
    painter.setFont(font);
    painter.setPen(QColor(GetRValue(color), GetGValue(color), GetBValue(color)));
    painter.drawText(QRect(0, 0, width, height), ToQtEquipTextAlignment(format) | Qt::TextSingleLine, label);
    AlphaBlendArgbToHdc(hdc, rect.left, rect.top, width, height, pixels.data(), width, height);
}
#endif

void DrawWindowText(HDC hdc, int x, int y, const std::string& text, COLORREF color, UINT format = DT_LEFT | DT_TOP | DT_SINGLELINE)
{
    if (!hdc || text.empty()) {
        return;
    }

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    RECT rect{ x, y, x + 260, y + 18 };
    HGDIOBJ oldFont = SelectObject(hdc, GetStockObject(DEFAULT_GUI_FONT));
#if RO_ENABLE_QT6_UI
    DrawEquipWindowTextQt(hdc, rect, text, color, format);
#else
    DrawTextA(hdc, text.c_str(), -1, &rect, format);
#endif
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
#if RO_ENABLE_QT6_UI
    DrawEquipWindowTextQt(hdc, drawRect, text, color, format);
#else
    DrawTextA(hdc, text.c_str(), -1, &drawRect, format);
#endif
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

bool ApplyAttachPointDelta(const CMotion* baseMotion, const CMotion* attachedMotion, POINT* inOutPoint)
{
    if (!baseMotion || !attachedMotion || !inOutPoint || baseMotion->attachInfo.empty() || attachedMotion->attachInfo.empty()) {
        return false;
    }

    const CAttachPointInfo& attached = attachedMotion->attachInfo.front();
    const CAttachPointInfo& base = baseMotion->attachInfo.front();
    if (attached.attr != base.attr) {
        return false;
    }

    inOutPoint->x += base.x - attached.x;
    inOutPoint->y += base.y - attached.y;
    return true;
}

POINT GetEquipPreviewLayerPoint(int layerPriority,
    int resolvedLayer,
    CImfRes* imfRes,
    const CMotion* motion,
    const std::string& bodyActName,
    const std::string& headActName,
    int curAction,
    int bodyMotionIndex,
    int curMotion,
    int headMotionIndex)
{
    POINT point = imfRes->GetPoint(resolvedLayer, curAction, curMotion);
    if (layerPriority != 1 || !motion || motion->attachInfo.empty()) {
        if (layerPriority < 2 || !motion || motion->attachInfo.empty()) {
            return point;
        }
    }

    CActRes* bodyActRes = g_resMgr.GetAs<CActRes>(bodyActName.c_str());
    if (!bodyActRes) {
        return point;
    }

    const CMotion* bodyMotion = bodyActRes->GetMotion(curAction, bodyMotionIndex);
    if (!bodyMotion || bodyMotion->attachInfo.empty()) {
        return point;
    }

    if (layerPriority == 1) {
        ApplyAttachPointDelta(bodyMotion, motion, &point);
        return point;
    }

    CActRes* headActRes = g_resMgr.GetAs<CActRes>(headActName.c_str());
    if (!headActRes) {
        return point;
    }

    const CMotion* headMotion = headActRes->GetMotion(curAction, headMotionIndex);
    if (!headMotion) {
        return point;
    }

    POINT headOffset{};
    ApplyAttachPointDelta(bodyMotion, headMotion, &headOffset);
    point.x += headOffset.x;
    point.y += headOffset.y;
    ApplyAttachPointDelta(headMotion, motion, &point);
    return point;
}

bool DrawEquipPreviewAccessoryMotion(HDC hdc,
    int drawX,
    int drawY,
    int curAction,
    int bodyMotionIndex,
    int headMotionIndex,
    const std::string& bodyActName,
    const std::string& headActName,
    const std::string& accessoryActName,
    const std::string& accessorySprName)
{
    if (accessoryActName.empty() || accessorySprName.empty()) {
        return false;
    }

    CActRes* bodyActRes = g_resMgr.GetAs<CActRes>(bodyActName.c_str());
    CActRes* headActRes = g_resMgr.GetAs<CActRes>(headActName.c_str());
    CActRes* accessoryActRes = g_resMgr.GetAs<CActRes>(accessoryActName.c_str());
    CSprRes* accessorySprRes = g_resMgr.GetAs<CSprRes>(accessorySprName.c_str());
    if (!bodyActRes || !headActRes || !accessoryActRes || !accessorySprRes) {
        return false;
    }

    const CMotion* bodyMotion = bodyActRes->GetMotion(curAction, bodyMotionIndex);
    const CMotion* headMotion = headActRes->GetMotion(curAction, headMotionIndex);
    const CMotion* accessoryMotion = accessoryActRes->GetMotion(curAction, headMotionIndex);
    if (!accessoryMotion) {
        accessoryMotion = accessoryActRes->GetMotion(curAction, 0);
    }
    if (!bodyMotion || !headMotion || !accessoryMotion) {
        return false;
    }

    POINT point{};
    ApplyAttachPointDelta(bodyMotion, headMotion, &point);
    ApplyAttachPointDelta(headMotion, accessoryMotion, &point);
    return DrawActMotionToHdc(hdc, drawX + point.x, drawY + point.y, accessorySprRes, accessoryMotion, accessorySprRes->m_pal);
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
    const std::string& headActName,
    int bodyMotionIndex,
    int headMotionIndex,
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

    const POINT point = GetEquipPreviewLayerPoint(layerIndex,
        resolvedLayer,
        imfRes,
        motion,
        bodyActName,
        headActName,
        curAction,
        bodyMotionIndex,
        curMotion,
        headMotionIndex);

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
    char accessoryBottomAct[260] = {};
    char accessoryBottomSpr[260] = {};
    char accessoryMidAct[260] = {};
    char accessoryMidSpr[260] = {};
    char accessoryTopAct[260] = {};
    char accessoryTopSpr[260] = {};
    char imfName[260] = {};
    char bodyPalette[260] = {};
    char headPalette[260] = {};

    const int sex = g_session.GetSex();
    int head = g_session.m_playerHead;
    const int curAction = 0;
    const int curMotion = 0;

    const std::string bodyActName = g_session.GetJobActName(g_session.m_playerJob, sex, bodyAct);
    const std::string bodySprName = g_session.GetJobSprName(g_session.m_playerJob, sex, bodySpr);
    const std::string headActName = g_session.GetHeadActName(g_session.m_playerJob, &head, sex, headAct);
    const std::string headSprName = g_session.GetHeadSprName(g_session.m_playerJob, &head, sex, headSpr);
    const std::string accessoryBottomActName = g_session.GetAccessoryActName(g_session.m_playerJob, &head, sex, g_session.m_playerAccessory, accessoryBottomAct);
    const std::string accessoryBottomSprName = g_session.GetAccessorySprName(g_session.m_playerJob, &head, sex, g_session.m_playerAccessory, accessoryBottomSpr);
    const std::string accessoryMidActName = g_session.GetAccessoryActName(g_session.m_playerJob, &head, sex, g_session.m_playerAccessory3, accessoryMidAct);
    const std::string accessoryMidSprName = g_session.GetAccessorySprName(g_session.m_playerJob, &head, sex, g_session.m_playerAccessory3, accessoryMidSpr);
    const std::string accessoryTopActName = g_session.GetAccessoryActName(g_session.m_playerJob, &head, sex, g_session.m_playerAccessory2, accessoryTopAct);
    const std::string accessoryTopSprName = g_session.GetAccessorySprName(g_session.m_playerJob, &head, sex, g_session.m_playerAccessory2, accessoryTopSpr);
    const std::string imfPath = g_session.GetImfName(g_session.m_playerJob, head, sex, imfName);
    const std::string bodyPaletteName = g_session.m_playerBodyPalette > 0
        ? g_session.GetBodyPaletteName(g_session.m_playerJob, sex, g_session.m_playerBodyPalette, bodyPalette)
        : std::string();
    const std::string headPaletteName = g_session.m_playerHeadPalette > 0
        ? g_session.GetHeadPaletteName(head, g_session.m_playerJob, sex, g_session.m_playerHeadPalette, headPalette)
        : std::string();

    bool drew = false;
    drew |= DrawEquipPreviewLayer(hdc, drawX, drawY, 0, curAction, curMotion, bodyActName, bodySprName, imfPath, bodyActName, headActName, curMotion, curMotion, bodyPaletteName);
    drew |= DrawEquipPreviewLayer(hdc, drawX, drawY, 1, curAction, curMotion, headActName, headSprName, imfPath, bodyActName, headActName, curMotion, curMotion, headPaletteName);
    if (!accessoryBottomActName.empty() && !accessoryBottomSprName.empty()) {
        drew |= DrawEquipPreviewAccessoryMotion(hdc, drawX, drawY, curAction, curMotion, curMotion, bodyActName, headActName, accessoryBottomActName, accessoryBottomSprName);
    }
    if (!accessoryMidActName.empty() && !accessoryMidSprName.empty()) {
        drew |= DrawEquipPreviewAccessoryMotion(hdc, drawX, drawY, curAction, curMotion, curMotion, bodyActName, headActName, accessoryMidActName, accessoryMidSprName);
    }
    if (!accessoryTopActName.empty() && !accessoryTopSprName.empty()) {
        drew |= DrawEquipPreviewAccessoryMotion(hdc, drawX, drawY, curAction, curMotion, curMotion, bodyActName, headActName, accessoryTopActName, accessoryTopSprName);
    }
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
    constexpr float kPreviewScaleBoost = 1.18f;

    ArgbDibSurface composeSurface;
    if (!composeSurface.EnsureSize(kComposeWidth, kComposeHeight)) {
        return false;
    }

    std::memset(composeSurface.GetBits(), 0, static_cast<size_t>(kComposeWidth) * static_cast<size_t>(kComposeHeight) * sizeof(unsigned int));

    const bool drew = DrawEquipPreviewPlayerSprite(composeSurface.GetDC(), kComposeWidth / 2, kComposeHeight - 14);
    RECT srcBounds{};
    const bool hasBounds = FindOpaqueBounds(static_cast<const unsigned int*>(composeSurface.GetBits()), kComposeWidth, kComposeHeight, &srcBounds);

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

        drawW = (std::min)(areaW, (std::max)(1, static_cast<int>(static_cast<float>(drawW) * kPreviewScaleBoost)));
        drawH = (std::min)(areaH, (std::max)(1, static_cast<int>(static_cast<float>(drawH) * kPreviewScaleBoost)));

        const int dstX = previewArea.left + (areaW - drawW) / 2;
        const int dstY = previewArea.top + (areaH - drawH) / 2;

        if (drawW == srcW && drawH == srcH) {
            AlphaBlendArgbToHdc(hdc,
                dstX,
                dstY,
                drawW,
                drawH,
                static_cast<const unsigned int*>(composeSurface.GetBits()),
                kComposeWidth,
                kComposeHeight,
                srcBounds.left,
                srcBounds.top,
                srcW,
                srcH);
        } else {
            ArgbDibSurface scaledSurface;
            if (!scaledSurface.EnsureSize(drawW, drawH)) {
                return false;
            }

            std::memset(
                scaledSurface.GetBits(),
                0,
                static_cast<size_t>(drawW) * static_cast<size_t>(drawH) * sizeof(unsigned int));

            StretchArgbToHdc(scaledSurface.GetDC(),
                0,
                0,
                drawW,
                drawH,
                static_cast<const unsigned int*>(composeSurface.GetBits()),
                kComposeWidth,
                kComposeHeight,
                srcBounds.left,
                srcBounds.top,
                srcW,
                srcH);

            AlphaBlendArgbToHdc(hdc,
                dstX,
                dstY,
                drawW,
                drawH,
                static_cast<const unsigned int*>(scaledSurface.GetBits()),
                drawW,
                drawH,
                0,
                0,
                drawW,
                drawH);
        }
    }

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
      m_backgroundLeft(),
      m_backgroundMid(),
      m_backgroundRight(),
      m_backgroundFull(),
      m_titleBarLeft(),
      m_titleBarMid(),
      m_titleBarRight(),
    m_hoveredSlot(-1),
      m_dragArmed(false),
      m_dragStartPoint{},
      m_dragItemId(0),
      m_dragItemIndex(0),
      m_dragItemEquipLocation(0),
      m_lastVisualStateToken(0ull),
      m_hasVisualStateToken(false)
{
    Create(kWindowWidth, kWindowHeight);
    Move(281, 121);
    int savedX = m_x;
    int savedY = m_y;
    if (LoadUiWindowPlacement("EquipWnd", &savedX, &savedY)) {
        g_windowMgr.ClampWindowToClient(&savedX, &savedY, m_w, m_h);
        Move(savedX, savedY);
    }
}

UIEquipWnd::~UIEquipWnd()
{
    ReleaseAssets();
}

void UIEquipWnd::SetShow(int show)
{
    UIWindow::SetShow(show);
    if (show == 0) {
        m_hoveredSlot = -1;
    }
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
    if (m_show == 0) {
        return false;
    }
    if (m_isDirty != 0 || !m_hasVisualStateToken) {
        return true;
    }
    return BuildVisualStateToken() != m_lastVisualStateToken;
}

msgresult_t UIEquipWnd::SendMsg(UIWindow* sender, int msg, msgparam_t wparam, msgparam_t lparam, msgparam_t extra)
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

    if (IsQtUiRuntimeEnabled()) {
        m_fullHeight = kWindowHeight;
        Resize(kWindowWidth, m_fullHeight);
        LayoutChildren();
        return;
    }

    if (m_backgroundFull.IsValid()) {
        m_fullHeight = kTitleBarHeight + m_backgroundFull.height;
        Resize(m_backgroundFull.width, m_fullHeight);
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
    const std::vector<const ITEM_INFO*> slotItems = BuildSlotAssignments();
    if (m_h > kMiniHeight) {
        for (const ITEM_INFO* item : slotItems) {
            if (!item) {
                continue;
            }
            GetItemIcon(*item);
        }
    }

    if (IsQtUiRuntimeEnabled()) {
        m_lastVisualStateToken = BuildVisualStateToken();
        m_hasVisualStateToken = true;
        m_isDirty = 0;
        return;
    }

    HDC hdc = AcquireDrawTarget();
    if (!hdc) {
        return;
    }

    RECT windowRect{ m_x, m_y, m_x + m_w, m_y + m_h };
    FillRectColor(hdc, windowRect, RGB(255, 255, 255));

    const int bodyTop = m_y + kTitleBarHeight;
    if (m_backgroundFull.IsValid()) {
        RECT bgRect{
            m_x,
            bodyTop,
            m_x + m_backgroundFull.width,
            bodyTop + m_backgroundFull.height
        };
        DrawBitmapPixelsStretched(hdc, m_backgroundFull, bgRect);
    } else {
        RECT bodyRect{ m_x, bodyTop, m_x + m_w, m_y + m_h };
        FillRectColor(hdc, bodyRect, RGB(255, 255, 255));

        for (int yPos = bodyTop; yPos < m_y + m_h; yPos += 8) {
            RECT leftRect{ m_x, yPos, m_x + 20, std::min(yPos + 8, m_y + m_h) };
            RECT rightRect{ m_x + m_w - 20, yPos, m_x + m_w, std::min(yPos + 8, m_y + m_h) };
            DrawBitmapPixelsStretched(hdc, m_backgroundLeft, leftRect);
            DrawBitmapPixelsStretched(hdc, m_backgroundRight, rightRect);
        }
    }

    const RECT titleStrip{ m_x, m_y, m_x + m_w, m_y + kTitleBarHeight };
    if (m_titleBarLeft.IsValid() && m_titleBarMid.IsValid() && m_titleBarRight.IsValid()) {
        const int leftW = (std::max)(0, m_titleBarLeft.width);
        const int rightW = (std::max)(0, m_titleBarRight.width);
        const int midW = (std::max)(0, static_cast<int>(titleStrip.right - titleStrip.left - leftW - rightW));
        if (leftW > 0) {
            RECT dst{ titleStrip.left, titleStrip.top, titleStrip.left + leftW, titleStrip.bottom };
            DrawBitmapPixelsSegmentTransparent(hdc, m_titleBarLeft, dst, 0, 0, leftW, (std::max)(1, m_titleBarLeft.height));
        }
        if (midW > 0) {
            RECT dst{ titleStrip.left + leftW, titleStrip.top, titleStrip.left + leftW + midW, titleStrip.bottom };
            DrawBitmapPixelsSegmentTransparent(hdc, m_titleBarMid, dst, 0, 0, (std::max)(1, m_titleBarMid.width), (std::max)(1, m_titleBarMid.height));
        }
        if (rightW > 0) {
            RECT dst{ titleStrip.right - rightW, titleStrip.top, titleStrip.right, titleStrip.bottom };
            DrawBitmapPixelsSegmentTransparent(hdc, m_titleBarRight, dst, 0, 0, rightW, (std::max)(1, m_titleBarRight.height));
        }
    } else if (m_backgroundMid.IsValid()) {
        DrawBitmapPixelsStretched(hdc, m_backgroundMid, titleStrip);
    }
    DrawWindowText(hdc, m_x + 18, m_y + 3, "Equipment", RGB(255, 255, 255));
    DrawWindowText(hdc, m_x + 17, m_y + 2, "Equipment", RGB(0, 0, 0));

    if (m_h > kMiniHeight) {
        const CGameMode* gameMode = g_modeMgr.GetCurrentGameMode();
        const bool hideDraggedItem = gameMode
            && gameMode->m_dragType == static_cast<int>(DragType::ShortcutItem)
            && gameMode->m_dragInfo.source == static_cast<int>(DragSource::EquipmentWindow)
            && gameMode->m_dragInfo.itemIndex != 0;
        RECT centerPanel{
            m_x + kCenterPanelLeft,
            m_y + kCenterPanelTop,
            m_x + (std::min)(kCenterPanelRight, m_w - 98),
            m_y + (std::min)(kCenterPanelBottom, m_h - 10)
        };

        if (!DrawEquipPreviewPlayerSpriteFitted(hdc, centerPanel)) {
            DrawWindowText(hdc, centerPanel.left + 14, centerPanel.top + 68, "No Preview", RGB(90, 90, 90));
        }

        for (size_t i = 0; i < kEquipSlots.size(); ++i) {
            const int slotX = GetEquipSlotX(kEquipSlots[i], m_w);
            const int slotY = GetEquipSlotY(kEquipSlots[i], m_h);
            RECT slotRect{
                m_x + slotX,
                m_y + slotY,
                m_x + slotX + kSlotIconSize,
                m_y + slotY + kSlotIconSize
            };
            const ITEM_INFO* drawItem = slotItems[i];
            if (drawItem
                && hideDraggedItem
                && drawItem->m_itemIndex == gameMode->m_dragInfo.itemIndex) {
                drawItem = nullptr;
            }
            if (drawItem) {
                if (const shopui::BitmapPixels* icon = GetItemIcon(*drawItem)) {
                    shopui::DrawBitmapPixelsTransparent(hdc, *icon, slotRect);
                }

                const bool leftColumn = IsLeftEquipSlot(kEquipSlots[i]);
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

                const std::string itemText = drawItem->GetEquipDisplayName();
                const UINT textFormat = (leftColumn ? DT_LEFT : DT_RIGHT) | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS;
                DrawWindowTextRect(hdc, textRect, itemText, RGB(0, 0, 0), textFormat);
            }
        }
    }

    DrawChildrenToHdc(hdc);
    ReleaseDrawTarget(hdc);

    m_lastVisualStateToken = BuildVisualStateToken();
    m_hasVisualStateToken = true;
    m_isDirty = 0;
}

void UIEquipWnd::OnLBtnDown(int x, int y)
{
    m_dragArmed = false;
    m_dragItemId = 0;
    m_dragItemIndex = 0;
    m_dragItemEquipLocation = 0;

    if (IsQtUiRuntimeEnabled()) {
        const RECT baseRect = MakeEquipRect(m_x, m_y, 3, 3, kQtButtonWidth, kQtButtonHeight);
        const RECT miniRect = MakeEquipRect(m_x, m_y, 247, 3, kQtButtonWidth, kQtButtonHeight);
        const RECT closeRect = MakeEquipRect(m_x, m_y, 265, 3, kQtButtonWidth, kQtButtonHeight);
        if (IsPointInRect(baseRect, x, y) || IsPointInRect(miniRect, x, y) || IsPointInRect(closeRect, x, y)) {
            UIWindow::OnLBtnDown(x, y);
            return;
        }
    }

    if (y >= m_y && y < m_y + kTitleBarHeight) {
        UIFrameWnd::OnLBtnDown(x, y);
        return;
    }

    if (m_h <= kMiniHeight) {
        return;
    }

    const std::vector<const ITEM_INFO*> slotItems = BuildSlotAssignments();
    for (size_t i = 0; i < kEquipSlots.size(); ++i) {
        const ITEM_INFO* item = slotItems[i];
        if (!item) {
            continue;
        }

        RECT slotRect{
            m_x + GetEquipSlotX(kEquipSlots[i], m_w),
            m_y + GetEquipSlotY(kEquipSlots[i], m_h),
            m_x + GetEquipSlotX(kEquipSlots[i], m_w) + kSlotIconSize,
            m_y + GetEquipSlotY(kEquipSlots[i], m_h) + kSlotIconSize
        };
        if (x >= slotRect.left && x < slotRect.right && y >= slotRect.top && y < slotRect.bottom) {
            m_dragArmed = true;
            m_dragStartPoint = POINT{ x, y };
            m_dragItemId = item->GetItemId();
            m_dragItemIndex = item->m_itemIndex;
            m_dragItemEquipLocation = item->m_location;
            return;
        }
    }
}

void UIEquipWnd::OnLBtnUp(int x, int y)
{
    m_dragArmed = false;
    m_dragItemId = 0;
    m_dragItemIndex = 0;
    m_dragItemEquipLocation = 0;

    if (IsQtUiRuntimeEnabled()) {
        const bool wasDragging = m_isDragging != 0;
        UIFrameWnd::OnLBtnUp(x, y);
        if (wasDragging) {
            return;
        }

        const RECT baseRect = MakeEquipRect(m_x, m_y, 3, 3, kQtButtonWidth, kQtButtonHeight);
        const RECT miniRect = MakeEquipRect(m_x, m_y, 247, 3, kQtButtonWidth, kQtButtonHeight);
        const RECT closeRect = MakeEquipRect(m_x, m_y, 265, 3, kQtButtonWidth, kQtButtonHeight);

        if (m_h == kMiniHeight && IsPointInRect(baseRect, x, y)) {
            SendMsg(this, 6, kButtonIdBase, 0, 0);
            return;
        }
        if (m_h > kMiniHeight && IsPointInRect(miniRect, x, y)) {
            SendMsg(this, 6, kButtonIdMini, 0, 0);
            return;
        }
        if (IsPointInRect(closeRect, x, y)) {
            SendMsg(this, 6, kButtonIdClose, 0, 0);
            return;
        }

        return;
    }

    UIFrameWnd::OnLBtnUp(x, y);
}

void UIEquipWnd::OnMouseMove(int x, int y)
{
    UIFrameWnd::OnMouseMove(x, y);
    UpdateHoveredSlot(x, y);
    if (!m_dragArmed) {
        return;
    }

    const int dx = x - m_dragStartPoint.x;
    const int dy = y - m_dragStartPoint.y;
    if ((dx * dx) + (dy * dy) < 16) {
        return;
    }

    if (CGameMode* gameMode = g_modeMgr.GetCurrentGameMode()) {
        gameMode->m_dragType = static_cast<int>(DragType::ShortcutItem);
        gameMode->m_dragInfo = DRAG_INFO{};
        gameMode->m_dragInfo.type = static_cast<int>(DragType::ShortcutItem);
        gameMode->m_dragInfo.source = static_cast<int>(DragSource::EquipmentWindow);
        gameMode->m_dragInfo.itemId = m_dragItemId;
        gameMode->m_dragInfo.itemIndex = m_dragItemIndex;
        gameMode->m_dragInfo.itemEquipLocation = m_dragItemEquipLocation;
        Invalidate();
        if (g_windowMgr.m_itemWnd) {
            g_windowMgr.m_itemWnd->Invalidate();
        }
        if (g_windowMgr.m_shortCutWnd) {
            g_windowMgr.m_shortCutWnd->Invalidate();
        }
    }

    m_dragArmed = false;
}

void UIEquipWnd::OnMouseHover(int x, int y)
{
    UpdateHoveredSlot(x, y);
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

            const int slotX = GetEquipSlotX(kEquipSlots[i], m_w);
            const int slotY = GetEquipSlotY(kEquipSlots[i], m_h);
            RECT slotRect{
                m_x + slotX,
                m_y + slotY,
                m_x + slotX + kSlotIconSize,
                m_y + slotY + kSlotIconSize
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

void UIEquipWnd::DragAndDrop(int x, int y, const DRAG_INFO* const dragInfo)
{
    if (m_show == 0 || m_h <= kMiniHeight || !dragInfo) {
        return;
    }
    if (dragInfo->type != static_cast<int>(DragType::ShortcutItem)
        || dragInfo->source != static_cast<int>(DragSource::InventoryWindow)
        || dragInfo->itemIndex == 0
        || dragInfo->itemEquipLocation == 0) {
        return;
    }

    if (x < m_x || x >= m_x + m_w || y < m_y + kTitleBarHeight || y >= m_y + m_h) {
        return;
    }

    if (g_modeMgr.SendMsg(
            CGameMode::GameMsg_RequestEquipInventoryItem,
            static_cast<int>(dragInfo->itemIndex),
            dragInfo->itemEquipLocation,
            0) != 0) {
        Invalidate();
        if (g_windowMgr.m_itemWnd) {
            g_windowMgr.m_itemWnd->Invalidate();
        }
    }
}

void UIEquipWnd::StoreInfo()
{
    SaveUiWindowPlacement("EquipWnd", m_x, m_y);
}

bool UIEquipWnd::IsMiniMode() const
{
    return m_h == kMiniHeight;
}

bool UIEquipWnd::GetDisplayDataForQt(DisplayData* outData) const
{
    if (!outData) {
        return false;
    }

    DisplayData data{};
    if (m_h > kMiniHeight) {
        const CGameMode* gameMode = g_modeMgr.GetCurrentGameMode();
        const bool hideDraggedItem = gameMode
            && gameMode->m_dragType == static_cast<int>(DragType::ShortcutItem)
            && gameMode->m_dragInfo.source == static_cast<int>(DragSource::EquipmentWindow)
            && gameMode->m_dragInfo.itemIndex != 0;
        const std::vector<const ITEM_INFO*> slotItems = BuildSlotAssignments();
        data.displaySlots.reserve(kEquipSlots.size());
        for (size_t i = 0; i < kEquipSlots.size(); ++i) {
            const int slotX = GetEquipSlotX(kEquipSlots[i], m_w);
            const int slotY = GetEquipSlotY(kEquipSlots[i], m_h);
            DisplaySlot slot{};
            slot.x = m_x + slotX;
            slot.y = m_y + slotY;
            slot.width = kSlotIconSize;
            slot.height = kSlotIconSize;
            slot.leftColumn = IsLeftEquipSlot(kEquipSlots[i]);

            const ITEM_INFO* drawItem = slotItems[i];
            if (drawItem
                && hideDraggedItem
                && drawItem->m_itemIndex == gameMode->m_dragInfo.itemIndex) {
                drawItem = nullptr;
            }
            if (drawItem) {
                slot.occupied = true;
                slot.hovered = static_cast<int>(i) == m_hoveredSlot;
                slot.itemId = drawItem->GetItemId();
                slot.label = drawItem->GetEquipDisplayName();
            }
            data.displaySlots.push_back(slot);
        }
    }

    *outData = std::move(data);
    return true;
}

bool UIEquipWnd::GetHoveredItemForQt(shopui::ItemHoverInfo* outData) const
{
    if (!outData || m_show == 0 || IsMiniMode() || m_hoveredSlot < 0 || m_hoveredSlot >= static_cast<int>(kEquipSlots.size())) {
        return false;
    }

    const std::vector<const ITEM_INFO*> slotItems = BuildSlotAssignments();
    const ITEM_INFO* item = slotItems[static_cast<size_t>(m_hoveredSlot)];
    if (!item) {
        return false;
    }

    outData->anchorRect = RECT{
        m_x + GetEquipSlotX(kEquipSlots[static_cast<size_t>(m_hoveredSlot)], m_w),
        m_y + GetEquipSlotY(kEquipSlots[static_cast<size_t>(m_hoveredSlot)], m_h),
        m_x + GetEquipSlotX(kEquipSlots[static_cast<size_t>(m_hoveredSlot)], m_w) + kSlotIconSize,
        m_y + GetEquipSlotY(kEquipSlots[static_cast<size_t>(m_hoveredSlot)], m_h) + kSlotIconSize,
    };
    outData->text = shopui::BuildItemHoverText(*item);
    outData->itemId = item->GetItemId();
    return outData->IsValid();
}

int UIEquipWnd::GetQtSystemButtonCount() const
{
    return 3;
}

bool UIEquipWnd::GetQtSystemButtonDisplayForQt(int index, QtButtonDisplay* outData) const
{
    if (!outData || index < 0 || index >= GetQtSystemButtonCount()) {
        return false;
    }

    switch (index) {
    case 0:
        outData->id = kButtonIdBase;
        outData->x = m_x + 247;
        outData->y = m_y + 3;
        outData->width = kQtButtonWidth;
        outData->height = kQtButtonHeight;
        outData->label = "B";
        outData->visible = IsMiniMode();
        return true;
    case 1:
        outData->id = kButtonIdMini;
        outData->x = m_x + 247;
        outData->y = m_y + 3;
        outData->width = kQtButtonWidth;
        outData->height = kQtButtonHeight;
        outData->label = "_";
        outData->visible = !IsMiniMode();
        return true;
    case 2:
        outData->id = kButtonIdClose;
        outData->x = m_x + 265;
        outData->y = m_y + 3;
        outData->width = kQtButtonWidth;
        outData->height = kQtButtonHeight;
        outData->label = "X";
        outData->visible = true;
        return true;
    default:
        return false;
    }
}

bool UIEquipWnd::BuildQtPreviewImage(QImage* outImage) const
{
#if RO_ENABLE_QT6_UI
    if (!outImage) {
        return false;
    }

    constexpr int kPreviewWidth = kCenterPanelRight - kCenterPanelLeft;
    constexpr int kPreviewHeight = kCenterPanelBottom - kCenterPanelTop;
    ArgbDibSurface previewSurface;
    if (!previewSurface.EnsureSize(kPreviewWidth, kPreviewHeight)) {
        outImage->fill(Qt::transparent);
        return false;
    }

    std::memset(
        previewSurface.GetBits(),
        0,
        static_cast<size_t>(kPreviewWidth) * static_cast<size_t>(kPreviewHeight) * sizeof(unsigned int));

    const RECT previewRect{ 0, 0, kPreviewWidth, kPreviewHeight };
    DrawEquipPreviewPlayerSpriteFitted(previewSurface.GetDC(), previewRect);

    const QImage source(
        reinterpret_cast<const uchar*>(previewSurface.GetBits()),
        kPreviewWidth,
        kPreviewHeight,
        kPreviewWidth * static_cast<int>(sizeof(unsigned int)),
        QImage::Format_ARGB32);
    *outImage = source.copy();
    return !outImage->isNull();
#else
    (void)outImage;
    return false;
#endif
}

unsigned long long UIEquipWnd::GetQtPreviewRevision() const
{
    return BuildVisualStateToken();
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
    m_backgroundLeft = LoadBitmapPixelsFromGameData(ResolveUiAssetPath("itemwin_left.bmp"));
    m_backgroundMid = LoadBitmapPixelsFromGameData(ResolveUiAssetPath("itemwin_mid.bmp"));
    m_backgroundRight = LoadBitmapPixelsFromGameData(ResolveUiAssetPath("itemwin_right.bmp"));
    m_backgroundFull = LoadBitmapPixelsFromGameData(ResolveUiAssetPath("equipwin_bg.bmp"));
    m_titleBarLeft = LoadBitmapPixelsFromGameData(ResolveUiAssetPath("titlebar_left.bmp"));
    m_titleBarMid = LoadBitmapPixelsFromGameData(ResolveUiAssetPath("titlebar_mid.bmp"));
    m_titleBarRight = LoadBitmapPixelsFromGameData(ResolveUiAssetPath("titlebar_right.bmp"));
}

void UIEquipWnd::ReleaseAssets()
{
    m_backgroundLeft.Clear();
    m_backgroundMid.Clear();
    m_backgroundRight.Clear();
    m_backgroundFull.Clear();
    m_titleBarLeft.Clear();
    m_titleBarMid.Clear();
    m_titleBarRight.Clear();
    m_iconCache.clear();
}

void UIEquipWnd::SetMiniMode(bool miniMode)
{
    Resize(kWindowWidth, miniMode ? kMiniHeight : m_fullHeight);
    LayoutChildren();
}

void UIEquipWnd::UpdateHoveredSlot(int globalX, int globalY)
{
    const int oldHoveredSlot = m_hoveredSlot;
    m_hoveredSlot = -1;
    if (IsMiniMode()) {
        if (oldHoveredSlot != m_hoveredSlot) {
            Invalidate();
        }
        return;
    }

    for (size_t i = 0; i < kEquipSlots.size(); ++i) {
        const int slotX = GetEquipSlotX(kEquipSlots[i], m_w);
        const int slotY = GetEquipSlotY(kEquipSlots[i], m_h);
        const RECT slotRect{
            m_x + slotX,
            m_y + slotY,
            m_x + slotX + kSlotIconSize,
            m_y + slotY + kSlotIconSize
        };
        if (globalX >= slotRect.left && globalX < slotRect.right && globalY >= slotRect.top && globalY < slotRect.bottom) {
            m_hoveredSlot = static_cast<int>(i);
            if (oldHoveredSlot != m_hoveredSlot) {
                Invalidate();
            }
            return;
        }
    }

    if (oldHoveredSlot != m_hoveredSlot) {
        Invalidate();
    }
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


const shopui::BitmapPixels* UIEquipWnd::GetItemIcon(const ITEM_INFO& item)
{
    const unsigned int itemId = item.GetItemId();
    const auto found = m_iconCache.find(itemId);
    if (found != m_iconCache.end()) {
        return found->second.IsValid() ? &found->second : nullptr;
    }

    shopui::BitmapPixels bitmap;
    shopui::TryLoadItemIconPixels(item, &bitmap);

    auto inserted = m_iconCache.emplace(itemId, std::move(bitmap));
    return inserted.first->second.IsValid() ? &inserted.first->second : nullptr;
}

unsigned long long UIEquipWnd::BuildVisualStateToken() const
{
    unsigned long long hash = 1469598103934665603ull;
    HashTokenValue(&hash, static_cast<unsigned long long>(m_show));
    HashTokenValue(&hash, static_cast<unsigned long long>(m_x));
    HashTokenValue(&hash, static_cast<unsigned long long>(m_y));
    HashTokenValue(&hash, static_cast<unsigned long long>(m_w));
    HashTokenValue(&hash, static_cast<unsigned long long>(m_h));
    HashTokenValue(&hash, static_cast<unsigned long long>(static_cast<unsigned int>(m_hoveredSlot + 1)));
    if (const CGameMode* gameMode = g_modeMgr.GetCurrentGameMode()) {
        HashTokenValue(&hash, static_cast<unsigned long long>(gameMode->m_dragType));
        HashTokenValue(&hash, static_cast<unsigned long long>(gameMode->m_dragInfo.source));
        HashTokenValue(&hash, static_cast<unsigned long long>(gameMode->m_dragInfo.itemIndex));
        HashTokenValue(&hash, static_cast<unsigned long long>(gameMode->m_dragInfo.itemId));
    }

    const std::list<ITEM_INFO>& items = g_session.GetInventoryItems();
    HashTokenValue(&hash, static_cast<unsigned long long>(items.size()));
    for (const ITEM_INFO& item : items) {
        HashTokenValue(&hash, static_cast<unsigned long long>(item.m_itemType));
        HashTokenValue(&hash, static_cast<unsigned long long>(item.m_location));
        HashTokenValue(&hash, static_cast<unsigned long long>(item.m_itemIndex));
        HashTokenValue(&hash, static_cast<unsigned long long>(item.m_wearLocation));
        HashTokenValue(&hash, static_cast<unsigned long long>(item.m_num));
        HashTokenValue(&hash, static_cast<unsigned long long>(item.m_isIdentified));
        HashTokenValue(&hash, static_cast<unsigned long long>(item.m_isDamaged));
        HashTokenValue(&hash, static_cast<unsigned long long>(item.m_refiningLevel));
        for (int slotValue : item.m_slot) {
            HashTokenValue(&hash, static_cast<unsigned long long>(slotValue));
        }
        HashTokenString(&hash, item.m_itemName);
    }

    return hash;
}

