    {
        struct RestoreTipWindow {
            CTipOfTheDay data=g_tipOfTheDay;int startup=g_showTipsAtStartup,code=g_codePage;ServiceType service=g_serviceType;
            bool checked=g_windowMgr.m_tipStartupChecked;
            ~RestoreTipWindow(){if(g_windowMgr.m_tipOfTheDayWnd)g_windowMgr.DeleteWindow(g_windowMgr.m_tipOfTheDayWnd);
                g_tipOfTheDay=std::move(data);g_showTipsAtStartup=startup;g_serviceType=service;g_codePage=code;g_windowMgr.m_tipStartupChecked=checked;}
        } restoreTipWindow;
        g_codePage=65001;g_serviceType=ServiceKorea;
        std::string tip="^FF0000RED ROW\n";for(int i=0;i<14;++i)tip+="line"+std::to_string(i)+"\n";tip+="#SECOND TIP#";
        {std::ofstream file("tipOfTheday.txt",std::ios::binary);file<<tip;}
        g_tipOfTheDay=CTipOfTheDay{};Require(g_tipOfTheDay.Load(9) && g_tipOfTheDay.GetNumTip()==2,"Tip UI source data setup failed");
        UITipTextViewer wrapped;wrapped.SetLines({"","^FF0000"+std::string(38,'A')+"Z","carry","^00FF00green"});
        Require(wrapped.GetItems().size()==4 && wrapped.GetRows()[0][0].text==std::string(38,'A') && wrapped.GetRows()[1][0].text=="Z"
            && wrapped.GetRows()[2][0].color==0xff0000 && wrapped.GetRows()[3][0].color==0x00ff00,"Tip viewer wrap/empty/color carry differs");
        wrapped.SetLines({"one "+std::string(40,'X')});Require(wrapped.GetItems()==std::vector<std::string>{"one ",std::string(38,'X'),"XX"},"Tip wrapping did not prefer previous space then hard-split");
        wrapped.SetLines({"a","b"});Require(wrapped.SendMsg(nullptr,7,1,0,0)==1 && wrapped.GetOffset()==1 && wrapped.GetScrollBar().maximum==0,"direct short-list source scroll lost unsigned upper-bound behavior");
        Require(wrapped.SendMsg(nullptr,18,0,0,0)==0 && wrapped.GetOffset()==1,"hidden scrollbar message18 must do nothing and return0");wrapped.OnWheel(-120);
        Require(wrapped.GetOffset()==1,"hidden scrollbar wheel must not move direct-message state");wrapped.SendMsg(nullptr,9,0,0,0);wrapped.SendMsg(nullptr,10,0,0,0);
        Require(wrapped.GetOffset()==5,"direct short-list page message was normalized to a hidden-bar clamp");
        g_codePage=1252;wrapped.SetLines({std::string("caf")+char(0xe9)});Require(wrapped.GetRows()[0][0].text==u8"café","Tip raw resource display did not decode selected codepage");g_codePage=65001;
        g_windowMgr.m_tipStartupChecked=false;g_showTipsAtStartup=0;
        Require(!g_windowMgr.OpenSourceStartupTip() && g_windowMgr.m_tipStartupChecked && !g_windowMgr.m_tipOfTheDayWnd,"startup OFF/once guard differs");
        g_showTipsAtStartup=1;Require(!g_windowMgr.OpenSourceStartupTip(),"changing flag bypassed already-consumed startup guard");
        g_windowMgr.m_tipStartupChecked=false;Require(g_windowMgr.OpenSourceStartupTip(),"first enabled startup did not create real Tip window");
        auto* window=dynamic_cast<UITipOfTheDayWnd*>(g_windowMgr.m_tipOfTheDayWnd);
        const auto client=g_windowMgr.GetChatClientRect();Require(window && window->m_w==280 && window->m_h==180 && window->m_x==(client.right-640)/2+200
            && window->m_y==120*client.bottom/480 && window->GetViewer()->m_w==260 && window->GetViewer()->m_h==125,"real Tip factory/viewer geometry differs");
        Require(g_windowMgr.MakeWindow(38)==window && std::count(g_windowMgr.m_children.begin(),g_windowMgr.m_children.end(),window)==1,"Tip factory duplicated retained owner");
        for(int i=0;i<3;++i){auto* button=window->GetButtons()[i];Require(button->m_id==(i==0?180:i==1?152:135) && button->m_x==window->m_x+144+45*i
            && button->m_y==window->m_y+156,"Tip source button IDs/positions differ");}
        const auto click=[&](UIWindow* child){const int x=child->m_x+2,y=child->m_y+2;g_windowMgr.OnLBtnDown(x,y);g_windowMgr.OnLBtnUp(x,y);};
        if(window->GetTipIndex()!=0)window->SendMsg(nullptr,6,180,0,0);
        auto* viewer=window->GetViewer();Require(viewer->GetItems().front()=="Did you know?" && viewer->GetScrollBar().page==6 && viewer->GetScrollBar().maximum>6,"Tip initial heading/row/page differs");
        viewer->SendMsg(nullptr,10,0,0,0);Require(viewer->GetOffset()==5,"Tip page down must move5 rows");
        Require(viewer->GetRows()[viewer->GetOffset()][0].color==0xff0000,"Tip scrolled rows lost prior color state");
        viewer->SendMsg(nullptr,7,INT_MAX,0,0);Require(viewer->GetOffset()==0,"Tip ADD32 overflow did not pass through signed lower clamp");
        viewer->SendMsg(nullptr,7,100000,0,0);Require(viewer->GetOffset()==viewer->GetScrollBar().maximum,"Tip scroll upper clamp differs");
        viewer->SendMsg(nullptr,9,0,0,0);const auto offset=viewer->GetOffset();viewer->OnWheel(120);Require(viewer->GetOffset()==(std::max)(0,offset-3),"Tip wheel must move3 rows");
        const CTipOfTheDay::Tip supplied{"Source message29 replacement"};window->SendMsg(nullptr,29,reinterpret_cast<msgparam_t>(&supplied),0,0);
        Require(window->GetTipIndex()==0 && viewer->GetOffset()==0 && viewer->GetItems()==std::vector<std::string>{"Did you know?","Source message29 replacement"},"Tip message29 did not replace rows independently from selection");
        click(window->GetButtons()[1]);Require(window->GetTipIndex()==1 && viewer->GetOffset()==0 && viewer->GetItems()==std::vector<std::string>{"Did you know?","SECOND TIP"},"Tip next did not rebuild/reset viewer");
        click(window->GetButtons()[1]);Require(window->GetTipIndex()==0,"Tip next did not wrap");
        Require(g_windowMgr.OnQtKeyDown(VK_RETURN,false,false,false) && window->GetTipIndex()==1,"Tip selected frame default180 did not route Enter to Back");
        Require(g_windowMgr.OnQtKeyDown(VK_ESCAPE,false,false,false) && g_windowMgr.m_tipOfTheDayWnd==window,"Tip default cancel117 must not invent a close action");
        g_windowMgr.ShowChatWindow(true);chat->Move(0,0);g_windowMgr.OnLBtnDown(20,2);g_windowMgr.OnLBtnUp(20,2);
        Require(g_windowMgr.m_editWindow!=window,"clicking main chat left the Tip frame consuming keyboard input");
        auto* check=window->GetStartupCheckBox();const bool before=check->IsChecked();g_windowMgr.OnLBtnDown(check->m_x+2,check->m_y+2);
        Require(check->IsChecked()!=before && g_showTipsAtStartup==int(!before) && LoadSettingsIniInt("Option","SHOWTIPSATSTARTUP",-1)==int(!before),"Tip checkbox147 did not persist on down");
        g_windowMgr.OnLBtnUp(-10,-10);const auto events=g_windowMgr.GetChatEvents().size();g_windowMgr.SetChatMsg("Tip retained buffer");g_session.m_cmdOnOffSet={{0,42},{9,42}};
        Require(submit("/tip") && !g_windowMgr.m_tipOfTheDayWnd && g_session.m_cmdOnOffSet[0].first==1 && g_session.m_cmdOnOffSet[1].first==9
            && std::string(g_windowMgr.GetChatMsg())=="Tip retained buffer" && g_windowMgr.GetChatEvents().size()==events,"command42 close/prelude/buffer/no-notice differs");
        Require(!g_windowMgr.OpenSourceStartupTip(),"deleting Tip reset the process startup guard");
        Require(submit("/tip"),"command42 failed to reopen real Tip");window=dynamic_cast<UITipOfTheDayWnd*>(g_windowMgr.m_tipOfTheDayWnd);
        window->SetShow(0);Require(submit("/tip") && !g_windowMgr.m_tipOfTheDayWnd,"command42 must delete an existing hidden Tip");
        g_tipOfTheDay=CTipOfTheDay{};{std::ofstream file("tipOfTheday.txt",std::ios::binary);}
        g_windowMgr.m_tipStartupChecked=false;g_showTipsAtStartup=1;Require(!g_windowMgr.OpenSourceStartupTip() && g_windowMgr.m_tipStartupChecked,"empty Tip source must consume guard without startup window");
        Require(submit("/tip"),"manual empty Tip must still open");window=dynamic_cast<UITipOfTheDayWnd*>(g_windowMgr.m_tipOfTheDayWnd);
        Require(window && window->GetViewer()->GetItems().empty(),"empty Tip window invented heading/content");
        click(window->GetButtons()[0]);click(window->GetButtons()[1]);click(window->GetButtons()[2]);Require(!g_windowMgr.m_tipOfTheDayWnd && !g_windowMgr.m_captureWindow,"Tip empty navigation/actual close left dangling owner/capture");
        std::cout<<"{\"case_id\":\"source_tip_window\",\"contract_id\":\"PK09.21\",\"actual_factory42\":true,\"source_viewer_controls\":true,\"once_startup\":true,\"outcome\":\"PASS\"}\n";
    }
