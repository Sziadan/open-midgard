    {
        Require(!g_windowMgr.m_commandListWnd && !mode.GetHelpBalloon(),"command replay requires fresh command/help owners");
        struct RestoreCommandWindow {
            std::vector<UIWindow*> original{g_windowMgr.m_children.begin(),g_windowMgr.m_children.end()};
            decltype(g_session.m_cmdOnOffSet) flags=g_session.m_cmdOnOffSet;
            decltype(g_session.m_friendList) friends=g_session.m_friendList;
            decltype(g_session.m_partyList) party=g_session.m_partyList;
            bool master=g_session.m_amIPartyMaster,multi=g_multiLang;int code=g_codePage;
            int miss=g_session.m_bMakeMissEffect,names=g_session.m_isShowNameTypeOld;ServiceType service=g_serviceType;
            ~RestoreCommandWindow(){const std::vector<UIWindow*> windows(g_windowMgr.m_children.begin(),g_windowMgr.m_children.end());
                for(auto* window:windows)if(std::find(original.begin(),original.end(),window)==original.end())g_windowMgr.DeleteWindow(window);
                g_session.m_cmdOnOffSet=std::move(flags);g_session.m_friendList=std::move(friends);g_session.m_partyList=std::move(party);
                g_session.m_amIPartyMaster=master;g_session.m_bMakeMissEffect=miss;g_session.m_isShowNameTypeOld=names;
                g_multiLang=multi;g_codePage=code;g_serviceType=service;}
        } restoreCommandWindow;
        g_serviceType=ServiceAmerica;g_codePage=65001;g_session.m_cmdOnOffSet={{1,134},{0,134}};
        auto* window=dynamic_cast<UICommandListWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_COMMANDLISTWND));
        Require(window && window->m_w==250 && window->m_h==200 && window->GetOffset()==0 && window->GetCurrentItem()==-1,
            "command factory/controller initial disposition changed");
        Require(g_windowMgr.MakeWindow(112)==window && window->m_children.size()==4,"command factory duplicated owner/controls");
        auto children=window->m_children.begin();Require(*children++==window->GetTab() && *children++==window->GetScrollControl()
            && *children++==window->GetButtons()[0] && *children==window->GetButtons()[1],"command source child priority changed");
        Require(window->GetCurItem(124,17)==0 && window->GetCurItem(125,17)==1 && window->GetCurItem(250,17)==2
            && window->GetCurItem(-1,17)==-1 && window->GetCurItem(50,16)==-1 && window->GetCurItem(50,177)==-1,
            "command common source hit bands/width alias changed");
        auto rows=window->GetVisibleRows();Require(rows.size()==16 && rows[0].x==25 && rows[1].x==130 && rows[0].textOffset==25
            && rows[0].hasIndicator && rows[0].on,"command toggle rows/first matching status join changed");
        window->OnMouseHover(window->m_x+50,window->m_y+22);
        Require(window->GetCurrentItem()==0 && mode.GetCursorAction()==2 && mode.GetHelpBalloon()
            && mode.GetHelpBalloon()->GetLines().front()=="HELP-787","command hover did not reach real mode help owner");
        g_windowMgr.OnMouseMove(window->m_x+150,window->m_y+22);Require(window->GetCurrentItem()==1,"moving within the same command owner did not update row hover");
        g_windowMgr.OnMouseMove(window->m_x+50,window->m_y+22);
        {
            struct RestoreMessageFile {~RestoreMessageFile(){std::error_code ec;std::filesystem::rename("msgstringtable.hover-save","msgstringtable.txt",ec);}} restoreMessageFile;
            std::filesystem::rename("msgstringtable.txt","msgstringtable.hover-save");
            const u32 previous=mode.GetHelpBalloonTick();auto* previousBalloon=mode.GetHelpBalloon();Sleep(20);
            g_windowMgr.ProcessSourceHoverFrame(window->m_x+50,window->m_y+22);
            Require(mode.GetHelpBalloon()==previousBalloon && mode.GetHelpBalloonTick()!=previous
                && mode.GetHelpBalloon()->GetLines().front()=="HELP-787","stationary hover failed to refresh/reuse the source owner or reread stable cached resources");
        }
        g_windowMgr.OnLBtnDown(window->m_x+5,window->m_y+17+45);g_windowMgr.OnLBtnUp(window->m_x+5,window->m_y+17+45);
        Require(window->GetTab()->m_state==1 && window->GetCurrentItem()==0 && window->GetOffset()==0,"tab child changed preserved selection/offset");
        rows=window->GetVisibleRows();Require(rows[0].textOffset==15,"action caption offset changed");
        window->OnLBtnDown(window->m_x+150,window->m_y+62);Require(peer.ReceiveExact(2)==std::vector<u8>({0xc1,0}),"actual command WHO row did not reach source recipient34");
        g_session.m_friendList.clear();g_windowMgr.ClearChatEvents();window->OnLBtnDown(window->m_x+150,window->m_y+22);
        Require(!g_windowMgr.GetChatEvents().empty() && g_windowMgr.GetChatEvents().back().text=="[ To Friends ] : (Hi) *^_^*","command Hi150 empty typed parameters/default greeting changed");
        Require(window->SendMsg(nullptr,7,4,0,0)==4,"command common scroll maximum changed");
        const auto actionHoleCount=g_windowMgr.GetChatEvents().size();
        window->OnLBtnDown(window->m_x+50,window->m_y+157);
        Require(window->GetCurrentItem()==22 && g_windowMgr.GetChatEvents().size()==actionHoleCount,"action-table22 out-of-range disposition changed source common selection");
        Require(SelectSocket(peer.peer.value,true,false,std::chrono::milliseconds(20))==0,"action hole activated a padded command");
        window->GetTab()->OnLBtnDown(window->m_x+5,window->m_y+17+90);
        Require(window->GetTab()->m_state==2 && window->GetOffset()==4 && window->GetScrollBar().total==12
            && window->GetVisibleRows().size()==9,"window tab changed source23-count scroll state");
        const auto count=g_windowMgr.m_children.size();window->OnLBtnDown(window->m_x+150,window->m_y+97);
        Require(window->GetCurrentItem()==17 && g_windowMgr.m_children.size()==count,"undefined selected-table access disposition changed common index or activated a fake row");
        window->OnWheel(0);Require(window->GetOffset()==1 && window->SendMsg(nullptr,9,0,0,0)==-1,"command wheel zero/page-up semantics changed");
        Require(window->SendMsg(nullptr,10,0,0,0)==4,"command page-down actual movement changed");window->m_isDirty=0;
        Require(window->SendMsg(nullptr,10,0,0,0)==0 && window->m_isDirty,"clamped changed raw scroll lost invalidation");
        Require(window->SendMsg(nullptr,7,INT_MAX,0,0)==-4 && window->GetOffset()==0,"command ADD32 overflow did not signed-clamp");
        const auto hoverType=[&](int type){window->GetTab()->SendMsg(nullptr,13,0,0,0);int index=-1;
            for(std::size_t i=0;i<std::size(ro::chat::SourceCommandToggles);++i)if(ro::chat::SourceCommandToggles[i].value==type)index=static_cast<int>(i);
            Require(index>=0,"hover fixture type absent");const int offset=(std::max)(0,index/2-7);
            window->SendMsg(nullptr,7,offset-window->GetOffset(),0,0);window->OnMouseHover(window->m_x+(index%2?150:50),window->m_y+22+20*(index/2-offset));};
        g_session.m_bMakeMissEffect=1;hoverType(53);Require(mode.GetHelpBalloon()->GetLines().front()=="T317","MISS true hover help changed");
        g_session.m_bMakeMissEffect=0;hoverType(53);Require(mode.GetHelpBalloon()->GetLines().front()=="T318","MISS false hover help changed");
        g_session.m_isShowNameTypeOld=1;hoverType(107);Require(mode.GetHelpBalloon()->GetLines().front()=="T722","SHOW NAME true hover help changed");
        g_session.m_isShowNameTypeOld=0;hoverType(107);Require(mode.GetHelpBalloon()->GetLines().front()=="T723","SHOW NAME false hover help changed");
        hoverType(137);Require(mode.GetHelpBalloon()->GetLines().front()=="/set2 : /set1 + /q3","America SET2 literal hover changed");
        g_serviceType=ServiceKorea;hoverType(137);Require(mode.GetHelpBalloon()->GetLines().front()=="HELP-735","non-America SET2 MsgStr changed");
        window->GetTab()->SendMsg(nullptr,13,1,0,0);window->SendMsg(nullptr,7,-window->GetOffset(),0,0);const auto oldHelp=mode.GetHelpBalloon()->GetLines();
        window->OnMouseHover(window->m_x+50,window->m_y+22);Require(mode.GetHelpBalloon()->GetLines()==oldHelp,"TAEKWON no-help unexpectedly replaced balloon");
        window->GetTab()->SendMsg(nullptr,13,2,0,0);window->OnMouseHover(window->m_x+50,window->m_y+22);
        Require(mode.GetHelpBalloon()->GetLines().front()=="Alt + C","window shortcut hover spacing changed");
        auto* close=window->GetButtons()[1];g_windowMgr.ProcessSourceHoverFrame(close->m_x+1,close->m_y+1);
        Require(mode.GetHelpBalloon()->GetLines().front()=="Alt+Y" && mode.GetHelpBalloon()->m_x==close->m_x
            && mode.GetHelpBalloon()->m_y==close->m_y-20,"source button tooltip anchor/text changed");
        auto* basic=g_windowMgr.m_basicInfoWnd;if(!basic)basic=static_cast<UIBasicInfoWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_BASICINFOWND));
        Require(basic,"command recipient basic-info fixture absent");
        for(int key:{67,73,90,72,77,76})Require(!g_windowMgr.ProcessSourceCommandWindowKey(key),"selected default window key acquired an action");
        FRIEND_INFO member{};member.isValid=true;member.characterName="CommandParty";g_session.m_partyList={member};g_session.m_amIPartyMaster=false;
        Require(g_windowMgr.ProcessSourceCommandWindowKey(80) && g_windowMgr.m_partyOptionWnd && !g_windowMgr.m_partyOptionWnd->CanEditSettings(),"party command did not open source configure/authority state");
        UIPartyOptionWnd::DisplayData partyData;g_windowMgr.m_partyOptionWnd->GetDisplayDataForQt(&partyData);
        Require(std::all_of(partyData.choices.begin(),partyData.choices.end(),[](const auto& choice){return !choice.enabled;}),"non-master party choices stayed enabled");
        const auto deniedChoice=partyData.choices.front();g_windowMgr.ClearChatEvents();
        g_windowMgr.m_partyOptionWnd->OnLBtnDown(deniedChoice.x+1,deniedChoice.y+1);g_windowMgr.m_partyOptionWnd->OnLBtnUp(deniedChoice.x+1,deniedChoice.y+1);
        Require(!g_windowMgr.GetChatEvents().empty() && g_windowMgr.GetChatEvents().back().text=="ONLY PARTY MASTER","non-master radio did not retain source294 feedback");
        g_windowMgr.m_partyOptionWnd->OnKeyDown(VK_RETURN);Require(!g_windowMgr.m_partyOptionWnd,"non-master confirm did not retire source party owner");
        Require(SelectSocket(peer.peer.value,true,false,std::chrono::milliseconds(20))==0,"non-master party route emitted an option packet");
        Require(g_windowMgr.ProcessSourceCommandWindowKey(80) && g_windowMgr.m_partyOptionWnd,"party settings did not reopen after source confirm");
        g_session.m_partyList.clear();Require(g_windowMgr.ProcessSourceCommandWindowKey(80) && !g_windowMgr.m_partyOptionWnd,"party deletion must precede current party-size guard");
        window->SendMsg(nullptr,6,134,0,0);window->OnKeyDown(VK_RETURN);window->OnKeyDown(VK_ESCAPE);
        Require(g_windowMgr.m_commandListWnd==window,"base/default key IDs gained command close behavior");
        g_windowMgr.m_captureWindow=window->GetScrollControl();window->SendMsg(nullptr,6,135,0,0);
        Require(!g_windowMgr.m_commandListWnd && !g_windowMgr.m_captureWindow && mode.GetHelpBalloon(),"command close/capture/help ownership changed");
        g_windowMgr.DeleteWindow(mode.GetHelpBalloon());Require(!mode.GetHelpBalloon(),"external help deletion left stale mode pointer");
        struct TickMode : CMode {void Tick(u32 value){m_helpBalloonTick=value;}} helpMode;
        helpMode.MakeHelpBalloon("word_word",-500,INT_MAX,0x123456);auto* balloon=helpMode.GetHelpBalloon();const u32 tick=helpMode.GetHelpBalloonTick();
        Require(balloon && balloon->GetLines().front()=="word word" && balloon->GetFontColor()==0x563412 && balloon->m_x==-3
            && !balloon->ShouldDoHitTest(),"source balloon sanitize/color/clamp/hit behavior changed");
        helpMode.ProcessHelpBalloon(tick+99);Require(helpMode.GetHelpBalloon()==balloon,"help expired before100 ticks");
        helpMode.ProcessHelpBalloon(tick+100);Require(!helpMode.GetHelpBalloon() && std::find(g_windowMgr.m_children.begin(),g_windowMgr.m_children.end(),balloon)!=g_windowMgr.m_children.end(),"help PostQuit must clear owner before deferred deletion");
        helpMode.MakeHelpBalloon("replacement",0,0,0xffffff);auto* replacement=helpMode.GetHelpBalloon();Require(replacement!=balloon,"queued balloon was reused prematurely");
        g_windowMgr.OnProcess();Require(helpMode.GetHelpBalloon()==replacement && g_windowMgr.m_children.back()==replacement,"old deferred help deletion affected replacement/top layer");
        replacement->SetText(("|00"+std::string(40,'A')).c_str(),35);g_multiLang=true;replacement->SetText(("|00"+std::string(40,'A')).c_str(),35);
        Require(replacement->GetLines().size()==2 && replacement->GetLines()[0].size()==35 && replacement->GetLines()[1]=="|00"+std::string(8,'A'),"balloon byte threshold/charset carry changed");
        const auto saved=replacement->GetLines();replacement->SetText("",35);Require(replacement->GetLines()==saved,"empty SetText cleared source retained text");
        g_codePage=949;Require(UITransBalloonText::ReplaceUnderbars("\x81\x5f_")==std::string("\x81\x5f ",3),"underbar conversion split a source code-page character");
        replacement->SetText((std::string(34,'A')+"\x81\x41Z").c_str(),35);Require(replacement->GetLines().front().size()==36,"balloon split a multibyte character at35 bytes");
        helpMode.Tick(0xfffffff0u);helpMode.ProcessHelpBalloon(0xfffffff0u);Require(!helpMode.GetHelpBalloon(),"help unsigned deadline wrap was changed to elapsed-time subtraction");g_windowMgr.OnProcess();
    }
