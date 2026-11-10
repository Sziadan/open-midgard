    {
        const auto oldFocus=g_windowMgr.m_editWindow;
        for(const int id:{84,101}){
            auto* window=static_cast<UIGroundSkillTextWnd*>(g_windowMgr.MakeWindow(id));
            Require(window && window->m_w==280 && window->m_h==120,"source text window geometry missing");
            window->Move(20,20);window->GetEditCtrl()->SetText("Ground skill fixture");
            Require(window->GetEditCtrl()->m_x==42 && window->GetEditCtrl()->m_y==70
                && window->GetEditCtrl()->m_maxchar==78,"source text edit position/limit differs");
            Require(g_windowMgr.IsWhisperPreferenceInputWindow(window) && g_windowMgr.IsWhisperPreferenceInputWindow(window->GetEditCtrl()),
                "ground prompt/child missing native Qt input ownership");
            Require(window->GetColorChip().IsValid() && window->GetColorChipRgb(2,2)==0x123456
                && window->GetColorChipRgb(20,6)==0xa1b2c3 && window->GetColorChipRgb(-1,0)==0xff0000
                && window->GetColorChipRgb(32,0)==0xff0000 && window->GetColorChipRgb(0,8)==0xff0000,
                "selected color-chip coordinates/channel extraction/red bounds differ");
            const auto pointer=[&](UINT message,int x,int y){
                WindowProc(g_hMainWnd,message,message==WM_LBUTTONUP?0:MK_LBUTTON,
                    MAKELPARAM(UiScaleLogicalToRawCoordinate(x),UiScaleLogicalToRawCoordinate(y)));
            };
            pointer(WM_LBUTTONDOWN,30,55);
            Require(window->IsFrameDragging() && g_windowMgr.m_captureWindow==window && window->m_transTarget==128,
                "native frame down failed capture/128 transition target");
            pointer(WM_MOUSEMOVE,70,80);
            Require(window->m_x==60 && window->m_y==45 && window->GetEditCtrl()->m_x==82 && window->GetEditCtrl()->m_y==95,
                "frame move accumulated drift or detached its edit child");
            pointer(WM_MOUSEMOVE,-10,-5);
            Require(window->m_x==-20 && window->m_y==-40,"source unregistered frame drag acquired a client clamp");
            pointer(WM_LBUTTONUP,400,300);
            Require(!window->IsFrameDragging() && !g_windowMgr.m_captureWindow && window->m_transTarget==255,
                "outside frame release failed ownership/255 transition target");
            window->OnMouseMove(100,100);Require(window->m_x==-20 && window->m_y==-40,"uncaptured frame followed pointer");
            window->Move(20,20);
            pointer(WM_LBUTTONDOWN,212,119);pointer(WM_MOUSEMOVE,350,180);pointer(WM_LBUTTONUP,350,180);
            g_windowMgr.OnProcess();Require(g_windowMgr.ContainsWindow(window) && window->GetButtonState(118)!=2,
                "outside button release submitted or left a pressed latch");
            pointer(WM_LBUTTONDOWN,50,75);pointer(WM_LBUTTONUP,50,75);
            WindowProc(g_hMainWnd,WM_CHAR,'!',0);
            Require(std::string(window->GetEditCtrl()->GetText())=="Ground skill fixture!","native edit focus/typing did not reach model");
            for(int i=0;i<6;++i)Require(window->GetFrameBitmap(i).IsValid(),"source text frame fixture failed raw-key asset lookup");
            ArgbDibSurface dib;Require(dib.EnsureSize(view.width(),view.height()),"ground text DIB failed");
            std::fill_n(dib.GetPixels(),view.width()*view.height(),0xff181b20u);
            g_windowMgr.DrawVisibleWindowsToHdc(dib.GetDC(),true);
            const auto image=QImage(reinterpret_cast<uchar*>(dib.GetPixels()),view.width(),view.height(),view.width()*4,QImage::Format_RGB32).copy(20,20,280,120);
            Require(image.pixelColor(3,3)==QColor("#d24240") && image.pixelColor(3,117)==QColor("#b09060")
                && image.pixelColor(192,99)==QColor("#c0a080"),"source text frame/footer/button placement differs");
            Require(image.pixelColor(50,80)==QColor("#123456") && image.pixelColor(0,70)==QColor("#a1b2c3"),"prompt fill/border bypassed color-chip samples");
            int ink=0;for(int y=51;y<65;++y)for(int x=25;x<200;++x)if(image.pixelColor(x,y).red()<120)++ink;
            Require(ink>30,"ground text edit disappeared in composed Qt/native UI");
            Require(window->GetCaption()=="Talkbox Window" && window->GetPrompt()=="Enter text for ground skill","source text message string IDs differ");
            Require(image.save(output+QString("/ground-text-%1-native.png").arg(id)),"ground text image save failed");
            pointer(WM_LBUTTONDOWN,30,55);g_windowMgr.DeleteWindow(window);
            Require(!g_windowMgr.m_captureWindow && GetCapture()!=g_hMainWnd,"owner deletion retained native capture");
        }
        g_windowMgr.m_editWindow=oldFocus;
        QFile proof(output+"/ground-text-window-render-results.json");Require(proof.open(QIODevice::WriteOnly),"ground text proof open failed");
        proof.write(QJsonDocument(QJsonObject{{"outcome","PASS"},{"native_frames",2},{"source_ids_84_101",true},
            {"source_edit_geometry_and_limit",true},{"source_key_frame_footer_button_samples",true},
            {"valid_colorchip_sample_and_paint",true},{"native_windowproc_input",true},{"unregistered_frame_drag",true},
            {"pointer_capture_and_outside_release",true},{"custom_skin_resource_acceptance",false},{"opacity_compositor_acceptance",false},
            {"native_keyboard_acceptance",false},{"source_colorchip_acceptance",false},{"skill_tree_window_changed",false}}).toJson(QJsonDocument::Compact));
    }
