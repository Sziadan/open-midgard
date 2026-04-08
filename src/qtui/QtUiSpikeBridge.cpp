#include "QtUiRuntime.h"

#include "QtUiStateAdapter.h"

#include "QtUiStatusIconCatalog.h"

#include "DebugLog.h"
#include "gamemode/GameMode.h"
#include "gamemode/View.h"
#include "render3d/RenderBackend.h"
#include "render3d/RenderDevice.h"
#include "res/Bitmap.h"
#include "session/Session.h"
#include "skill/Skill.h"
#include "ui/UIEquipWnd.h"
#include "ui/UILoginWnd.h"
#include "ui/UIMakeCharWnd.h"
#include "ui/UIMinimapWnd.h"
#include "ui/UINotifyLevelUpWnd.h"
#include "ui/UISelectCharWnd.h"
#include "ui/UISelectServerWnd.h"
#include "ui/UIShopCommon.h"
#include "ui/UIWindowMgr.h"
#include "world/GameActor.h"
#include "world/World.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <unordered_map>
#include <string>
#include <vector>

#if RO_ENABLE_QT6_UI

#include <QByteArray>
#include <QChar>
#include <QCoreApplication>
#include <QEventLoop>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlError>
#include <QImage>
#include <QPainter>
#include <QQuickImageProvider>
#include <QQuickGraphicsDevice>
#include <QQuickItem>
#include <QQuickRenderControl>
#include <QQuickRenderTarget>
#include <QQuickWindow>
#include <QResource>
#include <QSGRendererInterface>
#include <QVulkanInstance>

#if RO_PLATFORM_WINDOWS && RO_HAS_NATIVE_D3D12
#include <rhi/qrhi.h>
#include <rhi/qrhi_platform.h>
#endif

void EnsureQtUiResourcesInitialized()
{
    static bool initialized = false;
    if (!initialized) {
        Q_INIT_RESOURCE(QtUiSpikeAssets);
        initialized = true;
    }
}

namespace {

#if !RO_PLATFORM_WINDOWS
constexpr RoWindowMessage WM_MOUSEMOVE = 0x0200u;
constexpr RoWindowMessage WM_LBUTTONDOWN = 0x0201u;
constexpr RoWindowMessage WM_LBUTTONUP = 0x0202u;
constexpr RoWindowMessage WM_LBUTTONDBLCLK = 0x0203u;
constexpr RoWindowMessage WM_RBUTTONDOWN = 0x0204u;
constexpr RoWindowMessage WM_MOUSEWHEEL = 0x020Au;
constexpr RoWindowMessage WM_CHAR = 0x0102u;
constexpr RoWindowMessage WM_KEYDOWN = 0x0100u;
constexpr RoWindowMessage WM_SYSKEYDOWN = 0x0104u;
#endif

int GetLParamX(RoWindowLParam lParam)
{
    return static_cast<int>(static_cast<short>(static_cast<unsigned long>(lParam) & 0xFFFFu));
}

int GetLParamY(RoWindowLParam lParam)
{
    return static_cast<int>(static_cast<short>((static_cast<unsigned long>(lParam) >> 16) & 0xFFFFu));
}

int GetWheelDelta(RoWindowWParam wParam)
{
    return static_cast<int>(static_cast<short>((static_cast<unsigned long>(wParam) >> 16) & 0xFFFFu));
}

RenderBackendType GetCurrentUiRenderBackend()
{
    return GetRenderDevice().GetBackendType();
}

constexpr std::uint64_t kQtUiPerfLogIntervalFrames = 120ull;

double QtUiNowMs()
{
    using Clock = std::chrono::steady_clock;
    const Clock::time_point now = Clock::now();
    return std::chrono::duration<double, std::milli>(now.time_since_epoch()).count();
}

enum class QtUiImageRequestKind {
    Minimap = 0,
    Wallpaper,
    Item,
    Skill,
    Status,
    Panel,
    EquipPreview,
    Other,
    Count,
};

struct QtUiPerfStats {
    std::uint64_t totalFrames = 0;
    std::uint64_t menuCpuFrames = 0;
    std::uint64_t menuNativeFrames = 0;
    std::uint64_t gameplayCpuFrames = 0;
    std::uint64_t gameplayNativeFrames = 0;
    double menuCpuUpdateMs = 0.0;
    double menuCpuRenderMs = 0.0;
    double menuCpuBlendMs = 0.0;
    double menuNativeUpdateMs = 0.0;
    double menuNativeRenderMs = 0.0;
    double gameplayCpuUpdateMs = 0.0;
    double gameplayCpuRenderMs = 0.0;
    double gameplayCpuBlendMs = 0.0;
    double gameplayNativeUpdateMs = 0.0;
    double gameplayNativeRenderMs = 0.0;
    std::uint64_t imageRequestTotal = 0;
    double imageRequestTotalMs = 0.0;
    std::array<std::uint64_t, static_cast<size_t>(QtUiImageRequestKind::Count)> imageRequestCounts{};
    std::array<double, static_cast<size_t>(QtUiImageRequestKind::Count)> imageRequestMs{};
};

QtUiPerfStats g_qtUiPerfStats;

QtUiImageRequestKind CategorizeQtUiImageRequest(const QString& baseId)
{
    if (baseId == QStringLiteral("minimap")) {
        return QtUiImageRequestKind::Minimap;
    }
    if (baseId == QStringLiteral("wallpaper")) {
        return QtUiImageRequestKind::Wallpaper;
    }
    if (baseId.startsWith(QStringLiteral("item/"))) {
        return QtUiImageRequestKind::Item;
    }
    if (baseId.startsWith(QStringLiteral("skill/"))) {
        return QtUiImageRequestKind::Skill;
    }
    if (baseId.startsWith(QStringLiteral("status/"))) {
        return QtUiImageRequestKind::Status;
    }
    if (baseId == QStringLiteral("equippreview")) {
        return QtUiImageRequestKind::EquipPreview;
    }
    if (baseId == QStringLiteral("makecharpanel")
        || baseId == QStringLiteral("charselectpanel")
        || baseId == QStringLiteral("charselectslotselected")
        || baseId == QStringLiteral("loginpanel")
        || baseId.startsWith(QStringLiteral("makecharbutton/"))) {
        return QtUiImageRequestKind::Panel;
    }
    return QtUiImageRequestKind::Other;
}

const char* GetQtUiImageRequestKindName(QtUiImageRequestKind kind)
{
    switch (kind) {
    case QtUiImageRequestKind::Minimap:
        return "minimap";
    case QtUiImageRequestKind::Wallpaper:
        return "wallpaper";
    case QtUiImageRequestKind::Item:
        return "item";
    case QtUiImageRequestKind::Skill:
        return "skill";
    case QtUiImageRequestKind::Status:
        return "status";
    case QtUiImageRequestKind::Panel:
        return "panel";
    case QtUiImageRequestKind::EquipPreview:
        return "equip";
    case QtUiImageRequestKind::Other:
    case QtUiImageRequestKind::Count:
    default:
        return "other";
    }
}

const char* GetStatusIconDebugLabel(int statusType)
{
    const qtui::StatusIconCatalogEntry* const entry = qtui::FindStatusIconCatalogEntry(statusType);
    return (entry && entry->fallbackLabel) ? entry->fallbackLabel : "unknown";
}

void RecordQtUiImageRequest(QtUiImageRequestKind kind, double elapsedMs)
{
    const size_t index = static_cast<size_t>(kind);
    if (index >= g_qtUiPerfStats.imageRequestCounts.size()) {
        return;
    }

    g_qtUiPerfStats.imageRequestTotal += 1;
    g_qtUiPerfStats.imageRequestTotalMs += elapsedMs;
    g_qtUiPerfStats.imageRequestCounts[index] += 1;
    g_qtUiPerfStats.imageRequestMs[index] += elapsedMs;
}

void MaybeLogQtUiPerfStats()
{
    if (g_qtUiPerfStats.totalFrames == 0 || (g_qtUiPerfStats.totalFrames % kQtUiPerfLogIntervalFrames) != 0) {
        return;
    }

    const double menuCpuFrames = static_cast<double>((std::max)(std::uint64_t{1}, g_qtUiPerfStats.menuCpuFrames));
    const double menuNativeFrames = static_cast<double>((std::max)(std::uint64_t{1}, g_qtUiPerfStats.menuNativeFrames));
    const double gameplayCpuFrames = static_cast<double>((std::max)(std::uint64_t{1}, g_qtUiPerfStats.gameplayCpuFrames));
    const double gameplayNativeFrames = static_cast<double>((std::max)(std::uint64_t{1}, g_qtUiPerfStats.gameplayNativeFrames));
    const double imageRequestTotal = static_cast<double>((std::max)(std::uint64_t{1}, g_qtUiPerfStats.imageRequestTotal));

    DbgLog(
        "[QtUiPerf] frames=%llu menuCpu=%llu update=%.3fms render=%.3fms blend=%.3fms menuNative=%llu update=%.3fms render=%.3fms gameplayCpu=%llu update=%.3fms render=%.3fms blend=%.3fms gameplayNative=%llu update=%.3fms render=%.3fms imgReq=%llu avg=%.3fms %s=%llu/%.3fms %s=%llu/%.3fms %s=%llu/%.3fms %s=%llu/%.3fms %s=%llu/%.3fms %s=%llu/%.3fms %s=%llu/%.3fms %s=%llu/%.3fms\n",
        static_cast<unsigned long long>(g_qtUiPerfStats.totalFrames),
        static_cast<unsigned long long>(g_qtUiPerfStats.menuCpuFrames),
        g_qtUiPerfStats.menuCpuUpdateMs / menuCpuFrames,
        g_qtUiPerfStats.menuCpuRenderMs / menuCpuFrames,
        g_qtUiPerfStats.menuCpuBlendMs / menuCpuFrames,
        static_cast<unsigned long long>(g_qtUiPerfStats.menuNativeFrames),
        g_qtUiPerfStats.menuNativeUpdateMs / menuNativeFrames,
        g_qtUiPerfStats.menuNativeRenderMs / menuNativeFrames,
        static_cast<unsigned long long>(g_qtUiPerfStats.gameplayCpuFrames),
        g_qtUiPerfStats.gameplayCpuUpdateMs / gameplayCpuFrames,
        g_qtUiPerfStats.gameplayCpuRenderMs / gameplayCpuFrames,
        g_qtUiPerfStats.gameplayCpuBlendMs / gameplayCpuFrames,
        static_cast<unsigned long long>(g_qtUiPerfStats.gameplayNativeFrames),
        g_qtUiPerfStats.gameplayNativeUpdateMs / gameplayNativeFrames,
        g_qtUiPerfStats.gameplayNativeRenderMs / gameplayNativeFrames,
        static_cast<unsigned long long>(g_qtUiPerfStats.imageRequestTotal),
        g_qtUiPerfStats.imageRequestTotalMs / imageRequestTotal,
        GetQtUiImageRequestKindName(QtUiImageRequestKind::Minimap),
        static_cast<unsigned long long>(g_qtUiPerfStats.imageRequestCounts[static_cast<size_t>(QtUiImageRequestKind::Minimap)]),
        g_qtUiPerfStats.imageRequestMs[static_cast<size_t>(QtUiImageRequestKind::Minimap)],
        GetQtUiImageRequestKindName(QtUiImageRequestKind::Wallpaper),
        static_cast<unsigned long long>(g_qtUiPerfStats.imageRequestCounts[static_cast<size_t>(QtUiImageRequestKind::Wallpaper)]),
        g_qtUiPerfStats.imageRequestMs[static_cast<size_t>(QtUiImageRequestKind::Wallpaper)],
        GetQtUiImageRequestKindName(QtUiImageRequestKind::Item),
        static_cast<unsigned long long>(g_qtUiPerfStats.imageRequestCounts[static_cast<size_t>(QtUiImageRequestKind::Item)]),
        g_qtUiPerfStats.imageRequestMs[static_cast<size_t>(QtUiImageRequestKind::Item)],
        GetQtUiImageRequestKindName(QtUiImageRequestKind::Skill),
        static_cast<unsigned long long>(g_qtUiPerfStats.imageRequestCounts[static_cast<size_t>(QtUiImageRequestKind::Skill)]),
        g_qtUiPerfStats.imageRequestMs[static_cast<size_t>(QtUiImageRequestKind::Skill)],
        GetQtUiImageRequestKindName(QtUiImageRequestKind::Status),
        static_cast<unsigned long long>(g_qtUiPerfStats.imageRequestCounts[static_cast<size_t>(QtUiImageRequestKind::Status)]),
        g_qtUiPerfStats.imageRequestMs[static_cast<size_t>(QtUiImageRequestKind::Status)],
        GetQtUiImageRequestKindName(QtUiImageRequestKind::Panel),
        static_cast<unsigned long long>(g_qtUiPerfStats.imageRequestCounts[static_cast<size_t>(QtUiImageRequestKind::Panel)]),
        g_qtUiPerfStats.imageRequestMs[static_cast<size_t>(QtUiImageRequestKind::Panel)],
        GetQtUiImageRequestKindName(QtUiImageRequestKind::EquipPreview),
        static_cast<unsigned long long>(g_qtUiPerfStats.imageRequestCounts[static_cast<size_t>(QtUiImageRequestKind::EquipPreview)]),
        g_qtUiPerfStats.imageRequestMs[static_cast<size_t>(QtUiImageRequestKind::EquipPreview)],
        GetQtUiImageRequestKindName(QtUiImageRequestKind::Other),
        static_cast<unsigned long long>(g_qtUiPerfStats.imageRequestCounts[static_cast<size_t>(QtUiImageRequestKind::Other)]),
        g_qtUiPerfStats.imageRequestMs[static_cast<size_t>(QtUiImageRequestKind::Other)]);

    g_qtUiPerfStats = QtUiPerfStats{};
}

void RecordQtUiOverlayPerf(bool gameplay, bool nativePath, double updateMs, double renderMs, double blendMs)
{
    g_qtUiPerfStats.totalFrames += 1;
    if (gameplay) {
        if (nativePath) {
            g_qtUiPerfStats.gameplayNativeFrames += 1;
            g_qtUiPerfStats.gameplayNativeUpdateMs += updateMs;
            g_qtUiPerfStats.gameplayNativeRenderMs += renderMs;
        } else {
            g_qtUiPerfStats.gameplayCpuFrames += 1;
            g_qtUiPerfStats.gameplayCpuUpdateMs += updateMs;
            g_qtUiPerfStats.gameplayCpuRenderMs += renderMs;
            g_qtUiPerfStats.gameplayCpuBlendMs += blendMs;
        }
    } else {
        if (nativePath) {
            g_qtUiPerfStats.menuNativeFrames += 1;
            g_qtUiPerfStats.menuNativeUpdateMs += updateMs;
            g_qtUiPerfStats.menuNativeRenderMs += renderMs;
        } else {
            g_qtUiPerfStats.menuCpuFrames += 1;
            g_qtUiPerfStats.menuCpuUpdateMs += updateMs;
            g_qtUiPerfStats.menuCpuRenderMs += renderMs;
            g_qtUiPerfStats.menuCpuBlendMs += blendMs;
        }
    }

    MaybeLogQtUiPerfStats();
}

bool TryBuildItemIconImage(unsigned int itemId, QImage* outImage)
{
    if (!outImage || itemId == 0) {
        return false;
    }

    static std::unordered_map<unsigned int, QImage> s_itemIconCache;
    const auto cached = s_itemIconCache.find(itemId);
    if (cached != s_itemIconCache.end()) {
        *outImage = cached->second;
        return !outImage->isNull();
    }

    const ITEM_INFO* item = g_session.GetInventoryItemByItemId(itemId);
    ITEM_INFO fallbackItem;
    if (!item) {
        fallbackItem.SetItemId(itemId);
        fallbackItem.m_isIdentified = 1;
        item = &fallbackItem;
    }

    shopui::BitmapPixels bitmap;
    if (!shopui::TryLoadItemIconPixels(*item, &bitmap) || !bitmap.IsValid()) {
        return false;
    }

    const QImage source(
        reinterpret_cast<const uchar*>(bitmap.pixels.data()),
        bitmap.width,
        bitmap.height,
        bitmap.width * static_cast<int>(sizeof(unsigned int)),
        QImage::Format_ARGB32);
    *outImage = source.copy();
    if (!outImage->isNull()) {
        s_itemIconCache[itemId] = *outImage;
    }
    return !outImage->isNull();
}

std::string ResolveQtSkillIconPath(int skillId)
{
    g_skillMgr.EnsureLoaded();
    const SkillMetadata* metadata = g_skillMgr.GetSkillMetadata(skillId);
    if (metadata && !metadata->skillIdName.empty()) {
        const std::string lowered = shopui::ToLowerAscii(metadata->skillIdName);
        const std::string direct = "texture\\\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\item\\" + lowered + ".bmp";
        const std::string dataPath = "data\\" + direct;
        if (g_fileMgr.IsDataExist(direct.c_str())) {
            return direct;
        }
        if (g_fileMgr.IsDataExist(dataPath.c_str())) {
            return dataPath;
        }
        return direct;
    }
    return g_skillMgr.GetSkillIconPath(skillId);
}

bool TryBuildSkillIconImage(int skillId, QImage* outImage)
{
    if (!outImage || skillId == 0) {
        return false;
    }

    static std::unordered_map<int, QImage> s_skillIconCache;
    const auto cached = s_skillIconCache.find(skillId);
    if (cached != s_skillIconCache.end()) {
        *outImage = cached->second;
        return !outImage->isNull();
    }

    const std::string path = ResolveQtSkillIconPath(skillId);
    if (path.empty()) {
        return false;
    }

    shopui::BitmapPixels bitmap = shopui::LoadBitmapPixelsFromGameData(path, true);
    if (!bitmap.IsValid()) {
        return false;
    }

    const QImage source(
        reinterpret_cast<const uchar*>(bitmap.pixels.data()),
        bitmap.width,
        bitmap.height,
        bitmap.width * static_cast<int>(sizeof(unsigned int)),
        QImage::Format_ARGB32);
    *outImage = source.copy();
    if (!outImage->isNull()) {
        s_skillIconCache[skillId] = *outImage;
    }
    return !outImage->isNull();
}

bool TryBuildGameImageWithBitmapRes(const std::string& path, bool applyTransparentKey, QImage* outImage)
{
    if (!outImage || path.empty()) {
        return false;
    }

    int size = 0;
    unsigned char* bytes = g_fileMgr.GetData(path.c_str(), &size);
    if (!bytes || size <= 0) {
        delete[] bytes;
        return false;
    }

    CBitmapRes bitmapRes;
    const bool loaded = bitmapRes.LoadFromBuffer(path.c_str(), bytes, size);
    delete[] bytes;
    if (!loaded || !bitmapRes.m_data || bitmapRes.m_width <= 0 || bitmapRes.m_height <= 0) {
        return false;
    }

    const QImage source(
        reinterpret_cast<const uchar*>(bitmapRes.m_data),
        bitmapRes.m_width,
        bitmapRes.m_height,
        bitmapRes.m_width * static_cast<int>(sizeof(unsigned int)),
        QImage::Format_ARGB32);
    *outImage = source.copy();
    if (outImage->isNull()) {
        return false;
    }

    if (applyTransparentKey) {
        for (int y = 0; y < outImage->height(); ++y) {
            QRgb* row = reinterpret_cast<QRgb*>(outImage->scanLine(y));
            for (int x = 0; x < outImage->width(); ++x) {
                if ((row[x] & 0x00FFFFFFu) == 0x00FF00FFu) {
                    row[x] = 0u;
                }
            }
        }
    }

    return true;
}

void AddUniqueStatusPathCandidate(std::vector<std::string>* out, const std::string& raw)
{
    if (!out || raw.empty()) {
        return;
    }

    const std::string normalized = shopui::NormalizeSlash(raw);
    const std::string lowered = shopui::ToLowerAscii(normalized);
    for (const std::string& existing : *out) {
        if (shopui::ToLowerAscii(existing) == lowered) {
            return;
        }
    }
    out->push_back(normalized);
}

std::string ResolveQtStatusIconPath(int statusType)
{
    const qtui::StatusIconCatalogEntry* const entry = qtui::FindStatusIconCatalogEntry(statusType);
    if (!entry || !entry->legacyIconFileName || !entry->legacyIconFileName[0]) {
        DbgLog("[QtUiStatusIcon] no legacy filename mapped for statusType=%d label='%s'\n",
            statusType,
            GetStatusIconDebugLabel(statusType));
        return std::string();
    }

    const std::string fileName(entry->legacyIconFileName);
    std::vector<std::string> candidates;
    AddUniqueStatusPathCandidate(&candidates, fileName);
    AddUniqueStatusPathCandidate(&candidates, std::string("effect\\") + fileName);
    AddUniqueStatusPathCandidate(&candidates, std::string("texture\\effect\\") + fileName);
    AddUniqueStatusPathCandidate(&candidates, std::string("data\\texture\\effect\\") + fileName);
    AddUniqueStatusPathCandidate(&candidates, std::string("data\\effect\\") + fileName);

    for (const std::string& candidate : candidates) {
        if (g_fileMgr.IsDataExist(candidate.c_str())) {
            DbgLog("[QtUiStatusIcon] resolved statusType=%d label='%s' path='%s'\n",
                statusType,
                GetStatusIconDebugLabel(statusType),
                candidate.c_str());
            return candidate;
        }
    }

    DbgLog("[QtUiStatusIcon] missing statusType=%d label='%s' filename='%s'\n",
        statusType,
        GetStatusIconDebugLabel(statusType),
        entry->legacyIconFileName);
    for (const std::string& candidate : candidates) {
        DbgLog("[QtUiStatusIcon]   tried '%s'\n", candidate.c_str());
    }

    return std::string();
}

bool TryBuildStatusIconImage(int statusType, QImage* outImage)
{
    if (!outImage || statusType == 0) {
        return false;
    }

    static std::unordered_map<int, QImage> s_statusIconCache;
    const auto cached = s_statusIconCache.find(statusType);
    if (cached != s_statusIconCache.end()) {
        *outImage = cached->second;
        return !outImage->isNull();
    }

    const std::string path = ResolveQtStatusIconPath(statusType);
    if (path.empty()) {
        DbgLog("[QtUiStatusIcon] request failed: no path for statusType=%d label='%s'\n",
            statusType,
            GetStatusIconDebugLabel(statusType));
        return false;
    }

    QImage decodedImage;
    if (!TryBuildGameImageWithBitmapRes(path, true, &decodedImage)) {
        DbgLog("[QtUiStatusIcon] decode failed: statusType=%d label='%s' path='%s'\n",
            statusType,
            GetStatusIconDebugLabel(statusType),
            path.c_str());
        return false;
    }

    *outImage = decodedImage;
    if (!outImage->isNull()) {
        s_statusIconCache[statusType] = *outImage;
        DbgLog("[QtUiStatusIcon] loaded statusType=%d label='%s' path='%s' size=%dx%d\n",
            statusType,
            GetStatusIconDebugLabel(statusType),
            path.c_str(),
            outImage->width(),
            outImage->height());
    } else {
        DbgLog("[QtUiStatusIcon] Qt copy failed: statusType=%d label='%s' path='%s'\n",
            statusType,
            GetStatusIconDebugLabel(statusType),
            path.c_str());
    }
    return !outImage->isNull();
}

enum class MenuPointerCaptureTarget {
    None,
    Login,
    ServerSelect,
    CharSelect,
    MakeChar,
    NotifyLevelUp,
    NotifyJobLevelUp,
};

bool HasFrontMenuUiVisible()
{
    return (g_windowMgr.m_loginWnd && g_windowMgr.m_loginWnd->m_show != 0)
        || (g_windowMgr.m_selectServerWnd && g_windowMgr.m_selectServerWnd->m_show != 0)
        || (g_windowMgr.m_selectCharWnd && g_windowMgr.m_selectCharWnd->m_show != 0)
        || (g_windowMgr.m_makeCharWnd && g_windowMgr.m_makeCharWnd->m_show != 0);
}

bool IsEnabledInEnvironment()
{
    static bool logged = false;
    if (!logged) {
        DbgLog("[QtUi] Runtime force-enabled in code; environment variables are ignored for this test build.\n");
        logged = true;
    }
    return true;
}

class QtUiImageProvider final : public QQuickImageProvider {
public:
    QtUiImageProvider()
        : QQuickImageProvider(QQuickImageProvider::Image)
    {
    }

    QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override
    {
        const double requestStartMs = QtUiNowMs();
        QString baseId = id;
        const int queryPos = baseId.indexOf(QLatin1Char('?'));
        if (queryPos >= 0) {
            baseId.truncate(queryPos);
        }
        const QtUiImageRequestKind requestKind = CategorizeQtUiImageRequest(baseId);

        QImage image;
        if (baseId == QStringLiteral("minimap")) {
            const UIRoMapWnd* const minimapWnd = g_windowMgr.m_roMapWnd;
            if (minimapWnd && minimapWnd->m_show != 0) {
                minimapWnd->BuildQtMinimapImage(&image);
            }
        } else if (baseId.startsWith(QStringLiteral("makecharbutton/"))) {
            if (g_windowMgr.m_makeCharWnd) {
                bool ok = false;
                const int buttonIndex = baseId.mid(QStringLiteral("makecharbutton/").size()).toInt(&ok);
                if (ok) {
                    const unsigned int* pixels = nullptr;
                    int width = 0;
                    int height = 0;
                    if (g_windowMgr.m_makeCharWnd->GetQtButtonBitmap(buttonIndex, &pixels, &width, &height)
                        && pixels && width > 0 && height > 0) {
                        const QImage source(
                            reinterpret_cast<const uchar*>(pixels),
                            width,
                            height,
                            width * static_cast<int>(sizeof(unsigned int)),
                            QImage::Format_ARGB32);
                        image = source.copy();
                    }
                }
            }
        } else if (baseId == QStringLiteral("makecharpanel")) {
            if (g_windowMgr.m_makeCharWnd && g_windowMgr.m_makeCharWnd->m_show != 0) {
                const unsigned int* pixels = nullptr;
                int width = 0;
                int height = 0;
                if (g_windowMgr.m_makeCharWnd->GetQtBackgroundBitmap(&pixels, &width, &height)
                    && pixels && width > 0 && height > 0) {
                    const QImage source(
                        reinterpret_cast<const uchar*>(pixels),
                        width,
                        height,
                        width * static_cast<int>(sizeof(unsigned int)),
                        QImage::Format_ARGB32);
                    image = source.copy();
                    if (!image.isNull()) {
                        g_windowMgr.m_makeCharWnd->EnsureQtLayout();
                        g_windowMgr.m_makeCharWnd->DrawQtHexagon(&image);
                        g_windowMgr.m_makeCharWnd->DrawQtPreview(&image);

                        QPainter painter(&image);
                        for (int buttonIndex = 0; buttonIndex < g_windowMgr.m_makeCharWnd->GetQtButtonCount(); ++buttonIndex) {
                            UIMakeCharWnd::QtButtonDisplay buttonDisplay;
                            const unsigned int* buttonPixels = nullptr;
                            int buttonWidth = 0;
                            int buttonHeight = 0;
                            if (!g_windowMgr.m_makeCharWnd->GetQtButtonDisplayForQt(buttonIndex, &buttonDisplay)
                                || !g_windowMgr.m_makeCharWnd->GetQtButtonBitmap(
                                    buttonIndex,
                                    &buttonPixels,
                                    &buttonWidth,
                                    &buttonHeight)
                                || !buttonPixels
                                || buttonWidth <= 0
                                || buttonHeight <= 0) {
                                continue;
                            }

                            const QImage buttonSource(
                                reinterpret_cast<const uchar*>(buttonPixels),
                                buttonWidth,
                                buttonHeight,
                                buttonWidth * static_cast<int>(sizeof(unsigned int)),
                                QImage::Format_ARGB32);
                            painter.drawImage(
                                buttonDisplay.x - g_windowMgr.m_makeCharWnd->m_x,
                                buttonDisplay.y - g_windowMgr.m_makeCharWnd->m_y,
                                buttonSource);
                        }
                    }
                }
            }
        } else if (baseId == QStringLiteral("charselectpanel")) {
            if (g_windowMgr.m_selectCharWnd && g_windowMgr.m_selectCharWnd->m_show != 0) {
                const unsigned int* pixels = nullptr;
                int width = 0;
                int height = 0;
                if (g_windowMgr.m_selectCharWnd->GetQtBackgroundBitmap(&pixels, &width, &height)
                    && pixels && width > 0 && height > 0) {
                    const QImage source(
                        reinterpret_cast<const uchar*>(pixels),
                        width,
                        height,
                        width * static_cast<int>(sizeof(unsigned int)),
                        QImage::Format_ARGB32);
                    image = source.copy();
                    if (!image.isNull()) {
                        g_windowMgr.m_selectCharWnd->EnsureQtLayout();
                        g_windowMgr.m_selectCharWnd->DrawQtPreviews(&image);
                    }
                }
            }
        } else if (baseId == QStringLiteral("charselectslotselected")) {
            if (g_windowMgr.m_selectCharWnd && g_windowMgr.m_selectCharWnd->m_show != 0) {
                const unsigned int* pixels = nullptr;
                int width = 0;
                int height = 0;
                if (g_windowMgr.m_selectCharWnd->GetQtSelectedSlotBitmap(&pixels, &width, &height)
                    && pixels && width > 0 && height > 0) {
                    const QImage source(
                        reinterpret_cast<const uchar*>(pixels),
                        width,
                        height,
                        width * static_cast<int>(sizeof(unsigned int)),
                        QImage::Format_ARGB32);
                    image = source.copy();
                }
            }
        } else if (baseId == QStringLiteral("loginpanel")) {
            if (g_windowMgr.m_loginWnd && g_windowMgr.m_loginWnd->m_show != 0) {
                const unsigned int* pixels = nullptr;
                int width = 0;
                int height = 0;
                if (g_windowMgr.m_loginWnd->GetQtPanelBitmap(&pixels, &width, &height)
                    && pixels && width > 0 && height > 0) {
                    const QImage source(
                        reinterpret_cast<const uchar*>(pixels),
                        width,
                        height,
                        width * static_cast<int>(sizeof(unsigned int)),
                        QImage::Format_ARGB32);
                    image = source.copy();
                } else {
                    width = (std::max)(g_windowMgr.m_loginWnd->m_w, 1);
                    height = (std::max)(g_windowMgr.m_loginWnd->m_h, 1);
                    image = QImage(width, height, QImage::Format_ARGB32_Premultiplied);
                    image.fill(QColor(243, 240, 231, 224));

                    QPainter painter(&image);
                    painter.setRenderHint(QPainter::Antialiasing, false);
                    painter.setPen(QColor(120, 112, 96));
                    painter.drawRect(0, 0, width - 1, height - 1);
                }
            }
        } else if (baseId.startsWith(QStringLiteral("item/"))) {
            bool ok = false;
            const unsigned int itemId = baseId.mid(QStringLiteral("item/").size()).toUInt(&ok);
            if (ok) {
                TryBuildItemIconImage(itemId, &image);
            }
        } else if (baseId.startsWith(QStringLiteral("skill/"))) {
            bool ok = false;
            const int skillId = baseId.mid(QStringLiteral("skill/").size()).toInt(&ok);
            if (ok) {
                TryBuildSkillIconImage(skillId, &image);
            }
        } else if (baseId.startsWith(QStringLiteral("status/"))) {
            bool ok = false;
            const int statusType = baseId.mid(QStringLiteral("status/").size()).toInt(&ok);
            if (ok) {
                TryBuildStatusIconImage(statusType, &image);
            }
        } else if (baseId == QStringLiteral("equippreview")) {
            const UIEquipWnd* const equipWnd = g_windowMgr.m_equipWnd;
            if (equipWnd && equipWnd->m_show != 0 && !equipWnd->IsMiniMode()) {
                equipWnd->BuildQtPreviewImage(&image);
            }
        } else if (baseId == QStringLiteral("wallpaper")) {
            if (g_windowMgr.m_wallpaperSurface && g_windowMgr.m_wallpaperSurface->HasSoftwarePixels()) {
                const unsigned int width = g_windowMgr.m_wallpaperSurface->m_w;
                const unsigned int height = g_windowMgr.m_wallpaperSurface->m_h;
                const unsigned int* pixels = g_windowMgr.m_wallpaperSurface->GetSoftwarePixels();
                if (pixels && width > 0 && height > 0) {
                    const QImage source(
                        reinterpret_cast<const uchar*>(pixels),
                        static_cast<int>(width),
                        static_cast<int>(height),
                        static_cast<int>(width * sizeof(unsigned int)),
                        QImage::Format_ARGB32);
                    image = source.copy();
                }
            }

            if (image.isNull()) {
                const int fallbackWidth = requestedSize.isValid() && requestedSize.width() > 0 ? requestedSize.width() : 2;
                const int fallbackHeight = requestedSize.isValid() && requestedSize.height() > 0 ? requestedSize.height() : 2;
                image = QImage(fallbackWidth, fallbackHeight, QImage::Format_ARGB32_Premultiplied);
                image.fill(QColor(22, 26, 34));
            }
        }

        if (!image.isNull() && requestedSize.isValid() && requestedSize.width() > 0 && requestedSize.height() > 0) {
            image = image.scaled(requestedSize, Qt::IgnoreAspectRatio, Qt::FastTransformation);
        }

        if (size) {
            *size = image.size();
        }
        RecordQtUiImageRequest(requestKind, QtUiNowMs() - requestStartMs);
        return image;
    }
};

void BlendQtImageOntoBgraBuffer(const QImage& source, void* bgraPixels, int width, int height, int pitch)
{
    if (!bgraPixels || width <= 0 || height <= 0 || pitch < width * static_cast<int>(sizeof(unsigned int))) {
        return;
    }

    const QImage straightSource = source.convertToFormat(QImage::Format_ARGB32);
    const int copyWidth = (std::min)(width, straightSource.width());
    const int copyHeight = (std::min)(height, straightSource.height());
    if (copyWidth <= 0 || copyHeight <= 0) {
        return;
    }

    unsigned char* dstBytes = static_cast<unsigned char*>(bgraPixels);
    for (int y = 0; y < copyHeight; ++y) {
        const QRgb* srcRow = reinterpret_cast<const QRgb*>(straightSource.constScanLine(y));
        unsigned int* dstRow = reinterpret_cast<unsigned int*>(dstBytes + static_cast<size_t>(y) * static_cast<size_t>(pitch));
        for (int x = 0; x < copyWidth; ++x) {
            const unsigned int src = srcRow[x];
            const unsigned int srcA = (src >> 24) & 0xFFu;
            if (srcA == 0u) {
                continue;
            }

            const unsigned int dst = dstRow[x];
            const unsigned int dstA = (dst >> 24) & 0xFFu;

            const unsigned int srcR = (src >> 16) & 0xFFu;
            const unsigned int srcG = (src >> 8) & 0xFFu;
            const unsigned int srcB = src & 0xFFu;

            const unsigned int dstR = (dst >> 16) & 0xFFu;
            const unsigned int dstG = (dst >> 8) & 0xFFu;
            const unsigned int dstB = dst & 0xFFu;

            const unsigned int invSrcA = 255u - srcA;
            const unsigned int outA = srcA + (dstA * invSrcA + 127u) / 255u;
            if (outA == 0u) {
                dstRow[x] = 0u;
                continue;
            }

            const unsigned int outPremulR = srcR * srcA + (dstR * dstA * invSrcA + 127u) / 255u;
            const unsigned int outPremulG = srcG * srcA + (dstG * dstA * invSrcA + 127u) / 255u;
            const unsigned int outPremulB = srcB * srcA + (dstB * dstA * invSrcA + 127u) / 255u;

            const unsigned int outR = (outPremulR + outA / 2u) / outA;
            const unsigned int outG = (outPremulG + outA / 2u) / outA;
            const unsigned int outB = (outPremulB + outA / 2u) / outA;

            dstRow[x] = (outA << 24)
                | ((outR & 0xFFu) << 16)
                | ((outG & 0xFFu) << 8)
                | (outB & 0xFFu);
        }
    }
}

class QtUiRuntimeHost {
public:
    bool initialize(RoNativeWindowHandle mainWindow)
    {
        if (m_active) {
            return true;
        }
        if (!IsEnabledInEnvironment()) {
            return false;
        }

        m_mainWindow = mainWindow;
        if (!ensureApplication()) {
            return false;
        }
        if (!ensureScene()) {
            shutdown();
            return false;
        }

        m_active = true;
        DbgLog("[QtUi] Enabled offscreen Qt Quick runtime.\n");
        return true;
    }

    void shutdown()
    {
        m_active = false;
        m_nativeGraphicsInitialized = false;
        m_nativeTargetMirrorVertically = false;
        m_nativeGraphicsBackend = RenderBackendType::LegacyDirect3D7;
        m_nativeOverlayBackend = RenderBackendType::LegacyDirect3D7;

        if (m_renderControl) {
            m_renderControl->invalidate();
        }
        if (m_quickWindow) {
            m_quickWindow->setGraphicsDevice(QQuickGraphicsDevice());
            m_quickWindow->setRenderTarget(QQuickRenderTarget());
        }

        delete m_rootItem;
        m_rootItem = nullptr;

        delete m_engine;
        m_engine = nullptr;

        delete m_quickWindow;
        m_quickWindow = nullptr;

        delete m_renderControl;
        m_renderControl = nullptr;

    #if RO_PLATFORM_WINDOWS && RO_HAS_NATIVE_D3D12
        delete m_qtRhi;
        m_qtRhi = nullptr;
    #endif

        delete m_vulkanInstance;
        m_vulkanInstance = nullptr;

        delete m_stateAdapter;
        m_stateAdapter = nullptr;

        if (m_ownedApplication) {
            delete m_application;
            m_application = nullptr;
            m_ownedApplication = false;
        } else {
            m_application = nullptr;
        }

        m_renderImage = QImage();
    }

    void processEvents()
    {
        if (!m_active || !m_application) {
            return;
        }
        QCoreApplication::sendPostedEvents(nullptr, 0);
        QCoreApplication::processEvents(QEventLoop::AllEvents, 0);
    }

    void notifyWindowMessage(RoWindowMessage msg, RoWindowWParam wParam, RoWindowLParam lParam)
    {
        switch (msg) {
        case WM_MOUSEMOVE:
            m_mouseX = GetLParamX(lParam);
            m_mouseY = GetLParamY(lParam);
            m_lastInput = QStringLiteral("Mouse move %1,%2").arg(m_mouseX).arg(m_mouseY);
            break;
        case WM_LBUTTONDOWN:
            m_lastInput = QStringLiteral("Left click %1,%2")
                .arg(GetLParamX(lParam))
                .arg(GetLParamY(lParam));
            break;
        case WM_RBUTTONDOWN:
            m_lastInput = QStringLiteral("Right click %1,%2")
                .arg(GetLParamX(lParam))
                .arg(GetLParamY(lParam));
            break;
        case WM_MOUSEWHEEL:
            m_lastInput = QStringLiteral("Mouse wheel %1").arg(GetWheelDelta(wParam));
            break;
        case WM_KEYDOWN:
            m_lastInput = QStringLiteral("Key down VK %1").arg(static_cast<unsigned int>(wParam));
            break;
        case WM_CHAR:
            m_lastInput = QStringLiteral("Char '%1'").arg(QChar(static_cast<unsigned short>(wParam)));
            break;
        default:
            break;
        }
        if (m_stateAdapter && !m_lastInput.isEmpty()) {
            m_stateAdapter->setLastInput(m_lastInput);
        }
    }

    bool handleWindowMessage(RoWindowMessage msg, RoWindowWParam wParam, RoWindowLParam lParam)
    {
        notifyWindowMessage(msg, wParam, lParam);
        if (!m_active) {
            return false;
        }

        switch (msg) {
        case WM_MOUSEMOVE:
            if (g_windowMgr.m_selectServerWnd && g_windowMgr.m_selectServerWnd->m_show != 0) {
                g_windowMgr.m_selectServerWnd->OnMouseMove(
                    GetLParamX(lParam),
                    GetLParamY(lParam));
                return true;
            }
            return false;

        case WM_LBUTTONDBLCLK:
            if (g_windowMgr.m_selectCharWnd
                && g_windowMgr.m_selectCharWnd->m_show != 0
                && g_windowMgr.m_selectCharWnd->HandleQtDoubleClick(
                    GetLParamX(lParam),
                    GetLParamY(lParam))) {
                m_menuPointerCaptureTarget = MenuPointerCaptureTarget::None;
                return true;
            }
            [[fallthrough]];

        case WM_LBUTTONDOWN:
            if (g_windowMgr.m_makeCharWnd
                && g_windowMgr.m_makeCharWnd->m_show != 0
                && g_windowMgr.m_makeCharWnd->HandleQtMouseDown(
                    GetLParamX(lParam),
                    GetLParamY(lParam))) {
                m_menuPointerCaptureTarget = MenuPointerCaptureTarget::MakeChar;
                return true;
            }
            if (g_windowMgr.m_selectServerWnd
                && g_windowMgr.m_selectServerWnd->m_show != 0
                && g_windowMgr.m_selectServerWnd->HandleQtMouseDown(
                    GetLParamX(lParam),
                    GetLParamY(lParam))) {
                m_menuPointerCaptureTarget = MenuPointerCaptureTarget::ServerSelect;
                return true;
            }
            if (g_windowMgr.m_selectCharWnd
                && g_windowMgr.m_selectCharWnd->m_show != 0
                && g_windowMgr.m_selectCharWnd->HandleQtMouseDown(
                    GetLParamX(lParam),
                    GetLParamY(lParam))) {
                m_menuPointerCaptureTarget = MenuPointerCaptureTarget::CharSelect;
                return true;
            }
            if (g_windowMgr.m_loginWnd
                && g_windowMgr.m_loginWnd->m_show != 0
                && g_windowMgr.m_loginWnd->HandleQtMouseDown(
                    GetLParamX(lParam),
                    GetLParamY(lParam))) {
                m_menuPointerCaptureTarget = MenuPointerCaptureTarget::Login;
                return true;
            }
            if (g_windowMgr.m_notifyLevelUpWnd
                && g_windowMgr.m_notifyLevelUpWnd->m_show != 0
                && g_windowMgr.m_notifyLevelUpWnd->HandleQtMouseDown(
                    GetLParamX(lParam),
                    GetLParamY(lParam))) {
                m_menuPointerCaptureTarget = MenuPointerCaptureTarget::NotifyLevelUp;
                return true;
            }
            if (g_windowMgr.m_notifyJobLevelUpWnd
                && g_windowMgr.m_notifyJobLevelUpWnd->m_show != 0
                && g_windowMgr.m_notifyJobLevelUpWnd->HandleQtMouseDown(
                    GetLParamX(lParam),
                    GetLParamY(lParam))) {
                m_menuPointerCaptureTarget = MenuPointerCaptureTarget::NotifyJobLevelUp;
                return true;
            }
            return false;

        case WM_LBUTTONUP:
            if (m_menuPointerCaptureTarget == MenuPointerCaptureTarget::MakeChar) {
                if (g_windowMgr.m_makeCharWnd
                    && g_windowMgr.m_makeCharWnd->m_show != 0) {
                    g_windowMgr.m_makeCharWnd->HandleQtMouseUp(
                        GetLParamX(lParam),
                        GetLParamY(lParam));
                }
                m_menuPointerCaptureTarget = MenuPointerCaptureTarget::None;
                return true;
            }
            if (m_menuPointerCaptureTarget == MenuPointerCaptureTarget::CharSelect) {
                if (g_windowMgr.m_selectCharWnd
                    && g_windowMgr.m_selectCharWnd->m_show != 0
                    && g_windowMgr.m_selectCharWnd->HandleQtMouseUp(
                        GetLParamX(lParam),
                        GetLParamY(lParam))) {
                }
                m_menuPointerCaptureTarget = MenuPointerCaptureTarget::None;
                return true;
            }
            if (m_menuPointerCaptureTarget == MenuPointerCaptureTarget::NotifyLevelUp) {
                if (g_windowMgr.m_notifyLevelUpWnd
                    && g_windowMgr.m_notifyLevelUpWnd->m_show != 0) {
                    g_windowMgr.m_notifyLevelUpWnd->HandleQtMouseUp(
                        GetLParamX(lParam),
                        GetLParamY(lParam));
                }
                m_menuPointerCaptureTarget = MenuPointerCaptureTarget::None;
                return true;
            }
            if (m_menuPointerCaptureTarget == MenuPointerCaptureTarget::NotifyJobLevelUp) {
                if (g_windowMgr.m_notifyJobLevelUpWnd
                    && g_windowMgr.m_notifyJobLevelUpWnd->m_show != 0) {
                    g_windowMgr.m_notifyJobLevelUpWnd->HandleQtMouseUp(
                        GetLParamX(lParam),
                        GetLParamY(lParam));
                }
                m_menuPointerCaptureTarget = MenuPointerCaptureTarget::None;
                return true;
            }
            if (m_menuPointerCaptureTarget != MenuPointerCaptureTarget::None) {
                m_menuPointerCaptureTarget = MenuPointerCaptureTarget::None;
                return true;
            }
            return false;

        case WM_CHAR:
            if (HasFrontMenuUiVisible()) {
                g_windowMgr.OnChar(static_cast<char>(wParam));
                return true;
            }
            return false;

        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            if (HasFrontMenuUiVisible()) {
                g_windowMgr.OnKeyDown(static_cast<int>(wParam));
                return true;
            }
            return false;

        default:
            return false;
        }
    }

    bool compositeMenuOverlay(void* bgraPixels, int width, int height, int pitch)
    {
        if (!m_active || !bgraPixels || width <= 0 || height <= 0) {
            return false;
        }
        QtUiRenderTargetInfo targetInfo{};
        if (GetRenderDevice().GetQtUiRenderTargetInfo(&targetInfo) && targetInfo.available) {
            switch (targetInfo.backend) {
            case RenderBackendType::Direct3D11:
            case RenderBackendType::Direct3D12:
            case RenderBackendType::Vulkan:
                return false;
            default:
                break;
            }
        }
        const double updateStartMs = QtUiNowMs();
        if (!updateMenuState()) {
            return false;
        }
        const double updateMs = QtUiNowMs() - updateStartMs;

        const double renderStartMs = QtUiNowMs();
        if (!renderToImage(width, height)) {
            return false;
        }
        const double renderMs = QtUiNowMs() - renderStartMs;

        if (!m_loggedMenuCpuBridge) {
            DbgLog("[QtUi] Menu overlay using CPU bridge fallback (%dx%d).\n", width, height);
            m_loggedMenuCpuBridge = true;
        }
        const double blendStartMs = QtUiNowMs();
        BlendQtImageOntoBgraBuffer(m_renderImage, bgraPixels, width, height, pitch);
        RecordQtUiOverlayPerf(false, false, updateMs, renderMs, QtUiNowMs() - blendStartMs);
        return true;
    }

    bool renderMenuOverlayTexture(CTexture* texture, int width, int height)
    {
        if (!m_active || !texture || width <= 0 || height <= 0) {
            return false;
        }
        const double updateStartMs = QtUiNowMs();
        if (!updateMenuState()) {
            return false;
        }
        const double updateMs = QtUiNowMs() - updateStartMs;

        QtUiRenderTargetInfo targetInfo{};
        if (!GetRenderDevice().GetQtUiTextureTargetInfo(texture, &targetInfo) || !targetInfo.available) {
            if (!m_loggedMenuTextureUnavailable) {
                DbgLog("[QtUi] Menu overlay native texture target unavailable on backend '%s'.\n",
                    GetRenderBackendName(GetRenderDevice().GetBackendType()));
                m_loggedMenuTextureUnavailable = true;
            }
            m_nativeOverlayBackend = RenderBackendType::LegacyDirect3D7;
            return false;
        }

        const int targetWidth = targetInfo.width > 0 ? static_cast<int>(targetInfo.width) : width;
        const int targetHeight = targetInfo.height > 0 ? static_cast<int>(targetInfo.height) : height;
        bool rendered = false;
        const double renderStartMs = QtUiNowMs();
        switch (targetInfo.backend) {
        case RenderBackendType::Direct3D11:
#if RO_HAS_NATIVE_D3D11
            rendered = renderToD3D11Texture(targetInfo, targetWidth, targetHeight);
#endif
            break;
        case RenderBackendType::Direct3D12:
#if RO_HAS_NATIVE_D3D12
            rendered = renderToD3D12Texture(targetInfo, targetWidth, targetHeight);
#endif
            break;
        case RenderBackendType::Vulkan:
            rendered = renderToVulkanTexture(targetInfo, targetWidth, targetHeight);
            break;
        default:
            rendered = false;
            break;
        }
        const double renderMs = QtUiNowMs() - renderStartMs;

        if (!rendered) {
            if (!m_loggedNativeOverlayFailure) {
                DbgLog("[QtUi] Failed to render to native %s overlay texture target.\n",
                    GetRenderBackendName(targetInfo.backend));
                m_loggedNativeOverlayFailure = true;
            }
            m_nativeOverlayBackend = RenderBackendType::LegacyDirect3D7;
            return false;
        }

        if (!m_loggedNativeOverlaySuccess || m_nativeOverlayBackend != targetInfo.backend) {
            DbgLog("[QtUi] Rendering to %s overlay texture target.\n",
                GetRenderBackendName(targetInfo.backend));
            m_loggedNativeOverlaySuccess = true;
        }
        m_nativeOverlayBackend = targetInfo.backend;
        RecordQtUiOverlayPerf(false, true, updateMs, renderMs, 0.0);
        return true;
    }

    bool compositeGameplayOverlay(CGameMode& mode, void* bgraPixels, int width, int height, int pitch)
    {
        if (!m_active || !bgraPixels || width <= 0 || height <= 0) {
            return false;
        }
        QtUiRenderTargetInfo targetInfo{};
        if (GetRenderDevice().GetQtUiRenderTargetInfo(&targetInfo) && targetInfo.available) {
            switch (targetInfo.backend) {
            case RenderBackendType::Direct3D11:
            case RenderBackendType::Direct3D12:
            case RenderBackendType::Vulkan:
                return false;
            default:
                break;
            }
        }
        const double updateStartMs = QtUiNowMs();
        if (!updateState(mode)) {
            return false;
        }
        const double updateMs = QtUiNowMs() - updateStartMs;

        const double renderStartMs = QtUiNowMs();
        if (!renderToImage(width, height)) {
            return false;
        }
        const double renderMs = QtUiNowMs() - renderStartMs;

        if (!m_loggedGameplayCpuBridge) {
            DbgLog("[QtUi] Gameplay overlay using CPU bridge fallback (%dx%d).\n", width, height);
            m_loggedGameplayCpuBridge = true;
        }
        const double blendStartMs = QtUiNowMs();
        BlendQtImageOntoBgraBuffer(m_renderImage, bgraPixels, width, height, pitch);
        RecordQtUiOverlayPerf(true, false, updateMs, renderMs, QtUiNowMs() - blendStartMs);
        return true;
    }

    bool renderGameplayOverlayTexture(CGameMode& mode, CTexture* texture, int width, int height)
    {
        if (!m_active || !texture || width <= 0 || height <= 0) {
            return false;
        }
        const double updateStartMs = QtUiNowMs();
        if (!updateState(mode)) {
            return false;
        }
        const double updateMs = QtUiNowMs() - updateStartMs;

        QtUiRenderTargetInfo targetInfo{};
        if (!GetRenderDevice().GetQtUiTextureTargetInfo(texture, &targetInfo) || !targetInfo.available) {
            if (!m_loggedGameplayTextureUnavailable) {
                DbgLog("[QtUi] Gameplay overlay native texture target unavailable on backend '%s'.\n",
                    GetRenderBackendName(GetRenderDevice().GetBackendType()));
                m_loggedGameplayTextureUnavailable = true;
            }
            m_nativeOverlayBackend = RenderBackendType::LegacyDirect3D7;
            return false;
        }

        const int targetWidth = targetInfo.width > 0 ? static_cast<int>(targetInfo.width) : width;
        const int targetHeight = targetInfo.height > 0 ? static_cast<int>(targetInfo.height) : height;
        bool rendered = false;
        const double renderStartMs = QtUiNowMs();
        switch (targetInfo.backend) {
        case RenderBackendType::Direct3D11:
#if RO_HAS_NATIVE_D3D11
            rendered = renderToD3D11Texture(targetInfo, targetWidth, targetHeight);
#endif
            break;
        case RenderBackendType::Direct3D12:
#if RO_HAS_NATIVE_D3D12
            rendered = renderToD3D12Texture(targetInfo, targetWidth, targetHeight);
#endif
            break;
        case RenderBackendType::Vulkan:
            rendered = renderToVulkanTexture(targetInfo, targetWidth, targetHeight);
            break;
        default:
            rendered = false;
            break;
        }
        const double renderMs = QtUiNowMs() - renderStartMs;

        if (!rendered) {
            if (!m_loggedNativeOverlayFailure) {
                DbgLog("[QtUi] Failed to render to native %s overlay texture target.\n",
                    GetRenderBackendName(targetInfo.backend));
                m_loggedNativeOverlayFailure = true;
            }
            m_nativeOverlayBackend = RenderBackendType::LegacyDirect3D7;
            return false;
        }

        if (!m_loggedNativeOverlaySuccess || m_nativeOverlayBackend != targetInfo.backend) {
            DbgLog("[QtUi] Rendering to %s overlay texture target.\n",
                GetRenderBackendName(targetInfo.backend));
            m_loggedNativeOverlaySuccess = true;
        }
        m_nativeOverlayBackend = targetInfo.backend;
        RecordQtUiOverlayPerf(true, true, updateMs, renderMs, 0.0);
        return true;
    }

private:
    bool ensureApplication()
    {
        if (!QCoreApplication::instance()) {
#if !RO_PLATFORM_WINDOWS
            const char* qtQpaPlatform = std::getenv("QT_QPA_PLATFORM");
            const bool hasExplicitPlatform = qtQpaPlatform && *qtQpaPlatform;
            const bool isWsl = (std::getenv("WSL_DISTRO_NAME") && *std::getenv("WSL_DISTRO_NAME"))
                || (std::getenv("WSL_INTEROP") && *std::getenv("WSL_INTEROP"));
            if (isWsl && !hasExplicitPlatform) {
                qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("xcb"));
                DbgLog("[QtUi] WSL detected; defaulting QT_QPA_PLATFORM=xcb.\n");
            }
#endif
        }

        const RenderBackendType backend = GetCurrentUiRenderBackend();
        if (backend == RenderBackendType::Vulkan) {
            QQuickWindow::setGraphicsApi(QSGRendererInterface::Vulkan);
        }
#if RO_HAS_NATIVE_D3D11
        else if (backend == RenderBackendType::Direct3D11) {
            QQuickWindow::setGraphicsApi(QSGRendererInterface::Direct3D11);
        }
#endif
#if RO_HAS_NATIVE_D3D12
        else if (backend == RenderBackendType::Direct3D12) {
            QQuickWindow::setGraphicsApi(QSGRendererInterface::Direct3D12);
        }
#endif
        else {
            QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
        }

        if (QCoreApplication::instance()) {
            m_application = qobject_cast<QGuiApplication*>(QCoreApplication::instance());
            return m_application != nullptr;
        }

        m_application = new QGuiApplication(m_argc, m_argv);
        m_ownedApplication = true;
        return true;
    }

    bool ensureScene()
    {
        if (m_renderControl && m_quickWindow && m_engine && m_rootItem && m_stateAdapter) {
            return true;
        }

        EnsureQtUiResourcesInitialized();

        m_stateAdapter = new QtUiStateAdapter();
        m_renderControl = new QQuickRenderControl();
        m_quickWindow = new QQuickWindow(m_renderControl);
        m_quickWindow->setColor(Qt::transparent);
        m_quickWindow->setTitle(QStringLiteral("open-midgard-qt-ui"));

        m_engine = new QQmlEngine();
        m_engine->addImageProvider(QStringLiteral("openmidgard"), new QtUiImageProvider());
        m_engine->rootContext()->setContextProperty(QStringLiteral("uiState"), m_stateAdapter->stateObject());

        QQmlComponent component(m_engine, QUrl(QStringLiteral("qrc:/qtui/qml/GameOverlay.qml")));
        if (component.isError()) {
            const QList<QQmlError> errors = component.errors();
            for (const QQmlError& error : errors) {
                DbgLog("[QtUi] QML error: %s\n", error.toString().toLocal8Bit().constData());
            }
            return false;
        }

        QObject* createdObject = component.create();
        m_rootItem = qobject_cast<QQuickItem*>(createdObject);
        if (!m_rootItem) {
            delete createdObject;
            DbgLog("[QtUi] Failed to create QML root item.\n");
            return false;
        }

        m_rootItem->setParent(m_quickWindow);
        m_rootItem->setParentItem(m_quickWindow->contentItem());
        m_rootItem->setVisible(true);
        return true;
    }

    bool updateState(CGameMode& mode)
    {
        if (!m_stateAdapter) {
            return false;
        }
        return m_stateAdapter->syncGameplay(
            mode,
            GetCurrentUiRenderBackend(),
            m_nativeOverlayBackend,
            m_mouseX,
            m_mouseY);
    }

    bool updateMenuState()
    {
        if (!m_stateAdapter) {
            return false;
        }
        return m_stateAdapter->syncMenu(
            GetCurrentUiRenderBackend(),
            m_nativeOverlayBackend);
    }

    bool renderToImage(int width, int height)
    {
        if (!m_renderControl || !m_quickWindow || !m_rootItem || width <= 0 || height <= 0) {
            return false;
        }

        if (m_renderImage.size() != QSize(width, height) || m_renderImage.format() != QImage::Format_ARGB32_Premultiplied) {
            m_renderImage = QImage(width, height, QImage::Format_ARGB32_Premultiplied);
        }
        m_renderImage.fill(Qt::transparent);

        m_quickWindow->setGeometry(0, 0, width, height);
        m_quickWindow->setRenderTarget(QQuickRenderTarget::fromPaintDevice(&m_renderImage));
        if (m_quickWindow->contentItem()) {
            m_quickWindow->contentItem()->setSize(QSizeF(static_cast<qreal>(width), static_cast<qreal>(height)));
        }
        m_rootItem->setSize(QSizeF(static_cast<qreal>(width), static_cast<qreal>(height)));

        processEvents();
        m_renderControl->polishItems();
        m_renderControl->sync();
        m_renderControl->render();
        return true;
    }

    bool ensureVulkanRenderer(const QtUiRenderTargetInfo& targetInfo)
    {
        if (m_nativeGraphicsInitialized) {
            return m_nativeGraphicsBackend == RenderBackendType::Vulkan;
        }
        if (!m_renderControl || !m_quickWindow || !m_rootItem || !targetInfo.graphicsInstance
                || !targetInfo.graphicsPhysicalDevice || !targetInfo.graphicsDevice
                || targetInfo.queueFamilyIndex == 0xFFFFFFFFu) {
            return false;
        }

        m_vulkanInstance = new QVulkanInstance();
        m_vulkanInstance->setVkInstance(reinterpret_cast<VkInstance>(targetInfo.graphicsInstance));
        if (!m_vulkanInstance->create()) {
            DbgLog("[QtUi] QVulkanInstance::create failed for imported renderer instance.\n");
            delete m_vulkanInstance;
            m_vulkanInstance = nullptr;
            return false;
        }

        m_quickWindow->setVulkanInstance(m_vulkanInstance);
        m_quickWindow->setGraphicsDevice(QQuickGraphicsDevice::fromDeviceObjects(
            reinterpret_cast<VkPhysicalDevice>(targetInfo.graphicsPhysicalDevice),
            reinterpret_cast<VkDevice>(targetInfo.graphicsDevice),
            static_cast<int>(targetInfo.queueFamilyIndex),
            0));
        if (!m_renderControl->initialize()) {
            DbgLog("[QtUi] QQuickRenderControl::initialize failed for Vulkan.\n");
            return false;
        }

        m_nativeTargetMirrorVertically = false;
        DbgLog("[QtUi] Vulkan native overlay target does not use vertical mirroring.\n");
        m_nativeGraphicsInitialized = true;
        m_nativeGraphicsBackend = RenderBackendType::Vulkan;
        return true;
    }

#if RO_HAS_NATIVE_D3D11
    bool ensureD3D11Renderer(const QtUiRenderTargetInfo& targetInfo)
    {
        if (m_nativeGraphicsInitialized) {
            return m_nativeGraphicsBackend == RenderBackendType::Direct3D11;
        }
        if (!m_renderControl || !m_quickWindow || !m_rootItem
                || !targetInfo.graphicsDevice || !targetInfo.graphicsQueueOrContext) {
            return false;
        }

        m_quickWindow->setGraphicsDevice(QQuickGraphicsDevice::fromDeviceAndContext(
            targetInfo.graphicsDevice,
            targetInfo.graphicsQueueOrContext));
        if (!m_renderControl->initialize()) {
            DbgLog("[QtUi] QQuickRenderControl::initialize failed for D3D11.\n");
            return false;
        }

        m_nativeTargetMirrorVertically = false;
        m_nativeGraphicsInitialized = true;
        m_nativeGraphicsBackend = RenderBackendType::Direct3D11;
        return true;
    }
#endif

#if RO_PLATFORM_WINDOWS && RO_HAS_NATIVE_D3D12
    bool ensureD3D12Renderer(const QtUiRenderTargetInfo& targetInfo)
    {
        if (m_nativeGraphicsInitialized) {
            return m_nativeGraphicsBackend == RenderBackendType::Direct3D12;
        }
        if (!m_renderControl || !m_quickWindow || !m_rootItem
                || !targetInfo.graphicsDevice || !targetInfo.graphicsQueueOrContext) {
            return false;
        }

        QRhiD3D12InitParams params{};
        QRhiD3D12NativeHandles importDevice{};
        importDevice.dev = targetInfo.graphicsDevice;
        importDevice.commandQueue = targetInfo.graphicsQueueOrContext;
        importDevice.minimumFeatureLevel = static_cast<int>(targetInfo.minimumFeatureLevel);
        importDevice.adapterLuidLow = targetInfo.adapterLuidLow;
        importDevice.adapterLuidHigh = targetInfo.adapterLuidHigh;
        m_qtRhi = QRhi::create(QRhi::D3D12, &params, {}, &importDevice);
        if (!m_qtRhi) {
            DbgLog("[QtUi] QRhi::create failed for imported D3D12 device.\n");
            return false;
        }

        m_quickWindow->setGraphicsDevice(QQuickGraphicsDevice::fromRhi(m_qtRhi));
        if (!m_renderControl->initialize()) {
            DbgLog("[QtUi] QQuickRenderControl::initialize failed for D3D12.\n");
            return false;
        }

        m_nativeTargetMirrorVertically = false;
        m_nativeGraphicsInitialized = true;
        m_nativeGraphicsBackend = RenderBackendType::Direct3D12;
        return true;
    }
#endif

    bool renderToVulkanTexture(const QtUiRenderTargetInfo& targetInfo, int width, int height)
    {
        if (!m_renderControl || !m_quickWindow || !m_rootItem || width <= 0 || height <= 0
                || !targetInfo.colorTarget || targetInfo.colorImageLayout == 0u || targetInfo.colorFormat == 0u) {
            return false;
        }
        if (!ensureVulkanRenderer(targetInfo)) {
            return false;
        }

        QQuickRenderTarget quickRenderTarget = QQuickRenderTarget::fromVulkanImage(
            reinterpret_cast<VkImage>(targetInfo.colorTarget),
            static_cast<VkImageLayout>(targetInfo.colorImageLayout),
            static_cast<VkFormat>(targetInfo.colorFormat),
            QSize(width, height),
            1);
        quickRenderTarget.setMirrorVertically(m_nativeTargetMirrorVertically);

        m_quickWindow->setGeometry(0, 0, width, height);
        m_quickWindow->setRenderTarget(quickRenderTarget);
        if (m_quickWindow->contentItem()) {
            m_quickWindow->contentItem()->setSize(QSizeF(static_cast<qreal>(width), static_cast<qreal>(height)));
        }
        m_rootItem->setSize(QSizeF(static_cast<qreal>(width), static_cast<qreal>(height)));

        processEvents();
        m_renderControl->polishItems();
        m_renderControl->beginFrame();
        m_renderControl->sync();
        m_renderControl->render();
        m_renderControl->endFrame();
        return true;
    }

#if RO_HAS_NATIVE_D3D11
    bool renderToD3D11Texture(const QtUiRenderTargetInfo& targetInfo, int width, int height)
    {
        if (!m_renderControl || !m_quickWindow || !m_rootItem || width <= 0 || height <= 0
                || !targetInfo.colorTarget || targetInfo.colorFormat == 0u) {
            return false;
        }
        if (!ensureD3D11Renderer(targetInfo)) {
            return false;
        }

        QQuickRenderTarget quickRenderTarget = QQuickRenderTarget::fromD3D11Texture(
            targetInfo.colorTarget,
            targetInfo.colorFormat,
            QSize(width, height),
            static_cast<int>((std::max)(1u, targetInfo.targetSampleCount)));
        quickRenderTarget.setMirrorVertically(false);

        m_quickWindow->setGeometry(0, 0, width, height);
        m_quickWindow->setRenderTarget(quickRenderTarget);
        if (m_quickWindow->contentItem()) {
            m_quickWindow->contentItem()->setSize(QSizeF(static_cast<qreal>(width), static_cast<qreal>(height)));
        }
        m_rootItem->setSize(QSizeF(static_cast<qreal>(width), static_cast<qreal>(height)));

        processEvents();
        m_renderControl->polishItems();
        m_renderControl->beginFrame();
        m_renderControl->sync();
        m_renderControl->render();
        m_renderControl->endFrame();
        return true;
    }
#endif

#if RO_PLATFORM_WINDOWS && RO_HAS_NATIVE_D3D12
    bool renderToD3D12Texture(const QtUiRenderTargetInfo& targetInfo, int width, int height)
    {
        if (!m_renderControl || !m_quickWindow || !m_rootItem || width <= 0 || height <= 0
                || !targetInfo.colorTarget || targetInfo.colorFormat == 0u || targetInfo.colorTargetState == 0u) {
            return false;
        }
        if (!ensureD3D12Renderer(targetInfo)) {
            return false;
        }

        QQuickRenderTarget quickRenderTarget = QQuickRenderTarget::fromD3D12Texture(
            targetInfo.colorTarget,
            static_cast<int>(targetInfo.colorTargetState),
            targetInfo.colorFormat,
            QSize(width, height),
            static_cast<int>((std::max)(1u, targetInfo.targetSampleCount)));
        quickRenderTarget.setMirrorVertically(false);

        m_quickWindow->setGeometry(0, 0, width, height);
        m_quickWindow->setRenderTarget(quickRenderTarget);
        if (m_quickWindow->contentItem()) {
            m_quickWindow->contentItem()->setSize(QSizeF(static_cast<qreal>(width), static_cast<qreal>(height)));
        }
        m_rootItem->setSize(QSizeF(static_cast<qreal>(width), static_cast<qreal>(height)));

        processEvents();
        m_renderControl->polishItems();
        m_renderControl->beginFrame();
        m_renderControl->sync();
        m_renderControl->render();
        m_renderControl->endFrame();
        return true;
    }
#endif

    bool m_active = false;
    bool m_ownedApplication = false;
    bool m_nativeGraphicsInitialized = false;
    bool m_nativeTargetMirrorVertically = false;
    bool m_loggedNativeOverlaySuccess = false;
    bool m_loggedNativeOverlayFailure = false;
    bool m_loggedMenuCpuBridge = false;
    bool m_loggedGameplayCpuBridge = false;
    bool m_loggedMenuTextureUnavailable = false;
    bool m_loggedGameplayTextureUnavailable = false;
    RoNativeWindowHandle m_mainWindow = nullptr;
    int m_mouseX = 0;
    int m_mouseY = 0;
    MenuPointerCaptureTarget m_menuPointerCaptureTarget = MenuPointerCaptureTarget::None;
    QString m_lastInput;

    int m_argc = 1;
    char m_arg0[32] = "open-midgard";
    char* m_argv[2] = { m_arg0, nullptr };

    QGuiApplication* m_application = nullptr;
    QtUiStateAdapter* m_stateAdapter = nullptr;
    QQuickRenderControl* m_renderControl = nullptr;
    QQuickWindow* m_quickWindow = nullptr;
    QQmlEngine* m_engine = nullptr;
    QQuickItem* m_rootItem = nullptr;
#if RO_PLATFORM_WINDOWS && RO_HAS_NATIVE_D3D12
    QRhi* m_qtRhi = nullptr;
#endif
    QVulkanInstance* m_vulkanInstance = nullptr;
    QImage m_renderImage;
    RenderBackendType m_nativeGraphicsBackend = RenderBackendType::LegacyDirect3D7;
    RenderBackendType m_nativeOverlayBackend = RenderBackendType::LegacyDirect3D7;
};

QtUiRuntimeHost& Runtime()
{
    static QtUiRuntimeHost runtime;
    return runtime;
}

} // namespace

#endif

bool IsQtUiRuntimeCompiled()
{
#if RO_ENABLE_QT6_UI
    return true;
#else
    return false;
#endif
}

bool IsQtUiRuntimeEnabled()
{
#if RO_ENABLE_QT6_UI
    return IsEnabledInEnvironment();
#else
    return false;
#endif
}

void InitializeQtUiRuntime(RoNativeWindowHandle mainWindow)
{
#if RO_ENABLE_QT6_UI
    Runtime().initialize(mainWindow);
#else
    (void)mainWindow;
#endif
}

void ShutdownQtUiRuntime()
{
#if RO_ENABLE_QT6_UI
    Runtime().shutdown();
#endif
}

void ProcessQtUiRuntimeEvents()
{
#if RO_ENABLE_QT6_UI
    Runtime().processEvents();
#endif
}

void NotifyQtUiRuntimeWindowMessage(RoWindowMessage msg, RoWindowWParam wParam, RoWindowLParam lParam)
{
#if RO_ENABLE_QT6_UI
    Runtime().notifyWindowMessage(msg, wParam, lParam);
#else
    (void)msg;
    (void)wParam;
    (void)lParam;
#endif
}

bool HandleQtUiRuntimeWindowMessage(RoWindowMessage msg, RoWindowWParam wParam, RoWindowLParam lParam)
{
#if RO_ENABLE_QT6_UI
    return Runtime().handleWindowMessage(msg, wParam, lParam);
#else
    (void)msg;
    (void)wParam;
    (void)lParam;
    return false;
#endif
}

bool CompositeQtUiMenuOverlay(void* bgraPixels, int width, int height, int pitch)
{
#if RO_ENABLE_QT6_UI
    return Runtime().compositeMenuOverlay(bgraPixels, width, height, pitch);
#else
    (void)bgraPixels;
    (void)width;
    (void)height;
    (void)pitch;
    return false;
#endif
}

bool RenderQtUiMenuOverlayTexture(CTexture* texture, int width, int height)
{
#if RO_ENABLE_QT6_UI
    return Runtime().renderMenuOverlayTexture(texture, width, height);
#else
    (void)texture;
    (void)width;
    (void)height;
    return false;
#endif
}

bool CompositeQtUiGameplayOverlay(CGameMode& mode, void* bgraPixels, int width, int height, int pitch)
{
#if RO_ENABLE_QT6_UI
    return Runtime().compositeGameplayOverlay(mode, bgraPixels, width, height, pitch);
#else
    (void)mode;
    (void)bgraPixels;
    (void)width;
    (void)height;
    (void)pitch;
    return false;
#endif
}

bool RenderQtUiGameplayOverlayTexture(CGameMode& mode, CTexture* texture, int width, int height)
{
#if RO_ENABLE_QT6_UI
    return Runtime().renderGameplayOverlayTexture(mode, texture, width, height);
#else
    (void)mode;
    (void)texture;
    (void)width;
    (void)height;
    return false;
#endif
}
