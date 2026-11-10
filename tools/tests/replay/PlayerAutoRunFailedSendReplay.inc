    {
        auto* oldWorld=mode.m_world;const auto oldActors=mode.m_runtimeActors;const auto oldPending=mode.m_skillUseInfo;
        const auto oldCommerce=ro::commerce::state;const bool oldGood=g_isGoodSkillHanging;
        const auto oldChase=std::make_tuple(mode.m_skillChaseTargetGid,mode.m_skillChaseSkillId,mode.m_skillChaseSkillLevel,
            mode.m_skillChaseRange,mode.m_hasSkillChase,mode.m_hasGroundSkillChase,mode.m_hasTouchSkillChase);
        CWorld world;CPlayer player;CPc target;C3dAttr attr;
        attr.m_width=attr.m_height=64;attr.m_zoom=10;attr.m_cells.resize(64*64);
        world.m_player=&player;world.m_attr=&attr;player.m_pos={75,0,0};player.m_stateId=0;
        target.m_gid=0x77000001;target.m_pos={100,0,0};target.m_job=1002;world.RegisterActor(&target);
        mode.m_world=&world;mode.m_runtimeActors.clear();mode.m_runtimeActors[target.m_gid]=&target;
        struct RestoreAutoRunFailure {std::function<void()> fn;~RestoreAutoRunFailure(){fn();}} restore{[&]{
            world.UnregisterActor(&target);world.m_player=nullptr;world.m_attr=nullptr;
            mode.m_world=oldWorld;mode.m_runtimeActors=oldActors;mode.m_skillUseInfo=oldPending;
            ro::commerce::state=oldCommerce;g_isGoodSkillHanging=oldGood;
            std::tie(mode.m_skillChaseTargetGid,mode.m_skillChaseSkillId,mode.m_skillChaseSkillLevel,
                mode.m_skillChaseRange,mode.m_hasSkillChase,mode.m_hasGroundSkillChase,mode.m_hasTouchSkillChase)=oldChase;
        }};
        ro::commerce::state.vendingActive=false;
        player.SendMsg(nullptr,41,target.m_gid,28,5);player.SendMsg(nullptr,90,2,0,0);
        mode.m_skillUseInfo={30,7,11,2};g_isGoodSkillHanging=true;
        PLAYER_SKILL_INFO row{};row.m_isValid=1;row.SKID=40;row.type=4;row.level=2;
        mode.SendMsg(126,reinterpret_cast<msgparam_t>(&row),2,0);
        Require(mode.m_skillUseInfo.useType==0 && mode.m_skillUseInfo.id==30 && mode.m_skillUseInfo.level==7
            && mode.m_skillUseInfo.attackRange==11 && g_isGoodSkillHanging && player.m_proceedType.type==3,
            "failed source126 send rolled back null79 reset or cancelled old proceed");
        ReplayPumpSkillChase(mode);
        Require(player.m_proceedType.type==0 && !mode.m_hasSkillChase && player.m_skillId==28 && player.m_skillUseLevel==2,
            "failed ordinary42 dispatch did not retire13 while retaining stored skill fields");
    }
