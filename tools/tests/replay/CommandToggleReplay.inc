    {
        // Independent selected-type specification; exercise production47 rather than a test dispatcher.
        struct Spec {int type;int* field;int on,off;bool inverted;};
        const Spec specs[]={
            {53,&g_session.m_bMakeMissEffect,317,318,false},{105,&g_session.m_isNoShift,701,702,false},
            {106,&g_session.m_isAura,711,712,false},{108,&g_session.m_isNoCtrl,717,718,false},
            {112,&g_session.m_isSkillFail,730,731,false},{113,&g_session.m_isNoTalkMsg,733,734,true},
            {119,&g_session.m_isQuickSpell,765,766,false},{120,&g_session.m_isQuickSpell2,768,769,false},
            {134,&g_session.m_isBattle,785,786,false},{135,&g_session.m_isNoTrade,805,806,false},
            {140,&g_session.m_isWindow,850,851,false},{147,&g_session.m_isNoTalkMsg2,908,909,true},
            {164,&g_session.m_isLogInOut,1044,1045,false},{166,&g_session.m_isShopping,1108,1109,false}};
        struct RestoreToggles {
            std::vector<std::pair<int*,int>> fields;
            int sound=g_isSoundOn,driver=g_soundMode,name=g_session.m_isShowNameTypeOld;
            u8 shorten=NameBalloonShorten,bold=NameBalloonfontBold;
            bool fog=g_session.m_fogOn,paused=CAudio::GetInstance()->IsBgmPaused();
            bool vertex=g_renderer.m_isVertexFog,foggy=g_renderer.m_isFoggy,changed=g_renderer.m_fogChanged;
            FogPara para=g_renderer.m_fogPara;
            ServiceType service=g_serviceType;
            std::string selected;CGameMode& mode;UIOptionWnd* owned=nullptr;
            RestoreToggles(CGameMode& value):selected(value.m_streamFileName),mode(value){}
            ~RestoreToggles(){if(owned)g_windowMgr.DeleteWindow(owned);
                for(const auto& value:fields)*value.first=value.second;
                g_isSoundOn=sound;g_soundMode=driver;g_session.m_isShowNameTypeOld=name;
                NameBalloonShorten=shorten;NameBalloonfontBold=bold;g_session.m_fogOn=fog;
                g_renderer.m_isVertexFog=vertex;g_renderer.m_isFoggy=foggy;g_renderer.m_fogChanged=changed;g_renderer.m_fogPara=para;
                g_serviceType=service;mode.m_streamFileName=selected;CAudio::GetInstance()->SetBgmPaused(paused);}
        } restoreToggles(mode);
        for(const auto& spec:specs)restoreToggles.fields.push_back({spec.field,*spec.field});
        const auto notice=[&](const std::string& text){const auto& events=g_windowMgr.GetChatEvents();
            Require(!events.empty() && events.back().text==text && events.back().color==0xffff64u,"toggle notice text/color differs");};
        const auto label=[](int id){return id==717?std::string():"T"+std::to_string(id);};
        const auto hidden=[&](){g_windowMgr.ShowChatWindow(false);};
        g_windowMgr.SetChatMsg("toggle retained buffer");
        for(const auto& spec:specs){
            for(int initial:{0,1,4,INT_MIN}){
                *spec.field=initial;g_session.m_cmdOnOffSet={{7,spec.type},{9,spec.type}};hidden();
                const auto before=g_windowMgr.GetChatEvents().size();
                Require(mode.SendMsg(47,spec.type,0,0)!=0,"scalar toggle was not handled");
                const long long mathematical=1ll-initial;
                const int expected=mathematical>INT_MAX?INT_MIN+1:static_cast<int>(mathematical);
                Require(*spec.field==expected && g_session.m_cmdOnOffSet[0].first==0 && g_session.m_cmdOnOffSet[1].first==9,
                    "toggle raw integer wrap or first-match prelude changed");
                const auto expectedText=label((expected!=0)!=spec.inverted?spec.on:spec.off);
                Require(g_windowMgr.m_chatWndShow && g_windowMgr.GetChatEvents().size()==before+(expectedText.empty()?0:1),"toggle show/notice or known-empty resource differs");
                if(!expectedText.empty())notice(expectedText);
                Require(std::string(g_windowMgr.GetChatMsg())=="toggle retained buffer","toggle cleared source manager buffer");
            }
        }
        // SetTextType writes raw bytes only in four source-selected logical services.
        for(ServiceType service:{ServiceKorea,ServiceIndonesia,ServiceGermany,ServiceBrazil,ServiceAmerica,ServiceVietnam}){
            g_serviceType=service;NameBalloonShorten=2;NameBalloonfontBold=3;g_session.SetTextType(7,9);
            const bool allowed=service==ServiceKorea || service==ServiceIndonesia || service==ServiceGermany || service==ServiceBrazil;
            Require(NameBalloonShorten==(allowed?7:2) && NameBalloonfontBold==(allowed?9:3),"SetTextType locale/raw byte gate differs");
            g_session.m_isShowNameTypeOld=-4;
            Require(submit("/showname 123") && g_session.m_isShowNameTypeOld==0,"showname parsed a setter instead of toggling old==0");notice("T723");
            Require(NameBalloonShorten==(allowed?0:2) && NameBalloonfontBold==(allowed?0:3),"showname failed paired locale-gated writes");
            Require(submit("/showname ignored") && g_session.m_isShowNameTypeOld==1,"showname second toggle failed");notice("T722");
        }
        g_serviceType=ServiceAmerica;
        for(int composite:{114,121,137}){
            for(const auto& spec:specs)*spec.field=0;g_session.m_isShowNameTypeOld=0;
            g_session.m_cmdOnOffSet={{0,composite},{0,112},{0,107},{0,108},{0,119},{0,120},{0,140}};
            const auto before=g_windowMgr.GetChatEvents().size();
            Require(mode.SendMsg(47,composite,0,0)!=0,"composite dispatch failed");
            const std::vector<std::string> expected=composite==114?std::vector<std::string>{"T730","T722","T850"}
                :composite==121?std::vector<std::string>{"T765","T768"}:std::vector<std::string>{"T730","T722","T765","T768","T850"};
            const auto& events=g_windowMgr.GetChatEvents();Require(events.size()==before+expected.size(),"composite emitted extra/missing notices");
            for(size_t i=0;i<expected.size();++i)Require(events[before+i].text==expected[i],"composite child47 order changed");
            Require(g_session.m_cmdOnOffSet.front().first==1,"composite prelude was skipped");
            for(size_t i=1;i<g_session.m_cmdOnOffSet.size();++i){const int child=g_session.m_cmdOnOffSet[i].second;
                const bool expectedFlag=composite==121?(child==119 || child==120):composite==114?(child!=119 && child!=120):true;
                Require(g_session.m_cmdOnOffSet[i].first==int(expectedFlag),"composite child did not traverse47/prelude");}
        }
        // Construct real options after deliberately stale persisted preferences.
        SaveSettingsIniInt("OptionWnd","SoundOn",0);SaveSettingsIniInt("OptionWnd","BgmOn",0);
        SaveSettingsIniInt("OptionWnd","NoCtrl",0);SaveSettingsIniInt("OptionWnd","FogOn",0);
        g_isSoundOn=1;g_soundMode=2;g_session.m_isNoCtrl=1;g_session.m_fogOn=true;
        CAudio::GetInstance()->SetBgmPaused(false);
        auto* option=static_cast<UIOptionWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_OPTIONWND));restoreToggles.owned=option;
        Require(option && g_isSoundOn==1 && g_soundMode==2 && !CAudio::GetInstance()->IsBgmPaused() && g_session.m_fogOn,
            "opening options overwrote live commands with old preferences");
        UIOptionWnd::DisplayData display;option->GetDisplayDataForQt(&display);
        const auto tab=[&](int index){option->GetDisplayDataForQt(&display);const auto rect=display.tabs[index];
            option->OnLBtnDown(rect.x+rect.width/2,rect.y+rect.height/2);option->GetDisplayDataForQt(&display);};
        tab(2);Require(display.toggles.size()==2 && !display.toggles[0].checked && !display.toggles[1].checked,"options did not snapshot live audio");
        SaveSettingsIniInt("OptionWnd","SoundOn",7);
        hidden();Require(mode.SendMsg(47,11,0,0) && !g_isSoundOn && g_soundMode==2 && !g_windowMgr.m_chatWndShow,"sound toggle changed driver mode or showed chat");notice("T28");
        option->GetDisplayDataForQt(&display);Require(display.toggles[1].checked && LoadSettingsIniInt("OptionWnd","SoundOn",-1)==7,"option60 mutated/saved instead of refreshing");
        hidden();Require(mode.SendMsg(47,11,0,0) && g_isSoundOn==1 && !g_windowMgr.m_chatWndShow,"sound on path differs");notice("T27");
        g_soundMode=0;Require(mode.SendMsg(47,11,0,0) && g_soundMode==0,"sound command rewrote explicitly disabled driver mode");g_soundMode=2;
        g_isSoundOn=-4;hidden();Require(mode.SendMsg(47,11,0,0) && g_isSoundOn==5 && !g_windowMgr.m_chatWndShow,"sound raw integer toggle was booleanized");
        g_isSoundOn=0;option->SendMsg(nullptr,60,0,0,0);
        mode.m_streamFileName="command-toggle-selected.ogg";hidden();Require(mode.SendMsg(47,7,0,0) && CAudio::GetInstance()->IsBgmPaused() && g_windowMgr.m_chatWndShow
            && mode.m_streamFileName=="command-toggle-selected.ogg","BGM off show/pause or selected-track retention failed");notice("T31");
        option->GetDisplayDataForQt(&display);Require(display.toggles[0].checked && CAudio::GetInstance()->GetCurrentBgmPath().empty(),"BGM close/option refresh missing");
        mode.m_streamFileName.clear();Require(mode.SendMsg(47,7,0,0) && !CAudio::GetInstance()->IsBgmPaused(),"BGM on state failed for empty selected track");notice("T32");
        Require(submit("/v 23") && submit("/bv 37"),"volume commands failed with options open");option->GetDisplayDataForQt(&display);
        Require(display.sliders[0].value==37 && display.sliders[1].value==23,"volume command did not refresh option sliders");
        // Real slider input must retain mute and driver state from command refresh.
        const auto slider=display.sliders[1];option->OnLBtnDown(slider.x+slider.width/2,slider.y+slider.height/2);option->OnLBtnUp(slider.x,slider.y);
        Require(g_isSoundOn==0 && g_soundMode==2 && !CAudio::GetInstance()->IsBgmPaused(),"audio UI application reverted command flags/driver");
        tab(0);g_session.m_isNoCtrl=0;option->SendMsg(nullptr,60,0,0,0);Require(mode.SendMsg(47,108,0,0),"noctrl dispatch failed");
        option->GetDisplayDataForQt(&display);Require(display.toggles[0].checked && display.toggles[0].label=="Auto attack without Ctrl","noctrl live view/meaning differs");
        const auto noCtrl=display.toggles[0];option->OnLBtnDown(noCtrl.x+2,noCtrl.y+2);
        Require(g_session.m_isNoCtrl==0 && LoadSettingsIniInt("Option","m_isNoCtrl",-1)==0,"noctrl UI did not change/persist canonical source owner");
        tab(1);hidden();Require(mode.SendMsg(47,60,0,0) && !g_session.m_fogOn && !g_renderer.m_isFoggy && !g_windowMgr.m_chatWndShow,"fog off side effect/show differs");notice("Fog Off");
        option->GetDisplayDataForQt(&display);Require(display.toggles.size()==1 && !display.toggles[0].checked,"fog command did not refresh live view");
        hidden();Require(mode.SendMsg(47,60,0,0) && g_session.m_fogOn && !g_windowMgr.m_chatWndShow,"fog on command failed");notice("Fog On");
        option->GetDisplayDataForQt(&display);const auto fog=display.toggles[0];Require(fog.checked,"fog on view stale");
        option->OnLBtnDown(fog.x+2,fog.y+2);Require(!g_session.m_fogOn && LoadSettingsIniInt("OptionWnd","FogOn",-1)==0,"fog UI did not share/persist canonical session owner");
        // Native checkbox notifications use the same canonical owners as passive Qt coordinates.
        if(!IsQtUiRuntimeEnabled()){
        option->OnCreate(1280,720);
        const auto native=[&](int id,int check){UIWindow* sender=nullptr;for(auto* child:option->m_children)if(child->m_id==id)sender=child;
            Require(sender && option->SendMsg(sender,6,0,check,0),"native checkbox sender dispatch failed");};
        native(403,1);native(407,1);native(402,0);
        Require(g_session.m_isNoCtrl==1 && g_session.m_fogOn && g_isSoundOn==1 && g_soundMode==2
            && LoadSettingsIniInt("Option","m_isNoCtrl",-1)==1 && LoadSettingsIniInt("OptionWnd","FogOn",-1)==1,
            "native checkbox owner/persistence diverged from command/Qt path");
        native(402,1);Require(g_isSoundOn==0 && g_soundMode==2,"native mute disabled audio driver");
        }
        std::cout<<"{\"case_id\":\"source_command_toggles\",\"contract_id\":\"PK09.21\",\"scalar_wrap\":true,\"composite47\":true,\"locale_text_flags\":true,\"live_options\":true,\"outcome\":\"PASS\"}\n";
    }
