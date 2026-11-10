#include "ui/UICommandListWnd.h"
#include "ui/UITransBalloonText.h"
#include "core/SourceCommandListTables.h"
#include "ui/UiScale.h"
#include "main/WinMain.h"
#include "gamemode/Mode.h"
#include <QBuffer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQuickItem>
#include <QThread>
#include <filesystem>
#include <fstream>

void CheckCommandWindowUi(QGuiApplication& app,QQuickView& view,QtUiState* state,
    CGameMode& mode,QtUiStateAdapter& adapter,const QString& output)
{
    Require(state!=nullptr,"command list Qt state is missing");
    InventoryTabViewportFixture viewport;viewport.Create("command-window");
    g_windowMgr.RemoveAllWindows();
    struct Restore {
        int scale=GetConfiguredUiScalePercent(),folder=g_readFolderFirst,codePage=g_codePage;
        int battle=g_session.m_isBattle;
        ServiceType service=g_serviceType;
        decltype(g_session.m_cmdOnOffSet) flags=g_session.m_cmdOnOffSet;
        std::filesystem::path cwd=std::filesystem::current_path();
        ~Restore(){g_windowMgr.RemoveAllWindows();g_session.m_cmdOnOffSet=std::move(flags);g_serviceType=service;
            g_readFolderFirst=folder;g_codePage=codePage;g_session.m_isBattle=battle;SetRuntimeUiScalePercent(scale);std::filesystem::current_path(cwd);}
    } restore;

    const auto assets=std::filesystem::path(output.toStdString())/"command-window-assets";
    std::filesystem::create_directories(assets/"source-bitmaps");
    const std::string encodedUi="\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA";
    const std::string sourceDirectory="texture\\"+encodedUi+"\\basic_interface";
#if RO_PLATFORM_WINDOWS
    const auto texture=(assets.string()+"\\texture");
    const auto language=(texture+"\\"+encodedUi);
    const auto legacy=(language+"\\basic_interface");
    Require(CreateDirectoryA(texture.c_str(),nullptr) || GetLastError()==ERROR_ALREADY_EXISTS,"command texture fixture directory failed");
    Require(CreateDirectoryA(language.c_str(),nullptr) || GetLastError()==ERROR_ALREADY_EXISTS,"command source-key fixture directory failed");
    Require(CreateDirectoryA(legacy.c_str(),nullptr) || GetLastError()==ERROR_ALREADY_EXISTS,"command basic_interface fixture directory failed");
#else
    std::string legacy=assets.string()+"/"+sourceDirectory;
    std::replace(legacy.begin(),legacy.end(),'\\','/');std::filesystem::create_directories(legacy);
#endif
    struct BitmapFixture {const char* name;int width,height;const char* color;};
    const std::array<BitmapFixture,17> bitmaps{{
        {"tab_cmd_01.bmp",20,132,"#ac342c"},{"tab_cmd_02.bmp",20,132,"#286c9c"},{"tab_cmd_03.bmp",20,132,"#7c5820"},
        {"sys_base_off.bmp",11,11,"#9c3030"},{"sys_base_on.bmp",11,11,"#d05050"},
        {"sys_close_off.bmp",11,11,"#308040"},{"sys_close_on.bmp",11,11,"#50b060"},
        {"grp_offline.bmp",8,8,"#b040a0"},{"grp_online.bmp",8,8,"#40b0c0"},
        {"titlebar_left.bmp",12,17,"#d24240"},{"titlebar_mid.bmp",12,17,"#42d240"},{"titlebar_right.bmp",12,17,"#4042d2"},
        {"itemwin_left.bmp",8,8,"#d2d240"},{"itemwin_right.bmp",8,8,"#42d2d2"},
        {"btnbar_left.bmp",21,21,"#c040c0"},{"btnbar_mid.bmp",21,21,"#40c0a0"},{"btnbar_right.bmp",21,21,"#c08040"}
    }};
    for(const auto& spec:bitmaps){
        QImage image(spec.width,spec.height,QImage::Format_RGB32);image.fill(QColor(spec.color));
        QByteArray bytes;QBuffer buffer(&bytes);buffer.open(QIODevice::WriteOnly);
        Require(image.save(&buffer,"BMP"),"command bitmap fixture encoding failed");
        {std::ofstream file(assets/"source-bitmaps"/spec.name,std::ios::binary);file.write(bytes.constData(),bytes.size());
            Require(file.good(),"command bitmap fixture archive copy failed");}
#if RO_PLATFORM_WINDOWS
        const auto path=legacy+"\\"+spec.name;
        const auto file=CreateFileA(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        Require(file!=INVALID_HANDLE_VALUE,"command source-key bitmap open failed");DWORD count=0;
        const bool wrote=WriteFile(file,bytes.constData(),DWORD(bytes.size()),&count,nullptr)!=0;CloseHandle(file);
        Require(wrote && count==DWORD(bytes.size()),"command source-key bitmap write failed");
#else
        std::ofstream file(std::filesystem::path(legacy)/spec.name,std::ios::binary);
        file.write(bytes.constData(),bytes.size());Require(file.good(),"command source-key bitmap write failed");
#endif
    }
    {
        std::ofstream messages(assets/"msgstringtable.txt",std::ios::binary);
        for(int i=0;i<=787;++i)messages<<(i==787?"COMMAND HOVER HELP FIXTURE":"")<<"#\r\n";
        Require(messages.good(),"command help fixture message table failed");
    }
    std::filesystem::current_path(assets);g_readFolderFirst=1;g_codePage=65001;g_serviceType=ServiceKorea;
    SetRuntimeUiScalePercent(100);
    g_session.m_cmdOnOffSet.clear();
    for(const auto& item:ro::chat::SourceCommandToggles)g_session.m_cmdOnOffSet.push_back({item.value==134?1:0,item.value});

    auto* window=dynamic_cast<UICommandListWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_COMMANDLISTWND));
    Require(window && window->m_show && window->m_w==250 && window->GetVisibleRows().size()==16,
        "actual source command window creation/visible row fixture differs");
    Require(window->GetTab()->GetBitmap().IsValid() && window->GetIndicatorBitmap(true).IsValid()
        && window->GetIndicatorBitmap(false).IsValid() && window->GetFramePieces().size()>20,
        "command source-key bitmaps were not loaded by the native controller");
    Require(g_modeMgr.GetCurrentMode()==&mode,"command hover fixture requires the supplied live CGameMode owner");

    const auto pointer=[&](UINT msg,int x,int y){
        WindowProc(g_hMainWnd,msg,msg==WM_LBUTTONDOWN || (msg==WM_MOUSEMOVE && g_windowMgr.m_captureWindow)?MK_LBUTTON:0,
            MAKELPARAM(UiScaleLogicalToRawCoordinate(x),UiScaleLogicalToRawCoordinate(y)));
    };
    const auto click=[&](int x,int y){pointer(WM_LBUTTONDOWN,x,y);pointer(WM_LBUTTONUP,x,y);};
    const auto wheel=[&](int x,int y,int delta){POINT screen{UiScaleLogicalToRawCoordinate(x),UiScaleLogicalToRawCoordinate(y)};
        Require(ClientToScreen(g_hMainWnd,&screen)!=FALSE,"command wheel client-to-screen conversion failed");
        WindowProc(g_hMainWnd,WM_MOUSEWHEEL,MAKEWPARAM(0,static_cast<WORD>(static_cast<short>(delta))),MAKELPARAM(screen.x,screen.y));};

    int frames=0;
    const auto render=[&](const QString& name){
        state->setUiScale(GetConfiguredUiScaleFactor());adapter.syncChat();
        view.requestUpdate();for(int i=0;i<6;++i){app.processEvents();QThread::msleep(20);}
        auto* owner=Find(view.rootObject(),"sourceCommandList");
        Require(owner && owner->isVisible(),"actual QML command-list owner missing");
        const auto bounds=owner->mapRectToScene(owner->boundingRect());const double scale=GetConfiguredUiScaleFactor();
        Require(std::abs(bounds.left()-window->m_x*scale)<0.01 && std::abs(bounds.top()-window->m_y*scale)<0.01
            && std::abs(bounds.width()-window->m_w*scale)<0.01 && std::abs(bounds.height()-window->m_h*scale)<0.01,
            "actual command-list native/QML owner geometry differs");
        const auto data=state->chatWindowUi().value("commandList").toMap();
        Require(data.value("framePieces").toList().size()==static_cast<int>(window->GetFramePieces().size()),
            "actual QML frame-piece projection differs from native frame");
        ArgbDibSurface dib;Require(dib.EnsureSize(view.width(),view.height()),"command native DIB allocation failed");
        std::fill_n(dib.GetPixels(),view.width()*view.height(),0xff181b20u);g_windowMgr.DrawVisibleWindowsToHdc(dib.GetDC(),true);
        QImage native=QImage(reinterpret_cast<uchar*>(dib.GetPixels()),view.width(),view.height(),view.width()*4,QImage::Format_RGB32).copy();
        if(GetConfiguredUiScalePercent()!=100)native=native.scaled(qRound(native.width()*scale),qRound(native.height()*scale),
            Qt::IgnoreAspectRatio,Qt::FastTransformation).copy(0,0,view.width(),view.height());
        const QImage qml=view.grabWindow().convertToFormat(QImage::Format_RGB32);
        Require(native.save(output+"/"+name+".native.png") && qml.save(output+"/"+name+".qml.png"),
            "command native/QML frame save failed");
        const auto sample=[&](int x,int y){return QPoint(UiScaleLogicalToRawCoordinate(x),UiScaleLogicalToRawCoordinate(y));};
        const auto same=[&](QPoint p,const QColor& expected,const char* what){
            Require(native.rect().contains(p) && qml.rect().contains(p),"command frame sample outside viewport");
            if(native.pixelColor(p)!=expected || qml.pixelColor(p)!=expected){
                throw std::runtime_error((std::string("command native/QML ")+what+" source pixel mismatch: native="
                    +native.pixelColor(p).name().toStdString()+" qml="+qml.pixelColor(p).name().toStdString()
                    +" expected="+expected.name().toStdString()).c_str());
            }
        };
        same(sample(window->m_x+1,window->m_y+1),QColor("#d24240"),"title-left art");
        same(sample(window->m_x+22,window->m_y+30),QColor("white"),"inner body fill");
        same(sample(window->m_x+2,window->m_y+151),QColor("#d2d240"),"left side art");
        const QColor tabColor=window->GetTab()->m_state==0?QColor("#ac342c"):
            window->GetTab()->m_state==1?QColor("#286c9c"):QColor("#7c5820");
        same(sample(window->m_x+10,window->m_y+20),tabColor,"active tab art");
        same(sample(window->m_x+24,window->m_y+window->m_h-1),QColor("#40c0a0"),"bottom mid art");
        if(window->GetCurrentItem()>=0){
            const auto rows=window->GetVisibleRows();
            const auto row=std::find_if(rows.begin(),rows.end(),[&](const auto& item){return item.index==window->GetCurrentItem();});
            if(row!=rows.end())same(sample(window->m_x+row->x,window->m_y+row->y+2),QColor("#808080"),"selected-row border");
            if(row!=rows.end() && row->hasIndicator){
                const auto& indicator=window->GetIndicatorBitmap(row->on);
                same(sample(window->m_x+row->x+2+indicator.width/2,window->m_y+row->y+5+indicator.height/2),
                    QColor(row->on?"#40b0c0":"#b040a0"),"toggle indicator");
            }
        }
        ++frames;
    };

    const int windowX=window->m_x,windowY=window->m_y;
    click(windowX+55,windowY+25);
    Require(window->GetTab()->m_state==0 && window->GetCurrentItem()==0,
        "native manager input did not select the first toggle row");
    auto toggles=window->GetVisibleRows();
    Require(!toggles.empty() && toggles.front().hasIndicator && toggles.front().index==0
        && toggles.front().value==ro::chat::SourceCommandToggles[0].value,
        "first matching native toggle indicator missing");
    render("command-toggle-selected");

    click(windowX+5,windowY+17+45);
    Require(window->GetTab()->m_state==1,"native manager input did not select action tab");
    wheel(windowX+window->m_w-8,windowY+30,-120);
    Require(window->GetOffset()>0 && window->GetVisibleRows().front().index==2*window->GetOffset(),
        "native manager wheel did not scroll the action table");
    render("command-actions-scrolled");

    click(windowX+5,windowY+17+90);g_serviceType=ServiceBrazil;g_codePage=1252;
    Require(window->GetTab()->m_state==2 && window->GetTitle()=="Lista de Comandos",
        "Brazil window-tab title/state fixture differs");
    render("command-brazil-windows");

    mode.ProcessHelpBalloon(mode.GetHelpBalloonTick()+100);g_windowMgr.OnProcess();
    click(windowX+5,windowY+17+1);
    wheel(windowX+window->m_w-8,windowY+30,120);
    pointer(WM_MOUSEMOVE,windowX+55,windowY+25);
    auto* help=mode.GetHelpBalloon();
    Require(window->GetTab()->m_state==0 && help && !help->GetDisplayLines().empty()
        && help->GetDisplayLines().front().find("COMMAND HOVER HELP FIXTURE")!=std::string::npos,
        "native manager hover did not create the expected source help balloon");
    render("command-hover-help");
    auto* helpItem=Find(view.rootObject(),"sourceHelpBalloon");
    Require(helpItem && helpItem->isVisible(),"actual QML source help balloon missing");

    SetRuntimeUiScalePercent(125);render("command-125-percent");
    auto* close=window->GetButtons()[1];pointer(WM_MOUSEMOVE,close->m_x+1,close->m_y+1);
    Require(mode.GetHelpBalloon() && mode.GetHelpBalloon()->GetLines().front()=="Alt+Y","native close-button tooltip was not created");
    render("command-close-tooltip");
    auto* scroll=window->GetScrollControl();auto bar=window->GetScrollBar();
    const int thumbX=scroll->m_x+4,thumbY=scroll->m_y+bar.ThumbStart()+1;
    pointer(WM_LBUTTONDOWN,thumbX,thumbY);
    Require(g_windowMgr.m_captureWindow==scroll && GetCapture()==g_hMainWnd,"actual command scrollbar did not acquire native capture");
    pointer(WM_MOUSEMOVE,thumbX,thumbY+600);Require(window->GetOffset()==4,"captured command thumb did not clamp outside client");
    pointer(WM_LBUTTONUP,thumbX,thumbY+600);Require(!g_windowMgr.m_captureWindow && GetCapture()!=g_hMainWnd,"outside release retained command capture");
    bar=window->GetScrollBar();pointer(WM_LBUTTONDOWN,thumbX,scroll->m_y+bar.ThumbStart()+1);
    WindowProc(g_hMainWnd,WM_CANCELMODE,0,0);Require(!g_windowMgr.m_captureWindow && GetCapture()!=g_hMainWnd,"cancel mode retained command descendant capture");
    const int offset=window->GetOffset();pointer(WM_MOUSEMOVE,thumbX,thumbY-300);Require(window->GetOffset()==offset,"cancelled command thumb continued dragging");
    pointer(WM_LBUTTONDOWN,thumbX,scroll->m_y+bar.ThumbStart()+1);window->SendMsg(nullptr,6,135,0,0);
    Require(!g_windowMgr.m_commandListWnd && !g_windowMgr.m_captureWindow && GetCapture()!=g_hMainWnd,"captured command owner deletion retained native capture");
    adapter.syncChat();Require(state->chatWindowUi().value("commandList").toMap().isEmpty(),"deleted command owner retained Qt projection");
    QFile results(output+"/command-window-render-results.json");
    Require(results.open(QIODevice::WriteOnly),"command render result file open failed");
    results.write(QJsonDocument(QJsonObject{{"frames",frames},{"source_key_synthetic_bitmaps",true},
        {"native_manager_windowproc_input",true},{"paired_native_qml_artwork_samples",true},
        {"tab0_selected_first_toggle",true},{"scrolled_action_tab",true},{"brazil_window_tab",true},
        {"native_hover_help_owner",true},{"scale125",true},{"native_thumb_capture_outside_release",true},
        {"cancel_and_captured_owner_delete",true},{"outcome","PASS"}}).toJson(QJsonDocument::Compact));
}
