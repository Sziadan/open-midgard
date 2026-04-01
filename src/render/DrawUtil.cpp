#include "DrawUtil.h"
#include "Renderer.h"
#include "core/Globals.h"
#include "DC.h"
#include <cstdio>
#include <cstring>
#include <algorithm>

#if RO_ENABLE_QT6_UI
#include <QFont>
#include <QFontMetrics>
#include <QImage>
#include <QPainter>
#include <QString>
#endif

// --- Font Mapping Tables ---

const char* fontFace9x[] = {
    "Arial",            // FONT_DEFAULT
    "Arial",            // FONT_ENGLISH
    "MS Gothic",        // FONT_JAPANESE
    "SimSun",           // FONT_SIMP_CHINESE
    "MingLiU",          // FONT_TRAD_CHINESE
    "Angsana New",      // FONT_THAI
    "Arial",            // FONT_SCRIPT
    "Arial",            // FONT_PORTUGUESE
    "Arial",            // FONT_INDONESIA
    "Arial",            // FONT_RUSSIA
    "Tahoma"            // FONT_VIETNAM
};

const char* fontFaceNT[] = {
    "Arial",            // FONT_DEFAULT
    "Tahoma",           // FONT_ENGLISH
    "MS Gothic",        // FONT_JAPANESE
    "SimSun",           // FONT_SIMP_CHINESE
    "MingLiU",          // FONT_TRAD_CHINESE
    "Angsana New",      // FONT_THAI
    "Arial",            // FONT_SCRIPT
    "Arial",            // FONT_PORTUGUESE
    "Arial",            // FONT_INDONESIA
    "Arial",            // FONT_RUSSIA
    "Tahoma"            // FONT_VIETNAM
};

int g_fontCharSet[] = {
    ANSI_CHARSET,       // FONT_DEFAULT
    ANSI_CHARSET,       // FONT_ENGLISH
    SHIFTJIS_CHARSET,    // FONT_JAPANESE
    GB2312_CHARSET,     // FONT_SIMP_CHINESE
    CHINESEBIG5_CHARSET, // FONT_TRAD_CHINESE
    THAI_CHARSET,       // FONT_THAI
    ANSI_CHARSET,       // FONT_SCRIPT
    ANSI_CHARSET,       // FONT_PORTUGUESE
    ANSI_CHARSET,       // FONT_INDONESIA
    RUSSIAN_CHARSET,    // FONT_RUSSIA
    VIETNAMESE_CHARSET  // FONT_VIETNAM
};

// --- FontMgr Implementation ---

FontMgr g_fontMgr;

FontMgr::FontMgr() {}

FontMgr::~FontMgr() {
    for (auto& info : m_fontInfoList) {
        if (info.hFont) DeleteObject(info.hFont);
    }
}

HFONT FontMgr::GetFont(int fontType, int fontHeight, int charset, unsigned char bold) {
    FontInfo searchInfo(fontType, fontHeight, charset, bold);
    
    for (auto& info : m_fontInfoList) {
        if (info == searchInfo) return info.hFont;
    }

    // Create new font
    const char* face = fontFaceNT[fontType];
    HFONT hFont = CreateFontA(
        fontHeight, 0, 0, 0,
        bold ? FW_BOLD : FW_NORMAL,
        FALSE, FALSE, FALSE,
        charset,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        face
    );

    searchInfo.hFont = hFont;
    m_fontInfoList.push_back(searchInfo);
    return hFont;
}

// --- DrawDC Implementation ---

DrawDC::DrawDC(HDC hdc) : m_hdc(hdc), m_oldFont(NULL) {
    m_fontType = FONT_DEFAULT;
    m_fontHeight = 12;
    m_bold = 0;
    m_charset = ANSI_CHARSET;
    m_textColor = RGB(0, 0, 0);
}

DrawDC::~DrawDC() {
    if (m_oldFont) SelectObject(m_hdc, m_oldFont);
}

void DrawDC::SetFont(int fontType, int fontHeight, unsigned char bold) {
    m_fontType = fontType;
    m_fontHeight = fontHeight;
    m_bold = bold;
    m_charset = g_fontCharSet[fontType];
    
    HFONT hFont = g_fontMgr.GetFont(fontType, fontHeight, m_charset, bold);
    HFONT old = (HFONT)SelectObject(m_hdc, hFont);
    if (!m_oldFont) m_oldFont = old;
}

void DrawDC::SetTextColor(COLORREF color) {
    m_textColor = color;
    ::SetTextColor(m_hdc, color);
}

void DrawDC::TextOutA(int x, int y, const char* text, int len) {
    // Simple text out for now, ignoring multi-lang piping
#if RO_ENABLE_QT6_UI
    if (!m_hdc || !text || len <= 0) {
        return;
    }

    const QString label = QString::fromLocal8Bit(text, len);
    if (label.isEmpty()) {
        return;
    }

    QFont font(QString::fromLocal8Bit(fontFaceNT[m_fontType]));
    font.setPixelSize((std::max)(1, std::abs(m_fontHeight)));
    font.setBold(m_bold != 0);
    font.setStyleStrategy(QFont::NoAntialias);

    const QFontMetrics metrics(font);
    const int width = (std::max)(1, metrics.horizontalAdvance(label) + 2);
    const int height = (std::max)(1, metrics.height() + 2);
    std::vector<unsigned int> pixels(static_cast<size_t>(width) * static_cast<size_t>(height), 0u);
    QImage image(reinterpret_cast<uchar*>(pixels.data()), width, height, width * static_cast<int>(sizeof(unsigned int)), QImage::Format_ARGB32);
    if (image.isNull()) {
        return;
    }

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setRenderHint(QPainter::TextAntialiasing, false);
    painter.setFont(font);
    painter.setPen(QColor(GetRValue(m_textColor), GetGValue(m_textColor), GetBValue(m_textColor)));
    painter.drawText(0, metrics.ascent(), label);
    AlphaBlendArgbToHdc(m_hdc, x, y, width, height, pixels.data(), width, height);
#else
    ::TextOutA(m_hdc, x, y, text, len);
#endif
}

void DrawDC::GetTextExtentPoint32A(const char* text, int len, SIZE* size) {
#if RO_ENABLE_QT6_UI
    if (!size) {
        return;
    }

    size->cx = 0;
    size->cy = 0;
    if (!text || len <= 0) {
        return;
    }

    const QString label = QString::fromLocal8Bit(text, len);
    if (label.isEmpty()) {
        return;
    }

    QFont font(QString::fromLocal8Bit(fontFaceNT[m_fontType]));
    font.setPixelSize((std::max)(1, std::abs(m_fontHeight)));
    font.setBold(m_bold != 0);
    font.setStyleStrategy(QFont::NoAntialias);

    const QFontMetrics metrics(font);
    size->cx = metrics.horizontalAdvance(label);
    size->cy = metrics.height();
#else
    ::GetTextExtentPoint32A(m_hdc, text, len, size);
#endif
}

// --- Global Drawing Functions ---

void DrawBoxScreen(int x, int y, int cx, int cy, unsigned int color) {
    if ((color & 0xFF000000) == 0) return;

    RPQuadFace* qf = g_renderer.BorrowQuadRP();
    
    float fx = (float)x;
    float fy = (float)y;
    float fcx = (float)cx;
    float fcy = (float)cy;

    qf->primType = D3DPT_TRIANGLEFAN;
    qf->numVerts = 4;
    
    qf->m_verts[0] = { fx,       fy,       0.000001f, 0.999999f, color, 0, 0.0f, 0.0f };
    qf->m_verts[1] = { fx + fcx, fy,       0.000001f, 0.999999f, color, 0, 0.0f, 0.0f };
    qf->m_verts[2] = { fx + fcx, fy + fcy, 0.000001f, 0.999999f, color, 0, 0.0f, 0.0f };
    qf->m_verts[3] = { fx,       fy + fcy, 0.000001f, 0.999999f, color, 0, 0.0f, 0.0f };
    
    qf->verts = qf->m_verts;
    
    // Flag 513 = Alpha List (1) | Some other flags (512)
    g_renderer.AddRP((RPFace*)qf, 513);
}

int FindCharSet(const char* buf, unsigned int len) {
    // Placeholder for multi-lang piping logic
    return len;
}

int ReadCharSet(const char* buf) {
    // Placeholder for multi-lang piping logic
    return ANSI_CHARSET;
}
