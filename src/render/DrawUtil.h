#pragma once

#include <windows.h>
#include <vector>
#include "Renderer.h"

// Font Type Constants
enum FontType {
    FONT_DEFAULT = 0,
    FONT_ENGLISH = 1,
    FONT_JAPANESE = 2,
    FONT_SIMP_CHINESE = 3,
    FONT_TRAD_CHINESE = 4,
    FONT_THAI = 5,
    FONT_SCRIPT = 6,
    FONT_PORTUGUESE = 7,
    FONT_INDONESIA = 8,
    FONT_RUSSIA = 9,
    FONT_VIETNAM = 10
};

struct FontInfo {
    int fontType;
    int fontHeight;
    int fontCharset;
    unsigned char fontBold;
    HFONT hFont;

    FontInfo(int type = 0, int height = 0, int charset = 0, unsigned char bold = 0)
        : fontType(type), fontHeight(height), fontCharset(charset), fontBold(bold), hFont(NULL) {}

    bool operator<(const FontInfo& other) const {
        if (fontType != other.fontType) return fontType < other.fontType;
        if (fontHeight != other.fontHeight) return fontHeight < other.fontHeight;
        if (fontCharset != other.fontCharset) return fontCharset < other.fontCharset;
        return fontBold < other.fontBold;
    }

    bool operator==(const FontInfo& other) const {
        return fontType == other.fontType &&
               fontHeight == other.fontHeight && 
               fontCharset == other.fontCharset &&
               fontBold == other.fontBold;
    }
};

class FontMgr {
public:
    FontMgr();
    ~FontMgr();
    HFONT GetFont(int fontType, int fontHeight, int charset, unsigned char bold);

private:
    std::vector<FontInfo> m_fontInfoList;
};

extern FontMgr g_fontMgr;

// DrawDC class for GDI drawing on surfaces (similar to original)
class DrawDC {
public:
    DrawDC(HDC hdc); // In our case, we might need to handle surface DC differently
    ~DrawDC();

    void SetFont(int fontType, int fontHeight, unsigned char bold);
    void SetTextColor(COLORREF color);
    void TextOutA(int x, int y, const char* text, int len);
    void GetTextExtentPoint32A(const char* text, int len, SIZE* size);

private:
    HDC m_hdc;
    HFONT m_oldFont;
    int m_fontType;
    int m_fontHeight;
    unsigned char m_bold;
    int m_charset;
    COLORREF m_textColor;
};

// Global drawing functions
void DrawBoxScreen(int x, int y, int cx, int cy, unsigned int color);
int FindCharSet(const char* buf, unsigned int len);
int ReadCharSet(const char* buf);
