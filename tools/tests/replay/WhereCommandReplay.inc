    {
        C3dAttr attr;CWorld world;CPlayer player;
        struct RestoreWhere {
            CGameMode& mode;CWorld& world;CWorld* previous;
            ro::SourceMapNames names=g_session.m_mapNameTable;ServiceType service=g_serviceType;
            std::array<char,40> filename;
            RestoreWhere(CGameMode& m,CWorld& w):mode(m),world(w),previous(m.m_world){std::copy(std::begin(m.m_rswName),std::end(m.m_rswName),filename.begin());}
            ~RestoreWhere(){mode.m_world=previous;world.m_player=nullptr;world.m_attr=nullptr;
                g_session.m_mapNameTable=std::move(names);g_serviceType=service;std::copy(filename.begin(),filename.end(),std::begin(mode.m_rswName));}
        } restoreWhere(mode,world);
        world.m_attr=&attr;world.m_player=&player;mode.m_world=&world;
        std::string data="zed.rsw#Zed#\r\n//comment#ignored#\nprontera.rsw#Capital City#\n1@tower.rsw#Tower#\nEmpty.rsw##\n#EmptyKey#\n odd.rsw# Raw_Label #\n";
        data+="junk without delimiter\nxxx.rsw#X#\n";data.append("nul\0h.rsw#Nulvalue#\n",20);data+="unterminated.rsw#no final delimiter";
        g_session.m_mapNameTable.Parse(data);
        Require(g_session.GetMapName("prontera.rsw")=="Capital City" && g_session.GetMapName("Prontera.rsw")=="NOWHERE",
            "source map-name lookup lost exact case/sorted key/fallback");
        Require(g_session.GetMapName("0011@tower.rsw")=="Tower" && g_session.GetMapName("abc1@tower.rsw")=="Tower",
            "source map-name surgery must remove three bytes before the final four-byte-at boundary, without a digit guard");
        Require(g_session.GetMapName("1@tower.rsw")=="Tower" && g_session.GetMapName("Empty.rsw").empty()
            && g_session.GetMapName("")=="EmptyKey" && g_session.GetMapName(" odd.rsw")==" Raw_Label "
            && g_session.GetMapName("xxx.rsw")=="X" && g_session.GetMapName("nul")=="Nulvalue"
            && g_session.GetMapName("unterminated.rsw")=="NOWHERE","source map-name raw token/empty/NUL/odd-tail behavior changed");
        attr.m_width=5;attr.m_height=7;attr.m_zoom=10;player.m_pos={-0.5f,9999.0f,-31.0f};
        const auto where=[&](const char* filename,const char* expected,int rows){
            std::strcpy(mode.m_rswName,filename);g_windowMgr.ClearChatEvents();Require(submit("/where"),"WHERE parser/action dispatch failed");
            const auto& events=g_windowMgr.GetChatEvents();Require(static_cast<int>(events.size())==rows,"WHERE Germany/battle stream count changed");
            for(const auto& event:events)Require(event.text==expected && event.color==0xffff64u,"WHERE selected name/axis/floor/format/color changed");};
        g_serviceType=ServiceAmerica;where("prontera.rsw","Capital City(prontera) : 1, -1",2);
        g_serviceType=ServiceGermany;where("0011@tower.rsw","Tower(0011@tower) : 1, -1",1);
        g_serviceType=ServiceAmerica;where("four","NOWHERE(four) : 1, -1",2);where("abcde","NOWHERE(a) : 1, -1",2);
        attr.m_width=-5;attr.m_height=-7;attr.m_zoom=-10;where("prontera.rsw","Capital City(prontera) : -2, 0",2);
        attr.m_zoom=0;where("prontera.rsw","Capital City(prontera) : 0, 0",2);
        Require(SelectSocket(peer.peer.value,true,false,std::chrono::milliseconds(20))==0,"WHERE emitted a network packet");
        {std::ofstream file("mapnametable.txt",std::ios::binary);file<<"prontera.rsw#Base#";}
        std::filesystem::create_directories("german");
        {std::ofstream file("german/mapnametable.txt",std::ios::binary);file<<"prontera.rsw#Deutsch#";}
        g_serviceType=ServiceAmerica;Require(g_session.m_mapNameTable.Load(7) && g_session.GetMapName("prontera.rsw")=="Deutsch","map-name OS-language resource selection missing");
        g_serviceType=ServiceKorea;Require(g_session.m_mapNameTable.Load(7) && g_session.GetMapName("prontera.rsw")=="Base","Korea map-name localization bypass missing");
        g_serviceType=ServiceAmerica;Require(g_session.m_mapNameTable.Load(9) && g_session.GetMapName("prontera.rsw")=="Base","absent localized map-name table must fall back to base");
        std::filesystem::remove("mapnametable.txt");Require(!g_session.m_mapNameTable.Load(9) && g_session.GetMapName("prontera.rsw")=="NOWHERE","failed map-name reload retained stale rows");
    }
