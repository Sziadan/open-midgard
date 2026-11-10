    {
        const auto oldFocus=g_windowMgr.m_editWindow;
        for(const int id:{84,101}){
            auto* window=static_cast<UIGroundSkillTextWnd*>(g_windowMgr.MakeWindow(id));
            Require(window && window->m_w==280 && window->m_h==120,"source text window geometry missing");
            window->Move(20,20);window->GetEditCtrl()->SetText("Ground skill fixture");
            Require(window->GetEditCtrl()->m_x==42 && window->GetEditCtrl()->m_y==70
                && window->GetEditCtrl()->m_maxchar==78,"source text edit position/limit differs");
            for(int i=0;i<6;++i)Require(window->GetFrameBitmap(i).IsValid(),"source text frame fixture failed raw-key asset lookup");
            ArgbDibSurface dib;Require(dib.EnsureSize(view.width(),view.height()),"ground text DIB failed");
            std::fill_n(dib.GetPixels(),view.width()*view.height(),0xff181b20u);
            g_windowMgr.DrawVisibleWindowsToHdc(dib.GetDC(),true);
            const auto image=QImage(reinterpret_cast<uchar*>(dib.GetPixels()),view.width(),view.height(),view.width()*4,QImage::Format_RGB32).copy(20,20,280,120);
            Require(image.pixelColor(3,3)==QColor("#d24240") && image.pixelColor(3,117)==QColor("#b09060")
                && image.pixelColor(192,99)==QColor("#c0a080"),"source text frame/footer/button placement differs");
            int ink=0;for(int y=51;y<65;++y)for(int x=25;x<200;++x)if(image.pixelColor(x,y).red()<120)++ink;
            Require(ink>30,"ground text edit disappeared in composed Qt/native UI");
            Require(window->GetCaption()=="Talkbox Window" && window->GetPrompt()=="Enter text for ground skill","source text message string IDs differ");
            Require(image.save(output+QString("/ground-text-%1-native.png").arg(id)),"ground text image save failed");
            g_windowMgr.DeleteWindow(window);
        }
        g_windowMgr.m_editWindow=oldFocus;
        QFile proof(output+"/ground-text-window-render-results.json");Require(proof.open(QIODevice::WriteOnly),"ground text proof open failed");
        proof.write(QJsonDocument(QJsonObject{{"outcome","PASS"},{"native_frames",2},{"source_ids_84_101",true},
            {"source_edit_geometry_and_limit",true},{"source_key_frame_footer_button_samples",true},
            {"source_colorchip_acceptance",false},{"skill_tree_window_changed",false}}).toJson(QJsonDocument::Compact));
    }
