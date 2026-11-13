#include "UIGroundSkillTextWnd.h"
#include "UIWindowMgr.h"
#include "gamemode/Commerce.h"
#include "network/Connection.h"
#include "UILoadingWnd.h"
#include "UINewChatWnd.h"
#include "UIChatRoomWnd.h"
#include "UIChatRoomSettingsWnd.h"
#include "UIWhisperWnd.h"
#include "UIAutoMessageWnd.h"
#include "UIFriendOptionWnd.h"
#include "UITipOfTheDayWnd.h"
#include "UICommandListWnd.h"
#include "UIPetInfoWnd.h"
#include "UICompanionStatusWnd.h"
#include "UIAuctionWnd.h"
#include "UIInstanceWnd.h"
#include "UIQuestWnd.h"
#include "UICompanionSkillListWnd.h"
#include "UIPetEggListWnd.h"
#include "UIPetTamingDeceiveWnd.h"
#include "core/TipOfTheDay.h"
#include "audio/Audio.h"
#include "res/Res.h"
#include "core/UiMessageStrings.h"
#include "UIBasicInfoWnd.h"
#include "UIStatusWnd.h"
#include "UISayDialogWnd.h"
#include "UINpcMenuWnd.h"
#include "UIPlayerContextMenuWnd.h"
#include "UIJoinPartyAcceptWnd.h"
#include "UIMessengerGroupWnd.h"
#include "UIPartyOptionWnd.h"
#include "UIGuildWnd.h"
#include "UINpcInputWnd.h"
#include "UIChooseWnd.h"
#include "UISelectCartWnd.h"
#include "UIChooseSellBuyWnd.h"
#include "UIEquipWnd.h"
#include "UIItemCollectionWnd.h"
#include "UIItemCompositionWnd.h"
#include "UIItemIdentifyWnd.h"
#include "UISkillSelectWnd.h"
#include "UIMonsterInfoWnd.h"
#include "UIItemInfoWnd.h"
#include "UINotifyItemObtainWnd.h"
#include "UIItemWnd.h"
#include "UIStorageWnd.h"
#include "UIItemPurchaseWnd.h"
#include "UIItemSellWnd.h"
#include "UIItemShopWnd.h"
#include "UIShortCutWnd.h"
#include "UISkillDescribeWnd.h"
#include "UIMinimapWnd.h"
#include "UISkillListWnd.h"
#include "UILoginWnd.h"
#include "UISelectServerWnd.h"
#include "session/Session.h"
#include "UIMakeCharWnd.h"
#include "UINotifyLevelUpWnd.h"
#include "UIOptionWnd.h"
#include "UISelectCharWnd.h"
#include "UIWaitWnd.h"
#include "UiScale.h"
#include "gamemode/CursorRenderer.h"
#include "gamemode/GameMode.h"
#include "gamemode/LoginMode.h"
#include "gamemode/Mode.h"
#include "qtui/QtUiRuntime.h"

#include "core/File.h"
#include "core/SettingsIni.h"
#include "res/Bitmap.h"
#include "render3d/Device.h"
#include "render3d/RenderDevice.h"
#include "res/Texture.h"
#include "render/Renderer.h"
#include "main/WinMain.h"
#include "DebugLog.h"

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>
#include <memory>
#include <vector>

namespace {
const char* CompanionPlacementKey(int id)
{
    switch (id) {
    case UIWindowMgr::WID_HOMUNINFOWND: return "HomunInfoWnd";
    case UIWindowMgr::WID_MERINFOWND: return "MerInfoWnd";
    case UIWindowMgr::WID_HOMUNSKILLLISTWND: return "HomunSkillListWnd";
    case UIWindowMgr::WID_MERSKILLLISTWND: return "MerSkillListWnd";
    default: return nullptr;
    }
}

void RestoreCompanionPlacement(UIWindow& window, const RECT& viewport, int defaultX, int defaultY)
{
    const std::string key = CompanionPlacementKey(window.m_id);
    const int maxX = std::max(0L, viewport.right - viewport.left - window.m_w);
    const int maxY = std::max(0L, viewport.bottom - viewport.top - window.m_h);
    const int x = LoadSettingsIniInt("UIWindows", (key + "X").c_str(), defaultX);
    const int y = LoadSettingsIniInt("UIWindows", (key + "Y").c_str(), defaultY);
    if (x >= 0 && x <= maxX && y >= 0 && y <= maxY) {
        window.Move(x, y);
    } else {
        window.Move(std::clamp(defaultX, 0, maxX), std::clamp(defaultY, 0, maxY));
    }
}

UIGroundSkillTextWnd* GroundSkillTextOwner(UIWindow* window)
{
    for(auto* current=window;current;current=current->m_parent)
        if(auto* owner=dynamic_cast<UIGroundSkillTextWnd*>(current))return owner;
    return nullptr;
}
}

namespace {
std::string ShortcutVirtualKeyLabel(int virtualKey)
{
    if (virtualKey >= '0' && virtualKey <= '9') {
        return std::string(1, static_cast<char>(virtualKey));
    }
    if (virtualKey >= 'A' && virtualKey <= 'Z') {
        return std::string(1, static_cast<char>(virtualKey));
    }
    if (virtualKey >= VK_F1 && virtualKey <= VK_F24) {
        return "F" + std::to_string(virtualKey - VK_F1 + 1);
    }
    if (virtualKey >= VK_NUMPAD0 && virtualKey <= VK_NUMPAD9) {
        return "Num" + std::to_string(virtualKey - VK_NUMPAD0);
    }
    switch (virtualKey) {
    case VK_SPACE: return "Space";
    case VK_TAB: return "Tab";
    case VK_RETURN: return "Enter";
    case VK_ESCAPE: return "Esc";
    case VK_BACK: return "Backspace";
    case VK_DELETE: return "Delete";
    case VK_INSERT: return "Insert";
    case VK_HOME: return "Home";
    case VK_END: return "End";
    case VK_PRIOR: return "PgUp";
    case VK_NEXT: return "PgDn";
    case VK_LEFT: return "Left";
    case VK_RIGHT: return "Right";
    case VK_UP: return "Up";
    case VK_DOWN: return "Down";
    default: return "Key " + std::to_string(virtualKey);
    }
}

std::string ShortcutBindingLabel(const ShortcutKeyBinding& binding)
{
    std::string label;
    if (binding.modifiers & kShortcutKeyModifierControl) label += "Ctrl+";
    if (binding.modifiers & kShortcutKeyModifierShift) label += "Shift+";
    if (binding.modifiers & kShortcutKeyModifierAlt) label += "Alt+";
    return label + ShortcutVirtualKeyLabel(binding.virtualKey);
}


constexpr int kUiChatEventMsg = 101;
constexpr size_t kMaxChatEvents = 256;
constexpr bool kLogWallpaperLoad = false;
constexpr int kWindowSnapDistance = 14;

#define LOG_WALLPAPER_LOAD(...) do { if constexpr (kLogWallpaperLoad) { DbgLog(__VA_ARGS__); } } while (0)

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

const std::vector<std::string>& GetArchiveNamesByExtension(const char* ext)
{
    static std::map<std::string, std::vector<std::string>> s_byExt;
    const std::string key = ToLowerAscii(ext ? ext : "");
    auto it = s_byExt.find(key);
    if (it != s_byExt.end()) {
        return it->second;
    }

    std::vector<std::string> names;
    g_fileMgr.CollectDataNamesByExtension(key.c_str(), names);
    auto inserted = s_byExt.emplace(key, std::move(names));
    return inserted.first->second;
}

std::string GetLowerFileName(const std::string& value)
{
    std::string normalized = NormalizeSlash(value);
    const size_t slashPos = normalized.find_last_of('\\');
    if (slashPos != std::string::npos && slashPos + 1 < normalized.size()) {
        normalized = normalized.substr(slashPos + 1);
    }
    return ToLowerAscii(std::move(normalized));
}

std::string GetLowerFileExtension(const std::string& value)
{
    const std::string normalized = NormalizeSlash(value);
    const size_t dotPos = normalized.find_last_of('.');
    if (dotPos == std::string::npos || dotPos + 1 >= normalized.size()) {
        return {};
    }
    return ToLowerAscii(normalized.substr(dotPos + 1));
}

int ScoreArchiveWallpaperPath(const std::string& requestedPath, const std::string& resolvedPath)
{
    const std::string loweredRequested = ToLowerAscii(NormalizeSlash(requestedPath));
    const std::string loweredResolved = ToLowerAscii(NormalizeSlash(resolvedPath));
    int score = 0;

    if (loweredResolved == loweredRequested) {
        score += 10000;
    }
    if (loweredResolved.rfind("data\\", 0) == 0) {
        score += 800;
    }
    if (loweredResolved.find("\\texture\\") != std::string::npos) {
        score += 300;
    }
    if (loweredResolved.find("\\login_interface\\") != std::string::npos) {
        score += 250;
    }
    if (loweredResolved.find("\\basic_interface\\") != std::string::npos) {
        score += 200;
    }
    if (loweredResolved.find("\\interface\\") != std::string::npos) {
        score += 100;
    }
    if (loweredResolved.find("\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA") != std::string::npos) {
        score += 450;
    }

    return score;
}

bool ContainsManagedWindow(const UIWindowMgr& manager, const UIWindow* window)
{
    if (!window) {
        return false;
    }

    return std::find(manager.m_children.begin(), manager.m_children.end(), window) != manager.m_children.end();
}

bool IsWindowShown(const UIWindow* window)
{
    return window && window->m_show != 0;
}

bool IsLoadingScreenVisible(const UIWindowMgr& manager)
{
    return IsWindowShown(manager.m_loadingWnd);
}

std::string ResolveArchiveWallpaperByFileName(const std::string& requestedPath)
{
    const std::string extension = GetLowerFileExtension(requestedPath);
    if (extension.empty()) {
        return {};
    }

    const std::string requestedFileName = GetLowerFileName(requestedPath);
    if (requestedFileName.empty()) {
        return {};
    }

    const std::vector<std::string>& names = GetArchiveNamesByExtension(extension.c_str());
    int bestScore = -1;
    std::string bestPath;
    for (const std::string& name : names) {
        if (GetLowerFileName(name) != requestedFileName) {
            continue;
        }

        const int score = ScoreArchiveWallpaperPath(requestedPath, name);
        if (score > bestScore) {
            bestScore = score;
            bestPath = name;
        }
    }

    return bestPath;
}

bool QueueFullScreenOverlayQuad(CTexture* texture, int width, int height, float sortKey)
{
    if (!texture || width <= 0 || height <= 0) {
        return false;
    }

    RPFace* face = g_renderer.BorrowNullRP();
    if (!face) {
        return false;
    }

    const float left = OverlayScreenEdge(0);
    const float top = OverlayScreenEdge(0);
    const float right = OverlayScreenEdge(width);
    const float bottom = OverlayScreenEdge(height);
    const unsigned int overlayContentWidth = texture->m_surfaceUpdateWidth > 0 ? texture->m_surfaceUpdateWidth : static_cast<unsigned int>(width);
    const unsigned int overlayContentHeight = texture->m_surfaceUpdateHeight > 0 ? texture->m_surfaceUpdateHeight : static_cast<unsigned int>(height);
    const float maxU = texture->m_w != 0 ? static_cast<float>(overlayContentWidth) / static_cast<float>(texture->m_w) : 1.0f;
    const float maxV = texture->m_h != 0 ? static_cast<float>(overlayContentHeight) / static_cast<float>(texture->m_h) : 1.0f;

    face->primType = D3DPT_TRIANGLESTRIP;
    face->verts = face->m_verts;
    face->numVerts = 4;
    face->indices = nullptr;
    face->numIndices = 0;
    face->tex = texture;
    face->mtPreset = 3;
    face->cullMode = D3DCULL_NONE;
    face->srcAlphaMode = D3DBLEND_SRCALPHA;
    face->destAlphaMode = D3DBLEND_INVSRCALPHA;
    face->alphaSortKey = sortKey;

    face->m_verts[0] = { left, top, 0.0f, 1.0f, 0xFFFFFFFFu, 0xFF000000u, 0.0f, 0.0f };
    face->m_verts[1] = { right, top, 0.0f, 1.0f, 0xFFFFFFFFu, 0xFF000000u, maxU, 0.0f };
    face->m_verts[2] = { left, bottom, 0.0f, 1.0f, 0xFFFFFFFFu, 0xFF000000u, 0.0f, maxV };
    face->m_verts[3] = { right, bottom, 0.0f, 1.0f, 0xFFFFFFFFu, 0xFF000000u, maxU, maxV };
    g_renderer.AddRP(face, 1 | 8);
    return true;
}

bool ResolveCurrentModeCursorState(int* outCursorActNum, u32* outMouseAnimStartTick)
{
    if (!outCursorActNum || !outMouseAnimStartTick) {
        return false;
    }

    if (CLoginMode* loginMode = g_modeMgr.GetCurrentLoginMode()) {
        *outCursorActNum = loginMode->GetCursorAction();
        *outMouseAnimStartTick = loginMode->GetCursorAnimStartTick();
        return true;
    }

    if (CGameMode* gameMode = g_modeMgr.GetCurrentGameMode()) {
        *outCursorActNum = gameMode->GetCursorAction();
        *outMouseAnimStartTick = gameMode->GetCursorAnimStartTick();
        return true;
    }

    return false;
}

bool QueueMenuCursorOverlayQuadForCurrentMode()
{
    int cursorActNum = 0;
    u32 mouseAnimStartTick = 0;
    if (!ResolveCurrentModeCursorState(&cursorActNum, &mouseAnimStartTick) || !g_hMainWnd) {
        return false;
    }

    RECT clientRect{};
    GetClientRect(g_hMainWnd, &clientRect);
    const int clientWidth = clientRect.right - clientRect.left;
    const int clientHeight = clientRect.bottom - clientRect.top;
    if (clientWidth <= 0 || clientHeight <= 0) {
        return false;
    }

    POINT cursorPos{};
    if (!GetModeCursorClientPos(&cursorPos)) {
        return false;
    }

    RECT cursorBounds{};
    const bool hasCustomBounds = GetModeCursorDrawBounds(cursorActNum, mouseAnimStartTick, &cursorBounds);
    const int textureWidth = hasCustomBounds ? (std::max)(1, static_cast<int>(cursorBounds.right - cursorBounds.left)) : 32;
    const int textureHeight = hasCustomBounds ? (std::max)(1, static_cast<int>(cursorBounds.bottom - cursorBounds.top)) : 32;
    const int drawOriginX = hasCustomBounds ? -(std::min)(0, static_cast<int>(cursorBounds.left)) : 0;
    const int drawOriginY = hasCustomBounds ? -(std::min)(0, static_cast<int>(cursorBounds.top)) : 0;
    const int left = hasCustomBounds ? cursorPos.x + (std::min)(0, static_cast<int>(cursorBounds.left)) : cursorPos.x;
    const int top = hasCustomBounds ? cursorPos.y + (std::min)(0, static_cast<int>(cursorBounds.top)) : cursorPos.y;
    static std::vector<unsigned int> s_cursorComposePixels;
    static bool s_cursorTextureValid = false;
    static CTexture* s_cursorTexture = nullptr;
    static int s_cursorTextureWidth = 0;
    static int s_cursorTextureHeight = 0;
    static std::uint64_t s_cursorStateToken = 0ull;
    if (!s_cursorTexture || s_cursorTextureWidth != textureWidth || s_cursorTextureHeight != textureHeight) {
        delete s_cursorTexture;
        s_cursorTexture = new CTexture();
        if (!s_cursorTexture || !s_cursorTexture->Create(textureWidth, textureHeight, PF_A8R8G8B8, false)) {
            delete s_cursorTexture;
            s_cursorTexture = nullptr;
            s_cursorTextureWidth = 0;
            s_cursorTextureHeight = 0;
            return false;
        }
        s_cursorTextureWidth = textureWidth;
        s_cursorTextureHeight = textureHeight;
        s_cursorTextureValid = false;
        s_cursorStateToken = 0ull;
        s_cursorComposePixels.assign(static_cast<size_t>(textureWidth) * static_cast<size_t>(textureHeight), 0u);
    } else if (s_cursorComposePixels.size() != static_cast<size_t>(textureWidth) * static_cast<size_t>(textureHeight)) {
        s_cursorComposePixels.assign(static_cast<size_t>(textureWidth) * static_cast<size_t>(textureHeight), 0u);
    }

    std::uint64_t cursorStateToken = 1469598103934665603ull;
    cursorStateToken ^= static_cast<std::uint64_t>(cursorActNum);
    cursorStateToken *= 1099511628211ull;
    cursorStateToken ^= static_cast<std::uint64_t>(GetModeCursorVisualFrame(cursorActNum, mouseAnimStartTick));
    cursorStateToken *= 1099511628211ull;
    cursorStateToken ^= static_cast<std::uint64_t>(static_cast<unsigned int>(textureWidth));
    cursorStateToken *= 1099511628211ull;
    cursorStateToken ^= static_cast<std::uint64_t>(static_cast<unsigned int>(textureHeight));
    cursorStateToken *= 1099511628211ull;
    if (!s_cursorTextureValid || cursorStateToken != s_cursorStateToken) {
        std::fill(s_cursorComposePixels.begin(), s_cursorComposePixels.end(), 0u);
        if (!DrawModeCursorAtToArgb(
                s_cursorComposePixels.data(),
                textureWidth,
                textureHeight,
                drawOriginX,
                drawOriginY,
                cursorActNum,
                mouseAnimStartTick)) {
            return false;
        }
        s_cursorTexture->Update(0,
            0,
            textureWidth,
            textureHeight,
            s_cursorComposePixels.data(),
            true,
            textureWidth * static_cast<int>(sizeof(unsigned int)));
        s_cursorTextureValid = true;
        s_cursorStateToken = cursorStateToken;
    }

    RPFace* face = g_renderer.BorrowNullRP();
    if (!face) {
        return false;
    }

    const unsigned int overlayContentWidth = s_cursorTexture->m_surfaceUpdateWidth > 0 ? s_cursorTexture->m_surfaceUpdateWidth : static_cast<unsigned int>(textureWidth);
    const unsigned int overlayContentHeight = s_cursorTexture->m_surfaceUpdateHeight > 0 ? s_cursorTexture->m_surfaceUpdateHeight : static_cast<unsigned int>(textureHeight);
    const float maxU = s_cursorTexture->m_w != 0 ? static_cast<float>(overlayContentWidth) / static_cast<float>(s_cursorTexture->m_w) : 1.0f;
    const float maxV = s_cursorTexture->m_h != 0 ? static_cast<float>(overlayContentHeight) / static_cast<float>(s_cursorTexture->m_h) : 1.0f;
    const float quadLeft = static_cast<float>(left);
    const float quadTop = static_cast<float>(top);
    const float quadRight = static_cast<float>(left + textureWidth);
    const float quadBottom = static_cast<float>(top + textureHeight);

    face->primType = D3DPT_TRIANGLESTRIP;
    face->verts = face->m_verts;
    face->numVerts = 4;
    face->indices = nullptr;
    face->numIndices = 0;
    face->tex = s_cursorTexture;
    face->mtPreset = 0;
    face->cullMode = D3DCULL_NONE;
    face->srcAlphaMode = D3DBLEND_SRCALPHA;
    face->destAlphaMode = D3DBLEND_INVSRCALPHA;
    // Keep the cursor above the full-screen Qt menu quad (sort key 2).
    face->alphaSortKey = 3.0f;
    face->m_verts[0] = { quadLeft, quadTop, 0.0f, 1.0f, 0xFFFFFFFFu, 0xFF000000u, 0.0f, 0.0f };
    face->m_verts[1] = { quadRight, quadTop, 0.0f, 1.0f, 0xFFFFFFFFu, 0xFF000000u, maxU, 0.0f };
    face->m_verts[2] = { quadLeft, quadBottom, 0.0f, 1.0f, 0xFFFFFFFFu, 0xFF000000u, 0.0f, maxV };
    face->m_verts[3] = { quadRight, quadBottom, 0.0f, 1.0f, 0xFFFFFFFFu, 0xFF000000u, maxU, maxV };
    g_renderer.AddRP(face, 1 | 8);
    return true;
}

bool CompositeMenuCursorIntoArgbBuffer(unsigned int* pixels, int width, int height)
{
    int cursorActNum = 0;
    u32 mouseAnimStartTick = 0;
    if (!pixels || width <= 0 || height <= 0 || !ResolveCurrentModeCursorState(&cursorActNum, &mouseAnimStartTick)) {
        return false;
    }

    POINT cursorPos{};
    if (!GetModeCursorClientPos(&cursorPos)) {
        return false;
    }

    return DrawModeCursorAtToArgb(pixels, width, height, cursorPos.x, cursorPos.y, cursorActNum, mouseAnimStartTick);
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

std::vector<std::string> BuildWallpaperCandidates(const std::string& requestedWallpaper)
{
    std::vector<std::string> out;

    static const char* kUiKor =
        "texture\\"
        "\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA"
        "\\";

    const char* directDefaults[] = {
        "ad_title.png",
        "ad_title.jpg",
        "rag_title.jpg",
        "title.bmp",
        "title.jpg",
        "login_background.jpg",
        "login_background.bmp",
        nullptr
    };

    std::vector<std::string> baseNames;
    if (!requestedWallpaper.empty()) {
        baseNames.push_back(requestedWallpaper);
    }
    for (int i = 0; directDefaults[i]; ++i) {
        baseNames.push_back(directDefaults[i]);
    }

    const char* pathPrefixes[] = {
        "data\\",
        "data\\texture\\",
        "data\\texture\\interface\\",
        "data\\texture\\interface\\basic_interface\\",
        "data\\texture\\login_interface\\",
        "data\\texture\\\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\",
        "data\\texture\\\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\basic_interface\\",
        "data\\texture\\\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\login_interface\\",
        "",
        "texture\\",
        "texture\\interface\\",
        "texture\\interface\\basic_interface\\",
        "texture\\login_interface\\",
        "ui\\",
        kUiKor,
        "texture\\\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\basic_interface\\",
        "texture\\\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\login_interface\\",
        nullptr
    };

    for (const std::string& baseRaw : baseNames) {
        std::string base = NormalizeSlash(baseRaw);
        if (base.empty()) {
            continue;
        }

        std::string filenameOnly = base;
        const size_t slashPos = filenameOnly.find_last_of('\\');
        if (slashPos != std::string::npos && slashPos + 1 < filenameOnly.size()) {
            filenameOnly = filenameOnly.substr(slashPos + 1);
        }

        const bool hasExtension = filenameOnly.find('.') != std::string::npos;
        std::vector<std::string> nameForms;
        nameForms.push_back(base);
        if (filenameOnly != base) {
            nameForms.push_back(filenameOnly);
        }
        if (!hasExtension) {
            nameForms.push_back(filenameOnly + ".bmp");
            nameForms.push_back(filenameOnly + ".jpg");
            nameForms.push_back(filenameOnly + ".png");
            nameForms.push_back(filenameOnly + ".tga");
        }

        for (const std::string& nameForm : nameForms) {
            for (int i = 0; pathPrefixes[i]; ++i) {
                AddUniqueCandidate(out, std::string(pathPrefixes[i]) + nameForm);
            }
        }
    }

    return out;
}

bool HasDirtyWindowRecursive(UIWindow* window)
{
    if (!window) {
        return false;
    }
    // Hidden windows still need one refresh after SetShow(0),
    // otherwise the composed overlay can keep showing stale pixels.
    if (window->m_isDirty != 0) {
        return true;
    }
    if (window->m_show == 0) {
        return false;
    }
    if (window->IsUpdateNeed()) {
        return true;
    }
    for (UIWindow* child : window->m_children) {
        if (HasDirtyWindowRecursive(child)) {
            return true;
        }
    }
    return false;
}

void ClearDirtyWindowRecursive(UIWindow* window)
{
    if (!window) {
        return;
    }
    window->m_isDirty = 0;
    for (UIWindow* child : window->m_children) {
        ClearDirtyWindowRecursive(child);
    }
}

bool RangesOverlapWithMargin(int a0, int a1, int b0, int b1, int margin)
{
    return std::max(a0, b0) <= std::min(a1, b1) + margin;
}

float NormalizeUiScaleFactor(float scaleFactor)
{
    return scaleFactor > 0.01f ? scaleFactor : 1.0f;
}

void GetUiLogicalClientRectForScale(float scaleFactor, RECT* outRect)
{
    if (!outRect) {
        return;
    }

    RECT clientRect{ 0, 0, 640, 480 };
    if (g_hMainWnd) {
        GetClientRect(g_hMainWnd, &clientRect);
    }

    const float normalizedScale = NormalizeUiScaleFactor(scaleFactor);
    outRect->left = clientRect.left;
    outRect->top = clientRect.top;
    outRect->right = clientRect.left + (std::max)(1, static_cast<int>(std::floor(static_cast<float>(clientRect.right - clientRect.left) / normalizedScale)));
    outRect->bottom = clientRect.top + (std::max)(1, static_cast<int>(std::floor(static_cast<float>(clientRect.bottom - clientRect.top) / normalizedScale)));
}

void ClampWindowToLogicalClientRect(int* x, int* y, int w, int h, float scaleFactor, bool fullyVisible)
{
    if (!x || !y) {
        return;
    }

    RECT logicalClientRect{};
    GetUiLogicalClientRectForScale(scaleFactor, &logicalClientRect);

    const int minX = logicalClientRect.left;
    const int minY = logicalClientRect.top;
    const int clientRight = logicalClientRect.right;
    const int clientBottom = logicalClientRect.bottom;
    const int maxX = std::max(minX, clientRight - std::max(1, w));
    const int visibleHeight = fullyVisible ? std::max(1, h) : std::max(17, std::min(h, 64));
    const int maxY = std::max(minY, clientBottom - visibleHeight);
    *x = std::clamp(*x, minX, maxX);
    *y = std::clamp(*y, minY, maxY);
}

int UiLogicalToRawCoordinate(int logicalValue, float scaleFactor)
{
    return static_cast<int>(std::lround(static_cast<double>(logicalValue) * static_cast<double>(NormalizeUiScaleFactor(scaleFactor))));
}

int UiRawToLogicalCoordinateWithScale(int rawValue, float scaleFactor)
{
    return static_cast<int>(std::lround(static_cast<double>(rawValue) / static_cast<double>(NormalizeUiScaleFactor(scaleFactor))));
}
}

UIWindowMgr g_windowMgr;

UIWindowMgr::UIWindowMgr() 
    : m_chatWndX(0), m_chatWndY(0), m_chatWndHeight(98), m_chatWndShow(1),
      m_gronMsnWndShow(0), m_gronMsgWndShow(0), m_chatWndStatus(7),
      m_miniMapZoomFactor(1.0f), m_miniMapArgb(0), m_isDrawCompass(0),
      m_isDragAll(0), m_conversionMode(0), m_isInvalidatedByForce(false),
    m_captureWindow(nullptr), m_editWindow(nullptr), m_modalWindow(nullptr), m_lastHitWindow(nullptr),
        m_loadingWnd(nullptr), m_roMapWnd(nullptr), m_minimapZoomWnd(nullptr), m_statusWnd(nullptr), m_sayDialogWnd(nullptr), m_npcMenuWnd(nullptr), m_playerContextMenuWnd(nullptr), m_joinPartyAcceptWnd(nullptr), m_messengerGroupWnd(nullptr), m_partyOptionWnd(nullptr), m_npcInputWnd(nullptr), m_chooseSellBuyWnd(nullptr), m_itemShopWnd(nullptr), m_itemPurchaseWnd(nullptr), m_itemSellWnd(nullptr), m_storageWnd(nullptr), m_shortCutWnd(nullptr), m_chatWnd(nullptr),
            m_loginWnd(nullptr), m_selectServerWnd(nullptr), m_selectCharWnd(nullptr), m_makeCharWnd(nullptr), m_waitWnd(nullptr), m_chooseWnd(nullptr), m_selectCartWnd(nullptr), m_optionWnd(nullptr), m_itemWnd(nullptr), m_itemInfoWnd(nullptr), m_itemCollectionWnd(nullptr), m_itemCompositionWnd(nullptr), m_itemIdentifyWnd(nullptr), m_skillSelectWnd(nullptr), m_questWnd(nullptr), m_basicInfoWnd(nullptr), m_notifyLevelUpWnd(nullptr), m_notifyJobLevelUpWnd(nullptr), m_equipWnd(nullptr), m_skillDescribeWnd(nullptr), m_skillListWnd(nullptr),
                        m_wallpaperSurface(nullptr), m_uiComposeSurface(), m_chatActiveInputField(0), m_chatScrollLineOffset(0)
{
    m_loginStatus = "Login: idle";
    LoadShortcutKeyBindings();
}

void UIWindowMgr::LoadShortcutKeyBindings()
{
    for (int slot = 0; slot < kShortcutKeyBindingCount; ++slot) {
        const int column = slot % 9;
        ShortcutKeyBinding defaults{};
        if (slot < 9) {
            defaults.virtualKey = VK_F1 + column;
        } else {
            defaults.virtualKey = '1' + column;
            defaults.modifiers = slot < 18 ? kShortcutKeyModifierControl : kShortcutKeyModifierShift;
        }

        const std::string keyName = "Slot" + std::to_string(slot) + "Key";
        const std::string modifiersName = "Slot" + std::to_string(slot) + "Modifiers";
        ShortcutKeyBinding binding{};
        binding.virtualKey = LoadSettingsIniInt("ShortcutKeys", keyName.c_str(), defaults.virtualKey);
        binding.modifiers = static_cast<unsigned char>(LoadSettingsIniInt(
            "ShortcutKeys", modifiersName.c_str(), defaults.modifiers) & 0x07);
        if (binding.virtualKey <= 0 || binding.virtualKey > 0xFF) {
            binding = defaults;
        }
        m_shortcutKeyBindings[static_cast<size_t>(slot)] = binding;
    }
}

ShortcutKeyBinding UIWindowMgr::GetShortcutKeyBinding(int slotIndex) const
{
    if (slotIndex < 0 || slotIndex >= kShortcutKeyBindingCount) {
        return {};
    }
    return m_shortcutKeyBindings[static_cast<size_t>(slotIndex)];
}

bool UIWindowMgr::SetShortcutKeyBinding(int slotIndex, const ShortcutKeyBinding& binding)
{
    if (slotIndex < 0 || slotIndex >= kShortcutKeyBindingCount
        || binding.virtualKey <= 0 || binding.virtualKey > 0xFF) {
        return false;
    }
    const ShortcutKeyBinding normalized{
        binding.virtualKey,
        static_cast<unsigned char>(binding.modifiers & 0x07)
    };
    if (m_shortcutKeyBindings[static_cast<size_t>(slotIndex)].virtualKey == normalized.virtualKey
        && m_shortcutKeyBindings[static_cast<size_t>(slotIndex)].modifiers == normalized.modifiers) {
        return false;
    }
    m_shortcutKeyBindings[static_cast<size_t>(slotIndex)] = normalized;
    const std::string keyName = "Slot" + std::to_string(slotIndex) + "Key";
    const std::string modifiersName = "Slot" + std::to_string(slotIndex) + "Modifiers";
    SaveSettingsIniInt("ShortcutKeys", keyName.c_str(), normalized.virtualKey);
    SaveSettingsIniInt("ShortcutKeys", modifiersName.c_str(), normalized.modifiers);
    return true;
}

std::string UIWindowMgr::GetShortcutKeyBindingLabel(int slotIndex) const
{
    return ShortcutBindingLabel(GetShortcutKeyBinding(slotIndex));
}

bool UIWindowMgr::IsCapturingShortcutKeyBinding() const
{
    return m_optionWnd && m_optionWnd->m_show != 0 && m_optionWnd->IsCapturingShortcutKeyBinding();
}

UIWindowMgr::~UIWindowMgr() {
    Reset();
}

void UIWindowMgr::ClampWindowToClient(int* x, int* y, int w, int h) const
{
    ClampWindowToLogicalClientRect(
        x,
        y,
        w,
        h,
        IsQtUiRuntimeEnabled() ? GetConfiguredUiScaleFactor() : 1.0f,
        IsQtUiRuntimeEnabled());
}

int UIWindowMgr::GetInventoryTabSelection()
{
    InitializeInventoryWindowInfo();
    return m_inventoryWindowInfo.tab;
}

void UIWindowMgr::InitializeInventoryWindowInfo()
{
    if (m_inventoryWindowInfoInitialized) return;
    m_inventoryWindowInfoInitialized = true;
    // Ref reads the UI registry record once, then overwrites these inventory
    // fields. Later window recreation consumes the in-memory record instead.
    if (!HasSettingsIniSection("UIWindows")) return;
    // These records are read together at process initialization in Ref. A
    // missing value in an existing key leaves the global record zero-filled.
    const auto detailField=[](const char* key) {
        std::string raw;
        if (!TryLoadSettingsIniString("UIWindows",key,&raw)) return 0;
        int value=0;
        return TryLoadSettingsIniInt("UIWindows",key,&value) ? value : -1;
    };
    m_itemDetailWindowInfo = {
        detailField("ItemInfoWndX"),detailField("ItemInfoWndY"),
        detailField("ItemInfoWndWidth"),detailField("ItemInfoWndHeight")};
    m_cardDetailWindowInfo = {
        detailField("CardInfoWndX"),detailField("CardInfoWndY"),
        detailField("CardInfoWndWidth"),detailField("CardInfoWndHeight")};
    m_inventoryWindowInfo = {744,514,280,134,134,1,3,3,4};
    const auto radio = [](const char* key, int fallback) {
        std::string raw;
        if (!TryLoadSettingsIniString("UIWindows", key, &raw)) return fallback;
        int value = 0;
        return TryLoadSettingsIniInt("UIWindows", key, &value) ? value : -1;
    };
    m_inventoryWindowInfo.radio1 = radio("ItemWndRadio1",3);
    m_inventoryWindowInfo.radio2 = radio("ItemWndRadio2",3);
    m_inventoryWindowInfo.radio3 = radio("ItemWndRadio3",4);
    ValidateInventoryWindowInfo();
}

void UIWindowMgr::ValidateInventoryWindowInfo()
{
    RECT client{};
    GetUiLogicalClientRectForScale(IsQtUiRuntimeEnabled() ? GetConfiguredUiScaleFactor() : 1.0f, &client);
    const auto& info = m_inventoryWindowInfo;
    // Use wide subtraction for externally seeded or corrupt dimensions. The
    // source checks stored height (including17), not the expanded height.
    const bool valid = info.x >= 0 && int64_t(info.x) <= int64_t(client.right-client.left)-info.width
        && info.y >= 0 && int64_t(info.y) <= int64_t(client.bottom-client.top)-info.height
        && info.tab >= 0 && info.tab <= 2
        && info.radio1 >= 0 && info.radio1 < 4 && info.radio2 >= 0 && info.radio2 < 4
        && info.radio3 >= 0 && info.radio3 <= 4;
    if (!valid) {
        m_inventoryWindowInfo = InventoryWindowInfo{};
        m_inventoryWindowShown = false;
    }
}

void UIWindowMgr::SetInventoryTabSelection(int tab)
{
    InitializeInventoryWindowInfo();
    m_inventoryWindowInfo.tab = std::clamp(tab, 0, 2);
}

InventoryWindowInfo UIWindowMgr::GetInventoryWindowInfo()
{
    InitializeInventoryWindowInfo();
    // Message34 validates the complete record on each new window attachment.
    ValidateInventoryWindowInfo();
    return m_inventoryWindowInfo;
}

void UIWindowMgr::StoreInventoryWindowInfo(const InventoryWindowInfo& info)
{
    m_inventoryWindowInfoInitialized = true;
    m_inventoryWindowInfo = info;
    SaveSettingsIniInt("UIWindows","ItemWndX",info.x);
    SaveSettingsIniInt("UIWindows","ItemWndY",info.y);
    SaveSettingsIniInt("UIWindows","ItemWndWidth",info.width);
    SaveSettingsIniInt("UIWindows","ItemWndHeight",info.height);
    SaveSettingsIniInt("UIWindows","ItemWndOrgHeight",info.originalHeight);
    SaveSettingsIniInt("UIWindows","ItemWndTab",info.tab);
    SaveSettingsIniInt("UIWindows","ItemWndRadio1",info.radio1);
    SaveSettingsIniInt("UIWindows","ItemWndRadio2",info.radio2);
    SaveSettingsIniInt("UIWindows","ItemWndRadio3",info.radio3);
}

void UIWindowMgr::CloseInventoryWindow()
{
    if (!m_itemWnd) return;
    if (ContainsManagedWindow(*this,m_itemWnd)) DeleteWindow(m_itemWnd);
    else {
        m_itemWnd->SetShow(0);
        SetInventoryWindowShowState(false);
    }
}

bool UIWindowMgr::GetInventoryWindowShowState()
{
    InitializeInventoryWindowInfo();
    return m_inventoryWindowShown;
}

void UIWindowMgr::SetInventoryWindowShowState(bool shown)
{
    InitializeInventoryWindowInfo();
    m_inventoryWindowShown = shown;
    SaveSettingsIniInt("UIWindows","ItemWndShow",shown ? 1 : 0);
}

ItemDetailWindowInfo UIWindowMgr::GetItemDetailWindowInfo(ItemDetailKind kind)
{
    InitializeInventoryWindowInfo();
    return kind==ItemDetailKind::SlottedCard ? m_cardDetailWindowInfo : m_itemDetailWindowInfo;
}

void UIWindowMgr::RestoreItemDetailWindowPlacement(int* x, int* y, ItemDetailKind kind)
{
    InitializeInventoryWindowInfo();
    RECT client{};
    GetUiLogicalClientRectForScale(IsQtUiRuntimeEnabled() ? GetConfiguredUiScaleFactor() : 1.0f,&client);
    const int width = client.right-client.left;
    const int height = client.bottom-client.top;
    auto& info = kind==ItemDetailKind::SlottedCard ? m_cardDetailWindowInfo : m_itemDetailWindowInfo;
    if (info.width == 280 && info.height == 120 && info.x >= 0 && info.y >= 0
        && int64_t(info.x) <= int64_t(width)-280 && int64_t(info.y) <= int64_t(height)-120) {
        *x = info.x; *y = info.y;
        return;
    }
    info = ItemDetailWindowInfo{280,kind==ItemDetailKind::SlottedCard ? 278 : 126,280,120};
    // The source deliberately uses renderer WIDTH for the default Y formula.
    *x = 280+(width-640)/2;
    *y = static_cast<int>(int64_t(width)*info.y/640);
}

void UIWindowMgr::StoreItemDetailWindowInfo(const ItemDetailWindowInfo& info, ItemDetailKind kind)
{
    InitializeInventoryWindowInfo();
    (kind==ItemDetailKind::SlottedCard ? m_cardDetailWindowInfo : m_itemDetailWindowInfo) = info;
    const std::string prefix=kind==ItemDetailKind::SlottedCard ? "CardInfoWnd" : "ItemInfoWnd";
    SaveSettingsIniInt("UIWindows",(prefix+"X").c_str(),info.x);
    SaveSettingsIniInt("UIWindows",(prefix+"Y").c_str(),info.y);
    SaveSettingsIniInt("UIWindows",(prefix+"Width").c_str(),info.width);
    SaveSettingsIniInt("UIWindows",(prefix+"Height").c_str(),info.height);
}

void UIWindowMgr::RepositionManagedWindowsForUiScale(int previousPercent, int nextPercent)
{
    const int clampedPreviousPercent = ClampUiScalePercent(previousPercent);
    const int clampedNextPercent = ClampUiScalePercent(nextPercent);
    if (clampedPreviousPercent == clampedNextPercent) {
        return;
    }

    const float previousScale = static_cast<float>(clampedPreviousPercent) / 100.0f;
    const float nextScale = static_cast<float>(clampedNextPercent) / 100.0f;
    for (UIWindow* window : m_children) {
        if (!window || window->m_show == 0 || window->m_parent != nullptr || !window->IsFrameWnd()) {
            continue;
        }

        int nextX = UiRawToLogicalCoordinateWithScale(UiLogicalToRawCoordinate(window->m_x, previousScale), nextScale);
        int nextY = UiRawToLogicalCoordinateWithScale(UiLogicalToRawCoordinate(window->m_y, previousScale), nextScale);
        ClampWindowToLogicalClientRect(&nextX, &nextY, window->m_w, window->m_h, nextScale, true);
        window->Move(nextX, nextY);
        window->StoreInfo();
    }
}

void UIWindowMgr::SnapWindowToNearby(UIWindow* window, int* x, int* y) const
{
    if (!window || !x || !y) {
        return;
    }

    int snappedX = *x;
    int snappedY = *y;
    int bestXDistance = kWindowSnapDistance + 1;
    int bestYDistance = kWindowSnapDistance + 1;

    RECT clientRect{};
    GetUiLogicalClientRectForScale(
        IsQtUiRuntimeEnabled() ? GetConfiguredUiScaleFactor() : 1.0f,
        &clientRect);

    const auto considerX = [&](int candidateX) {
        const int distance = std::abs(*x - candidateX);
        if (distance < bestXDistance && distance <= kWindowSnapDistance) {
            bestXDistance = distance;
            snappedX = candidateX;
        }
    };
    const auto considerY = [&](int candidateY) {
        const int distance = std::abs(*y - candidateY);
        if (distance < bestYDistance && distance <= kWindowSnapDistance) {
            bestYDistance = distance;
            snappedY = candidateY;
        }
    };

    considerX(clientRect.left);
    considerX(clientRect.right - window->m_w);
    considerY(clientRect.top);
    considerY(clientRect.bottom - window->m_h);

    const int proposedLeft = *x;
    const int proposedRight = *x + window->m_w;
    const int proposedTop = *y;
    const int proposedBottom = *y + window->m_h;

    for (UIWindow* other : m_children) {
        if (!other || other == window || other->m_show == 0 || other->m_parent != nullptr || !other->IsFrameWnd()) {
            continue;
        }

        const int otherLeft = other->m_x;
        const int otherRight = other->m_x + other->m_w;
        const int otherTop = other->m_y;
        const int otherBottom = other->m_y + other->m_h;

        if (RangesOverlapWithMargin(proposedTop, proposedBottom, otherTop, otherBottom, kWindowSnapDistance)) {
            considerX(otherLeft - window->m_w);
            considerX(otherRight);
            considerX(otherLeft);
            considerX(otherRight - window->m_w);
        }

        if (RangesOverlapWithMargin(proposedLeft, proposedRight, otherLeft, otherRight, kWindowSnapDistance)) {
            considerY(otherTop - window->m_h);
            considerY(otherBottom);
            considerY(otherTop);
            considerY(otherBottom - window->m_h);
        }
    }

    *x = snappedX;
    *y = snappedY;
    ClampWindowToClient(x, y, window->m_w, window->m_h);
}

bool UIWindowMgr::IsChatAuxiliaryAtPoint(int x, int y) const
{
    const auto* hit = HitTestWindow(x, y);
    return hit && (hit == m_subChatWndST || hit == m_subChatWndBT || hit == m_subChatMiniWnd || hit == m_battleMsgOptionWnd || hit == m_chatRoomWnd || hit == m_chatRoomPasswordWnd || hit==m_chatRoomMakeWnd || hit==m_chatRoomChangeWnd || hit==m_roomMessageBox || hit==m_chatRoomMenuWnd || hit==m_autoMessageWnd || IsWhisperPreferenceInputWindow(hit) || dynamic_cast<const UIWhisperWnd*>(hit));
}
bool UIWindowMgr::IsWhisperPreferenceInputWindow(const UIWindow* window) const
{
    if(!window)return false;
    for (const auto* owner = window; owner; owner = owner->m_parent)
        if (owner == m_homunInfoWnd || owner == m_merInfoWnd
            || owner == m_homunSkillListWnd || owner == m_merSkillListWnd) return true;
    if(window->m_parent==m_messengerGroupWnd && window->m_id==168)return true;
    if(dynamic_cast<const UIPetCommandMenu*>(window))return true;
    for(const auto* owner=window;owner;owner=owner->m_parent)if(dynamic_cast<const UIGroundSkillTextWnd*>(owner) || owner==m_friendOptionWnd || owner==m_tipOfTheDayWnd || owner==m_commandListWnd || owner==m_petInfoWnd || owner==m_petEggListWnd || owner==m_petTamingDeceiveWnd)return true;
    return false;
}

RECT UIWindowMgr::GetChatClientRect() const
{
    RECT client{0,0,WINDOW_WIDTH>0?WINDOW_WIDTH:640,WINDOW_HEIGHT>0?WINDOW_HEIGHT:480};
    if(g_hMainWnd)GetClientRect(g_hMainWnd,&client);
    const float scale=IsQtUiRuntimeEnabled()?GetConfiguredUiScaleFactor():1.0f;
    const float normalized=scale>0?scale:1.0f;
    int width=(std::max)(1,static_cast<int>(std::floor(static_cast<float>(client.right-client.left)/normalized)));
    int height=(std::max)(1,static_cast<int>(std::floor(static_cast<float>(client.bottom-client.top)/normalized)));
    if(WINDOW_WIDTH>0)width=(std::min)(width,WINDOW_WIDTH);
    if(WINDOW_HEIGHT>0)height=(std::min)(height,WINDOW_HEIGHT);
    return {0,0,width,height};
}

void UIWindowMgr::LoadChatWindowPreferences()
{
    m_friendOptionWindowInfo={LoadSettingsIniInt("UIWindows","FriendOptionWndX",200),LoadSettingsIniInt("UIWindows","FriendOptionWndY",120),
        LoadSettingsIniInt("UIWindows","FriendOptionWndW",280),LoadSettingsIniInt("UIWindows","FriendOptionWndH",120)};
    m_whisperWindowInfo={LoadSettingsIniInt("UIWindows","WhisperWndX",0),LoadSettingsIniInt("UIWindows","WhisperWndY",347),
        LoadSettingsIniInt("UIWindows","WhisperWndW",280),LoadSettingsIniInt("UIWindows","WhisperWndH",120)};
    // Selected source reads height/placement/show but never reads the status
    // value it writes. Status therefore keeps the manager's session default7.
    m_chatWndHeight=LoadSettingsIniInt("UIWindows","ChatWndHeight",98);
    // Avoid malformed dimensions causing oversized render allocations; normal
    // restored heights are not quantized into SetState before placement.
    if(m_chatWndHeight<1 || m_chatWndHeight>8192)m_chatWndHeight=98;
    m_chatWndX=LoadSettingsIniInt("UIWindows","ChatWndX",0);
    m_chatWndY=LoadSettingsIniInt("UIWindows","ChatWndY",static_cast<int>(GetChatClientRect().bottom)-98);
    m_chatWndShow=LoadSettingsIniInt("UIWindows","ChatWndShow",1);
    m_chatWndStickOn=LoadSettingsIniInt("UIWindows","ChatWndStickOn",1)!=0;
    m_onStSubChat=LoadSettingsIniInt("UIWindows","OnStSubChat",0)!=0;
    m_onBtSubChat=LoadSettingsIniInt("UIWindows","OnBtSubChat",0)!=0;
    m_chatRoomWindowInfo={LoadSettingsIniInt("UIWindows","ChatRoomWndX",500),
        LoadSettingsIniInt("UIWindows","ChatRoomWndY",180),
        LoadSettingsIniInt("UIWindows","ChatRoomWndW",280),
        LoadSettingsIniInt("UIWindows","ChatRoomWndH",120)};
    for(int kind=0;kind<2;++kind){
        const std::string prefix=kind==0?"SubChatWndSt":"SubChatWndBt";
        auto& info=m_subChatInfo[kind];
        info.x=LoadSettingsIniInt("UIWindows",(prefix+"X").c_str(),0);
        info.y=LoadSettingsIniInt("UIWindows",(prefix+"Y").c_str(),0);
        info.width=LoadSettingsIniInt("UIWindows",(prefix+"W").c_str(),0);
        info.height=LoadSettingsIniInt("UIWindows",(prefix+"H").c_str(),0);
    }
}

void UIWindowMgr::SaveChatWindowPreferences() const
{
    SaveSettingsIniInt("UIWindows","FriendOptionWndX",m_friendOptionWindowInfo.x);SaveSettingsIniInt("UIWindows","FriendOptionWndY",m_friendOptionWindowInfo.y);
    SaveSettingsIniInt("UIWindows","FriendOptionWndW",m_friendOptionWindowInfo.width);SaveSettingsIniInt("UIWindows","FriendOptionWndH",m_friendOptionWindowInfo.height);
    SaveSettingsIniInt("UIWindows","WhisperWndX",m_whisperWindowInfo.x);SaveSettingsIniInt("UIWindows","WhisperWndY",m_whisperWindowInfo.y);
    SaveSettingsIniInt("UIWindows","WhisperWndW",m_whisperWindowInfo.width);SaveSettingsIniInt("UIWindows","WhisperWndH",m_whisperWindowInfo.height);
    // Shutdown calls this after RemoveAllWindows has snapshotted every owner.
    // Hidden windows, checkbox edits and127 toggles do not write this record.
    SaveSettingsIniInt("UIWindows","ChatWndStatus",m_chatWndStatus);
    SaveSettingsIniInt("UIWindows","ChatWndHeight",m_chatWndHeight);
    SaveSettingsIniInt("UIWindows","ChatWndX",m_chatWndX);
    SaveSettingsIniInt("UIWindows","ChatWndY",m_chatWndY);
    SaveSettingsIniInt("UIWindows","ChatWndShow",m_chatWndShow);
    SaveSettingsIniInt("UIWindows","ChatWndStickOn",m_chatWndStickOn);
    SaveSettingsIniInt("UIWindows","OnStSubChat",m_onStSubChat);
    SaveSettingsIniInt("UIWindows","OnBtSubChat",m_onBtSubChat);
    SaveSettingsIniInt("UIWindows","ChatRoomWndX",m_chatRoomWindowInfo.x);
    SaveSettingsIniInt("UIWindows","ChatRoomWndY",m_chatRoomWindowInfo.y);
    SaveSettingsIniInt("UIWindows","ChatRoomWndW",m_chatRoomWindowInfo.width);
    SaveSettingsIniInt("UIWindows","ChatRoomWndH",m_chatRoomWindowInfo.height);
    for(int kind=0;kind<2;++kind){
        const std::string prefix=kind==0?"SubChatWndSt":"SubChatWndBt";
        const auto& info=m_subChatInfo[kind];
        SaveSettingsIniInt("UIWindows",(prefix+"X").c_str(),info.x);
        SaveSettingsIniInt("UIWindows",(prefix+"Y").c_str(),info.y);
        SaveSettingsIniInt("UIWindows",(prefix+"W").c_str(),info.width);
        SaveSettingsIniInt("UIWindows",(prefix+"H").c_str(),info.height);
    }
}

void UIWindowMgr::InitializeChatAuxiliaries()
{
    // Ref main OnCreate: deleting an existing auxiliary must NOT recreate it.
    if (m_subChatWndST) DeleteWindow(m_subChatWndST); else MakeWindow(WID_SUBCHATST);
    if (m_subChatWndBT) DeleteWindow(m_subChatWndBT); else MakeWindow(WID_SUBCHATBT);
    if (m_subChatMiniWnd) DeleteWindow(m_subChatMiniWnd); else MakeWindow(WID_SUBCHATMINI);
    UpdateChatAuxiliaryVisibility();
}

void UIWindowMgr::UpdateChatAuxiliaryVisibility()
{
    if (!m_subChatMiniWnd) return;
    m_subChatMiniWnd->SetShow(m_chatWnd && m_chatWnd->m_show && !m_chatWndStickOn);
    if (m_chatWnd) m_subChatMiniWnd->SendMsg(m_chatWnd, 6, 131,
        m_chatWnd->m_x + m_chatWnd->m_w, m_chatWnd->m_y + m_chatWnd->m_h);
    m_subChatMiniWnd->UpdateHistoryVisibility();
}

void UIWindowMgr::ShowChatWindow(bool show)
{
    EnsureChatWindowVisible();
    m_chatWndShow=show;
    m_chatWnd->SetShow(show);
    if(!show)m_chatWnd->StoreInfo();
}

void UIWindowMgr::EnsureChatWindowVisible()
{
    if (!m_chatWnd) {
        m_chatWnd = new UINewChatWnd();
        ++m_sourceChatGeneration;
        InitializeChatAuxiliaries();
        m_children.push_back(m_chatWnd);
        m_chatWnd->RestorePersistentState(m_chatInputHistory,
            m_chatWhisperTargetText,
            m_chatInputText,
            m_chatActiveInputField,
            m_chatScrollLineOffset);
    }

    m_children.remove(m_chatWnd);
    m_children.push_back(m_chatWnd);
    m_chatWndShow=1;
    m_chatWnd->SetShow(1);
}

bool UIWindowMgr::HasActiveNpcDialog() const
{
    return (m_sayDialogWnd && m_sayDialogWnd->m_show != 0)
        || (m_npcMenuWnd && m_npcMenuWnd->m_show != 0)
        || (m_npcInputWnd && m_npcInputWnd->m_show != 0);
}

bool UIWindowMgr::CanRouteQtChatInput() const
{
    if (GetFocusedGroundSkillText()) return false;
    if(m_autoMessageWnd && m_autoMessageWnd->m_show)return false;
    if(auto* whisper=dynamic_cast<UIWhisperWnd*>(m_editWindow))if(whisper->m_show)return false;
    if(m_modalWindow && m_modalWindow->m_show)return false;
    if((m_chatRoomMakeWnd && m_editWindow==m_chatRoomMakeWnd) || (m_chatRoomChangeWnd && m_editWindow==m_chatRoomChangeWnd))return false;
    if(HasFrontMenuUiVisible() || HasActiveNpcDialog() || IsCapturingShortcutKeyBinding())return false;
    if(m_chatRoomPasswordWnd && m_chatRoomPasswordWnd->m_show && m_editWindow==m_chatRoomPasswordWnd)return false;
    if(m_chatRoomWnd && m_chatRoomWnd->m_show && m_editWindow==m_chatRoomWnd)return false;
    // These dialogs already take keyboard priority over chat in OnKeyDown.
    const UIWindow* dialogs[]={m_joinPartyAcceptWnd,m_skillSelectWnd,m_playerContextMenuWnd,
        m_itemPurchaseWnd,m_itemSellWnd,m_itemShopWnd,m_storageWnd,m_chooseSellBuyWnd,m_loadingWnd,m_waitWnd};
    for(const auto* dialog:dialogs)if(dialog && dialog->m_show)return false;
    return true;
}

void UIWindowMgr::CloseNpcDialogWindows()
{
    if (m_sayDialogWnd) {
        m_sayDialogWnd->HideConversation();
    }
    if (m_npcMenuWnd) {
        m_npcMenuWnd->HideMenu();
    }
    if (m_npcInputWnd) {
        m_npcInputWnd->HideInput();
    }
}

void UIWindowMgr::ClosePlayerContextMenu()
{
    if (m_playerContextMenuWnd) {
        m_playerContextMenuWnd->HideMenu();
    }
}

void UIWindowMgr::CloseNpcShopWindows()
{
    CloseNpcShopQuantityPrompt();
    if (m_chooseSellBuyWnd) {
        m_chooseSellBuyWnd->SetShow(0);
        m_chooseSellBuyWnd->StoreInfo();
    }
    if (m_itemShopWnd) {
        m_itemShopWnd->SetShow(0);
        m_itemShopWnd->StoreInfo();
    }
    if (m_itemPurchaseWnd) {
        m_itemPurchaseWnd->SetShow(0);
        m_itemPurchaseWnd->StoreInfo();
    }
    if (m_itemSellWnd) {
        m_itemSellWnd->SetShow(0);
        m_itemSellWnd->StoreInfo();
    }
}

void UIWindowMgr::CloseNpcShopQuantityPrompt()
{
    if (m_npcInputWnd && (m_npcInputWnd->IsGameNumberPromptFor(CGameMode::GameMsg_RequestShopAdjustDeal)
        || m_npcInputWnd->IsGameNumberPromptFor(CGameMode::GameMsg_ConfirmCashShopContribution)))
        m_npcInputWnd->HideInput();
    if (m_itemPurchaseWnd && m_roomMessageBox && m_roomMessageBox->GetSourceOwner() == m_itemPurchaseWnd) {
        m_roomMessageBox->Dismiss(false);
    }
}

void UIWindowMgr::CloseStorageWindows()
{
    if (m_npcInputWnd && (m_npcInputWnd->IsGameNumberPromptFor(CGameMode::GameMsg_RequestStorageStoreItem)
        || m_npcInputWnd->IsGameNumberPromptFor(CGameMode::GameMsg_RequestStorageWithdrawItem))) {
        m_npcInputWnd->HideInput();
    }
    if (m_storageWnd) {
        m_storageWnd->SetShow(0);
        m_storageWnd->StoreInfo();
    }
}

void UIWindowMgr::ReleaseComposeSurface()
{
    m_uiComposeSurface.Release();
}

bool UIWindowMgr::EnsureComposeSurface(int width, int height)
{
    return m_uiComposeSurface.EnsureSize(width, height);
}

bool UIWindowMgr::Init() {
    if (!m_loginWnd) {
        m_loginWnd = new UILoginWnd();
        if (!m_loginWallpaper.empty()) {
            m_loginWnd->SetWallpaperName(m_loginWallpaper);
        }
        m_children.push_back(m_loginWnd);
    }

    if (!m_chatWnd) {
        m_chatWnd = new UINewChatWnd();
        ++m_sourceChatGeneration;
        InitializeChatAuxiliaries();
        m_children.push_back(m_chatWnd);
        m_chatWnd->SetShow(m_chatWndShow);
    }
    return true;
}

void UIWindowMgr::ShowRoomValidationMessage(int messageId)
{
    if(m_modalWindow)return;
    if(m_autoMessageWnd)m_autoMessageWnd->CancelPointerCapture();
    if(m_friendOptionWnd)m_friendOptionWnd->CancelPointerCapture();
    if(auto* tip=dynamic_cast<UITipOfTheDayWnd*>(m_tipOfTheDayWnd))tip->CancelPointerCapture();
    if(m_messengerGroupWnd)m_messengerGroupWnd->CancelSourcePointerCapture();
    for(const auto& entry:m_whisperWindows)entry.second->CancelPointerCapture();
    ro::UiMessageStrings table;table.Load();
    const char* fallback=messageId==13?"Please enter a room title.":messageId==14?"That title cannot be used.":messageId==15?"Please enter a password."
        :messageId==16?"Please enter at least four password characters.":messageId==190?"Only English text is allowed."
        :messageId==191?"Not supported yet.":messageId==661?"You cannot create a room while playing dead.":"You cannot create a chat room while a shop is open.";
    if(auto* settings=dynamic_cast<UIChatRoomSettingsWnd*>(m_captureWindow))settings->CancelPointerCapture();
    if(m_chatWnd)m_chatWnd->CancelPointerCapture();
    if(g_hMainWnd && GetCapture()==g_hMainWnd)ReleaseCapture();
    m_roomMessageBox=new UIChatRoomMessageBox(table.Get(messageId,fallback),messageId);m_children.push_back(m_roomMessageBox);
    m_captureWindow=nullptr;m_editWindow=nullptr;m_modalWindow=m_roomMessageBox;
}

void UIWindowMgr::AddWhisperCharName(const std::string& name)
{
    if(m_chatWnd)m_chatWnd->RememberWhisperTarget(name);
}
bool UIWindowMgr::WriteWhisperChatToFile(const std::string& name)
{
    const auto found=m_whisperWindows.find(name);
    return found!=m_whisperWindows.end() && found->second->SendMsg(nullptr,48,0,0,0)!=0;
}
bool UIWindowMgr::AddFriendCharName(const std::string& input)
{
    const std::string name(input.c_str());if(name.size()>24)return false;
    std::array<char,26> packet{};packet[0]=2;packet[1]=2;std::copy(name.begin(),name.end(),packet.begin()+2);
    return CRagConnection::instance()->SendPacket(packet.data(),static_cast<int>(packet.size()));
}
bool UIWindowMgr::OpenWhisperWindow(const std::string& input,const char* label)
{
    const std::string name(input.c_str());
    if(m_whisperWindows.find(name)!=m_whisperWindows.end())return false;
    const std::string title=label?name+"  ("+label+")  *^_^*":name;
    auto* window=new UIWhisperWnd(name,title);
    m_children.insert(std::find(m_children.begin(),m_children.end(),m_modalWindow),window);
    const auto& info=m_whisperWindowInfo;const auto client=GetChatClientRect();
    const long long offset=17LL*static_cast<long long>(m_whisperWindows.size());
    if(offset>0){
        const long long x=info.x+offset,y=info.y+offset;
        if(x>0 && x+info.width<client.right-17 && y>0 && y+info.height<client.bottom-17)window->Move(static_cast<int>(x),static_cast<int>(y));
        else if(offset+info.width<client.right-17 && offset+info.height<client.bottom-17)window->Move(static_cast<int>(offset),static_cast<int>(offset));
    }
    m_whisperWindows.emplace(name,window);AddWhisperCharName(name);
    if(!m_modalWindow){m_editWindow=window;if(m_chatWnd)m_chatWnd->ClearInputFocus();}
    return true;
}
bool UIWindowMgr::AddWhisperChatToWhisperWnd(const std::string& input,const std::string& text,u32 colorRef)
{
    const std::string name(input.c_str());
    const bool friendName=g_session.IsFriendName(name.c_str());bool created=false;
    if(g_session.m_isShowWhisperWnd && !friendName){
        if(g_session.IsGuildMember(name.c_str()))created=OpenWhisperWindow(name,"GuildMember");
        else {
            int warning=g_session.IsCheatName(name.c_str())?917:g_session.IsCheatGuildName(name.c_str())?919:0;
            if(warning){ro::UiMessageStrings table;table.Load();const auto message=table.Get(warning,"Check the sender's identity.");
                SendMsg(1,reinterpret_cast<msgparam_t>(message.c_str()),65535);}
            created=OpenWhisperWindow(name);
        }
    }else if(g_session.m_isShowWhisperWnd_Friend && friendName){
        ro::UiMessageStrings table;table.Load();const auto label=table.Get(102,"Friend");created=OpenWhisperWindow(name,label.c_str());
    }
    if(created){
        const char* primary="..\\..\\BGM\\Msg_NewMsg.wav";
        if(g_resMgr.IsExist(primary))PlayWave(primary,0,0,0,250,40,1.0f);
        else if(g_session.m_isPlayWhisperOpenSound)PlayWave("\xB9\xF6\xC6\xB0\xBC\xD2\xB8\xAE.wav",0,0,0,250,40,1.0f);
    }
    if(name==g_session.GetPlayerName())return false;
    if(g_session.m_isShowWhisperWnd && created)AddWhisperCharName(name);
    const auto found=m_whisperWindows.find(name);if(found==m_whisperWindows.end())return false;
    auto* window=found->second;window->Append(text.c_str(),ro::chat::ColorRefToUiRgb(colorRef));
    // Original does not unhide an existing window; absent the map overlay it
    // brings even that retained owner to the end of the managed draw order.
    if(!m_roMapWnd){m_children.remove(window);m_children.insert(std::find(m_children.begin(),m_children.end(),m_modalWindow),window);}
    return true;
}

bool UIWindowMgr::OpenChatRoomMake(bool toggle)
{
    if(toggle && m_chatRoomMakeWnd){DeleteWindow(m_chatRoomMakeWnd);return false;}
    if(ro::commerce::state.vending.visible || ro::commerce::state.vendingActive){ShowRoomValidationMessage(587);return false;}
    return MakeWindow(WID_CHATROOMMAKE)!=nullptr;
}

UIWindow* UIWindowMgr::MakeWindow(int windowId)
{
    switch (windowId) {
    case WID_TALKBOXTRAPINPUTWND:case WID_GRAFFISTRBOXWND:{
        auto*& window=m_groundSkillTextWindows[windowId==WID_TALKBOXTRAPINPUTWND?0:1];
        if(!window){window=new UIGroundSkillTextWnd(windowId);m_children.insert(std::find(m_children.begin(),m_children.end(),m_modalWindow),window);}
        else window->RefreshSkin();
        window->SetShow(1);window->FocusEdit();
        if(m_chatWnd)m_chatWnd->ClearInputFocus();
        return window;
    }
    case WID_PETINFOWND:
        if(g_session.m_petStatus.job==-1)return nullptr;
        if(!m_petInfoWnd){
            m_petInfoWnd=new UIPetInfoWnd();UIPetInfoWnd::LoadStoredInfo(m_petInfoWindowInfo);
            m_children.push_back(m_petInfoWnd);m_petInfoWnd->SendMsg(nullptr,34,reinterpret_cast<msgparam_t>(&m_petInfoWindowInfo),0,0);
        }
        m_petInfoWnd->SetShow(1);return m_petInfoWnd;
    case WID_QUESTWND:
        if (!m_questWnd) {
            m_questWnd = new UIQuestWnd(windowId);
            m_children.insert(std::find(m_children.begin(), m_children.end(), m_modalWindow), m_questWnd);
        }
        m_questWnd->SetShow(1);
        return m_questWnd;
    case WID_INSTANCWND:
        if (g_session.m_instanceStatus.name.empty()) return nullptr;
        if (!m_instanceWnd) {
            m_instanceWnd = new UIInstanceWnd(windowId);
            m_children.insert(std::find(m_children.begin(), m_children.end(), m_modalWindow), m_instanceWnd);
        }
        m_instanceWnd->SetShow(1);
        m_instanceWnd->Invalidate();
        return m_instanceWnd;
    case WID_AUCTIONWND: {
        if (!m_auctionWnd) {
            m_auctionWnd = new UIAuctionWnd(windowId);
            const auto viewport = GetChatClientRect();
            const int width = viewport.right - viewport.left;
            const int height = viewport.bottom - viewport.top;
            m_auctionWnd->Move(viewport.left + (std::max)(0, (width - m_auctionWnd->m_w) / 2),
                viewport.top + (std::max)(0, (height - m_auctionWnd->m_h) / 2));
            m_children.insert(std::find(m_children.begin(), m_children.end(), m_modalWindow), m_auctionWnd);
        }
        m_auctionWnd->SetShow(1);
        return m_auctionWnd;
    }
    case WID_HOMUNINFOWND:
    case WID_MERINFOWND: {
        const bool homunculus = windowId == WID_HOMUNINFOWND;
        const int job = homunculus ? g_session.m_homunStatus.job : g_session.m_merStatus.job;
        if (job == -1) return nullptr;
        auto*& window = homunculus ? m_homunInfoWnd : m_merInfoWnd;
        if (!window) {
            window = new UICompanionStatusWnd(windowId);
            const auto viewport = GetChatClientRect();
            const int width = viewport.right - viewport.left;
            const int height = viewport.bottom - viewport.top;
            RestoreCompanionPlacement(*window, viewport, (width - 640) / 2 + 185, (height - 480) / 2 + 340);
            m_children.insert(std::find(m_children.begin(), m_children.end(), m_modalWindow), window);
        }
        window->SetShow(1);
        return window;
    }
    case WID_HOMUNSKILLLISTWND:
    case WID_MERSKILLLISTWND: {
        auto*& window = windowId == WID_HOMUNSKILLLISTWND ? m_homunSkillListWnd : m_merSkillListWnd;
        if (!window) {
            window = new UICompanionSkillListWnd(windowId);
            const auto viewport = GetChatClientRect();
            const int width = viewport.right - viewport.left;
            const int height = viewport.bottom - viewport.top;
            const std::string key = CompanionPlacementKey(windowId);
            window->Resize(LoadSettingsIniInt("UIWindows", (key + "Width").c_str(), 280),
                LoadSettingsIniInt("UIWindows", (key + "Height").c_str(), 120));
            RestoreCompanionPlacement(*window, viewport, (width - 640) / 2 + 200, (height - 480) / 2 + 200);
            m_children.insert(std::find(m_children.begin(), m_children.end(), m_modalWindow), window);
        }
        window->SetShow(1);
        return window;
    }
    case WID_FRIENDOPTIONWND:
        if(!m_friendOptionWnd){
            m_friendOptionWnd=new UIFriendOptionWnd();const auto client=GetChatClientRect();
            m_friendOptionWnd->Move((client.right-640)/2+200,120*client.bottom/480);
            m_children.insert(std::find(m_children.begin(),m_children.end(),m_modalWindow),m_friendOptionWnd);
            m_friendOptionWnd->SendMsg(nullptr,34,reinterpret_cast<msgparam_t>(&m_friendOptionWindowInfo),0,0);
        }
        m_friendOptionWnd->SetShow(1);return m_friendOptionWnd;
    case WID_TIPOFTHEDAYWND:
        if(!m_tipOfTheDayWnd){auto* tip=new UITipOfTheDayWnd();m_tipOfTheDayWnd=tip;const auto client=GetChatClientRect();
            tip->Move((client.right-640)/2+200,120*client.bottom/480);m_children.push_back(tip);}
        m_tipOfTheDayWnd->SetShow(1);return m_tipOfTheDayWnd;
    case WID_COMMANDLISTWND:
        if(!m_commandListWnd){auto* window=new UICommandListWnd();m_commandListWnd=window;const auto client=GetChatClientRect();
            window->Move((client.right-640)/2+200,120*client.bottom/480);m_children.push_back(window);}
        m_commandListWnd->SetShow(1);return m_commandListWnd;
    case WID_AUTOMESSAGEWND:
        if(!m_autoMessageWnd){m_autoMessageWnd=new UIAutoMessageWnd();m_children.insert(std::find(m_children.begin(),m_children.end(),m_modalWindow),m_autoMessageWnd);}
        m_autoMessageWnd->SetShow(1);m_autoMessageWnd->FocusForDraw();return m_autoMessageWnd;
    case WID_CHATROOMMAKE:case WID_CHATROOMCHANGE: {
        auto*& owner=windowId==WID_CHATROOMMAKE?m_chatRoomMakeWnd:m_chatRoomChangeWnd;
        if(!owner){
            const bool trick=g_session.IsStatusIconActive(29);
            if(trick && windowId==WID_CHATROOMMAKE){ShowRoomValidationMessage(661);return nullptr;}
            if(trick && m_chatRoomMakeWnd)DeleteWindow(m_chatRoomMakeWnd);
            owner=new UIChatRoomSettingsWnd(windowId==WID_CHATROOMCHANGE);m_children.push_back(owner);
            m_editWindow=owner;if(m_chatWnd)m_chatWnd->ClearInputFocus();
            if(trick)ShowRoomValidationMessage(661);
        }else if(!m_modalWindow)m_editWindow=owner;
        owner->SetShow(1);return owner;
    }
    case WID_CHATROOMPASSWORD:
        if(!m_chatRoomPasswordWnd){m_chatRoomPasswordWnd=new UIChatRoomPasswordWnd();m_children.push_back(m_chatRoomPasswordWnd);}
        m_editWindow=m_chatRoomPasswordWnd;if(m_chatWnd)m_chatWnd->ClearInputFocus();
        m_chatRoomPasswordWnd->SetShow(1);return m_chatRoomPasswordWnd;

    case WID_CHATROOMWND:
        if(!m_chatRoomWnd){
            m_chatRoomWnd=new UIChatRoomWnd();m_children.push_back(m_chatRoomWnd);
            m_editWindow=m_chatRoomWnd;if(m_chatWnd)m_chatWnd->ClearInputFocus();
        }
        m_chatRoomWnd->SetShow(1);return m_chatRoomWnd;

    case WID_BATTLEMSGOPTION:
        if (!m_battleMsgOptionWnd) {
            m_battleMsgOptionWnd=new UIBattleMsgOptionWnd();
            m_children.push_back(m_battleMsgOptionWnd);
        }
        m_battleMsgOptionWnd->SetShow(1);
        return m_battleMsgOptionWnd;
    case WID_SUBCHATST:
    case WID_SUBCHATBT: {
        const bool bt = windowId == WID_SUBCHATBT;
        auto*& window = bt ? m_subChatWndBT : m_subChatWndST;
        if (!window) {
            window = new UISubChatHisWnd(bt ? 1 : 0);
            m_children.push_back(window);
        }
        return window;
    }
    case WID_SUBCHATMINI:
        if (!m_subChatMiniWnd) {
            m_subChatMiniWnd = new UISubChatMiniWnd();
            m_children.push_back(m_subChatMiniWnd);
        }
        return m_subChatMiniWnd;
    case WID_BASICINFOWND: {
        if (!m_basicInfoWnd) {
            m_basicInfoWnd = new UIBasicInfoWnd();
            m_children.push_back(m_basicInfoWnd);
        }
        m_children.remove(m_basicInfoWnd);
        m_children.push_front(m_basicInfoWnd);
        m_basicInfoWnd->SetShow(1);
        // Source basic UI also recreates chat even when its saved show is0.
        const bool rememberedChatShow=m_chatWndShow!=0;
        ShowChatWindow(rememberedChatShow);
        // Ref case0 reconstructs remembered inventory after map teardown.
        if (GetInventoryWindowShowState()) MakeWindow(WID_ITEMWND);
        return m_basicInfoWnd;
    }

    case WID_STATUSWND:
        if (!m_statusWnd) {
            m_statusWnd = new UIStatusWnd();
            m_children.push_back(m_statusWnd);
        }
        m_children.remove(m_statusWnd);
        m_children.push_back(m_statusWnd);
        m_statusWnd->SetShow(1);
        return m_statusWnd;

    case WID_NPCMENUWND:
        if (!m_npcMenuWnd) {
            m_npcMenuWnd = new UINpcMenuWnd();
            m_children.push_back(m_npcMenuWnd);
        }
        m_children.remove(m_npcMenuWnd);
        m_children.push_back(m_npcMenuWnd);
        m_npcMenuWnd->SetShow(1);
        return m_npcMenuWnd;

    case WID_PLAYERCONTEXTMENUWND:
        if (!m_playerContextMenuWnd) {
            m_playerContextMenuWnd = new UIPlayerContextMenuWnd();
            m_children.push_back(m_playerContextMenuWnd);
        }
        m_children.remove(m_playerContextMenuWnd);
        m_children.push_back(m_playerContextMenuWnd);
        m_playerContextMenuWnd->SetShow(1);
        return m_playerContextMenuWnd;

    case WID_MESSENGERGROUPWND:
        if (!m_messengerGroupWnd) {
            m_messengerGroupWnd = new UIMessengerGroupWnd();
            m_children.push_back(m_messengerGroupWnd);
        }
        m_children.remove(m_messengerGroupWnd);
        m_children.push_back(m_messengerGroupWnd);
        m_messengerGroupWnd->SetShow(1);
        return m_messengerGroupWnd;

    case WID_JOINPARTYACCEPTWND:
        if (!m_joinPartyAcceptWnd) {
            m_joinPartyAcceptWnd = new UIJoinPartyAcceptWnd();
            m_children.push_back(m_joinPartyAcceptWnd);
        }
        m_children.remove(m_joinPartyAcceptWnd);
        m_children.push_back(m_joinPartyAcceptWnd);
        m_joinPartyAcceptWnd->SetShow(1);
        return m_joinPartyAcceptWnd;

    case WID_PARTYOPTIONWND:
        if (!m_partyOptionWnd) {
            m_partyOptionWnd = new UIPartyOptionWnd();
            m_children.push_back(m_partyOptionWnd);
        }
        m_children.remove(m_partyOptionWnd);
        m_children.push_back(m_partyOptionWnd);
        m_partyOptionWnd->SetShow(1);
        return m_partyOptionWnd;

    case WID_GUILDWND:
        if(!m_guildWnd){m_guildWnd=new UIGuildWnd();m_children.push_back(m_guildWnd);}
        m_children.remove(m_guildWnd);m_children.push_back(m_guildWnd);
        m_guildWnd->SetShow(1);return m_guildWnd;

    case WID_CHOOSESELLBUYWND:
        if (!m_chooseSellBuyWnd) {
            m_chooseSellBuyWnd = new UIChooseSellBuyWnd();
            m_children.push_back(m_chooseSellBuyWnd);
        }
        m_children.remove(m_chooseSellBuyWnd);
        m_children.push_back(m_chooseSellBuyWnd);
        m_chooseSellBuyWnd->SetShow(1);
        return m_chooseSellBuyWnd;

    case WID_ITEMSHOPWND:
        if (!m_itemShopWnd) {
            m_itemShopWnd = new UIItemShopWnd();
            m_children.push_back(m_itemShopWnd);
        }
        m_children.remove(m_itemShopWnd);
        m_children.push_back(m_itemShopWnd);
        m_itemShopWnd->SetShow(1);
        return m_itemShopWnd;

    case WID_ITEMPURCHASEWND:
        if (!m_itemPurchaseWnd) {
            m_itemPurchaseWnd = new UIItemPurchaseWnd();
            m_children.push_back(m_itemPurchaseWnd);
        }
        m_children.remove(m_itemPurchaseWnd);
        m_children.push_back(m_itemPurchaseWnd);
        m_itemPurchaseWnd->SetShow(1);
        return m_itemPurchaseWnd;

    case WID_ITEMSELLWND:
        if (!m_itemSellWnd) {
            m_itemSellWnd = new UIItemSellWnd();
            m_children.push_back(m_itemSellWnd);
        }
        m_children.remove(m_itemSellWnd);
        m_children.push_back(m_itemSellWnd);
        m_itemSellWnd->SetShow(1);
        return m_itemSellWnd;

    case WID_STORAGEWND:
        if (!m_storageWnd) {
            m_storageWnd = new UIStorageWnd();
            m_children.push_back(m_storageWnd);
        }
        m_children.remove(m_storageWnd);
        m_children.push_back(m_storageWnd);
        m_storageWnd->SetShow(1);
        return m_storageWnd;

    case WID_SHORTCUTWND:
        if (!m_shortCutWnd) {
            m_shortCutWnd = new UIShortCutWnd();
            m_children.push_back(m_shortCutWnd);
        }
        m_children.remove(m_shortCutWnd);
        m_children.push_back(m_shortCutWnd);
        m_shortCutWnd->SetShow(1);
        return m_shortCutWnd;

    case WID_NOTIFYLEVELUPWND:
        if (!m_notifyLevelUpWnd) {
            m_notifyLevelUpWnd = new UINotifyLevelUpWnd();
            m_children.push_back(m_notifyLevelUpWnd);
        }
        m_children.remove(m_notifyLevelUpWnd);
        m_children.push_back(m_notifyLevelUpWnd);
        m_notifyLevelUpWnd->SetShow(1);
        return m_notifyLevelUpWnd;

    case WID_NOTIFYJOBLEVELUPWND:
        if (!m_notifyJobLevelUpWnd) {
            m_notifyJobLevelUpWnd = new UINotifyJobLevelUpWnd();
            m_children.push_back(m_notifyJobLevelUpWnd);
        }
        m_children.remove(m_notifyJobLevelUpWnd);
        m_children.push_back(m_notifyJobLevelUpWnd);
        m_notifyJobLevelUpWnd->SetShow(1);
        return m_notifyJobLevelUpWnd;

    case WID_ITEMWND:
        if (!m_itemWnd) {
            m_itemWnd = new UIItemWnd();
            m_children.push_back(m_itemWnd);
        }
        m_children.remove(m_itemWnd);
        m_children.push_back(m_itemWnd);
        m_itemWnd->SetShow(1);
        SetInventoryWindowShowState(true);
        return m_itemWnd;

    case WID_ITEMINFOWND:
        if (!m_itemInfoWnd) {
            m_itemInfoWnd = new UIItemInfoWnd();
            m_children.push_back(m_itemInfoWnd);
        }
        m_children.remove(m_itemInfoWnd);
        m_children.push_back(m_itemInfoWnd);
        m_itemInfoWnd->SetShow(1);
        return m_itemInfoWnd;

    case WID_CARDINFOWND:
        if (!m_cardInfoWnd) {
            m_cardInfoWnd = new UIItemInfoWnd(ItemDetailKind::SlottedCard);
            m_children.push_back(m_cardInfoWnd);
        }
        m_children.remove(m_cardInfoWnd);
        m_children.push_back(m_cardInfoWnd);
        m_cardInfoWnd->SetShow(1);
        return m_cardInfoWnd;

    case WID_NOTIFYITEMOBTAINWND:
        if (!m_itemObtainWnd) {
            m_itemObtainWnd=new UINotifyItemObtainWnd();
            m_children.push_back(m_itemObtainWnd);
        }
        m_children.remove(m_itemObtainWnd);m_children.push_back(m_itemObtainWnd);
        m_itemObtainWnd->SetShow(1);
        return m_itemObtainWnd;

    case WID_ITEMCOLLECTIONWND:
        if (!m_itemCollectionWnd) {
            m_itemCollectionWnd = new UIItemCollectionWnd();
            m_children.push_back(m_itemCollectionWnd);
        }
        m_children.remove(m_itemCollectionWnd);
        m_children.push_back(m_itemCollectionWnd);
        m_itemCollectionWnd->SetShow(1);
        return m_itemCollectionWnd;

    case WID_ITEMIDENTIFYWND:
        if (!m_itemIdentifyWnd) {
            m_itemIdentifyWnd = new UIItemIdentifyWnd();
            m_children.push_back(m_itemIdentifyWnd);
        }
        m_children.remove(m_itemIdentifyWnd);
        m_children.push_back(m_itemIdentifyWnd);
        m_itemIdentifyWnd->SetShow(1);
        return m_itemIdentifyWnd;

    case WID_SELECTPETEGGWND:
        if(!m_petEggListWnd){m_petEggListWnd=new UIPetEggListWnd();m_children.push_back(m_petEggListWnd);}
        m_petEggListWnd->SetShow(1);return m_petEggListWnd;
    case WID_PETTAMINGDECEIVEWND:
        if(!m_petTamingDeceiveWnd){m_petTamingDeceiveWnd=new UIPetTamingDeceiveWnd();m_children.push_back(m_petTamingDeceiveWnd);}
        m_petTamingDeceiveWnd->SetShow(1);return m_petTamingDeceiveWnd;

    case WID_MONSTERINFOWND:
        if (!m_monsterInfoWnd) m_monsterInfoWnd = new UIMonsterInfoWnd();
        m_children.remove(m_monsterInfoWnd);
        m_children.push_back(m_monsterInfoWnd);
        m_monsterInfoWnd->SetShow(1);
        return m_monsterInfoWnd;
    case WID_SKILLSELECTWND:
        if (!m_skillSelectWnd) {
            m_skillSelectWnd = new UISkillSelectWnd();
            m_children.push_back(m_skillSelectWnd);
        }
        m_children.remove(m_skillSelectWnd);
        m_children.push_back(m_skillSelectWnd);
        m_skillSelectWnd->SetShow(1);
        return m_skillSelectWnd;

    case WID_ITEMCOMPOSITIONWND:
        if (!m_itemCompositionWnd) {
            m_itemCompositionWnd = new UIItemCompositionWnd();
            m_children.push_back(m_itemCompositionWnd);
        }
        m_children.remove(m_itemCompositionWnd);
        m_children.push_back(m_itemCompositionWnd);
        m_itemCompositionWnd->SetShow(1);
        return m_itemCompositionWnd;

    case WID_EQUIPWND:
        if (!m_equipWnd) {
            m_equipWnd = new UIEquipWnd();
            m_children.push_back(m_equipWnd);
        }
        m_children.remove(m_equipWnd);
        m_children.push_back(m_equipWnd);
        m_equipWnd->SetShow(1);
        return m_equipWnd;

    case WID_SELECTCARTWND:
        if (!m_selectCartWnd) {
            m_selectCartWnd = new UISelectCartWnd();
            m_children.push_back(m_selectCartWnd);
        }
        m_children.remove(m_selectCartWnd);
        m_children.push_back(m_selectCartWnd);
        m_selectCartWnd->SetShow(1);
        return m_selectCartWnd;

    case WID_SKILLDESCRIBEWND:
        if (!m_skillDescribeWnd) {
            m_skillDescribeWnd = new UISkillDescribeWnd();
            m_children.push_back(m_skillDescribeWnd);
        }
        m_children.remove(m_skillDescribeWnd);
        m_children.push_back(m_skillDescribeWnd);
        m_skillDescribeWnd->SetShow(1);
        return m_skillDescribeWnd;

    case WID_SKILLLISTWND:
        if (!m_skillListWnd) {
            m_skillListWnd = new UISkillListWnd();
            m_children.push_back(m_skillListWnd);
        }
        m_children.remove(m_skillListWnd);
        m_children.push_back(m_skillListWnd);
        m_skillListWnd->SetShow(1);
        return m_skillListWnd;

    case WID_LOGINWND:
        if (!m_loginWnd) {
            m_loginWnd = new UILoginWnd();
            if (!m_loginWallpaper.empty()) {
                m_loginWnd->SetWallpaperName(m_loginWallpaper);
            }
            m_children.push_back(m_loginWnd);
        }
        m_loginWnd->SetShow(1);
        return m_loginWnd;

    case WID_SELECTSERVERWND:
        if (!m_selectServerWnd) {
            m_selectServerWnd = new UISelectServerWnd();
            m_children.push_back(m_selectServerWnd);
        }
        m_children.remove(m_selectServerWnd);
        m_children.push_back(m_selectServerWnd);
        m_selectServerWnd->SetShow(1);
        return m_selectServerWnd;

    case WID_WAITWND: {
        if (!m_waitWnd) {
            m_waitWnd = new UIWaitWnd();
            m_children.push_back(m_waitWnd);
        }
        m_children.remove(m_waitWnd);
        m_children.push_back(m_waitWnd);
        m_waitWnd->SetShow(1);
        return m_waitWnd;
    }

    case WID_LOADINGWND:
        if (!m_loadingWnd) {
            m_loadingWnd = new UILoadingWnd();
            m_children.push_back(m_loadingWnd);
        }
        m_loadingWnd->SetShow(1);
        return m_loadingWnd;

    case WID_ROMAPWND:
        if (!m_roMapWnd) {
            m_roMapWnd = new UIRoMapWnd();
            m_children.push_back(m_roMapWnd);
        }
        m_children.remove(m_roMapWnd);
        m_children.push_back(m_roMapWnd);
        m_roMapWnd->SetShow(1);
        return m_roMapWnd;

    case WID_SELECTCHARWND:
        DbgLog("[WindowMgr] MakeWindow WID_SELECTCHARWND existing=%p\n", m_selectCharWnd);
        if (!m_selectCharWnd) {
            DbgLog("[WindowMgr] Creating UISelectCharWnd\n");
            m_selectCharWnd = new UISelectCharWnd();
            m_children.push_back(m_selectCharWnd);
            DbgLog("[WindowMgr] Created UISelectCharWnd=%p\n", m_selectCharWnd);
        }
        m_selectCharWnd->SetShow(1);
        DbgLog("[WindowMgr] WID_SELECTCHARWND show enabled\n");
        return m_selectCharWnd;

    case WID_MAKECHARWND:
        if (!m_makeCharWnd) {
            m_makeCharWnd = new UIMakeCharWnd();
            m_children.push_back(m_makeCharWnd);
        }
        m_makeCharWnd->SetShow(1);
        return m_makeCharWnd;

    case WID_SAYDIALOGWND:
        if (!m_sayDialogWnd) {
            m_sayDialogWnd = new UISayDialogWnd();
            m_children.push_back(m_sayDialogWnd);
        }
        m_children.remove(m_sayDialogWnd);
        m_children.push_back(m_sayDialogWnd);
        m_sayDialogWnd->SetShow(1);
        return m_sayDialogWnd;

    case WID_NPCINPUTWND:
        if (!m_npcInputWnd) {
            m_npcInputWnd = new UINpcInputWnd();
            m_children.push_back(m_npcInputWnd);
        }
        m_children.remove(m_npcInputWnd);
        m_children.push_back(m_npcInputWnd);
        m_npcInputWnd->SetShow(1);
        return m_npcInputWnd;

    case WID_CHOOSEWND:
        if (!m_chooseWnd) {
            m_chooseWnd = new UIChooseWnd();
            m_children.push_back(m_chooseWnd);
        }
        m_children.remove(m_chooseWnd);
        m_children.push_back(m_chooseWnd);
        m_chooseWnd->SetShow(1);
        return m_chooseWnd;

    case WID_OPTIONWND:
        if (!m_optionWnd) {
            m_optionWnd = new UIOptionWnd();
            m_children.push_back(m_optionWnd);
        }
        m_children.remove(m_optionWnd);
        m_children.push_back(m_optionWnd);
        m_optionWnd->SetShow(1);
        return m_optionWnd;

    default:
        return nullptr;
    }
}

bool UIWindowMgr::ToggleWindow(int windowId)
{
    if (windowId == WID_HOMUNSKILLLISTWND || windowId == WID_MERSKILLLISTWND) {
        auto* window = windowId == WID_HOMUNSKILLLISTWND ? m_homunSkillListWnd : m_merSkillListWnd;
        if (!window) return MakeWindow(windowId) != nullptr;
        DeleteWindow(window);
        return false;
    }
    if (windowId==WID_BATTLEMSGOPTION) {
        if (m_battleMsgOptionWnd) {DeleteWindow(m_battleMsgOptionWnd);return false;}
        return MakeWindow(windowId)!=nullptr;
    }
    UIWindow* window = nullptr;
    switch (windowId) {
    case WID_QUESTWND:
        window = m_questWnd;
        break;
    case WID_STATUSWND:
        window = m_statusWnd;
        break;
    case WID_GUILDWND:
        window = m_guildWnd;
        break;
    case WID_ITEMWND:
        // The source basic-info button/Alt-E tries DeleteWindow first,
        // including retained hidden instances, then creates only if absent.
        if (m_itemWnd) {
            CloseInventoryWindow();
            return false;
        }
        return MakeWindow(WID_ITEMWND) != nullptr;
    case WID_STORAGEWND:
        window = m_storageWnd;
        break;
    case WID_MESSENGERGROUPWND:
        window = m_messengerGroupWnd;
        break;
    case WID_EQUIPWND:
        window = m_equipWnd;
        break;
    case WID_SELECTCARTWND:
        window = m_selectCartWnd;
        break;
    case WID_SKILLLISTWND:
        window = m_skillListWnd;
        break;
    case WID_OPTIONWND:
        window = m_optionWnd;
        break;
    case WID_ROMAPWND:
        window = m_roMapWnd;
        break;
    default:
        window = nullptr;
        break;
    }

    if (window && window->m_show != 0) {
        window->SetShow(0);
        window->StoreInfo();
        return false;
    }

    return MakeWindow(windowId) != nullptr;
}

void UIWindowMgr::AddWindowFront(UIWindow* window)
{
    if (!window) {
        return;
    }

    m_children.remove(window);
    m_children.push_front(window);
    window->SetShow(1);
}

void UIWindowMgr::PostQuit(UIWindow* window)
{
    if(window && ContainsManagedWindow(*this,window) && std::find(m_postedQuits.begin(),m_postedQuits.end(),window)==m_postedQuits.end())m_postedQuits.push_back(window);
}

void UIWindowMgr::ProcessSourceHoverFrame(int x,int y)
{
    if(m_captureWindow)return;
    auto* hit=HitTestWindow(x,y);
    if(m_lastHitWindow!=hit){if(m_lastHitWindow)m_lastHitWindow->OnMouseHover(-1,-1);m_lastHitWindow=hit;}
    if(hit)hit->OnMouseHover(x,y);
}

bool UIWindowMgr::ProcessSourceCommandWindowKey(int key)
{
    // UICommandListWnd invokes ProcessPushButton(value,10000,0). Keep source
    // owner guards and virtual-key mapping separate from modern hotkeys.
    int button=0;
    switch(key){
    case 65:button=126;break;case 79:button=127;break;case 69:button=128;break;
    case 81:button=129;break;case 83:button=130;break;case 86:button=136;break;
    case 67:case 73:case 90:case 72:case 77:case 76:return false; // Selected defaults, not missing routes.
    case 71:
        if(m_basicInfoWnd && g_session.m_guildWindow.id){if(m_guildWnd)DeleteWindow(m_guildWnd);else MakeWindow(WID_GUILDWND);}return true;
    case 80:
        if(m_basicInfoWnd){if(m_partyOptionWnd)DeleteWindow(m_partyOptionWnd);
            else if(g_session.GetNumParty()!=0){auto* window=static_cast<UIPartyOptionWnd*>(MakeWindow(WID_PARTYOPTIONWND));if(window)window->OpenConfigureDialog();}}
        return true;
    case 74:
        if(m_basicInfoWnd){if(m_petInfoWnd)DeleteWindow(m_petInfoWnd);else MakeWindow(WID_PETINFOWND);}return true;
    case 68:case 87:
        // These selected recipients still require their real screen/Up
        // owners. In particular87 remaps to38; it must never open cart181.
        DbgLog("[SourceCommandWindow] key=%d awaits source recipient integration\n",key);return false;
    default:return false;
    }
    if(!m_basicInfoWnd)return false;m_basicInfoWnd->SendMsg(nullptr,6,button,0,0);return true;
}

void UIWindowMgr::CloseCompanionWindows(bool homunculus)
{
    DeleteWindow(homunculus ? m_homunInfoWnd : m_merInfoWnd);
    DeleteWindow(homunculus ? m_homunSkillListWnd : m_merSkillListWnd);
}

void UIWindowMgr::DeleteWindow(UIWindow* window)
{
    DeleteWindowImpl(window,false);
}

bool UIWindowMgr::ContainsWindow(const UIWindow* target) const
{
    if(!target)return false;
    const auto contains=[&](auto&& self,const UIWindow* window)->bool{
        if(!window)return false;
        if(window==target)return true;
        for(const auto* child:window->m_children)if(self(self,child))return true;
        return false;
    };
    for(const auto* root:m_children)if(contains(contains,root))return true;
    return false;
}

void UIWindowMgr::DeleteWindowImpl(UIWindow* window, bool preserveInventoryShow)
{
    if (!window) {
        return;
    }

    if(m_roomMessageBox && m_roomMessageBox!=window && m_roomMessageBox->GetSourceOwner()==window)
        DeleteWindow(m_roomMessageBox);

    const bool deletingTip=window==m_tipOfTheDayWnd;
    const bool deletingCommand=window==m_commandListWnd;
    const bool deletingPet=window==m_petInfoWnd;
    const bool deletingPetEgg=window==m_petEggListWnd;
    const bool deletingPetTaming=window==m_petTamingDeceiveWnd;
    const bool deletingCompanion = window == m_homunInfoWnd || window == m_merInfoWnd
        || window == m_homunSkillListWnd || window == m_merSkillListWnd;
    if (deletingCompanion) {
        const std::string key = CompanionPlacementKey(window->m_id);
        std::vector<std::pair<std::string, int>> placement{{key + "X", window->m_x}, {key + "Y", window->m_y}};
        if (window == m_homunSkillListWnd || window == m_merSkillListWnd) {
            placement.emplace_back(key + "Width", window->m_w);
            placement.emplace_back(key + "Height", window->m_h);
        }
        SaveSettingsIniIntValues("UIWindows", placement);
    }
    const bool deletingGroundText=GroundSkillTextOwner(window)!=nullptr;
    for(auto*& owner:m_groundSkillTextWindows)if(owner==window)owner=nullptr;
    if(window==m_commandListWnd)m_commandListWnd=nullptr;
    if(window==m_tipOfTheDayWnd)m_tipOfTheDayWnd=nullptr;
    if(window==m_petInfoWnd){m_petInfoWnd->StoreInfo();m_petInfoWnd=nullptr;}
    if (window == m_homunInfoWnd) m_homunInfoWnd = nullptr;
    if (window == m_auctionWnd) m_auctionWnd = nullptr;
    if (window == m_instanceWnd) m_instanceWnd = nullptr;
    if (window == m_questWnd) m_questWnd = nullptr;
    if (window == m_merInfoWnd) m_merInfoWnd = nullptr;
    if (window == m_homunSkillListWnd) m_homunSkillListWnd = nullptr;
    if (window == m_merSkillListWnd) m_merSkillListWnd = nullptr;
    if(deletingPetEgg)m_petEggListWnd=nullptr;
    if(deletingPetTaming)m_petTamingDeceiveWnd=nullptr;

    const auto belongsToWindow=[window](const UIWindow* candidate) {
        for (auto* current=candidate;current;current=current->m_parent) if (current==window) return true;
        return false;
    };
    const bool localChatCapture = window == m_chatWnd && m_chatWnd && m_chatWnd->IsQtPointerCaptured();
    const bool nativeChatCapture = belongsToWindow(m_captureWindow)
        && (deletingTip || deletingCommand || deletingPet || deletingPetEgg || deletingPetTaming || deletingGroundText || IsWhisperPreferenceInputWindow(window) || window == m_chatWnd || window == m_subChatWndST || window == m_subChatWndBT || window == m_subChatMiniWnd || window == m_battleMsgOptionWnd || window == m_chatRoomWnd || window == m_chatRoomPasswordWnd || window==m_chatRoomMakeWnd || window==m_chatRoomChangeWnd || window==m_roomMessageBox || window==m_chatRoomMenuWnd || window==m_autoMessageWnd || window==m_friendOptionWnd || window==m_messengerGroupWnd || dynamic_cast<UIWhisperWnd*>(window));
    if ((localChatCapture || nativeChatCapture || (deletingCompanion && belongsToWindow(m_captureWindow)))
        && g_hMainWnd && GetCapture() == g_hMainWnd) ReleaseCapture();
    if (belongsToWindow(m_captureWindow)) {
        m_captureWindow = nullptr;
    }
    if (belongsToWindow(m_editWindow)) {
        m_editWindow = nullptr;
    }
    if (window == m_npcInputWnd && m_npcInputWnd && m_editWindow == m_npcInputWnd->GetEditCtrl()) {
        m_editWindow = nullptr;
    }
    if (belongsToWindow(m_modalWindow)) {
        m_modalWindow = nullptr;
    }
    if (belongsToWindow(m_lastHitWindow)) {
        m_lastHitWindow = nullptr;
    }

    m_children.remove(window);

    m_postedQuits.erase(std::remove(m_postedQuits.begin(),m_postedQuits.end(),window),m_postedQuits.end());

    if (window == m_loginWnd) {
        m_loginWnd = nullptr;
    }
    if (window == m_selectServerWnd) {
        m_selectServerWnd = nullptr;
    }
    if (window == m_selectCharWnd) {
        m_selectCharWnd = nullptr;
    }
    if (window == m_makeCharWnd) {
        m_makeCharWnd = nullptr;
    }
    if (window == m_waitWnd) {
        m_waitWnd = nullptr;
    }
    if (window == m_chooseWnd) {
        m_chooseWnd = nullptr;
    }
    if (window == m_optionWnd) {
        m_optionWnd = nullptr;
    }
    if (window == m_loadingWnd) {
        m_loadingWnd = nullptr;
    }
    if (window == m_roMapWnd) {
        m_roMapWnd = nullptr;
    }
    if (window == m_chatWnd) {
        m_chatWnd->StoreInfo();
        if(!preserveInventoryShow)m_chatWndShow=0;
        m_chatWnd = nullptr;
    }
    if (window == m_chatRoomMakeWnd)m_chatRoomMakeWnd=nullptr;
    if (window == m_chatRoomChangeWnd)m_chatRoomChangeWnd=nullptr;
    if (window == m_roomMessageBox)m_roomMessageBox=nullptr;
    if (window == m_chatRoomMenuWnd)m_chatRoomMenuWnd=nullptr;
    if (window == m_autoMessageWnd)m_autoMessageWnd=nullptr;
    if (window == m_friendOptionWnd){m_friendOptionWnd->StoreInfo();m_friendOptionWnd=nullptr;}
    if (window == m_chatRoomPasswordWnd) m_chatRoomPasswordWnd=nullptr;
    if (window == m_chatRoomWnd) { m_chatRoomWnd->StoreInfo();m_chatRoomWnd=nullptr; }
    if (window == m_subChatWndST) { m_subChatWndST->StoreInfo(); m_subChatWndST = nullptr; }
    if (window == m_subChatWndBT) { m_subChatWndBT->StoreInfo(); m_subChatWndBT = nullptr; }
    if (window == m_subChatMiniWnd) m_subChatMiniWnd = nullptr;
    if (window == m_battleMsgOptionWnd) m_battleMsgOptionWnd = nullptr;
    if (window == m_basicInfoWnd) {
        m_basicInfoWnd = nullptr;
    }
    if (window == m_statusWnd) {
        m_statusWnd = nullptr;
    }
    if (window == m_sayDialogWnd) {
        m_sayDialogWnd = nullptr;
    }
    if (window == m_npcMenuWnd) {
        m_npcMenuWnd = nullptr;
    }
    if (window == m_playerContextMenuWnd) {
        m_playerContextMenuWnd = nullptr;
    }
    if (window == m_joinPartyAcceptWnd) {
        m_joinPartyAcceptWnd = nullptr;
    }
    if (window == m_messengerGroupWnd) {
        m_messengerGroupWnd = nullptr;
    }
    if (window == m_partyOptionWnd) {
        m_partyOptionWnd = nullptr;
    }
    if(window==m_guildWnd)m_guildWnd=nullptr;
    if (window == m_npcInputWnd) {
        m_npcInputWnd = nullptr;
    }
    if (window == m_chooseSellBuyWnd) {
        m_chooseSellBuyWnd = nullptr;
    }
    if (window == m_itemShopWnd) {
        m_itemShopWnd = nullptr;
    }
    if (window == m_itemPurchaseWnd) {
        m_itemPurchaseWnd = nullptr;
    }
    if (window == m_itemSellWnd) {
        m_itemSellWnd = nullptr;
    }
    if (window == m_storageWnd) {
        m_storageWnd = nullptr;
    }
    if (window == m_shortCutWnd) {
        m_shortCutWnd = nullptr;
    }
    if (window == m_notifyLevelUpWnd) {
        m_notifyLevelUpWnd = nullptr;
    }
    if (window == m_notifyJobLevelUpWnd) {
        m_notifyJobLevelUpWnd = nullptr;
    }
    if (window == m_itemWnd) {
        m_itemWnd->StoreInfo();
        m_itemWnd = nullptr;
        if (!preserveInventoryShow) SetInventoryWindowShowState(false);
    }
    if (window == m_itemInfoWnd) {
        m_itemInfoWnd->StoreInfo();
        m_itemInfoWnd = nullptr;
    }
    if (window == m_cardInfoWnd) {
        m_cardInfoWnd->StoreInfo();
        m_cardInfoWnd = nullptr;
    }
    if (window == m_itemObtainWnd) m_itemObtainWnd=nullptr;
    if (window == m_itemCollectionWnd) {
        m_itemCollectionWnd = nullptr;
    }
    if (window == m_itemCompositionWnd) {
        m_itemCompositionWnd = nullptr;
    }
    if (window == m_itemIdentifyWnd) {
        m_itemIdentifyWnd = nullptr;
    }
    if (window == m_skillSelectWnd) {
        m_skillSelectWnd->CancelMaterialDrag();
        m_skillSelectWnd = nullptr;
    }
    if (window == m_monsterInfoWnd) {
        m_monsterInfoWnd = nullptr;
    }
    if (window == m_equipWnd) {
        m_equipWnd = nullptr;
    }
    if (window == m_selectCartWnd) {
        m_selectCartWnd = nullptr;
    }
    if (window == m_skillDescribeWnd) {
        m_skillDescribeWnd = nullptr;
    }
    if (window == m_skillListWnd) {
        m_skillListWnd = nullptr;
    }

    if(auto* whisper=dynamic_cast<UIWhisperWnd*>(window))whisper->StoreInfo();
    delete window;
}

void UIWindowMgr::RemoveAllWindows()
{
    if (m_chatWnd) {
        m_chatInputHistory = m_chatWnd->GetInputHistory();
        m_chatWhisperTargetText = m_chatWnd->GetWhisperTargetText();
        m_chatInputText = m_chatWnd->GetInputText();
        if (m_chatWnd->IsWhisperTargetActive()) {
            m_chatActiveInputField = UINewChatWnd::InputField_WhisperTarget;
        } else if (m_chatWnd->IsMessageInputActive()) {
            m_chatActiveInputField = UINewChatWnd::InputField_Message;
        } else {
            m_chatActiveInputField = UINewChatWnd::InputField_None;
        }
        m_chatScrollLineOffset = m_chatWnd->GetScrollBarState().totalLines > 0 ? m_chatWnd->GetScrollBarState().totalLines - (m_chatWnd->GetScrollBarState().firstVisibleLine + m_chatWnd->GetScrollBarState().visibleLineCount) : 0;
    }

    while (!m_children.empty()) {
        // Bulk teardown stores windows without the explicit close semantics
        // that clear inventory's remembered show bit in Ref case8.
        DeleteWindowImpl(m_children.front(),true);
    }

    m_captureWindow = nullptr;
    m_editWindow = nullptr;
    m_modalWindow = nullptr;
    m_lastHitWindow = nullptr;

    m_loadingWnd = nullptr;
    m_roMapWnd = nullptr;
    m_minimapZoomWnd = nullptr;
    m_statusWnd = nullptr;
    m_sayDialogWnd = nullptr;
    m_npcMenuWnd = nullptr;
    m_playerContextMenuWnd = nullptr;
    m_joinPartyAcceptWnd = nullptr;
    m_messengerGroupWnd = nullptr;
    m_partyOptionWnd = nullptr;
    m_guildWnd = nullptr;
    m_npcInputWnd = nullptr;
    m_chooseSellBuyWnd = nullptr;
    m_itemShopWnd = nullptr;
    m_itemPurchaseWnd = nullptr;
    m_itemSellWnd = nullptr;
    m_storageWnd = nullptr;
    m_shortCutWnd = nullptr;
    m_chatWnd = nullptr;
    m_basicInfoWnd = nullptr;
    m_notifyLevelUpWnd = nullptr;
    m_notifyJobLevelUpWnd = nullptr;
    m_loginWnd = nullptr;
    m_selectServerWnd = nullptr;
    m_selectCharWnd = nullptr;
    m_makeCharWnd = nullptr;
    m_waitWnd = nullptr;
    m_chooseWnd = nullptr;
    m_selectCartWnd = nullptr;
    m_optionWnd = nullptr;
    m_itemWnd = nullptr;
    m_itemInfoWnd = nullptr;
    m_cardInfoWnd = nullptr;
    m_itemObtainWnd = nullptr;
    m_itemCollectionWnd = nullptr;
    m_itemCompositionWnd = nullptr;
    m_itemIdentifyWnd = nullptr;
    m_skillSelectWnd = nullptr;
    m_questWnd = nullptr;
    m_basicInfoWnd = nullptr;
    m_equipWnd = nullptr;
    m_skillDescribeWnd = nullptr;
    m_skillListWnd = nullptr;
}

void UIWindowMgr::Reset() {
    RemoveAllWindows();
    m_loginWnd = nullptr;
    m_selectCharWnd = nullptr;
    m_makeCharWnd = nullptr;
    m_chooseWnd = nullptr;
    m_selectCartWnd = nullptr;
    m_optionWnd = nullptr;
    m_chatWnd = nullptr;
    m_chatEvents.clear();
    m_chatInputHistory.clear();
    m_chatWhisperTargetText.clear();
    m_chatInputText.clear();
    m_chatActiveInputField = 0;
    m_chatScrollLineOffset = 0;
    m_skillListWnd = nullptr;

    if (m_wallpaperSurface) {
        delete m_wallpaperSurface;
        m_wallpaperSurface = nullptr;
    }
    ReleaseComposeSurface();
    m_loadedWallpaperPath.clear();
}

void UIWindowMgr::OnProcess() {
    const auto posted=std::move(m_postedQuits);m_postedQuits.clear();
    for(auto* window:posted)if(ContainsManagedWindow(*this,window))DeleteWindow(window);
    const std::vector<UIWindow*> children(m_children.begin(), m_children.end());
    for (UIWindow* child : children) {
        if (!ContainsManagedWindow(*this, child)) {
            continue;
        }
        child->OnProcess();
    }
    // The original mode polls hover every frame. Refresh the live native
    // pointer even when Windows emits no new movement, so the shared100ms help
    // deadline measures leaving a help target rather than stopping the mouse.
    if(g_hMainWnd && IsWindowVisible(g_hMainWnd) && GetForegroundWindow()==g_hMainWnd && !m_captureWindow){
        POINT point{};if(GetCursorPos(&point) && ScreenToClient(g_hMainWnd,&point)){
            const int x=UiScaleRawToLogicalCoordinate(point.x),y=UiScaleRawToLogicalCoordinate(point.y);
            ProcessSourceHoverFrame(x,y);
        }
    }
}

bool UIWindowMgr::HasDirtyVisualState() const
{
    for (UIWindow* child : const_cast<UIWindowMgr*>(this)->m_children) {
        if (HasDirtyWindowRecursive(child)) {
            return true;
        }
    }
    return false;
}

bool UIWindowMgr::HasDirtyVisualStateExcludingRoMap() const
{
    for (UIWindow* child : const_cast<UIWindowMgr*>(this)->m_children) {
        if (child == m_roMapWnd) {
            continue;
        }
        if (HasDirtyWindowRecursive(child)) {
            return true;
        }
    }
    return false;
}

bool UIWindowMgr::HasRoMapDirtyVisualState() const
{
    return HasDirtyWindowRecursive(m_roMapWnd);
}

void UIWindowMgr::ClearDirtyVisualState()
{
    for (UIWindow* child : m_children) {
        if (child) {
            ClearDirtyWindowRecursive(child);
        }
    }
}

void UIWindowMgr::ClearDirtyVisualStateExcludingRoMap()
{
    for (UIWindow* child : m_children) {
        if (child && child != m_roMapWnd) {
            ClearDirtyWindowRecursive(child);
        }
    }
}

#if RO_PLATFORM_WINDOWS
bool UIWindowMgr::BuildGroundSkillPromptOverlay(ArgbDibSurface& surface, RECT client, u32 tick, RECT& bounds)
{
    bounds = {};
    auto* focused = GetFocusedGroundSkillText();
    const bool hasPopup = focused && focused->HasImePopup();
    if (hasPopup) {
        bounds = client;
    } else {
        for (const auto* prompt : m_groundSkillTextWindows) {
            if (!prompt || !prompt->m_show) continue;
            const RECT frame{prompt->m_x,prompt->m_y,prompt->m_x+prompt->m_w,prompt->m_y+prompt->m_h};
            RECT visible{};
            if (IntersectRect(&visible, &frame, &client)) UnionRect(&bounds, &bounds, &visible);
        }
    }
    const int width = bounds.right - bounds.left;
    const int height = bounds.bottom - bounds.top;
    if (width <= 0 || height <= 0) return true;
    if (!surface.EnsureSize(width, height)) return false;

    GdiFlush();
    std::fill_n(surface.GetPixels(), static_cast<std::size_t>(width) * height, 0u);
    const auto dc = surface.GetDC();
    for (auto* child : m_children) {
        auto* prompt = dynamic_cast<UIGroundSkillTextWnd*>(child);
        if (prompt && prompt->m_show && !prompt->DrawFrameToOverlay(dc, {bounds.left,bounds.top}, tick))
            return false;
    }
    if (hasPopup) {
        // Gameplay viewports start at (0, 0), so popup drawing uses client coordinates.
        focused->DrawImeCandidates(dc, client, true);
        focused->DrawImeKeystrokes(dc, client, true);
    }
    return true;
}
#endif

void UIWindowMgr::DrawVisibleWindowsToHdc(HDC targetDC, bool includeRoMap, bool includeGroundPrompts)
{
    if (!g_hMainWnd || !targetDC) {
        return;
    }

    RECT clientRect{};
    GetClientRect(g_hMainWnd, &clientRect);
    const std::vector<UIWindow*> children(m_children.begin(), m_children.end());
    for (UIWindow* child : children) {
        if (child
            && ContainsManagedWindow(*this, child)
            && child->m_show != 0
            && (includeGroundPrompts || !GroundSkillTextOwner(child))
            && (includeRoMap || child != m_roMapWnd)) {
            child->DrawToHdc(targetDC);
        }
    }
    if (includeGroundPrompts) {
        if (auto* prompt = GetFocusedGroundSkillText()) {
            prompt->DrawImeCandidates(targetDC, clientRect);
            prompt->DrawImeKeystrokes(targetDC, clientRect);
        }
    }
    if (m_itemWnd && m_itemWnd->m_show != 0) {
        m_itemWnd->DrawHoverOverlay(targetDC, clientRect);
    }
    if (m_storageWnd && m_storageWnd->m_show != 0) {
        m_storageWnd->DrawHoverOverlay(targetDC, clientRect);
    }
    // Source help balloons belong above every owner, including a second detail.
    if (m_itemInfoWnd) m_itemInfoWnd->DrawHoverOverlay(targetDC);
    if (m_cardInfoWnd) m_cardInfoWnd->DrawHoverOverlay(targetDC);
    if (includeRoMap) {
        ClearDirtyVisualState();
    } else {
        ClearDirtyVisualStateExcludingRoMap();
    }
}

void UIWindowMgr::OnDraw() {
    if (!g_hMainWnd) {
        return;
    }

    const bool hasMenuUi =
        (m_loginWnd && m_loginWnd->m_show != 0) ||
        (m_selectCharWnd && m_selectCharWnd->m_show != 0) ||
        (m_makeCharWnd && m_makeCharWnd->m_show != 0) ||
        (m_loadingWnd && m_loadingWnd->m_show != 0);

    RECT clientRect{};
    GetClientRect(g_hMainWnd, &clientRect);
    const int clientWidth = clientRect.right - clientRect.left;
    const int clientHeight = clientRect.bottom - clientRect.top;
    if (clientWidth <= 0 || clientHeight <= 0) {
        return;
    }

    const bool hasModernBackend = GetRenderDevice().GetLegacyDevice() == nullptr;
    const bool qtMenuRuntimeEnabled = hasMenuUi
        && IsQtUiRuntimeEnabled()
        && hasModernBackend;

    {
        static int s_lastLoggedUiMgrMenuMode = -1;
        const int menuMode = qtMenuRuntimeEnabled ? 1 : 0;
        if (menuMode != s_lastLoggedUiMgrMenuMode) {
            DbgLog("[UIWindowMgr] front-menu qtRuntime=%d hasMenuUi=%d modernBackend=%d size=%dx%d\n",
                qtMenuRuntimeEnabled ? 1 : 0,
                hasMenuUi ? 1 : 0,
                hasModernBackend ? 1 : 0,
                clientWidth,
                clientHeight);
            s_lastLoggedUiMgrMenuMode = menuMode;
        }
    }

    if (qtMenuRuntimeEnabled) {
        static CTexture* s_qtMenuOverlayTexture = nullptr;
        static int s_qtMenuOverlayTextureWidth = 0;
        static int s_qtMenuOverlayTextureHeight = 0;
        static std::vector<unsigned int> s_qtMenuComposePixels;
        if (!s_qtMenuOverlayTexture
            || s_qtMenuOverlayTextureWidth != clientWidth
            || s_qtMenuOverlayTextureHeight != clientHeight) {
            delete s_qtMenuOverlayTexture;
            s_qtMenuOverlayTexture = new CTexture();
            if (!s_qtMenuOverlayTexture
                || !s_qtMenuOverlayTexture->Create(clientWidth, clientHeight, PF_A8R8G8B8, false)) {
                delete s_qtMenuOverlayTexture;
                s_qtMenuOverlayTexture = nullptr;
                s_qtMenuOverlayTextureWidth = 0;
                s_qtMenuOverlayTextureHeight = 0;
            } else {
                s_qtMenuOverlayTextureWidth = clientWidth;
                s_qtMenuOverlayTextureHeight = clientHeight;
            }
        }

        if (s_qtMenuOverlayTexture
            && RenderQtUiMenuOverlayTexture(s_qtMenuOverlayTexture, clientWidth, clientHeight)
            && QueueFullScreenOverlayQuad(s_qtMenuOverlayTexture, clientWidth, clientHeight, 2.0f)) {
            QueueMenuCursorOverlayQuadForCurrentMode();
            ClearDirtyVisualState();
            g_renderer.ClearBackground();
            g_renderer.Clear(0);
            RenderWallPaper();
            g_renderer.DrawScene();
            g_renderer.Flip(false);
            return;
        }

        if (s_qtMenuOverlayTexture) {
            const size_t pixelCount = static_cast<size_t>(clientWidth) * static_cast<size_t>(clientHeight);
            if (s_qtMenuComposePixels.size() != pixelCount) {
                s_qtMenuComposePixels.assign(pixelCount, 0u);
            } else {
                std::fill(s_qtMenuComposePixels.begin(), s_qtMenuComposePixels.end(), 0u);
            }

            if (CompositeQtUiMenuOverlay(
                    s_qtMenuComposePixels.data(),
                    clientWidth,
                    clientHeight,
                    clientWidth * static_cast<int>(sizeof(unsigned int)))) {
                s_qtMenuOverlayTexture->Update(
                    0,
                    0,
                    clientWidth,
                    clientHeight,
                    s_qtMenuComposePixels.data(),
                    true,
                    clientWidth * static_cast<int>(sizeof(unsigned int)));
                if (QueueFullScreenOverlayQuad(s_qtMenuOverlayTexture, clientWidth, clientHeight, 2.0f)) {
                    QueueMenuCursorOverlayQuadForCurrentMode();
                    ClearDirtyVisualState();
                    g_renderer.ClearBackground();
                    g_renderer.Clear(0);
                    RenderWallPaper();
                    g_renderer.DrawScene();
                    g_renderer.Flip(false);
                    return;
                }
            }
        }
    }

    const bool useCompose = EnsureComposeSurface(clientWidth, clientHeight);
    HDC targetDC = nullptr;
    HDC drawDC = nullptr;
    if (useCompose) {
        drawDC = m_uiComposeSurface.GetDC();
        if (m_wallpaperSurface && m_wallpaperSurface->HasSoftwarePixels()) {
            DrawWallpaperToDC(drawDC, clientWidth, clientHeight);
        } else {
            HBRUSH clearBrush = CreateSolidBrush(RGB(0, 0, 0));
            FillRect(drawDC, &clientRect, clearBrush);
            DeleteObject(clearBrush);
        }
    } else {
        targetDC = AcquireMainWindowDrawTarget();
        if (!targetDC) {
            return;
        }
        drawDC = targetDC;
    }

    const bool qtMenuComposeFallback = useCompose && qtMenuRuntimeEnabled;
    if (!qtMenuComposeFallback) {
        DrawVisibleWindowsToHdc(drawDC, true);
    } else {
        ClearDirtyVisualState();
        CompositeQtUiMenuOverlay(
            m_uiComposeSurface.GetBits(),
            clientWidth,
            clientHeight,
            clientWidth * static_cast<int>(sizeof(unsigned int)));
        CompositeMenuCursorIntoArgbBuffer(
            static_cast<unsigned int*>(m_uiComposeSurface.GetBits()),
            clientWidth,
            clientHeight);
    }
    bool presentedModernUiFrame = false;
    if (useCompose && hasModernBackend && m_uiComposeSurface.GetBits()) {
        presentedModernUiFrame = GetRenderDevice().UpdateBackBufferFromMemory(
            m_uiComposeSurface.GetBits(),
            clientWidth,
            clientHeight,
            clientWidth * static_cast<int>(sizeof(unsigned int)));
        if (presentedModernUiFrame) {
            g_renderer.Flip(false);
        }
    }

    if (useCompose) {
        if (!presentedModernUiFrame) {
            if (!BlitArgbBitsToMainWindow(m_uiComposeSurface.GetBits(), clientWidth, clientHeight)) {
                return;
            }
        }
    } else {
        ReleaseMainWindowDrawTarget(targetDC);
    }
}

void UIWindowMgr::OnDrawExcludingRoMapToHdc(HDC targetDC, bool includeGroundPrompts)
{
    DrawVisibleWindowsToHdc(targetDC, false, includeGroundPrompts);
}

bool UIWindowMgr::DrawRoMapToHdc(HDC targetDC, int x, int y)
{
    if (!targetDC || !m_roMapWnd || m_roMapWnd->m_show == 0) {
        return false;
    }

    m_roMapWnd->DrawToHdc(targetDC, x, y);
    ClearDirtyWindowRecursive(m_roMapWnd);
    return true;
}

bool UIWindowMgr::GetRoMapRect(RECT* outRect) const
{
    if (!outRect || !m_roMapWnd || m_roMapWnd->m_show == 0) {
        return false;
    }

    outRect->left = m_roMapWnd->m_x;
    outRect->top = m_roMapWnd->m_y;
    outRect->right = m_roMapWnd->m_x + m_roMapWnd->m_w;
    outRect->bottom = m_roMapWnd->m_y + m_roMapWnd->m_h;
    return true;
}

void UIWindowMgr::RenderWallPaper() {
    m_isInvalidatedByForce = false;
    if (!m_wallpaperSurface || !g_hMainWnd) {
        return;
    }

    RECT rc{};
    GetClientRect(g_hMainWnd, &rc);
    const int w = rc.right - rc.left;
    const int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) {
        return;
    }

    m_wallpaperSurface->DrawSurfaceStretch(0, 0, w, h);
}

void UIWindowMgr::InvalidateUpdateNeededUI()
{
    if (m_isInvalidatedByForce) {
        return;
    }
    m_isInvalidatedByForce = true;
    for (UIWindow* window : m_children) {
        if (window && window->IsUpdateNeed()) {
            window->InvalidateChildren();
        }
    }
}

void UIWindowMgr::DrawWallpaperToDC(HDC targetDC, int width, int height) {
    if (!targetDC || !m_wallpaperSurface || !m_wallpaperSurface->HasSoftwarePixels() || width <= 0 || height <= 0) {
        return;
    }

    const unsigned int sourceWidth = m_wallpaperSurface->m_w;
    const unsigned int sourceHeight = m_wallpaperSurface->m_h;
    const unsigned int* sourcePixels = m_wallpaperSurface->GetSoftwarePixels();
    if (!sourcePixels || sourceWidth == 0 || sourceHeight == 0) {
        return;
    }

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = static_cast<LONG>(sourceWidth);
    bmi.bmiHeader.biHeight = -static_cast<LONG>(sourceHeight);
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    SetStretchBltMode(targetDC, HALFTONE);
    StretchDIBits(targetDC,
        0,
        0,
        width,
        height,
        0,
        0,
        static_cast<int>(sourceWidth),
        static_cast<int>(sourceHeight),
        sourcePixels,
        &bmi,
        DIB_RGB_COLORS,
        SRCCOPY);
}

void UIWindowMgr::SetWallpaper(CBitmapRes* bitmap) {
    if (!bitmap || !bitmap->m_data || bitmap->m_width <= 0 || bitmap->m_height <= 0) {
        DbgLog("[SetWallpaper] Called with null/empty bitmap, clearing surface\n");
        if (m_wallpaperSurface) {
            delete m_wallpaperSurface;
            m_wallpaperSurface = nullptr;
        }
        return;
    }

    if (!m_wallpaperSurface) {
        m_wallpaperSurface = g_3dDevice.CreateWallPaper(bitmap->m_width, bitmap->m_height);
        DbgLog("[SetWallpaper] CreateWallPaper(%d,%d) -> %p\n",
               bitmap->m_width, bitmap->m_height, (void*)m_wallpaperSurface);
    }

    if (!m_wallpaperSurface) {
        DbgLog("[SetWallpaper] FAIL: surface is null after CreateWallPaper\n");
        return;
    }

    DbgLog("[SetWallpaper] Calling Update with %dx%d pixels\n",
           bitmap->m_width, bitmap->m_height);
    m_wallpaperSurface->Update(0, 0, bitmap->m_width, bitmap->m_height, bitmap->m_data, false, bitmap->m_width * 4);
    DbgLog("[SetWallpaper] Update done, hasPixels=%d\n", m_wallpaperSurface->HasSoftwarePixels() ? 1 : 0);
}

bool UIWindowMgr::SetWallpaperFromGameData(const std::string& wallpaperName) {
    const std::vector<std::string> candidates = BuildWallpaperCandidates(wallpaperName);
    std::vector<std::string> fallbackCandidates;
    LOG_WALLPAPER_LOAD("[WallpaperLoad] Searching for wallpaper '%s', %zu candidates\n",
           wallpaperName.c_str(), candidates.size());

    for (const std::string& candidate : candidates) {
        u32* pixels = nullptr;
        int width = 0;
        int height = 0;
        if (!LoadBgraPixelsFromGameData(candidate.c_str(), &pixels, &width, &height)
            || !pixels
            || width <= 0
            || height <= 0) {
            LOG_WALLPAPER_LOAD("[WallpaperLoad]   MISS: %s\n", candidate.c_str());
            const std::string fallback = ResolveArchiveWallpaperByFileName(candidate);
            if (!fallback.empty()) {
                AddUniqueCandidate(fallbackCandidates, fallback);
            }
            delete[] pixels;
            continue;
        }
        LOG_WALLPAPER_LOAD("[WallpaperLoad]   HIT:  %s (%dx%d)\n", candidate.c_str(), width, height);

        CBitmapRes bitmap;
        bitmap.m_width = width;
        bitmap.m_height = height;
        bitmap.m_data = pixels;

        SetWallpaper(&bitmap);
        bitmap.m_data = nullptr;
        delete[] pixels;
        m_loadedWallpaperPath = candidate;
        LOG_WALLPAPER_LOAD("[WallpaperLoad] SUCCESS: loaded '%s'\n", candidate.c_str());
        return true;
    }

    for (const std::string& candidate : fallbackCandidates) {
        u32* pixels = nullptr;
        int width = 0;
        int height = 0;
        if (!LoadBgraPixelsFromGameData(candidate.c_str(), &pixels, &width, &height)
            || !pixels
            || width <= 0
            || height <= 0) {
            delete[] pixels;
            continue;
        }

        CBitmapRes bitmap;
        bitmap.m_width = width;
        bitmap.m_height = height;
        bitmap.m_data = pixels;

        SetWallpaper(&bitmap);
        bitmap.m_data = nullptr;
        delete[] pixels;
        m_loadedWallpaperPath = candidate;
        DbgLog("[WallpaperLoad] BASENAME HIT: %s\n", candidate.c_str());
        return true;
    }

    LOG_WALLPAPER_LOAD("[WallpaperLoad] FAIL: no candidate resolved (tried %zu)\n", candidates.size());
    m_loadedWallpaperPath.clear();
    SetWallpaper(nullptr);
    return false;
}

void UIWindowMgr::ShowLoadingScreen(const std::string& wallpaperName, const std::string& message, float progress)
{
    const bool loadingAlreadyVisible = IsLoadingScreenVisible(*this);
    if (!wallpaperName.empty() && !loadingAlreadyVisible) {
        SetWallpaperFromGameData(wallpaperName);
    }

    UILoadingWnd* loadingWnd = m_loadingWnd ? m_loadingWnd : static_cast<UILoadingWnd*>(MakeWindow(WID_LOADINGWND));
    if (!loadingWnd) {
        return;
    }

    loadingWnd->SetMessage(message);
    loadingWnd->SetProgress(progress);
    loadingWnd->SetShow(1);
}

void UIWindowMgr::UpdateLoadingScreen(const std::string& message, float progress)
{
    if (!m_loadingWnd) {
        return;
    }

    m_loadingWnd->SetMessage(message);
    m_loadingWnd->SetProgress(progress);
    m_loadingWnd->SetShow(1);
}

void UIWindowMgr::HideLoadingScreen()
{
    if (m_loadingWnd) {
        m_loadingWnd->SetShow(0);
    }
    m_loadedWallpaperPath.clear();
    SetWallpaper(nullptr);
}

bool UIWindowMgr::OpenSourceStartupTip()
{
    if(m_tipStartupChecked)return false;m_tipStartupChecked=true;
    if(!g_showTipsAtStartup || !g_tipOfTheDay.GetNumTip())return false;
    return MakeWindow(WID_TIPOFTHEDAYWND)!=nullptr;
}

void UIWindowMgr::InitializeSourceCommandFlags()
{
    g_session.InitializeSourceCommandFlags(CAudio::GetInstance()->IsBgmPaused(),g_isSoundOn,m_tipOfTheDayWnd!=nullptr);
}

void UIWindowMgr::SendMsg(int msg, msgparam_t wparam, msgparam_t lparam, msgparam_t noticeType, msgparam_t talker) {
    if(msg==6){if(m_chatWnd)m_chatWnd->WriteChatToFile();return;}
    if(msg==7 || msg==18){if(m_chatRoomWnd)m_chatRoomWnd->WriteChatToFile(msg==18);return;}
    // Selected manager17 has no case: main /report intentionally has no action.
    if(msg==17)return;
    if(msg==14){if(wparam)OpenWhisperWindow(reinterpret_cast<const char*>(wparam),reinterpret_cast<const char*>(lparam));return;}
    if(msg==5){
        if(m_chatRoomWnd && wparam)m_chatRoomWnd->SendMsg(nullptr,37,wparam,lparam,0);
        return;
    }
    if ((msg == 1 || msg == 20) && wparam) {
        const auto color = static_cast<u32>(lparam) != 0 ? static_cast<u32>(lparam) : 0x00ffffffu;
        PushSourceChatEvent(reinterpret_cast<const char*>(wparam), ro::chat::ColorRefToUiRgb(color), 6,
            msg == 1 ? ro::chat::NoticeRoute::Normal : ro::chat::NoticeRoute::Command,
            static_cast<ro::chat::NoticeType>(noticeType), reinterpret_cast<const char*>(talker));
        return;
    }
    // Original manager2/3 are state/show operations, distinct from notices.
    if (msg == 2) {
        if (m_chatWnd) m_chatWnd->SendMsg(nullptr, 15, wparam, 0, 0);
        return;
    }
    if (msg == 3) {
        // Original ShowWindow(WID_CHATWND,1) creates a missing main window.
        EnsureChatWindowVisible();
        m_chatWndShow = 1;
        m_chatWnd->SendMsg(nullptr, 16, 0, 0, 0);
        return;
    }
    if (msg != kUiChatEventMsg || wparam == 0) {
        return;
    }

    const char* text = reinterpret_cast<const char*>(wparam);
    const u32 color = static_cast<u32>(lparam) & 0x00FFFFFFu;
    const u8 channel = static_cast<u8>((static_cast<u32>(lparam) >> 24) & 0xFFu);
    PushChatEvent(text, color, channel, GetTickCount());
}

void UIWindowMgr::PushSourceChatEvent(const char* text, u32 uiRgb, u8 channel, ro::chat::NoticeRoute route,
    ro::chat::NoticeType type, const char* talker, u32 tick)
{
    if (!text || route == ro::chat::NoticeRoute::Unspecified) return;
    uiRgb = uiRgb ? uiRgb & 0x00ffffffu : 0x00ffffffu;
    if (m_chatWnd) {
        if (ro::chat::IsNoticeSentinel(text) || !m_chatWnd->ReceiveSourceNotice(text, uiRgb, route, type, talker)) return;
    } else {
        // Source backlog retains only split text/color, without route/type/talker.
        for (auto& line : ro::chat::SplitNotice(text)) m_sourceChatBacklog.push_back({std::move(line), uiRgb, {}});
    }
    PushChatEvent(text, uiRgb, channel, tick, {route, type, talker ? talker : "", m_chatWnd ? m_sourceChatGeneration : 0});
}

void UIWindowMgr::PushChatEvent(const char* text, u32 color, u8 channel, u32 tick, ro::chat::NoticeSource source)
{
    if (!text || *text == '\0') {
        return;
    }

    UIChatEvent event{};
    event.text = text;
    event.color = color & 0x00FFFFFFu;
    event.channel = channel;
    event.tick = tick != 0 ? tick : GetTickCount();
    event.source = std::move(source);

    if (m_chatEvents.size() >= kMaxChatEvents) {
        m_chatEvents.erase(m_chatEvents.begin());
    }
    m_chatEvents.push_back(std::move(event));

    if (m_chatWnd) {
        const UIChatEvent& storedEvent = m_chatEvents.back();
        m_chatWnd->AddChatLine(storedEvent.text.c_str(), color, channel, storedEvent.tick);
    }
}

void UIWindowMgr::OnLBtnDown(int x, int y)
{
    if(m_petInfoWnd && m_petInfoWnd->GetCommandMenu()){
        const auto* menu=m_petInfoWnd->GetCommandMenu();
        if(x<menu->m_x || x>=menu->m_x+menu->m_w || y<menu->m_y || y>=menu->m_y+menu->m_h)m_petInfoWnd->CloseCommandMenu();
    }
    UIWindow* hit = HitTestWindow(x, y);

    if (m_playerContextMenuWnd && m_playerContextMenuWnd->m_show != 0) {
        UIWindow* topLevel = hit;
        while (topLevel && topLevel->m_parent) {
            topLevel = topLevel->m_parent;
        }
        if (topLevel != m_playerContextMenuWnd) {
            m_playerContextMenuWnd->HideMenu();
            hit = HitTestWindow(x, y);
        }
    }

    if(hit==m_chatWnd && (m_editWindow==m_chatRoomWnd || m_editWindow==m_chatRoomPasswordWnd || m_editWindow==m_chatRoomMakeWnd || m_editWindow==m_chatRoomChangeWnd || m_editWindow==m_autoMessageWnd || m_editWindow==m_tipOfTheDayWnd || dynamic_cast<UIWhisperWnd*>(m_editWindow)))m_editWindow=nullptr;
    m_captureWindow = hit;

    if (hit) {
        UIWindow* topLevel = hit;
        while (topLevel && topLevel->m_parent) {
            topLevel = topLevel->m_parent;
        }
        if (topLevel) {
            if(dynamic_cast<UITipOfTheDayWnd*>(topLevel)){m_editWindow=topLevel;if(m_chatWnd)m_chatWnd->ClearInputFocus();}
            auto found = std::find(m_children.begin(), m_children.end(), topLevel);
            if (found != m_children.end() && std::next(found) != m_children.end()) {
                m_children.erase(found);
                m_children.push_back(topLevel);
            }
        }

        // Ground prompts handle their own focus transition.
        if (hit->CanReceiveKeyInput() && !GroundSkillTextOwner(hit)) {
            m_editWindow = hit;
        }
        hit->OnLBtnDown(x, y);
        if (g_hMainWnd && (hit == m_chatWnd || hit == m_subChatWndST || hit == m_subChatWndBT || hit == m_subChatMiniWnd || hit == m_battleMsgOptionWnd || hit == m_chatRoomWnd || hit == m_chatRoomPasswordWnd || hit==m_chatRoomMakeWnd || hit==m_chatRoomChangeWnd || hit==m_roomMessageBox || hit==m_chatRoomMenuWnd || hit==m_autoMessageWnd || IsWhisperPreferenceInputWindow(hit) || dynamic_cast<UIWhisperWnd*>(hit)))
            SetCapture(g_hMainWnd);
    }
}

void UIWindowMgr::OnLBtnDblClk(int x, int y)
{
    UIWindow* hit = HitTestWindow(x, y);
    if (m_playerContextMenuWnd && m_playerContextMenuWnd->m_show != 0) {
        UIWindow* topLevel = hit;
        while (topLevel && topLevel->m_parent) {
            topLevel = topLevel->m_parent;
        }
        if (topLevel != m_playerContextMenuWnd) {
            m_playerContextMenuWnd->HideMenu();
            hit = HitTestWindow(x, y);
        }
    }
    if (!hit) {
        return;
    }

    m_captureWindow = hit;

    UIWindow* topLevel = hit;
    while (topLevel && topLevel->m_parent) {
        topLevel = topLevel->m_parent;
    }
    if (topLevel) {
        auto found = std::find(m_children.begin(), m_children.end(), topLevel);
        if (found != m_children.end() && std::next(found) != m_children.end()) {
            m_children.erase(found);
            m_children.push_back(topLevel);
        }
    }

    if (hit->CanReceiveKeyInput()) {
        m_editWindow = hit;
    }
    hit->OnLBtnDblClk(x, y);
}

void UIWindowMgr::OnLBtnUp(int x, int y)
{
    const bool chatCapture = m_captureWindow && (m_captureWindow == m_chatWnd || m_captureWindow == m_subChatWndST
        || m_captureWindow == m_subChatWndBT || m_captureWindow == m_subChatMiniWnd || m_captureWindow == m_battleMsgOptionWnd || m_captureWindow == m_chatRoomWnd || m_captureWindow == m_chatRoomPasswordWnd || m_captureWindow==m_chatRoomMakeWnd || m_captureWindow==m_chatRoomChangeWnd || m_captureWindow==m_roomMessageBox || m_captureWindow==m_chatRoomMenuWnd || m_captureWindow==m_autoMessageWnd || IsWhisperPreferenceInputWindow(m_captureWindow) || dynamic_cast<UIWhisperWnd*>(m_captureWindow));
    if (m_captureWindow) {
        m_captureWindow->OnLBtnUp(x, y);
        m_captureWindow = nullptr;
    }

    if (chatCapture && g_hMainWnd && GetCapture() == g_hMainWnd) ReleaseCapture();
    if (CGameMode* gameMode = g_modeMgr.GetCurrentGameMode()) {
        if (gameMode->m_dragType != static_cast<int>(DragType::None)) {
            UIWindow* dropTarget = HitTestWindow(x, y);
            if (gameMode->m_dragInfo.source == static_cast<int>(DragSource::ProductionMaterial)) {
                if (m_skillSelectWnd) m_skillSelectWnd->FinishMaterialDrag(x, y, gameMode->m_dragInfo.itemIndex);
            } else if (dropTarget) {
                dropTarget->DragAndDrop(x, y, &gameMode->m_dragInfo);
            } else if (gameMode->m_dragInfo.type == static_cast<int>(DragType::ShortcutItem)
                && gameMode->m_dragInfo.source == static_cast<int>(DragSource::InventoryWindow)
                && gameMode->m_dragInfo.itemIndex != 0) {
                const int dropAmount = (std::max)(1, gameMode->m_dragInfo.itemCount);
                if (dropAmount > 1) {
                    if (auto* inputWnd = static_cast<UINpcInputWnd*>(MakeWindow(WID_NPCINPUTWND))) {
                        inputWnd->OpenGameNumberPrompt(
                            "Enter amount to drop",
                            CGameMode::GameMsg_RequestDropInventoryItem,
                            static_cast<msgparam_t>(gameMode->m_dragInfo.itemIndex),
                            static_cast<u32>(dropAmount),
                            static_cast<u32>(dropAmount),
                            nullptr,
                            gameMode->m_dragInfo.itemId);
                    }
                } else {
                    g_modeMgr.SendMsg(
                        CGameMode::GameMsg_RequestDropInventoryItem,
                        gameMode->m_dragInfo.itemIndex,
                        dropAmount,
                        gameMode->m_dragInfo.itemId);
                }
            }
            if (gameMode->m_dragInfo.source == static_cast<int>(DragSource::ShortcutWindow)
                && gameMode->m_dragInfo.shortcutSlotAbsoluteIndex >= 0
                && dropTarget != m_shortCutWnd) {
                if (g_session.ClearShortcutSlotByAbsoluteIndex(gameMode->m_dragInfo.shortcutSlotAbsoluteIndex)) {
                    g_modeMgr.SendMsg(
                        CGameMode::GameMsg_RequestShortcutUpdate,
                        gameMode->m_dragInfo.shortcutSlotAbsoluteIndex,
                        0,
                        0);
                }
            }
            gameMode->m_dragType = static_cast<int>(DragType::None);
            gameMode->m_dragInfo = DRAG_INFO{};
            if (m_itemWnd) {
                m_itemWnd->Invalidate();
            }
            if (m_equipWnd) {
                m_equipWnd->Invalidate();
            }
            if (m_skillListWnd) {
                m_skillListWnd->Invalidate();
            }
            if (m_shortCutWnd) {
                m_shortCutWnd->Invalidate();
            }
        }
    }
}

void UIWindowMgr::OnRBtnDown(int x, int y)
{
    UIWindow* hit = HitTestWindow(x, y);
    if (m_playerContextMenuWnd && m_playerContextMenuWnd->m_show != 0) {
        UIWindow* topLevel = hit;
        while (topLevel && topLevel->m_parent) {
            topLevel = topLevel->m_parent;
        }
        if (topLevel != m_playerContextMenuWnd) {
            m_playerContextMenuWnd->HideMenu();
            hit = HitTestWindow(x, y);
        }
    }
    m_captureWindow = hit;

    if (!hit) {
        return;
    }

    UIWindow* topLevel = hit;
    while (topLevel && topLevel->m_parent) {
        topLevel = topLevel->m_parent;
    }
    if (topLevel) {
        auto found = std::find(m_children.begin(), m_children.end(), topLevel);
        if (found != m_children.end() && std::next(found) != m_children.end()) {
            m_children.erase(found);
            m_children.push_back(topLevel);
        }
    }

    if (hit->CanReceiveKeyInput()) {
        m_editWindow = hit;
    }
    hit->OnRBtnDown(x, y);
    if(g_hMainWnd && dynamic_cast<UIWhisperWnd*>(hit))SetCapture(g_hMainWnd);
}

void UIWindowMgr::OnRBtnUp(int x, int y)
{
    if (!m_captureWindow) {
        return;
    }

    const bool whisperCapture=dynamic_cast<UIWhisperWnd*>(m_captureWindow)!=nullptr;
    m_captureWindow->OnRBtnUp(x, y);
    m_captureWindow = nullptr;
    if(whisperCapture && g_hMainWnd && GetCapture()==g_hMainWnd)ReleaseCapture();
}

void UIWindowMgr::OnMouseMove(int x, int y)
{
    if (m_captureWindow) {
        m_captureWindow->OnMouseMove(x, y);
        return;
    }

    UIWindow* hit = HitTestWindow(x, y);

    if (m_lastHitWindow != hit) {
        if (m_lastHitWindow) {
            m_lastHitWindow->OnMouseHover(-1, -1);
        }
        if (hit) {
            hit->OnMouseHover(x, y);
        }
        m_lastHitWindow = hit;
    }

    if (hit) {
        hit->OnMouseMove(x, y);
    }
}

bool UIWindowMgr::OnWheel(int x, int y, int delta)
{
    if (m_captureWindow) {
        m_captureWindow->OnWheel(delta);
        return true;
    }

    UIWindow* hit = HitTestWindow(x, y);
    if (!hit) {
        return false;
    }

    hit->OnWheel(delta);
    return true;
}

void UIWindowMgr::ShowItemInfoWindow(const ITEM_INFO& item, int preferredX, int preferredY,
    ItemInfoSource source, ItemInfoAction action)
{
    // Test the pre-open visibility: MakeWindow shows a retained hidden window.
    // Reopening such a window corresponds to a fresh source instance, not a
    // second selection of the previously visible item.
    if (action == ItemInfoAction::Select && m_itemInfoWnd && m_itemInfoWnd->m_show != 0
        && m_itemInfoWnd->MatchesSelection(item)) {
        DeleteWindow(m_itemInfoWnd);
        return;
    }
    UIItemInfoWnd* const window = static_cast<UIItemInfoWnd*>(MakeWindow(WID_ITEMINFOWND));
    if (!window) {
        return;
    }
    window->SetItemInfo(item, preferredX, preferredY, source);
}

void UIWindowMgr::ShowCardInfoWindow(const ITEM_INFO& item, ItemInfoAction action)
{
    if (action==ItemInfoAction::Select && m_cardInfoWnd && m_cardInfoWnd->m_show
        && m_cardInfoWnd->MatchesSelection(item)) {
        DeleteWindow(m_cardInfoWnd);
        return;
    }
    auto* window=static_cast<UIItemInfoWnd*>(MakeWindow(WID_CARDINFOWND));
    if (window) window->SetItemInfo(item,0,0,ItemInfoSource::Snapshot);
}

void UIWindowMgr::ShowItemObtainNotification(const ITEM_INFO& item)
{
    auto* window=static_cast<UINotifyItemObtainWnd*>(MakeWindow(WID_NOTIFYITEMOBTAINWND));
    if(window)window->SetItem(item,timeGetTime());
}

void UIWindowMgr::ShowItemCollectionWindow(const ITEM_INFO& item, int preferredX, int preferredY)
{
    UIItemCollectionWnd* const window = static_cast<UIItemCollectionWnd*>(MakeWindow(WID_ITEMCOLLECTIONWND));
    if (!window) {
        return;
    }
    window->SetItemInfo(item, preferredX, preferredY);
}

void UIWindowMgr::ShowSkillDescribeWindow(const PLAYER_SKILL_INFO& skillInfo, int preferredX, int preferredY)
{
    UISkillDescribeWnd* const window = static_cast<UISkillDescribeWnd*>(MakeWindow(WID_SKILLDESCRIBEWND));
    if (!window) {
        return;
    }
    window->SetSkillInfo(skillInfo, preferredX, preferredY);
}

UIWindow* UIWindowMgr::HitTestWindow(int x, int y) const
{
    if(m_modalWindow && m_modalWindow->m_show)return m_modalWindow->HitTestDeep(x,y);
    for (auto it = m_children.rbegin(); it != m_children.rend(); ++it) {
        UIWindow* hit = (*it)->HitTestDeep(x, y);
        if (hit) {
            return hit;
        }
    }

    return nullptr;
}

bool UIWindowMgr::HasWindowAtPoint(int x, int y) const
{
    return (m_modalWindow && m_modalWindow->m_show) || ro::commerce::HitTest(x,y) || HitTestWindow(x, y) != nullptr;
}

bool UIWindowMgr::HasGameplayInputModal(bool allowProductionInventory) const
{
    if((m_modalWindow && m_modalWindow->m_show) || HasActiveNpcDialog()||IsCapturingShortcutKeyBinding())return true;
    if (m_skillSelectWnd && m_skillSelectWnd->m_show
        && !(allowProductionInventory && m_skillSelectWnd->IsProductionConfirmation())) return true;
    const UIWindow* windows[]={m_optionWnd,m_waitWnd,m_loadingWnd,m_joinPartyAcceptWnd,m_chooseSellBuyWnd};
    for(const auto* w:windows)if(w&&w->m_show)return true;
    return false;
}
bool UIWindowMgr::HasNativePopupAtPoint(int x,int y) const
{
    if (m_skillSelectWnd && m_skillSelectWnd->IsProductionConfirmation()
        && HitTestWindow(x, y) == m_skillSelectWnd) return true;
    const UIWindow* windows[]={m_itemInfoWnd,m_cardInfoWnd,m_skillDescribeWnd,m_chatRoomMenuWnd};
    for(const auto* w:windows)if(w&&w->m_show&&x>=w->m_x&&y>=w->m_y&&x<w->m_x+w->m_w&&y<w->m_y+w->m_h)return true;
    if (m_auctionWnd || m_homunInfoWnd || m_merInfoWnd || m_homunSkillListWnd || m_merSkillListWnd) {
        for (const auto* hit = HitTestWindow(x, y); hit; hit = hit->m_parent) {
            if (hit == m_auctionWnd || hit == m_homunInfoWnd || hit == m_merInfoWnd
                || hit == m_homunSkillListWnd || hit == m_merSkillListWnd) return true;
        }
    }
    return false;
}

UIGroundSkillTextWnd* UIWindowMgr::GetFocusedGroundSkillText() const
{
    if (HasGameplayInputModal() || HasFrontMenuUiVisible()) return nullptr;
    auto* prompt = GroundSkillTextOwner(m_editWindow);
    return prompt && prompt->m_show ? prompt : nullptr;
}

UIEditCtrl* UIWindowMgr::GetFocusedCompanionNameEdit() const
{
    if (!m_editWindow || !m_homunInfoWnd || !m_homunInfoWnd->m_show
        || (m_modalWindow && m_modalWindow->m_show)) return nullptr;
    auto* edit = m_homunInfoWnd->GetNameEdit();
    return edit == m_editWindow ? edit : nullptr;
}

void UIWindowMgr::RefreshGroundPromptSkins()
{
    for (auto* prompt : m_groundSkillTextWindows)
        if (prompt) prompt->RefreshSkin();
}

void UIWindowMgr::OnChar(char c)
{
    if(m_modalWindow && m_modalWindow->m_show){m_modalWindow->OnChar(c);return;}
    if (auto* edit = GetFocusedCompanionNameEdit()) {
        edit->OnChar(c);
        return;
    }
    if(m_petInfoWnd && m_editWindow)for(auto* focus=m_editWindow;focus;focus=focus->m_parent)
        if(focus==m_petInfoWnd){m_editWindow->OnChar(c);return;}
    if (c == '\r' || c == '\n') {
        DbgLog("[UI] char c=0x%02X chat=%p login=%p edit=%p\n",
            static_cast<unsigned char>(c),
            static_cast<void*>(m_chatWnd),
            static_cast<void*>(m_loginWnd),
            static_cast<void*>(m_editWindow));
    }

    if (m_npcInputWnd && m_npcInputWnd->m_show != 0) {
        if (UIEditCtrl* editCtrl = m_npcInputWnd->GetEditCtrl()) {
            m_editWindow = editCtrl;
            editCtrl->OnChar(c);
        }
        return;
    }

    if (m_joinPartyAcceptWnd && m_joinPartyAcceptWnd->m_show != 0) {
        return;
    }

    if (m_playerContextMenuWnd && m_playerContextMenuWnd->m_show != 0) {
        return;
    }

    if (HasActiveNpcDialog()) {
        return;
    }

    if(m_autoMessageWnd && m_autoMessageWnd->m_show && m_editWindow==m_autoMessageWnd){m_autoMessageWnd->OnChar(c);return;}
    if(auto* whisper=dynamic_cast<UIWhisperWnd*>(m_editWindow)){if(whisper->m_show){whisper->OnChar(c);return;}}
    if(auto* settings=dynamic_cast<UIChatRoomSettingsWnd*>(m_editWindow)){if(settings->m_show){settings->OnChar(c);return;}}
    if(m_chatRoomPasswordWnd && m_chatRoomPasswordWnd->m_show && m_editWindow==m_chatRoomPasswordWnd){m_chatRoomPasswordWnd->OnChar(c);return;}
    if(m_chatRoomWnd && m_chatRoomWnd->m_show && m_editWindow==m_chatRoomWnd){m_chatRoomWnd->OnChar(c);return;}
    if (m_chatWnd && m_chatWnd->m_show != 0 && m_chatWnd->HandleChar(c)) {
        return;
    }

    if (m_editWindow) {
        m_editWindow->OnChar(c);
    }
}

bool UIWindowMgr::HasFrontMenuUiVisible() const
{
    return (m_loginWnd && m_loginWnd->m_show != 0)
        || (m_selectCharWnd && m_selectCharWnd->m_show != 0)
        || (m_makeCharWnd && m_makeCharWnd->m_show != 0);
}

void UIWindowMgr::SetInstanceWindowVisible(bool visible)
{
    g_session.m_instanceStatus.showWindow = visible && !g_session.m_instanceStatus.name.empty();
    if (g_session.m_instanceStatus.showWindow) {
        MakeWindow(WID_INSTANCWND);
    } else if (m_instanceWnd) {
        m_instanceWnd->SetShow(0);
    }
}

bool UIWindowMgr::HandleHotkeyBeforeFocusedUi(int virtualKey, bool isAltDown, bool isCtrlDown, bool hasFrontMenuUi)
{
    if (virtualKey == 'R' && (isAltDown || isCtrlDown) && !hasFrontMenuUi && m_basicInfoWnd
        && !HasGameplayInputModal()) {
        bool handled = false;
        if (isAltDown && g_session.m_homunStatus.alive) {
            if (m_homunInfoWnd) DeleteWindow(m_homunInfoWnd);
            else MakeWindow(WID_HOMUNINFOWND);
            handled = true;
        }
        if (isCtrlDown && g_session.m_merStatus.alive) {
            if (m_merInfoWnd) DeleteWindow(m_merInfoWnd);
            else MakeWindow(WID_MERINFOWND);
            handled = true;
        }
        if (handled) return true;
    }
    if(isAltDown && virtualKey=='J' && !hasFrontMenuUi)return ProcessSourceCommandWindowKey(74);
    if (virtualKey==VK_ESCAPE && !hasFrontMenuUi && m_battleMsgOptionWnd && m_battleMsgOptionWnd->m_show
        && !HasGameplayInputModal() && CanRouteQtChatInput()) {
        m_battleMsgOptionWnd->SendMsg(nullptr,6,135,0,0);
        return true;
    }
    if (isCtrlDown && virtualKey == VK_TAB && !hasFrontMenuUi) {
        ToggleWindow(WID_ROMAPWND);
        return true;
    }

    if (isAltDown && !hasFrontMenuUi) {
        switch (virtualKey) {
        case 'B':
            if (HasGameplayInputModal()) return false;
            SetInstanceWindowVisible(!g_session.m_instanceStatus.showWindow);
            return true;
        case 'O':
            ToggleWindow(WID_OPTIONWND);
            return true;
        case 'C':
            OpenChatRoomMake(true);
            return true;
        case 'A':
            ToggleWindow(WID_STATUSWND);
            return true;
        case 'E':
            ToggleWindow(WID_ITEMWND);
            return true;
        case 'Q':
            ToggleWindow(WID_EQUIPWND);
            return true;
        case 'S':
            ToggleWindow(WID_SKILLLISTWND);
            return true;
        case 'Z':
            ToggleWindow(WID_MESSENGERGROUPWND);
            return true;
        case 'G':
            ToggleWindow(WID_GUILDWND);
            return true;
        default:
            break;
        }
    }

    return false;
}

bool UIWindowMgr::HandleHotkeyAfterFocusedUi(int virtualKey, bool isAltDown, bool isCtrlDown, bool isShiftDown, bool hasFrontMenuUi)
{
    if (!hasFrontMenuUi) {
        const unsigned char modifiers = static_cast<unsigned char>(
            (isCtrlDown ? kShortcutKeyModifierControl : 0)
            | (isShiftDown ? kShortcutKeyModifierShift : 0)
            | (isAltDown ? kShortcutKeyModifierAlt : 0));
        for (int slot = 0; slot < kShortcutKeyBindingCount; ++slot) {
            const ShortcutKeyBinding& binding = m_shortcutKeyBindings[static_cast<size_t>(slot)];
            if (binding.virtualKey != virtualKey || binding.modifiers != modifiers) {
                continue;
            }
            g_modeMgr.SendMsg(CGameMode::GameMsg_RequestShortcutUse, slot, 0, 0);
            return true;
        }
    }

    if (virtualKey == 'Q' && !isAltDown && !isCtrlDown && !isShiftDown && !hasFrontMenuUi && !m_editWindow) {
        ToggleWindow(WID_QUESTWND);
        return true;
    }

    if (virtualKey == VK_ESCAPE && !hasFrontMenuUi && !(m_autoMessageWnd && m_autoMessageWnd->m_show)) {
        UIWindow* chooseWnd = MakeWindow(WID_CHOOSEWND);
        if (chooseWnd) {
            chooseWnd->SetShow(1);
        }
        return true;
    }

    if (virtualKey == VK_INSERT && !hasFrontMenuUi) {
        g_modeMgr.SendMsg(CGameMode::GameMsg_ToggleSitStand, 0, 0, 0);
        return true;
    }

    return false;
}

bool UIWindowMgr::HasBlockingUiForGameplayHotkeys() const
{
    return (GroundSkillTextOwner(m_editWindow) && GroundSkillTextOwner(m_editWindow)->m_show)
        || (m_autoMessageWnd && m_autoMessageWnd->m_show != 0)
        || (m_chatRoomPasswordWnd && m_chatRoomPasswordWnd->m_show != 0 && m_editWindow==m_chatRoomPasswordWnd)
        || (m_npcInputWnd && m_npcInputWnd->m_show != 0)
        || (m_joinPartyAcceptWnd && m_joinPartyAcceptWnd->m_show != 0)
        || (m_npcMenuWnd && m_npcMenuWnd->m_show != 0)
        || (m_playerContextMenuWnd && m_playerContextMenuWnd->m_show != 0)
        || (m_sayDialogWnd && m_sayDialogWnd->m_show != 0)
        || (m_itemPurchaseWnd && m_itemPurchaseWnd->m_show != 0)
        || (m_itemSellWnd && m_itemSellWnd->m_show != 0)
        || (m_itemShopWnd && m_itemShopWnd->m_show != 0)
        || (m_storageWnd && m_storageWnd->m_show != 0)
        || (m_chooseSellBuyWnd && m_chooseSellBuyWnd->m_show != 0)
        || (m_chooseWnd && m_chooseWnd->m_show != 0)
        || (m_optionWnd && m_optionWnd->m_show != 0)
        || (m_skillSelectWnd && m_skillSelectWnd->m_show != 0);
}

bool UIWindowMgr::OnQtKeyDown(int virtualKey, bool isAltDown, bool isCtrlDown, bool isShiftDown)
{
    if(m_modalWindow && m_modalWindow->m_show){m_modalWindow->OnKeyDown(virtualKey);return true;}
    if (IsCapturingShortcutKeyBinding()) {
        return m_optionWnd->CaptureShortcutKey(virtualKey, isAltDown, isCtrlDown, isShiftDown);
    }

    const bool hasFrontMenuUi = HasFrontMenuUiVisible();
    if (HandleHotkeyBeforeFocusedUi(virtualKey, isAltDown, isCtrlDown, hasFrontMenuUi)) {
        return true;
    }

    if (auto* edit = GetFocusedCompanionNameEdit(); edit && virtualKey == VK_BACK) {
        edit->OnKeyDown(virtualKey);
        return true;
    }

    if (m_storageWnd && m_storageWnd->m_show != 0 && virtualKey == VK_ESCAPE) {
        return g_modeMgr.SendMsg(CGameMode::GameMsg_RequestStorageClose, 0, 0, 0) != 0;
    }

    if(auto* ground=GroundSkillTextOwner(m_editWindow)){if(ground->m_show){if(!isAltDown && !isCtrlDown)ground->OnKeyDown(virtualKey);return true;}}
    if(m_autoMessageWnd && m_autoMessageWnd->m_show){if(!isAltDown && !isCtrlDown)m_autoMessageWnd->OnKeyDown(virtualKey);return true;}
    if(auto* tip=dynamic_cast<UITipOfTheDayWnd*>(m_editWindow)){if(tip->m_show){if(!isAltDown && !isCtrlDown)tip->OnKeyDown(virtualKey);return true;}}
    if(auto* whisper=dynamic_cast<UIWhisperWnd*>(m_editWindow)){if(whisper->m_show){if(!isAltDown && !isCtrlDown)whisper->OnKeyDown(virtualKey);return true;}}
    if(auto* settings=dynamic_cast<UIChatRoomSettingsWnd*>(m_editWindow)){
        if(settings->m_show){if(!isAltDown && !isCtrlDown)settings->OnKeyDown(virtualKey);return true;}
    }
    if(m_chatRoomPasswordWnd && m_chatRoomPasswordWnd->m_show && m_editWindow==m_chatRoomPasswordWnd){
        if(!isAltDown && !isCtrlDown)m_chatRoomPasswordWnd->OnKeyDown(virtualKey);
        return true;
    }
    if(m_chatRoomWnd && m_chatRoomWnd->m_show && m_editWindow==m_chatRoomWnd){
        if(!isAltDown && !isCtrlDown)m_chatRoomWnd->OnKeyDown(virtualKey);
        return true;
    }
    if (HasBlockingUiForGameplayHotkeys()) {
        return false;
    }

    if (!hasFrontMenuUi && m_chatWnd && m_chatWnd->IsInputActive()) {
        return false;
    }

    return HandleHotkeyAfterFocusedUi(virtualKey, isAltDown, isCtrlDown, isShiftDown, hasFrontMenuUi);
}

void UIWindowMgr::OnKeyDown(int virtualKey)
{
    if(m_modalWindow && m_modalWindow->m_show){m_modalWindow->OnKeyDown(virtualKey);return;}
    const bool hasFrontMenuUi = HasFrontMenuUiVisible();
    const bool isAltDown = (GetKeyState(VK_MENU) & 0x8000) != 0;
    const bool isCtrlDown = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    const bool isShiftDown = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

    if (IsCapturingShortcutKeyBinding()) {
        m_optionWnd->CaptureShortcutKey(virtualKey, isAltDown, isCtrlDown, isShiftDown);
        return;
    }

    if (HandleHotkeyBeforeFocusedUi(virtualKey, isAltDown, isCtrlDown, hasFrontMenuUi)) {
        return;
    }

    if(auto* menu=dynamic_cast<UIPetCommandMenu*>(m_editWindow)){menu->OnKeyDown(virtualKey);return;}
    if (GetFocusedCompanionNameEdit()) return; // The native character event owns text and Backspace.
    if(m_petInfoWnd && m_editWindow){
        for(auto* focus=m_editWindow;focus;focus=focus->m_parent)if(focus==m_petInfoWnd){m_editWindow->OnKeyDown(virtualKey);return;}
    }

    if (virtualKey == VK_RETURN || virtualKey == VK_ESCAPE) {
        DbgLog("[UI] keydown vk=%d chat=%p show=%d login=%p select=%p make=%p edit=%p\n",
            virtualKey,
            static_cast<void*>(m_chatWnd),
            (m_chatWnd ? m_chatWnd->m_show : 0),
            static_cast<void*>(m_loginWnd),
            static_cast<void*>(m_selectCharWnd),
            static_cast<void*>(m_makeCharWnd),
            static_cast<void*>(m_editWindow));
    }

    if (m_npcInputWnd && m_npcInputWnd->m_show != 0) {
        m_npcInputWnd->HandleKeyDown(virtualKey);
        return;
    }

    if (m_joinPartyAcceptWnd && m_joinPartyAcceptWnd->m_show != 0) {
        m_joinPartyAcceptWnd->HandleKeyDown(virtualKey);
        return;
    }

    if (m_npcMenuWnd && m_npcMenuWnd->m_show != 0) {
        m_npcMenuWnd->HandleKeyDown(virtualKey);
        return;
    }

    if (m_skillSelectWnd && m_skillSelectWnd->m_show != 0) {
        m_skillSelectWnd->HandleKeyDown(virtualKey);
        return;
    }

    if (m_playerContextMenuWnd && m_playerContextMenuWnd->m_show != 0) {
        m_playerContextMenuWnd->HandleKeyDown(virtualKey);
        return;
    }

    if (m_sayDialogWnd && m_sayDialogWnd->m_show != 0) {
        m_sayDialogWnd->HandleKeyDown(virtualKey);
        return;
    }

    if (m_itemPurchaseWnd && m_itemPurchaseWnd->m_show != 0) {
        m_itemPurchaseWnd->HandleKeyDown(virtualKey);
        return;
    }

    if (m_itemSellWnd && m_itemSellWnd->m_show != 0) {
        m_itemSellWnd->HandleKeyDown(virtualKey);
        return;
    }

    if (m_itemShopWnd && m_itemShopWnd->m_show != 0) {
        m_itemShopWnd->HandleKeyDown(virtualKey);
        return;
    }

    if (m_storageWnd && m_storageWnd->m_show != 0) {
        if (virtualKey == VK_ESCAPE) {
            g_modeMgr.SendMsg(CGameMode::GameMsg_RequestStorageClose, 0, 0, 0);
            return;
        }
        m_storageWnd->OnKeyDown(virtualKey);
        return;
    }

    if (m_chooseSellBuyWnd && m_chooseSellBuyWnd->m_show != 0) {
        m_chooseSellBuyWnd->HandleKeyDown(virtualKey);
        return;
    }

    if(auto* ground=GroundSkillTextOwner(m_editWindow)){if(ground->m_show){ground->OnKeyDown(virtualKey);return;}}
    if(m_autoMessageWnd && m_autoMessageWnd->m_show){m_autoMessageWnd->OnKeyDown(virtualKey);return;}
    if(auto* tip=dynamic_cast<UITipOfTheDayWnd*>(m_editWindow)){if(tip->m_show){tip->OnKeyDown(virtualKey);return;}}
    if(auto* whisper=dynamic_cast<UIWhisperWnd*>(m_editWindow)){if(whisper->m_show){whisper->OnKeyDown(virtualKey);return;}}
    if(auto* settings=dynamic_cast<UIChatRoomSettingsWnd*>(m_editWindow)){if(settings->m_show){settings->OnKeyDown(virtualKey);return;}}
    if(m_chatRoomPasswordWnd && m_chatRoomPasswordWnd->m_show && m_editWindow==m_chatRoomPasswordWnd){m_chatRoomPasswordWnd->OnKeyDown(virtualKey);return;}
    if(m_chatRoomWnd && m_chatRoomWnd->m_show && m_editWindow==m_chatRoomWnd){m_chatRoomWnd->OnKeyDown(virtualKey);return;}
    if (m_chatWnd && m_chatWnd->m_show != 0 && m_chatWnd->HandleKeyDown(virtualKey)) {
        if (virtualKey == VK_RETURN || virtualKey == VK_ESCAPE) {
            DbgLog("[UI] keydown consumed by chat vk=%d\n", virtualKey);
        }
        return;
    }

    if (m_chooseWnd && m_chooseWnd->m_show != 0) {
        m_chooseWnd->OnKeyDown(virtualKey);
        return;
    }

    if (m_selectServerWnd && m_selectServerWnd->m_show != 0
        && (virtualKey == VK_UP || virtualKey == VK_DOWN)) {
        m_selectServerWnd->OnKeyDown(virtualKey);
        return;
    }

    if (m_optionWnd && m_optionWnd->m_show != 0) {
        m_optionWnd->OnKeyDown(virtualKey);
        return;
    }

    if (!hasFrontMenuUi && m_chatWnd && m_chatWnd->IsInputActive()) {
        return;
    }

    if (HandleHotkeyAfterFocusedUi(virtualKey, isAltDown, isCtrlDown, isShiftDown, hasFrontMenuUi)) {
        return;
    }

    // Route Enter/Escape to the login window or the topmost frame window
    if (m_loginWnd && m_loginWnd->m_show != 0) {
        m_loginWnd->OnKeyDown(virtualKey);
    } else if (m_selectCharWnd && m_selectCharWnd->m_show != 0) {
        m_selectCharWnd->OnKeyDown(virtualKey);
    } else if (m_makeCharWnd && m_makeCharWnd->m_show != 0) {
        m_makeCharWnd->OnKeyDown(virtualKey);
    }
}

void UIWindowMgr::SetLoginStatus(const std::string& status) {
    if (status.empty()) {
        return;
    }

    m_loginStatus = status;
}

void UIWindowMgr::SetLoginWallpaper(const std::string& wallpaperName) {
    m_loginWallpaper = wallpaperName;
    if (!IsLoadingScreenVisible(*this)) {
        SetWallpaperFromGameData(wallpaperName);
    }

    if (m_loginWnd) {
        m_loginWnd->SetWallpaperName(wallpaperName);
    }
}

const std::string& UIWindowMgr::GetLoginStatus() const {
    return m_loginStatus;
}

const std::vector<UIChatEvent>& UIWindowMgr::GetChatEvents() const {
    return m_chatEvents;
}

std::vector<UIChatEvent> UIWindowMgr::GetChatPreviewEvents(size_t maxCount) const {
    std::vector<UIChatEvent> preview;
    if (maxCount == 0 || m_chatEvents.empty()) {
        return preview;
    }

    const size_t start = m_chatEvents.size() > maxCount ? (m_chatEvents.size() - maxCount) : 0;
    preview.reserve(m_chatEvents.size() - start);

    for (size_t i = start; i < m_chatEvents.size(); ++i) {
        preview.push_back(m_chatEvents[i]);
    }
    return preview;
}

void UIWindowMgr::ClearChatEvents() {
    m_chatEvents.clear();
    if (m_chatWnd) {
        m_chatWnd->ClearLines();
    }
}
