#include "UIMakeCharWnd.h"

#include "core/File.h"
#include "render/DC.h"
#include "render3d/Device.h"
#include "res/Bitmap.h"
#include "res/PaletteRes.h"
#include "session/Session.h"
#include "gamemode/LoginMode.h"
#include "gamemode/Mode.h"
#include "DebugLog.h"
#include "main/WinMain.h"
#include "qtui/QtUiRuntime.h"
#include "ui/UIWindowMgr.h"

#include <windows.h>

#if RO_ENABLE_QT6_UI
#include <QFont>
#include <QImage>
#include <QPainter>
#include <QString>
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {

constexpr int kMakeCharNameLeft = 62;
constexpr int kMakeCharNameTop = 244;
constexpr int kMakeCharNameWidth = 100;
constexpr int kMakeCharNameHeight = 18;
constexpr int kMakeCharOkX = 483;
constexpr int kMakeCharOkY = 318;
constexpr int kMakeCharOkW = 44;
constexpr int kMakeCharOkH = 22;
constexpr int kMakeCharCancelX = 530;
constexpr int kMakeCharCancelY = 318;
constexpr int kMakeCharCancelW = 44;
constexpr int kMakeCharCancelH = 22;
constexpr int kMakeCharStatIds[6] = { 139, 142, 140, 144, 141, 143 };
constexpr int kMakeCharStatX[6] = { 270, 270, 190, 349, 349, 190 };
constexpr int kMakeCharStatY[6] = { 50, 244, 104, 190, 104, 190 };
constexpr int kMakeCharHairIds[3] = { 161, 160, 213 };
constexpr int kMakeCharHairX[3] = { 48, 130, 89 };
constexpr int kMakeCharHairY[3] = { 135, 135, 101 };
constexpr int kMakeCharSmallButtonW = 16;
constexpr int kMakeCharSmallButtonH = 14;
constexpr const char* kMakeCharStatBmp[6] = {
    "arw-str0.bmp",
    "arw-int0.bmp",
    "arw-agi0.bmp",
    "arw-luk0.bmp",
    "arw-vit0.bmp",
    "arw-dex0.bmp"
};
constexpr const char* kMakeCharHairBmp[3] = {
    "scroll1left.bmp",
    "scroll1right.bmp",
    "scroll0up.bmp"
};
constexpr const char* kMakeCharStatBmpPressed[6] = {
    "arw-str1.bmp",
    "arw-int1.bmp",
    "arw-agi1.bmp",
    "arw-luk1.bmp",
    "arw-vit1.bmp",
    "arw-dex1.bmp"
};

std::vector<std::string> BuildUiAssetCandidates(const char* fileName);
shopui::BitmapPixels LoadFirstBitmapPixelsFromCandidates(const std::vector<std::string>& candidates, std::string* outPath);
std::string ResolveUiAssetPath(const char* fileName);

shopui::BitmapPixels LoadBitmapPixelsFromGameData(const char* path)
{
    return shopui::LoadBitmapPixelsFromGameData(path ? path : "", true);
}

bool LoadMakeCharQtButtonBitmap(int index, bool pressed, shopui::BitmapPixels* out)
{
    if (!out) {
        return false;
    }

    const char* assetName = nullptr;
    if (index >= 0 && index < 6) {
        assetName = pressed ? kMakeCharStatBmpPressed[index] : kMakeCharStatBmp[index];
    } else if (index >= 6 && index < 9) {
        assetName = kMakeCharHairBmp[index - 6];
    } else if (index == 9) {
        assetName = pressed ? "btn_ok_a.bmp" : "btn_ok.bmp";
    } else if (index == 10) {
        assetName = pressed ? "btn_cancel_a.bmp" : "btn_cancel.bmp";
    }

    if (!assetName) {
        return false;
    }

    static bool loggedMiss[11][2] = {};
    static bool loggedHit[11][2] = {};
    const int pressedIndex = pressed ? 1 : 0;

    const std::string resolvedPath = ResolveUiAssetPath(assetName);
    if (resolvedPath.empty()) {
        if (!loggedMiss[index][pressedIndex]) {
            DbgLog("[MakeCharQt] BUTTON MISS index=%d pressed=%d asset=%s\n", index, pressed ? 1 : 0, assetName);
            loggedMiss[index][pressedIndex] = true;
        }
        return false;
    }

    *out = LoadBitmapPixelsFromGameData(resolvedPath.c_str());
    if (!out->IsValid()) {
        if (!loggedMiss[index][pressedIndex]) {
            DbgLog("[MakeCharQt] BUTTON LOAD FAIL index=%d pressed=%d asset=%s path=%s\n",
                index,
                pressed ? 1 : 0,
                assetName,
                resolvedPath.c_str());
            loggedMiss[index][pressedIndex] = true;
        }
        return false;
    }

    // These legacy UI button BMPs use transparency as a cutout, not soft alpha.
    // Force all non-transparent pixels to fully opaque so the Qt image path
    // matches the original UI button presentation.
    for (unsigned int& pixel : out->pixels) {
        if ((pixel >> 24) != 0u) {
            pixel = 0xFF000000u | (pixel & 0x00FFFFFFu);
        }
    }

    if (!loggedHit[index][pressedIndex]) {
        int minX = out->width;
        int minY = out->height;
        int maxX = -1;
        int maxY = -1;
        unsigned int opaqueCount = 0;
        for (int y = 0; y < out->height; ++y) {
            for (int x = 0; x < out->width; ++x) {
                const unsigned int pixel = out->pixels[static_cast<size_t>(y) * static_cast<size_t>(out->width) + static_cast<size_t>(x)];
                if ((pixel >> 24) != 0u) {
                    ++opaqueCount;
                    minX = (std::min)(minX, x);
                    minY = (std::min)(minY, y);
                    maxX = (std::max)(maxX, x);
                    maxY = (std::max)(maxY, y);
                }
            }
        }
        DbgLog("[MakeCharQt] BUTTON HIT index=%d pressed=%d asset=%s path=%s size=%dx%d\n",
            index,
            pressed ? 1 : 0,
            assetName,
            resolvedPath.c_str(),
            out->width,
            out->height);
        DbgLog("[MakeCharQt] BUTTON ALPHA index=%d pressed=%d opaque=%u bounds=(%d,%d)-(%d,%d)\n",
            index,
            pressed ? 1 : 0,
            opaqueCount,
            minX,
            minY,
            maxX,
            maxY);
        loggedHit[index][pressedIndex] = true;
    }
    return true;
}

void DrawBitmapPixelsStretched(HDC target, const shopui::BitmapPixels& bmp, const RECT& dst)
{
    if (!target || !bmp.IsValid() || dst.right <= dst.left || dst.bottom <= dst.top) {
        return;
    }

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = bmp.width;
    bmi.bmiHeader.biHeight = -bmp.height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    const int oldStretchMode = SetStretchBltMode(target, HALFTONE);
    StretchDIBits(target, dst.left, dst.top, dst.right - dst.left, dst.bottom - dst.top,
        0, 0, bmp.width, bmp.height, bmp.pixels.data(), &bmi, DIB_RGB_COLORS, SRCCOPY);
    SetStretchBltMode(target, oldStretchMode);
}

#if RO_ENABLE_QT6_UI
QFont BuildMakeCharFontFromHdc(HDC hdc)
{
    LOGFONTA logFont{};
    if (hdc) {
        if (HGDIOBJ fontObject = GetCurrentObject(hdc, OBJ_FONT)) {
            GetObjectA(fontObject, sizeof(logFont), &logFont);
        }
    }

    const QString family = logFont.lfFaceName[0] != '\0'
        ? QString::fromLocal8Bit(logFont.lfFaceName)
        : QStringLiteral("Tahoma");
    QFont font(family);
    font.setPixelSize(logFont.lfHeight != 0 ? (std::max)(1, static_cast<int>(std::abs(logFont.lfHeight))) : 11);
    font.setBold(logFont.lfWeight >= FW_BOLD);
    font.setStyleStrategy(QFont::NoAntialias);
    return font;
}

void DrawMakeCharText(HDC hdc, const RECT& rect, const char* text, COLORREF color, Qt::Alignment alignment)
{
    if (!hdc || !text || rect.right <= rect.left || rect.bottom <= rect.top) {
        return;
    }

    const QString label = QString::fromLocal8Bit(text);
    if (label.isEmpty()) {
        return;
    }

    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    std::vector<unsigned int> pixels(static_cast<size_t>(width) * static_cast<size_t>(height), 0u);
    QImage image(reinterpret_cast<uchar*>(pixels.data()), width, height, width * static_cast<int>(sizeof(unsigned int)), QImage::Format_ARGB32);
    if (image.isNull()) {
        return;
    }

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setRenderHint(QPainter::TextAntialiasing, false);
    painter.setFont(BuildMakeCharFontFromHdc(hdc));
    painter.setPen(QColor(GetRValue(color), GetGValue(color), GetBValue(color)));
    painter.drawText(QRect(0, 0, width, height), alignment | Qt::TextSingleLine, label);
    AlphaBlendArgbToHdc(hdc, rect.left, rect.top, width, height, pixels.data(), width, height);
}

#endif

std::string ToLowerAscii(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
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

    static const char* kUiKor =
        "texture\\"
        "\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA"
        "\\";

    const char* pathPrefixes[] = {
        "",
        "skin\\default\\",
        "skin\\default\\basic_interface\\",
        "skin\\default\\login_interface\\",
        "skin\\default\\interface\\",
        "texture\\",
        "texture\\interface\\",
        "texture\\interface\\basic_interface\\",
        "texture\\login_interface\\",
        "data\\",
        "data\\texture\\",
        "data\\texture\\interface\\",
        "data\\texture\\interface\\basic_interface\\",
        "data\\texture\\login_interface\\",
        kUiKor,
        "texture\\\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\basic_interface\\",
        "texture\\\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\login_interface\\",
        "data\\texture\\\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\",
        "data\\texture\\\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\basic_interface\\",
        "data\\texture\\\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\login_interface\\",
        nullptr
    };

    std::string base = NormalizeSlash(fileName);
    AddUniqueCandidate(out, base);

    std::string filenameOnly = base;
    const size_t slashPos = filenameOnly.find_last_of('\\');
    if (slashPos != std::string::npos && slashPos + 1 < filenameOnly.size()) {
        filenameOnly = filenameOnly.substr(slashPos + 1);
    }

    for (int i = 0; pathPrefixes[i]; ++i) {
        AddUniqueCandidate(out, std::string(pathPrefixes[i]) + filenameOnly);
    }

    static const char* uiPrefixCandidates[] = {
        "texture\\interface\\",
        "texture\\interface\\basic_interface\\",
        "texture\\login_interface\\",
        "data\\texture\\interface\\",
        "data\\texture\\interface\\basic_interface\\",
        "data\\texture\\login_interface\\",
        "texture\\\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\",
        "texture\\\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\basic_interface\\",
        "texture\\\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\login_interface\\",
        "data\\texture\\\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\",
        "data\\texture\\\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\basic_interface\\",
        "data\\texture\\\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\login_interface\\",
        nullptr
    };

    for (int i = 0; uiPrefixCandidates[i]; ++i) {
        const std::string prefix = uiPrefixCandidates[i];
        if (base.size() > prefix.size() && ToLowerAscii(base.substr(0, prefix.size())) == ToLowerAscii(prefix)) {
            const std::string suffix = base.substr(prefix.size());
            AddUniqueCandidate(out, std::string("skin\\default\\") + suffix);
            AddUniqueCandidate(out, std::string("skin\\default\\basic_interface\\") + suffix);
            AddUniqueCandidate(out, std::string("skin\\default\\login_interface\\") + suffix);
        }
    }

    return out;
}

shopui::BitmapPixels LoadFirstBitmapPixelsFromCandidates(const std::vector<std::string>& candidates, std::string* outPath)
{
    for (const std::string& candidate : candidates) {
        shopui::BitmapPixels bmp = LoadBitmapPixelsFromGameData(candidate.c_str());
        if (bmp.IsValid()) {
            if (outPath) {
                *outPath = candidate;
            }
            return bmp;
        }
    }
    return {};
}

std::string ResolveUiAssetPath(const char* fileName)
{
    if (!fileName || !*fileName) {
        return {};
    }

    const std::vector<std::string> candidates = BuildUiAssetCandidates(fileName);
    for (const std::string& candidate : candidates) {
        if (g_fileMgr.IsDataExist(candidate.c_str())) {
            return candidate;
        }
    }
    return {};
}

RECT MakeRect(int x, int y, int w, int h)
{
    RECT rc = { x, y, x + w, y + h };
    return rc;
}

bool BuildPaletteOverride(const std::string& paletteName, std::array<unsigned int, 256>& outPalette)
{
    CPaletteRes* palRes = g_resMgr.GetAs<CPaletteRes>(paletteName.c_str());
    if (!palRes) {
        return false;
    }
    g_3dDevice.ConvertPalette(outPalette.data(), palRes->m_pal, 256);
    return true;
}

POINT GetPreviewLayerPoint(const UIMakeCharWnd::PreviewState& preview, int layerPriority, int resolvedLayer, CImfRes* imfRes, const CMotion* motion)
{
    POINT point = imfRes->GetPoint(resolvedLayer, preview.curAction, preview.curMotion);
    if (layerPriority != 1 || !motion || motion->attachInfo.empty()) {
        return point;
    }

    CActRes* bodyActRes = g_resMgr.GetAs<CActRes>(preview.actName[0].c_str());
    if (!bodyActRes) {
        return point;
    }

    const CMotion* bodyMotion = bodyActRes->GetMotion(preview.curAction, preview.curMotion);
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

bool DrawPreviewLayer(HDC hdc, const UIMakeCharWnd::PreviewState& preview, int layerIndex)
{
    if (layerIndex < 0 || layerIndex > 1) {
        return false;
    }

    CActRes* actRes = g_resMgr.GetAs<CActRes>(preview.actName[layerIndex].c_str());
    CSprRes* sprRes = g_resMgr.GetAs<CSprRes>(preview.sprName[layerIndex].c_str());
    CImfRes* imfRes = g_resMgr.GetAs<CImfRes>(preview.imfName.c_str());
    if (!actRes || !sprRes || !imfRes) {
        return false;
    }

    int resolvedLayer = imfRes->GetLayer(layerIndex, preview.curAction, preview.curMotion);
    if (resolvedLayer < 0) {
        resolvedLayer = layerIndex;
    }

    const CMotion* motion = actRes->GetMotion(preview.curAction, preview.curMotion);
    if (!motion || resolvedLayer >= static_cast<int>(motion->sprClips.size())) {
        return false;
    }

    const POINT point = GetPreviewLayerPoint(preview, layerIndex, resolvedLayer, imfRes, motion);

    std::array<unsigned int, 256> paletteOverride{};
    unsigned int* palette = sprRes->m_pal;
    if (layerIndex == 0 && preview.bodyPalette > 0 && !preview.bodyPaletteName.empty()) {
        if (BuildPaletteOverride(preview.bodyPaletteName, paletteOverride)) {
            palette = paletteOverride.data();
        }
    }
    if (layerIndex == 1 && preview.headPalette > 0 && !preview.headPaletteName.empty()) {
        if (BuildPaletteOverride(preview.headPaletteName, paletteOverride)) {
            palette = paletteOverride.data();
        }
    }

    CMotion singleLayerMotion{};
    singleLayerMotion.sprClips.push_back(motion->sprClips[resolvedLayer]);
    return DrawActMotionToHdc(hdc, preview.x + point.x, preview.y + point.y, sprRes, &singleLayerMotion, palette);
}

#if RO_ENABLE_QT6_UI
bool DrawPreviewLayerToArgb(unsigned int* pixels,
    int width,
    int height,
    const UIMakeCharWnd::PreviewState& preview,
    int ownerX,
    int ownerY,
    int layerIndex)
{
    if (!pixels || width <= 0 || height <= 0 || layerIndex < 0 || layerIndex > 1) {
        return false;
    }

    CActRes* actRes = g_resMgr.GetAs<CActRes>(preview.actName[layerIndex].c_str());
    CSprRes* sprRes = g_resMgr.GetAs<CSprRes>(preview.sprName[layerIndex].c_str());
    CImfRes* imfRes = g_resMgr.GetAs<CImfRes>(preview.imfName.c_str());
    if (!actRes || !sprRes || !imfRes) {
        return false;
    }

    int resolvedLayer = imfRes->GetLayer(layerIndex, preview.curAction, preview.curMotion);
    if (resolvedLayer < 0) {
        resolvedLayer = layerIndex;
    }

    const CMotion* motion = actRes->GetMotion(preview.curAction, preview.curMotion);
    if (!motion || resolvedLayer >= static_cast<int>(motion->sprClips.size())) {
        return false;
    }

    const POINT point = GetPreviewLayerPoint(preview, layerIndex, resolvedLayer, imfRes, motion);

    std::array<unsigned int, 256> paletteOverride{};
    unsigned int* palette = sprRes->m_pal;
    if (layerIndex == 0 && preview.bodyPalette > 0 && !preview.bodyPaletteName.empty()) {
        if (BuildPaletteOverride(preview.bodyPaletteName, paletteOverride)) {
            palette = paletteOverride.data();
        }
    }
    if (layerIndex == 1 && preview.headPalette > 0 && !preview.headPaletteName.empty()) {
        if (BuildPaletteOverride(preview.headPaletteName, paletteOverride)) {
            palette = paletteOverride.data();
        }
    }

    CMotion singleLayerMotion{};
    singleLayerMotion.sprClips.push_back(motion->sprClips[resolvedLayer]);
    return DrawActMotionToArgb(
        pixels,
        width,
        height,
        preview.x - ownerX + point.x,
        preview.y - ownerY + point.y,
        sprRes,
        &singleLayerMotion,
        palette);
}
#endif

}

UIMakeCharWnd::UIMakeCharWnd()
    : m_controlsCreated(false), m_assetsProbed(false), m_backgroundBmp(),
      m_composeSurface(),
      m_nameEditCtrl(nullptr), m_okButton(nullptr), m_cancelButton(nullptr),
      m_stats{5, 5, 5, 5, 5, 5}, m_hairIdx(1), m_hairColor(0),
      m_statBtns{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
      m_hairBtns{nullptr, nullptr, nullptr}, m_lastPreviewAdvanceTick(0),
      m_pressedQtButtonIndex(-1)
{
    m_defPushId = 118;
    m_defCancelPushId = 119;
}

UIMakeCharWnd::~UIMakeCharWnd()
{
    if (m_nameEditCtrl && m_nameEditCtrl->m_parent != this) {
        delete m_nameEditCtrl;
        m_nameEditCtrl = nullptr;
    }
    ReleaseComposeSurface();
    ClearAssets();
}

bool UIMakeCharWnd::HandleQtMouseDown(int x, int y)
{
    if (!g_hMainWnd || m_show == 0) {
        return false;
    }

    RECT rcClient{};
    GetClientRect(g_hMainWnd, &rcClient);
    const int clientW = rcClient.right - rcClient.left;
    const int clientH = rcClient.bottom - rcClient.top;
    if (!m_controlsCreated && clientW > 0 && clientH > 0) {
        OnCreate(clientW, clientH);
    }

    if (x < m_x || y < m_y || x >= m_x + m_w || y >= m_y + m_h) {
        SetQtPressedButtonIndex(-1);
        return false;
    }

    int buttonIndex = -1;
    HitQtButton(x, y, &buttonIndex);
    SetQtPressedButtonIndex(buttonIndex);

    if (m_nameEditCtrl
        && x >= m_x + kMakeCharNameLeft && x < m_x + kMakeCharNameLeft + kMakeCharNameWidth
        && y >= m_y + kMakeCharNameTop && y < m_y + kMakeCharNameTop + kMakeCharNameHeight) {
        m_nameEditCtrl->m_hasFocus = true;
        m_nameEditCtrl->Invalidate();
        g_windowMgr.m_editWindow = m_nameEditCtrl;
    }

    return true;
}

bool UIMakeCharWnd::HandleQtMouseUp(int x, int y)
{
    const int pressedButtonIndex = m_pressedQtButtonIndex;
    int releasedButtonIndex = -1;
    const bool insidePanel = x >= m_x && y >= m_y && x < m_x + m_w && y < m_y + m_h;
    if (insidePanel) {
        HitQtButton(x, y, &releasedButtonIndex);
    }

    SetQtPressedButtonIndex(-1);

    if (pressedButtonIndex >= 0 && releasedButtonIndex == pressedButtonIndex) {
        QtButtonDisplay buttonDisplay{};
        if (GetQtButtonDisplayForQt(pressedButtonIndex, &buttonDisplay)) {
            SendMsg(nullptr, 6, buttonDisplay.id, 0, 0);
            return true;
        }
    }

    return insidePanel || pressedButtonIndex >= 0;
}

bool UIMakeCharWnd::GetMakeCharDisplay(MakeCharDisplay* out) const
{
    if (!out) {
        return false;
    }

    MakeCharDisplay display{};
    display.name = m_nameEditCtrl && m_nameEditCtrl->GetText() ? m_nameEditCtrl->GetText() : "";
    display.nameFocused = m_nameEditCtrl && m_nameEditCtrl->m_hasFocus;
    for (int i = 0; i < 6; ++i) {
        display.stats[i] = m_stats[i];
    }
    display.hairIndex = m_hairIdx;
    display.hairColor = m_hairColor;
    *out = display;
    return true;
}

int UIMakeCharWnd::GetQtButtonCount() const
{
    return 11;
}

bool UIMakeCharWnd::GetQtButtonDisplayForQt(int index, QtButtonDisplay* out) const
{
    if (!out || index < 0 || index >= GetQtButtonCount()) {
        return false;
    }

    const unsigned int* pixels = nullptr;
    int bitmapWidth = 0;
    int bitmapHeight = 0;
    const bool hasBitmap = const_cast<UIMakeCharWnd*>(this)->GetQtButtonBitmap(index, &pixels, &bitmapWidth, &bitmapHeight);
    (void)pixels;
    out->pressed = (index == m_pressedQtButtonIndex);

    if (index < 6) {
        out->id = kMakeCharStatIds[index];
        out->x = m_x + kMakeCharStatX[index];
        out->y = m_y + kMakeCharStatY[index];
        out->width = hasBitmap ? bitmapWidth : kMakeCharSmallButtonW;
        out->height = hasBitmap ? bitmapHeight : kMakeCharSmallButtonH;
        out->label = "+";
        return true;
    }

    if (index < 9) {
        const int hairIndex = index - 6;
        out->id = kMakeCharHairIds[hairIndex];
        out->x = m_x + kMakeCharHairX[hairIndex];
        out->y = m_y + kMakeCharHairY[hairIndex];
        out->width = hasBitmap ? bitmapWidth : kMakeCharSmallButtonW;
        out->height = hasBitmap ? bitmapHeight : kMakeCharSmallButtonH;
        out->label = hairIndex == 0 ? "<" : (hairIndex == 1 ? ">" : "^");
        return true;
    }

    if (index == 9) {
        out->id = 118;
        out->x = m_x + kMakeCharOkX;
        out->y = m_y + kMakeCharOkY;
        out->width = hasBitmap ? bitmapWidth : kMakeCharOkW;
        out->height = hasBitmap ? bitmapHeight : kMakeCharOkH;
        out->label = "OK";
        return true;
    }

    out->id = 119;
    out->x = m_x + kMakeCharCancelX;
    out->y = m_y + kMakeCharCancelY;
    out->width = hasBitmap ? bitmapWidth : kMakeCharCancelW;
    out->height = hasBitmap ? bitmapHeight : kMakeCharCancelH;
    out->label = "Cancel";
    return true;
}

int UIMakeCharWnd::GetQtStatFieldCount() const
{
    return 6;
}

bool UIMakeCharWnd::GetQtStatFieldDisplayForQt(int index, QtStatFieldDisplay* out) const
{
    if (!out || index < 0 || index >= GetQtStatFieldCount()) {
        return false;
    }

    static const int kValueY[6] = { 40, 56, 72, 88, 104, 120 };
    static const char* kLabels[6] = { "STR", "AGI", "VIT", "INT", "DEX", "LUK" };

    out->x = m_x + 456;
    out->y = m_y + kValueY[index];
    out->width = 58;
    out->height = 13;
    out->label = kLabels[index];
    out->value = m_stats[index];
    return true;
}

void UIMakeCharWnd::ClearAssets()
{
    m_backgroundBmp.Clear();
    m_backgroundPath.clear();
}

void UIMakeCharWnd::ReleaseComposeSurface()
{
    m_composeSurface.Release();
}

bool UIMakeCharWnd::EnsureComposeSurface(int width, int height)
{
    return m_composeSurface.EnsureSize(width, height);
}

void UIMakeCharWnd::EnsureResourceCache()
{
    if (m_assetsProbed) {
        return;
    }
    m_assetsProbed = true;

    const char* panelNames[] = {
        "win_makechar.bmp",
        "win_make_char.bmp",
        "win_make.bmp",
        "makechar.bmp",
        nullptr
    };

    for (int i = 0; panelNames[i] && !m_backgroundBmp.IsValid(); ++i) {
        m_backgroundBmp = LoadFirstBitmapPixelsFromCandidates(BuildUiAssetCandidates(panelNames[i]), &m_backgroundPath);
    }
}

void UIMakeCharWnd::EnsureButtons()
{
    if (!m_okButton) {
        auto* btn = new UIBitmapButton();
        btn->SetBitmapName(ResolveUiAssetPath("btn_ok.bmp").c_str(), 0);
        btn->SetBitmapName(ResolveUiAssetPath("btn_ok_a.bmp").c_str(), 1);
        btn->SetBitmapName(ResolveUiAssetPath("btn_ok_b.bmp").c_str(), 2);
        btn->Create(btn->m_bitmapWidth > 0 ? btn->m_bitmapWidth : 44,
                    btn->m_bitmapHeight > 0 ? btn->m_bitmapHeight : 22);
        btn->Move(m_x + 483, m_y + 318);  // Ref: pos[0] = (483, 318)
        btn->m_id = 118;
        AddChild(btn);
        m_okButton = btn;
    }

    if (!m_cancelButton) {
        auto* btn = new UIBitmapButton();
        btn->SetBitmapName(ResolveUiAssetPath("btn_cancel.bmp").c_str(), 0);
        btn->SetBitmapName(ResolveUiAssetPath("btn_cancel_a.bmp").c_str(), 1);
        btn->SetBitmapName(ResolveUiAssetPath("btn_cancel_b.bmp").c_str(), 2);
        btn->Create(btn->m_bitmapWidth > 0 ? btn->m_bitmapWidth : 44,
                    btn->m_bitmapHeight > 0 ? btn->m_bitmapHeight : 22);
        btn->Move(m_x + 530, m_y + 318);  // Ref: pos[1] = (530, 318)
        btn->m_id = 119;
        AddChild(btn);
        m_cancelButton = btn;
    }

    // Six stat-swap arrows at Ref-exact positions surrounding the hexagon.
    // Each arrow sits near the vertex it feeds INTO (direction: source -> dest).
    // Ref IDs and positions from UIFrameWnd.cpp MakeButton:
    //   139 (270, 50)  Int→Str  — above Str vertex (288,86)
    //   142 (270,244)  Str→Int  — at  Int vertex (288,244)
    //   140 (190,104)  Luk→Agi  — left of Agi vertex (220,126)
    //   144 (349,190)  Agi→Luk  — right of Luk vertex (356,206)
    //   141 (349,104)  Dex→Vit  — right of Vit vertex (356,126)
    //   143 (190,190)  Vit→Dex  — left of Dex vertex (220,206)
    static const int kStatIds[6] = { 139, 142, 140, 144, 141, 143 };
    static const int kStatX[6]   = { 270, 270, 190, 349, 349, 190 };
    static const int kStatY[6]   = {  50, 244, 104, 190, 104, 190 };
    // Ref arw-xxx bitmaps: normal state = arw-xxx0.bmp, pressed = arw-xxx1.bmp.
    // Each button is labeled with the stat it FEEDS INTO.
    static const char* kStatBmpPressed[6] = {
        "arw-str1.bmp",
        "arw-int1.bmp",
        "arw-agi1.bmp",
        "arw-luk1.bmp",
        "arw-vit1.bmp",
        "arw-dex1.bmp"
    };
    for (int i = 0; i < 6; ++i) {
        if (m_statBtns[i]) continue;
        auto* btn = new UIBitmapButton();
        btn->SetBitmapName(ResolveUiAssetPath(kMakeCharStatBmp[i]).c_str(), 0);
        btn->SetBitmapName(ResolveUiAssetPath(kStatBmpPressed[i]).c_str(), 1);
        btn->SetBitmapName(ResolveUiAssetPath(kMakeCharStatBmp[i]).c_str(), 2);  // disabled = normal
        btn->Create(btn->m_bitmapWidth > 0 ? btn->m_bitmapWidth : 16,
                    btn->m_bitmapHeight > 0 ? btn->m_bitmapHeight : 14);
        btn->Move(m_x + kStatX[i], m_y + kStatY[i]);
        btn->m_id = kStatIds[i];
        AddChild(btn);
        m_statBtns[i] = btn;
    }

    // Char1 hair buttons (left character preview) — Ref positions:
    //   161 (48,135)  hair prev
    //   160 (130,135) hair next
    //   213 (89,101)  hair color cycle
    static const int kHairIds[3]  = { 161, 160, 213 };
    static const int kHairX[3]    = {  48, 130,  89 };
    static const int kHairY[3]    = { 135, 135, 101 };
    for (int i = 0; i < 3; ++i) {
        if (m_hairBtns[i]) continue;
        auto* btn = new UIBitmapButton();
        btn->SetBitmapName(ResolveUiAssetPath(kMakeCharHairBmp[i]).c_str(), 0);
        btn->Create(btn->m_bitmapWidth > 0 ? btn->m_bitmapWidth : 16,
                    btn->m_bitmapHeight > 0 ? btn->m_bitmapHeight : 14);
        btn->Move(m_x + kHairX[i], m_y + kHairY[i]);
        btn->m_id = kHairIds[i];
        AddChild(btn);
        m_hairBtns[i] = btn;
    }
}

void UIMakeCharWnd::DrawHexagon(HDC hdc) const
{
    // Exact Ref vertex table (Ref/UIFrameWnd.cpp DrawHexagon, center 288,166)
    // Ref params order: [0]=Str [1]=Vit [2]=Luk [3]=Int [4]=Dex [5]=Agi
    // Our m_stats:      [0]=Str [1]=Agi [2]=Vit [3]=Int [4]=Dex [5]=Luk
    static const int kMaxX[6] = { 288, 356, 356, 288, 220, 220 };
    static const int kMaxY[6] = {  86, 126, 206, 244, 206, 126 };
    static const int kStatMap[6] = { 0, 2, 5, 3, 4, 1 };  // hex[i] = m_stats[kStatMap[i]]

    const int kCX = m_x + 288;
    const int kCY = m_y + 166;

    POINT mPts[6], vPts[6];
    for (int i = 0; i < 6; ++i) {
        const int sv = std::min(std::max(m_stats[kStatMap[i]], 0), 9);
        mPts[i].x = kMaxX[i] + m_x;
        mPts[i].y = kMaxY[i] + m_y;
        vPts[i].x = sv * (kMaxX[i] - 288) / 9 + 288 + m_x;
        vPts[i].y = sv * (kMaxY[i] - 166) / 9 + 166 + m_y;
    }

    // Max hexagon outline
    HPEN outlinePen = CreatePen(PS_SOLID, 1, RGB(140, 105, 82));
    HGDIOBJ oldPen   = SelectObject(hdc, outlinePen);
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Polygon(hdc, mPts, 6);

    // Filled stat polygon — Ref fill color 0xCE977C
    HPEN    statPen  = CreatePen(PS_SOLID, 1, RGB(0xA0, 0x60, 0x20));
    HBRUSH  statFill = CreateSolidBrush(RGB(0xCE, 0x97, 0x7C));
    SelectObject(hdc, statPen);
    SelectObject(hdc, statFill);
    Polygon(hdc, vPts, 6);

    // Axis lines from center to each max vertex
    HPEN axisPen = CreatePen(PS_SOLID, 1, RGB(160, 120, 90));
    SelectObject(hdc, axisPen);
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    for (int i = 0; i < 6; ++i) {
        MoveToEx(hdc, kCX, kCY, nullptr);
        LineTo(hdc, mPts[i].x, mPts[i].y);
    }

    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(outlinePen); DeleteObject(statPen);
    DeleteObject(statFill);   DeleteObject(axisPen);
}

#if RO_ENABLE_QT6_UI
void UIMakeCharWnd::DrawQtHexagon(QImage* image) const
{
    if (!image || image->isNull()) {
        return;
    }

    static const int kMaxX[6] = { 288, 356, 356, 288, 220, 220 };
    static const int kMaxY[6] = {  86, 126, 206, 244, 206, 126 };
    static const int kStatMap[6] = { 0, 2, 5, 3, 4, 1 };
    static const QPoint kCenter(288, 166);

    QPoint maxPoints[6];
    QPoint valuePoints[6];
    for (int i = 0; i < 6; ++i) {
        const int statValue = (std::max)(0, (std::min)(m_stats[kStatMap[i]], 9));
        maxPoints[i] = QPoint(kMaxX[i], kMaxY[i]);
        valuePoints[i] = QPoint(
            statValue * (kMaxX[i] - kCenter.x()) / 9 + kCenter.x(),
            statValue * (kMaxY[i] - kCenter.y()) / 9 + kCenter.y());
    }

    QPainter painter(image);
    painter.setRenderHint(QPainter::Antialiasing, false);

    painter.setPen(QPen(QColor(140, 105, 82), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawPolygon(maxPoints, 6);

    painter.setPen(QPen(QColor(0xA0, 0x60, 0x20), 1));
    painter.setBrush(QBrush(QColor(0xCE, 0x97, 0x7C)));
    painter.drawPolygon(valuePoints, 6);

    painter.setPen(QPen(QColor(160, 120, 90), 1));
    painter.setBrush(Qt::NoBrush);
    for (int i = 0; i < 6; ++i) {
        painter.drawLine(kCenter, maxPoints[i]);
    }
}

void UIMakeCharWnd::DrawQtPreview(QImage* image)
{
    if (!image || image->isNull()) {
        return;
    }

    std::vector<unsigned int> overlayPixels(
        static_cast<size_t>(image->width()) * static_cast<size_t>(image->height()),
        0u);
    if (overlayPixels.empty()) {
        return;
    }

    DrawPreviewLayerToArgb(overlayPixels.data(), image->width(), image->height(), m_preview, m_x, m_y, 0);
    DrawPreviewLayerToArgb(overlayPixels.data(), image->width(), image->height(), m_preview, m_x, m_y, 1);

    const QImage overlay(
        reinterpret_cast<const uchar*>(overlayPixels.data()),
        image->width(),
        image->height(),
        image->width() * static_cast<int>(sizeof(unsigned int)),
        QImage::Format_ARGB32);
    QPainter painter(image);
    painter.drawImage(0, 0, overlay);
}
#endif

void UIMakeCharWnd::EnsureQtLayout()
{
    if (!g_hMainWnd) {
        return;
    }

    RECT clientRect{};
    if (!GetClientRect(g_hMainWnd, &clientRect)) {
        return;
    }

    OnCreate(clientRect.right - clientRect.left, clientRect.bottom - clientRect.top);
}

bool UIMakeCharWnd::GetQtBackgroundBitmap(const unsigned int** pixels, int* width, int* height)
{
    EnsureResourceCache();
    if (!m_backgroundBmp.IsValid()) {
        return false;
    }

    if (pixels) {
        *pixels = m_backgroundBmp.pixels.data();
    }
    if (width) {
        *width = m_backgroundBmp.width;
    }
    if (height) {
        *height = m_backgroundBmp.height;
    }
    return true;
}

bool UIMakeCharWnd::GetQtButtonBitmap(int index, const unsigned int** pixels, int* width, int* height)
{
    static shopui::BitmapPixels cachedButtons[11][2];

    if (index < 0 || index >= 11) {
        return false;
    }
    const int pressedIndex = index == m_pressedQtButtonIndex ? 1 : 0;
    if (!cachedButtons[index][pressedIndex].IsValid()) {
        LoadMakeCharQtButtonBitmap(index, pressedIndex != 0, &cachedButtons[index][pressedIndex]);
    }
    if (!cachedButtons[index][pressedIndex].IsValid()) {
        return false;
    }

    if (pixels) {
        *pixels = cachedButtons[index][pressedIndex].pixels.data();
    }
    if (width) {
        *width = cachedButtons[index][pressedIndex].width;
    }
    if (height) {
        *height = cachedButtons[index][pressedIndex].height;
    }
    return true;
}

bool UIMakeCharWnd::HitQtButton(int x, int y, int* outIndex) const
{
    if (outIndex) {
        *outIndex = -1;
    }

    for (int index = 0; index < GetQtButtonCount(); ++index) {
        QtButtonDisplay buttonDisplay{};
        if (!GetQtButtonDisplayForQt(index, &buttonDisplay)) {
            continue;
        }
        if (x >= buttonDisplay.x && x < buttonDisplay.x + buttonDisplay.width
            && y >= buttonDisplay.y && y < buttonDisplay.y + buttonDisplay.height) {
            if (outIndex) {
                *outIndex = index;
            }
            return true;
        }
    }
    return false;
}

void UIMakeCharWnd::SetQtPressedButtonIndex(int index)
{
    if (m_pressedQtButtonIndex == index) {
        return;
    }
    m_pressedQtButtonIndex = index;
    Invalidate();
}

void UIMakeCharWnd::OnCreate(int cx, int cy)
{
    if (m_controlsCreated) {
        return;
    }
    m_controlsCreated = true;

    Create(576, 342);
    Move((cx - 640) / 2 + 33, (cy - 480) / 2 + 65);

    // Name edit at Ref position (62, 244)
    m_nameEditCtrl = new UIEditCtrl();
    m_nameEditCtrl->Create(100, 18);
    m_nameEditCtrl->Move(m_x + 62, m_y + 244);
    m_nameEditCtrl->m_maxchar = 24;
    m_nameEditCtrl->SetFrameColor(242, 242, 242);
    m_nameEditCtrl->m_hasFocus = true;
    if (!IsQtUiRuntimeEnabled()) {
        AddChild(m_nameEditCtrl);
    }

    const CLoginMode* loginMode = g_modeMgr.GetCurrentLoginMode();
    const char* savedName = loginMode ? loginMode->GetMakingCharName() : nullptr;
    if (savedName) {
        m_nameEditCtrl->SetText(savedName);
    }

    g_windowMgr.m_editWindow = m_nameEditCtrl;
    if (!IsQtUiRuntimeEnabled()) {
        EnsureButtons();
    }
    RebuildPreview();
}

void UIMakeCharWnd::RebuildPreview()
{
    char path[260] = {};
    int head = m_hairIdx;
    const int sex = g_session.GetSex();
    int preservedAction = m_preview.curAction;
    int preservedMotion = m_preview.curMotion;

    if (preservedAction < m_preview.baseAction || preservedAction >= m_preview.baseAction + 8) {
        preservedAction = m_preview.baseAction;
    }
    if (preservedMotion < 0) {
        preservedMotion = 0;
    }

    m_preview.x = m_x + 95;
    m_preview.y = m_y + 213;
    m_preview.baseAction = 0;
    m_preview.curAction = preservedAction;
    m_preview.curMotion = preservedMotion;
    m_preview.bodyPalette = 0;
    m_preview.headPalette = m_hairColor;
    m_preview.actName[0] = g_session.GetJobActName(0, sex, path);
    m_preview.sprName[0] = g_session.GetJobSprName(0, sex, path);
    m_preview.actName[1] = g_session.GetHeadActName(0, &head, sex, path);
    m_preview.sprName[1] = g_session.GetHeadSprName(0, &head, sex, path);
    m_preview.imfName = g_session.GetImfName(0, head, sex, path);
    m_hairIdx = head;

    if (m_preview.bodyPalette > 0) {
        m_preview.bodyPaletteName = g_session.GetBodyPaletteName(0, sex, m_preview.bodyPalette, path);
    } else {
        m_preview.bodyPaletteName.clear();
    }

    if (m_preview.headPalette > 0) {
        m_preview.headPaletteName = g_session.GetHeadPaletteName(head, 0, sex, m_preview.headPalette, path);
    } else {
        m_preview.headPaletteName.clear();
    }
}

void UIMakeCharWnd::DrawPreview(HDC hdc, const PreviewState& preview)
{
    DrawPreviewLayer(hdc, preview, 0);
    DrawPreviewLayer(hdc, preview, 1);
}

void UIMakeCharWnd::OnProcess()
{
    constexpr DWORD kPreviewFrameIntervalMs = 160;
    bool previewFrameChanged = false;
    const DWORD now = GetTickCount();
    if (m_lastPreviewAdvanceTick == 0) {
        m_lastPreviewAdvanceTick = now;
    }
    if (now - m_lastPreviewAdvanceTick >= kPreviewFrameIntervalMs) {
        ++m_preview.curAction;
        if (m_preview.curAction >= m_preview.baseAction + 8) {
            m_preview.curAction = m_preview.baseAction;
        }
        m_lastPreviewAdvanceTick = now;
        previewFrameChanged = true;
    }
    ++m_stateCnt;
    m_preview.curMotion = 0;
    if (previewFrameChanged) {
        Invalidate();
    }
}

void UIMakeCharWnd::OnDraw()
{
    if (!g_hMainWnd || m_show == 0) {
        return;
    }

    EnsureResourceCache();

    RECT rcClient{};
    GetClientRect(g_hMainWnd, &rcClient);
    const int clientW = rcClient.right - rcClient.left;
    const int clientH = rcClient.bottom - rcClient.top;
    if (!m_controlsCreated && clientW > 0 && clientH > 0) {
        OnCreate(clientW, clientH);
    }

    if (IsQtUiRuntimeEnabled()) {
        return;
    }

    const bool useCompose = EnsureComposeSurface(clientW, clientH);
    HDC targetDC = nullptr;
    HDC hdc = nullptr;
    if (useCompose) {
        PatBlt(m_composeSurface.GetDC(), 0, 0, clientW, clientH, BLACKNESS);
        g_windowMgr.DrawWallpaperToDC(m_composeSurface.GetDC(), clientW, clientH);
        hdc = m_composeSurface.GetDC();
    } else {
        targetDC = AcquireDrawTarget();
        if (!targetDC) {
            return;
        }
        hdc = targetDC;
        g_windowMgr.DrawWallpaperToDC(hdc, clientW, clientH);
    }

    RECT panel = MakeRect(m_x, m_y, m_w, m_h);
    if (m_backgroundBmp.IsValid()) {
        DrawBitmapPixelsStretched(hdc, m_backgroundBmp, panel);
    } else {
        HBRUSH bg = CreateSolidBrush(RGB(255, 255, 255));
        FillRect(hdc, &panel, bg);
        DeleteObject(bg);
        FrameRect(hdc, &panel, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
    }

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(96, 50, 36));

    // --- Stat hexagon (Ref center 288,166) with surrounding swap arrows ---
    DrawHexagon(hdc);

    // Draw only the numeric stat values; labels are already baked into the background art.
    SetTextColor(hdc, RGB(60, 36, 20));
    {
        static const int kStatIdx[6] = { 0, 1, 2, 3, 4, 5 };
        static const int kValueY[6] = { 40, 56, 72, 88, 104, 120 };
        const int valueX = m_x + 484;
        HFONT statFont = CreateFontA(-11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE, "Tahoma");
        HGDIOBJ oldFont = nullptr;
        if (statFont) {
            oldFont = SelectObject(hdc, statFont);
        }
        char buf[16];
        for (int i = 0; i < 6; ++i) {
            std::snprintf(buf, sizeof(buf), "%d", m_stats[kStatIdx[i]]);
            RECT valueRc = MakeRect(valueX, m_y + kValueY[i], 30, 13);
#if RO_ENABLE_QT6_UI
            DrawMakeCharText(hdc, valueRc, buf, RGB(60, 36, 20), Qt::AlignCenter | Qt::AlignVCenter);
#else
            DrawTextA(hdc, buf, -1, &valueRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
#endif
        }
        if (oldFont) {
            SelectObject(hdc, oldFont);
        }
        if (statFont) {
            DeleteObject(statFont);
        }
    }

    DrawPreview(hdc, m_preview);

    DrawChildrenToHdc(hdc);

    if (useCompose) {
        if (!BlitArgbBitsToDrawTarget(m_composeSurface.GetBits(), clientW, clientH)) {
            return;
        }
    } else {
        ReleaseDrawTarget(targetDC);
    }
}

msgresult_t UIMakeCharWnd::SendMsg(UIWindow* sender, int msg, msgparam_t wparam, msgparam_t lparam, msgparam_t extra)
{
    (void)sender;
    (void)lparam;
    (void)extra;

    if (msg != 6) {
        return 0;
    }

    // Ref stat-swap logic: min value 1 (check src < 2 before decrement).
    // Each pair maintains sum=10; reset both to 5 if the sum somehow breaks.
    auto swapStat = [&](int srcIdx, int dstIdx) -> int {
        if (m_stats[srcIdx] < 2) return 0;
        --m_stats[srcIdx];
        ++m_stats[dstIdx];
        if (m_stats[srcIdx] + m_stats[dstIdx] != 10) {
            m_stats[srcIdx] = 5;
            m_stats[dstIdx] = 5;
        }
        Invalidate();
        return 1;
    };

    switch (wparam) {
    case 118: {
        PlayUiButtonSound();
        int param[8] = { m_stats[0], m_stats[1], m_stats[2], m_stats[3], m_stats[4], m_stats[5], m_hairColor, m_hairIdx };
        g_modeMgr.SendMsg(CLoginMode::LoginMsg_SetCharParam, reinterpret_cast<msgparam_t>(param), 0, 0);
        g_modeMgr.SendMsg(CLoginMode::LoginMsg_SetMakingCharName,
            reinterpret_cast<msgparam_t>(m_nameEditCtrl ? m_nameEditCtrl->GetText() : ""), 0, 0);
        return g_modeMgr.SendMsg(CLoginMode::LoginMsg_CreateCharacter, 0, 0, 0);
    }

    case 119:
        PlayUiButtonSound();
        return g_modeMgr.SendMsg(CLoginMode::LoginMsg_ReturnToCharSelect, 0, 0, 0);

    // Stat swaps (Ref IDs and directions)
    case 139: return swapStat(3, 0);  // Int→Str
    case 142: return swapStat(0, 3);  // Str→Int
    case 140: return swapStat(5, 1);  // Luk→Agi
    case 144: return swapStat(1, 5);  // Agi→Luk
    case 141: return swapStat(4, 2);  // Dex→Vit
    case 143: return swapStat(2, 4);  // Vit→Dex

    case 160:
        m_hairIdx = (m_hairIdx >= 25) ? 1 : m_hairIdx + 1;
        RebuildPreview();
        Invalidate();
        return 1;
    case 161:
        m_hairIdx = (m_hairIdx <= 1) ? 25 : m_hairIdx - 1;
        RebuildPreview();
        Invalidate();
        return 1;
    case 213:
        m_hairColor = (m_hairColor >= 8) ? 0 : m_hairColor + 1;
        RebuildPreview();
        Invalidate();
        return 1;

    default:
        return 0;
    }
}

void UIMakeCharWnd::OnKeyDown(int virtualKey)
{
    if (virtualKey == VK_RETURN) {
        SendMsg(nullptr, 6, 118, 0, 0);
    } else if (virtualKey == VK_ESCAPE) {
        SendMsg(nullptr, 6, 119, 0, 0);
    }
}