#pragma once

#include "core/File.h"
#include "item/Item.h"
#include "render/DC.h"
#include "res/Bitmap.h"

#include <windows.h>

#if RO_ENABLE_QT6_UI
#include <QFont>
#include <QFontMetrics>
#include <QImage>
#include <QPainter>
#include <QString>
#endif

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#pragma comment(lib, "msimg32.lib")

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
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(hdc, &rect, brush);
    DeleteObject(brush);
}

inline void FrameRectColor(HDC hdc, const RECT& rect, COLORREF color)
{
    HBRUSH brush = CreateSolidBrush(color);
    FrameRect(hdc, &rect, brush);
    DeleteObject(brush);
}

inline HFONT GetUiFont()
{
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
}

#if RO_ENABLE_QT6_UI
inline QFont BuildUiFontFromHdc(HDC hdc)
{
    LOGFONTA logFont{};
    if (hdc) {
        if (HGDIOBJ fontObject = GetCurrentObject(hdc, OBJ_FONT)) {
            GetObjectA(fontObject, sizeof(logFont), &logFont);
        }
    }

    const QString family = logFont.lfFaceName[0] != '\0'
        ? QString::fromLocal8Bit(logFont.lfFaceName)
        : QStringLiteral("MS Sans Serif");
    QFont font(family);
    font.setPixelSize(logFont.lfHeight != 0 ? (std::max)(1, std::abs(logFont.lfHeight)) : 11);
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

inline std::string GetItemDisplayName(const ITEM_INFO& item)
{
    const std::string displayName = item.GetDisplayName();
    if (!displayName.empty()) {
        return displayName;
    }
    if (!item.m_itemName.empty()) {
        return item.m_itemName;
    }
    return "Unknown Item";
}

} // namespace shopui
