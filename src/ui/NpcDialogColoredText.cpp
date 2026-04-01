#include "NpcDialogColoredText.h"

#include "render/DC.h"

#if RO_ENABLE_QT6_UI
#include <QFont>
#include <QFontMetrics>
#include <QImage>
#include <QPainter>
#include <QString>
#endif

namespace {

bool IsHexDigit(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

int HexValue(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    return 0;
}

bool IsColorCodeAt(const std::string& t, size_t i)
{
    if (i + 7 > t.size() || t[i] != '^') {
        return false;
    }
    for (size_t k = 1; k <= 6; ++k) {
        if (!IsHexDigit(t[i + k])) {
            return false;
        }
    }
    return true;
}

bool TryConsumeColorCode(const std::string& t, size_t& i, COLORREF& out)
{
    if (!IsColorCodeAt(t, i)) {
        return false;
    }
    const int r = (HexValue(t[i + 1]) << 4) | HexValue(t[i + 2]);
    const int g = (HexValue(t[i + 3]) << 4) | HexValue(t[i + 4]);
    const int b = (HexValue(t[i + 5]) << 4) | HexValue(t[i + 6]);
    out = RGB(r, g, b);
    i += 7;
    return true;
}

#if RO_ENABLE_QT6_UI
QFont BuildDialogFontFromHdc(HDC hdc)
{
    LOGFONTA logFont{};
    const HGDIOBJ fontObject = hdc ? GetCurrentObject(hdc, OBJ_FONT) : nullptr;
    if (fontObject) {
        GetObjectA(fontObject, sizeof(logFont), &logFont);
    }

    const QString family = logFont.lfFaceName[0] != '\0'
        ? QString::fromLocal8Bit(logFont.lfFaceName)
        : QStringLiteral("MS Sans Serif");
    QFont font(family);
    font.setPixelSize(logFont.lfHeight != 0 ? (std::max)(1, std::abs(logFont.lfHeight)) : 13);
    font.setBold(logFont.lfWeight >= FW_BOLD);
    font.setItalic(logFont.lfItalic != 0);
    font.setUnderline(logFont.lfUnderline != 0);
    font.setStrikeOut(logFont.lfStrikeOut != 0);
    font.setStyleStrategy(QFont::NoAntialias);
    return font;
}

QString ToQtText(const std::string& text)
{
    return QString::fromLocal8Bit(text.c_str(), static_cast<int>(text.size()));
}

void DrawTextRun(QPainter& painter, const QFontMetrics& metrics, int x, int y, COLORREF color, const std::string& text)
{
    if (text.empty()) {
        return;
    }

    painter.setPen(QColor(GetRValue(color), GetGValue(color), GetBValue(color)));
    painter.drawText(x, y + metrics.ascent(), ToQtText(text));
}

int MeasureTextWidth(const QFontMetrics& metrics, const std::string& text)
{
    if (text.empty()) {
        return 0;
    }
    return metrics.horizontalAdvance(ToQtText(text));
}

void DrawFragmentWrappedQt(
    QPainter& painter,
    const QFontMetrics& metrics,
    int areaWidth,
    int areaHeight,
    int& x,
    int& y,
    int lineHeight,
    COLORREF color,
    const std::string& frag)
{
    if (frag.empty()) {
        return;
    }

    size_t p = 0;
    while (p < frag.size()) {
        if (frag[p] == ' ' || frag[p] == '\t') {
            size_t q = p;
            while (q < frag.size() && (frag[q] == ' ' || frag[q] == '\t')) {
                ++q;
            }
            const std::string ws = frag.substr(p, q - p);
            const int width = MeasureTextWidth(metrics, ws);
            if (x + width > areaWidth && x > 0) {
                x = 0;
                y += lineHeight;
            }
            if (y + lineHeight > areaHeight) {
                return;
            }
            DrawTextRun(painter, metrics, x, y, color, ws);
            x += width;
            p = q;
            continue;
        }

        size_t q = p;
        while (q < frag.size() && frag[q] != ' ' && frag[q] != '\t') {
            ++q;
        }
        const std::string word = frag.substr(p, q - p);
        const int width = MeasureTextWidth(metrics, word);

        if (x + width > areaWidth && x > 0) {
            x = 0;
            y += lineHeight;
        }
        if (y + lineHeight > areaHeight) {
            return;
        }

        if (x + width > areaWidth && x == 0) {
            size_t wi = 0;
            while (wi < word.size()) {
                if (y + lineHeight > areaHeight) {
                    return;
                }
                int lo = 1;
                int hi = static_cast<int>(word.size() - wi);
                int best = 1;
                while (lo <= hi) {
                    const int mid = (lo + hi) / 2;
                    const int midWidth = MeasureTextWidth(metrics, word.substr(wi, static_cast<size_t>(mid)));
                    if (midWidth <= areaWidth) {
                        best = mid;
                        lo = mid + 1;
                    } else {
                        hi = mid - 1;
                    }
                }
                if (best < 1) {
                    best = 1;
                }
                const std::string part = word.substr(wi, static_cast<size_t>(best));
                DrawTextRun(painter, metrics, x, y, color, part);
                x += MeasureTextWidth(metrics, part);
                wi += static_cast<size_t>(best);
                if (wi < word.size()) {
                    x = 0;
                    y += lineHeight;
                }
            }
            p = q;
            continue;
        }

        DrawTextRun(painter, metrics, x, y, color, word);
        x += width;
        p = q;
    }
}
#endif

#if !RO_ENABLE_QT6_UI
void DrawFragmentWrapped(
    HDC hdc, const RECT& area, int& x, int& y, int lineHeight, COLORREF color, const std::string& frag)
{
    if (frag.empty()) {
        return;
    }

    SetTextColor(hdc, color);
    const int left = area.left;
    const int right = area.right;
    size_t p = 0;

    while (p < frag.size()) {
        if (frag[p] == ' ' || frag[p] == '\t') {
            size_t q = p;
            while (q < frag.size() && (frag[q] == ' ' || frag[q] == '\t')) {
                ++q;
            }
            const std::string ws = frag.substr(p, q - p);
            SIZE sz{};
            GetTextExtentPoint32A(hdc, ws.c_str(), static_cast<int>(ws.size()), &sz);
            if (x + sz.cx > right && x > left) {
                x = left;
                y += lineHeight;
            }
            TextOutA(hdc, x, y, ws.c_str(), static_cast<int>(ws.size()));
            x += sz.cx;
            p = q;
            continue;
        }

        size_t q = p;
        while (q < frag.size() && frag[q] != ' ' && frag[q] != '\t') {
            ++q;
        }
        const std::string word = frag.substr(p, q - p);
        SIZE sz{};
        GetTextExtentPoint32A(hdc, word.c_str(), static_cast<int>(word.size()), &sz);

        if (x + sz.cx > right && x > left) {
            x = left;
            y += lineHeight;
        }

        if (x + sz.cx > right && x == left) {
            size_t wi = 0;
            while (wi < word.size()) {
                if (y + lineHeight > area.bottom) {
                    return;
                }
                int lo = 1;
                int hi = static_cast<int>(word.size() - wi);
                int best = 1;
                while (lo <= hi) {
                    const int mid = (lo + hi) / 2;
                    GetTextExtentPoint32A(hdc, word.c_str() + wi, mid, &sz);
                    if (x + sz.cx <= right) {
                        best = mid;
                        lo = mid + 1;
                    } else {
                        hi = mid - 1;
                    }
                }
                if (best < 1) {
                    best = 1;
                }
                GetTextExtentPoint32A(hdc, word.c_str() + wi, best, &sz);
                TextOutA(hdc, x, y, word.c_str() + wi, best);
                x += sz.cx;
                wi += static_cast<size_t>(best);
                if (wi < word.size()) {
                    x = left;
                    y += lineHeight;
                }
            }
            p = q;
            continue;
        }

        TextOutA(hdc, x, y, word.c_str(), static_cast<int>(word.size()));
        x += sz.cx;
        p = q;
    }
}
#endif

} // namespace

void DrawNpcSayDialogColoredText(HDC hdc, const RECT& textRect, const std::string& text)
{
#if RO_ENABLE_QT6_UI
    const int width = textRect.right - textRect.left;
    const int height = textRect.bottom - textRect.top;
    if (!hdc || width <= 0 || height <= 0 || text.empty()) {
        return;
    }

    std::vector<unsigned int> pixels(static_cast<size_t>(width) * static_cast<size_t>(height), 0u);
    QImage image(reinterpret_cast<uchar*>(pixels.data()), width, height, width * static_cast<int>(sizeof(unsigned int)), QImage::Format_ARGB32);
    if (image.isNull()) {
        return;
    }

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setRenderHint(QPainter::TextAntialiasing, false);
    painter.setFont(BuildDialogFontFromHdc(hdc));
    const QFontMetrics metrics(painter.font());
    const int lineHeight = (std::max)(1, metrics.height());
    int x = 0;
    int y = 0;
    COLORREF color = RGB(0, 0, 0);
    size_t i = 0;

    while (i < text.size()) {
        if (y + lineHeight > height) {
            break;
        }
        if (text[i] == '\r' && i + 1 < text.size() && text[i + 1] == '\n') {
            i += 2;
            x = 0;
            y += lineHeight;
            continue;
        }
        if (text[i] == '\n') {
            ++i;
            x = 0;
            y += lineHeight;
            continue;
        }

        COLORREF newColor{};
        if (TryConsumeColorCode(text, i, newColor)) {
            color = newColor;
            continue;
        }

        const size_t start = i;
        while (i < text.size()) {
            if (text[i] == '\r' || text[i] == '\n') {
                break;
            }
            if (text[i] == '^' && IsColorCodeAt(text, i)) {
                break;
            }
            ++i;
        }

        if (i > start) {
            DrawFragmentWrappedQt(painter, metrics, width, height, x, y, lineHeight, color, text.substr(start, i - start));
        }
    }

    AlphaBlendArgbToHdc(hdc, textRect.left, textRect.top, width, height, pixels.data(), width, height);
#else
    TEXTMETRICA tm{};
    GetTextMetricsA(hdc, &tm);
    const int lineHeight = tm.tmHeight;
    int x = textRect.left;
    int y = textRect.top;
    COLORREF color = RGB(0, 0, 0);
    size_t i = 0;

    while (i < text.size()) {
        if (y + lineHeight > textRect.bottom) {
            break;
        }

        if (text[i] == '\r' && i + 1 < text.size() && text[i + 1] == '\n') {
            i += 2;
            x = textRect.left;
            y += lineHeight;
            continue;
        }
        if (text[i] == '\n') {
            ++i;
            x = textRect.left;
            y += lineHeight;
            continue;
        }

        COLORREF newColor{};
        if (TryConsumeColorCode(text, i, newColor)) {
            color = newColor;
            continue;
        }

        const size_t start = i;
        while (i < text.size()) {
            if (text[i] == '\r' || text[i] == '\n') {
                break;
            }
            if (text[i] == '^' && IsColorCodeAt(text, i)) {
                break;
            }
            ++i;
        }

        if (i > start) {
            DrawFragmentWrapped(
                hdc, textRect, x, y, lineHeight, color, text.substr(start, i - start));
        }
    }
#endif
}

void DrawNpcMenuOptionColoredText(HDC hdc, const RECT& textRect, const std::string& text)
{
#if RO_ENABLE_QT6_UI
    const int width = textRect.right - textRect.left;
    const int height = textRect.bottom - textRect.top;
    if (!hdc || width <= 0 || height <= 0 || text.empty()) {
        return;
    }

    std::vector<unsigned int> pixels(static_cast<size_t>(width) * static_cast<size_t>(height), 0u);
    QImage image(reinterpret_cast<uchar*>(pixels.data()), width, height, width * static_cast<int>(sizeof(unsigned int)), QImage::Format_ARGB32);
    if (image.isNull()) {
        return;
    }

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setRenderHint(QPainter::TextAntialiasing, false);
    painter.setFont(BuildDialogFontFromHdc(hdc));
    const QFontMetrics metrics(painter.font());
    const int lineHeight = (std::max)(1, metrics.height());
    const int yBase = (std::max)(0, (height - lineHeight) / 2);

    COLORREF color = RGB(0, 0, 0);
    int x = 0;
    size_t i = 0;

    while (i < text.size() && x < width) {
        COLORREF newColor{};
        if (TryConsumeColorCode(text, i, newColor)) {
            color = newColor;
            continue;
        }

        const size_t start = i;
        while (i < text.size()) {
            if (text[i] == '^' && IsColorCodeAt(text, i)) {
                break;
            }
            ++i;
        }
        if (i <= start) {
            break;
        }

        const std::string seg = text.substr(start, i - start);
        const int segWidth = MeasureTextWidth(metrics, seg);
        const int avail = width - x;
        if (segWidth > avail) {
            const QString elided = metrics.elidedText(ToQtText(seg), Qt::ElideRight, avail);
            if (!elided.isEmpty()) {
                painter.setPen(QColor(GetRValue(color), GetGValue(color), GetBValue(color)));
                painter.drawText(x, yBase + metrics.ascent(), elided);
            }
            break;
        }

        DrawTextRun(painter, metrics, x, yBase, color, seg);
        x += segWidth;
    }

    AlphaBlendArgbToHdc(hdc, textRect.left, textRect.top, width, height, pixels.data(), width, height);
#else
    TEXTMETRICA tm{};
    GetTextMetricsA(hdc, &tm);
    const int lineHeight = tm.tmHeight;
    const int yBase = textRect.top + ((textRect.bottom - textRect.top) - lineHeight) / 2;

    COLORREF color = RGB(0, 0, 0);
    int x = textRect.left;
    size_t i = 0;

    while (i < text.size() && x < textRect.right) {
        COLORREF newColor{};
        if (TryConsumeColorCode(text, i, newColor)) {
            color = newColor;
            continue;
        }

        const size_t start = i;
        while (i < text.size()) {
            if (text[i] == '^' && IsColorCodeAt(text, i)) {
                break;
            }
            ++i;
        }

        if (i <= start) {
            break;
        }

        const std::string seg = text.substr(start, i - start);
        SetTextColor(hdc, color);
        SIZE sz{};
        GetTextExtentPoint32A(hdc, seg.c_str(), static_cast<int>(seg.size()), &sz);
        const int avail = textRect.right - x;
        if (sz.cx > avail) {
            static const char kEll[] = "...";
            SIZE esz{};
            GetTextExtentPoint32A(hdc, kEll, 3, &esz);
            if (avail <= esz.cx) {
                break;
            }
            int lo = 0;
            int hi = static_cast<int>(seg.size());
            int best = 0;
            while (lo <= hi) {
                const int mid = (lo + hi) / 2;
                SIZE msz{};
                GetTextExtentPoint32A(hdc, seg.c_str(), mid, &msz);
                if (msz.cx + esz.cx <= avail) {
                    best = mid;
                    lo = mid + 1;
                } else {
                    hi = mid - 1;
                }
            }
            SIZE prefixSz{};
            GetTextExtentPoint32A(hdc, seg.c_str(), best, &prefixSz);
            TextOutA(hdc, x, yBase, seg.c_str(), best);
            TextOutA(hdc, x + prefixSz.cx, yBase, kEll, 3);
            break;
        }

        TextOutA(hdc, x, yBase, seg.c_str(), static_cast<int>(seg.size()));
        x += sz.cx;
    }
#endif
}
