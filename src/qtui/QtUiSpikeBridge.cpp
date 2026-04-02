#include "QtUiRuntime.h"

#include "QtUiStateAdapter.h"

#include "DebugLog.h"
#include "gamemode/GameMode.h"
#include "gamemode/View.h"
#include "main/WinMain.h"
#include "render3d/RenderBackend.h"
#include "render3d/RenderDevice.h"
#include "session/Session.h"
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

#include <rhi/qrhi.h>
#include <rhi/qrhi_platform.h>

void EnsureQtUiResourcesInitialized()
{
    static bool initialized = false;
    if (!initialized) {
        Q_INIT_RESOURCE(QtUiSpikeAssets);
        initialized = true;
    }
}

namespace {

bool TryBuildItemIconImage(unsigned int itemId, QImage* outImage)
{
    if (!outImage || itemId == 0) {
        return false;
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
        QString baseId = id;
        const int queryPos = baseId.indexOf(QLatin1Char('?'));
        if (queryPos >= 0) {
            baseId.truncate(queryPos);
        }

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
                }
            }
        } else if (baseId.startsWith(QStringLiteral("item/"))) {
            bool ok = false;
            const unsigned int itemId = baseId.mid(QStringLiteral("item/").size()).toUInt(&ok);
            if (ok) {
                TryBuildItemIconImage(itemId, &image);
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
        }

        if (!image.isNull() && requestedSize.isValid() && requestedSize.width() > 0 && requestedSize.height() > 0) {
            image = image.scaled(requestedSize, Qt::IgnoreAspectRatio, Qt::FastTransformation);
        }

        if (size) {
            *size = image.size();
        }
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
    bool initialize(HWND mainWindow)
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

        delete m_qtRhi;
        m_qtRhi = nullptr;

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

    void notifyWindowMessage(UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch (msg) {
        case WM_MOUSEMOVE:
            m_mouseX = static_cast<int>(static_cast<short>(LOWORD(lParam)));
            m_mouseY = static_cast<int>(static_cast<short>(HIWORD(lParam)));
            m_lastInput = QStringLiteral("Mouse move %1,%2").arg(m_mouseX).arg(m_mouseY);
            break;
        case WM_LBUTTONDOWN:
            m_lastInput = QStringLiteral("Left click %1,%2")
                .arg(static_cast<int>(static_cast<short>(LOWORD(lParam))))
                .arg(static_cast<int>(static_cast<short>(HIWORD(lParam))));
            break;
        case WM_RBUTTONDOWN:
            m_lastInput = QStringLiteral("Right click %1,%2")
                .arg(static_cast<int>(static_cast<short>(LOWORD(lParam))))
                .arg(static_cast<int>(static_cast<short>(HIWORD(lParam))));
            break;
        case WM_MOUSEWHEEL:
            m_lastInput = QStringLiteral("Mouse wheel %1").arg(GET_WHEEL_DELTA_WPARAM(wParam));
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

    bool handleWindowMessage(UINT msg, WPARAM wParam, LPARAM lParam)
    {
        notifyWindowMessage(msg, wParam, lParam);
        if (!m_active) {
            return false;
        }

        switch (msg) {
        case WM_MOUSEMOVE:
            if (g_windowMgr.m_selectServerWnd && g_windowMgr.m_selectServerWnd->m_show != 0) {
                g_windowMgr.m_selectServerWnd->OnMouseMove(
                    static_cast<int>(static_cast<short>(LOWORD(lParam))),
                    static_cast<int>(static_cast<short>(HIWORD(lParam))));
                return true;
            }
            return false;

        case WM_LBUTTONDBLCLK:
            if (g_windowMgr.m_selectCharWnd
                && g_windowMgr.m_selectCharWnd->m_show != 0
                && g_windowMgr.m_selectCharWnd->HandleQtDoubleClick(
                    static_cast<int>(static_cast<short>(LOWORD(lParam))),
                    static_cast<int>(static_cast<short>(HIWORD(lParam))))) {
                m_menuPointerCaptureTarget = MenuPointerCaptureTarget::None;
                return true;
            }
            [[fallthrough]];

        case WM_LBUTTONDOWN:
            if (g_windowMgr.m_makeCharWnd
                && g_windowMgr.m_makeCharWnd->m_show != 0
                && g_windowMgr.m_makeCharWnd->HandleQtMouseDown(
                    static_cast<int>(static_cast<short>(LOWORD(lParam))),
                    static_cast<int>(static_cast<short>(HIWORD(lParam))))) {
                m_menuPointerCaptureTarget = MenuPointerCaptureTarget::MakeChar;
                return true;
            }
            if (g_windowMgr.m_selectServerWnd
                && g_windowMgr.m_selectServerWnd->m_show != 0
                && g_windowMgr.m_selectServerWnd->HandleQtMouseDown(
                    static_cast<int>(static_cast<short>(LOWORD(lParam))),
                    static_cast<int>(static_cast<short>(HIWORD(lParam))))) {
                m_menuPointerCaptureTarget = MenuPointerCaptureTarget::ServerSelect;
                return true;
            }
            if (g_windowMgr.m_selectCharWnd
                && g_windowMgr.m_selectCharWnd->m_show != 0
                && g_windowMgr.m_selectCharWnd->HandleQtMouseDown(
                    static_cast<int>(static_cast<short>(LOWORD(lParam))),
                    static_cast<int>(static_cast<short>(HIWORD(lParam))))) {
                m_menuPointerCaptureTarget = MenuPointerCaptureTarget::CharSelect;
                return true;
            }
            if (g_windowMgr.m_loginWnd
                && g_windowMgr.m_loginWnd->m_show != 0
                && g_windowMgr.m_loginWnd->HandleQtMouseDown(
                    static_cast<int>(static_cast<short>(LOWORD(lParam))),
                    static_cast<int>(static_cast<short>(HIWORD(lParam))))) {
                m_menuPointerCaptureTarget = MenuPointerCaptureTarget::Login;
                return true;
            }
            if (g_windowMgr.m_notifyLevelUpWnd
                && g_windowMgr.m_notifyLevelUpWnd->m_show != 0
                && g_windowMgr.m_notifyLevelUpWnd->HandleQtMouseDown(
                    static_cast<int>(static_cast<short>(LOWORD(lParam))),
                    static_cast<int>(static_cast<short>(HIWORD(lParam))))) {
                m_menuPointerCaptureTarget = MenuPointerCaptureTarget::NotifyLevelUp;
                return true;
            }
            if (g_windowMgr.m_notifyJobLevelUpWnd
                && g_windowMgr.m_notifyJobLevelUpWnd->m_show != 0
                && g_windowMgr.m_notifyJobLevelUpWnd->HandleQtMouseDown(
                    static_cast<int>(static_cast<short>(LOWORD(lParam))),
                    static_cast<int>(static_cast<short>(HIWORD(lParam))))) {
                m_menuPointerCaptureTarget = MenuPointerCaptureTarget::NotifyJobLevelUp;
                return true;
            }
            return false;

        case WM_LBUTTONUP:
            if (m_menuPointerCaptureTarget == MenuPointerCaptureTarget::MakeChar) {
                if (g_windowMgr.m_makeCharWnd
                    && g_windowMgr.m_makeCharWnd->m_show != 0) {
                    g_windowMgr.m_makeCharWnd->HandleQtMouseUp(
                        static_cast<int>(static_cast<short>(LOWORD(lParam))),
                        static_cast<int>(static_cast<short>(HIWORD(lParam))));
                }
                m_menuPointerCaptureTarget = MenuPointerCaptureTarget::None;
                return true;
            }
            if (m_menuPointerCaptureTarget == MenuPointerCaptureTarget::CharSelect) {
                if (g_windowMgr.m_selectCharWnd
                    && g_windowMgr.m_selectCharWnd->m_show != 0
                    && g_windowMgr.m_selectCharWnd->HandleQtMouseUp(
                        static_cast<int>(static_cast<short>(LOWORD(lParam))),
                        static_cast<int>(static_cast<short>(HIWORD(lParam))))) {
                }
                m_menuPointerCaptureTarget = MenuPointerCaptureTarget::None;
                return true;
            }
            if (m_menuPointerCaptureTarget == MenuPointerCaptureTarget::NotifyLevelUp) {
                if (g_windowMgr.m_notifyLevelUpWnd
                    && g_windowMgr.m_notifyLevelUpWnd->m_show != 0) {
                    g_windowMgr.m_notifyLevelUpWnd->HandleQtMouseUp(
                        static_cast<int>(static_cast<short>(LOWORD(lParam))),
                        static_cast<int>(static_cast<short>(HIWORD(lParam))));
                }
                m_menuPointerCaptureTarget = MenuPointerCaptureTarget::None;
                return true;
            }
            if (m_menuPointerCaptureTarget == MenuPointerCaptureTarget::NotifyJobLevelUp) {
                if (g_windowMgr.m_notifyJobLevelUpWnd
                    && g_windowMgr.m_notifyJobLevelUpWnd->m_show != 0) {
                    g_windowMgr.m_notifyJobLevelUpWnd->HandleQtMouseUp(
                        static_cast<int>(static_cast<short>(LOWORD(lParam))),
                        static_cast<int>(static_cast<short>(HIWORD(lParam))));
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
        if (!updateMenuState() || !renderToImage(width, height)) {
            return false;
        }

        if (!m_loggedMenuCpuBridge) {
            DbgLog("[QtUi] Menu overlay using CPU bridge fallback (%dx%d).\n", width, height);
            m_loggedMenuCpuBridge = true;
        }
        BlendQtImageOntoBgraBuffer(m_renderImage, bgraPixels, width, height, pitch);
        return true;
    }

    bool renderMenuOverlayTexture(CTexture* texture, int width, int height)
    {
        if (!m_active || !texture || width <= 0 || height <= 0) {
            return false;
        }
        if (!updateMenuState()) {
            return false;
        }

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
        switch (targetInfo.backend) {
        case RenderBackendType::Direct3D11:
            rendered = renderToD3D11Texture(targetInfo, targetWidth, targetHeight);
            break;
        case RenderBackendType::Direct3D12:
            rendered = renderToD3D12Texture(targetInfo, targetWidth, targetHeight);
            break;
        case RenderBackendType::Vulkan:
            rendered = renderToVulkanTexture(targetInfo, targetWidth, targetHeight);
            break;
        default:
            rendered = false;
            break;
        }

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
        if (!updateState(mode) || !renderToImage(width, height)) {
            return false;
        }

        if (!m_loggedGameplayCpuBridge) {
            DbgLog("[QtUi] Gameplay overlay using CPU bridge fallback (%dx%d).\n", width, height);
            m_loggedGameplayCpuBridge = true;
        }
        BlendQtImageOntoBgraBuffer(m_renderImage, bgraPixels, width, height, pitch);
        return true;
    }

    bool renderGameplayOverlayTexture(CGameMode& mode, CTexture* texture, int width, int height)
    {
        if (!m_active || !texture || width <= 0 || height <= 0) {
            return false;
        }
        if (!updateState(mode)) {
            return false;
        }

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
        switch (targetInfo.backend) {
        case RenderBackendType::Direct3D11:
            rendered = renderToD3D11Texture(targetInfo, targetWidth, targetHeight);
            break;
        case RenderBackendType::Direct3D12:
            rendered = renderToD3D12Texture(targetInfo, targetWidth, targetHeight);
            break;
        case RenderBackendType::Vulkan:
            rendered = renderToVulkanTexture(targetInfo, targetWidth, targetHeight);
            break;
        default:
            rendered = false;
            break;
        }

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
        return true;
    }

private:
    bool ensureApplication()
    {
        if (QCoreApplication::instance()) {
            m_application = qobject_cast<QGuiApplication*>(QCoreApplication::instance());
            return m_application != nullptr;
        }

        if (GetActiveRenderBackend() == RenderBackendType::Vulkan) {
            QQuickWindow::setGraphicsApi(QSGRendererInterface::Vulkan);
        } else if (GetActiveRenderBackend() == RenderBackendType::Direct3D11) {
            QQuickWindow::setGraphicsApi(QSGRendererInterface::Direct3D11);
        } else if (GetActiveRenderBackend() == RenderBackendType::Direct3D12) {
            QQuickWindow::setGraphicsApi(QSGRendererInterface::Direct3D12);
        } else {
            QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
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
            GetActiveRenderBackend(),
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
            GetActiveRenderBackend(),
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
    HWND m_mainWindow = nullptr;
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
    QRhi* m_qtRhi = nullptr;
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

void InitializeQtUiRuntime(HWND mainWindow)
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

void NotifyQtUiRuntimeWindowMessage(UINT msg, WPARAM wParam, LPARAM lParam)
{
#if RO_ENABLE_QT6_UI
    Runtime().notifyWindowMessage(msg, wParam, lParam);
#else
    (void)msg;
    (void)wParam;
    (void)lParam;
#endif
}

bool HandleQtUiRuntimeWindowMessage(UINT msg, WPARAM wParam, LPARAM lParam)
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
