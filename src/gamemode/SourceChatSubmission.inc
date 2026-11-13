// Original caller47/11 and ProcessTalkType578F10 ownership. Included in
// GameMode.cpp after the existing request helpers, inside the same translation unit.
#include "core/SourceLanguageCharset.h"
namespace {
struct SourceEmotionRoute {int type,emotion;};
#include "core/SourceChatEmotions.generated.inc"
#include "core/SourceChatHelpMessages.generated.inc"
std::string SourceChatLabel(int id,const char* fallback)
{
    ro::UiMessageStrings table;table.Load();return table.Get(id,fallback);
}
void SourceChatNotice(int id,const char* fallback,int color)
{
    const auto text=SourceChatLabel(id,fallback);
    g_windowMgr.SendMsg(1,reinterpret_cast<msgparam_t>(text.c_str()),color);
}
int InvertSourceCommandInt(int value)
{
    // Selected x86 SUB wraps modulo32, including nonboolean saved state.
    const u32 bits=1u-static_cast<u32>(value);s32 result;
    std::memcpy(&result,&bits,sizeof(result));return result;
}
s32 SourceWhereCoordinate(float position,int extent,int zoom)
{
    if(zoom==0)return 0;
    // Selected579A10: signed integer half, integer zoom rounded through float,
    // x87 quotient/add then a double spill before floor/__ftol2's low DWORD.
    const double value=static_cast<double>(static_cast<long double>(position)/static_cast<float>(zoom)+extent/2);
    const double integral=std::floor(value);
    if(!std::isfinite(integral) || integral< -9223372036854775808.0 || integral>=9223372036854775808.0)return 0;
    const u32 bits=static_cast<u32>(static_cast<s64>(integral));s32 result;std::memcpy(&result,&bits,sizeof(result));return result;
}
void RefreshSourceOptions()
{
    if(g_windowMgr.m_optionWnd)g_windowMgr.m_optionWnd->SendMsg(nullptr,60,0,0,0);
}

bool SendPrivacyConfiguration(u8 type, bool enabled)
{
    const u8 packet[] = {0xd8, 0x02, type, 0, 0, 0, static_cast<u8>(enabled), 0, 0, 0};
    return CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(packet), sizeof(packet));
}
}

u32 CGameMode::GetSourceChatTick() const
{
    return m_sourceChatInputServices && m_sourceChatInputServices->ticks?m_sourceChatInputServices->ticks():timeGetTime();
}

std::string CGameMode::GetSourceChatCharset() const
{
    if(!g_multiLang)return {};
    u16 language=0;
    if(m_sourceChatInputServices && m_sourceChatInputServices->languageId)language=m_sourceChatInputServices->languageId();
#if RO_PLATFORM_WINDOWS
    else language=static_cast<u16>(reinterpret_cast<std::uintptr_t>(GetKeyboardLayout(0)));
#endif
    return ro::chat::SourceCharsetPrefix(g_multiLang,language);
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
    if (text == "/refuserecall" || text == "/acceptrecall") {
        return SendPrivacyConfiguration(1, text == "/refuserecall");
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
        // Original multilingual wire text reserves three bytes before the219-byte copy.
        m_lastWhisper=g_multiLang?GetSourceChatCharset()+text.substr(0,219):text;
        g_windowMgr.ClearChatMsg();
        if(m_lastWhisper.size()>=222)return false;
        return SendWhisperChatMessage(m_lastWhisperName,m_lastWhisper);
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
    case 181: // /accept
    case 182: { // /refuse
        const u8 packet[] = {0xc8, 0x02, static_cast<u8>(type == 182)};
        return CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(packet), sizeof(packet));
    }
    case 184: // /eqopen
        return SendMsg(270, !g_session.m_showEquipmentToOthers, 0, 0);
    case 9: {
        if(!m_world || !m_world->m_player || !m_world->m_attr)return 0;
        const auto* attr=m_world->m_attr;const auto& position=m_world->m_player->m_pos;
        const int x=SourceWhereCoordinate(position.x,attr->m_width,attr->m_zoom);
        const int y=SourceWhereCoordinate(position.z,attr->m_height,attr->m_zoom);
        std::string filename(std::begin(m_rswName),std::find(std::begin(m_rswName),std::end(m_rswName),'\0'));
        std::string stem=filename;if(stem.size()>4)stem.resize(stem.size()-4);
        const auto text=g_session.GetMapName(filename.c_str())+"("+stem+") : "+std::to_string(x)+", "+std::to_string(y);
        g_windowMgr.SendMsg(3,0,0,0);g_windowMgr.SendMsg(1,reinterpret_cast<msgparam_t>(text.c_str()),6619135);
        if(g_serviceType!=ServiceGermany)g_windowMgr.SendMsg(20,reinterpret_cast<msgparam_t>(text.c_str()),6619135);
        return 1;
    }
    case 14: {
        g_windowMgr.SendMsg(3,0,0,0);
        ro::UiMessageStrings table;table.Load();
        const auto show=[&](int id){const auto text=table.Get(id,"");g_windowMgr.SendMsg(1,reinterpret_cast<msgparam_t>(text.c_str()),16777060);};
        for(int id:kSourceHelpMessageIds)show(id);
        // The source consumes a second/third shared RNG draw only when the
        // earlier branch fails. The first branch emits BOTH warning rows.
        if(std::rand()%10<4){show(kSourceHelpTailMessages[0]);show(kSourceHelpTailMessages[1]);}
        else if(std::rand()%5<2)show(kSourceHelpTailMessages[2]);
        else if(std::rand()%3<2)show(kSourceHelpTailMessages[3]);
        return 1;
    }
    case 6:return SendMsg(34,0,0,0);
    case 43:return SendMsg(83,0,0,0);
    case 145:return SendMsg(194,0,0,0);
    case 146:return SendMsg(195,0,0,0);
    case 151:return SendMsg(197,0,0,0);
    case 160:return SendMsg(201,0,0,0);
    case 168:return SendMsg(262,0,0,0);
    case 42:
        if(g_windowMgr.m_tipOfTheDayWnd){g_windowMgr.DeleteWindow(g_windowMgr.m_tipOfTheDayWnd);return 1;}
        return g_windowMgr.MakeWindow(UIWindowMgr::WID_TIPOFTHEDAYWND)?1:0;
    case 8: {
        g_session.m_isEffectOn=!g_session.m_isEffectOn;
        if(CWeather::IsSnowMap(m_rswName)){
            if(g_session.m_isEffectOn)g_Weather.LaunchSnow();else g_Weather.StopSnow();
        }
        g_windowMgr.SendMsg(3,0,0,0);
        SourceChatNotice(g_session.m_isEffectOn?23:24,"",6619135);
        const std::array<u8,6> packet{{0x1d,0x02,static_cast<u8>(g_session.m_isEffectOn?0:1),0,0,0}};
        return CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(packet.data()),static_cast<int>(packet.size()))?1:0;
    }
    case 7: {
        auto* audio=CAudio::GetInstance();audio->ToggleSourceBgm(m_streamFileName.c_str());
        g_windowMgr.SendMsg(3,0,0,0);
        SourceChatNotice(audio->IsBgmPaused()?31:32,"",6619135);RefreshSourceOptions();return 1;
    }
    case 11:
        g_isSoundOn=InvertSourceCommandInt(g_isSoundOn);
        SourceChatNotice(g_isSoundOn?27:28,"",6619135);RefreshSourceOptions();return 1;
    case 60: {
        g_session.m_fogOn=!g_session.m_fogOn;MakeFog(g_session.m_fogOn?1:0);
        const char* text=g_session.m_fogOn?"Fog On":"Fog Off";
        g_windowMgr.SendMsg(1,reinterpret_cast<msgparam_t>(text),6619135);RefreshSourceOptions();return 1;
    }
    case 53:case 105:case 106:case 107:case 108:case 112:case 113:
    case 119:case 120:case 134:case 135:case 140:case 147:case 164:case 166: {
        struct Toggle {int type;int CSession::* field;int on,off;bool inverse;};
        static const Toggle toggles[]={
            {53,&CSession::m_bMakeMissEffect,317,318,false},{105,&CSession::m_isNoShift,701,702,false},
            {106,&CSession::m_isAura,711,712,false},{107,&CSession::m_isShowNameTypeOld,722,723,false},
            {108,&CSession::m_isNoCtrl,717,718,false},{112,&CSession::m_isSkillFail,730,731,false},
            {113,&CSession::m_isNoTalkMsg,733,734,true},{119,&CSession::m_isQuickSpell,765,766,false},
            {120,&CSession::m_isQuickSpell2,768,769,false},{134,&CSession::m_isBattle,785,786,false},
            {135,&CSession::m_isNoTrade,805,806,false},{140,&CSession::m_isWindow,850,851,false},
            {147,&CSession::m_isNoTalkMsg2,908,909,true},{164,&CSession::m_isLogInOut,1044,1045,false},
            {166,&CSession::m_isShopping,1108,1109,false}};
        for(const auto& toggle:toggles)if(toggle.type==type){
            int& value=g_session.*toggle.field;
            value=type==107?(value==0):InvertSourceCommandInt(value);
            if(type==107)g_session.SetTextType(static_cast<u8>(value),static_cast<u8>(value));
            g_windowMgr.SendMsg(3,0,0,0);
            SourceChatNotice((value!=0)!=toggle.inverse?toggle.on:toggle.off,"",6619135);
            if(type==108)RefreshSourceOptions();return 1;
        }
        return 0;
    }
    case 114:case 121:case 137: {
        const int set1[]={112,107,108,140},q3[]={119,120},set2[]={112,107,108,119,120,140};
        const int* first=type==114?set1:type==121?q3:set2;const int count=type==114?4:type==121?2:6;
        for(int i=0;i<count;++i)g_modeMgr.SendMsg(47,first[i],0,0);
        return 1;
    }
    case 2: case 20: case 136:return 1; // Selected jump table returns without an action.
    case 3:SourceChatNotice(95,"Invalid command.",65535);return 0;
    case 12: case 13: {
        const int value=(std::max)(0,(std::min)(127,static_cast<int>(ro::chat::SourceChatAtoi(parameters[0]))));
        if(type==12)CAudio::GetInstance()->SetVolume(static_cast<float>(value)/127.0f);
        else CAudio::GetInstance()->SetBgmVolume(value);
        const auto label=SourceChatLabel(type==12?25:26,type==12?"Sound volume":"BGM volume");
        const auto notice=label+" "+std::to_string(value);
        g_windowMgr.SendMsg(1,reinterpret_cast<msgparam_t>(notice.c_str()),6619135);
        RefreshSourceOptions();
        return 1;
    }
    case 15: {
        const std::array<u8,2> packet{{0xd3,0}};
        return CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(packet.data()),static_cast<int>(packet.size()))?1:0;
    }
    case 16:case 17: {
        m_isWaitingWhisperSetting=1;
        const std::array<u8,3> packet{{0xd0,0,static_cast<u8>(type==17)}};
        const bool sent=CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(packet.data()),static_cast<int>(packet.size()));
        // Source clears local exclusions after the attempt, including send failure.
        if(type==17){g_session.m_exNameList.clear();g_session.m_exAidList.clear();}
        return sent?1:0;
    }
    case 18:case 19: {
        m_isWaitingWhisperSetting=1;
        m_refuseWhisperName.assign(parameters[0].c_str());
        if(m_refuseWhisperName.size()>24)return 0;
        // Fixed 24-byte field; deterministic zero padding replaces unused stack bytes.
        std::array<u8,27> packet{};packet[0]=0xcf;packet[26]=static_cast<u8>(type==19);
        std::copy(m_refuseWhisperName.begin(),m_refuseWhisperName.end(),packet.begin()+2);
        return CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(packet.data()),static_cast<int>(packet.size()))?1:0;
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
    case 51:case 52:case 115:case 116:
        g_windowMgr.SendMsg(type==51?6:type==52?7:type==115?17:18,0,0,0);return 1;
    case 55: {
        const auto& name=parameters[0];
        if(name.empty() || name.size()>23 || !m_world || !m_world->m_player || !g_session.m_gid)return 0;
        const auto packet=ro::guild::CreatePacket(g_session.m_gid,name);
        return CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(packet.data()),static_cast<int>(packet.size()))?1:0;
    }
    case 57:g_windowMgr.SetChatMsg(parameters[0]);return SendMsg(142,0,0,0);
    case 111: {
        if(!m_world || !m_world->m_player || !std::isfinite(m_world->m_player->m_roty))return 0;
        auto* player=m_world->m_player;
        player->m_roty-=45.0f;
        if(player->m_roty<0.0f)player->m_roty+=360.0f;
        // Selected57A6DB: truncate yaw through __ftol2, use its low DWORD,
        // ADD32(180), signed /45 and signed remainder8. Invalid conversion
        // yields x86 integer-indefinite (low DWORD0); avoid C++ overflow/UB.
        const double yaw=player->m_roty;
        const u32 low=yaw>=-9223372036854775808.0 && yaw<9223372036854775808.0
            ?static_cast<u32>(static_cast<s64>(yaw)):0u;
        const u32 bits=low+180u;s32 signedAngle;std::memcpy(&signedAngle,&bits,sizeof(signedAngle));
        const int body=(signedAngle/45)%8;
        if(auto* pc=dynamic_cast<CPc*>(player))pc->InvalidateBillboard();
        return SendMsg(152,body,0,0);
    }
    case 141: {
        if(!m_world || !m_world->m_isPKZone)return 0;
        // Selected57A3DF copies both adjacent source session DWORDs, aid/gid.
        std::array<u8,10> packet{{0x0f,0x02}};
        for(int i=0;i<4;++i){packet[2+i]=static_cast<u8>(static_cast<u32>(g_session.m_aid)>>(8*i));
            packet[6+i]=static_cast<u8>(static_cast<u32>(g_session.m_gid)>>(8*i));}
        return CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(packet.data()),static_cast<int>(packet.size()))?1:0;
    }
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
    case 150: {
        const bool defaultGreeting=parameters[0].empty();
        const std::string displayed(g_windowMgr.GetChatMsg());
        const std::string greeting="(Hi) "+(defaultGreeting?std::string("*^_^*"):displayed);
        for(const auto& member:g_session.m_friendList){
            g_windowMgr.SetChatMsg(greeting);
            SendMsg(11,reinterpret_cast<msgparam_t>(member.characterName.c_str()),0,0);
            // Source INC AL wraps as a signed byte, then caps positive values at40.
            const int next=(static_cast<u8>(g_MessageHi)+1)&255;
            g_MessageHi=static_cast<s8>(next>=128?next-256:next);
            if(g_MessageHi>40)g_MessageHi=40;
        }
        const std::string notice=defaultGreeting?"[ To Friends ] : (Hi) *^_^*"
            :"[ To Friends ] : (Hi) "+(g_serviceType!=ServiceKorea?GetSourceChatCharset():std::string())+displayed;
        g_windowMgr.SendMsg(1,reinterpret_cast<msgparam_t>(notice.c_str()),2031615);
        return 1;
    }
    case 152:return ToggleSourceAutoMessage()?1:0;
    default:
        // Retained open cases are tracked as missing implementations. Never
        // leak a recognized/unknown slash command into ordinary global chat.
        DbgLog("[SourceChat] command type=%d awaits source action integration\n",type);return 0;
    }
}
