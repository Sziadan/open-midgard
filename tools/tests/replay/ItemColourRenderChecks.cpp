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
#include "ui/UIItemInfoWnd.h"
#include "ui/UINewChatWnd.h"
#include "ui/UiScale.h"
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
#include <set>
#include <type_traits>
#include <QDir>
#include <algorithm>
#include <array>
#include <cstdlib>
#include <ctime>
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
void Click(QGuiApplication& app,QQuickView& view,QQuickItem* item) {
    Require(item && item->isVisible(),"cart button is missing or hidden");
    const auto p=item->mapToScene(QPointF(item->width()/2,item->height()/2));
    Mouse(app,view,p);
    QMouseEvent press(QEvent::MouseButtonPress,p,view.mapToGlobal(p.toPoint()),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
    QMouseEvent release(QEvent::MouseButtonRelease,p,view.mapToGlobal(p.toPoint()),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
    QCoreApplication::sendEvent(&view,&press);QCoreApplication::sendEvent(&view,&release);Frames(app,view);
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
    // Independent selected EQUIPITEM_INFO[26]: index291, Knife1201, type5,
    // location2/wear0 (visible in the native bag), expiry0, raw bind word at
    // row+24. No client serializer.
    std::vector<u8> bytes={lowOpcode,2,30,0, 0x23,1,0xb1,4,5,1,2,0,0,0,0,0,
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

#include "ItemCardNameRenderChecks.inc"
#include "SpecialItemNameRenderChecks.inc"
#include "RentalItemRenderChecks.inc"
#include "CommerceItemDetailRenderChecks.inc"
#include "ItemCategoryRenderChecks.inc"
#include "ItemDetailToggleRenderChecks.inc"

int main(int argc,char** argv) {
    qputenv("QT_QUICK_BACKEND","software");qputenv("QT_QPA_PLATFORM","offscreen");
    if(qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM_PLUGIN_PATH"))qputenv("QT_QPA_PLATFORM_PLUGIN_PATH",RO_PACKET_QT_PLATFORM_PLUGINS);
    QGuiApplication app(argc,argv);
    try {
        const QString output=QDir::cleanPath(argc>1?QString::fromLocal8Bit(argv[1]):QDir::currentPath()+"/item-renders");
        Require(QDir().mkpath(output),"item render directory failed");
        Require(QDir().mkpath(output+"/data"),"item metadata fixture directory failed");
        const bool specialOnly=argc>2 && QString::fromLocal8Bit(argv[2])=="special-names";
        const bool rentalOnly=argc>2 && QString::fromLocal8Bit(argv[2])=="rental-items";
        const bool commerceDetailsOnly=argc>2 && QString::fromLocal8Bit(argv[2])=="commerce-details";
        const bool itemCategoriesOnly=argc>2 && QString::fromLocal8Bit(argv[2])=="item-categories";
        const bool itemDetailToggleOnly=argc>2 && QString::fromLocal8Bit(argv[2])=="item-detail-toggle";
        if(specialOnly || rentalOnly) {
            QFile source(QString::fromUtf8(RO_PACKET_SOURCE_DIR)+"/tools/tests/fixtures/packet-item-data/"+(rentalOnly?"rental-msgstringtable.txt":"special-name-msgstringtable.txt"));
            Require(source.open(QIODevice::ReadOnly),"special source message fixture missing");
            const auto bytes=source.readAll();QFile target(output+"/data/msgstringtable.txt");
            Require(target.open(QIODevice::WriteOnly) && target.write(bytes)==bytes.size(),"copy special messages failed");
        }
        if(rentalOnly) {
            QFile source(QString::fromUtf8(RO_PACKET_SOURCE_DIR)+"/tools/tests/fixtures/packet-item-data/rental-idnum2itemdesctable.txt");
            Require(source.open(QIODevice::ReadOnly),"source rental description missing");const auto bytes=source.readAll();
            QFile target(output+"/data/idnum2itemdesctable.txt");Require(target.open(QIODevice::WriteOnly) && target.write(bytes)==bytes.size(),"copy rental description failed");
        }
        for(const QString& table:{QString("idnum2itemdisplaynametable.txt"),QString("num2itemdisplaynametable.txt"),QString("itemslotcounttable.txt"),QString("cardprefixnametable.txt"),QString("cardpostfixnametable.txt"),QString("carditemnametable.txt")}) {
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
        if(itemDetailToggleOnly) {
            CheckItemDetailToggleUi(app,view,state,mode,inventory,storage,adapter,output);
            Require(errors==0,"item detail toggle UI emitted QML errors");
            QFile file(output+"/item-detail-toggle-render-results.json");
            Require(file.open(QIODevice::WriteOnly),"open item detail toggle render results failed");
            file.write(QJsonDocument(QJsonObject{{"qml_errors",errors},{"outcome","PASS"}}).toJson());
            view.hide();g_windowMgr.RemoveAllWindows();g_windowMgr.m_itemWnd=nullptr;g_windowMgr.m_storageWnd=nullptr;
            SkillPacketReplayAccess::SetCurrent(nullptr);mode.m_world=nullptr;mode.m_view=nullptr;
            std::cout<<"PASS primary item detail selection/update rendering\n";return 0;
        }
        if(itemCategoriesOnly) {
            CheckItemCategoryUi(app,view,state,mode,inventory,storage,adapter,output);
            Require(errors==0,"item category UI emitted QML errors");
            QFile file(output+"/item-category-render-results.json");
            Require(file.open(QIODevice::WriteOnly),"open item category render results failed");
            file.write(QJsonDocument(QJsonObject{{"qml_errors",errors},{"outcome","PASS"}}).toJson());
            view.hide();g_windowMgr.RemoveAllWindows();g_windowMgr.m_itemWnd=nullptr;g_windowMgr.m_storageWnd=nullptr;
            SkillPacketReplayAccess::SetCurrent(nullptr);mode.m_world=nullptr;mode.m_view=nullptr;
            std::cout<<"PASS item category source/native/Qt rendering\n";return 0;
        }
        if(commerceDetailsOnly) {
            CheckCommerceItemDetails(app,view,state,mode,adapter,output);
            Require(errors==0,"commerce item details emitted QML errors");
            QFile file(output+"/commerce-detail-render-results.json");
            Require(file.open(QIODevice::WriteOnly),"open commerce detail render results failed");
            file.write(QJsonDocument(QJsonObject{{"qml_errors",errors},{"outcome","PASS"}}).toJson());
            view.hide();g_windowMgr.RemoveAllWindows();g_windowMgr.m_itemWnd=nullptr;g_windowMgr.m_storageWnd=nullptr;
            SkillPacketReplayAccess::SetCurrent(nullptr);mode.m_world=nullptr;mode.m_view=nullptr;
            std::cout<<"PASS commerce item detail context rendering\n";return 0;
        }
        if(rentalOnly) {
            CheckRentalItemUi(app,view,state,mode,inventory,storage,adapter,output);
            Require(errors==0,"rental item UI emitted QML errors");QFile file(output+"/rental-render-results.json");
            Require(file.open(QIODevice::WriteOnly),"open rental render result failed");file.write(QJsonDocument(QJsonObject{{"qml_errors",errors},{"outcome","PASS"}}).toJson());
            view.hide();g_windowMgr.RemoveAllWindows();g_windowMgr.m_itemWnd=nullptr;g_windowMgr.m_storageWnd=nullptr;
            SkillPacketReplayAccess::SetCurrent(nullptr);mode.m_world=nullptr;mode.m_view=nullptr;
            std::cout<<"PASS rental expiry detail/lifetime rendering\n";return 0;
        }
        if(specialOnly) {
            CheckSpecialItemNames(app,view,state,mode,inventory,storage,adapter,output);
            Require(errors==0,"special item UI emitted QML errors");
            QFile file(output+"/special-render-results.json");Require(file.open(QIODevice::WriteOnly),"open special render results failed");
            file.write(QJsonDocument(QJsonObject{{"qml_errors",errors},{"outcome","PASS"}}).toJson());
            view.hide();g_windowMgr.m_itemWnd=nullptr;g_windowMgr.m_storageWnd=nullptr;
            SkillPacketReplayAccess::SetCurrent(nullptr);mode.m_world=nullptr;mode.m_view=nullptr;
            std::cout<<"PASS special item names and delayed producer replies\n";return 0;
        }
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
                Require(label->property("text").toString()==(identified?"Knife: 1 ea":"Dagger: 1 ea"),
                    "actual item hover incorrectly appended a list-only socket count");
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
        // Status owns count/weight independently of rows; 012B closes contents,
        // and the actual QML Cart button must still reopen an empty cart.
        Dispatch(mode,{0x21,1,1,0,100,0,0x34,0x30,0,0,0xa0,0x86,1,0});
        Dispatch(mode,Equipment(0xd2,true,2));ro::commerce::state.cart.visible=true;
        PopulateCommerceUi(state,mode,false);Frames(app,view);
        auto checkCartFooter=[&](int count,int weight) {
            const auto data=state->commerceData();
            Require(data.value("cartCount").toInt()==count && data.value("cartWeight").toInt()==weight
                && data.value("cartMaxWeight").toInt()==10000,"cart status projection changed counters or scaled weight twice");
            auto* footer=Find(view.rootObject(),"commerceFooter_cart");
            Require(footer && footer->isVisible(),"actual cart footer missing");
            Require(footer->property("text").toString()==QString("%1 / 100 slots · Weight %2 / 10000").arg(count).arg(weight),
                "rendered cart footer differs from authoritative status");
        };
        checkCartFooter(1,1234);
        Require(view.grabWindow().save(output+"/cart-before-clear.png"),"save cart before clear failed");
        Dispatch(mode,{0x2b,1});PopulateCommerceUi(state,mode,false);Frames(app,view);
        Require(!ro::commerce::state.cart.visible && state->commerceData().value("hasCart").toBool()
            && state->commerceData().value("cartItems").toList().isEmpty(),"cart clear lost presence or retained rows");
        Click(app,view,Find(view.rootObject(),"commerceCartButton"));
        PopulateCommerceUi(state,mode,false);Frames(app,view);
        Require(ro::commerce::state.cart.visible && state->commerceData().value("cartItems").toList().isEmpty(),
            "actual QML button did not reopen the cleared empty cart");
        checkCartFooter(1,1234);
        Require(view.grabWindow().save(output+"/cart-reopened-empty.png"),"save reopened empty cart failed");
        Dispatch(mode,{0x21,1,0,0,100,0,0,0,0,0,0xa0,0x86,1,0});
        PopulateCommerceUi(state,mode,false);Frames(app,view);checkCartFooter(0,0);
        Require(state->commerceData().value("hasCart").toBool(),"empty status removed cart button entitlement");
        Require(view.grabWindow().save(output+"/cart-status-reconciled.png"),"save reconciled cart failed");
        // Ref GetItemName suppresses refine/card composition when unidentified.
        // Socket counts are appended explicitly by particular list consumers,
        // not by the inventory GetItemName + MSI_EA2 hover path.
        g_session.ClearInventoryItems();ro::commerce::state.cartItems.clear();
        ro::commerce::state.inventoryTab=1;
        inventory.OnLBtnDown(inventory.m_x+5,inventory.m_y+58);inventory.OnLBtnUp(inventory.m_x+5,inventory.m_y+58);
        for(bool identified:{true,false}) {
            auto refined=Equipment(0xd0,identified,0);refined[15]=7;Dispatch(mode,refined);
            refined[0]=0xd2;Dispatch(mode,refined);PopulateCommerceUi(state,mode,false);Frames(app,view);
            const QString expectedList=identified?"+7 Knife [3]":"Dagger [3]";
            const QString expectedHover=identified?"+7 Knife: 1 ea":"Dagger: 1 ea";
            const auto invRows=state->commerceData().value("inventoryItems").toList();
            const auto cartRows=state->commerceData().value("cartItems").toList();
            Require(invRows.size()==1 && cartRows.size()==1
                && invRows[0].toMap().value("name").toString()==expectedList
                && cartRows[0].toMap().value("name").toString()==expectedList,
                "commerce list name leaked refinement on an unidentified item");
            Hover(inventory);shopui::ItemHoverInfo nativeHover;
            Require(inventory.GetHoveredItemForQt(&nativeHover) && QString::fromStdString(nativeHover.text)==expectedHover,
                "native inventory hover name differs from primary GetItemName");
            inventory.OnMouseMove(-100,-100);
            for(const QString& kind:{QString("inventory"),QString("cart")}) {
                auto* tile=Find(view.rootObject(),"commerceTile_"+kind+"_291");Require(tile,"refined item tile missing");
                Mouse(app,view,tile->mapToScene(QPointF(tile->width()/2,tile->height()/2)));
                auto* label=Find(view.rootObject(),"commerceItemTooltipText");
                Require(label && label->isVisible() && label->property("text").toString()==expectedHover,
                    "actual QML refined/unidentified hover differs from primary GetItemName");
                Require(view.grabWindow().save(output+QString("/name-%1-%2.png").arg(identified?"identified":"unidentified",kind)),
                    "save source item name frame failed");
                Mouse(app,view,QPointF(Width-1,Height-1));
            }
        }
        g_session.ClearInventoryItems();ro::commerce::state.cartItems.clear();
        ro::commerce::state.inventoryTab=2;
        inventory.OnLBtnDown(inventory.m_x+5,inventory.m_y+88);inventory.OnLBtnUp(inventory.m_x+5,inventory.m_y+88);
        for(const u8 amount:{u8(9),u8(1)}) {
            std::vector<u8> normal={0xe8,2,26,0,0x24,1,0xd6,6,10,1,amount,0,0,0x80,0,0,0,0,0,0,0,0,0,0,0,0};
            Dispatch(mode,normal);normal[0]=0xe9;Dispatch(mode,normal);
            PopulateCommerceUi(state,mode,false);Frames(app,view);
            UIItemWnd::DisplayData nativeData;Require(inventory.GetDisplayDataForQt(&nativeData),"quantity native projection failed");
            auto slot=std::find_if(nativeData.displaySlots.begin(),nativeData.displaySlots.end(),[](const auto& s){return s.occupied;});
            Require(slot!=nativeData.displaySlots.end() && slot->count==amount,"native quantity did not follow replacement snapshot");
            for(const QString& kind:{QString("inventory"),QString("cart")}) {
                auto* count=Find(view.rootObject(),"commerceStackLabel_"+kind+"_292");
                Require(count && count->property("text").toString()==QString::number(amount)
                    && count->isVisible()==(amount>1),"actual QML quantity badge text/visibility is stale");
            }
            Require(view.grabWindow().save(output+QString("/quantity-%1.png").arg(amount)),"save quantity frame failed");
        }
        auto* quantityTile=Find(view.rootObject(),"commerceTile_cart_292");Require(quantityTile,"quantity tile missing");
        Mouse(app,view,quantityTile->mapToScene(QPointF(quantityTile->width()/2,quantityTile->height()/2)));
        Dispatch(mode,{0xe8,2,4,0});Dispatch(mode,{0xe9,2,4,0});PopulateCommerceUi(state,mode,false);Frames(app,view);
        Require(!Find(view.rootObject(),"commerceTile_inventory_292") && !Find(view.rootObject(),"commerceTile_cart_292")
            && !Find(view.rootObject(),"commerceItemTooltip")->isVisible(),"quantity snapshot removal left stale delegates/hover");
        Require(view.grabWindow().save(output+"/quantity-empty.png"),"save empty quantity frame failed");
        CheckCardNameCases(app,view,state,mode,inventory,storage,adapter,output);
        Require(errors==0,"item UI emitted QML warnings/errors");
        QImage contact(760,int(contacts.size())*52,QImage::Format_RGB32);contact.fill(QColor("#181b20"));QPainter painter(&contact);painter.setPen(Qt::white);
        for(size_t i=0;i<contacts.size();++i){painter.drawImage(0,int(i)*52,contacts[i]);painter.drawText(280,int(i)*52+23,contactNames[i]);}painter.end();
        Require(contact.save(output+"/tooltip-contact.png"),"save item contact sheet failed");
        QFile file(output+"/render-results.json");Require(file.open(QIODevice::WriteOnly),"item results open failed");
        file.write(QJsonDocument(QJsonObject{{"cases",results},{"qml_errors",errors},{"hover_exit_and_row_removal",true},{"worn_bag_success_failure_ammunition",true},{"cart_status_clear_reopen",true},{"ordinary_item_name_refine_identification",true},{"quantity_snapshot_badges",true},{"card_name_cases",17},{"outcome","PASS"}}).toJson());
        view.hide();g_windowMgr.m_itemWnd=nullptr;g_windowMgr.m_storageWnd=nullptr;
        SkillPacketReplayAccess::SetCurrent(nullptr);mode.m_world=nullptr;mode.m_view=nullptr;
        std::cout<<"PASS six item colour cases across native inventory/storage and production QML inventory/cart/storage\n";return 0;
    } catch(const std::exception& e) {
        g_windowMgr.m_itemWnd=nullptr;g_windowMgr.m_storageWnd=nullptr;SkillPacketReplayAccess::SetCurrent(nullptr);
        std::cerr<<"FAIL item colour render: "<<e.what()<<'\n';return 1;
    }
}
