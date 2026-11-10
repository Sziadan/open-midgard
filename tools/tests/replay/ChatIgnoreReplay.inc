    // Included in the managed source-submission fixture after its socket is disconnected.
    {
        struct RestoreIgnore {
            CGameMode& mode;
            int waiting=mode.m_isWaitingWhisperSetting;
            std::string name=mode.m_refuseWhisperName;
            decltype(g_session.m_exNameList) names=g_session.m_exNameList;
            decltype(g_session.m_exAidList) aids=g_session.m_exAidList;
            decltype(g_session.m_cmdOnOffSet) flags=g_session.m_cmdOnOffSet;
            ~RestoreIgnore(){mode.m_isWaitingWhisperSetting=waiting;mode.m_refuseWhisperName=name;
                g_session.m_exNameList=std::move(names);g_session.m_exAidList=std::move(aids);g_session.m_cmdOnOffSet=std::move(flags);}
        } restoreIgnore{mode};
        Require(ro::net::GetPacketSize(0xcf)==27 && ro::net::GetPacketSize(0xd0)==3
            && ro::net::GetPacketSize(0xd1)==4 && ro::net::GetPacketSize(0xd2)==4
            && ro::net::GetPacketSize(0xd3)==2 && ro::net::GetPacketSize(0xd4)==-1,"selected ignore packet registry differs");
        g_session.m_exNameList={"LocalOnly"};g_session.m_exAidList={99};
        mode.m_isWaitingWhisperSetting=0;mode.m_refuseWhisperName="old";
        Require(!submit("/ex") && !mode.m_isWaitingWhisperSetting,"failed list request changed source pending state");
        Require(!submit("/exall") && mode.m_isWaitingWhisperSetting==1 && g_session.IsExName("LocalOnly")
            && !g_session.m_exAidList.empty(),"failed deny-all cleared exclusions or rolled back waiting");
        Require(!submit("/inall") && mode.m_isWaitingWhisperSetting==1 && g_session.m_exNameList.empty() && g_session.m_exAidList.empty(),
            "allow-all must clear both local lists after a failed send");
        const std::string tooLong(25,'N');
        Require(!submit("/ex "+tooLong) && mode.m_isWaitingWhisperSetting==1 && mode.m_refuseWhisperName==tooLong,
            "overlong per-PC name must update pending fields before rejecting send");
        LoopbackPeer ignorePeer(*connection);
        const auto perPc=[&](const std::string& name,u8 operation){
            std::vector<u8> expected(27,0);expected[0]=0xcf;expected[26]=operation;
            std::copy(name.begin(),name.end(),expected.begin()+2);
            Require(ignorePeer.ReceiveExact(27)==expected,"actual CF fixed-name/operation frame differs");
        };
        TestConnection receive;CGameModePacketRouter router;RegisterDefaultGameModePacketHandlers(router);
        const auto dispatch=[&](const std::vector<u8>& packet){
            Require(receive.QueueBytes(packet),"ignore response queue failed");
            Require(ProcessGameModePacketLoop(mode,receive,router,1).packetsDispatched==1 && receive.BufferedBytes()==0,"ignore response framing/router failed");
        };
        mode.m_isWaitingWhisperSetting=7;
        Require(submit("/ex"),"list request failed while a setting was pending");
        Require(ignorePeer.ReceiveExact(2)==std::vector<u8>({0xd3,0}) && mode.m_isWaitingWhisperSetting==7,"D3 changed pending state");
        g_session.m_cmdOnOffSet={{0,18}};
        // Actual common Enter -> parser ->47; no waiting-state send gate or FIFO.
        chat->FocusQtInput(2);chat->SetQtInputText(2,"/ex Peer");chat->HandleKeyDown(VK_RETURN);perPc("Peer",0);
        Require(chat->GetInputText().empty() && chat->GetInputHistory().front()=="/ex Peer"
            && mode.m_isWaitingWhisperSetting==1 && mode.m_refuseWhisperName=="Peer" && g_session.m_cmdOnOffSet[0].first==1,
            "real ignore Enter lost source history/prelude/pending ownership");
        const std::string fullName="ABCDEFGHIJKLMNOPQRSTUVWX";
        Require(submit("/in "+fullName),"source must allow all24 name bytes");perPc(fullName,1);
        Require(mode.m_refuseWhisperName==fullName,"new request did not overwrite pending name");
        dispatch({0xd1,0,0,0});
        Require(!mode.m_isWaitingWhisperSetting && g_windowMgr.GetChatEvents().back().text==fullName+" IGNORE OK",
            "older reply must use latest pending name, not an invented per-request queue");
        Require(!submit("/ex "+tooLong) && mode.m_refuseWhisperName==tooLong,"overlong name was truncated or pending name retained");
        Require(SelectSocket(ignorePeer.peer.value,true,false,std::chrono::milliseconds(20))==0,"overlong request leaked bytes");
        Require(mode.ProcessSourceTalk(18,{})!=0,"source empty per-PC name rejected");perPc("",0);
        g_session.m_exNameList={"LocalOnly"};g_session.m_exAidList={99};
        Require(submit("/exall"),"deny-all send failed");Require(ignorePeer.ReceiveExact(3)==std::vector<u8>({0xd0,0,0}),"deny-all state byte differs");
        Require(g_session.IsExName("LocalOnly"),"deny-all invented local exclusion changes");
        Require(submit("/inall"),"allow-all send failed");Require(ignorePeer.ReceiveExact(3)==std::vector<u8>({0xd0,0,1}),"allow-all state byte differs");
        Require(g_session.m_exNameList.empty() && g_session.m_exAidList.empty() && mode.m_isWaitingWhisperSetting==1,"allow-all clear/waiting order differs");
        g_session.m_exNameList={"LocalOnly"};g_session.m_exAidList={99};
        mode.m_refuseWhisperName="LastRequested";
        const char* suffix[2][3]={{" IGNORE OK"," IGNORE FAIL1",""},{" ALLOW OK"," ALLOW FAIL1"," ALLOW FAIL2"}};
        for(u8 op=0;op<2;++op)for(u8 result=0;result<3;++result){
            mode.m_isWaitingWhisperSetting=1;
            const auto before=g_windowMgr.GetChatEvents().size();dispatch({0xd1,0,op,result});
            Require(!mode.m_isWaitingWhisperSetting && mode.m_refuseWhisperName=="LastRequested"
                && g_windowMgr.GetChatEvents().size()==before+1 && g_windowMgr.GetChatEvents().back().text==std::string("LastRequested")+suffix[op][result]
                && g_windowMgr.GetChatEvents().back().color==0xffff00,"D1 result/pending name/known-empty suffix/source color differs");
            Require(g_session.m_exNameList==std::vector<std::string>{"LocalOnly"} && g_session.m_exAidList==std::list<u32>{99},"D1 reply invented local exclusion changes");
        }
        mode.m_isWaitingWhisperSetting=0;dispatch({0xd1,0,0,0});
        Require(!mode.m_isWaitingWhisperSetting && g_windowMgr.GetChatEvents().back().text=="LastRequested IGNORE OK","unsolicited source reply was gated on pending state");
        const char* states[2][2]={{"ALL DENIED",""},{"ALL ALLOWED","ALL ALLOW FAIL"}};
        for(u8 op=0;op<2;++op)for(u8 result=0;result<2;++result){
            mode.m_isWaitingWhisperSetting=1;const auto before=g_windowMgr.GetChatEvents().size();dispatch({0xd2,0,op,result});
            Require(!mode.m_isWaitingWhisperSetting,"D2 did not clear pending");
            if(*states[op][result])Require(g_windowMgr.GetChatEvents().size()==before+1 && g_windowMgr.GetChatEvents().back().text==states[op][result]
                && g_windowMgr.GetChatEvents().back().color==0xffff00,"D2 language branch or source color differs");
            else Require(g_windowMgr.GetChatEvents().size()==before,"known-empty D2 resource was replaced with fallback");
        }
        for(const auto packet:std::vector<std::vector<u8>>{{0xd1,0,2,0},{0xd1,0,0,3},{0xd2,0,2,0},{0xd2,0,0,2}}){
            mode.m_isWaitingWhisperSetting=1;g_windowMgr.m_chatWndShow=0;const auto before=g_windowMgr.GetChatEvents().size();dispatch(packet);
            Require(!mode.m_isWaitingWhisperSetting && g_windowMgr.m_chatWndShow==1 && g_windowMgr.GetChatEvents().size()==before,
                "unrecognized ignore reply must show/clear pending without a notice");
        }
        mode.m_isWaitingWhisperSetting=1;
        auto before=g_windowMgr.GetChatEvents().size();Require(receive.QueueBytes({0xd1,0,0}),"partial D1 queue failed");
        Require(ProcessGameModePacketLoop(mode,receive,router,8).packetsDispatched==0 && mode.m_isWaitingWhisperSetting==1
            && g_windowMgr.GetChatEvents().size()==before,"fragmented D1 dispatched early");
        Require(receive.QueueBytes({0}),"D1 tail queue failed");
        Require(ProcessGameModePacketLoop(mode,receive,router,8).packetsDispatched==1 && !mode.m_isWaitingWhisperSetting,"fragmented D1 did not complete");
        mode.m_isWaitingWhisperSetting=1;dispatch({0xd4,0,4,0});
        Require(mode.m_isWaitingWhisperSetting==1 && g_windowMgr.GetChatEvents().back().text=="EMPTY LIST"
            && g_windowMgr.GetChatEvents().back().color==0x00ffff,"empty list header/color or pending ownership differs");
        std::vector<u8> list(53,0xcc);list[0]=0xd4;list[1]=0;list[2]=53;list[3]=0;
        std::copy_n("ListPeer",8,list.begin()+4);list[12]=0;std::copy(fullName.begin(),fullName.end(),list.begin()+28);
        before=g_windowMgr.GetChatEvents().size();dispatch(list);
        Require(g_windowMgr.GetChatEvents().size()==before+3 && g_windowMgr.GetChatEvents()[before].text=="EXCLUSION LIST"
            && g_windowMgr.GetChatEvents()[before+1].text=="ListPeer" && g_windowMgr.GetChatEvents()[before+2].text==fullName
            && mode.m_isWaitingWhisperSetting==1 && g_session.m_exNameList==std::vector<std::string>{"LocalOnly"},
            "D4 count/24-byte names/ignored partial tail or non-mutating list semantics differ");
        std::vector<u8> partialList(27,0);partialList[0]=0xd4;partialList[2]=27;dispatch(partialList);
        Require(g_windowMgr.GetChatEvents().back().text=="EMPTY LIST","partial first list name should not count");
        // Direct handler boundary checks supplement the framed valid packet paths.
        before=g_windowMgr.GetChatEvents().size();g_windowMgr.m_chatWndShow=0;
        const std::array<u8,4> shortPc{{0xd1,0,0,0}},badList{{0xd4,0,28,0}};
        router.Dispatch(mode,{0xd1,3,shortPc.data()});router.Dispatch(mode,{0xd2,4,nullptr});router.Dispatch(mode,{0xd4,4,badList.data()});
        Require(mode.m_isWaitingWhisperSetting==1 && g_windowMgr.m_chatWndShow==0 && g_windowMgr.GetChatEvents().size()==before,
            "malformed ignore reply changed source state or displayed a notice");
        std::vector<u8> signedList(32768,0);signedList[0]=0xd4;signedList[3]=0x80;
        router.Dispatch(mode,{0xd4,32768,signedList.data()});
        Require(g_windowMgr.GetChatEvents().size()==before+1 && g_windowMgr.GetChatEvents().back().text=="EXCLUSION LIST",
            "selected signed16 list count was changed to unsigned iteration");
        connection->Disconnect();
        std::cout<<"{\"case_id\":\"source_chat_ignore\",\"contract_id\":\"PK09.21\",\"actual_request_frames\":true,\"framed_D1_D2_D4\":true,\"pending_before_failure\":true,\"last_name_not_fifo\":true,\"outcome\":\"PASS\"}\n";
    }
