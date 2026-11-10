    {
        struct RestoreGreeting {
            decltype(g_session.m_friendList) friends=g_session.m_friendList;
            bool multi=g_multiLang,whisper=g_session.m_isShowWhisperWnd,friendWhisper=g_session.m_isShowWhisperWnd_Friend;s8 hi=g_MessageHi;
            ServiceType service=g_serviceType;ServerType server=g_serverType;
            u16 language=submissionLanguage;
            int show=g_windowMgr.m_chatWndShow,visible=g_windowMgr.m_chatWnd?g_windowMgr.m_chatWnd->m_show:0;UIWindow* edit=g_windowMgr.m_editWindow;
            UIWhisperWnd* owned=nullptr;
            ~RestoreGreeting(){if(owned)g_windowMgr.DeleteWindow(owned);g_session.m_friendList=std::move(friends);
                g_multiLang=multi;g_MessageHi=hi;g_session.m_isShowWhisperWnd=whisper;g_session.m_isShowWhisperWnd_Friend=friendWhisper;g_serviceType=service;g_serverType=server;submissionLanguage=language;
                if(g_windowMgr.m_chatWnd)g_windowMgr.m_chatWnd->SetShow(visible);g_windowMgr.m_chatWndShow=show;g_windowMgr.m_editWindow=edit;g_session.SetActiveStatusIcon(107,false,0);}
        } restoreGreeting;
        const auto member=[](const std::string& name,bool online){FRIEND_INFO value{};value.characterName=name;value.isValid=online;return value;};
        LoopbackPeer hiPeer(*connection);
        const auto expectWhisper=[&](const std::string& target,const std::string& body){
            std::vector<u8> expected(body.size()+29,0);expected[0]=0x96;expected[2]=u8(expected.size());expected[3]=u8(expected.size()>>8);
            std::copy(target.begin(),target.end(),expected.begin()+4);std::copy(body.begin(),body.end(),expected.begin()+28);
            const auto actual=hiPeer.ReceiveExact(expected.size());
            Require(actual==expected,"Hi/shared whisper frame differs target="+target+" expected="+std::to_string(expected.size())+" actual bytes="+std::to_string(actual.size()));
        };
        TestConnection receive;CGameModePacketRouter router;RegisterDefaultGameModePacketHandlers(router);
        const auto ack=[&](CGameMode& owner,u8 result){Require(receive.QueueBytes({0x98,0,result}),"Hi acknowledgement queue failed");
            Require(ProcessGameModePacketLoop(owner,receive,router,1).packetsDispatched==1,"Hi acknowledgement production router failed");};
        g_multiLang=false;g_MessageHi=0;g_serviceType=ServiceAmerica;g_serverType=ServerNormal;
        g_session.m_isShowWhisperWnd=false;g_session.m_isShowWhisperWnd_Friend=false;
        mode.m_sourceLastSubmittedChat.clear();mode.m_sourceSameChatRepeatCount=0;
        g_session.m_friendList.clear();g_windowMgr.SetChatMsg("preserve default source buffer");chat->SetShow(0);g_windowMgr.m_chatWndShow=0;
        const std::array<std::string,3> emptyParameters{};
        Require(mode.SendMsg(47,150,reinterpret_cast<msgparam_t>(&emptyParameters),0)!=0,"UI Hi150 empty parameter array failed");
        Require(std::string(g_windowMgr.GetChatMsg())=="preserve default source buffer" && !g_MessageHi && g_windowMgr.m_chatWndShow==0 && !chat->m_show
            && g_windowMgr.GetChatEvents().back().text=="[ To Friends ] : (Hi) *^_^*" && g_windowMgr.GetChatEvents().back().color==0xffff1e,
            "zero-friend default greeting changed buffer/count/visibility or selected literal/color");
        Require(SelectSocket(hiPeer.peer.value,true,false,std::chrono::milliseconds(20))==0,"empty friend list sent a request");
        g_session.m_friendList={member("First",false),member("Second",true),member("First",false)};
        const auto before=g_windowMgr.GetChatEvents().size();
        Require(submit("/hi \"two words\" next"),"parameterized Hi parser dispatch failed");
        expectWhisper("First","(Hi) two words next ");expectWhisper("Second","(Hi) two words next ");expectWhisper("First","(Hi) two words next ");
        Require(g_MessageHi==3 && mode.m_lastWhisperName=="First" && mode.m_lastWhisper=="(Hi) two words next "
            && mode.m_sourceSameChatRepeatCount==0 && std::string(g_windowMgr.GetChatMsg()).empty()
            && g_windowMgr.GetChatEvents().size()==before+1 && g_windowMgr.GetChatEvents().back().text=="[ To Friends ] : (Hi) two words next ",
            "friend order/offline/duplicates/repeat bypass or saved notice copy differs");
        auto events=g_windowMgr.GetChatEvents().size();ack(mode,0);
        Require(g_MessageHi==2 && g_windowMgr.GetChatEvents().size()==events,"Hi success fallback was not hidden/decremented");
        {
            CGameMode nextMode;
            Require(&nextMode.m_hiddenWhisperAcknowledgements==&g_MessageHi && nextMode.m_hiddenWhisperAcknowledgements==2,
                "new mode reset the process-global Hi counter");
            ack(nextMode,1);
        }
        Require(g_MessageHi==1 && g_windowMgr.GetChatEvents().size()==events,"Hi counter did not survive another mode's failed-target reply/destruction");
        ack(mode,2);Require(g_MessageHi==1 && g_windowMgr.GetChatEvents().size()==events+1,"refused whisper reply incorrectly consumed Hi count");
        Require(g_windowMgr.OpenWhisperWindow("First",nullptr),"Hi private-owner fixture failed");restoreGreeting.owned=g_windowMgr.m_whisperWindows.at("First");
        const auto privateRows=restoreGreeting.owned->GetRows().size();ack(mode,0);
        Require(g_MessageHi==1 && restoreGreeting.owned->GetRows().size()==privateRows+1,"dedicated owner must consume success before global Hi guard");
        g_windowMgr.DeleteWindow(restoreGreeting.owned);restoreGreeting.owned=nullptr;
        // Source increments after every attempted send, even when Berserk rejects all.
        g_MessageHi=0;g_session.m_friendList={member("Blocked1",false),member("Blocked2",true)};
        g_session.SetActiveStatusIcon(107,true,0);const auto oldTarget=mode.m_lastWhisperName;
        Require(submit("/hi"),"guarded Hi should still be consumed and announced");
        Require(g_MessageHi==2 && mode.m_lastWhisperName==oldTarget && std::string(g_windowMgr.GetChatMsg())=="(Hi) *^_^*"
            && g_windowMgr.GetChatEvents().back().text=="[ To Friends ] : (Hi) *^_^*","failed Hi attempts rolled back count, cleared retained buffer or omitted notice");
        Require(SelectSocket(hiPeer.peer.value,true,false,std::chrono::milliseconds(20))==0,"Berserk Hi leaked a request");
        g_session.SetActiveStatusIcon(107,false,0);g_MessageHi=0;
        g_session.m_friendList.clear();for(int i=0;i<43;++i)g_session.m_friendList.push_back(member("F"+std::to_string(i),i%2!=0));
        mode.m_sourceLastSubmittedChat.clear();mode.m_sourceSameChatRepeatCount=0;
        Require(submit("/hi"),"large friend greeting failed");
        for(int i=0;i<43;++i)expectWhisper("F"+std::to_string(i),"(Hi) *^_^*");
        Require(g_MessageHi==40 && mode.m_lastWhisperName=="F42","Hi cap or last-target state differs");
        events=g_windowMgr.GetChatEvents().size();for(int i=0;i<40;++i)ack(mode,1);
        Require(!g_MessageHi && g_windowMgr.GetChatEvents().size()==events,"first40 hidden failures were not consumed");
        ack(mode,1);Require(g_windowMgr.GetChatEvents().size()==events+1,"source cap must expose a later failure after40 acknowledgements");
        g_session.m_friendList={member("SignedByte",true)};g_MessageHi=127;
        mode.m_sourceSameChatRepeatCount=0;Require(submit("/hi"),"signed-byte boundary greeting failed");expectWhisper("SignedByte","(Hi) *^_^*");
        Require(sizeof(g_MessageHi)==1 && g_MessageHi==-128,"Hi INC AL signed-byte wrap was replaced by integer saturation");
        // Selected language prefixes use LANGID, never service/languageType as a substitute.
        const std::pair<u16,const char*> languages[]={
            {0x0401,"|b2"},{0x0402,"|cc"},{0x0419,"|cc"},{0x0422,"|cc"},{0x0423,"|cc"},
            {0x0404,"|88"},{0x0804,"|86"},{0x0c04,"|a1"},{0x0405,"|ee"},{0x040e,"|ee"},{0x0415,"|ee"},
            {0x0418,"|ee"},{0x041a,"|ee"},{0x041b,"|ee"},{0x0424,"|ee"},{0x042f,"|ee"},{0x0408,"|a1"},
            {0x040d,"|b1"},{0x0411,"|80"},{0x0412,"|81"},{0x041e,"|de"},{0x041f,"|a2"},
            {0x0425,"|ba"},{0x0426,"|ba"},{0x0427,"|ba"},{0x042a,"|a3"},{0x0409,"|00"}};
        for(const auto& row:languages)Require(ro::chat::SourceCharsetPrefix(true,row.first)==row.second
            && ro::chat::SourceCharsetPrefix(false,row.first).empty(),"selected LANGID->charset prefix mapping differs");
        g_multiLang=false;g_serviceType=ServiceGermany;ApplySourceChatLanguageMode();Require(g_multiLang,"German service must enable multi-language");
        g_serviceType=ServiceAmerica;ApplySourceChatLanguageMode();Require(g_multiLang,"unspecified service must retain prior multi-language state");
        for(const auto service:{ServiceIndia,ServiceBrazil,ServiceFrance,ServiceRussia,ServiceVietnam,ServiceChile}){
            g_multiLang=true;g_serviceType=service;ApplySourceChatLanguageMode();Require(!g_multiLang,"selected service must disable multi-language");}
        g_serverType=ServerInstantEvent;ApplySourceChatLanguageMode();Require(g_multiLang,"instant-event override must follow service disable");g_serverType=ServerNormal;
        {CLoginMode login;Require(!g_multiLang,"login mode must temporarily suspend multi-language");}
        Require(g_multiLang,"login destruction must restore saved multi-language state");
        g_MessageHi=0;g_serviceType=ServiceAmerica;submissionLanguage=0x0412;
        g_session.m_friendList={member("LanguagePeer",false)};mode.m_sourceLastSubmittedChat.clear();mode.m_sourceSameChatRepeatCount=0;
        Require(submit("/hi language"),"multilingual greeting failed");expectWhisper("LanguagePeer","|81(Hi) language ");
        Require(mode.m_lastWhisper=="|81(Hi) language " && g_windowMgr.GetChatEvents().back().text=="[ To Friends ] : (Hi) |81language ",
            "multilingual Hi wire/echo/notice prefix placement differs");
        g_serviceType=ServiceKorea;g_MessageHi=0;Require(submit("/hi korea"),"Korean-service greeting failed");expectWhisper("LanguagePeer","|81(Hi) korea ");
        Require(g_windowMgr.GetChatEvents().back().text=="[ To Friends ] : (Hi) korea ","Korean service should omit notice charset while wire remains multilingual");
        g_serviceType=ServiceAmerica;g_MessageHi=0;mode.m_sourceLastSubmittedChat.clear();mode.m_sourceSameChatRepeatCount=0;
        const std::string accepted(218,'a'),rejected(219,'b');
        Require(mode.SendSourceWhisper("Boundary",accepted),"multilingual218-byte body should be accepted");expectWhisper("Boundary","|81"+accepted);
        Require(!mode.SendSourceWhisper("Boundary",rejected) && mode.m_lastWhisper=="|81"+rejected && std::string(g_windowMgr.GetChatMsg()).empty(),
            "multilingual219-byte body must retain prefixed pending text but reject222-byte wire text after clear");
        Require(SelectSocket(hiPeer.peer.value,true,false,std::chrono::milliseconds(20))==0,"multilingual length guard leaked bytes");
        const std::string longHi(214,'x');g_MessageHi=0;
        Require(submit("/hi "+longHi) && g_MessageHi==1 && mode.m_lastWhisper.size()==222
            && g_windowMgr.GetChatEvents().back().text=="[ To Friends ] : (Hi) |81"+longHi+" ","Hi length failure lost count/full notice copy");
        Require(SelectSocket(hiPeer.peer.value,true,false,std::chrono::milliseconds(20))==0,"overlong Hi leaked bytes");
        connection->Disconnect();
        std::cout<<"{\"case_id\":\"source_friend_greeting\",\"contract_id\":\"PK09.21\",\"actual_friend_frames\":true,\"global_signed_counter\":true,\"mode_lifetime\":true,\"charset_prefix_and_limits\":true,\"outcome\":\"PASS\"}\n";
    }
