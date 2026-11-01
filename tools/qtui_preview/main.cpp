#include "../../src/qtui/QtUiState.h"
#include "../../src/ui/BasicInfoLayout.h"
#include "../../src/ui/StatusLayout.h"
#include "../../src/qtui/EquipmentLayoutForQt.h"
#include "../../src/qtui/SkillTreeGeometry.h"
#include <QGuiApplication>
#include <QQuickView>
#include <QQuickItem>
#include <QQmlContext>
#include <QQmlComponent>
#include "../../src/ui/UiMetrics.h"
#include "../../src/ui/SkillTreeModel.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QQuickImageProvider>
#include <QPainter>
#include <QTimer>
#include <QImage>
#include <QDir>
#include <QDebug>
#include <memory>
#include <cstdio>
#include <QFontDatabase>
static int qmlErrors = 0;
QQuickItem* FindVisualItem(QQuickItem* item,const QString& name) {
    if(item->objectName()==name)return item;
    for(auto* child:item->childItems())if(auto* found=FindVisualItem(child,name))return found;
    return nullptr;
}
int main(int argc, char** argv) {
    qputenv("QT_QUICK_BACKEND", "software");
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qInstallMessageHandler([](QtMsgType type, const QMessageLogContext&, const QString& message) {
        if (type != QtInfoMsg && (message.contains("TypeError") || message.contains("ReferenceError") || message.contains("Error:") || message.contains("Binding loop"))) ++qmlErrors;
        FILE* log = fopen("build-qt-preview/preview.log", "a");
        if(log) { const auto utf=message.toUtf8(); fwrite(utf.constData(),1,utf.size(),log); fputc('\n',log); fclose(log); }
    });
    qputenv("QT_QPA_FONTDIR", "C:/Windows/Fonts");
    QGuiApplication app(argc,argv);
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf");
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/seguisb.ttf");
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeuib.ttf");
    const double scale=argc>1?QString(argv[1]).toDouble():1;
    const bool gatedTree=argc>2&&QString(argv[2])=="tree-gated";
    const bool statusCase=argc>2&&QString(argv[2]).startsWith("status");
    const bool equipCase=argc>2&&QString(argv[2]).startsWith("equipment");
    const bool equipCompact=equipCase&&QString(argv[2]).startsWith("equipment-compact");
    const bool equipExtreme=equipCase&&QString(argv[2])=="equipment-extreme";
    const bool equipEmpty=equipCase&&QString(argv[2]).endsWith("empty");
    const bool statusDisabled=argc>2&&QString(argv[2])=="status-disabled";
    const bool extreme=argc>2&&(QString(argv[2])=="extreme"||QString(argv[2])=="status-extreme");
    const bool mini=argc>2&&(QString(argv[2])=="mini"||QString(argv[2])=="status-mini"||(equipCase&&QString(argv[2]).endsWith("mini")));
    const bool small=argc>3&&QString(argv[3])=="small";
    const bool large=argc>3&&QString(argv[3])=="large";
    const int basicX=small?0:24, basicY=small?0:24;
    const bool treeCase=argc>2&&QString(argv[2]).startsWith("tree");
    QtUiState state;
    state.setUiScale(scale);
    state.setBasicInfoVisible(true);
    state.setBasicInfoMini(mini);
    state.setBasicInfoGeometry(basicX,basicY,320,mini?58:196);
    QVariantMap basic{{"name",extreme?"A very long character name to check truncation":"Sziadan"},{"jobName",extreme?"Transcendent High Priest with a long title":"Assassin"},
        {"hp",extreme?1:8609},{"maxHp",extreme?99999999:8609},{"sp",extreme?0:426},{"maxSp",extreme?99999999:426},
        {"level",99},{"jobLevel",50},{"expPercent",extreme?0:72},{"jobExpPercent",extreme?100:38},
        {"weight",extreme?999999:1200},{"maxWeight",extreme?999999:5000},
        {"weightText",extreme?"Weight: 999999 / 999999":"Weight: 1200 / 5000"},
        {"moneyText",extreme?"Zeny: 2,147,483,647":"Zeny: 40,800"},
        {"miniHeaderText","Base Lv. 99 / Assassin / 72%"},{"miniStatusText","HP 8609 / 8609   SP 426 / 426"}};
    QVariantList buttons;
    const QStringList icons{"status","option","items","equip","skill","map","comm","friend"};
    for(int i=0;i<8;++i) { const auto rect=basicInfoLayout::menuButton(i);
    if(rect.x<0||rect.y<17||rect.x+rect.width>basicInfoLayout::width||rect.y+rect.height>basicInfoLayout::height)return 4;
    buttons.append(QVariantMap{{"x",basicX+rect.x},{"y",basicY+rect.y},{"width",rect.width},{"height",rect.height},{"visible",true},{"icon",icons[i]},{"label",""},{"hovered",i==0},{"pressed",false}}); }
    for(int i=0;i<8;++i)for(int j=i+1;j<8;++j) {
        const auto a=basicInfoLayout::menuButton(i), b=basicInfoLayout::menuButton(j);
        if(a.x < b.x+b.width && a.x+a.width > b.x && a.y < b.y+b.height && a.y+a.height > b.y)return 6;
    }
    basic["menuButtons"]=buttons;
    basic["systemButtons"]=QVariantList{QVariantMap{{"x",basicX+(mini?3:306)},{"y",basicY+3},{"width",11},{"height",11},{"visible",true},{"label",mini?"B":"_"},{"hovered",false},{"pressed",false}}};
    state.setBasicInfoData(basic);
    state.setOptionVisible(true);state.setOptionGeometry(368,24,308,374);
    QVariantMap option{{"title","Options"},{"collapsed",false},{"activeTab",3},{"keybindingBar",1},{"contentX",378},{"contentY",69},{"contentWidth",288},{"contentHeight",317}};
    QVariantList tabs,bindings;
    const QStringList tabNames{"Game","Graphics","Audio","Keybinding"};
    for(int i=0;i<4;++i)tabs.append(QVariantMap{{"x",378+72*i},{"y",47},{"width",70},{"height",23},{"label",tabNames[i]},{"active",i==3}});
    option["systemButtons"]=QVariantList{QVariantMap{{"x",662},{"y",27},{"width",11},{"height",11},{"label","X"},{"visible",true}}};
    option["tabs"]=tabs;
    for(int i=0;i<9;++i)bindings.append(QVariantMap{{"x",390+(i%3)*89},{"y",159+(i/3)*56},{"width",85},{"height",48},{"column",i},{"label",""},{"value",QString("Ctrl+%1").arg(i+1)},{"capturing",i==4}});
    option["keybindings"]=bindings;
    state.setOptionData(option);
    state.setShortCutVisible(true);state.setShortCutGeometry(24,432,295,89);state.setShortCutRows(3);
    QVariantList shortcuts;
    for(int i=0;i<27;++i)shortcuts.append(QVariantMap{{"row",i/9},{"column",i%9},{"occupied",false},{"isSkill",false},{"hover",i==4},{"count",0}});
    state.setShortCutSlots(shortcuts);
    const int statusX=statusCase?12:24, statusY=statusCase?16:232;
    state.setStatusVisible(true);state.setStatusMini(mini);
    state.setStatusGeometry(statusX,statusY,statusLayout::width,mini?statusLayout::miniHeight:statusLayout::height);
    QVariantList stats;
    const QStringList statNames{"STR","AGI","VIT","INT","DEX","LUK"};
    for(int i=0;i<6;++i)stats.append(QVariantMap{{"label",statNames[i]},
        {"base",extreme?9999:30+i*10},{"bonus",extreme?(i==2?-999:999):2+i*3},
        {"rowY",statusLayout::rowY(i)},{"canIncrease",!statusDisabled&&i<4},{"cost",i<4?2+i:0},
        {"increaseX",statusX+statusLayout::increaseX},{"increaseY",statusY+statusLayout::rowY(i)+1},
        {"increaseWidth",statusLayout::increaseSize},{"increaseHeight",statusLayout::increaseSize}});
    state.setStatusData(QVariantMap{{"title","Status"},{"stats",stats},
        {"attackText",extreme?"999999 + 999999":"224 + 30"},{"matkText",extreme?"999999~999999":"302~480"},
        {"hit",165},{"critical",8},{"itemDefText",extreme?"999999 + 999999":"13 + 2"},
        {"itemMdefText","0 + 94"},{"fleeText","102 + 9"},{"aspdText","147"},{"statusPoint",statusDisabled?0:172},
        {"miniPointsText",statusDisabled?"Points 0":"Points 172"},
        {"systemButtons",QVariantList{
            QVariantMap{{"x",statusX+statusLayout::width-28},{"y",statusY+3},{"width",12},{"height",11},{"visible",true},{"label",mini?"B":"_"}},
            QVariantMap{{"x",statusX+statusLayout::width-14},{"y",statusY+3},{"width",12},{"height",11},{"visible",true},{"label","X"}}}}});
    state.setAnchors(QVariantList{QVariantMap{{"x",24*scale},{"y",390*scale},{"text","Inventory - Alt+E"},{"kind","uiTooltip"},{"background","#f0363b42"},{"foreground","#ffffff"},{"showBubble",true}}});
    if(small) {
        state.setOptionVisible(false);state.setStatusVisible(false);state.setShortCutVisible(false);state.setAnchors({});
    }
    if(statusCase){state.setBasicInfoVisible(false);state.setOptionVisible(false);state.setShortCutVisible(false);state.setAnchors({});}
    equipmentLayout::Layout equipLayout;
    if(equipCase) {
        state.setBasicInfoVisible(false);state.setOptionVisible(false);state.setShortCutVisible(false);state.setStatusVisible(false);state.setAnchors({});
        const QStringList names{"+7 Apple of Archer", "Gangster Scarf", "+7 Immune Muffler [1]", "Clip of Muscle [1]",
            "Elven Ears", "+8 Pantie of Ares [1]", "+7 Cranial Buckler [1]", "+7 Green Boots [1]",
            "Glove of Dexterity [1]", "+10 Quadruple Bloody Composite Bow [4]", "Silver Arrow"};
        std::array<int,11> heights{};
        for(int i=0;i<11;++i) heights[i]=equipmentLayout::NameHeightForQt(equipExtreme
            ?QString("+10 Quadruple Very Long Prefix %1 of Extremely Long Card Suffix [4]").arg(names[i]):names[i]);
        equipLayout=equipmentLayout::Build(heights,equipCompact);
        const int x=12,y=12;
        state.setEquipVisible(true);state.setEquipMini(mini);
        state.setEquipGeometry(x,y,equipLayout.width,mini?equipmentLayout::miniHeight:equipLayout.height);
        QVariantList equipmentSlots;
        for(int i=0;i<11;++i) {
            const auto icon=equipLayout.icons[i],card=equipLayout.cards[i],name=equipLayout.names[i];
            equipmentSlots.append(QVariantMap{{"x",x+icon.x},{"y",y+icon.y},{"width",icon.width},{"height",icon.height},
                {"cardX",x+card.x},{"cardY",y+card.y},{"cardWidth",card.width},{"cardHeight",card.height},
                {"nameX",x+name.x},{"nameY",y+name.y},{"nameWidth",name.width},{"nameHeight",name.height},
                {"slotName",equipmentLayout::labels[i]},{"occupied",!equipEmpty},{"hovered",i==9},{"itemId",1750+i},
                {"label",equipExtreme?QString("+10 Quadruple Very Long Prefix %1 of Extremely Long Card Suffix [4]").arg(names[i]):names[i]}});
        }
        state.setEquipData(QVariantMap{{"title","Equipment"},{"compact",equipCompact},{"slots",equipmentSlots},{"previewRevision","1"},
            {"previewX",equipLayout.preview.x},{"previewY",equipLayout.preview.y},{"previewWidth",equipLayout.preview.width},{"previewHeight",equipLayout.preview.height},
            {"systemButtons",QVariantList{
                QVariantMap{{"id",136},{"x",x+equipLayout.width-28},{"y",y+3},{"width",12},{"height",11},{"visible",true},{"label",mini?"B":"_"}},
                QVariantMap{{"id",135},{"x",x+equipLayout.width-14},{"y",y+3},{"width",12},{"height",11},{"visible",true},{"label","X"}},
                QVariantMap{{"id",145},{"x",x+equipLayout.width-42},{"y",y+3},{"width",12},{"height",11},{"visible",!mini},{"label","C"},{"icon",equipCompact?"details":"compact"}},
                QVariantMap{{"id",138},{"x",x+equipLayout.removeOption.x},{"y",y+equipLayout.removeOption.y},
                    {"width",equipLayout.removeOption.width},{"height",equipLayout.removeOption.height},
                    {"visible",!mini&&!equipEmpty},{"label",equipExtreme?"Remove cart":"Release Falcon"}}}}});
    }
    if (treeCase) {
        state.setBasicInfoVisible(false);state.setOptionVisible(false);state.setStatusVisible(false);state.setShortCutVisible(false);state.setAnchors({});
        QFile catalog("src/qtui/data/skill_tree_catalog.json"); if(!catalog.open(QIODevice::ReadOnly))return 8;
        const auto root=QJsonDocument::fromJson(catalog.readAll()).object();
        std::vector<skilltree::Skill> definitions;
        for(const auto& j:root["jobs"].toArray()) {
            std::vector<skilltree::Skill> jobDefinitions;
            for(const auto& entry:j.toObject()["skills"].toArray()) {
                auto n=entry.toObject();skilltree::Skill sk;
                sk.id=n["id"].toInt();sk.cap=n["cap"].toInt();sk.name=n["name"].toString().toStdString();sk.symbol=n["symbol"].toString().toStdString();
                sk.baseLevel=n["baseLevel"].toInt();sk.jobLevel=n["jobLevel"].toInt();sk.quest=n["quest"].toBool();sk.passive=n["passive"].toBool();
                sk.grantedOnly=n["grantedOnly"].toBool();sk.accessGroup=n["accessGroup"].toInt(-1);
                for(const auto& r:n["requires"].toArray())sk.requires.push_back({r.toObject()["id"].toInt(),r.toObject()["level"].toInt()});
                jobDefinitions.push_back(sk);
            }
            jobDefinitions=skilltree::VisibleSkills(jobDefinitions,{});
            skilltree::Model layoutModel;if(!layoutModel.Load(jobDefinitions))return 9;
            for(int viewportWidth:{264,400,764}) {
                const auto layout=skilltree::Layout(layoutModel,viewportWidth);
                for(const auto& [id,p]:layout) {
                    if(p.x<0||p.x+skilltree::Geometry::width+skilltree::Geometry::margin>viewportWidth)return 10;
                    for(const auto& req:layoutModel.skills.at(id).requires)if(layout.at(req.id).y>=p.y)return 11;
                    for(const auto& [other,q]:layout)if(id!=other
                        &&p.x<q.x+skilltree::Geometry::width&&q.x<p.x+skilltree::Geometry::width
                        &&p.y<q.y+skilltree::Geometry::height&&q.y<p.y+skilltree::Geometry::height)return 12;
                }
            }
            if(j.toObject()["id"].toInt()==(gatedTree?8:4))definitions=std::move(jobDefinitions);
        }
        skilltree::Model model;if(!model.Load(definitions))return 9;
        skilltree::Snapshot snap;snap.points=5;snap.baseLevel=30;snap.jobLevel=30;
        if(gatedTree){snap.enforceServerGroups=true;snap.availableGroups={0,4};snap.levels[1]=9;}
        model.Sync(snap);
        if(!gatedTree){model.Stage(30);model.Stage(30);}
        const auto positions=skilltree::Layout(model);const auto requirements=model.Requirements(30);
        QVariantList nodes,edges;int contentWidth=0,contentHeight=0;
        for(const auto& [id,sk]:model.skills) {
            const auto pos=positions.at(id);
            nodes.append(QVariantMap{{"id",id},{"name",QString::fromStdString(sk.name)},{"symbol",QString::fromStdString(sk.symbol)},
                {"x",pos.x},{"y",pos.y},{"learned",model.Learned(id)},{"level",model.Projected(id)},{"selected",model.Selected(id)},
                {"jobBlocked",!model.GroupAvailable(id)},{"pending",model.Pending(id)>0},{"locked",!model.Unlocked(id)},{"available",model.Unlocked(id)&&model.Projected(id)<sk.cap&&!sk.quest},
                {"hovered",id==30},{"prerequisite",requirements.count(id)!=0},{"required",requirements.count(id)?requirements.at(id):0},{"passive",sk.passive},{"cap",sk.cap}});
            contentWidth=std::max(contentWidth,pos.x+skilltree::Geometry::width+skilltree::Geometry::margin);contentHeight=std::max(contentHeight,pos.y+skilltree::Geometry::height+skilltree::Geometry::margin);
            for(const auto& req:sk.requires) {
                const auto pp=positions.at(req.id);
                edges.append(QVariantMap{{"x1",pp.x+skilltree::Geometry::width/2},{"y1",pp.y+skilltree::Geometry::height},{"x2",pos.x+skilltree::Geometry::width/2},{"y2",pos.y},
                    {"detour",pos.y-pp.y>skilltree::Geometry::height+skilltree::Geometry::rowGap},
                    {"highlight",(id==30||requirements.count(id))&&requirements.count(req.id)},{"met",model.Projected(req.id)>=req.level},{"level",req.level}});
            }
        }
        state.setSkillListVisible(true);state.setSkillListGeometry(24,24,std::max(480,contentWidth+16),580);
        state.setSkillListData(QVariantMap{{"geometry",SkillTreeGeometryForQt()},{"treeMode",true},{"title",gatedTree?"Skill Tree - Priest":"Skill Tree - Acolyte"},{"points",model.Remaining()},{"pending",static_cast<int>(model.draft.size())},
            {"nodes",nodes},{"edges",edges},{"contentWidth",contentWidth},{"contentHeight",contentHeight},{"scrollX",0},{"scrollY",0},
            {"canApply",!gatedTree},{"canReset",!gatedTree},{"applying",false},{"collapsed",false},
            {"status",gatedTree?"Apply earlier-job points to unlock Priest skills":"Planned Heal Lv. 2 for Decrease Agility"},
            {"tooltip",gatedTree?"Magnificat | Not available from server yet. Apply earlier-job points first.":"Decrease Agility | Heal 2 / 3 required | Increase Agility 0 / 1 required"}});
    }
    QQuickView view;
    view.setColor(QColor("#b9c5cd"));view.resize(small?640:large?1920:qRound(710*scale),small?480:large?1080:qRound(550*scale));
    if(treeCase)view.resize(qRound(828*scale),qRound(628*scale));
    if(statusCase)view.resize(qRound(456*scale),qRound((mini?50:284)*scale));
    if(equipCase)view.resize(qRound((equipLayout.width+24)*scale),qRound((mini?42:equipLayout.height+24)*scale));
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.rootContext()->setContextProperty("uiState", &state);
    if(treeCase||equipCase) {
        class Icons : public QQuickImageProvider {
        public: Icons():QQuickImageProvider(QQuickImageProvider::Image){}
            QImage requestImage(const QString& id,QSize* size,const QSize&)override {
                if(id.startsWith("equippreview")) {
                    QImage image(72,154,QImage::Format_ARGB32);image.fill(Qt::transparent);
                    QPainter p(&image);p.setPen(QColor("#47596a"));
                    p.setBrush(QColor("#725970"));p.drawEllipse(22,6,28,24);
                    p.setBrush(QColor("#e3b8a0"));p.drawEllipse(25,15,22,24);
                    p.setBrush(QColor("#58768a"));p.drawRoundedRect(17,43,38,59,6,6);
                    p.setBrush(QColor("#8d6d56"));p.drawRect(10,58,10,39);p.drawRect(52,58,10,39);
                    p.setBrush(QColor("#47596a"));p.drawRect(22,100,11,36);p.drawRect(40,100,11,36);
                    p.setBrush(QColor("#73563e"));p.drawRect(17,134,17,12);p.drawRect(39,134,17,12);
                    if(size)*size=image.size();return image;
                }
                QImage image(32,32,QImage::Format_ARGB32);image.fill(id.contains("grey=1")?QColor("#adb4ba"):QColor("#7b9dca"));
                QPainter p(&image);p.setPen(Qt::white);p.drawText(image.rect(),Qt::AlignCenter,id.section('/',1).section('?',0,0));
                if(size)*size=image.size();return image;
            }
        };
        view.engine()->addImageProvider("openmidgard",new Icons);
    }
    view.setSource(QUrl::fromLocalFile(QDir::current().absoluteFilePath("src/qtui/qml/GameOverlay.qml")));
    if(view.status()==QQuickView::Error)return 2;
    QQmlComponent themeComponent(view.engine(), QUrl::fromLocalFile(QDir::current().absoluteFilePath("src/qtui/qml/RoTheme.qml")));
    std::unique_ptr<QObject> themeObject(themeComponent.create());
    if (!themeObject || themeObject->property("titleHeight").toInt() != uiMetrics::titleHeight) return 7;
    view.show();
    QTimer::singleShot(600,[&]{
        if (statusCase && !mini) {
            // Inspect the actual QML items against the native click rectangles.
            for (int i=0;i<6;++i) {
                auto* button=FindVisualItem(view.rootObject(),QString("statusIncrease%1").arg(i));
                if (!button) { app.exit(12); return; }
                const QPointF position=button->mapToScene(QPointF(0,0));
                if (std::abs(position.x()-(statusX+statusLayout::increaseX)*scale)>0.1
                    ||std::abs(position.y()-(statusY+statusLayout::rowY(i)+1)*scale)>0.1
                    ||button->width()!=statusLayout::increaseSize||button->height()!=statusLayout::increaseSize) {
                    qCritical()<<"Status button differs from native hit rectangle"<<i<<position;
                    app.exit(13);return;
                }
            }
            for (int i=0;i<8;++i) {
                auto* value=FindVisualItem(view.rootObject(),QString("statusCombatValue%1").arg(i));
                if (!value||value->property("truncated").toBool()
                    ||value->property("paintedWidth").toDouble()>value->width()+0.5) {
                    qCritical()<<"Status combat value clipped"<<i;
                    app.exit(14);return;
                }
            }
        }
        if(equipCase&&!mini) {
            for(int i=0;i<11;++i) {
                auto* name=FindVisualItem(view.rootObject(),QString("equipmentName%1").arg(i));
                auto* card=FindVisualItem(view.rootObject(),QString("equipmentCard%1").arg(i));
                if(!name||!card||(!equipCompact&&(name->property("paintedHeight").toDouble()>name->height()+0.5
                    ||name->property("paintedWidth").toDouble()>name->width()+0.5))
                    ||(equipCompact&&name->isVisible())) {
                    qCritical()<<"Equipment full name clipped"<<i;app.exit(15);return;
                }
                const auto pos=card->mapToScene(QPointF(0,0));
                const auto expected=equipLayout.cards[i];
                if(std::abs(pos.x()-(12+expected.x)*scale)>0.1||std::abs(pos.y()-(12+expected.y)*scale)>0.1
                    ||card->width()!=expected.width||card->height()!=expected.height) {
                    qCritical()<<"Equipment card differs from native hit rectangle"<<i;app.exit(16);return;
                }
            }
        }
        QDir().mkpath("build-qt-preview/renders");
        const QString out=QString("build-qt-preview/renders/gui-%1-%2.png").arg(scale).arg((statusCase||equipCase)?QString(argv[2]):gatedTree?"tree-gated":treeCase?"tree":small?"small":large?"large":mini?"mini":extreme?"extreme":"normal");
        QImage image=view.grabWindow();
        if(image.isNull()||!image.save(out)){qCritical()<<"Unable to render";app.exit(3);return;}
        qInfo()<<"Saved"<<out;app.exit(qmlErrors ? 5 : 0);
    });
    return app.exec();
}
