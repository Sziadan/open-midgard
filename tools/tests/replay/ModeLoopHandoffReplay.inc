void ModeLoopHandoffReplay()
{
    SetCase("same_world_mode_handoff","PK05.05","deployed-world-entry-regression","actual0091 -> terminal receive guard -> real CModeMgr::Run -> replacement mode drains tail");
    MapTransitionLifetimeSnapshot session;
    const CModeMgr manager=g_modeMgr;
    struct Restore {
        MapTransitionLifetimeSnapshot& session;const CModeMgr& manager;
        bool hadQuit=false;int quitCode=0;
        Restore(MapTransitionLifetimeSnapshot& s,const CModeMgr& m):session(s),manager(m){
            MSG message{};while(PeekMessageW(&message,nullptr,WM_QUIT,WM_QUIT,PM_REMOVE)){hadQuit=true;quitCode=static_cast<int>(message.wParam);}
        }
        ~Restore(){g_windowMgr.RemoveAllWindows();session.Restore();g_modeMgr=manager;
            MSG message{};while(PeekMessageW(&message,nullptr,WM_QUIT,WM_QUIT,PM_REMOVE)){}if(hadQuit)PostQuitMessage(quitCode); }
    } restore{session,manager};
    g_windowMgr.RemoveAllWindows();SkillPacketReplayAccess::SetCurrent(nullptr);
    ClearPendingDisconnectAction();g_session.m_pendingReturnToCharSelect=0;
    auto* connection=CRagConnection::instance();connection->Disconnect();LoopbackPeer peer(*connection);
    auto& router=GetProductionGameModePacketRouter();router.Clear();RegisterDefaultGameModePacketHandlers(router);
    int initialized=0,exited=0,firstRuns=0,replacementRuns=0,tailPackets=0,presented=0;
    bool oldModeRepeated=false;
    router.Register(0x007f,[&](CGameMode&,const PacketView&){++tailPackets;});
    std::vector<u8> packet(22);packet[0]=0x91;std::copy_n("prontera",8,packet.begin()+2);packet[18]=156;packet[20]=184;
    packet.insert(packet.end(), {0x52, 2, 12, 0, 1, 0, 0, 0, 0, 0, 0, 0});
    packet.insert(packet.end(), {0x89, 2, 20, 0, 0, 0, 30, 0, 0, 0, 0, 0});
    packet.insert(packet.end(), {0xca, 0, 0});
    packet.insert(packet.end(),{0x7f,0,0x12,0x34,0x56,0x78});peer.Send(packet);
    MapTransitionServices mapServices;mapServices.presentLoading=[&](const char*,float){++presented;};
    struct ProbeMode : CGameMode {
        CRagConnection& connection;CGameModePacketRouter& router;int ordinal;
        int& initialized;int& exited;int& firstRuns;int& replacementRuns;int& tailPackets;bool& oldModeRepeated;
        ProbeMode(CRagConnection& c,CGameModePacketRouter& r,int n,int& i,int& e,int& f,int& replacement,int& tails,bool& repeated)
            :connection(c),router(r),ordinal(n),initialized(i),exited(e),firstRuns(f),replacementRuns(replacement),tailPackets(tails),oldModeRepeated(repeated){}
        void OnInit(const char* name) override {
            Require(std::string(name)=="prontera.rsw","same-world handoff changed requested world");
            std::snprintf(m_rswName,sizeof(m_rswName),"%s",name);m_mapLoadingStage=MapLoading_PresentScreen;++initialized;
            g_session.ClearNpcShopState(!m_preserveCommerceRepliesOnMapReload);
            if (ordinal == 1) {
                g_session.m_cashShopPending = true;
                g_session.m_cashShopPendingQuote = g_session.m_cashShopQuote;
                g_session.m_shopUsesCash = true;
                g_session.m_cashShopPoints = 100;
                g_session.QueueNpcShopResult(NpcShopMode::Buy);
                m_auctionStaging.stagedIndex = 42;
                m_auctionStaging.stagedItemId = 985;
                m_auctionSearch.open = true;
                m_auctionSearch.awaitingReply = true;
                m_auctionSearch.sentAt = 100;
                m_auctionSearch.rows.emplace_back();
            }
        }
        void OnExit() override {
            g_session.ClearNpcShopState(!m_preserveCommerceRepliesOnMapReload);
            ++exited;
        }
        int OnRun() override {
            GameModePacketLoopResult result{};
            if(ordinal==1){
                if(++firstRuns>1){oldModeRepeated=true;g_modeMgr.Quit();return 1;}
                Require(!ProcessNetworkUpdate(connection,router,128,GetTickCount(),&result)
                    && result.modeOperationRequested && tailPackets==0,"real0091 failed to stop old mode before queued tail");
                Require(g_session.m_cashShopPending && !g_session.m_shopUsesCash,
                    "warp discarded pending cash reply ownership or kept the old quote open");
            }else {
                Require(connection.IsOpen() && m_auctionStaging.stagedIndex == 42
                    && m_auctionStaging.stagedItemId == 985 && !m_auctionSearch.open
                    && m_auctionSearch.awaitingReply == (replacementRuns == 0)
                    && m_auctionSearch.discardReply == (replacementRuns == 0)
                    && m_auctionSearch.sentAt == 100 && m_auctionSearch.rows.empty(),
                    "replacement mode lost auction reply ownership or kept stale auction UI state");
                Require(g_session.m_cashShopPending == (replacementRuns == 0),
                    "replacement mode did not inherit the pending cash reply exactly once");
                Require(g_session.GetNpcShopPendingResult() == (replacementRuns == 0 ? NpcShopMode::Buy : NpcShopMode::None),
                    "replacement mode lost ownership of the pending NPC purchase");
                Require(ProcessNetworkUpdate(connection,router,128,GetTickCount(),&result),"replacement mode inherited terminal generation latch");
                Require(!m_auctionSearch.awaitingReply && m_auctionSearch.rows.empty(),
                    "replacement mode did not drain the old auction page");
                Require(!g_session.m_cashShopPending && !g_session.m_cashShopPoints && !g_session.m_kafraShopPoints,
                    "old cash reply updated a retired shop quote");
                Require(g_session.GetNpcShopPendingResult() == NpcShopMode::None,
                    "replacement mode did not consume the NPC purchase reply");
                Require(tailPackets==1,"replacement mode lost or repeated queued tail");
                if(++replacementRuns==2)g_modeMgr.Quit();
            }
            return 1;
        }
    };
    int created=0;ModeRunServices services;
    services.createMode=[&](int type)->CMode* {
        Require(type==1 && created<3,"same-world mode request restarted continuously");
        auto* mode=new ProbeMode(*connection,router,++created,initialized,exited,firstRuns,replacementRuns,tailPackets,oldModeRepeated);
        mode->SetMapTransitionServices(&mapServices);return mode;
    };
    services.presentLoading=[&](const char*,float){++presented;};
    g_modeMgr.Run(1,"prontera.rsw",&services);
    Require(!oldModeRepeated && created==2 && initialized==2 && exited==2 && firstRuns==1 && replacementRuns==2
        && tailPackets==1 && presented==2,"same-world handoff lifecycle counts: created="+std::to_string(created)+" init="+std::to_string(initialized)
            +" exit="+std::to_string(exited)+" first="+std::to_string(firstRuns)+" replacement="+std::to_string(replacementRuns)+" tail="+std::to_string(tailPackets)+" loading="+std::to_string(presented));
    std::cout<<"{\"case_id\":\"same_world_mode_handoff\",\"contract_id\":\"PK05.05\",\"actual0091\":true,\"real_mode_loop\":true,\"same_world_reinitialized_once\":true,\"terminal_mode_retired\":true,\"queued_tail_resumed\":true,\"outcome\":\"PASS\"}\n";
}
