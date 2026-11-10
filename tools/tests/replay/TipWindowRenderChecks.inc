#include "ui/UITipOfTheDayWnd.h"
#include "core/TipOfTheDay.h"

void CheckTipWindowUi(QGuiApplication& app,QQuickView& view,QtUiState* state,
    CGameMode& mode,QtUiStateAdapter& adapter,const QString& output)
{
    InventoryTabViewportFixture viewport;viewport.Create("tip-window");
    g_windowMgr.RemoveAllWindows();
    struct Restore {
        CTipOfTheDay tips=g_tipOfTheDay;int startup=g_showTipsAtStartup,scale=GetConfiguredUiScalePercent(),folder=g_readFolderFirst,code=g_codePage;
        ServiceType service=g_serviceType;bool checked=g_windowMgr.m_tipStartupChecked;
        decltype(g_session.m_cmdOnOffSet) flags=g_session.m_cmdOnOffSet;std::filesystem::path cwd=std::filesystem::current_path();
        ~Restore(){g_windowMgr.RemoveAllWindows();g_tipOfTheDay=std::move(tips);g_showTipsAtStartup=startup;g_serviceType=service;g_codePage=code;
            g_readFolderFirst=folder;g_windowMgr.m_tipStartupChecked=checked;g_session.m_cmdOnOffSet=std::move(flags);SetRuntimeUiScalePercent(scale);std::filesystem::current_path(cwd);}
    } restore;
    const auto assets=std::filesystem::path(output.toStdString())/"tip-assets";std::filesystem::create_directories(assets/"source-bitmaps");
    const std::string sourceDirectory="texture\\\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA";
#if RO_PLATFORM_WINDOWS
    const auto legacy=assets.string()+"\\"+sourceDirectory;CreateDirectoryA((assets.string()+"\\texture").c_str(),nullptr);
    Require(CreateDirectoryA(legacy.c_str(),nullptr) || GetLastError()==ERROR_ALREADY_EXISTS,"Tip legacy fixture directory failed");
#else
    std::string legacy=assets.string()+"/"+sourceDirectory;std::replace(legacy.begin(),legacy.end(),'\\','/');std::filesystem::create_directories(legacy);
#endif
    const auto bmp=[&](const std::string& name,const char* color){QImage image(42,20,QImage::Format_RGB32);image.fill(QColor(color));
        QByteArray bytes;QBuffer buffer(&bytes);buffer.open(QIODevice::WriteOnly);Require(image.save(&buffer,"BMP"),"Tip bitmap encode failed");
        {std::ofstream file(assets/"source-bitmaps"/name,std::ios::binary);file.write(bytes.constData(),bytes.size());}
#if RO_PLATFORM_WINDOWS
        const auto file=CreateFileA((legacy+"\\"+name).c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        Require(file!=INVALID_HANDLE_VALUE,"Tip bitmap fixture open failed");DWORD count=0;const bool wrote=WriteFile(file,bytes.constData(),DWORD(bytes.size()),&count,nullptr)!=0;CloseHandle(file);
        Require(wrote && count==DWORD(bytes.size()),"Tip bitmap fixture write failed");
#else
        std::ofstream file(std::filesystem::path(legacy)/name,std::ios::binary);file.write(bytes.constData(),bytes.size());
#endif
    };
    for(const char* base:{"btn_back","btn_next","btn_close"}){bmp(std::string(base)+".bmp","#4a83b0");bmp(std::string(base)+"_a.bmp","#6aa3d0");bmp(std::string(base)+"_b.bmp","#245580");}
    std::filesystem::current_path(assets);g_readFolderFirst=1;g_codePage=65001;g_serviceType=ServiceKorea;SetRuntimeUiScalePercent(100);
    const auto labels=[&](bool longLabel){std::ofstream file("msgstringtable.txt",std::ios::binary);for(int i=0;i<=176;++i)
        file<<(i==175?"Did you know?":i==176?(longLabel?std::string(300,'X'):std::string("Show tips at startup")):std::string())<<"#\r\n";};
    labels(false);{std::ofstream file("tipOfTheday.txt",std::ios::binary);file<<"^FF0000<RED SOURCE TIP>\n";
        for(int i=0;i<16;++i)file<<"tip row "<<i<<"\n";file<<"#SECOND TIP#";}
    g_tipOfTheDay=CTipOfTheDay{};Require(g_tipOfTheDay.Load(9),"Tip rendering data load failed");g_showTipsAtStartup=1;
    Require(mode.SendMsg(47,42,0,0),"Tip render command42 failed");auto* tip=dynamic_cast<UITipOfTheDayWnd*>(g_windowMgr.m_tipOfTheDayWnd);
    Require(tip && tip->GetButtons()[0]->GetButton().bitmaps[0].IsValid(),"actual Tip source-key assets not loaded");
    if(tip->GetTipIndex())tip->SendMsg(nullptr,6,180,0,0);
    const auto pointer=[&](UINT msg,int x,int y){WindowProc(g_hMainWnd,msg,msg==WM_LBUTTONUP?0:MK_LBUTTON,
        MAKELPARAM(UiScaleLogicalToRawCoordinate(x),UiScaleLogicalToRawCoordinate(y)));};
    QImage native,qt;int frames=0;
    const auto render=[&](){state->setUiScale(GetConfiguredUiScaleFactor());adapter.syncChat();Frames(app,view);
        ArgbDibSurface dib;Require(dib.EnsureSize(view.width(),view.height()),"Tip native DIB failed");std::fill_n(dib.GetPixels(),view.width()*view.height(),0xff181b20u);
        g_windowMgr.DrawVisibleWindowsToHdc(dib.GetDC(),true);
        native=QImage(reinterpret_cast<uchar*>(dib.GetPixels()),view.width(),view.height(),view.width()*4,QImage::Format_RGB32).copy();
        const double scale=GetConfiguredUiScaleFactor();native=native.scaled(qRound(native.width()*scale),qRound(native.height()*scale),Qt::IgnoreAspectRatio,Qt::FastTransformation).copy(0,0,view.width(),view.height());
        qt=view.grabWindow().convertToFormat(QImage::Format_RGB32);};
    const auto capture=[&](const QString& name,bool red){render();auto* owner=Find(view.rootObject(),"sourceTip");auto* viewer=Find(view.rootObject(),"sourceTipViewer");
        native.save(output+"/"+name+".native.png");qt.save(output+"/"+name+".qml.png");
        const double scale=GetConfiguredUiScaleFactor();Require(owner && viewer && owner->isVisible(),"actual Tip QML tree missing");
        const auto bounds=owner->mapRectToScene(owner->boundingRect()),v=viewer->mapRectToScene(viewer->boundingRect());
        Require(std::abs(bounds.left()-tip->m_x*scale)<0.001 && std::abs(bounds.width()-280*scale)<0.001 && std::abs(bounds.height()-180*scale)<0.001
            && std::abs(v.left()-(tip->m_x+10)*scale)<0.001 && std::abs(v.top()-(tip->m_y+22)*scale)<0.001
            && std::abs(v.width()-260*scale)<0.001 && std::abs(v.height()-125*scale)<0.001,"Tip native/QML selected geometry differs");
        for(auto* button:tip->GetButtons()){const QPoint sample(UiScaleLogicalToRawCoordinate(button->m_x+5),UiScaleLogicalToRawCoordinate(button->m_y+5));
            const QColor color=button->GetVisualState()==2?QColor("#245580"):button->GetVisualState()==1?QColor("#6aa3d0"):QColor("#4a83b0");
            Require(native.pixelColor(sample)==color && qt.pixelColor(sample)==color,
                ("Tip native/QML button state pixels differ: "+name.toStdString()+" id="+std::to_string(button->m_id)+" native="+native.pixelColor(sample).name().toStdString()+" qml="+qt.pixelColor(sample).name().toStdString()+" expected="+color.name().toStdString()).c_str());}
        if(red){const auto redPixels=[&](const QImage& frame){int n=0;const QRect area=v.toAlignedRect().intersected(frame.rect());
                for(int y=area.top();y<=area.bottom();++y)for(int x=area.left();x<=area.right();++x){const auto c=frame.pixelColor(x,y);if(c.red()>150 && c.green()<100 && c.blue()<100)++n;}return n;};
            Require(redPixels(native)>10 && redPixels(qt)>10,"Tip prior-row red color/plain text missing in native/QML rendering");}
        Require(native.save(output+"/"+name+".native.png") && qt.save(output+"/"+name+".qml.png"),"Tip render frame save failed");++frames;};
    capture("tip-initial",true);
    auto* next=tip->GetButtons()[1];pointer(WM_MOUSEMOVE,next->m_x+4,next->m_y+4);capture("tip-hover",true);
    pointer(WM_LBUTTONDOWN,next->m_x+4,next->m_y+4);Require(GetCapture()==g_hMainWnd && g_windowMgr.m_captureWindow==next,"Tip child press did not capture through native manager");capture("tip-pressed",true);
    pointer(WM_LBUTTONUP,-20,-20);Require(tip->GetTipIndex()==0 && !GetCapture(),"Tip outside release activated next/retained capture");
    auto* viewer=tip->GetViewer();g_windowMgr.OnWheel(viewer->m_x+8,viewer->m_y+8,-120);Require(viewer->GetOffset()==3,"Tip actual wheel did not route to child viewer");capture("tip-scroll-color",true);
    const auto bar=viewer->GetScrollBar();const int sx=viewer->m_x+viewer->m_w-6,sy=viewer->m_y+bar.ThumbStart()+2;
    pointer(WM_LBUTTONDOWN,sx,sy);pointer(WM_MOUSEMOVE,sx,viewer->m_y+viewer->m_h-20);pointer(WM_LBUTTONUP,sx,viewer->m_y+viewer->m_h-20);
    Require(viewer->GetOffset()>3 && !GetCapture(),"Tip actual thumb drag failed");capture("tip-thumb",true);
    const int x=tip->m_x,y=tip->m_y;pointer(WM_LBUTTONDOWN,x+2,y+2);pointer(WM_MOUSEMOVE,x+27,y+17);pointer(WM_LBUTTONUP,-20,-20);
    Require(tip->m_x==x+25 && tip->m_y==y+15 && viewer->m_x==tip->m_x+10,"Tip actual frame drag failed to move children");
    pointer(WM_LBUTTONDOWN,tip->m_x+2,tip->m_y+2);WindowProc(g_hMainWnd,WM_CANCELMODE,0,0);pointer(WM_MOUSEMOVE,x+150,y+150);
    Require(tip->m_x==x+25 && !GetCapture() && !g_windowMgr.m_captureWindow,"cancelled Tip drag remained active");
    const int blankX=tip->m_x,blankY=tip->m_y;
    pointer(WM_LBUTTONDOWN,blankX+4,blankY+40);pointer(WM_MOUSEMOVE,blankX+9,blankY+45);pointer(WM_LBUTTONUP,-20,-20);
    Require(tip->m_x==blankX+5 && tip->m_y==blankY+5,"source Tip blank-frame hit did not drag outside titlebar");
    SetRuntimeUiScalePercent(125);capture("tip-125",true);
    next=tip->GetButtons()[1];pointer(WM_LBUTTONDOWN,next->m_x+4,next->m_y+4);pointer(WM_LBUTTONUP,next->m_x+4,next->m_y+4);
    Require(tip->GetTipIndex()==1 && viewer->GetOffset()==0 && viewer->GetItems().back()=="SECOND TIP","Tip native next did not update data/reset scroll");capture("tip-next",false);
    auto* check=tip->GetStartupCheckBox();const bool was=check->IsChecked();pointer(WM_LBUTTONDOWN,check->m_x+2,check->m_y+2);
    Require(check->IsChecked()!=was && g_showTipsAtStartup==int(!was),"Tip native checkbox did not notify on down");pointer(WM_LBUTTONUP,-20,-20);
    auto* close=tip->GetButtons()[2];pointer(WM_LBUTTONDOWN,close->m_x+4,close->m_y+4);g_windowMgr.DeleteWindow(tip);
    Require(!g_windowMgr.m_tipOfTheDayWnd && !g_windowMgr.m_captureWindow && !GetCapture(),"Tip captured owner deletion left dangling native capture");
    labels(true);{std::ofstream file("tipOfTheday.txt",std::ios::binary);}g_tipOfTheDay=CTipOfTheDay{};
    Require(mode.SendMsg(47,42,0,0),"Tip empty render reopen failed");tip=dynamic_cast<UITipOfTheDayWnd*>(g_windowMgr.m_tipOfTheDayWnd);
    Require(tip && tip->GetViewer()->GetItems().empty() && tip->GetStartupCheckBox()->m_w>280,"Tip empty/long-label source bounds changed");
    pointer(WM_LBUTTONDOWN,tip->m_x+240,tip->m_y+168);
    Require(g_windowMgr.m_captureWindow==tip->GetStartupCheckBox(),"Tip source child order did not prioritize late long checkbox over button");pointer(WM_LBUTTONUP,-20,-20);
    // Long label deliberately overlaps buttons; geometry/clip checks are separate from art-state sample assertions.
    render();Require(native.save(output+"/tip-empty-long.native.png") && qt.save(output+"/tip-empty-long.qml.png"),"Tip long-label frames failed");++frames;
    g_windowMgr.DeleteWindow(tip);render();Require(!Find(view.rootObject(),"sourceTip")->isVisible(),"deleted Tip remained in actual Qt tree");
    QFile result(output+"/tip-window-results.json");Require(result.open(QIODevice::WriteOnly),"Tip results open failed");
    result.write(QJsonDocument(QJsonObject{{"frames",frames},{"source_viewer_height",125},{"complete_rows",6},{"actual_native_input",true},{"scale125",true},{"source_child_order",true},{"captured_owner_teardown",true},{"outcome","PASS"}}).toJson());
}
