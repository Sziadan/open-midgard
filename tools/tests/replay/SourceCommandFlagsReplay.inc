    {
        // Exercise the actual CSession method, with a specification independent of its table.
        CSession snapshot;
        Require(snapshot.m_cmdOnOffSet.empty(),"source flag initialization ran during global construction");
        snapshot.InitializeSourceCommandFlags(false,1,false);
        const std::vector<std::pair<int,int>> defaults={
            {1,7},{1,8},{1,11},{0,134},{0,42},{0,135},{0,105},{0,108},{1,112},
            {1,113},{1,147},{1,106},{0,140},{1,53},{0,114},{0,137},{0,119},
            {0,120},{0,121},{1,107},{1,60},{1,164},{0,166}};
        Require(snapshot.m_cmdOnOffSet==defaults,"source23 startup defaults/order/types differ");
        snapshot.m_cmdOnOffSet.clear();snapshot.m_isEffectOn=false;snapshot.m_isBattle=5;
        snapshot.m_isNoTrade=-2;snapshot.m_isNoShift=2;snapshot.m_isNoCtrl=3;snapshot.m_isSkillFail=4;
        snapshot.m_isNoTalkMsg=-1;snapshot.m_isNoTalkMsg2=0;snapshot.m_isAura=6;snapshot.m_isWindow=7;
        snapshot.m_bMakeMissEffect=8;snapshot.m_isQuickSpell=9;snapshot.m_isQuickSpell2=10;
        snapshot.m_isShowNameTypeOld=-1;snapshot.m_fogOn=false;snapshot.m_isLogInOut=11;snapshot.m_isShopping=12;
        snapshot.InitializeSourceCommandFlags(true,-7,true);
        const std::vector<std::pair<int,int>> live={
            {0,7},{0,8},{-7,11},{5,134},{1,42},{-2,135},{2,105},{3,108},{4,112},
            {0,113},{1,147},{6,106},{7,140},{8,53},{0,114},{0,137},{9,119},
            {10,120},{0,121},{-1,107},{0,60},{11,164},{12,166}};
        Require(snapshot.m_cmdOnOffSet==live,"source23 live values/inversion/raw integers or fixed-zero entries differ");
        snapshot.LoadSourceCommandOptions();
        Require(snapshot.m_isEffectOn && snapshot.m_bMakeMissEffect==1 && snapshot.m_isNoCtrl==0 && snapshot.m_isBattle==5,
            "missing source preferences did not use selected defaults or reset unrelated state");
        Require(SaveSettingsIniInt("Option","isEffectOn",-7) && SaveSettingsIniInt("Option","m_bMakeMissEffect",0)
            && SaveSettingsIniInt("Option","m_isNoCtrl",9) && SaveSettingsIniInt("OptionWnd","NoCtrl",0)
            && SaveSettingsIniInt("OptionWnd","FogOn",0),"source flag settings setup failed");
        snapshot.LoadSourceCommandOptions();
        Require(snapshot.m_isEffectOn && snapshot.m_bMakeMissEffect==0 && snapshot.m_isNoCtrl==1 && !snapshot.m_fogOn,
            "source option normalization/ownership differs from reference");
        snapshot.m_isEffectOn=false;snapshot.m_bMakeMissEffect=7;snapshot.m_isNoCtrl=-3;
        Require(snapshot.SaveSourceCommandOptions(),"source command option save failed");
        CSession reloaded;reloaded.LoadSourceCommandOptions();
        Require(!reloaded.m_isEffectOn && reloaded.m_bMakeMissEffect==1 && reloaded.m_isNoCtrl==1 && !reloaded.m_fogOn,
            "source command option roundtrip lost live fog or bool normalization");
        struct RestoreFlags {
            bool effect=g_session.m_isEffectOn,fog=g_session.m_fogOn;
            int miss=g_session.m_bMakeMissEffect,noCtrl=g_session.m_isNoCtrl,sound=g_isSoundOn,soundMode=g_soundMode;
            bool paused=CAudio::GetInstance()->IsBgmPaused();
            float volume=CAudio::GetInstance()->GetVolume();int bgm=CAudio::GetInstance()->GetBgmVolume();
            decltype(g_session.m_cmdOnOffSet) flags=g_session.m_cmdOnOffSet;
            UIWindow* tip=g_windowMgr.m_tipOfTheDayWnd;UIWindow* commands=g_windowMgr.m_commandListWnd;
            UIWindow* ownedTip=nullptr;UIWindow* ownedCommands=nullptr;
            ~RestoreFlags(){
                if(ownedTip)g_windowMgr.DeleteWindow(ownedTip);
                if(ownedCommands)g_windowMgr.DeleteWindow(ownedCommands);
                g_windowMgr.m_tipOfTheDayWnd=tip;g_windowMgr.m_commandListWnd=commands;
                g_session.m_isEffectOn=effect;g_session.m_fogOn=fog;g_session.m_bMakeMissEffect=miss;g_session.m_isNoCtrl=noCtrl;
                g_isSoundOn=sound;g_soundMode=soundMode;g_session.m_cmdOnOffSet=std::move(flags);
                CAudio::GetInstance()->SetBgmPaused(paused);CAudio::GetInstance()->SetVolume(volume);CAudio::GetInstance()->SetBgmVolume(bgm);
            }
        } restoreFlags;
        Require(SaveSettingsIniInt("OptionWnd","BgmOn",0) && SaveSettingsIniInt("OptionWnd","SoundOn",0),"audio startup setting setup failed");
        g_session.LoadSourceCommandOptions();ApplySavedAudioSettings();
        g_session.m_cmdOnOffSet.clear();g_windowMgr.m_tipOfTheDayWnd=nullptr;
        g_windowMgr.InitializeSourceCommandFlags();
        Require(g_session.m_cmdOnOffSet.size()==23 && g_session.m_cmdOnOffSet[0]==std::pair<int,int>{0,7}
            && g_session.m_cmdOnOffSet[1]==std::pair<int,int>{0,8} && g_session.m_cmdOnOffSet[2]==std::pair<int,int>{0,11}
            && g_session.m_cmdOnOffSet[4]==std::pair<int,int>{0,42} && g_session.m_cmdOnOffSet[7]==std::pair<int,int>{1,108}
            && g_session.m_cmdOnOffSet[13]==std::pair<int,int>{1,53},"actual boot bridge did not capture persisted session/audio owners");
        const auto firstSnapshot=g_session.m_cmdOnOffSet;
        restoreFlags.ownedTip=new UIWindow();restoreFlags.ownedTip->Create(1,1);restoreFlags.ownedTip->SetShow(0);
        g_windowMgr.m_tipOfTheDayWnd=restoreFlags.ownedTip;g_windowMgr.m_children.push_back(restoreFlags.ownedTip);
        CAudio::GetInstance()->SetBgmPaused(false);g_isSoundOn=-4;g_session.m_isEffectOn=true;
        g_windowMgr.InitializeSourceCommandFlags();
        Require(g_session.m_cmdOnOffSet.size()==46 && std::equal(firstSnapshot.begin(),firstSnapshot.end(),g_session.m_cmdOnOffSet.begin())
            && g_session.m_cmdOnOffSet[23]==std::pair<int,int>{1,7} && g_session.m_cmdOnOffSet[24]==std::pair<int,int>{1,8}
            && g_session.m_cmdOnOffSet[25]==std::pair<int,int>{-4,11} && g_session.m_cmdOnOffSet[27]==std::pair<int,int>{1,42},
            "reinitialization must append and tip presence must count even when hidden");
        g_windowMgr.DeleteWindow(restoreFlags.ownedTip);restoreFlags.ownedTip=nullptr;
        Require(!g_windowMgr.m_tipOfTheDayWnd,"tip owner deletion retained stale startup pointer");
        restoreFlags.ownedCommands=new UIWindow();restoreFlags.ownedCommands->Create(1,1);
        g_windowMgr.m_commandListWnd=restoreFlags.ownedCommands;g_windowMgr.m_children.push_back(restoreFlags.ownedCommands);
        g_windowMgr.AddWindowFront(restoreFlags.ownedCommands);
        Require(g_windowMgr.m_commandListWnd==restoreFlags.ownedCommands,"front-ordering incorrectly cleared command-list owner");
        g_windowMgr.DeleteWindow(restoreFlags.ownedCommands);restoreFlags.ownedCommands=nullptr;
        Require(!g_windowMgr.m_commandListWnd,"command-list deletion retained stale owner");
        std::cout<<"{\"case_id\":\"source_command_flags\",\"contract_id\":\"PK09.21\",\"all23_defaults_and_live_values\":true,\"actual_boot_bridge\":true,\"append_not_replace\":true,\"owner_lifetime\":true,\"outcome\":\"PASS\"}\n";
    }
