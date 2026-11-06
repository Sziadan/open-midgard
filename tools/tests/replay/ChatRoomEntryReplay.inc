#include "ui/UIChatRoomEntry.h"
#include "ui/UiScale.h"
#include "render/Renderer.h"

void ChatRoomEntryReplay()
{
    SetCase("chat_room_title_and_entry","PK09.21","managed-title-d9-password-wait",
        "D7 -> managed actor title -> native double-click/password -> actual D9 -> DB and owner teardown");
    g_windowMgr.RemoveAllWindows();g_windowMgr.ClearChatEvents();
    CWorld world;CGameActor owner;CGameMode mode;TestConnection receive;
    struct Restore {
        CWorld& world;CGameActor& owner;CGameMode& mode;
        SubChatWindowInfo geometry=g_windowMgr.m_chatRoomWindowInfo;
        CMode* current=SkillPacketReplayAccess::Current();int type=SkillPacketReplayAccess::CurrentType();
        int scale=GetConfiguredUiScalePercent(),width=g_renderer.m_width,height=g_renderer.m_height;
        float hpc=g_renderer.m_hpc,vpc=g_renderer.m_vpc,x=g_renderer.m_xoffset,y=g_renderer.m_yoffset;
        Restore(CWorld& w,CGameActor& a,CGameMode& m):world(w),owner(a),mode(m){}
        ~Restore(){g_windowMgr.RemoveAllWindows();world.UnregisterActor(&owner);mode.m_runtimeActors.clear();mode.m_world=nullptr;
            CRagConnection::instance()->Disconnect();SkillPacketReplayAccess::Restore(current,type);SetRuntimeUiScalePercent(scale);
            g_windowMgr.m_chatRoomWindowInfo=geometry;g_windowMgr.ClearChatEvents();
            g_renderer.m_width=width;g_renderer.m_height=height;g_renderer.m_hpc=hpc;g_renderer.m_vpc=vpc;g_renderer.m_xoffset=x;g_renderer.m_yoffset=y;}
    } restore(world,owner,mode);
    SetRuntimeUiScalePercent(100);SkillPacketReplayAccess::SetCurrent(&mode);mode.m_world=&world;
    owner.m_gid=0x12345678;owner.m_isVisible=1;owner.m_lastTlvertX=400;owner.m_lastTlvertY=300;
    world.RegisterActor(&owner);mode.m_runtimeActors[owner.m_gid]=&owner;
    auto& router=GetProductionGameModePacketRouter();router.Clear();RegisterDefaultGameModePacketHandlers(router);
    const auto dispatch=[&](const std::vector<u8>& frame){Require(receive.QueueBytes(frame),"room entry frame queue failed");
        const auto result=ProcessGameModePacketLoop(mode,receive,router,1);Require(result.packetsDispatched==1 && receive.BufferedBytes()==0,"room entry framing failed");};
    const auto listing=[&](u8 type,const std::string& title){dispatch(RoomListingFrame(0xd7,owner.m_gid,0xfedc1234,8,2,type,title));};
    listing(1,"Room <literal>");auto* title=owner.m_chatTitle;
    Require(title && title->m_w==200 && title->m_h==34 && title->GetTitle()=="Room <literal> (2/8)"
        && std::find(g_windowMgr.m_children.begin(),g_windowMgr.m_children.end(),title)!=g_windowMgr.m_children.end(),
        "D7 title is not a real managed owner or text/count formatting differs");
    Require(title->m_x==300 && title->m_y==300-110*g_windowMgr.GetChatClientRect().bottom/480,
        "receipt-time title placement differs from source");
    g_windowMgr.OnMouseMove(title->m_x+10,title->m_y+10);Require(mode.GetCursorAction()==2,"title did not select source cursor2");
    listing(1,std::string("X\0Y",3));Require(title->GetTitle()=="X (2/8)","source C-string title prefix lost count suffix");
    listing(3,"Arena");Require(owner.m_chatTitle==title && title->GetTitle()=="Arena","type3 title/count rule differs");
    auto* connection=CRagConnection::instance();connection->Disconnect();LoopbackPeer peer(*connection);
    title->OnLBtnDblClk(0,0);Require(!g_windowMgr.m_chatRoomPasswordWnd && SelectSocket(peer.peer.value,true,false,std::chrono::milliseconds(20))==0,
        "type3 title invented a join request");
    const std::vector<u8> publicJoin={0xd9,0,0x34,0x12,0xdc,0xfe,0,0,0,0,0,0,0,0};
    Require(mode.RequestEnterChatRoom(0xfedc1234,nullptr,100),"first room join was suppressed");
    Require(peer.ReceiveExact(14)==publicJoin && mode.m_isWaitingEnterRoom && mode.m_reqTickChatRoom==100 && !g_windowMgr.m_chatRoomWnd,
        "D9 bytes, wait ownership or lack of optimistic room creation differ");
    Require(!mode.RequestEnterChatRoom(0xfedc1234,nullptr,1100),"1000ms strict wait boundary changed");
    Require(mode.RequestEnterChatRoom(0xfedc1234,nullptr,1101),"expired join wait did not permit retry");
    Require(peer.ReceiveExact(14)==publicJoin,"retry D9 differs");
    mode.m_isWaitingEnterRoom=1;mode.m_reqTickChatRoom=0xfffffff0u;
    Require(!mode.RequestEnterChatRoom(0xfedc1234,nullptr,0x3d8u),"wrap-safe1000ms boundary changed");
    Require(mode.RequestEnterChatRoom(0xfedc1234,nullptr,0x3d9u),"wrapped elapsed1001ms did not expire wait");
    Require(peer.ReceiveExact(14)==publicJoin,"wrapped retry bytes differ");
    dispatch({0xda,0,3});Require(!mode.m_isWaitingEnterRoom,"unknown refusal did not clear join wait");
    listing(1,"Public");g_windowMgr.OnLBtnDblClk(title->m_x+20,title->m_y+10);g_windowMgr.OnLBtnUp(0,0);
    Require(peer.ReceiveExact(14)==publicJoin && !g_windowMgr.m_chatRoomPasswordWnd,"public title did not dispatch real D9");
    dispatch({0xda,0,3});listing(2,"Full event");
    auto fullInfo=title->m_roomInfo;fullInfo.numPeople=8;title->SetInfo(fullInfo);title->OnLBtnDblClk(0,0);
    Require(peer.ReceiveExact(14)==publicJoin,"type2 or full room was incorrectly blocked by the client");
    dispatch({0xda,0,3});listing(0,"Private");
    g_windowMgr.OnLBtnDblClk(title->m_x+20,title->m_y+10);g_windowMgr.OnLBtnUp(0,0);
    auto* password=g_windowMgr.m_chatRoomPasswordWnd;const auto client=g_windowMgr.GetChatClientRect();
    Require(password && password->GetRoomId()==0xfedc1234 && password->m_w==176 && password->m_h==46
        && password->m_x==(client.right-640)/2+200 && password->m_y==200*client.bottom/480
        && !g_windowMgr.CanRouteQtChatInput() && g_windowMgr.m_editWindow==password,"private title password owner/geometry/focus differ");
    password->SetInputText("123456789");Require(password->GetInputText()=="12345678","password byte limit differs");
    g_windowMgr.OnKeyDown(VK_ESCAPE);Require(g_windowMgr.m_chatRoomPasswordWnd==password,"source password cancel117 was not inert");
    g_windowMgr.OnKeyDown(VK_RETURN);auto privateJoin=publicJoin;
    for(int i=0;i<8;++i)privateJoin[6+i]=static_cast<u8>('1'+i);
    Require(!g_windowMgr.m_chatRoomPasswordWnd && !g_windowMgr.m_editWindow && peer.ReceiveExact(14)==privateJoin,
        "password Enter did not close on dispatch or preserve fixed8 bytes");
    g_windowMgr.m_captureWindow=title;g_windowMgr.m_lastHitWindow=title;
    dispatch(RoomEntryFrame(0xfedc1234,{{0,"Owner"},{1,"Guest"}}));
    Require(!owner.m_chatTitle && g_windowMgr.m_chatRoomWnd && !g_windowMgr.m_captureWindow && !g_windowMgr.m_lastHitWindow,
        "DB title-to-room lifetime retained a deleted actor title/capture");
    Require(!mode.RequestEnterChatRoom(0xfedc1234,nullptr,5000),"active room allowed a second join");
    listing(0,"Pending other private join");title=owner.m_chatTitle;title->OnLBtnDblClk(0,0);
    password=g_windowMgr.m_chatRoomPasswordWnd;Require(password,"could not create a retained password owner");
    auto* room=g_windowMgr.m_chatRoomWnd;room->Move(0,0);const auto edit=room->GetLayout().edit;
    g_windowMgr.OnLBtnDown(edit.left+5,edit.top+5);g_windowMgr.OnLBtnUp(0,0);g_windowMgr.OnChar('r');
    Require(room->GetInputText()=="r" && password->GetInputText().empty(),"unfocused password stole the room editor's input");
    g_windowMgr.EnsureChatWindowVisible();auto* main=g_windowMgr.m_chatWnd;main->Move(0,300);
    g_windowMgr.m_editWindow=password;g_windowMgr.OnLBtnDown(main->m_x+100,main->m_y+main->m_h-10);g_windowMgr.OnLBtnUp(0,0);
    Require(g_windowMgr.m_editWindow!=password,"main click retained password input ownership");
    Require(!password->SubmitInput() && !g_windowMgr.m_chatRoomPasswordWnd && g_windowMgr.m_chatRoomWnd==room,
        "active-room join guard changed ownership or retained password dialog");
    g_windowMgr.RemoveAllWindows();listing(1,"Projected");title=owner.m_chatTitle;
    g_renderer.m_width=640;g_renderer.m_height=480;g_renderer.m_hpc=g_renderer.m_vpc=400;g_renderer.m_xoffset=320;g_renderer.m_yoffset=240;
    matrix view{};view.m[0][0]=view.m[1][1]=view.m[2][2]=view.m[3][3]=1;view.m[3][2]=100;
    owner.m_pos={0,0,0};world.ProcessActorChatRoomTitles(view);
    Require(title->m_x==220 && title->m_y==167 && owner.m_lastTlvertX==320 && owner.m_lastTlvertY==240,
        "projected room title anchor/normalized pixel ratio differs from source");
    owner.m_pos.x=10;world.ProcessActorChatRoomTitles(view);Require(title->m_x==260,"room title did not follow actor movement");
    owner.m_pos={0,0,0};g_renderer.m_height=960;world.ProcessActorChatRoomTitles(view);
    Require(title->m_y==195,"source first-use screen-offset cache was recomputed after resize");g_renderer.m_height=480;
    owner.m_isVisible=0;world.ProcessActorChatRoomTitles(view);Require(!title->m_show,"hidden actor retained a visible title");
    owner.m_isVisible=1;owner.m_pos.z=-200;world.ProcessActorChatRoomTitles(view);Require(!title->m_show,"behind-camera actor retained a visible title");
    owner.m_pos={0,0,0};SetRuntimeUiScalePercent(125);world.ProcessActorChatRoomTitles(view);
    Require(title->m_x==156 && title->m_y==130 && title->m_show,"125% title anchor did not share logical UI coordinates");
    SetRuntimeUiScalePercent(100);g_windowMgr.RemoveAllWindows();Require(!owner.m_chatTitle,"bulk UI teardown left an actor dangling title");
    owner.m_pos={20,0,0};world.ProcessActorChatRoomTitles(view);
    Require(owner.m_lastTlvertX==400 && owner.m_lastTlvertY==240,"untitled actor projection cache was not maintained for later D7 placement");
    listing(1,"Late title");Require(owner.m_chatTitle->m_x==300,"new D7 title ignored the maintained actor screen cache");
    g_windowMgr.RemoveAllWindows();
    auto* transient=new CGameActor;ro::chat::RoomInfo info;info.title="Transient";
    transient->SendMsg(nullptr,32,reinterpret_cast<msgparam_t>(&info),0,0);auto* transientTitle=transient->m_chatTitle;
    delete transient;Require(std::find(g_windowMgr.m_children.begin(),g_windowMgr.m_children.end(),transientTitle)==g_windowMgr.m_children.end(),
        "actor teardown retained its managed title");
    connection->Disconnect();mode.m_isWaitingEnterRoom=0;
    Require(!mode.RequestEnterChatRoom(9,"secret",6000) && mode.m_isWaitingEnterRoom,"immediate failed send lost source waiting state");
    auto* failedPassword=static_cast<UIChatRoomPasswordWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_CHATROOMPASSWORD));
    failedPassword->SetRoomId(9);failedPassword->SetInputText("secret");
    Require(!failedPassword->SubmitInput() && !g_windowMgr.m_chatRoomPasswordWnd,"guard rejection retained source password dialog");
    std::cout<<"{\"case_id\":\"chat_room_title_and_entry\",\"contract_id\":\"PK09.21\",\"actual_d9_wire\":true,\"wait_boundary\":true,\"actor_manager_lifetime\":true,\"projected_anchor\":true,\"outcome\":\"PASS\"}\n";
}
