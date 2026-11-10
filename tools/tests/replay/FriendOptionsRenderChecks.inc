#include "ui/UIFriendOptionWnd.h"
#include "ui/UIMessengerGroupWnd.h"

void CheckFriendOptionsUi(QGuiApplication& app,QQuickView& view,QtUiState* state,
    CGameMode&,QtUiStateAdapter& adapter,const QString& output)
{
    g_windowMgr.RemoveAllWindows();
    struct Restore {
        int scale=GetConfiguredUiScalePercent(),folder=g_readFolderFirst,codePage=g_codePage;
        bool base=g_session.m_isShowWhisperWnd,friendWindows=g_session.m_isShowWhisperWnd_Friend,sound=g_session.m_isPlayWhisperOpenSound;
        SubChatWindowInfo info=g_windowMgr.m_friendOptionWindowInfo;std::filesystem::path cwd=std::filesystem::current_path();
        ~Restore(){g_windowMgr.RemoveAllWindows();SetRuntimeUiScalePercent(scale);g_readFolderFirst=folder;g_codePage=codePage;std::filesystem::current_path(cwd);
            g_session.m_isShowWhisperWnd=base;g_session.m_isShowWhisperWnd_Friend=friendWindows;g_session.m_isPlayWhisperOpenSound=sound;g_windowMgr.m_friendOptionWindowInfo=info;}
    } restore;
    const auto assets=std::filesystem::path(output.toStdString())/"friend-options-assets";std::filesystem::create_directories(assets/"source-bitmaps");
    const std::string sourceDirectory="texture\\\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA";
#if RO_PLATFORM_WINDOWS
    const auto legacy=assets.string()+"\\"+sourceDirectory;CreateDirectoryA((assets.string()+"\\texture").c_str(),nullptr);
    Require(CreateDirectoryA(legacy.c_str(),nullptr) || GetLastError()==ERROR_ALREADY_EXISTS,"friend legacy directory failed");
    Require(CreateDirectoryA((legacy+"\\basic_interface").c_str(),nullptr) || GetLastError()==ERROR_ALREADY_EXISTS,"friend legacy basic directory failed");
#else
    std::string legacy=assets.string()+"/"+sourceDirectory;std::replace(legacy.begin(),legacy.end(),'\\','/');std::filesystem::create_directories(std::filesystem::path(legacy)/"basic_interface");
#endif
    const auto bmp=[&](const char* name,int width,int height,const char* color){QImage image(width,height,QImage::Format_RGB32);image.fill(QColor(color));
        QByteArray bytes;QBuffer buffer(&bytes);buffer.open(QIODevice::WriteOnly);Require(image.save(&buffer,"BMP"),"friend bitmap encode failed");
        const std::string filename=name;const auto leaf=filename.substr(filename.find_last_of('\\')+1);
        {std::ofstream file(assets/"source-bitmaps"/leaf,std::ios::binary);file.write(bytes.constData(),bytes.size());}
#if RO_PLATFORM_WINDOWS
        const auto file=CreateFileA((legacy+"\\"+name).c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        Require(file!=INVALID_HANDLE_VALUE,"friend source bitmap open failed");DWORD count=0;const bool wrote=WriteFile(file,bytes.constData(),static_cast<DWORD>(bytes.size()),&count,nullptr)!=0;CloseHandle(file);
        Require(wrote && count==static_cast<DWORD>(bytes.size()),"friend source bitmap write failed");
#else
        std::string relative=name;std::replace(relative.begin(),relative.end(),'\\','/');std::ofstream file(std::filesystem::path(legacy)/relative,std::ios::binary);file.write(bytes.constData(),bytes.size());
#endif
    };
    bmp("basic_interface\\mesbtn_04.bmp",22,20,"#4a83b0");bmp("basic_interface\\mesbtn_04_a.bmp",25,21,"#6aa3d0");bmp("basic_interface\\mesbtn_04_b.bmp",24,23,"#245580");
    bmp("basic_interface\\sys_close_off.bmp",11,10,"#b08040");bmp("basic_interface\\sys_close_on.bmp",13,12,"#80b040");
    bmp("checkbox_1.bmp",9,9,"#2070b0");bmp("checkbox_0.bmp",11,11,"#a0b0c0");
    std::filesystem::current_path(assets);g_readFolderFirst=1;g_codePage=65001;SetRuntimeUiScalePercent(100);
    const auto language=[](bool empty){std::ofstream file("msgstringtable.txt",std::ios::binary);
        const std::map<int,std::string> labels={{355,empty?"":"Whisper preferences <literal>"},{359,empty?"":"Friend private windows"},{361,empty?std::string(120,'W'):"Regular private windows"},{362,"New-window sound"}};
        for(int i=0;i<=362;++i){const auto found=labels.find(i);if(found!=labels.end())file<<found->second;file<<"#\r\n";}
    };language(false);
    g_session.m_isShowWhisperWnd=g_session.m_isShowWhisperWnd_Friend=true;g_session.m_isPlayWhisperOpenSound=false;g_windowMgr.m_friendOptionWindowInfo={90,80,280,120};
    auto* messenger=static_cast<UIMessengerGroupWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_MESSENGERGROUPWND));messenger->Move(430,80);auto* entry=messenger->GetFriendOptionButton();
    Require(entry && entry->m_w==25 && entry->m_h==23,"source messenger button did not use all-state max bitmap dimensions");
    const auto pointer=[&](UINT message,int x,int y){WindowProc(g_hMainWnd,message,message==WM_LBUTTONUP?0:MK_LBUTTON,MAKELPARAM(UiScaleLogicalToRawCoordinate(x),UiScaleLogicalToRawCoordinate(y)));};
    const auto down=[&](UIWindow* window){pointer(WM_LBUTTONDOWN,window->m_x+2,window->m_y+2);};
    const auto up=[&](UIWindow* window){pointer(WM_LBUTTONUP,window->m_x+2,window->m_y+2);};
    QImage nativeFrame,qtFrame;
    const auto render=[&](){state->setUiScale(GetConfiguredUiScaleFactor());adapter.syncMessenger();adapter.syncChat();Frames(app,view);
        ArgbDibSurface dib;Require(dib.EnsureSize(view.width(),view.height()),"friend native DIB failed");std::fill_n(dib.GetPixels(),view.width()*view.height(),0xff181b20u);
        g_windowMgr.DrawVisibleWindowsToHdc(dib.GetDC(),true);nativeFrame=QImage(reinterpret_cast<uchar*>(dib.GetPixels()),view.width(),view.height(),view.width()*4,QImage::Format_RGB32).copy();
        const double scale=GetConfiguredUiScaleFactor();nativeFrame=nativeFrame.scaled(qRound(nativeFrame.width()*scale),qRound(nativeFrame.height()*scale),Qt::IgnoreAspectRatio,Qt::FastTransformation).copy(0,0,view.width(),view.height());
        qtFrame=view.grabWindow().convertToFormat(QImage::Format_RGB32);
    };
    const auto capture=[&](const QString& name,bool covered=false){render();
        auto* item=Find(view.rootObject(),"sourceMessengerFriendOptions");Require(item && item->isVisible(),"actual QML messenger options entry missing");
        const auto bounds=item->mapRectToScene(item->boundingRect());const double scale=GetConfiguredUiScaleFactor();
        Require(std::abs(bounds.left()-entry->m_x*scale)<0.001 && std::abs(bounds.top()-entry->m_y*scale)<0.001
            && std::abs(bounds.width()-25*scale)<0.001 && std::abs(bounds.height()-23*scale)<0.001,"actual messenger entry geometry differs from source child");
        const auto sample=QPoint(UiScaleLogicalToRawCoordinate(entry->m_x+3),UiScaleLogicalToRawCoordinate(entry->m_y+3));
        const QColor color=entry->GetVisualState()==2?QColor("#245580"):entry->GetVisualState()==1?QColor("#6aa3d0"):QColor("#4a83b0");
        Require(nativeFrame.pixelColor(sample)==color && qtFrame.pixelColor(sample)==color,"actual native/QML source entry bitmap/state differs");
        if(auto* options=g_windowMgr.m_friendOptionWnd){auto* frame=Find(view.rootObject(),"sourceFriendOptions");Require(frame && frame->isVisible(),"actual QML options owner absent");
            const auto rect=frame->mapRectToScene(frame->boundingRect());Require(std::abs(rect.left()-options->m_x*scale)<0.001 && std::abs(rect.top()-options->m_y*scale)<0.001
                && std::abs(rect.width()-280*scale)<0.001 && std::abs(rect.height()-120*scale)<0.001,"actual options restore/scale geometry differs");
            Require(Find(view.rootObject(),"sourceFriendOptionsCaption")->property("text").toString()==QString::fromUtf8(options->GetCaption().c_str()),"actual options caption lookup differs");
            for(int i=0;i<3;++i){const auto* row=options->GetCheckBox(i);auto* check=Find(view.rootObject(),("sourceFriendCheck"+QString::number(i)).toUtf8().constData());
                Require(check && check->width()==row->m_w && check->height()==12,"actual QML checkbox hit rectangle differs");
                if(!covered){const auto point=QPoint(UiScaleLogicalToRawCoordinate(row->m_x+3),UiScaleLogicalToRawCoordinate(row->m_y+3));
                    const QColor expected=row->IsChecked()?QColor("#2070b0"):QColor("#a0b0c0");
                    Require(nativeFrame.pixelColor(point)==expected && qtFrame.pixelColor(point)==expected,"actual checkbox source bitmap/state differs");}
            }
        }
        Require(nativeFrame.save(output+"/"+name+".native.png") && qtFrame.save(output+"/"+name+".qml.png"),"friend source frames save failed");
    };
    capture("friend-options-entry");down(entry);Require(GetCapture()==g_hMainWnd && g_windowMgr.m_captureWindow==entry,"messenger entry lacks real child capture");capture("friend-options-entry-pressed");
    pointer(WM_LBUTTONUP,-20,-20);Require(!g_windowMgr.m_friendOptionWnd && !GetCapture(),"outside messenger entry release opened options");
    down(entry);pointer(WM_MOUSEMOVE,-20,-20);pointer(WM_MOUSEMOVE,entry->m_x+2,entry->m_y+2);up(entry);
    auto* options=g_windowMgr.m_friendOptionWnd;Require(options && !GetCapture() && options->GetCloseButton()->m_w==13 && options->GetCloseButton()->m_h==12,"actual messenger entry/close max-size lifecycle failed");capture("friend-options-open");
    auto* row=options->GetCheckBox(0);down(row);Require(GetCapture()==g_hMainWnd && g_windowMgr.m_captureWindow==row && !g_session.m_isShowWhisperWnd
        && g_session.m_isShowWhisperWnd_Friend,"native down checkbox failed notification/capture or coupled flags");capture("friend-options-down-toggle");
    pointer(WM_LBUTTONUP,-20,-20);Require(!g_session.m_isShowWhisperWnd && !GetCapture(),"outside checkbox release reverted immediate toggle");
    pointer(WM_LBUTTONDOWN,row->m_x+row->m_w-2,row->m_y+4);pointer(WM_LBUTTONUP,row->m_x+row->m_w-2,row->m_y+4);Require(g_session.m_isShowWhisperWnd,"label-width checkbox hit did not toggle");
    const int x=options->m_x,y=options->m_y;pointer(WM_LBUTTONDOWN,x+2,y+2);pointer(WM_MOUSEMOVE,x+37,y+22);pointer(WM_LBUTTONUP,-20,-20);
    Require(options->m_x==x+35 && options->m_y==y+20 && options->GetCheckBox(0)->m_x==options->m_x+6,"native options drag did not move owned child controls");
    pointer(WM_LBUTTONDOWN,options->m_x+2,options->m_y+2);WindowProc(g_hMainWnd,WM_CANCELMODE,0,0);pointer(WM_MOUSEMOVE,x+200,y+200);
    Require(options->m_x==x+35 && options->m_y==y+20 && !GetCapture() && !g_windowMgr.m_captureWindow,"cancelled preferences drag remained active");
    SetRuntimeUiScalePercent(125);capture("friend-options-125");
    options->Move(messenger->m_x+10,messenger->m_y+30);options->SetShow(0);render();
    const QPoint overlap(UiScaleLogicalToRawCoordinate(messenger->m_x+20),UiScaleLogicalToRawCoordinate(messenger->m_y+48));
    const auto nativeParent=nativeFrame.pixelColor(overlap),qtParent=qtFrame.pixelColor(overlap);options->SetShow(1);capture("friend-options-overlap");
    Require(nativeFrame.pixelColor(overlap)==QColor("#f2f2f2") && qtFrame.pixelColor(overlap)==QColor("#f2f2f2"),"new options owner was not above messenger");
    pointer(WM_LBUTTONDOWN,messenger->m_x+8,messenger->m_y+4);pointer(WM_LBUTTONUP,-20,-20);capture("friend-options-parent-raised",true);
    Require(nativeFrame.pixelColor(overlap)==nativeParent && qtFrame.pixelColor(overlap)==qtParent,"raising messenger did not preserve parent/entry/options stacking in both renderers");
    language(true);down(entry);up(entry);Require(!g_windowMgr.m_friendOptionWnd,"source entry failed to close independently owned options");down(entry);up(entry);options=g_windowMgr.m_friendOptionWnd;
    Require(options && options->GetCaption().empty() && options->GetCheckBox(1)->GetLabel().empty() && options->GetCheckBox(1)->m_w==12
        && options->GetCheckBox(0)->m_w==265,"empty/long source labels changed checkbox bounds");capture("friend-options-empty-labels");
    row=options->GetCheckBox(0);const auto clipped=QPoint(UiScaleLogicalToRawCoordinate(options->m_x+273),UiScaleLogicalToRawCoordinate(row->m_y+5));
    Require(nativeFrame.pixelColor(clipped)==QColor("#f2f2f2") && qtFrame.pixelColor(clipped)==QColor("#f2f2f2"),"long checkbox label escaped its source viewport");
    pointer(WM_MOUSEMOVE,row->m_x+2,row->m_y+2);down(row);g_windowMgr.DeleteWindow(options);
    Require(!g_windowMgr.m_friendOptionWnd && !GetCapture() && !g_windowMgr.m_captureWindow,"deleting options left a captured child");
    down(entry);g_windowMgr.DeleteWindow(messenger);pointer(WM_LBUTTONUP,-20,-20);
    Require(!g_windowMgr.m_messengerGroupWnd && !g_windowMgr.m_friendOptionWnd && !GetCapture() && !g_windowMgr.m_captureWindow,"deleting captured messenger entry retained parent/control or activated stale pointer");
    render();Require(!Find(view.rootObject(),"sourceFriendOptions")->isVisible() && !Find(view.rootObject(),"sourceMessengerFriendOptions")->isVisible(),"actual QML retained deleted source options controls");
    QFile result(output+"/friend-options-results.json");Require(result.open(QIODevice::WriteOnly),"friend results open failed");
    result.write(QJsonDocument(QJsonObject{{"native_qml_frames",16},{"actual_messenger168",true},{"child147_down_notification",true},{"max_bitmap_bounds",true},{"native_capture_cancel_release",true},
        {"scale125",true},{"parent_entry_stacking",true},{"empty_long_labels",true},{"captured_child_teardown",true},{"outcome","PASS"}}).toJson());
}
