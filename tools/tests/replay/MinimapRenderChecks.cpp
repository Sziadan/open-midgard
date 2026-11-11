#include "gamemode/GameMode.h"
#include "gamemode/GameModePacket.h"
#include "gamemode/LoginMode.h"
#include "gamemode/Mode.h"
#include "qtui/QtUiStateAdapter.h"
#include "qtui/QtUiState.h"
#include "render/DC.h"
#include "session/Session.h"
#include "ui/UIMinimapWnd.h"
#include "ui/UIWindowMgr.h"
#include "world/World.h"
#include <QGuiApplication>
#include <QQuickView>
#include <QQuickItem>
#include <QQuickImageProvider>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlError>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QFontDatabase>
#include <QFont>
#include <QPainter>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QThread>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <stdexcept>

struct SkillPacketReplayAccess {
    static void SetCurrent(CGameMode* mode) {
        g_modeMgr.m_curMode = mode;
        g_modeMgr.m_curModeType = mode ? 1 : 0;
    }
};

namespace {
void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

class MinimapImages final : public QQuickImageProvider {
public:
    explicit MinimapImages(UIRoMapWnd& map) : QQuickImageProvider(Image), map_(map) {}
    QImage requestImage(const QString& id, QSize* size, const QSize&) override {
        QImage result;
        if (id.startsWith("minimap")) Require(map_.BuildQtMinimapImage(&result), "production Qt minimap image failed");
        else { result = QImage(1, 1, QImage::Format_ARGB32); result.fill(Qt::transparent); }
        if (size) *size = result.size();
        return result;
    }
private:
    UIRoMapWnd& map_;
};

std::vector<u8> MemberRecord(bool offline, const char* map = "pk14_fixture.gat") {
    std::vector<u8> bytes(81, 0);
    bytes[0] = 0xe9; bytes[1] = 1; bytes[2] = 42; bytes[6] = 1;
    bytes[14] = offline ? 1 : 0;
    std::memcpy(bytes.data() + 15, "Render party", 12);
    std::memcpy(bytes.data() + 39, "Marker peer", 11);
    std::memcpy(bytes.data() + 63, map, (std::min)(std::strlen(map), size_t(16)));
    return bytes;
}

std::vector<u8> RosterRecord() {
    std::vector<u8> bytes(74, 0);
    bytes[0] = 0xfb; bytes[2] = 74;
    std::memcpy(bytes.data() + 4, "Render party", 12);
    bytes[28] = 42;
    std::memcpy(bytes.data() + 32, "Marker peer", 11);
    std::memcpy(bytes.data() + 56, "pk14_fixture.gat", 16);
    bytes[72] = 1;
    return bytes;
}

std::vector<u8> PositionRecord(u16 x, u16 y) {
    return {7,1,42,0,0,0,u8(x),u8(x>>8),u8(y),u8(y>>8)};
}

std::vector<u8> CompassRecord(u32 type, u32 x, u32 y, u8 id, u32 color) {
    std::vector<u8> bytes(23, 0);
    bytes[0] = 0x44; bytes[1] = 0x01;
    const auto put32 = [&bytes](size_t offset, u32 value) {
        for (int i = 0; i < 4; ++i) bytes[offset + size_t(i)] = u8(value >> (i * 8));
    };
    put32(6, type); put32(10, x); put32(14, y);
    bytes[18] = id;
    put32(19, color);
    return bytes;
}

void Dispatch(CGameMode& mode, const std::vector<u8>& bytes) {
    PacketView packet{}; int consumed = 0;
    Require(TryReadPacket(bytes.data(), int(bytes.size()), packet, consumed), "render fixture did not frame");
    Require(consumed == int(bytes.size()), "render fixture left bytes unframed");
    Require(GetProductionGameModePacketRouter().Dispatch(mode, packet), "render fixture missed production router");
}

QImage NativeImage(UIRoMapWnd& map) {
    map.OnProcess();
    ArgbDibSurface surface;
    Require(surface.EnsureSize(map.m_w, map.m_h), "native DIB allocation failed");
    std::fill_n(surface.GetPixels(), map.m_w * map.m_h, 0xff181b20u);
    map.DrawToHdc(surface.GetDC(), 0, 0);
    return QImage(reinterpret_cast<const uchar*>(surface.GetPixels()), map.m_w, map.m_h,
        map.m_w * 4, QImage::Format_RGB32).copy();
}

QImage QtImage(QGuiApplication& app, QQuickView& view) {
    view.requestUpdate();
    for (int i=0; i<6; ++i) { app.processEvents(); QThread::msleep(20); }
    QImage image = view.grabWindow();
    Require(!image.isNull(), "Qt QQuickView grab failed");
    return image.convertToFormat(QImage::Format_RGB32);
}

void SaveImage(const QImage& image, const QString& path) {
    Require(image.save(path), "render PNG save failed");
}

bool IsRgb(const QImage& image, int x, int y, QRgb rgb) {
    return (image.pixel(x,y) & 0x00ffffffu) == (rgb & 0x00ffffffu);
}

void RequireCompassCross(const QImage& image, int x, int y, QRgb color, const char* message) {
    for (int dx = -4; dx < 4; ++dx) Require(IsRgb(image, x + dx, y, color), message);
    for (int dy = -4; dy < 4; ++dy) Require(IsRgb(image, x, y + dy, color), message);
}
}

int main(int argc, char** argv) {
    std::cerr << "minimap-render: main\n";
    qputenv("QT_QUICK_BACKEND", "software");
    qputenv("QT_QPA_PLATFORM", "offscreen");
    // The app deploys qwindows only; this test needs the matching kit's offscreen plugin.
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM_PLUGIN_PATH"))
        qputenv("QT_QPA_PLATFORM_PLUGIN_PATH", RO_PACKET_QT_PLATFORM_PLUGINS);
    QGuiApplication app(argc,argv);
    std::cerr << "minimap-render: Qt application ready\n";
    try {
        const QString fontPath = QString::fromUtf8(RO_PACKET_SOURCE_DIR)+"/third_party/fonts/NotoSans.ttf";
        Require(QFontDatabase::addApplicationFont(fontPath)>=0,"bundled UI font did not load");
        QGuiApplication::setFont(QFont("Noto Sans"));
        const QString output = QDir::cleanPath(argc>1 ? QString::fromLocal8Bit(argv[1]) : QDir::currentPath()+"/minimap-renders");
        Require(QDir().mkpath(output), "render output directory failed");
        Require(QDir::setCurrent(output), "render fixture working directory failed");
        QImage bitmap(64,64,QImage::Format_RGB32); bitmap.fill(QColor("#324d65"));
        Require(bitmap.save("pk14_fixture.bmp"), "synthetic minimap BMP save failed");
        std::cerr << "minimap-render: bitmap ready\n";

        CWorld world;
        world.m_rootNode.m_attrArea = {0,0,100,100};
        CGameMode mode;
        mode.m_world = &world;
        std::snprintf(mode.m_minimapBmpName,sizeof(mode.m_minimapBmpName),"pk14_fixture.bmp");
        SkillPacketReplayAccess::SetCurrent(&mode);
        g_session.m_aid = 1;
        std::snprintf(g_session.m_curMap,sizeof(g_session.m_curMap),"pk14_fixture");
        g_session.SetPlayerPosDir(50,50,0);
        g_session.ClearParty();
        g_windowMgr.m_miniMapZoomFactor = 1.0f;
        g_windowMgr.m_isDrawCompass = 0;
        UIRoMapWnd map;
        map.Move(0,0);
        g_windowMgr.m_roMapWnd = &map;
        map.SetShow(1);
        Require(map.HasLoadedMinimap() && map.m_show, "synthetic minimap did not load");
        std::cerr << "minimap-render: native window ready\n";
        RegisterDefaultGameModePacketHandlers(GetProductionGameModePacketRouter());
        QtUiStateAdapter adapter;
        auto* state = qobject_cast<QtUiState*>(adapter.stateObject());
        Require(state != nullptr, "adapter has no real QtUiState");
        state->setLoadingVisible(false);
        state->setUiScale(1.0);
        QQuickView view;
        view.setColor(QColor("#181b20"));
        view.resize(map.m_w,map.m_h);
        view.setResizeMode(QQuickView::SizeRootObjectToView);
        view.rootContext()->setContextProperty("uiState",adapter.stateObject());
        view.engine()->addImageProvider("openmidgard",new MinimapImages(map));
        int qmlErrors = 0;
        QObject::connect(view.engine(),&QQmlEngine::warnings,[&](const QList<QQmlError>& errors) {
            qmlErrors += int(errors.size());
            for (const auto& error:errors) std::cerr << error.toString().toStdString() << '\n';
        });
        view.setSource(QUrl::fromLocalFile(QString::fromUtf8(RO_PACKET_SOURCE_DIR)+"/src/qtui/qml/GameOverlay.qml"));
        Require(view.status()!=QQuickView::Error,"production GameOverlay QML did not load");
        view.show();
        std::cerr << "minimap-render: QML ready\n";
        QJsonArray cases;
        std::vector<QImage> nativeContacts, qtContacts;
        std::vector<QString> caseNames;
        QImage lastNative, lastQt;
        constexpr QRgb partyRgb = 0x000a31b8u; // First source roster accent, packed RRGGBB.
        auto capture = [&](const char* name, int x, int y, bool visible,
                           int expectedCount = -1, QRgb nativeColor = 0x000a31b8u, QRgb qtColor = 0x000a31b8u) {
            adapter.syncMinimap();
            Require(state->minimapVisible(),"production adapter suppressed synthetic loaded minimap");
            const auto model = state->minimapData().value("markers").toList();
            Require(model.size() == (expectedCount < 0 ? int(visible) : expectedCount),"production adapter marker count differs from boundary");
            QImage native = NativeImage(map);
            QImage qt = QtImage(app,view);
            const QString base = QString::fromLatin1(name);
            SaveImage(native,output+"/"+base+"-native.png");
            SaveImage(qt,output+"/"+base+"-qt.png");
            QJsonObject row{{"case",base},{"expected_visible",visible},{"x",x},{"y",y},
                {"native_rgb",QColor(native.pixel(x,y)).name()},{"qt_rgb",QColor(qt.pixel(x,y)).name()}};
            cases.push_back(row);
            std::cout << QJsonDocument(row).toJson(QJsonDocument::Compact).toStdString() << '\n';
            Require(IsRgb(native,x,y,visible ? nativeColor : 0x00324d65u),"native dot/background pixels differ from expected boundary/color");
            Require(IsRgb(qt,x,y,visible ? qtColor : 0x00324d65u),"Qt dot/background pixels differ from expected boundary/color");
            nativeContacts.push_back(native); qtContacts.push_back(qt); caseNames.push_back(base);
            lastNative = native; lastQt = qt;
        };

        Dispatch(mode,MemberRecord(false));
        Dispatch(mode,PositionRecord(25,35));
        Require(g_session.FindPartyMemberByAid(42)->color == 0xff0a31b8u,"unexpected source roster accent");
        capture("online",51,126,true);
        Dispatch(mode,RosterRecord());
        capture("stationary-roster",51,126,true);
        Dispatch(mode,PositionRecord(75,65));
        capture("online-movement",133,77,true);
        Require(IsRgb(lastNative,51,126,0x00324d65u) && IsRgb(lastQt,51,126,0x00324d65u),
            "movement left a stale dot at the prior position");
        Dispatch(mode,PositionRecord(25,35));
        capture("moved-back",51,126,true);
        Require(IsRgb(lastNative,133,77,0x00324d65u) && IsRgb(lastQt,133,77,0x00324d65u),
            "movement back left a stale dot at the second position");
        Dispatch(mode,MemberRecord(true));
        capture("offline",51,126,false);
        Dispatch(mode,MemberRecord(false));
        capture("online-without-position",51,126,false);
        Dispatch(mode,PositionRecord(75,65));
        capture("fresh-position",133,77,true);
        Dispatch(mode,PositionRecord(0xffff,0xffff));
        capture("sentinel-removal",133,77,false);
        Dispatch(mode,PositionRecord(25,35));
        std::vector<u8> emptyRoster(28,0); emptyRoster[0]=0xfb; emptyRoster[2]=28;
        Dispatch(mode,emptyRoster);
        Require(g_session.GetNumParty()==0,"empty full roster retained its former member");
        capture("roster-loss",51,126,false);
        Dispatch(mode,MemberRecord(false));
        Dispatch(mode,PositionRecord(25,35));
        Dispatch(mode,MemberRecord(false,"payon.gat"));
        capture("other-map",51,126,false);
        Dispatch(mode,PositionRecord(25,35));
        capture("other-map-position-rejected",51,126,false);
        Dispatch(mode,MemberRecord(false));
        Dispatch(mode,PositionRecord(25,35));
        mode.m_guildPosList[77].x = 25;
        mode.m_guildPosList[77].y = 35;
        mode.m_guildPosList[77].color = 0x0080ff80u;
        capture("guild-over-party",51,126,true,2,0x0080ff80u,0x0080ff80u);
        g_session.SetPlayerPosDir(25,35,0);
        capture("player-over-guild-and-party",51,126,true,2,0x00f5e07eu,0x00ffffffu);
        mode.m_guildPosList.clear();
        g_session.SetPlayerPosDir(50,50,0);
        Dispatch(mode,PositionRecord(0,50));
        capture("left-map-edge",10,102,true);
        for (int y=98; y<=106; ++y) for (int x=5; x<10; ++x) {
            Require(!IsRgb(lastNative,x,y,partyRgb),"native party marker leaked outside the map rectangle");
            Require(!IsRgb(lastQt,x,y,partyRgb),"Qt party marker leaked outside the map rectangle");
        }
        constexpr u32 compassRgb = 0x00ef32a1u;
        constexpr int compassX = 133;
        constexpr int compassY = 77;
        Dispatch(mode,CompassRecord(1,75,65,9,compassRgb));
        Require(mode.m_compassPosList.size() == 1 && mode.m_compassPosList.at(9).type == 1,
            "production 0144 type-1 compass marker was not retained");
        capture("compass-visible",compassX,compassY,true,2,compassRgb,compassRgb);
        RequireCompassCross(lastNative,compassX,compassY,compassRgb,"native 0144 marker is not the expected 8x2 plus 2x8 cross");
        RequireCompassCross(lastQt,compassX,compassY,compassRgb,"Qt 0144 marker is not the expected 8x2 plus 2x8 cross");
        mode.m_compassPosList.at(9).startTime = timeGetTime() - 1800u;
        capture("compass-blink-hidden",compassX,compassY,false,1);
        Dispatch(mode,CompassRecord(1,75,65,9,compassRgb));
        capture("compass-blink-visible",compassX,compassY,true,2,compassRgb,compassRgb);
        Dispatch(mode,CompassRecord(2,0,0,9,0));
        Require(mode.m_compassPosList.empty(),"production 0144 type-2 packet did not remove the compass marker");
        capture("compass-removed",compassX,compassY,false,1);
        Dispatch(mode,PositionRecord(25,35));
        MapTransitionServices services;
        services.presentLoading = [](const char*,float) {};
        services.requestMode = [](int type,const char* worldName) { g_modeMgr.Switch(type,worldName); };
        mode.SetMapTransitionServices(&services);
        std::vector<u8> mapChange(22,0); mapChange[0]=0x91;
        std::memcpy(mapChange.data()+2,"geffen",6); mapChange[18]=10; mapChange[20]=20;
        Dispatch(mode,mapChange);
        Require(g_session.GetNumParty()==1,"map transition discarded party membership");
        capture("map-transition",51,126,false);
        for (bool reconnectSucceeds : {true,false}) {
            std::snprintf(g_session.m_curMap,sizeof(g_session.m_curMap),"pk14_fixture");
            Dispatch(mode,MemberRecord(false));
            Dispatch(mode,PositionRecord(25,35));
            services.reconnect = [=](u32 ip,u16 port) {
                Require(ip==0x0100000au && port==6121,"render reconnect fixture lost its endpoint");
                Require(!g_session.FindPartyMemberByAid(42)->partyPositionValid,
                    "render reconnect ran before marker invalidation");
                return reconnectSucceeds;
            };
            std::vector<u8> serverMove = mapChange; serverMove.resize(28);
            serverMove[0]=0x92; serverMove[22]=10; serverMove[25]=1; serverMove[26]=0xe9; serverMove[27]=0x17;
            Dispatch(mode,serverMove);
            Require(g_modeMgr.GetRequestedModeType()==(reconnectSucceeds?1:0),"render reconnect selected the wrong handoff");
            capture(reconnectSucceeds?"reconnect-success":"reconnect-failure",51,126,false);
        }
        std::snprintf(g_session.m_curMap,sizeof(g_session.m_curMap),"pk14_fixture");
        Dispatch(mode,MemberRecord(false));
        Dispatch(mode,PositionRecord(25,35));
        CLoginMode login;
        login.OnExit();
        capture("session-boundary",51,126,false);
        for (size_t first=0; first<caseNames.size(); first+=6) {
            const int rows = int((std::min)(size_t(6),caseNames.size()-first));
            QImage contact(2*map.m_w,rows*(map.m_h+20),QImage::Format_RGB32); contact.fill(Qt::black);
            QPainter painter(&contact); painter.setPen(Qt::white);
            for (int row=0; row<rows; ++row) {
                const size_t index=first+size_t(row); const int top=row*(map.m_h+20);
                painter.drawText(4,top+15,caseNames[index]);
                painter.drawImage(0,top+20,nativeContacts[index]); painter.drawImage(map.m_w,top+20,qtContacts[index]);
            }
            painter.end();
            SaveImage(contact,output+QString("/contact-%1.png").arg(first/6));
        }
        Require(qmlErrors==0,"production QML emitted errors/warnings during minimap render");
        QFile result(output+"/render-results.json");
        Require(result.open(QIODevice::WriteOnly),"render result JSON open failed");
        result.write(QJsonDocument(QJsonObject{{"cases",cases},{"qml_errors",qmlErrors},{"outcome","PASS"}}).toJson());
        view.hide();
        g_windowMgr.m_roMapWnd = nullptr;
        SkillPacketReplayAccess::SetCurrent(nullptr);
        mode.m_world = nullptr;
        std::cout << "PASS native and production-QML minimap rendering\n";
        return 0;
    } catch (const std::exception& error) {
        g_windowMgr.m_roMapWnd = nullptr;
        SkillPacketReplayAccess::SetCurrent(nullptr);
        std::cerr << "FAIL minimap render: " << error.what() << '\n';
        return 1;
    }
}
