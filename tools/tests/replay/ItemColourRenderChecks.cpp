#include "gamemode/GameMode.h"
#include "gamemode/GameModePacket.h"
#include "gamemode/Mode.h"
#include "gamemode/Commerce.h"
#include "gamemode/View.h"
#include "qtui/QtUiStateAdapter.h"
#include "qtui/QtUiState.h"
#include "qtui/QtCommerce.h"
#include "render3d/RenderBackend.h"
#include "render/DC.h"
#include "session/Session.h"
#include "ui/UIItemWnd.h"
#include "ui/UIStorageWnd.h"
#include "ui/UIShopCommon.h"
#include "ui/UIWindowMgr.h"
#include "world/World.h"
#include <QGuiApplication>
#include <QQuickView>
#include <QQuickItem>
#include <QQuickImageProvider>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlError>
#include <QFontDatabase>
#include <QMouseEvent>
#include <QEnterEvent>
#include <QThread>
#include <QPainter>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <QDir>
#include <algorithm>
#include <iostream>
#include <stdexcept>

struct SkillPacketReplayAccess {
    static void SetCurrent(CGameMode* mode) {
        g_modeMgr.m_curMode=mode; g_modeMgr.m_curModeType=mode?1:0;
    }
};
namespace {
constexpr int Width=1300, Height=640;
void Require(bool ok,const char* text) {if(!ok)throw std::runtime_error(text);}
class FixtureImages final : public QQuickImageProvider {
public:
    FixtureImages():QQuickImageProvider(Image){}
    QImage requestImage(const QString&,QSize* size,const QSize&) override {
        QImage image(1,1,QImage::Format_ARGB32); image.fill(Qt::transparent);
        if(size)*size=image.size(); return image;
    }
};
void Frames(QGuiApplication& app,QQuickView& view) {
    view.requestUpdate(); for(int i=0;i<6;++i){app.processEvents();QThread::msleep(20);}
}
QQuickItem* Find(QQuickItem* root,const QString& name) {
    if(root->objectName()==name)return root;
    for(auto* child:root->childItems())if(auto* found=Find(child,name))return found;
    return nullptr;
}
int VisibleBagTiles(QQuickItem* root) {
    int count=root->objectName().startsWith("commerceTile_inventory_") && root->isVisible()
        && root->property("item").toMap().value("id").toUInt()!=0 ? 1 : 0;
    for(auto* child:root->childItems())count+=VisibleBagTiles(child);
    return count;
}
void Mouse(QGuiApplication& app,QQuickView& view,QPointF p) {
    QEnterEvent enter(p,p,view.mapToGlobal(p.toPoint()));
    QCoreApplication::sendEvent(&view,&enter);
    QMouseEvent move(QEvent::MouseMove,p,view.mapToGlobal(p.toPoint()),Qt::NoButton,Qt::NoButton,Qt::NoModifier);
    QCoreApplication::sendEvent(&view,&move); Frames(app,view);
}
int Pixels(const QImage& image,const QRect& bounds,QRgb rgb,const QImage* before=nullptr) {
    int count=0;
    for(int y=bounds.top();y<=bounds.bottom();++y)for(int x=bounds.left();x<=bounds.right();++x){
        if((image.pixel(x,y)&0xffffffu)!=(rgb&0xffffffu))continue;
        if(before && (before->pixel(x,y)&0xffffffu)==(rgb&0xffffffu))continue;
        ++count;
    }
    return count;
}
void Dispatch(CGameMode& mode,const std::vector<u8>& bytes) {
    PacketView packet{};int consumed=0;
    Require(TryReadPacket(bytes.data(),int(bytes.size()),packet,consumed),"item colour fixture did not frame");
    Require(consumed==int(bytes.size()),"item colour fixture left unframed bytes");
    Require(GetProductionGameModePacketRouter().Dispatch(mode,packet),"item colour fixture missed real router");
}
std::vector<u8> Equipment(u8 lowOpcode,bool identified,u8 bind) {
    // Independent selected EQUIPITEM_INFO[26]: index291, Knife1201, type4,
    // location2/wear0 (visible in the native bag), expiry0, raw bind word at
    // row+24. No client serializer.
    std::vector<u8> bytes={lowOpcode,2,30,0, 0x23,1,0xb1,4,4,1,2,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    bytes[9]=identified?1:0;bytes[28]=bind;return bytes;
}
template<class Window> QPoint Hover(Window& window) {
    typename Window::DisplayData data;
    Require(window.GetDisplayDataForQt(&data),"real native item display failed");
    for(const auto& slot:data.displaySlots)if(slot.occupied){
        QPoint p(slot.x+slot.width/2,slot.y+slot.height/2);
        window.OnMouseMove(p.x(),p.y());return p;
    }
    std::cerr<<"native hover missing: title="<<data.title<<" tab="<<data.currentTab<<" count="<<data.currentItemCount
        <<" slots="<<data.displaySlots.size()<<" size="<<window.m_w<<','<<window.m_h<<'\n';
    throw std::runtime_error("real native equipment tab has no occupied slot");
}
template<class Window> QImage Native(Window& window,QRgb expected) {
    ArgbDibSurface surface;Require(surface.EnsureSize(Width,Height),"native item DIB failed");
    std::fill_n(surface.GetPixels(),Width*Height,0xff181b20u);
    window.DrawNativeToHdc(surface.GetDC());
    QImage before=QImage(reinterpret_cast<uchar*>(surface.GetPixels()),Width,Height,Width*4,QImage::Format_RGB32).copy();
    window.DrawNativeHoverOverlay(surface.GetDC(),RECT{0,0,Width,Height});
    QImage image=QImage(reinterpret_cast<uchar*>(surface.GetPixels()),Width,Height,Width*4,QImage::Format_RGB32).copy();
    Require(Pixels(image,image.rect(),expected,&before)>8,"native hover did not paint source-defined foreground");
    return image;
}
}

int main(int argc,char** argv) {
    qputenv("QT_QUICK_BACKEND","software");qputenv("QT_QPA_PLATFORM","offscreen");
    if(qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM_PLUGIN_PATH"))qputenv("QT_QPA_PLATFORM_PLUGIN_PATH",RO_PACKET_QT_PLATFORM_PLUGINS);
    QGuiApplication app(argc,argv);
    try {
        const QString output=QDir::cleanPath(argc>1?QString::fromLocal8Bit(argv[1]):QDir::currentPath()+"/item-renders");
        Require(QDir().mkpath(output),"item render directory failed");
        Require(QDir().mkpath(output+"/data"),"item metadata fixture directory failed");
        for(const QString& table:{QString("idnum2itemdisplaynametable.txt"),QString("num2itemdisplaynametable.txt"),QString("itemslotcounttable.txt")}) {
            QFile source(QString::fromUtf8(RO_PACKET_SOURCE_DIR)+"/tools/tests/fixtures/packet-item-data/"+table);
            Require(source.open(QIODevice::ReadOnly),"source-derived item fixture table missing");
            const QByteArray bytes=source.readAll();QFile target(output+"/data/"+table);
            Require(target.open(QIODevice::WriteOnly) && target.write(bytes)==bytes.size(),"item fixture table copy failed");
        }
        Require(QDir::setCurrent(output),"item fixture working directory failed");
        Require(QFontDatabase::addApplicationFont(QString::fromUtf8(RO_PACKET_SOURCE_DIR)+"/third_party/fonts/NotoSans.ttf")>=0,"bundled item font failed");
        QGuiApplication::setFont(QFont("Noto Sans"));
        CWorld world;CView camera;camera.SetWorld(&world);
        CGameMode mode;mode.m_world=&world;mode.m_view=&camera;SkillPacketReplayAccess::SetCurrent(&mode);
        RegisterDefaultGameModePacketHandlers(GetProductionGameModePacketRouter());
        g_session.ClearInventoryItems();g_session.CloseStorage();ro::commerce::Reset();
        UIItemWnd inventory;inventory.SetShow(1);inventory.Resize(350,420);inventory.Move(20,100);
        UIStorageWnd storage;storage.SetShow(1);storage.Resize(350,420);storage.Move(900,100);
        g_windowMgr.m_itemWnd=&inventory;g_windowMgr.m_storageWnd=&storage;
        inventory.OnLBtnDown(inventory.m_x+5,inventory.m_y+58);inventory.OnLBtnUp(inventory.m_x+5,inventory.m_y+58);
        storage.OnLBtnDown(storage.m_x+5,storage.m_y+58);storage.OnLBtnUp(storage.m_x+5,storage.m_y+58);
        ro::commerce::state.inventoryTab=1;
        ro::commerce::state.cart.visible=true;ro::commerce::state.cart.x=450;ro::commerce::state.cart.y=100;
        QtUiStateAdapter adapter;auto* state=qobject_cast<QtUiState*>(adapter.stateObject());
        Require(state,"item adapter state missing");state->setLoadingVisible(false);state->setUiScale(1.0);
        int errors=0;
        QQuickView view;view.setColor(QColor("#181b20"));view.resize(Width,Height);view.setResizeMode(QQuickView::SizeRootObjectToView);
        view.rootContext()->setContextProperty("uiState",adapter.stateObject());
        view.engine()->addImageProvider("openmidgard",new FixtureImages);
        QObject::connect(view.engine(),&QQmlEngine::warnings,[&](const QList<QQmlError>& list){errors+=int(list.size());for(const auto& e:list)std::cerr<<e.toString().toStdString()<<'\n';});
        view.setSource(QUrl::fromLocalFile(QString::fromUtf8(RO_PACKET_SOURCE_DIR)+"/src/qtui/qml/GameOverlay.qml"));
        Require(view.status()!=QQuickView::Error,"actual item QML failed to load");view.show();Frames(app,view);
        QJsonArray results;std::vector<QImage> contacts;std::vector<QString> contactNames;
        for(bool identified:{true,false})for(u8 bind:{u8(0),u8(1),u8(2)}) {
            const QRgb expected=identified?(bind==2?0xffffff00u:0xffffffffu):0xffaaaaaau;
            const QString name=QString("%1-bind%2").arg(identified?"identified":"unidentified").arg(bind);
            Mouse(app,view,QPointF(Width-1,Height-1));
            Dispatch(mode,Equipment(0xd0,identified,bind));Dispatch(mode,Equipment(0xd2,identified,bind));
            g_session.OpenStorage(1,600);Dispatch(mode,Equipment(0xd1,identified,bind));
            Require(g_session.GetInventoryItemByIndex(291) && g_session.GetInventoryItemByIndex(291)->m_isYours==bind,"inventory raw bind was not retained");
            Require(ro::commerce::state.cartItems.at(291).m_isYours==bind,"cart raw bind was not retained");
            PopulateCommerceUi(state,mode,false);
            const auto invRows=state->commerceData().value("inventoryItems").toList();
            const auto cartRows=state->commerceData().value("cartItems").toList();
            Require(invRows.size()==1 && cartRows.size()==1,"production item rows not populated");
            const QString expectedName=identified?"Knife [3]":"Dagger [3]";
            Require(invRows[0].toMap().value("name").toString()==expectedName
                && cartRows[0].toMap().value("name").toString()==expectedName,"item name metadata/identification or slot suffix changed");
            const QString colour=QColor::fromRgba(expected).name(QColor::HexArgb);
            Require(invRows[0].toMap().value("nameColor").toString()==colour && cartRows[0].toMap().value("nameColor").toString()==colour,
                "production inventory/cart row lost the source-defined name colour");
            const QPoint invPoint=Hover(inventory);
            auto native=Native(inventory,expected);Require(native.save(output+"/"+name+"-native-inventory.png"),"save native item failed");
            inventory.OnMouseMove(-100,-100);
            const QPoint storagePoint=Hover(storage);
            native=Native(storage,expected);Require(native.save(output+"/"+name+"-native-storage.png"),"save native storage failed");
            Require(adapter.syncGameplay(mode,RenderBackendType::LegacyDirect3D7,RenderBackendType::LegacyDirect3D7,storagePoint.x(),storagePoint.y()),"real gameplay adapter failed");
            bool anchorFound=false;
            for(const auto& value:state->anchors()) {
                const auto anchor=value.toMap();if(anchor.value("kind").toString()!="uiTooltip")continue;
                Require(QColor(anchor.value("foreground").toString())==QColor::fromRgba(expected),"storage tooltip anchor lost source colour");anchorFound=true;
            }
            Require(anchorFound,"real storage hover anchor missing");
            Frames(app,view);QImage qtStorage=view.grabWindow().convertToFormat(QImage::Format_RGB32);
            auto* storageLabel=Find(view.rootObject(),"hoverPlayerName");
            Require(storageLabel && storageLabel->isVisible()
                && storageLabel->property("color").value<QColor>()==QColor::fromRgba(expected),"actual QML storage hover label lost colour");
            const QRect storageBounds=storageLabel->mapRectToScene(storageLabel->boundingRect()).toAlignedRect().intersected(QRect(0,0,Width,Height));
            Require(!storageBounds.isEmpty() && Pixels(qtStorage,storageBounds,expected)>8,"QML storage hover colour pixels absent");
            Require(qtStorage.save(output+"/"+name+"-qt-storage.png"),"save QML storage failed");
            storage.OnMouseMove(-100,-100);state->setAnchors({});
            for(const QString& kind:{QString("inventory"),QString("cart")}) {
                PopulateCommerceUi(state,mode,false);Frames(app,view);
                auto* tile=Find(view.rootObject(),"commerceTile_"+kind+"_291");Require(tile,"actual item tile missing");
                Mouse(app,view,tile->mapToScene(QPointF(tile->width()/2,tile->height()/2)));
                auto* tip=Find(view.rootObject(),"commerceItemTooltip");Require(tip && tip->isVisible(),"real QML item hover tooltip missing");
                auto* label=Find(tip,"commerceItemTooltipText");Require(label,"QML hover name text missing");
                Require(label->property("color").value<QColor>()==QColor::fromRgba(expected),"actual QML hover text has wrong colour");
                const QRect bounds=tip->mapRectToScene(tip->boundingRect()).toAlignedRect().intersected(QRect(0,0,Width,Height));
                QImage qt=view.grabWindow().convertToFormat(QImage::Format_RGB32);
                Require(!bounds.isEmpty() && Pixels(qt,bounds,expected)>8,"QML item tooltip pixels lost expected foreground");
                Require(qt.save(output+"/"+name+"-qt-"+kind+".png"),"save QML item failed");
                contacts.push_back(qt.copy(bounds));contactNames.push_back(name+" / "+kind);Mouse(app,view,QPointF(Width-1,Height-1));
                Require(!tip->isVisible(),"QML item hover persisted after pointer exit");
            }
            results.push_back(QJsonObject{{"case",name},{"identified",identified},{"bind",int(bind)},{"expected_argb",colour},{"outcome","PASS"}});
            std::cout<<QJsonDocument(results.last().toObject()).toJson(QJsonDocument::Compact).toStdString()<<'\n';
        }
        // Primary Ref moves equipped items out of the bag. This rewrite keeps
        // authoritative inventory rows centrally, so both projections filter.
        auto checkBag=[&](bool visible,const char* phase){
            PopulateCommerceUi(state,mode,false);
            UIItemWnd::DisplayData nativeData;Require(inventory.GetDisplayDataForQt(&nativeData),"native bag projection failed");
            const int nativeRows=int(std::count_if(nativeData.displaySlots.begin(),nativeData.displaySlots.end(),[](const auto& row){return row.occupied;}));
            const auto rows=state->commerceData().value("inventoryItems").toList();
            Require(nativeRows==int(visible) && nativeData.currentItemCount==int(visible),"native bag rows/count lost the expected wear predicate");
            if(rows.size()!=int(visible) || state->commerceData().value("inventoryCount").toInt()!=int(visible))
                throw std::runtime_error(std::string("Qt bag rows/count diverge from native/source at ")+phase);
            Frames(app,view);
            Require(VisibleBagTiles(view.rootObject())==int(visible),"actual QML bag delegates diverged from source rows");
        };
        auto worn=Equipment(0xd0,true,2);worn[12]=2;
        Dispatch(mode,worn);checkBag(false,"worn snapshot");
        Require(g_session.GetInventoryItemByIndex(291)->m_wearLocation==2,"bag filtering erased authoritative worn state");
        Require(view.grabWindow().save(output+"/bag-worn-snapshot.png"),"save worn bag frame failed");
        mode.m_waitingTakeoffEquipAck=291;
        Dispatch(mode,{0xac,0,0x23,1,2,0,0});checkBag(false,"failed unequip");
        Require(mode.m_waitingTakeoffEquipAck==0,"unequip failure did not release pending request");
        Dispatch(mode,{0xac,0,0x23,1,2,0,1});checkBag(true,"successful unequip");
        Require(g_session.GetInventoryItemByIndex(291)->m_wearLocation==0,"successful unequip lost inventory row/wear update");
        Require(view.grabWindow().save(output+"/bag-unequipped.png"),"save unequipped bag frame failed");
        mode.m_waitingWearEquipAck=291;
        Dispatch(mode,{0xaa,0,0x23,1,2,0,0});checkBag(true,"failed equip");
        Require(mode.m_waitingWearEquipAck==0,"equip failure did not release pending request");
        Dispatch(mode,{0xaa,0,0x23,1,2,0,1});checkBag(false,"successful equip");
        Require(g_session.GetInventoryItemByIndex(291)->m_wearLocation==2,"successful equip erased authoritative row");
        Require(view.grabWindow().save(output+"/bag-equipped.png"),"save equipped bag frame failed");
        // Empty equipment snapshot clears worn item while normal ammunition
        // remains present and visible, even after its real equip notification.
        Dispatch(mode,{0xe8,2,26,0,0x24,1,0xd6,6,10,1,9,0,0,0x80,0,0,0,0,0,0,0,0,0,0,0,0});
        Dispatch(mode,{0xd0,2,4,0});ro::commerce::state.inventoryTab=2;
        inventory.OnLBtnDown(inventory.m_x+5,inventory.m_y+88);inventory.OnLBtnUp(inventory.m_x+5,inventory.m_y+88);
        checkBag(true,"normal ammunition snapshot");
        Require(g_session.GetInventoryItemByIndex(292)->m_wearLocation==0,"normal allowed ammunition slot set actual wear");
        Dispatch(mode,{0x3c,1,0x24,1});checkBag(true,"equipped ammunition exception");
        Require(g_session.GetInventoryItemByIndex(292)->m_wearLocation==0x8000,"ammo equip did not retain wear mask");
        Require(view.grabWindow().save(output+"/bag-equipped-ammunition.png"),"save ammunition bag frame failed");
        // Return the inventory fixture for the independent cart-hover lifetime check.
        Dispatch(mode,Equipment(0xd0,false,2));
        auto* cartTile=Find(view.rootObject(),"commerceTile_cart_291");Require(cartTile,"cart lifetime fixture tile missing");
        Mouse(app,view,cartTile->mapToScene(QPointF(cartTile->width()/2,cartTile->height()/2)));
        auto* tip=Find(view.rootObject(),"commerceItemTooltip");Require(tip && tip->isVisible(),"cart lifetime hover did not open");
        Dispatch(mode,{0xd2,2,4,0});PopulateCommerceUi(state,mode,false);Frames(app,view);
        Require(!tip->isVisible() && g_session.GetInventoryItemByIndex(291),"cart row removal retained stale hover or changed inventory");
        Require(errors==0,"item UI emitted QML warnings/errors");
        QImage contact(760,int(contacts.size())*52,QImage::Format_RGB32);contact.fill(QColor("#181b20"));QPainter painter(&contact);painter.setPen(Qt::white);
        for(size_t i=0;i<contacts.size();++i){painter.drawImage(0,int(i)*52,contacts[i]);painter.drawText(280,int(i)*52+23,contactNames[i]);}painter.end();
        Require(contact.save(output+"/tooltip-contact.png"),"save item contact sheet failed");
        QFile file(output+"/render-results.json");Require(file.open(QIODevice::WriteOnly),"item results open failed");
        file.write(QJsonDocument(QJsonObject{{"cases",results},{"qml_errors",errors},{"hover_exit_and_row_removal",true},{"worn_bag_success_failure_ammunition",true},{"outcome","PASS"}}).toJson());
        view.hide();g_windowMgr.m_itemWnd=nullptr;g_windowMgr.m_storageWnd=nullptr;
        SkillPacketReplayAccess::SetCurrent(nullptr);mode.m_world=nullptr;mode.m_view=nullptr;
        std::cout<<"PASS six item colour cases across native inventory/storage and production QML inventory/cart/storage\n";return 0;
    } catch(const std::exception& e) {
        g_windowMgr.m_itemWnd=nullptr;g_windowMgr.m_storageWnd=nullptr;SkillPacketReplayAccess::SetCurrent(nullptr);
        std::cerr<<"FAIL item colour render: "<<e.what()<<'\n';return 1;
    }
}
