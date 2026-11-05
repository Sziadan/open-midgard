#pragma once

#include "core/File.h"
#include "item/Item.h"
#include "render/DC.h"
#include "res/Bitmap.h"

#include "platform/WindowsCompat.h"

#if RO_ENABLE_QT6_UI
#include <QFont>
#include <QFontMetrics>
#include <QImage>
#include <QPainter>
#include <QString>
#endif

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#if RO_PLATFORM_WINDOWS
#pragma comment(lib, "msimg32.lib")
#endif

namespace shopui {

struct BitmapPixels {
    std::vector<unsigned int> pixels;
    int width = 0;
    int height = 0;

    bool IsValid() const
    {
        return width > 0
            && height > 0
            && pixels.size() >= static_cast<size_t>(width) * static_cast<size_t>(height);
    }

    void Clear()
    {
        pixels.clear();
        width = 0;
        height = 0;
    }
};

struct ItemHoverInfo {
    RECT anchorRect{ 0, 0, 0, 0 };
    std::string text;
    unsigned int itemId = 0;
    bool identified = true;
    unsigned int nameColorArgb = 0xffffffffu;

    bool IsValid() const
    {
        return anchorRect.right > anchorRect.left
            && anchorRect.bottom > anchorRect.top
            && !text.empty();
    }
};

inline std::string ToLowerAscii(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        if (ch >= 'A' && ch <= 'Z') {
            return static_cast<char>(ch - 'A' + 'a');
        }
        return static_cast<char>(ch);
    });
    return value;
}

inline std::string NormalizeSlash(std::string value)
{
    std::replace(value.begin(), value.end(), '/', '\\');
    return value;
}

inline void AddUniqueCandidate(std::vector<std::string>& out, const std::string& raw)
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

inline std::vector<std::string> BuildUiAssetCandidates(const char* fileName)
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

inline std::string ResolveUiAssetPath(const char* fileName)
{
    for (const std::string& candidate : BuildUiAssetCandidates(fileName)) {
        if (g_fileMgr.IsDataExist(candidate.c_str())) {
            return candidate;
        }
    }
    return NormalizeSlash(fileName ? fileName : "");
}

inline BitmapPixels LoadBitmapPixelsFromGameData(const std::string& path, bool applyTransparentKey = false)
{
    BitmapPixels bitmap;
    u32* rawPixels = nullptr;
    if (!LoadBgraPixelsFromGameData(path.c_str(), &rawPixels, &bitmap.width, &bitmap.height)
        || !rawPixels
        || bitmap.width <= 0
        || bitmap.height <= 0) {
        delete[] rawPixels;
        bitmap.Clear();
        return bitmap;
    }

    const size_t pixelCount = static_cast<size_t>(bitmap.width) * static_cast<size_t>(bitmap.height);
    bitmap.pixels.assign(rawPixels, rawPixels + pixelCount);
    delete[] rawPixels;

    if (applyTransparentKey) {
        for (unsigned int& pixel : bitmap.pixels) {
            if ((pixel & 0x00FFFFFFu) == 0x00FF00FFu) {
                pixel = 0;
            }
        }
    }

    return bitmap;
}

inline void DrawBitmapPixelsTransparent(HDC target, const BitmapPixels& bitmap, const RECT& dst)
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

inline void FillRectColor(HDC hdc, const RECT& rect, COLORREF color)
{
#if RO_PLATFORM_WINDOWS
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(hdc, &rect, brush);
    DeleteObject(brush);
#else
    (void)hdc;
    (void)rect;
    (void)color;
#endif
}

inline void FrameRectColor(HDC hdc, const RECT& rect, COLORREF color)
{
#if RO_PLATFORM_WINDOWS
    HBRUSH brush = CreateSolidBrush(color);
    FrameRect(hdc, &rect, brush);
    DeleteObject(brush);
#else
    (void)hdc;
    (void)rect;
    (void)color;
#endif
}

inline HFONT GetUiFont()
{
#if RO_PLATFORM_WINDOWS
    static HFONT s_font = CreateFontA(
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
    return s_font;
#else
    return nullptr;
#endif
}

#if RO_ENABLE_QT6_UI
inline QFont BuildUiFontFromHdc(HDC hdc)
{
    LOGFONTA logFont{};
#if RO_PLATFORM_WINDOWS
    if (hdc) {
        if (HGDIOBJ fontObject = GetCurrentObject(hdc, OBJ_FONT)) {
            GetObjectA(fontObject, sizeof(logFont), &logFont);
        }
    }
#else
    (void)hdc;
    std::strncpy(logFont.lfFaceName, "MS Sans Serif", LF_FACESIZE - 1);
    logFont.lfHeight = -11;
    logFont.lfWeight = FW_NORMAL;
#endif

    const QString family = logFont.lfFaceName[0] != '\0'
        ? QString::fromLocal8Bit(logFont.lfFaceName)
        : QStringLiteral("MS Sans Serif");
    QFont font(family);
    font.setPixelSize(logFont.lfHeight != 0 ? (std::max)(1, static_cast<int>(std::abs(logFont.lfHeight))) : 11);
    font.setBold(logFont.lfWeight >= FW_BOLD);
    font.setStyleStrategy(QFont::NoAntialias);
    return font;
}

inline Qt::Alignment ToQtTextAlignment(UINT format)
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

inline void DrawWindowTextRectQt(HDC hdc, const RECT& rect, const std::string& text, COLORREF color, UINT format)
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
    const QFont font = BuildUiFontFromHdc(hdc);
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
    painter.drawText(QRect(0, 0, width, height), ToQtTextAlignment(format) | Qt::TextSingleLine, label);
    AlphaBlendArgbToHdc(hdc, rect.left, rect.top, width, height, pixels.data(), width, height);
}
#endif

inline void DrawWindowTextRect(HDC hdc, const RECT& rect, const std::string& text, COLORREF color, UINT format)
{
    if (!hdc || text.empty() || rect.right <= rect.left || rect.bottom <= rect.top) {
        return;
    }

#if RO_PLATFORM_WINDOWS
    RECT drawRect = rect;
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    HGDIOBJ oldFont = SelectObject(hdc, GetUiFont());
#if RO_ENABLE_QT6_UI
    DrawWindowTextRectQt(hdc, drawRect, text, color, format);
#else
    DrawTextA(hdc, text.c_str(), -1, &drawRect, format);
#endif
    SelectObject(hdc, oldFont);
#elif RO_ENABLE_QT6_UI
    DrawWindowTextRectQt(hdc, rect, text, color, format);
#else
    (void)color;
    (void)format;
#endif
}

inline RECT MakeRect(int x, int y, int w, int h)
{
    RECT rc = { x, y, x + w, y + h };
    return rc;
}

inline void HashTokenValue(unsigned long long* hash, unsigned long long value)
{
    if (!hash) {
        return;
    }
    *hash ^= value;
    *hash *= 1099511628211ull;
}

inline void HashTokenString(unsigned long long* hash, const std::string& value)
{
    if (!hash) {
        return;
    }
    for (unsigned char ch : value) {
        HashTokenValue(hash, static_cast<unsigned long long>(ch));
    }
    HashTokenValue(hash, 0xFFull);
}

inline void DrawFrameWindow(HDC hdc, const RECT& bounds, const char* title)
{
    const RECT titleRect = MakeRect(bounds.left, bounds.top, bounds.right - bounds.left, 17);
    const RECT bodyRect = MakeRect(bounds.left + 1, bounds.top + 17, bounds.right - bounds.left - 2, bounds.bottom - bounds.top - 18);
    FillRectColor(hdc, bounds, RGB(220, 220, 220));
    FillRectColor(hdc, titleRect, RGB(82, 101, 123));
    FillRectColor(hdc, bodyRect, RGB(236, 236, 236));
    FrameRectColor(hdc, bounds, RGB(72, 72, 72));
    const RECT titleTextRect = MakeRect(titleRect.left + 6, titleRect.top + 1, titleRect.right - titleRect.left - 12, titleRect.bottom - titleRect.top - 2);
    DrawWindowTextRect(hdc, titleTextRect, title ? title : "", RGB(255, 255, 255), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

inline std::vector<std::string> BuildItemIconCandidates(const ITEM_INFO& item)
{
    static const char* kUiKorPrefix =
        "texture\\"
        "\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA"
        "\\";

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

inline std::vector<std::string> BuildItemCollectionCandidates(const ITEM_INFO& item)
{
    static const char* kUiKorPrefix =
        "texture\\"
        "\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA"
        "\\collection\\";

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
        "data\\texture\\\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\collection\\",
        "collection\\",
        "texture\\collection\\",
        "texture\\interface\\collection\\",
        "data\\collection\\",
        "data\\texture\\collection\\",
        "data\\texture\\interface\\collection\\",
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

inline bool TryLoadItemIconPixels(const ITEM_INFO& item, BitmapPixels* outBitmap)
{
    if (!outBitmap) {
        return false;
    }

    outBitmap->Clear();
    for (const std::string& candidate : BuildItemIconCandidates(item)) {
        if (!g_fileMgr.IsDataExist(candidate.c_str())) {
            continue;
        }

        *outBitmap = LoadBitmapPixelsFromGameData(candidate, true);
        if (outBitmap->IsValid()) {
            return true;
        }
    }

    return false;
}

inline bool TryLoadItemCollectionPixels(const ITEM_INFO& item, BitmapPixels* outBitmap)
{
    if (!outBitmap) {
        return false;
    }

    outBitmap->Clear();
    for (const std::string& candidate : BuildItemCollectionCandidates(item)) {
        if (!g_fileMgr.IsDataExist(candidate.c_str())) {
            continue;
        }

        *outBitmap = LoadBitmapPixelsFromGameData(candidate, true);
        if (outBitmap->IsValid()) {
            return true;
        }
    }

    return false;
}

inline std::vector<std::string> BuildCardIllustCandidates(const ITEM_INFO& item)
{
    static const char* kIllustPrefix =
        "texture\\"
        "\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA"
        "\\Illust\\";
    static const char* kCardBmpPrefix =
        "texture\\"
        "\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA"
        "\\cardbmp\\";

    std::vector<std::string> out;
    std::string illustName = item.GetCardIllustName();
    if (illustName.empty()) {
        return out;
    }

    illustName = NormalizeSlash(illustName);
    AddUniqueCandidate(out, illustName);
    AddUniqueCandidate(out, illustName + ".bmp");

    const char* prefixes[] = {
        kIllustPrefix,
        kCardBmpPrefix,
        "data\\texture\\\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\Illust\\",
        "data\\texture\\\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\cardbmp\\",
        "Illust\\",
        "cardbmp\\",
        nullptr
    };

    for (int index = 0; prefixes[index]; ++index) {
        AddUniqueCandidate(out, std::string(prefixes[index]) + illustName);
        AddUniqueCandidate(out, std::string(prefixes[index]) + illustName + ".bmp");
    }

    return out;
}

inline bool TryLoadCardIllustPixels(const ITEM_INFO& item, BitmapPixels* outBitmap)
{
    if (!outBitmap) {
        return false;
    }

    outBitmap->Clear();
    for (const std::string& candidate : BuildCardIllustCandidates(item)) {
        if (!g_fileMgr.IsDataExist(candidate.c_str())) {
            continue;
        }

        *outBitmap = LoadBitmapPixelsFromGameData(candidate, true);
        if (outBitmap->IsValid()) {
            return true;
        }
    }

    return false;
}

inline std::string GetItemDisplayName(const ITEM_INFO& item, bool includeSlotSuffix = true)
{
    std::string displayName;
    switch (item.m_itemType) {
    case 4:
    case 5:
    case 8:
    case 9:
    case 11:
    case 12:
    case 13:
    case 14:
    case 15:
    case 19:
        displayName = item.m_isIdentified != 0 ? item.GetEquipDisplayName(includeSlotSuffix) : item.GetDisplayName(includeSlotSuffix);
        break;
    default:
        displayName = item.GetDisplayName(includeSlotSuffix);
        break;
    }
    if (!displayName.empty()) {
        return displayName;
    }
    if (!item.m_itemName.empty()) {
        return item.m_itemName;
    }
    return "Unknown Item";
}

inline std::string BuildItemHoverNameText(const ITEM_INFO& item)
{
    // Primary GetItemName does not append GetSocketNumber; list consumers do so explicitly.
    return GetItemDisplayName(item, false);
}

inline std::string BuildItemHoverText(const ITEM_INFO& item)
{
    std::string text = BuildItemHoverNameText(item);
    text += ": ";
    text += std::to_string((std::max)(1, item.m_num));
    text += " ea";
    return text;
}

inline std::string BuildGroundItemHoverText(const std::string& itemNameValue,
    unsigned int itemId,
    bool identified,
    unsigned int amount)
{
    std::string itemName = itemNameValue;
    if (itemName.empty() && itemId != 0) {
        itemName = g_ttemmgr.GetDisplayName(itemId, identified);
    }
    if (itemName.empty()) {
        itemName = "Item";
    }

    char amountText[64]{};
    std::snprintf(amountText, sizeof(amountText), "%s: %u ea", itemName.c_str(), amount);
    return amountText;
}

} // namespace shopui
