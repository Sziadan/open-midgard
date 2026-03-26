#include "UISelectCharWnd.h"

#include "core/File.h"
#include "gamemode/LoginMode.h"
#include "gamemode/Mode.h"
#include "main/WinMain.h"
#include "render/DC.h"
#include "render3d/Device.h"
#include "res/ActRes.h"
#include "res/ImfRes.h"
#include "res/PaletteRes.h"
#include "res/Sprite.h"
#include "session/Session.h"
#include "ui/UIWindowMgr.h"

#include <gdiplus.h>
#include <windows.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <vector>

#pragma comment(lib, "gdiplus.lib")

namespace {

constexpr int kWindowWidth = 576;
constexpr int kWindowHeight = 342;
constexpr int kVisibleSlotsPerPage = 3;
constexpr int kSlotWidth = 163;
constexpr int kSlotHeight = 166;
constexpr int kSlotGap = 0;
constexpr int kSlotLeft = 56;
constexpr int kSlotTop = 40;
constexpr int kPreviewBaseX = 124;
constexpr int kPreviewBaseY = 157;
constexpr int kPageButtonXPrev = 44;
constexpr int kPageButtonXNext = 520;
constexpr int kPageButtonY = 110;
constexpr int kPageButtonWidth = 12;
constexpr int kPageButtonHeight = 48;
constexpr char kRegPath[] = "Software\\Gravity Soft\\Ragnarok Online";
constexpr char kCurSlotValue[] = "CURSLOT";

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

HBITMAP LoadBitmapFromGameData(const char* path)
{
    if (!path || !*path || !EnsureGdiplusStarted()) {
        return nullptr;
    }

    int size = 0;
    unsigned char* bytes = g_fileMgr.GetData(path, &size);
    if (!bytes || size <= 0) {
        delete[] bytes;
        return nullptr;
    }

    HBITMAP outBmp = nullptr;
    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, static_cast<SIZE_T>(size));
    if (mem) {
        void* dst = GlobalLock(mem);
        if (dst) {
            std::memcpy(dst, bytes, static_cast<size_t>(size));
            GlobalUnlock(mem);

            IStream* stream = nullptr;
            if (CreateStreamOnHGlobal(mem, TRUE, &stream) == S_OK) {
                auto* bmp = Gdiplus::Bitmap::FromStream(stream, FALSE);
                if (bmp && bmp->GetLastStatus() == Gdiplus::Ok) {
                    bmp->GetHBITMAP(RGB(0, 0, 0), &outBmp);
                }
                delete bmp;
                stream->Release();
            } else {
                GlobalFree(mem);
            }
        } else {
            GlobalFree(mem);
        }
    }

    delete[] bytes;
    return outBmp;
}

void DrawBitmapStretched(HDC target, HBITMAP bmp, const RECT& dst)
{
    if (!target || !bmp) {
        return;
    }

    BITMAP bm{};
    if (!GetObjectA(bmp, sizeof(bm), &bm) || bm.bmWidth <= 0 || bm.bmHeight <= 0) {
        return;
    }

    HDC srcDC = CreateCompatibleDC(target);
    if (!srcDC) {
        return;
    }

    HGDIOBJ old = SelectObject(srcDC, bmp);
    SetStretchBltMode(target, HALFTONE);
    StretchBlt(target,
        dst.left,
        dst.top,
        dst.right - dst.left,
        dst.bottom - dst.top,
        srcDC,
        0,
        0,
        bm.bmWidth,
        bm.bmHeight,
        SRCCOPY);
    SelectObject(srcDC, old);
    DeleteDC(srcDC);
}

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

HBITMAP LoadFirstBitmapFromCandidates(const std::vector<std::string>& candidates, std::string* outPath)
{
    for (const std::string& candidate : candidates) {
        HBITMAP bmp = LoadBitmapFromGameData(candidate.c_str());
        if (bmp) {
            if (outPath) {
                *outPath = candidate;
            }
            return bmp;
        }
    }
    return nullptr;
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

bool PointInRectXY(const RECT& rc, int x, int y)
{
    return x >= rc.left && x < rc.right && y >= rc.top && y < rc.bottom;
}

void FillRectColor(HDC hdc, const RECT& rc, COLORREF color)
{
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(hdc, &rc, brush);
    DeleteObject(brush);
}

void DrawRectFrame(HDC hdc, const RECT& rc, COLORREF color)
{
    HBRUSH brush = CreateSolidBrush(color);
    FrameRect(hdc, &rc, brush);
    DeleteObject(brush);
}

void DrawSlotInset(HDC hdc, const RECT& rc, COLORREF color)
{
    RECT inset = rc;
    InflateRect(&inset, -1, -1);
    DrawRectFrame(hdc, inset, color);
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

POINT GetPreviewLayerPoint(const UISelectCharWnd::PreviewState& preview,
    int layerPriority,
    int resolvedLayer,
    CImfRes* imfRes,
    const CMotion* motion)
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

bool DrawPreviewLayer(HDC hdc, const UISelectCharWnd::PreviewState& preview, int layerIndex)
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

void CopyCharacterName(const CHARACTER_INFO& info, char (&out)[25])
{
    std::memcpy(out, info.name, 24);
    out[24] = 0;
}

const char* ResolveJobName(int job)
{
    switch (job) {
    case 0: return "Novice";
    case 1: return "Swordsman";
    case 2: return "Mage";
    case 3: return "Archer";
    case 4: return "Acolyte";
    case 5: return "Merchant";
    case 6: return "Thief";
    case 7: return "Knight";
    case 8: return "Priest";
    case 9: return "Wizard";
    case 10: return "Blacksmith";
    case 11: return "Hunter";
    case 12: return "Assassin";
    case 14: return "Crusader";
    case 15: return "Monk";
    case 16: return "Sage";
    case 17: return "Rogue";
    case 18: return "Alchemist";
    case 19: return "Bard";
    default: return "Unknown";
    }
}

} // namespace

UISelectCharWnd::UISelectCharWnd()
    : m_controlsCreated(false), m_assetsProbed(false), m_backgroundBmp(nullptr),
      m_slotBmp(nullptr), m_slotSelectedBmp(nullptr),
      m_composeDC(nullptr), m_composeBitmap(nullptr), m_composeWidth(0), m_composeHeight(0),
      m_okButton(nullptr), m_cancelButton(nullptr), m_makeButton(nullptr),
      m_deleteButton(nullptr), m_chargeButton(nullptr), m_selectedSlot(0), m_page(0),
      m_lastAnimTick(0), m_animAction(0)
{
    m_defPushId = 118;
    m_defCancelPushId = 119;
}

UISelectCharWnd::~UISelectCharWnd()
{
    ReleaseComposeSurface();
    ClearAssets();
}

void UISelectCharWnd::ClearAssets()
{
    if (m_backgroundBmp) {
        DeleteObject(m_backgroundBmp);
        m_backgroundBmp = nullptr;
    }
    m_backgroundPath.clear();
    if (m_slotBmp) {
        DeleteObject(m_slotBmp);
        m_slotBmp = nullptr;
    }
    if (m_slotSelectedBmp) {
        DeleteObject(m_slotSelectedBmp);
        m_slotSelectedBmp = nullptr;
    }
}

void UISelectCharWnd::ReleaseComposeSurface()
{
    if (m_composeBitmap) {
        DeleteObject(m_composeBitmap);
        m_composeBitmap = nullptr;
    }
    if (m_composeDC) {
        DeleteDC(m_composeDC);
        m_composeDC = nullptr;
    }
    m_composeWidth = 0;
    m_composeHeight = 0;
}

bool UISelectCharWnd::EnsureComposeSurface(HDC referenceDC, int width, int height)
{
    if (!referenceDC || width <= 0 || height <= 0) {
        return false;
    }

    if (m_composeDC && m_composeBitmap && m_composeWidth == width && m_composeHeight == height) {
        return true;
    }

    ReleaseComposeSurface();

    m_composeDC = CreateCompatibleDC(referenceDC);
    if (!m_composeDC) {
        return false;
    }

    m_composeBitmap = CreateCompatibleBitmap(referenceDC, width, height);
    if (!m_composeBitmap) {
        ReleaseComposeSurface();
        return false;
    }

    SelectObject(m_composeDC, m_composeBitmap);
    m_composeWidth = width;
    m_composeHeight = height;
    return true;
}

void UISelectCharWnd::EnsureResourceCache()
{
    if (m_assetsProbed) {
        return;
    }
    m_assetsProbed = true;

    const char* panelNames[] = {
        "win_select.bmp",
        "win_selectchar.bmp",
        "win_selchar.bmp",
        "selwin.bmp",
        nullptr
    };

    for (int i = 0; panelNames[i] && !m_backgroundBmp; ++i) {
        m_backgroundBmp = LoadFirstBitmapFromCandidates(BuildUiAssetCandidates(panelNames[i]), &m_backgroundPath);
    }

    const char* slotBmpNames[] = {
        "selectslot_img.bmp",
        "selslot.bmp",
        "slot_bg.bmp",
        nullptr
    };
    for (int i = 0; slotBmpNames[i] && !m_slotBmp; ++i) {
        m_slotBmp = LoadFirstBitmapFromCandidates(BuildUiAssetCandidates(slotBmpNames[i]), nullptr);
    }

    const char* selSlotBmpNames[] = {
        "selectslot_select.bmp",
        "selslot_on.bmp",
        nullptr
    };
    for (int i = 0; selSlotBmpNames[i] && !m_slotSelectedBmp; ++i) {
        m_slotSelectedBmp = LoadFirstBitmapFromCandidates(BuildUiAssetCandidates(selSlotBmpNames[i]), nullptr);
    }
}

void UISelectCharWnd::EnsureButtons()
{
    auto createButton = [this](UIBitmapButton*& button,
        int id,
        const char* normal,
        const char* hover,
        const char* pressed,
        int x,
        int y,
        int fallbackW,
        int fallbackH) {
            if (button) {
                button->Move(x, y);
                return;
            }

            button = new UIBitmapButton();
            button->SetBitmapName(ResolveUiAssetPath(normal).c_str(), 0);
            button->SetBitmapName(ResolveUiAssetPath(hover).c_str(), 1);
            button->SetBitmapName(ResolveUiAssetPath(pressed).c_str(), 2);
            button->Create(button->m_bitmapWidth > 0 ? button->m_bitmapWidth : fallbackW,
                button->m_bitmapHeight > 0 ? button->m_bitmapHeight : fallbackH);
            button->Move(x, y);
            button->m_id = id;
            AddChild(button);
        };

    createButton(m_cancelButton, 119,
        "btn_cancel.bmp", "btn_cancel_a.bmp", "btn_cancel_b.bmp",
        m_x + 484, m_y + 318, 90, 24);
    createButton(m_okButton, 118,
        "btn_ok.bmp", "btn_ok_a.bmp", "btn_ok_b.bmp",
        m_x + 404, m_y + 318, 80, 24);
    createButton(m_makeButton, 137,
        "btn_make.bmp", "btn_make_a.bmp", "btn_make_b.bmp",
        m_x + 404, m_y + 318, 80, 24);
    createButton(m_deleteButton, 145,
        "btn_del.bmp", "btn_del_a.bmp", "btn_del_b.bmp",
        m_x + 5, m_y + 318, 80, 24);
    createButton(m_chargeButton, 218,
        "btn_charge.bmp", "btn_charge_a.bmp", "btn_charge_b.bmp",
        m_x + 314, m_y + 318, 85, 24);
}

void UISelectCharWnd::UpdateActionButtons()
{
    EnsureButtons();

    const bool selectedEmpty = (FindCharacterIndexForSlot(m_selectedSlot) < 0);
    if (m_okButton) {
        m_okButton->m_show = selectedEmpty ? 0 : 1;
    }
    if (m_makeButton) {
        m_makeButton->m_show = selectedEmpty ? 1 : 0;
    }
    if (m_deleteButton) {
        m_deleteButton->m_show = selectedEmpty ? 0 : 1;
    }
    if (m_cancelButton) {
        m_cancelButton->m_show = 1;
    }
    if (m_chargeButton) {
        m_chargeButton->m_show = 1;
    }

    m_defPushId = selectedEmpty ? 137 : 118;
}

void UISelectCharWnd::OnCreate(int cx, int cy)
{
    if (!m_controlsCreated) {
        Create(kWindowWidth, kWindowHeight);
        LoadSelectionFromRegistry();
        m_controlsCreated = true;
    }

    Move((cx - 640) / 2 + 33, (cy - 480) / 2 + 65);
    ClampSelection();
    EnsureButtons();
    UpdateActionButtons();
}

void UISelectCharWnd::OnProcess()
{
}

int UISelectCharWnd::GetCharacterCount() const
{
    return g_modeMgr.SendMsg(CLoginMode::LoginMsg_GetCharCount, 0, 0, 0);
}

CHARACTER_INFO* UISelectCharWnd::GetCharacters() const
{
    return reinterpret_cast<CHARACTER_INFO*>(g_modeMgr.SendMsg(CLoginMode::LoginMsg_GetCharInfo, 0, 0, 0));
}

int UISelectCharWnd::GetPageCount() const
{
    const int count = GetCharacterCount();
    CHARACTER_INFO* chars = GetCharacters();
    int maxSlot = kVisibleSlotsPerPage - 1;
    if (chars) {
        for (int index = 0; index < count; ++index) {
            maxSlot = std::max(maxSlot, static_cast<int>(chars[index].CharNum));
        }
    }
    return std::clamp((maxSlot / kVisibleSlotsPerPage) + 1, 1, 4);
}

int UISelectCharWnd::GetSlotCount() const
{
    return GetPageCount() * kVisibleSlotsPerPage;
}

int UISelectCharWnd::FindCharacterIndexForSlot(int slotNumber) const
{
    const int count = GetCharacterCount();
    CHARACTER_INFO* chars = GetCharacters();
    if (!chars) {
        return -1;
    }

    for (int index = 0; index < count; ++index) {
        if (static_cast<int>(chars[index].CharNum) == slotNumber) {
            return index;
        }
    }
    return -1;
}

int UISelectCharWnd::GetVisibleSlotStart() const
{
    return m_page * kVisibleSlotsPerPage;
}

void UISelectCharWnd::ClampSelection()
{
    const int pageCount = GetPageCount();
    if (m_page < 0) {
        m_page = pageCount - 1;
    }
    if (m_page >= pageCount) {
        m_page = 0;
    }

    const int slotCount = GetSlotCount();
    if (m_selectedSlot < 0) {
        m_selectedSlot = 0;
    }
    if (m_selectedSlot >= slotCount) {
        m_selectedSlot = slotCount - 1;
    }

    m_page = m_selectedSlot / kVisibleSlotsPerPage;
}

void UISelectCharWnd::SaveSelectionToRegistry() const
{
    DWORD slot = static_cast<DWORD>(m_selectedSlot);
    HKEY key = nullptr;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, kRegPath, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) == ERROR_SUCCESS) {
        RegSetValueExA(key, kCurSlotValue, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&slot), sizeof(slot));
        RegCloseKey(key);
    }
}

void UISelectCharWnd::LoadSelectionFromRegistry()
{
    DWORD slot = 0;
    DWORD size = sizeof(slot);
    HKEY key = nullptr;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, kRegPath, 0, KEY_READ, &key) == ERROR_SUCCESS) {
        const LONG result = RegQueryValueExA(key, kCurSlotValue, nullptr, nullptr, reinterpret_cast<BYTE*>(&slot), &size);
        RegCloseKey(key);
        if (result == ERROR_SUCCESS) {
            m_selectedSlot = static_cast<int>(slot);
            m_page = m_selectedSlot / kVisibleSlotsPerPage;
        }
    }
}

void UISelectCharWnd::SetSelectedSlot(int slotNumber)
{
    m_selectedSlot = slotNumber;
    ClampSelection();
    SaveSelectionToRegistry();
    UpdateActionButtons();
}

void UISelectCharWnd::MoveSelection(int delta)
{
    const int slotCount = GetSlotCount();
    if (slotCount <= 0) {
        return;
    }

    int slot = m_selectedSlot + delta;
    while (slot < 0) {
        slot += slotCount;
    }
    while (slot >= slotCount) {
        slot -= slotCount;
    }

    SetSelectedSlot(slot);
}

void UISelectCharWnd::ChangePage(int delta)
{
    const int pageCount = GetPageCount();
    if (pageCount <= 1) {
        return;
    }

    const int slotOffset = m_selectedSlot % kVisibleSlotsPerPage;
    int page = m_page + delta;
    while (page < 0) {
        page += pageCount;
    }
    while (page >= pageCount) {
        page -= pageCount;
    }

    SetSelectedSlot(page * kVisibleSlotsPerPage + slotOffset);
}

void UISelectCharWnd::ActivateSelectedSlot()
{
    ClampSelection();
    const int charIndex = FindCharacterIndexForSlot(m_selectedSlot);
    if (charIndex >= 0) {
        g_modeMgr.SendMsg(CLoginMode::LoginMsg_SelectCharacter, charIndex, 0, 0);
        return;
    }

    ActivateCreate();
}

void UISelectCharWnd::ActivateCreate()
{
    g_modeMgr.SendMsg(CLoginMode::LoginMsg_RequestMakeCharacter, m_selectedSlot, 0, 0);
}

void UISelectCharWnd::ActivateDelete()
{
    const int charIndex = FindCharacterIndexForSlot(m_selectedSlot);
    if (charIndex < 0) {
        return;
    }
    g_modeMgr.SendMsg(CLoginMode::LoginMsg_RequestDeleteCharacter, m_selectedSlot, charIndex, 0);
}

void UISelectCharWnd::BuildPreviewForSlot(int visibleIndex, const CHARACTER_INFO& info)
{
    if (visibleIndex < 0 || visibleIndex >= static_cast<int>(m_visiblePreviews.size())) {
        return;
    }

    PreviewState& preview = m_visiblePreviews[visibleIndex];
    preview = PreviewState{};
    preview.valid = true;
    preview.x = m_x + kPreviewBaseX + visibleIndex * kSlotWidth;
    preview.y = m_y + kPreviewBaseY;
    preview.baseAction = 0;
    preview.curAction = preview.baseAction;
    preview.curMotion = 0;
    preview.bodyPalette = info.bodypalette;
    preview.headPalette = info.headpalette;

    char path[260] = {};
    int head = info.head;
    const int sex = g_session.GetSex();

    preview.actName[0] = g_session.GetJobActName(info.job, sex, path);
    preview.sprName[0] = g_session.GetJobSprName(info.job, sex, path);
    preview.actName[1] = g_session.GetHeadActName(info.job, &head, sex, path);
    preview.sprName[1] = g_session.GetHeadSprName(info.job, &head, sex, path);
    preview.imfName = g_session.GetImfName(info.job, head, sex, path);

    if (preview.bodyPalette > 0) {
        preview.bodyPaletteName = g_session.GetBodyPaletteName(info.job, sex, preview.bodyPalette, path);
    }
    if (preview.headPalette > 0) {
        preview.headPaletteName = g_session.GetHeadPaletteName(head, info.job, sex, preview.headPalette, path);
    }
}

void UISelectCharWnd::RebuildVisiblePreviews()
{
    for (PreviewState& preview : m_visiblePreviews) {
        preview = PreviewState{};
    }

    CHARACTER_INFO* chars = GetCharacters();
    if (!chars) {
        return;
    }

    const int baseSlot = GetVisibleSlotStart();
    for (int visibleIndex = 0; visibleIndex < kVisibleSlotsPerPage; ++visibleIndex) {
        const int slotNumber = baseSlot + visibleIndex;
        const int charIndex = FindCharacterIndexForSlot(slotNumber);
        if (charIndex >= 0) {
            BuildPreviewForSlot(visibleIndex, chars[charIndex]);
        }
    }
}

void UISelectCharWnd::DrawPreview(HDC hdc, const PreviewState& preview) const
{
    if (!preview.valid) {
        return;
    }

    DrawPreviewLayer(hdc, preview, 0);
    DrawPreviewLayer(hdc, preview, 1);
}

int UISelectCharWnd::HitSlot(int x, int y) const
{
    for (int offset = 0; offset < kVisibleSlotsPerPage; ++offset) {
        const RECT rc = MakeRect(
            m_x + kSlotLeft + offset * (kSlotWidth + kSlotGap),
            m_y + kSlotTop,
            kSlotWidth,
            kSlotHeight);
        if (PointInRectXY(rc, x, y)) {
            return GetVisibleSlotStart() + offset;
        }
    }
    return -1;
}

bool UISelectCharWnd::HitPrevPageButton(int x, int y) const
{
    const RECT rc = MakeRect(m_x + kPageButtonXPrev, m_y + kPageButtonY, kPageButtonWidth, kPageButtonHeight);
    return PointInRectXY(rc, x, y);
}

bool UISelectCharWnd::HitNextPageButton(int x, int y) const
{
    const RECT rc = MakeRect(m_x + kPageButtonXNext, m_y + kPageButtonY, kPageButtonWidth, kPageButtonHeight);
    return PointInRectXY(rc, x, y);
}

void UISelectCharWnd::OnLBtnUp(int x, int y)
{
    const int slot = HitSlot(x, y);
    if (slot >= 0) {
        SetSelectedSlot(slot);
        return;
    }

    if (HitPrevPageButton(x, y)) {
        ChangePage(-1);
        return;
    }

    if (HitNextPageButton(x, y)) {
        ChangePage(1);
    }
}

int UISelectCharWnd::SendMsg(UIWindow* sender, int msg, int wparam, int lparam, int extra)
{
    (void)sender;
    (void)lparam;
    (void)extra;

    if (msg != 6) {
        return 0;
    }

    switch (wparam) {
    case 118:
        PlayUiButtonSound();
        ActivateSelectedSlot();
        return 1;

    case 119:
        PlayUiButtonSound();
        return g_modeMgr.SendMsg(CLoginMode::LoginMsg_ReturnToLogin, 0, 0, 0);

    case 137:
        PlayUiButtonSound();
        ActivateCreate();
        return 1;

    case 145:
        PlayUiButtonSound();
        ActivateDelete();
        return 1;

    case 218:
        PlayUiButtonSound();
        g_windowMgr.SetLoginStatus("Character service billing is not implemented in this rebuild.");
        return 1;

    default:
        return 0;
    }
}

void UISelectCharWnd::OnKeyDown(int virtualKey)
{
    switch (virtualKey) {
    case VK_LEFT:
        MoveSelection(-1);
        break;
    case VK_RIGHT:
        MoveSelection(1);
        break;
    case VK_PRIOR:
        ChangePage(-1);
        break;
    case VK_NEXT:
        ChangePage(1);
        break;
    case VK_RETURN:
        SendMsg(nullptr, 6, m_defPushId, 0, 0);
        break;
    case VK_DELETE:
        SendMsg(nullptr, 6, 145, 0, 0);
        break;
    case VK_ESCAPE:
        SendMsg(nullptr, 6, 119, 0, 0);
        break;
    default:
        break;
    }
}

void UISelectCharWnd::OnDraw()
{
    if (!g_hMainWnd || m_show == 0) {
        return;
    }

    EnsureResourceCache();

    RECT rcClient{};
    GetClientRect(g_hMainWnd, &rcClient);
    const int clientW = rcClient.right - rcClient.left;
    const int clientH = rcClient.bottom - rcClient.top;
    if (clientW > 0 && clientH > 0) {
        OnCreate(clientW, clientH);
    }

    ClampSelection();
    UpdateActionButtons();
    RebuildVisiblePreviews();

    const bool useShared = (UIWindow::GetSharedDrawDC() != nullptr);
    HDC targetDC = useShared ? UIWindow::GetSharedDrawDC() : GetDC(g_hMainWnd);
    if (!targetDC) {
        return;
    }

    HDC hdc = targetDC;
    const bool useCompose = EnsureComposeSurface(targetDC, clientW, clientH);
    if (useCompose) {
        PatBlt(m_composeDC, 0, 0, clientW, clientH, BLACKNESS);
        g_windowMgr.DrawWallpaperToDC(m_composeDC, clientW, clientH);
        hdc = m_composeDC;
    } else {
        g_windowMgr.DrawWallpaperToDC(hdc, clientW, clientH);
    }

    const RECT panel = MakeRect(m_x, m_y, m_w, m_h);
    if (m_backgroundBmp) {
        DrawBitmapStretched(hdc, m_backgroundBmp, panel);
    } else {
        FillRectColor(hdc, panel, RGB(255, 255, 255));
        DrawRectFrame(hdc, panel, RGB(78, 58, 41));
    }

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(94, 56, 38));

    if (GetPageCount() > 1) {
        RECT prevRc = MakeRect(m_x + kPageButtonXPrev, m_y + kPageButtonY, kPageButtonWidth, kPageButtonHeight);
        RECT nextRc = MakeRect(m_x + kPageButtonXNext, m_y + kPageButtonY, kPageButtonWidth, kPageButtonHeight);
        DrawRectFrame(hdc, prevRc, RGB(120, 96, 68));
        DrawRectFrame(hdc, nextRc, RGB(120, 96, 68));
        DrawTextA(hdc, "<", -1, &prevRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        DrawTextA(hdc, ">", -1, &nextRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    CHARACTER_INFO* chars = GetCharacters();
    const int baseSlot = GetVisibleSlotStart();

    for (int offset = 0; offset < kVisibleSlotsPerPage; ++offset) {
        DrawPreview(hdc, m_visiblePreviews[offset]);
    }

    const int selectedIndex = FindCharacterIndexForSlot(m_selectedSlot);
    if (selectedIndex >= 0 && chars) {
        const CHARACTER_INFO& info = chars[selectedIndex];
        char nameBuf[25];
        CopyCharacterName(info, nameBuf);
        char line[64];
        auto drawLabel = [&](int rx, int ry, const char* text) {
            RECT rc = MakeRect(m_x + rx, m_y + ry, 140, 14);
            DrawTextA(hdc, text, -1, &rc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        };

        SetTextColor(hdc, RGB(80, 50, 30));
        drawLabel(69, 206, nameBuf);
        drawLabel(69, 222, ResolveJobName(info.job));
        std::snprintf(line, sizeof(line), "%d", info.level);
        drawLabel(69, 238, line);
        std::snprintf(line, sizeof(line), "%u", info.exp);
        drawLabel(69, 254, line);
        std::snprintf(line, sizeof(line), "%d", info.hp);
        drawLabel(69, 270, line);
        std::snprintf(line, sizeof(line), "%d", info.sp);
        drawLabel(69, 286, line);

        std::snprintf(line, sizeof(line), "%d", info.Str); drawLabel(213, 206, line);
        std::snprintf(line, sizeof(line), "%d", info.Agi); drawLabel(213, 222, line);
        std::snprintf(line, sizeof(line), "%d", info.Vit); drawLabel(213, 238, line);
        std::snprintf(line, sizeof(line), "%d", info.Int); drawLabel(213, 254, line);
        std::snprintf(line, sizeof(line), "%d", info.Dex); drawLabel(213, 270, line);
        std::snprintf(line, sizeof(line), "%d", info.Luk); drawLabel(213, 286, line);
    }

    HDC prevShared = UIWindow::GetSharedDrawDC();
    UIWindow::SetSharedDrawDC(hdc);
    DrawChildren();
    UIWindow::SetSharedDrawDC(prevShared);

    if (useCompose) {
        BitBlt(targetDC, 0, 0, clientW, clientH, hdc, 0, 0, SRCCOPY);
    }

    if (!useShared) {
        ReleaseDC(g_hMainWnd, targetDC);
    }
}