#include "../../src/qtui/QtUiState.h"
#include "../../src/ui/BasicInfoLayout.h"
#include "../../src/qtui/SkillTreeGeometry.h"
#include <QGuiApplication>
#include <QQuickView>
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
    const bool extreme=argc>2&&QString(argv[2])=="extreme";
    const bool mini=argc>2&&QString(argv[2])=="mini";
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
    state.setStatusVisible(true);state.setStatusGeometry(24,246,280,118);state.setStatusPage(0);
    QVariantList stats;
    const QStringList statNames{"STR","AGI","VIT","INT","DEX","LUK"};
    for(int i=0;i<6;++i)stats.append(QVariantMap{{"label",statNames[i]},{"value",QString::number(30+i*10)},{"canIncrease",false},{"cost",0}});
    state.setStatusData(QVariantMap{{"title","Status"},{"stats",stats},{"attackText","180"},{"matkText","35"},{"hit",165},{"critical",8},{"itemDefText","42"},{"itemMdefText","18"},{"fleeText","185"},{"aspdText","174"},{"statusPoint",12}});
    state.setAnchors(QVariantList{QVariantMap{{"x",24*scale},{"y",390*scale},{"text","Inventory - Alt+E"},{"kind","uiTooltip"},{"background","#f0363b42"},{"foreground","#ffffff"},{"showBubble",true}}});
    if(small) {
        state.setOptionVisible(false);state.setStatusVisible(false);state.setShortCutVisible(false);state.setAnchors({});
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
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.rootContext()->setContextProperty("uiState", &state);
    if(treeCase) {
        class Icons : public QQuickImageProvider {
        public: Icons():QQuickImageProvider(QQuickImageProvider::Image){}
            QImage requestImage(const QString& id,QSize* size,const QSize&)override {
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
        QDir().mkpath("build-qt-preview/renders");
        const QString out=QString("build-qt-preview/renders/gui-%1-%2.png").arg(scale).arg(gatedTree?"tree-gated":treeCase?"tree":small?"small":large?"large":mini?"mini":extreme?"extreme":"normal");
        QImage image=view.grabWindow();
        if(image.isNull()||!image.save(out)){qCritical()<<"Unable to render";app.exit(3);return;}
        qInfo()<<"Saved"<<out;app.exit(qmlErrors ? 5 : 0);
    });
    return app.exec();
}
