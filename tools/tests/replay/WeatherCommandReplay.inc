    {
        CWorld world;CPlayer player;player.m_gid=0xf0012345;world.m_player=&player;
        struct RestoreSnow {
            CGameMode& mode;CWorld& world;CWorld* oldWorld;
            decltype(mode.m_runtimeActors) actors;CWeather weather=g_Weather;
            std::array<char,40> map{};bool enabled=g_session.m_isEffectOn;
            int skip=g_isSkipFrame;
            RestoreSnow(CGameMode& m,CWorld& w):mode(m),world(w),oldWorld(m.m_world),actors(std::move(m.m_runtimeActors)){
                std::copy(std::begin(m.m_rswName),std::end(m.m_rswName),map.begin());m.m_world=&w;g_Weather.Reset();}
            ~RestoreSnow(){g_Weather.Reset();for(auto* object:world.m_gameObjectList)delete object;world.m_gameObjectList.clear();
                mode.m_world=oldWorld;mode.m_runtimeActors=std::move(actors);std::copy(map.begin(),map.end(),std::begin(mode.m_rswName));
                g_Weather=weather;g_session.m_isEffectOn=enabled;g_isSkipFrame=skip;CRagConnection::instance()->Disconnect();}
        } restoreSnow(mode,world);
        mode.m_runtimeActors[player.m_gid]=&player;g_session.m_isEffectOn=true;
        Require(!g_Weather.m_Effect,"weather constructor/reset retained a handle");
        g_Weather.LaunchSnow();auto* first=g_Weather.m_Effect;
        Require(first && first->GetEffectType()==162 && first->GetDuration()==99999999 && first->GetStateCount()==0
            && g_Weather.m_pPlayer==&player && player.m_effectList.size()==1 && world.m_gameObjectList.size()==1,
            "snow launch did not use actual actor/world effect ownership");
        g_Weather.LaunchSnow();auto* second=g_Weather.m_Effect;
        Require(second && second!=first && player.m_effectList.size()==2,"selected JLE branch inverted: >300 must launch again");
        const auto duration=[](CRagEffect* effect,int end){effect->SendMsg(nullptr,80,end,0,0);};
        // Advance the actual effect without spawning resource-dependent primitives.
        g_isSkipFrame=1;duration(second,100);for(int i=0;i<10;++i)Require(second->OnProcess(),"snow logic step unexpectedly expired");
        Require(second->GetStateCount()==10,"actual snow state count did not advance");
        duration(second,310);g_Weather.LaunchSnow();
        Require(g_Weather.m_Effect==second && player.m_effectList.size()==2,"remaining==300 must skip launch");
        duration(second,309);g_Weather.LaunchSnow();Require(g_Weather.m_Effect==second,"remaining299 must skip launch");
        duration(second,9);g_Weather.LaunchSnow();Require(g_Weather.m_Effect==second,"negative remaining must skip launch");
        duration(second,311);g_Weather.LaunchSnow();auto* current=g_Weather.m_Effect;
        Require(current && current!=second && player.m_effectList.size()==3,"remaining301 must launch");
        duration(current,100);for(int i=0;i<37;++i)Require(current->OnProcess(),"snow logic step unexpectedly expired");g_Weather.StopSnow();
        Require(current->GetStateCount()==37 && current->GetDuration()==337 && g_Weather.m_Effect==current && player.m_effectList.size()==3,"stop must shorten expiry without removing effects");
        duration(current,99999999);
        connection->Disconnect();LoopbackPeer weatherPeer(*connection);
        const auto packet=[&](bool on){Require(weatherPeer.ReceiveExact(6)==std::vector<u8>({0x1d,0x02,static_cast<u8>(on?0:1),0,0,0}),"effect command541 frame/value differs");};
        const auto event=[&](const char* text){const auto& events=g_windowMgr.GetChatEvents();
            Require(!events.empty() && events.back().text==text && events.back().color==0xffff64 && g_windowMgr.m_chatWndShow,"effect notice/color/show differs");};
        std::strcpy(mode.m_rswName,"xmas.rsw");g_session.m_cmdOnOffSet={{0,8},{9,8}};g_windowMgr.SetChatMsg("effect retained buffer");
        g_windowMgr.ShowChatWindow(false);Require(mode.SendMsg(47,8,0,0) && !g_session.m_isEffectOn,"effect off command failed");packet(false);event("EFFECT OFF");
        Require(current->GetDuration()==337 && g_session.m_cmdOnOffSet[0].first==1 && g_session.m_cmdOnOffSet[1].first==9
            && std::string(g_windowMgr.GetChatMsg())=="effect retained buffer","effect snow-stop/prelude/buffer differs");
        Require(submit("/effect") && g_session.m_isEffectOn && g_Weather.m_Effect==current && player.m_effectList.size()==3,"effect on during <=300 tail must not relaunch");packet(true);event("EFFECT ON");
        duration(current,338);Require(mode.SendMsg(47,8,0,0),"second effect off failed");packet(false);
        // Stop sets exactly300 again; force the independent >300 boundary before ON.
        duration(current,338);std::strcpy(mode.m_rswName,"xmas_fild01.rsw");
        Require(mode.SendMsg(47,8,0,0),"second Xmas map effect on failed");packet(true);
        Require(g_Weather.m_Effect!=current && player.m_effectList.size()==4,"second exact Xmas map did not launch via weather owner");
        current=g_Weather.m_Effect;
        for(const char* map:{"prontera.rsw","Xmas.rsw","xmas_fild01","xmas_fild01.rsw.extra"}){
            std::strcpy(mode.m_rswName,map);const auto count=player.m_effectList.size();const int previousDuration=current->GetDuration();
            const bool next=!g_session.m_isEffectOn;Require(mode.SendMsg(47,8,0,0),"non-Xmas effect toggle failed");packet(next);
            Require(current==g_Weather.m_Effect && current->GetDuration()==previousDuration && player.m_effectList.size()==count,"snow map gate used normalized/prefix matching");
        }
        // Reset forgets only the handle; the world still owns all four effects.
        g_Weather.Reset();Require(!g_Weather.m_Effect && g_Weather.m_pPlayer==&player && player.m_effectList.size()==4,"weather reset destroyed/detached owned effects");
        g_session.m_isEffectOn=true;g_Weather.LaunchSnow();current=g_Weather.m_Effect;
        // Remove earlier effects, then exercise the actual world's final expiry sweep.
        for(auto it=world.m_gameObjectList.begin();it!=world.m_gameObjectList.end();){auto* object=*it;
            if(object==current){++it;continue;}it=world.m_gameObjectList.erase(it);delete object;}
        Require(player.m_effectList.size()==1,"actual effect destructor did not update actor attachment list");
        g_Weather.StopSnow();Require(current->GetDuration()==current->GetStateCount()+300 && !world.m_gameObjectList.empty(),"stop removed its world object");
        // Duration message80 also belongs to the production effect API. Force its
        // final tick without manufacturing map assets or mutating private clocks.
        duration(current,current->GetStateCount()+1);g_isSkipFrame=1;
        world.UpdateGameObjects();Require(world.m_gameObjectList.empty() && player.m_effectList.empty(),"snow did not expire through actual world/effect processing");
        g_Weather.Process();Require(!g_Weather.m_Effect && g_Weather.m_pPlayer==&player,"weather Process failed to forget stale effect without dereferencing it");
        // A retained stale handle is also validated before an immediate command.
        g_Weather.LaunchSnow();current=g_Weather.m_Effect;world.m_gameObjectList.remove(current);delete current;
        g_Weather.StopSnow();Require(!g_Weather.m_Effect,"immediate stop dereferenced an already-deleted effect");
        auto* retiredPlayer=new CPlayer();retiredPlayer->m_gid=0xf0012346;
        world.m_player=retiredPlayer;mode.m_runtimeActors[retiredPlayer->m_gid]=retiredPlayer;g_Weather.LaunchSnow();
        Require(g_Weather.m_pPlayer==retiredPlayer && g_Weather.m_Effect,"replacement player snow did not launch");
        const auto retiredId=retiredPlayer->m_gid;delete retiredPlayer;mode.m_runtimeActors.erase(retiredId);world.m_player=&player;
        Require(!g_Weather.m_Effect && !g_Weather.m_pPlayer,"actor destruction retained dangling weather owner");
        std::strcpy(mode.m_rswName,"prontera.rsw");connection->Disconnect();const bool expected=!g_session.m_isEffectOn;
        Require(!mode.SendMsg(47,8,0,0) && g_session.m_isEffectOn==expected,"failed effect request rolled back local source toggle");event(expected?"EFFECT ON":"EFFECT OFF");
        std::cout<<"{\"case_id\":\"source_weather_command\",\"contract_id\":\"PK09.21\",\"selected_JLE_threshold\":true,\"real_world_effect_lifetime\":true,\"exact_maps\":true,\"packet541\":true,\"outcome\":\"PASS\"}\n";
    }
