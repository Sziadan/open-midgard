    {
        const auto pet=g_session.m_petStatus;
        const auto pendingSkill=mode.m_skillUseInfo;
        const auto roots=g_windowMgr.m_children;
        const auto inventory=g_session.GetInventoryItems();
        const auto check=[&](msgparam_t argument,u32 expected){
            Require(mode.SendMsg(162,argument,0,0)==1,"source capture sender failed");
            Require(peer.ReceiveExact(6)==std::vector<u8>({0x9f,0x01,static_cast<u8>(expected),
                static_cast<u8>(expected>>8),static_cast<u8>(expected>>16),static_cast<u8>(expected>>24)}),
                "mode162 did not emit exact little-endian019F/6 target frame");
            Require(g_session.m_petStatus.job==pet.job && g_session.m_petStatus.eggIndex==pet.eggIndex
                && g_session.m_petStatus.actorId==pet.actorId && mode.m_skillUseInfo.id==pendingSkill.id
                && mode.m_skillUseInfo.level==pendingSkill.level && g_windowMgr.m_children==roots
                && g_session.GetInventoryItems().size()==inventory.size(),
                "capture sender changed pet/targeting/owner/inventory state");
        };
        check(0,0);check(0x12345678,0x12345678u);check(-1,0xffffffffu);
        check(static_cast<msgparam_t>(0x80000001u),0x80000001u);
        if constexpr(sizeof(msgparam_t)>4)check(static_cast<msgparam_t>(0x123456789abcdef0ULL),0x9abcdef0u);
    }
