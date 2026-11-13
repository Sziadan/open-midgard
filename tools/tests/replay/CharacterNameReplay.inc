std::string ReplayResolveHoveredActorName(CGameMode& mode, CGameActor& actor);

void ActorNameRequestReplyReplay()
{
    SetCase("actor_name_request_reply", "PK46.02", "selected-008c-0095-0195",
        "production hover label and sender -> loopback -> actor-name handler");
    constexpr u32 npcId = 0x12345678;
    constexpr u32 playerId = npcId + 1;
    auto* connection = CRagConnection::instance();
    connection->Disconnect();
    CGameMode mode;
    auto npcOwner = std::make_unique<CGameActor>();
    CGameActor& npc = *npcOwner;
    npc.m_gid = npcId;
    npc.m_objectType = 6;
    CGameActor player;
    player.m_gid = playerId;
    player.m_isPc = 1;
    mode.m_runtimeActors.emplace(npcId, &npc);
    mode.m_runtimeActors.emplace(playerId, &player);
    Require(ReplayResolveHoveredActorName(mode, npc) == "NPC" && mode.m_actorNameByGIDReqTimer.empty(),
        "offline hover armed the actor name retry timer");
    LoopbackPeer peer(*connection);
    const auto expectRequest = [&](CGameActor& actor, u8 firstIdByte) {
        ReplayResolveHoveredActorName(mode, actor);
        auto bytes = peer.ReceiveExact(11);
        std::fill_n(bytes.begin() + 2, 5, 0);
        Require(bytes == std::vector<u8>{0x8c, 0, 0, 0, 0, 0, 0, firstIdByte, 0x56, 0x34, 0x12},
            "hover name request differs from selected registry");
    };
    expectRequest(npc, 0x78);
    ReplayResolveHoveredActorName(mode, npc);
    Require(SelectSocket(peer.peer.value, true, false, std::chrono::milliseconds(30)) == 0,
        "hover name request flooded during the cooldown");
    mode.m_actorNameByGIDReqTimer[npcId] = GetTickCount() - 1000u;
    expectRequest(npc, 0x78);
    expectRequest(player, 0x79);

    TestConnection input;
    CGameModePacketRouter router;
    RegisterDefaultGameModePacketHandlers(router);
    const auto dispatch = [&](const std::vector<u8>& frame) {
        Require(input.QueueBytes(frame)
            && ProcessGameModePacketLoop(mode, input, router, 1).packetsDispatched == 1,
            "actor name reply did not pass production framing and routing");
    };
    std::vector<u8> npcReply(30, 0);
    const std::array<u8, 6> npcHeader{0x95, 0, 0x78, 0x56, 0x34, 0x12};
    std::copy(npcHeader.begin(), npcHeader.end(), npcReply.begin());
    std::memcpy(npcReply.data() + 6, "Guide#internal", 14);
    mode.m_characterNameCache[npcId] = "Character";
    dispatch(npcReply);
    Require(ReplayResolveHoveredActorName(mode, npc) == "Guide"
        && mode.m_actorNameByGIDReqTimer.count(npcId) == 0
        && mode.m_characterNameCache.at(npcId) == "Character",
        "NPC reply did not resolve the hover label independently of character names");

    std::vector<u8> playerReply(102, 0);
    const std::array<u8, 6> playerHeader{0x95, 1, 0x79, 0x56, 0x34, 0x12};
    std::copy(playerHeader.begin(), playerHeader.end(), playerReply.begin());
    std::memcpy(playerReply.data() + 6, "Trader", 6);
    std::memcpy(playerReply.data() + 30, "Friends", 7);
    std::memcpy(playerReply.data() + 54, "Guild", 5);
    std::memcpy(playerReply.data() + 78, "Officer", 7);
    dispatch(playerReply);
    const auto& entry = mode.m_actorNameListByGID.at(playerId);
    Require(entry.name == "Trader" && entry.partyName == "Friends" && entry.guildName == "Guild"
        && entry.guildTitle == "Officer" && mode.m_actorNameByGIDReqTimer.count(playerId) == 0,
        "full actor name reply did not populate the player label and affiliations");
    Require(SelectSocket(peer.peer.value, true, false, std::chrono::milliseconds(30)) == 0,
        "resolved actor label sent another name request");
    dispatch({0x80, 0, 0x78, 0x56, 0x34, 0x12, 0});
    mode.m_deferredActorDeletes.clear();
    Require(mode.m_runtimeActors.count(npcId) == 0 && mode.m_actorNameListByGID.count(npcId) == 0,
        "actor removal did not clear the NPC and its name");
    dispatch(npcReply);
    Require(mode.m_runtimeActors.count(npcId) == 0 && mode.m_actorNameListByGID.count(npcId) == 0
        && mode.m_actorNameList.count(npcId) == 0, "late name reply recreated a vanished actor");
    mode.m_runtimeActors.clear();
    connection->Disconnect();
}

void CharacterNameReplyIsolationReplay()
{
    SetCase("character_name_reply_isolation", "PK45.05", "selected-20080910-0194-character-id",
        "ProcessGameModePacketLoop -> production router -> character-name acknowledgement");
    constexpr u32 cid=0x12345678;
    CWorld world;
    CGameActor actor;
    CGameMode mode;
    struct Restore {
        CGameMode& mode;
        ~Restore() { mode.m_runtimeActors.clear();mode.m_world=nullptr; }
    } restore{mode};
    mode.m_world=&world;actor.m_gid=cid;mode.m_runtimeActors[cid]=&actor;
    mode.m_actorNameList[cid].name="Visible actor";
    mode.m_actorNameListByGID[cid].name="Visible actor";
    mode.m_actorNameByGIDReqTimer[cid]=42;
    TestConnection connection;
    auto& router=GetProductionGameModePacketRouter();router.Clear();RegisterDefaultGameModePacketHandlers(router);
    std::vector<u8> frame={0x94,1,0x78,0x56,0x34,0x12,'P','r','o','d','u','c','e','r',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    Require(connection.QueueBytes({frame.begin(),frame.end()-1}),"could not queue fragmented character reply");
    auto result=ProcessGameModePacketLoop(mode,connection,router,1);
    Require(result.packetsProcessed==0 && connection.BufferedBytes()==29,"partial character reply mutated framing state");
    Require(connection.QueueBytes({frame.back()}),"could not complete character reply");
    result=ProcessGameModePacketLoop(mode,connection,router,1);
    Require(result.packetsProcessed==1 && result.packetsDispatched==1 && connection.BufferedBytes()==0,
        "character reply missed production framing/router");
    Require(mode.m_actorNameList[cid].name=="Visible actor" && mode.m_actorNameListByGID[cid].name=="Visible actor"
        && mode.m_actorNameByGIDReqTimer[cid]==42,"character-ID reply overwrote actor-ID cache or request timer");
    Require(mode.m_characterNameCache.at(cid)=="Producer","character-ID reply did not update its own cache");
    frame[2]=0x21;frame[3]=0x43;frame[4]=0x65;frame[5]=0x87;
    Require(connection.QueueBytes(frame),"could not queue unseen character reply");
    result=ProcessGameModePacketLoop(mode,connection,router,1);
    Require(result.packetsDispatched==1 && mode.m_runtimeActors.size()==1
        && mode.m_actorNameList.count(0x87654321)==0 && mode.m_actorNameListByGID.count(0x87654321)==0,
        "character-ID reply created a phantom world actor/cache");
    Require(mode.m_characterNameCache.at(0x87654321)=="Producer","unseen character name was not cached");
    // Empty/zero-ID replies cannot poison a good cache or alter actor state.
    frame[2]=0x78;frame[3]=0x56;frame[4]=0x34;frame[5]=0x12;
    std::fill(frame.begin()+6,frame.end(),0);
    Require(connection.QueueBytes(frame),"could not queue empty character reply");
    result=ProcessGameModePacketLoop(mode,connection,router,1);
    Require(mode.m_characterNameCache.at(cid)=="Producer","empty name erased a known character name");
    frame[2]=frame[3]=frame[4]=frame[5]=0;frame[6]='X';
    Require(connection.QueueBytes(frame),"could not queue zero-ID character reply");
    result=ProcessGameModePacketLoop(mode,connection,router,1);
    Require(mode.m_characterNameCache.count(0)==0 && mode.m_runtimeActors.size()==1,"zero-ID name reply created state");
    std::cout<<"{\"case_id\":\"character_name_reply_isolation\",\"contract_id\":\"PK45.05\",\"opcode\":\"0194\",\"outcome\":\"PASS\"}"<<std::endl;
}

void CharacterNameRequestReplyReplay()
{
    SetCase("character_name_request_reply", "PK45.05", "selected-20080910-00a2-15-cid11-0194",
        "CGameMode::ResolveCharacterName -> active builder -> CRagConnection::SendPacket; production receive loop");
    constexpr u32 cid=0x92345678;
    auto* connection=CRagConnection::instance();connection->Disconnect();
    CGameMode mode;
    Require(mode.ResolveCharacterName(cid).empty() && mode.m_characterNameReqTimer.empty(),
        "failed character lookup send suppressed a later retry");
    const auto& profile=ro::net::GetActiveMapGameplaySendProfile();
    Require(profile.characterNameRequest==0x00a2 && profile.characterNameRequestLength==15
        && profile.characterNameRequestIdOffset==11,"selected name lookup differs from final server registration");
    std::array<u8,15> guard;guard.fill(0xa5);int length=99;
    Require(!ro::net::BuildActiveCharacterNameRequestPacket(cid,guard.data(),14,&length) && length==0
        && std::all_of(guard.begin(),guard.end(),[](u8 b){return b==0xa5;}),"short name buffer was mutated/accepted");
    Require(!ro::net::BuildActiveCharacterNameRequestPacket(0,guard.data(),15,&length),"zero character ID was sent");
    LoopbackPeer peer(*connection);
    Require(mode.ResolveCharacterName(cid).empty() && mode.m_characterNameReqTimer.count(cid)==1,
        "unknown character did not enter pending state after a successful send");
    const auto captured=peer.ReceiveExact(15);
    Require(captured[0]==0xa2 && captured[1]==0 && captured[11]==0x78 && captured[12]==0x56
        && captured[13]==0x34 && captured[14]==0x92,"captured request differs from server opcode/length/CID offset");
    Require(mode.ResolveCharacterName(cid).empty(),"pending lookup invented a name");
    Require(SelectSocket(peer.peer.value,true,false,std::chrono::milliseconds(40))==0,"pending lookup flooded duplicate requests");
    // Expire the production timer without a wall-clock sleep, including wrap-safe subtraction.
    mode.m_characterNameReqTimer[cid]=GetTickCount()-2500u;
    Require(mode.ResolveCharacterName(cid).empty(),"retry invented a name");
    const auto retry=peer.ReceiveExact(15);
    Require(retry[0]==0xa2 && retry[11]==0x78 && retry[14]==0x92,"expired lookup did not retry selected request");
    TestConnection input;
    auto& router=GetProductionGameModePacketRouter();router.Clear();RegisterDefaultGameModePacketHandlers(router);
    const std::vector<u8> reply={0x94,1,0x78,0x56,0x34,0x92,'S','m','i','t','h',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    Require(input.QueueBytes(reply),"could not queue producer-name reply");
    const auto result=ProcessGameModePacketLoop(mode,input,router,1);
    Require(result.packetsDispatched==1 && mode.ResolveCharacterName(cid)=="Smith"
        && mode.m_characterNameReqTimer.count(cid)==0,"reply did not complete the matching character lookup");
    Require(SelectSocket(peer.peer.value,true,false,std::chrono::milliseconds(40))==0,"cached name emitted another request");
    Require(mode.m_runtimeActors.empty() && mode.m_actorNameList.empty() && mode.m_actorNameListByGID.empty(),
        "character lookup created actor state");
    connection->Disconnect();
    std::cout<<"{\"case_id\":\"character_name_request_reply\",\"contract_id\":\"PK45.05\",\"request_opcode\":\"00A2\",\"length\":15,\"character_id_offset\":11,\"captured_hex\":\""<<Hex(captured)
        <<"\",\"padding\":\"ignored bytes 2..10 vary with tick\",\"outcome\":\"PASS\"}"<<std::endl;
}
