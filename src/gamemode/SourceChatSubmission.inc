// Original caller47/11 and ProcessTalkType578F10 ownership. Included in
// GameMode.cpp after the existing request helpers, inside the same translation unit.
namespace {
struct SourceEmotionRoute {int type,emotion;};
#include "core/SourceChatEmotions.generated.inc"
std::string SourceChatLabel(int id,const char* fallback)
{
    ro::UiMessageStrings table;table.Load();return table.Get(id,fallback);
}
void SourceChatNotice(int id,const char* fallback,int color)
{
    const auto text=SourceChatLabel(id,fallback);
    g_windowMgr.SendMsg(1,reinterpret_cast<msgparam_t>(text.c_str()),color);
}
}

u32 CGameMode::GetSourceChatTick() const
{
    return m_sourceChatInputServices && m_sourceChatInputServices->ticks?m_sourceChatInputServices->ticks():timeGetTime();
}

bool CGameMode::SubmitSourceChat(const std::string& input,const std::string& target,UIWindow* origin)
{
    const std::string text(input.c_str());
    if(text.empty())return false;
    // Whitespace in a source target is significant. Slash only bypasses the
    // target when it is the first original byte, before any trimming.
    if(!target.empty() && text[0]!='/'){
        g_windowMgr.SetChatMsg(text);
        return SendMsg(11,reinterpret_cast<msgparam_t>(target.c_str()),0,0)!=0;
    }
    ro::UiMessageStrings messages;if(text[0]=='/')messages.Load();
    ro::chat::SourceChatParseContext context;
    if(const auto alias=messages.Find(698))context.minEffectAlias=*alias;
    if(text[0]=='/'){
        context.soundVolume=static_cast<int>(std::lround(CAudio::GetInstance()->GetVolume()*127.0f));
        context.musicVolume=CAudio::GetInstance()->GetBgmVolume();
    }
    auto result=ro::chat::ParseSourceChat(text,context,[&](std::string_view command){
        return ro::chat::LookupSourceTalkType(command,[&](int id){return messages.Find(id);},context.encoding,g_serverType==ServerInstantEvent);
    });
    if(result.parameterOverflow){DbgLog("[SourceChat] rejected unsafe source parameter overflow\n");return false;}
    // These effects belong to GetTalkType, before caller message assignment
    // and before ProcessTalkType's command flag/list prelude.
    if(result.toggleMinEffect){
        g_session.m_isMinEffect=!g_session.m_isMinEffect;
        g_session.m_isMinEffectOld=g_session.m_isMinEffect;
        SourceChatNotice(g_session.m_isMinEffect?687:688,g_session.m_isMinEffect?"Minimum effects on":"Minimum effects off",6619135);
    }
    if(result.statIndex>=0)m_SiyeonStatus[static_cast<std::size_t>(result.statIndex)]=result.statValue;
    if(result.messageOverride)g_windowMgr.SetChatMsg(*result.messageOverride);
    if(result.messageOffset>=0)g_windowMgr.SetChatMsg(text.substr(static_cast<std::size_t>(result.messageOffset)));
    if(auto* privateWindow=dynamic_cast<UIWhisperWnd*>(origin)){
        // The private caller consumes these types before mode47.
        if(result.type==24){g_windowMgr.DeleteWindow(privateWindow);return true;}
        if(result.type==51)return privateWindow->WriteChatToFile();
        if(result.type==115)return privateWindow->WriteChatToFile(true);
    }
    if(dynamic_cast<UIChatRoomWnd*>(origin)){
        if(result.type==0)result.type=50;
        else if(result.type==51)result.type=52;
        else if(result.type==115)result.type=116;
    }
    // A -1 offset retains the old manager message and STILL dispatches47.
    return SendMsg(47,result.type,reinterpret_cast<msgparam_t>(&result.parameters),0)!=0;
}

bool CGameMode::SendSourceWhisper(const std::string& target,const std::string& text)
{
    g_windowMgr.SetChatMsg(text);
    return SendMsg(11,reinterpret_cast<msgparam_t>(target.c_str()),0,0)!=0;
}

bool CGameMode::SendBufferedSourceChat(int operation,const std::string& target)
{
    const bool whisper=operation==11;
    std::string text(g_windowMgr.GetChatMsg());
    if(text.empty() || g_session.IsStatusIconActive(107) || (operation==142 && !g_session.m_guildWindow.id))return false;
    if(whisper && text.size()>222)text.resize(222);
    auto& filter=ro::GetMannerFilter();filter.EnsureLoaded();
    const auto reject=[&](int id,const char* fallback){
        g_windowMgr.SendMsg(3,0,0,0);SourceChatNotice(id,fallback,255);g_windowMgr.ClearChatMsg();return false;
    };
    if(filter.IsBadUtf8Sentence(text))return reject(50,"That message cannot be sent.");
    if(!whisper || m_hiddenWhisperAcknowledgements==0){
        if(m_sourceLastSubmittedChat==text){if(m_sourceSameChatRepeatCount<INT_MAX)++m_sourceSameChatRepeatCount;}
        else m_sourceSameChatRepeatCount=0;
    }
    if((!whisper || !g_session.m_onAutoMSG) && g_serverType!=ServerInstantEvent
        && g_serviceType!=ServiceGermany && m_sourceSameChatRepeatCount>=2)
        return reject(49,"Please do not repeat the same sentence.");
    m_sourceLastSubmittedChat=text;
    if(whisper){
        m_lastWhisperName=std::string(target.c_str());
        if(m_lastWhisperName==g_session.GetPlayerName()){g_windowMgr.ClearChatMsg();return false;}
        m_lastWhisper=text;
        g_windowMgr.ClearChatMsg();
        if(text.size()>=222)return false;
        return SendWhisperChatMessage(m_lastWhisperName,text);
    }
    const auto& profile=ro::net::GetActiveMapGameplaySendProfile();
    const auto payload=std::string(g_session.GetPlayerName())+" : "+text;
    // Source clears before the length guard and network attempt.
    g_windowMgr.ClearChatMsg();
    if(operation==142 && payload.size()>=0x3fcu)return false;
    if(operation==6)return SendGlobalChatMessage(g_session.GetPlayerName(),text);
    const u16 opcode=operation==72?profile.partyMessage:operation==142?profile.guildMessage:profile.battleMessage;
    const std::size_t length=payload.size()+5;
    if(length>0xffffu)return false;
    std::vector<u8> packet(length,0);packet[0]=static_cast<u8>(opcode);packet[1]=static_cast<u8>(opcode>>8);
    packet[2]=static_cast<u8>(length);packet[3]=static_cast<u8>(length>>8);std::copy(payload.begin(),payload.end(),packet.begin()+4);
    return CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(packet.data()),static_cast<int>(packet.size()));
}

msgresult_t CGameMode::ProcessSourceTalk(int type,const std::array<std::string,3>& parameters)
{
    for(auto& entry:g_session.m_cmdOnOffSet)if(entry.second==type){entry.first=entry.first==0;break;}
    if(g_windowMgr.m_commandListWnd)g_windowMgr.m_commandListWnd->Invalidate();
    if(g_serviceType==ServiceVietnam && type==6)type=3;
    if(type==0 || type==50){
        auto text=std::string(g_windowMgr.GetChatMsg());
        const auto keys=m_sourceChatInputServices && m_sourceChatInputServices->readModifiers
            ?m_sourceChatInputServices->readModifiers()
            :ro::chat::SourceChatModifiers{(GetAsyncKeyState(VK_CONTROL)&0x8000)!=0,(GetAsyncKeyState(VK_MENU)&0x8000)!=0};
        const int operation=ro::chat::SourceChatRoute(type,text,g_session.m_talkType,g_session.m_isBattleChat,
            g_session.GetNumParty()!=0,g_session.m_guildWindow.id!=0,keys);
        g_windowMgr.SetChatMsg(text);
        return operation?SendMsg(operation,0,0,0):0;
    }
    for(const auto& route:kSourceEmotionRoutes)if(route.type==type){
        return SendMsg(33,route.emotion>=0?route.emotion:std::rand()%6+58,0,0);
    }
    switch(type){
    case 2: case 20: case 136:return 1; // Selected jump table returns without an action.
    case 3:SourceChatNotice(95,"Invalid command.",65535);return 0;
    case 12: case 13: {
        const int value=(std::max)(0,(std::min)(127,static_cast<int>(ro::chat::SourceChatAtoi(parameters[0]))));
        if(type==12)CAudio::GetInstance()->SetVolume(static_cast<float>(value)/127.0f);
        else CAudio::GetInstance()->SetBgmVolume(value);
        const auto label=SourceChatLabel(type==12?25:26,type==12?"Sound volume":"BGM volume");
        const auto notice=label+" "+std::to_string(value);
        g_windowMgr.SendMsg(1,reinterpret_cast<msgparam_t>(notice.c_str()),6619135);
        if(g_windowMgr.m_optionWnd)g_windowMgr.m_optionWnd->Invalidate();
        return 1;
    }
    case 21: case 22: {
        // Both virtual CPlayer messages30/31 use the same source toggle.
        if(!m_world || !m_world->m_player)return 0;
        auto* player=m_world->m_player;
        if((player->m_effectState&2)!=0 || (player->m_stateId!=0 && player->m_stateId!=6))return 0;
        return SendActionRequestPacket(player->m_proceedTargetGid,player->m_stateId==6?3:2)?1:0;
    }
    case 23:return g_windowMgr.OpenChatRoomMake(false)?1:0;
    case 24:return SendMsg(53,0,0,0);
    case 26:return SendMsg(GameMsg_RequestPartyCreate,reinterpret_cast<msgparam_t>(parameters[0].c_str()),0,0);
    case 27:return SendMsg(67,0,0,0);
    case 52:return g_windowMgr.m_chatRoomWnd && g_windowMgr.m_chatRoomWnd->WriteChatToFile()?1:0;
    case 55: {
        const auto& name=parameters[0];
        if(name.empty() || name.size()>23 || !m_world || !m_world->m_player || !g_session.m_gid)return 0;
        const auto packet=ro::guild::CreatePacket(g_session.m_gid,name);
        return CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(packet.data()),static_cast<int>(packet.size()))?1:0;
    }
    case 57:g_windowMgr.SetChatMsg(parameters[0]);return SendMsg(142,0,0,0);
    case 109: case 110: {
        if(!m_world || !m_world->m_player || !std::isfinite(m_world->m_player->m_roty))return 0;
        auto* actor=m_world->m_player;auto* pc=dynamic_cast<CPc*>(actor);if(!pc)return 0;
        if(type==109)pc->m_headDir=actor->m_curMotion=actor->m_curMotion!=2?2:1;
        else {actor->m_roty+=45.0f;if(actor->m_roty>=360.0f)actor->m_roty-=360.0f;}
        const int body=static_cast<int>((static_cast<long long>(actor->m_roty)+180)/45)%8;
        g_session.SetPlayerPosDir(g_session.m_playerPosX,g_session.m_playerPosY,body&7);
        pc->InvalidateBillboard();
        const bool sent=SendMsg(152,body,pc->m_headDir,0)!=0;
        if(type==109 && actor->m_stateId==6){
            std::move(std::begin(m_doritime)+1,std::end(m_doritime),std::begin(m_doritime));
            const u32 now=GetSourceChatTick();const u32 elapsed=now-m_doritime[0];m_doritime[5]=now;
            if(elapsed<3000 && elapsed>1500){SendMsg(183,0,0,0);std::fill(std::begin(m_doritime),std::end(m_doritime),0);}
        }
        return sent?1:0;
    }
    case 104:return ToggleSourceChatAutoSave()?1:0;
    case 152:return ToggleSourceAutoMessage()?1:0;
    default:
        // Retained open cases are tracked as missing implementations. Never
        // leak a recognized/unknown slash command into ordinary global chat.
        DbgLog("[SourceChat] command type=%d awaits source action integration\n",type);return 0;
    }
}
