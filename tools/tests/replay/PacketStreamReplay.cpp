#include "Connection.h"
#include "GronPacket.h"
#include "MapSendProfile.h"
#include "Packet.h"
#include "companion_ai/CompanionAiController.h"
#include "core/ClientInfoLocale.h"
#include "core/Globals.h"
#include "core/SettingsIni.h"
#include "core/UiMessageStrings.h"
#include "core/Timer.h"
#include "gamemode/ActionRequestSender.h"
#include "gamemode/Commerce.h"
#include "gamemode/GameMode.h"
#include "gamemode/GameModePacket.h"
#include "gamemode/LoginMode.h"
#include "gamemode/Mode.h"
#include "item/Item.h"
#include "pathfinder/PathFinder.h"
#include "res/Texture.h"
#include "render/Renderer.h"
#include "session/Session.h"
#include "skill/Skill.h"
#include "qtui/QtUiStateAdapter.h"
#include "ui/UIChooseWnd.h"
#include "ui/UIGroundSkillTextWnd.h"
#include "core/MannerFilter.h"
#include "ui/UIChooseSellBuyWnd.h"
#include "ui/UINpcInputWnd.h"
#include "ui/UINpcMenuWnd.h"
#include "ui/UISayDialogWnd.h"
#include "ui/UIItemShopWnd.h"
#include "ui/UIItemPurchaseWnd.h"
#include "ui/UIItemSellWnd.h"
#include "ui/UIWindowMgr.h"
#include "ui/UIRechargeGage.h"
#include "ui/UICompanionStatusWnd.h"
#include "ui/UICompanionSkillListWnd.h"
#include "ui/UIChatRoomSettingsWnd.h"
#include "ui/UIFriendOptionWnd.h"
#include "ui/UINewChatWnd.h"
#include "ui/UIGuildWnd.h"
#include "ui/UIMinimapWnd.h"
#include "ui/UIStatusWnd.h"
#include "ui/UISkillSelectWnd.h"
#include "ui/UIMonsterInfoWnd.h"
#include "world/GameActor.h"
#include "world/RagEffect.h"
#include "world/World.h"
#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <type_traits>
#include <sstream>
#if !RO_PLATFORM_WINDOWS
#include <sys/ioctl.h>
#include <unistd.h>
#endif
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#if RO_ENABLE_QT6_UI
#include "render3d/RenderBackend.h"
#include <QGuiApplication>
#include <QVariant>
#endif
#if !RO_PLATFORM_WINDOWS
#include <fcntl.h>
#endif

struct SkillPacketReplayAccess {
    static CMode* Current() { return g_modeMgr.m_curMode; }
    static int CurrentType() { return g_modeMgr.m_curModeType; }
    static int NextType() { return g_modeMgr.m_nextModeType; }
    static void SetCurrent(CGameMode* mode) {
        g_modeMgr.m_curMode=mode;
        g_modeMgr.m_curModeType=mode?1:0;
    }
    static void Restore(CMode* mode,int type) {
        g_modeMgr.m_curMode=mode;
        g_modeMgr.m_curModeType=type;
    }
};

struct PacketStreamReplayAccess {
    static SOCKET Native(const CRagConnection& connection) { return connection.m_socket; }
    static int BufferedBytes(const CRagConnection& connection) { return connection.m_recvQueue.GetSize(); }
};

struct LoginModeReplayAccess : CLoginMode {
    int NextSubMode() const { return m_nextSubMode; }
    static void SeedCharServerForConnect(LoginModeReplayAccess& mode, u32 ip, s16 port) {
        mode.m_numServer = 1;
        mode.m_serverSelected = 0;
        std::memset(mode.m_serverInfo, 0, sizeof(mode.m_serverInfo));
        mode.m_serverInfo[0].ip = ip;
        mode.m_serverInfo[0].port = port;
    }
    static void PrepareHandshakeOnly(LoginModeReplayAccess& mode) {
        mode.m_syncRequestTime = 123;
        mode.m_waitingForPasswordHash = true;
        mode.m_plainAccountLoginSent = true;
    }
    static bool HandshakeCleared(const LoginModeReplayAccess& mode) {
        return mode.m_syncRequestTime == 0 && !mode.m_waitingForPasswordHash
            && !mode.m_plainAccountLoginSent;
    }
    static void SendHashRequest(LoginModeReplayAccess& mode) {
        mode.SendPasswordHashRequest();
    }
    static void RunPlainAccountLogin(LoginModeReplayAccess& mode) {
        mode.CLoginMode::SendPlainAccountLogin();
    }
    static void RunAckHash(LoginModeReplayAccess& mode, const std::vector<u8>& packet) {
        mode.OnAckHash(packet);
    }
    static std::string FormatRefusal(const std::vector<u8>& packet) {
        return FormatLoginRefusalMessage(packet);
    }
    static void RunRefusal(LoginModeReplayAccess& mode, const std::vector<u8>& packet) {
        mode.OnRefuseLogin(packet);
    }
    static void RunCharRefusal(LoginModeReplayAccess& mode, const std::vector<u8>& packet) {
        mode.OnRefuseChar(packet);
    }
    static bool IsConnected(const LoginModeReplayAccess& mode) {
        return mode.m_isConnected != 0;
    }
    static void RunNotifyZonesvr(LoginModeReplayAccess& mode, const std::vector<u8>& packet) {
        mode.OnNotifyZonesvr(packet);
    }
    static bool ZoneEndpointMatches(const LoginModeReplayAccess& mode) {
        return std::string(mode.m_zoneAddr) == "10.0.0.1" && mode.m_zonePort == 6121;
    }
    static void RunAcceptLogin(LoginModeReplayAccess& mode, const std::vector<u8>& packet) {
        mode.OnAcceptLogin(packet);
    }
    static void RunAcceptChar(LoginModeReplayAccess& mode, const std::vector<u8>& packet) {
        mode.OnAcceptChar(packet);
    }
    static void RunAcceptMakeChar(LoginModeReplayAccess& mode, const std::vector<u8>& packet) {
        mode.OnAcceptMakeChar(packet);
    }
    static void SeedCharacterForSelection(LoginModeReplayAccess& mode, u8 slot) {
        mode.m_numChar = 1;
        std::memset(&mode.m_charInfo[0], 0, sizeof(mode.m_charInfo[0]));
        mode.m_charInfo[0].CharNum = slot;
        mode.m_charInfo[0].GID = 0x12345678u;
        std::memcpy(mode.m_charInfo[0].name, "RuneA", 5);
    }
    static void RunMakeCharRefusal(LoginModeReplayAccess& mode, const std::vector<u8>& packet) {
        mode.OnRefuseMakeChar(packet);
    }
    static bool AcceptedCharacterFieldsMatch(const LoginModeReplayAccess& mode) {
        const CHARACTER_INFO* const characters = mode.GetCharInfo();
        return mode.GetCharCount() == 1 && characters != nullptr
            && characters[0].GID == 0x12345678u
            && std::string(reinterpret_cast<const char*>(characters[0].name)) == "RuneA"
            && characters[0].job == 7
            && characters[0].headpalette == 18
            && characters[0].CharNum == 7
            && characters[0].haircolor == 18
            && characters[0].bIsChangedCharName == 1;
    }
    static bool AcceptedCharacterListFieldsMatch(const LoginModeReplayAccess& mode) {
        const CHARACTER_INFO* const characters = mode.GetCharInfo();
        return mode.GetCharCount() == 2 && characters != nullptr
            && characters[0].GID == 0x12345678u
            && std::string(reinterpret_cast<const char*>(characters[0].name)) == "RuneA"
            && characters[0].job == 7 && characters[0].headpalette == 18
            && characters[0].CharNum == 7 && characters[0].haircolor == 18
            && characters[0].bIsChangedCharName == 1
            && characters[1].GID == 0x87654321u
            && std::string(reinterpret_cast<const char*>(characters[1].name)) == "RuneB"
            && characters[1].job == 8 && characters[1].headpalette == 9
            && characters[1].CharNum == 3 && characters[1].haircolor == 7
            && characters[1].bIsChangedCharName == 1;
    }
    static bool FirstCharacterSelected(const LoginModeReplayAccess& mode) {
        return mode.m_selectedCharIndex == 0;
    }
    static bool AcceptLoginFieldsMatch(const LoginModeReplayAccess& mode) {
        if (mode.m_numServer != 2 || mode.m_serverSelected != 0
            || mode.m_serverInfo[0].ip != 0x0a01a8c0u
            || static_cast<u16>(mode.m_serverInfo[0].port) != 6121u
            || std::string(reinterpret_cast<const char*>(mode.m_serverInfo[0].name)) != "char-main"
            || mode.m_serverInfo[0].usercount != 3
            || mode.m_serverInfo[0].state != 0x1234
            || mode.m_serverInfo[0].property != 2
            || mode.m_serverInfo[1].ip != 0x077100cbu
            || static_cast<u16>(mode.m_serverInfo[1].port) != 6122u
            || std::string(reinterpret_cast<const char*>(mode.m_serverInfo[1].name)) != "char-backup"
            || mode.m_serverInfo[1].usercount != 9
            || mode.m_serverInfo[1].state != 0x5678
            || mode.m_serverInfo[1].property != 1) {
            return false;
        }
        return true;
    }
    static bool MalformedAcceptLoginClearedList(const LoginModeReplayAccess& mode) {
        return mode.m_numServer == 0 && mode.m_serverSelected == -1;
    }
    static std::string FormatDisconnect(const std::vector<u8>& packet) {
        return FormatLoginDisconnectMessage(packet);
    }
    static void RunDisconnect(LoginModeReplayAccess& mode, const std::vector<u8>& packet) {
        mode.OnDisconnectMsg(packet);
    }
    static void PrepareDisconnectState(LoginModeReplayAccess& mode) {
        mode.m_syncRequestTime = 123;
        mode.m_waitingForPasswordHash = true;
        mode.m_plainAccountLoginSent = true;
        mode.m_isConnected = 1;
    }
    static bool DisconnectStateCleared(const LoginModeReplayAccess& mode) {
        return mode.m_syncRequestTime == 0 && !mode.m_waitingForPasswordHash
            && !mode.m_plainAccountLoginSent && mode.m_isConnected == 0;
    }
    static void AdoptDeferredStatus(LoginModeReplayAccess& mode, const char* status) {
        SetPendingLoginStatus(status);
        AdoptPendingLoginStatus(mode.m_deferredLoginStatus);
    }
    static const std::string& DeferredStatus(const LoginModeReplayAccess& mode) {
        return mode.m_deferredLoginStatus;
    }
    static void SetStatus(LoginModeReplayAccess& mode, const char* status) {
        mode.SetLoginStatus(status);
    }
    static std::string ConsumeStableStatus(LoginModeReplayAccess& mode, const char* normalStatus) {
        return ConsumeDeferredLoginStatus(mode.m_deferredLoginStatus,normalStatus);
    }
    static void SetConnected(LoginModeReplayAccess& mode, bool connected) {
        mode.m_isConnected=connected?1:0;
    }
    static void RunPollNetwork(LoginModeReplayAccess& mode) {
        mode.PollNetwork();
    }
};

namespace {
using Clock=std::chrono::steady_clock;
constexpr auto kTimeout=std::chrono::seconds(2);
constexpr const char* kFixedFixture="PK03.02-map-006c-synthetic";
constexpr const char* kFixedSha="a8649967f8ca13630d18e21c07eaad14b309a54534f1cd9f633b692c3b2f341f";
constexpr const char* kVariableFixture="PK03.02-variable-0069-synthetic";
constexpr const char* kVariableSha="d5eb153194bad9d1ab1c303bb77da191fedc51db5728e646153b92cf302df42b";
int g_decodeCount=0;int g_recvInvocations=0;std::string g_currentCase="startup";std::string g_currentStage="setup";std::string g_currentFixture="unknown";std::string g_currentProductionEntry="unknown";
void SetCase(const std::string& name,const char* stage="generic_transport",const char* fixture="unknown",const char* productionEntry="unknown"){g_currentCase=name;g_currentStage=stage;g_currentFixture=fixture;g_currentProductionEntry=productionEntry;g_recvInvocations=0;}
void Require(bool condition,const std::string& message){if(!condition)throw std::runtime_error(message);}
void CloseSocket(SOCKET s){if(s==INVALID_SOCKET)return;
#if RO_PLATFORM_WINDOWS
    closesocket(s);
#else
    close(s);
#endif
}
struct SocketOwner{
    SOCKET value=INVALID_SOCKET;SocketOwner()=default;explicit SocketOwner(SOCKET s):value(s){};~SocketOwner(){CloseSocket(value);}
    SocketOwner(const SocketOwner&)=delete;SocketOwner& operator=(const SocketOwner&)=delete;
    SOCKET release(){SOCKET s=value;value=INVALID_SOCKET;return s;}
};
int SelectSocket(SOCKET s,bool readable,bool writable,std::chrono::milliseconds timeout){
    fd_set r,w;FD_ZERO(&r);FD_ZERO(&w);if(readable)FD_SET(s,&r);if(writable)FD_SET(s,&w);
    timeval tv{};tv.tv_sec=static_cast<long>(timeout.count()/1000);tv.tv_usec=static_cast<long>((timeout.count()%1000)*1000);
#if RO_PLATFORM_WINDOWS
    return select(0,readable?&r:nullptr,writable?&w:nullptr,nullptr,&tv);
#else
    return select(s+1,readable?&r:nullptr,writable?&w:nullptr,nullptr,&tv);
#endif
}
std::vector<u8> ReceiveExactSocket(SOCKET socketValue,size_t count){
    std::vector<u8> bytes;bytes.reserve(count);const auto deadline=Clock::now()+kTimeout;
    while(bytes.size()<count){
        const int ready=SelectSocket(socketValue,true,false,std::chrono::milliseconds(100));
        if(ready<=0){Require(ready==0,"socket select failed while waiting for captured bytes");Require(Clock::now()<deadline,"socket capture timed out");continue;}
        std::array<u8,4096> buffer{};
        const int received=recv(socketValue,reinterpret_cast<char*>(buffer.data()),static_cast<int>(std::min(buffer.size(),count-bytes.size())),0);
        if(received>0){bytes.insert(bytes.end(),buffer.begin(),buffer.begin()+received);continue;}
        Require(false,"socket closed before expected captured bytes arrived");
    }
    return bytes;
}
class TestConnection final:public CRagConnection{
public:SOCKET Native()const{return m_socket;}int BufferedBytes()const{return m_recvQueue.GetSize();}
    bool QueueBytes(const std::vector<u8>& bytes){return m_recvQueue.Push(reinterpret_cast<const char*>(bytes.data()),static_cast<int>(bytes.size()));}
};
bool Receive(TestConnection& c,std::vector<u8>& out){++g_recvInvocations;return c.RecvPacket(out);}
struct LoopbackPeer{
    SocketOwner peer;
    explicit LoopbackPeer(CRagConnection& connection){
        SocketOwner listener(socket(AF_INET,SOCK_STREAM,IPPROTO_TCP));Require(listener.value!=INVALID_SOCKET,"listener socket failed");
        sockaddr_in addr{};addr.sin_family=AF_INET;addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);addr.sin_port=0;
        Require(bind(listener.value,reinterpret_cast<sockaddr*>(&addr),sizeof(addr))==0,"loopback bind failed");
        Require(listen(listener.value,1)==0,"loopback listen failed");
#if RO_PLATFORM_WINDOWS
        int addrLen=sizeof(addr);
#else
        socklen_t addrLen=sizeof(addr);
#endif
        Require(getsockname(listener.value,reinterpret_cast<sockaddr*>(&addr),&addrLen)==0,"getsockname failed");
        Require(connection.Connect("127.0.0.1",ntohs(addr.sin_port)),std::string("production Connect failed: ")+connection.GetLastConnectError());
        Require(SelectSocket(listener.value,true,false,std::chrono::duration_cast<std::chrono::milliseconds>(kTimeout))>0,"accept timed out");
        SOCKET accepted=accept(listener.value,nullptr,nullptr);Require(accepted!=INVALID_SOCKET,"loopback accept failed");peer.value=accepted;
#if RO_PLATFORM_WINDOWS
        u_long nonblocking=1;Require(ioctlsocket(peer.value,FIONBIO,&nonblocking)==0,"peer nonblocking setup failed");
#else
        int flags=fcntl(peer.value,F_GETFL,0);Require(flags>=0&&fcntl(peer.value,F_SETFL,flags|O_NONBLOCK)==0,"peer nonblocking setup failed");
#endif
    }
    void Send(const std::vector<u8>& bytes){size_t sent=0;auto deadline=Clock::now()+kTimeout;while(sent<bytes.size()){
        if(SelectSocket(peer.value,false,true,std::chrono::milliseconds(100))<=0){Require(Clock::now()<deadline,"loopback send timed out");continue;}
        int n=::send(peer.value,reinterpret_cast<const char*>(bytes.data()+sent),static_cast<int>(bytes.size()-sent),0);if(n>0){sent+=static_cast<size_t>(n);continue;}
#if RO_PLATFORM_WINDOWS
        int err=WSAGetLastError();if(err==WSAEWOULDBLOCK)continue;
#else
        if(errno==EWOULDBLOCK||errno==EAGAIN)continue;
#endif
        throw std::runtime_error("loopback send failed");
    }}
    std::vector<u8> ReceiveExact(size_t count){std::vector<u8> bytes;bytes.reserve(count);auto deadline=Clock::now()+kTimeout;while(bytes.size()<count){int ready=SelectSocket(peer.value,true,false,std::chrono::milliseconds(100));if(ready<=0){Require(ready==0,"peer select failed while waiting for outbound bytes");Require(Clock::now()<deadline,"outbound capture timed out");continue;}std::array<u8,4096> buffer{};int n=recv(peer.value,reinterpret_cast<char*>(buffer.data()),static_cast<int>(std::min(buffer.size(),count-bytes.size())),0);if(n>0){bytes.insert(bytes.end(),buffer.begin(),buffer.begin()+n);continue;}
#if RO_PLATFORM_WINDOWS
        if(n==SOCKET_ERROR&&WSAGetLastError()==WSAEWOULDBLOCK)continue;
#else
        if(n<0&&(errno==EWOULDBLOCK||errno==EAGAIN))continue;
#endif
        throw std::runtime_error("peer closed while capturing outbound bytes");}return bytes;}
};
void WaitReadable(const TestConnection& c,Clock::time_point deadline){while(Clock::now()<deadline){auto remain=std::chrono::duration_cast<std::chrono::milliseconds>(deadline-Clock::now());int n=SelectSocket(c.Native(),true,false,std::min(remain,std::chrono::milliseconds(100)));if(n>0)return;Require(n==0,"socket select failed while waiting for input");}throw std::runtime_error("packet receive timed out");}
void PumpUntilBuffered(TestConnection& c,int bytes){auto deadline=Clock::now()+kTimeout;std::vector<u8> out;while(c.BufferedBytes()<bytes){WaitReadable(c,deadline);Require(!Receive(c,out),"unexpected complete packet while accumulating prefix");}Require(c.BufferedBytes()==bytes,"production queue exceeded expected prefix length");}
std::vector<u8> PumpUntilPacket(TestConnection& c){auto deadline=Clock::now()+kTimeout;std::vector<u8> out;while(!Receive(c,out))WaitReadable(c,deadline);return out;}
const char* ReceiveErrorName(CRagConnection::ReceiveError error){switch(error){case CRagConnection::ReceiveError::None:return "None";case CRagConnection::ReceiveError::UnknownOpcode:return "UnknownOpcode";case CRagConnection::ReceiveError::InvalidVariableLength:return "InvalidVariableLength";}return "InvalidEnumValue";}
void WaitKernelPending(const TestConnection& c,int bytes){auto deadline=Clock::now()+kTimeout;while(Clock::now()<deadline){int pending=0;
#if RO_PLATFORM_WINDOWS
    Require(ioctlsocket(c.Native(),FIONREAD,reinterpret_cast<u_long*>(&pending))==0,"FIONREAD failed");
#else
    Require(ioctl(c.Native(),FIONREAD,&pending)==0,"FIONREAD failed");
#endif
    if(pending>=bytes)return;std::this_thread::sleep_for(std::chrono::milliseconds(1));}throw std::runtime_error("coalesced bytes did not reach receive socket before deadline");}
std::string Hex(const std::vector<u8>& bytes){static const char* d="0123456789abcdef";std::string out;for(u8 b:bytes){out.push_back(d[b>>4]);out.push_back(d[b&15]);}return out;}
std::vector<u8> Unhex(std::string_view value){Require(value.size()%2==0,"fixture hex has odd digit count");std::vector<u8> bytes;bytes.reserve(value.size()/2);auto digit=[](char c)->u8{if(c>='0'&&c<='9')return static_cast<u8>(c-'0');if(c>='a'&&c<='f')return static_cast<u8>(c-'a'+10);if(c>='A'&&c<='F')return static_cast<u8>(c-'A'+10);throw std::runtime_error("fixture hex contains a non-hex digit");};for(size_t i=0;i<value.size();i+=2)bytes.push_back(static_cast<u8>((digit(value[i])<<4)|digit(value[i+1])));return bytes;}
std::string ChunkJson(const std::vector<std::vector<u8>>& chunks){std::string out="[";for(size_t i=0;i<chunks.size();++i){if(i)out+=",";out+="\""+Hex(chunks[i])+"\"";}return out+"]";}
std::string JsonEscape(const std::string& value){static const char* hex="0123456789abcdef";std::string out;for(unsigned char ch:value){if(ch=='"'||ch=='\\'){out.push_back('\\');out.push_back(static_cast<char>(ch));}else if(ch<0x20){out+="\\u00";out.push_back(hex[ch>>4]);out.push_back(hex[ch&0x0f]);}else out.push_back(static_cast<char>(ch));}return out;}
#ifndef RO_PACKET_REPLAY_SOURCE_COMMIT_AT_CONFIGURE
#define RO_PACKET_REPLAY_SOURCE_COMMIT_AT_CONFIGURE "unknown"
#endif
#ifndef RO_PACKET_REPLAY_SOURCE_TREE_STATUS_AT_CONFIGURE
#define RO_PACKET_REPLAY_SOURCE_TREE_STATUS_AT_CONFIGURE "unknown"
#endif
std::string ConfiguredSourceProvenanceJson(){return "\"runtime_baseline_commit\":\"af41f6484cb7ff31825296babe4b913aa36c4c67\",\"source_commit_at_configure\":\""+std::string(RO_PACKET_REPLAY_SOURCE_COMMIT_AT_CONFIGURE)+"\",\"source_tree_status_at_configure\":\""+RO_PACKET_REPLAY_SOURCE_TREE_STATUS_AT_CONFIGURE+"\"";}
void Report(const std::string& id,const char* fixture,const char* sha,const std::vector<std::vector<u8>>& chunks,const std::string& expected,const std::string& actual,int expectedQueue,int actualQueue,const TestConnection& connection,const char* expectedState="open",const char* classification="asserted",int maxExpectedQueue=-1){
    g_currentStage="generic_transport";g_currentFixture=fixture;g_currentProductionEntry="CRagConnection::Connect/RecvPacket -> TryReadPacket";
    if(expectedQueue>=0)Require(actualQueue==expectedQueue,"queue remainder mismatch: expected "+std::to_string(expectedQueue)+", actual "+std::to_string(actualQueue));
    if(maxExpectedQueue>=0)Require(actualQueue<=maxExpectedQueue,"queue remainder exceeds maximum "+std::to_string(maxExpectedQueue)+": actual "+std::to_string(actualQueue));
    const char* actualState=connection.IsOpen()?"open":"closed";Require(std::string(expectedState)==actualState,"connection state mismatch: expected "+std::string(expectedState)+", actual "+actualState);
    const char* queueAssertion=expectedQueue>=0?"exact":maxExpectedQueue>=0?"upper_bound":"not_asserted";
    std::cout<<"{\"case_id\":\""<<id<<"\",\"contract_id\":\"PK01.P02.FRAMING\",\"fixture_id\":\""<<fixture<<"\",\"expected_fixture_sha256\":\""<<sha<<"\","<<ConfiguredSourceProvenanceJson()<<",\"build_preset\":\"mingw-qt-x64-ci; deploy OFF\",\"production_entry\":\"CRagConnection::Connect/RecvPacket -> TryReadPacket\",\"stage\":\"generic_transport\",\"profile\":23,\"input_chunks_hex\":"<<ChunkJson(chunks)<<",\"invocation_count\":"<<g_recvInvocations<<",\"expected_delivery\":\""<<(expected.empty()?"no_packet":"packet")<<"\",\"expected_packet_hex\":\""<<expected<<"\",\"actual_packet_hex\":\""<<actual<<"\",\"expected_connection_state\":\""<<expectedState<<"\",\"actual_connection_state\":\""<<actualState<<"\",\"queue_remainder_assertion\":\""<<queueAssertion<<"\",\"expected_queue_remainder\":"<<(expectedQueue<0?"null":std::to_string(expectedQueue))<<",\"expected_queue_remainder_max\":"<<(maxExpectedQueue<0?"null":std::to_string(maxExpectedQueue))<<",\"queue_remainder\":"<<actualQueue<<",\"classification\":\""<<classification<<"\",\"outcome\":\"PASS\"}\n";
}
std::vector<u8> ReadOne(TestConnection& c){return PumpUntilPacket(c);}
PacketView VerifyRoute(const std::vector<u8>& packet,u16 id){PacketView view{};int consumed=0;Require(TryReadPacket(packet.data(),static_cast<int>(packet.size()),view,consumed),"production packet decoder rejected framed packet");Require(view.packetId==id&&view.packetLength==packet.size()&&consumed==static_cast<int>(packet.size()),"production packet decoder returned wrong boundary");++g_decodeCount;return view;}
void ProductionRouterDispatchReplay(){
    constexpr const char* fixtureId="PK03.04-router-01f8-01b0";
    constexpr const char* fixtureSha="f5ef8e6a3ff7b85b522b6a38c5e8e0d3f76e92a4bbcbfead36c838ff91ac8d10";
    const std::vector<u8> first={0xf8,0x01};
    const std::vector<u8> second={0xb0,0x01,0,0,0,0,0,0,0,0,0};
    std::vector<u8> combined=first;
    combined.insert(combined.end(),second.begin(),second.end());

    SetCase("production_router_dispatch","production_router_replay",fixtureId,"CGameMode::OnUpdate -> ProcessGameModePacketLoop -> production global router");
    TestConnection connection;
    LoopbackPeer peer(connection);
    peer.Send(combined);
    WaitKernelPending(connection,static_cast<int>(combined.size()));

    CGameMode gameMode;
    CGameModePacketRouter& router=GetProductionGameModePacketRouter();
    router.Clear();
    RegisterDefaultGameModePacketHandlers(router);
    const GameModePacketLoopResult loopResult=ProcessGameModePacketLoop(gameMode,connection,router,4);
    Require(loopResult.packetsProcessed==2,"production packet loop processed the wrong packet count");
    Require(loopResult.packetsDispatched==2&&loopResult.packetsUnhandled==0,"production packet loop did not dispatch both default handlers");
    Require(loopResult.receiveDrained,"production packet loop did not report drained receive");
    Require(connection.BufferedBytes()==0,"production router replay left coalesced bytes queued");

    std::vector<u8> unknown={0x34,0x12};
    PacketView unknownView{0x1234,static_cast<u16>(unknown.size()),unknown.data()};
    const bool unknownDispatched=router.Dispatch(gameMode,unknownView);
    Require(!unknownDispatched,"production packet router dispatched an unregistered opcode");
    std::cout<<"{\"case_id\":\"production_router_dispatch\",\"contract_id\":\"PK01.P02.FRAMING\",\"fixture_id\":\""<<fixtureId
        <<"\",\"expected_fixture_sha256\":\""<<fixtureSha
        <<"\","<<ConfiguredSourceProvenanceJson()<<",\"build_preset\":\"mingw-qt-x64-ci; deploy OFF\""
        <<",\"production_entry\":\"CGameMode::OnUpdate -> ProcessGameModePacketLoop -> CRagConnection::RecvPacket -> TryReadPacket -> production global router dispatch\""
        <<",\"fixture_provenance\":\"synthetic receive-profile length samples; no server-emission claim\""
        <<",\"stage\":\"production_router_replay\",\"assertion_scope\":\"shared production loop and global default router; full CGameMode::OnUpdate frame not instantiated\",\"input_chunks\":"<<ChunkJson({combined})
        <<",\"expected_input_packet_hexes\":[\""<<Hex(first)<<"\",\""<<Hex(second)<<"\"]"
        <<",\"packets_processed\":"<<loopResult.packetsProcessed<<",\"packets_dispatched\":"<<loopResult.packetsDispatched
        <<",\"packets_unhandled\":"<<loopResult.packetsUnhandled<<",\"receive_drained\":"<<(loopResult.receiveDrained?"true":"false")
        <<",\"expected_router_results\":[true,true,false],\"actual_router_results\":[true,true,"
        <<(unknownDispatched?"true":"false")<<"]"
        <<",\"expected_connection_state\":\"open\",\"actual_connection_state\":\""<<(connection.IsOpen()?"open":"closed")
        <<"\",\"expected_queue_remainder\":0,\"actual_queue_remainder\":"<<connection.BufferedBytes()<<",\"outcome\":\"PASS\"}\n";
}

void SelectedInventoryCartRouteIsolationReplay()
{
    SetCase("selected_inventory_cart_route_isolation", "PK09.03",
            "PK09.02-selected-20080910-four-lists", "TryReadPacket -> production router -> inventory/commerce list handlers");
    const auto savedCommerce = ro::commerce::state;
    ro::commerce::state = ro::commerce::State{};
    g_session.ClearInventoryItems();

    ITEM_INFO oldNormal{};
    oldNormal.m_itemIndex = 8001; oldNormal.SetItemId(9001); oldNormal.m_itemType = 3; oldNormal.m_num = 7;
    ITEM_INFO oldEquipment{};
    oldEquipment.m_itemIndex = 8002; oldEquipment.SetItemId(9002); oldEquipment.m_itemType = 4;
    oldEquipment.m_num = 1; oldEquipment.m_location = 1; oldEquipment.m_wearLocation = 1;
    g_session.SetInventoryItem(oldNormal);
    g_session.SetInventoryItem(oldEquipment);

    ITEM_INFO oldCartNormal{};
    oldCartNormal.m_itemIndex = 8101; oldCartNormal.SetItemId(9101); oldCartNormal.m_itemType = 3; oldCartNormal.m_num = 2;
    ITEM_INFO oldCartEquipment{};
    oldCartEquipment.m_itemIndex = 8102; oldCartEquipment.SetItemId(9102); oldCartEquipment.m_itemType = 4; oldCartEquipment.m_num = 1;
    ro::commerce::state.cartItems[oldCartNormal.m_itemIndex] = oldCartNormal;
    ro::commerce::state.cartItems[oldCartEquipment.m_itemIndex] = oldCartEquipment;

    CGameMode mode;
    CGameModePacketRouter& router = GetProductionGameModePacketRouter();
    RegisterDefaultGameModePacketHandlers(router);
    Require(!ro::net::IsActiveEquipInventoryListPacket(0x01EF), "historical 0x01EF still claims equipment-inventory profile ownership");
    Require(ro::net::IsActiveNormalInventoryListPacket(0x02E8), "selected normal inventory route is not active");
    Require(ro::net::IsActiveEquipInventoryListPacket(0x02D0), "selected equipment inventory route is not active");

    const auto makePacket = [](u16 id, size_t entrySize, u16 itemIndex, u16 itemId, u8 itemType, u16 amount) {
        std::vector<u8> bytes(4 + entrySize, 0);
        bytes[0] = static_cast<u8>(id & 0xFF); bytes[1] = static_cast<u8>(id >> 8);
        bytes[2] = static_cast<u8>(bytes.size() & 0xFF); bytes[3] = static_cast<u8>(bytes.size() >> 8);
        const auto put16 = [&](size_t offset, u16 value) { bytes[offset] = static_cast<u8>(value & 0xFF); bytes[offset + 1] = static_cast<u8>(value >> 8); };
        if (entrySize >= 10) {
            const size_t e = 4;
            put16(e, itemIndex); put16(e + 2, itemId); bytes[e + 4] = itemType; bytes[e + 5] = 1;
            if (id == 0x02E8 || id == 0x02E9) {
                put16(e + 6, amount); put16(e + 8, 0);
            } else {
                put16(e + 6, 1); put16(e + 8, 1); bytes[e + 10] = 0; bytes[e + 11] = 0;
                put16(e + 12, 0); put16(e + 14, 0); put16(e + 16, 0); put16(e + 18, 0);
                put16(e + 20, 0); put16(e + 22, 0);
            }
        }
        return bytes;
    };
    const auto dispatch = [&](const std::vector<u8>& bytes, const char* label) {
        PacketView packet{}; int consumed = 0;
        Require(TryReadPacket(bytes.data(), static_cast<int>(bytes.size()), packet, consumed), std::string(label) + " did not frame");
        Require(consumed == static_cast<int>(bytes.size()), std::string(label) + " left trailing bytes");
        Require(router.Dispatch(mode, packet), std::string(label) + " had no production handler");
    };

    dispatch(makePacket(0x02E8, 22, 8201, 9201, 3, 5), "normal inventory");
    Require(g_session.GetInventoryItemByIndex(8001) == nullptr && g_session.GetInventoryItemByIndex(8002) != nullptr,
            "normal inventory replacement changed the equipment category");
    dispatch(makePacket(0x02D0, 26, 8202, 9202, 4, 1), "equipment inventory");
    Require(g_session.GetInventoryItemByIndex(8201) != nullptr && g_session.GetInventoryItemByIndex(8002) == nullptr,
            "equipment inventory replacement changed the normal category");

    dispatch(makePacket(0x02E9, 22, 8301, 9301, 3, 6), "normal cart");
    Require(ro::commerce::state.cartItems.count(8301) == 1 && ro::commerce::state.cartItems.count(8101) == 0
            && ro::commerce::state.cartItems.count(8102) == 1, "normal cart replacement changed the equipment-cart category");
    dispatch(makePacket(0x02D2, 26, 8302, 9302, 4, 1), "equipment cart");
    Require(ro::commerce::state.cartItems.count(8302) == 1 && ro::commerce::state.cartItems.count(8301) == 1
            && ro::commerce::state.cartItems.count(8102) == 0, "equipment cart replacement changed the normal-cart category");
    Require(g_session.GetInventoryItemByIndex(8201) != nullptr && g_session.GetInventoryItemByIndex(8202) != nullptr,
            "cart list dispatch modified inventory state");

    // Historical normal-cart compatibility bytes have an 18-byte row, not an
    // equipment inventory row. Prove the final dispatch owner, not just a predicate.
    const std::vector<u8> historicalCart = {
        0xef,1,22,0, 0x6f,0x20,0x57,0x24,3,1,2,0,0,0,
        0xa1,0xa1,0xb2,0xb2,0xc3,0xc3,0xd4,0xd4
    };
    dispatch(historicalCart, "historical 0x01EF normal cart compatibility");
    Require(g_session.GetInventoryItems().size()==2 && g_session.GetInventoryItemByIndex(8202)
            && g_session.GetInventoryItemByIndex(8202)->m_wearLocation==1,
            "historical 0x01EF changed selected equipment inventory");
    Require(ro::commerce::state.cartItems.count(0x206f)==1
            && ro::commerce::state.cartItems.at(0x206f).m_num==2
            && ro::commerce::state.cartItems.count(8302)==1,
            "historical 0x01EF did not dispatch uniquely to normal cart compatibility");

    dispatch(makePacket(0x02E8, 22, 8203, 9203, 3, 4), "normal inventory after equipment");
    Require(g_session.GetInventoryItemByIndex(8202) != nullptr, "normal list after equipment list removed equipment state");
    dispatch(makePacket(0x02E8, 1, 0, 0, 0, 0), "malformed normal inventory");
    Require(g_session.GetInventoryItemByIndex(8203) != nullptr, "malformed list changed existing inventory state");
    dispatch(makePacket(0x02E8, 0, 0, 0, 0, 0), "empty normal inventory");
    Require(g_session.GetInventoryItemByIndex(8203) == nullptr && g_session.GetInventoryItemByIndex(8202) != nullptr,
            "empty normal list did not preserve equipment state");

    g_session.ClearInventoryItems();
    ro::commerce::state = savedCommerce;
    std::cout << "{\"case_id\":\"selected_inventory_cart_route_isolation\",\"contract_id\":\"PK09.03\",\"routes\":[\"02E8\",\"02D0\",\"02E9\",\"02D2\"],\"outcome\":\"PASS\"}\n";
}

void DisconnectAckRefusalDispatchReplay(PendingDisconnectAction action,const char* caseId)
{
    constexpr const char* expectedMessage="The server refused the disconnect request.";
    constexpr const char* fixtureSha="795662f84b1ae022443f1d955705a42ca741baa5533f9342fca7045dbc0529b4";
    const std::vector<u8> refusal={0x8b,0x01,0x01,0x00};
    SetCase(caseId,"production_disconnect_ack","PK03.04-018b-result-1-independent","ProcessGameModePacketLoop -> CRagConnection::RecvPacket -> GetProductionGameModePacketRouter -> HandleDisconnectAck");
    ClearPendingDisconnectAction();
    Require(BeginPendingDisconnectAction(action),"could not stage disconnect action for production refusal replay");
    g_windowMgr.ClearChatEvents();

    CRagConnection* connection=CRagConnection::instance();
    connection->Disconnect();
    LoopbackPeer peer(*connection);
    peer.Send(refusal);
    CGameMode gameMode;
    CGameModePacketRouter& router=GetProductionGameModePacketRouter();
    RegisterDefaultGameModePacketHandlers(router);
    GameModePacketLoopResult loopResult{};
    int loopCalls=0;
    const auto deadline=Clock::now()+kTimeout;
    do {
        loopResult=ProcessGameModePacketLoop(gameMode,*connection,router,4);
        ++loopCalls;
        if(loopResult.packetsProcessed==0)std::this_thread::sleep_for(std::chrono::milliseconds(1));
    } while(loopResult.packetsProcessed==0&&Clock::now()<deadline);
    Require(loopResult.packetsProcessed==1&&loopResult.packetsDispatched==1
        &&loopResult.packetsUnhandled==0&&loopResult.receiveDrained,
        "refusal replay did not dispatch and drain exactly one production packet");
    Require(GetPendingDisconnectAction()==PendingDisconnectAction::None,"server refusal did not clear the pending action");
    Require(connection->IsOpen(),"server refusal disconnected the active singleton map connection");
    const std::vector<UIChatEvent>& events=g_windowMgr.GetChatEvents();
    Require(!events.empty()&&events.back().text==expectedMessage,"server refusal did not queue the expected system chat event");

    std::cout<<"{\"case_id\":\""<<caseId<<"\",\"contract_id\":\"PK03.04.DISCONNECT-ACK\",\"fixture_id\":\"PK03.04-018b-result-1-independent\",\"expected_fixture_sha256\":\""<<fixtureSha<<"\","
        <<ConfiguredSourceProvenanceJson()<<",\"build_preset\":\"mingw-qt-x64-ci; deploy OFF\""
        <<",\"production_entry\":\"ProcessGameModePacketLoop -> CRagConnection::RecvPacket -> TryReadPacket -> GetProductionGameModePacketRouter::Dispatch -> HandleDisconnectAck -> UIWindowMgr::PushChatEvent\""
        <<",\"stage\":\"map_disconnect_refusal\",\"assertion_scope\":\"handler queued refusal event; native/Qt chat rendering not exercised\",\"pending_action_before\":"<<static_cast<int>(action)
        <<",\"input_chunks_hex\":"<<ChunkJson({refusal})<<",\"expected_result\":1,\"actual_result\":1,\"loop_invocation_count\":"<<loopCalls
        <<",\"expected_message\":\""<<JsonEscape(expectedMessage)<<"\""
        <<",\"packets_processed\":"<<loopResult.packetsProcessed<<",\"packets_dispatched\":"<<loopResult.packetsDispatched
        <<",\"packets_unhandled\":"<<loopResult.packetsUnhandled
        <<",\"expected_queued_chat_event\":\""<<JsonEscape(expectedMessage)<<"\",\"actual_queued_chat_event\":\""<<JsonEscape(events.back().text)<<"\",\"actual_queued_chat_channel\":"<<static_cast<unsigned int>(events.back().channel)<<",\"expected_pending_action\":0,\"actual_pending_action\":"<<static_cast<int>(GetPendingDisconnectAction())
        <<",\"expected_singleton_connection_state\":\"open\",\"actual_singleton_connection_state\":\""<<(connection->IsOpen()?"open":"closed")<<"\",\"outcome\":\"PASS\"}\n";
    ClearPendingDisconnectAction();
    connection->Disconnect();
    g_windowMgr.ClearChatEvents();
}
void DisconnectAckEveryBoundaryReplay()
{
    constexpr const char* fixtureSha="795662f84b1ae022443f1d955705a42ca741baa5533f9342fca7045dbc0529b4";
    const std::vector<u8> acknowledgement={0x8b,0x01,0x01,0x00};
    CGameMode gameMode;
    CGameModePacketRouter& router=GetProductionGameModePacketRouter();
    router.Clear();
    RegisterDefaultGameModePacketHandlers(router);
    for(size_t split=1;split<acknowledgement.size();++split){
        const std::string caseId="disconnect_ack_fragment_"+std::to_string(split);
        SetCase(caseId,"production_disconnect_ack_fragment","PK03.04-018b-result-1-independent","ProcessGameModePacketLoop -> CRagConnection::RecvPacket -> TryReadPacket -> production default router -> HandleDisconnectAck");
        TestConnection connection;
        LoopbackPeer peer(connection);
        ClearPendingDisconnectAction();
        g_windowMgr.ClearChatEvents();
        Require(BeginPendingDisconnectAction(PendingDisconnectAction::ReturnToCharSelect),"could not stage pending action for fragmented acknowledgement");
        MarkPendingDisconnectActionSent(100u);
        const std::vector<u8> prefix(acknowledgement.begin(),acknowledgement.begin()+static_cast<std::ptrdiff_t>(split));
        const std::vector<u8> suffix(acknowledgement.begin()+static_cast<std::ptrdiff_t>(split),acknowledgement.end());
        peer.Send(prefix);
        PumpUntilBuffered(connection,static_cast<int>(split));
        const GameModePacketLoopResult prefixResult=ProcessGameModePacketLoop(gameMode,connection,router,4);
        Require(prefixResult.packetsProcessed==0&&prefixResult.receiveDrained,"partial acknowledgement was dispatched or failed to drain");
        const PendingDisconnectAction pendingAfterPrefix=GetPendingDisconnectAction();
        Require(pendingAfterPrefix==PendingDisconnectAction::ReturnToCharSelect,"partial acknowledgement cleared pending action");
        peer.Send(suffix);
        GameModePacketLoopResult completeResult{};
        const auto deadline=Clock::now()+kTimeout;
        do {
            completeResult=ProcessGameModePacketLoop(gameMode,connection,router,4);
            if(completeResult.packetsProcessed==0)std::this_thread::sleep_for(std::chrono::milliseconds(1));
        } while(completeResult.packetsProcessed==0&&Clock::now()<deadline);
        Require(completeResult.packetsProcessed==1&&completeResult.packetsDispatched==1&&completeResult.packetsUnhandled==0,
            "fragmented acknowledgement did not dispatch once through the production loop");
        Require(GetPendingDisconnectAction()==PendingDisconnectAction::None,"complete refusal did not clear pending action");
        const auto& events=g_windowMgr.GetChatEvents();
        Require(!events.empty()&&events.back().text=="The server refused the disconnect request.","fragmented refusal did not queue the expected chat event");
        std::cout<<"{\"case_id\":\""<<caseId<<"\",\"contract_id\":\"PK03.04.DISCONNECT-ACK\",\"fixture_id\":\"PK03.04-018b-result-1-independent\",\"expected_fixture_sha256\":\""<<fixtureSha<<"\","<<ConfiguredSourceProvenanceJson()
            <<",\"build_preset\":\"mingw-qt-x64-ci; deploy OFF\",\"production_entry\":\"ProcessGameModePacketLoop -> CRagConnection::RecvPacket -> TryReadPacket -> default router dispatch -> HandleDisconnectAck\",\"stage\":\"production_disconnect_ack_fragment\",\"input_chunks_hex\":[\""<<Hex(prefix)<<"\",\""<<Hex(suffix)<<"\"],\"partial_packets_processed\":"<<prefixResult.packetsProcessed
            <<",\"complete_packets_processed\":"<<completeResult.packetsProcessed<<",\"complete_packets_dispatched\":"<<completeResult.packetsDispatched<<",\"expected_pending_action_after_prefix\":1,\"actual_pending_action_after_prefix\":"<<static_cast<int>(pendingAfterPrefix)<<",\"expected_pending_action_after_frame\":0,\"actual_pending_action_after_frame\":"<<static_cast<int>(GetPendingDisconnectAction())<<",\"expected_queued_chat_event\":\"The server refused the disconnect request.\",\"actual_queued_chat_event\":\""<<JsonEscape(events.back().text)<<"\",\"outcome\":\"PASS\"}\n";
        ClearPendingDisconnectAction();
        g_windowMgr.ClearChatEvents();
    }
}

void DisconnectChooserActivationReplay()
{
    SetCase("disconnect_chooser_activation","production_disconnect_chooser","PK03.04.DISCONNECT-ACK","UIChooseWnd::OnKeyDown -> UIChooseWnd::ActivateSelection -> CModeMgr::SendMsg -> CGameMode::SendMsg");
    CRagConnection* connection=CRagConnection::instance();
    connection->Disconnect();
    ClearPendingDisconnectAction();
    CMode* oldMode=SkillPacketReplayAccess::Current();
    const int oldModeType=SkillPacketReplayAccess::CurrentType();
    CGameMode gameMode;
    SkillPacketReplayAccess::SetCurrent(&gameMode);
    UIChooseWnd chooser;
    chooser.SetShow(1);
    Require(chooser.m_show!=0,"chooser did not become visible in the headless UI fixture");
    Require(chooser.GetEntryCount()==4,"chooser option count changed");
    Require(std::string(chooser.GetEntryLabelForQt(0))=="Character Select"
        &&std::string(chooser.GetEntryLabelForQt(2))=="Exit To Windows","chooser does not expose both disconnect options");

    chooser.OnKeyDown(VK_DOWN); // Return To Game -> Exit To Windows.
    chooser.OnKeyDown(VK_RETURN);
    const bool failedExitKeptOpen=chooser.m_show!=0&&GetPendingDisconnectAction()==PendingDisconnectAction::None;
    Require(failedExitKeptOpen,
        "failed exit send closed chooser or retained pending action");
    LoopbackPeer exitPeer(*connection);
    chooser.OnKeyDown(VK_RETURN);
    const std::vector<u8> exitRequest=exitPeer.ReceiveExact(4);
    Require(exitRequest==std::vector<u8>({0x8a,0x01,0x00,0x00}),"chooser exit option sent incorrect request bytes");
    const bool successfulExitClosed=chooser.m_show==0&&GetPendingDisconnectAction()==PendingDisconnectAction::ExitToWindows;
    Require(successfulExitClosed,
        "successful exit send did not close chooser and retain pending action");
    connection->Disconnect();
    ClearPendingDisconnectAction();

    chooser.SetShow(1);
    chooser.OnKeyDown(VK_DOWN); // Exit To Windows -> Return To Save Point.
    chooser.OnKeyDown(VK_DOWN); // Return To Save Point -> Character Select.
    chooser.OnKeyDown(VK_RETURN);
    const bool failedReturnKeptOpen=chooser.m_show!=0&&GetPendingDisconnectAction()==PendingDisconnectAction::None;
    Require(failedReturnKeptOpen,
        "failed character-select send closed chooser or retained pending action");
    LoopbackPeer returnPeer(*connection);
    chooser.OnKeyDown(VK_RETURN);
    const std::vector<u8> returnRequest=returnPeer.ReceiveExact(3);
    Require(returnRequest==std::vector<u8>({0xb2,0x00,0x01}),"chooser character-select option sent incorrect request bytes");
    const bool successfulReturnClosed=chooser.m_show==0&&GetPendingDisconnectAction()==PendingDisconnectAction::ReturnToCharSelect;
    Require(successfulReturnClosed,
        "successful character-select send did not close chooser and retain pending action");
    std::cout<<"{\"case_id\":\"disconnect_chooser_activation\",\"contract_id\":\"PK03.04.DISCONNECT-ACK\",\"fixture_id\":\"PK03.04-chooser-production-options\","<<ConfiguredSourceProvenanceJson()
        <<",\"expected_return_fixture_sha256\":\"9d28c69c5f71e98ec1faefd86fd4ea400703a7cd710340f588fcba01c5f1b1c7\",\"expected_exit_fixture_sha256\":\"8d2f50703af28decdd8e9f407c074d71c12c059f9259a780a1697ca64e526b4b\",\"build_preset\":\"mingw-qt-x64-ci; deploy OFF\",\"production_entry\":\"UIChooseWnd::OnKeyDown -> UIChooseWnd::ActivateSelection -> CModeMgr::SendMsg -> CGameMode::SendMsg\",\"stage\":\"headless_chooser_activation\",\"assertion_scope\":\"production chooser activation and send-result visibility; Qt rendering not exercised\",\"expected_visible_options\":[\"Character Select\",\"Exit To Windows\"],\"actual_visible_options\":[\""<<JsonEscape(chooser.GetEntryLabelForQt(0))<<"\",\""<<JsonEscape(chooser.GetEntryLabelForQt(2))<<"\"],\"failed_exit_kept_open\":"<<(failedExitKeptOpen?"true":"false")<<",\"successful_exit_closed\":"<<(successfulExitClosed?"true":"false")<<",\"failed_return_kept_open\":"<<(failedReturnKeptOpen?"true":"false")<<",\"successful_return_closed\":"<<(successfulReturnClosed?"true":"false")<<",\"return_request_hex\":\""<<Hex(returnRequest)<<"\",\"exit_request_hex\":\""<<Hex(exitRequest)<<"\",\"outcome\":\"PASS\"}\n";
    connection->Disconnect();
    ClearPendingDisconnectAction();
    SkillPacketReplayAccess::Restore(oldMode,oldModeType);
}

void FixedEveryBoundary(const std::vector<u8>& frame){for(size_t split=1;split<frame.size();++split){std::string caseId="fixed_fragment_"+std::to_string(split);SetCase(caseId);TestConnection c;LoopbackPeer p(c);std::vector<u8> out;
    p.Send(std::vector<u8>(frame.begin(),frame.begin()+split));PumpUntilBuffered(c,static_cast<int>(split));Require(out.empty(),"partial prefix returned bytes");Report(caseId+"_prefix",kFixedFixture,kFixedSha,{std::vector<u8>(frame.begin(),frame.begin()+split)},"","",split,c.BufferedBytes(),c);
    p.Send(std::vector<u8>(frame.begin()+split,frame.end()));out=PumpUntilPacket(c);Require(out==frame,"fragmented fixed packet changed");VerifyRoute(out,0x006c);Report(caseId,kFixedFixture,kFixedSha,{std::vector<u8>(frame.begin(),frame.begin()+split),std::vector<u8>(frame.begin()+split,frame.end())},Hex(frame),Hex(out),0,c.BufferedBytes(),c);}}
void FixedCoalesced(const std::vector<u8>& frame){SetCase("fixed_coalesced");TestConnection c;LoopbackPeer p(c);std::vector<u8> both=frame;both.insert(both.end(),frame.begin(),frame.end());p.Send(both);WaitKernelPending(c,static_cast<int>(both.size()));auto first=ReadOne(c);VerifyRoute(first,0x006c);Require(first==frame,"first coalesced packet bytes differ");Report("fixed_coalesced_first",kFixedFixture,kFixedSha,{both},Hex(frame),Hex(first),-1,c.BufferedBytes(),c,"open","asserted",static_cast<int>(frame.size()));auto second=ReadOne(c);VerifyRoute(second,0x006c);Require(second==frame&&c.BufferedBytes()==0,"second coalesced packet/remainder mismatch");Report("fixed_coalesced_second",kFixedFixture,kFixedSha,{both},Hex(frame),Hex(second),0,c.BufferedBytes(),c);}
void VariableEveryBoundary(const std::vector<u8>& variable,const std::vector<u8>& fixed){for(size_t split=1;split<variable.size();++split){std::string caseId="variable_fragment_"+std::to_string(split);SetCase(caseId);TestConnection c;LoopbackPeer p(c);std::vector<u8> out;
    p.Send(std::vector<u8>(variable.begin(),variable.begin()+split));PumpUntilBuffered(c,static_cast<int>(split));Require(out.empty(),"partial variable prefix returned bytes");Report(caseId+"_prefix",kVariableFixture,kVariableSha,{std::vector<u8>(variable.begin(),variable.begin()+split)},"","",split,c.BufferedBytes(),c);
    std::vector<u8> rest(variable.begin()+split,variable.end());rest.insert(rest.end(),fixed.begin(),fixed.end());p.Send(rest);out=PumpUntilPacket(c);VerifyRoute(out,0x0069);Require(out==variable,"variable frame changed");Report(caseId,kVariableFixture,kVariableSha,{std::vector<u8>(variable.begin(),variable.begin()+split),rest},Hex(variable),Hex(out),-1,c.BufferedBytes(),c);auto next=PumpUntilPacket(c);VerifyRoute(next,0x006c);Require(next==fixed,"fixed frame after variable frame lost");Report(caseId+"_adjacent_fixed",kFixedFixture,kFixedSha,{rest},Hex(fixed),Hex(next),0,c.BufferedBytes(),c);}}
void ReportTypedReceiveError(const char* id,CRagConnection::ReceiveError expected,CRagConnection::ReceiveError actual){std::cout<<"{\"case_id\":\""<<id<<"_typed_error\",\"contract_id\":\"PK05.UNKNOWN-OPCODE\",\"expected_receive_error\":\""<<ReceiveErrorName(expected)<<"\",\"actual_receive_error\":\""<<ReceiveErrorName(actual)<<"\",\"outcome\":\"PASS\"}\n";}
void RejectUnframedInput(const std::vector<u8>& prefix,const std::vector<u8>& suffix,const char* id,CRagConnection::ReceiveError expectedError){SetCase(id);TestConnection c;LoopbackPeer p(c);std::vector<u8> input=prefix;input.insert(input.end(),suffix.begin(),suffix.end());p.Send(input);const auto deadline=Clock::now()+kTimeout;std::vector<u8> out;while(c.IsOpen()){if(Receive(c,out)){throw std::runtime_error("invalid stream boundary unexpectedly delivered a packet");}if(c.IsOpen())WaitReadable(c,deadline);}Require(c.GetLastReceiveError()==expectedError,"invalid stream boundary returned the wrong typed error");Require(out.empty()&&c.BufferedBytes()==0,"invalid stream boundary left packet or queued suffix bytes");Report(id,kFixedFixture,kFixedSha,{input},"","",0,c.BufferedBytes(),c,"closed","asserted");ReportTypedReceiveError(id,expectedError,c.GetLastReceiveError());}
void RejectFragmentedUnframedInput(const std::vector<u8>& first,const std::vector<u8>& rest,const char* id,CRagConnection::ReceiveError expectedError){SetCase(id);TestConnection c;LoopbackPeer p(c);p.Send(first);PumpUntilBuffered(c,static_cast<int>(first.size()));Require(c.IsOpen(),"incomplete invalid boundary closed before enough header bytes arrived");p.Send(rest);const auto deadline=Clock::now()+kTimeout;std::vector<u8> out;while(c.IsOpen()){if(Receive(c,out)){throw std::runtime_error("fragmented invalid stream boundary unexpectedly delivered a packet");}if(c.IsOpen())WaitReadable(c,deadline);}Require(c.GetLastReceiveError()==expectedError,"fragmented invalid stream boundary returned the wrong typed error");Require(out.empty()&&c.BufferedBytes()==0,"fragmented invalid stream boundary left bytes queued");Report(id,kFixedFixture,kFixedSha,{first,rest},"","",0,c.BufferedBytes(),c,"closed","asserted");ReportTypedReceiveError(id,expectedError,c.GetLastReceiveError());}
void ProtocolErrorReconnectReset(const std::vector<u8>& fixed){SetCase("protocol_error_reconnect_reset");TestConnection c;{LoopbackPeer peer(c);peer.Send({0xff,0xff});std::vector<u8> out;const auto deadline=Clock::now()+kTimeout;while(c.IsOpen()){if(Receive(c,out))throw std::runtime_error("unknown packet emitted before reconnect check");if(c.IsOpen())WaitReadable(c,deadline);}Require(c.GetLastReceiveError()==CRagConnection::ReceiveError::UnknownOpcode,"unknown opcode error was not retained after disconnect");}LoopbackPeer reconnect(c);Require(c.GetLastReceiveError()==CRagConnection::ReceiveError::None,"reconnect retained stale receive error");reconnect.Send(fixed);const auto packet=PumpUntilPacket(c);Require(packet==fixed,"valid framing did not resume after protocol-error reconnect");Report("protocol_error_reconnect_reset",kFixedFixture,kFixedSha,{fixed},Hex(fixed),Hex(packet),0,c.BufferedBytes(),c);std::cout<<"{\"case_id\":\"protocol_error_reconnect_reset_error\",\"contract_id\":\"PK05.UNKNOWN-OPCODE\",\"error_before_reconnect\":\"UnknownOpcode\",\"error_after_reconnect\":\""<<ReceiveErrorName(c.GetLastReceiveError())<<"\",\"reconnected_packet_hex\":\""<<Hex(packet)<<"\",\"outcome\":\"PASS\"}\n";}
void EofCase(const std::vector<u8>& bytes,size_t count,const char* id,const char* fixture,const char* sha){SetCase(id);TestConnection c;LoopbackPeer p(c);if(count){p.Send(std::vector<u8>(bytes.begin(),bytes.begin()+count));PumpUntilBuffered(c,static_cast<int>(count));}std::vector<u8> out;if(count)Require(!Receive(c,out),"truncated packet returned at EOF case");CloseSocket(p.peer.value);p.peer.value=INVALID_SOCKET;WaitReadable(c,Clock::now()+kTimeout);Require(!Receive(c,out),"EOF should not return a packet");Require(!c.IsOpen()&&c.BufferedBytes()==0,"EOF did not close socket and clear partial queue");Report(id,fixture,sha,{std::vector<u8>(bytes.begin(),bytes.begin()+count)},"",Hex(out),0,c.BufferedBytes(),c,"closed");}
void OutboundTransportCapture(const std::vector<u8>& frame){SetCase("outbound_transport_capture");TestConnection c;LoopbackPeer p(c);Require(!c.SendPacket(nullptr,static_cast<int>(frame.size())),"null outbound buffer was accepted");Require(!c.SendPacket(reinterpret_cast<const char*>(frame.data()),0),"zero-length outbound packet was accepted");Require(c.SendPacket(reinterpret_cast<const char*>(frame.data()),static_cast<int>(frame.size())),"production SendPacket rejected structural sample");auto captured=p.ReceiveExact(frame.size());Require(captured==frame,"mock peer captured different outbound bytes");std::cout<<"{\"case_id\":\"outbound_transport_capture\",\"contract_id\":\"PK03.03.transport-only\",\"fixture_id\":\"PK03.02-map-006c-synthetic\",\"expected_fixture_sha256\":\"a8649967f8ca13630d18e21c07eaad14b309a54534f1cd9f633b692c3b2f341f\","<<ConfiguredSourceProvenanceJson()<<",\"production_entry\":\"CRagConnection::SendPacket -> CConnection::Send/FlushSendQueue\",\"expected_packet_hex\":\""<<Hex(frame)<<"\",\"captured_packet_hex\":\""<<Hex(captured)<<"\",\"invalid_null_rejected\":true,\"invalid_empty_rejected\":true,\"outcome\":\"PASS\"}\n";}
void GroundSkillTextRequestReplay(){
    constexpr std::string_view fixtureHex="7e00000000030000002301006400000000000000c8006669787475726500000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000";
    const std::vector<u8> expected=Unhex(fixtureHex);
    Require(expected.size()==102,"independent selected-source ground-skill fixture length changed");
    const ro::net::MapGameplaySendProfile& profile=ro::net::GetActiveMapGameplaySendProfile();
    Require(profile.id==ro::net::MapGameplaySendProfileId::PacketVer23 && profile.useSkillToPosInfo==0x007E,
        "fixture requires the PacketVer23 0x007E ground-skill profile");
    SetCase("ground_skill_text_request_send_capture");
    CRagConnection* const connection=CRagConnection::instance();
    CGameMode mode;
    connection->Disconnect();
    Require(!mode.SendSkillToGroundWithText(0x0123,3,100,200,"fixture"),
        "valid ground-skill request reported success while the connection was closed");
    LoopbackPeer peer(*connection);
    const std::string tooLong(80,'X');
    Require(!mode.SendSkillToGroundWithText(0x0123,3,100,200,tooLong),"ground-skill sender accepted text beyond its 79-byte bound");
    Require(!mode.SendSkillToGroundWithText(0,3,100,200,"fixture"),"ground-skill sender accepted zero skill ID");
    Require(!mode.SendSkillToGroundWithText(0x0123,0,100,200,"fixture"),"ground-skill sender accepted zero skill level");
    Require(!mode.SendSkillToGroundWithText(0x0123,3,-1,200,"fixture"),"ground-skill sender accepted a negative X coordinate");
    Require(!mode.SendSkillToGroundWithText(0x0123,3,100,0x8000,"fixture"),"ground-skill sender accepted an out-of-range signed Y coordinate");
    Require(SelectSocket(peer.peer.value,true,false,std::chrono::milliseconds(50))==0,"rejected ground-skill requests emitted socket bytes");
    Require(mode.SendSkillToGroundWithText(0x0123,3,100,200,"fixture"),"production ground-skill sender rejected the valid server-layout sample");
    const std::vector<u8> captured=peer.ReceiveExact(expected.size());
    Require(captured==expected,"captured production ground-skill request differs from independent server fixture");
    Require(SelectSocket(peer.peer.value,true,false,std::chrono::milliseconds(50))==0,"ground-skill sender emitted unexpected trailing bytes");
    connection->Disconnect();
    std::cout<<"{\"case_id\":\"ground_skill_text_request_send_capture\",\"contract_id\":\"PK01.P05.007E\",\"fixture_id\":\"P05-skill-ground-text-synthetic\",\"expected_fixture_sha256\":\"6964f423166f13b756fc9b47180d16243873390f38e5c9aef2a912738c3efbd6\","<<ConfiguredSourceProvenanceJson()<<",\"profile\":23,\"production_entry\":\"CGameMode::SendSkillToGroundWithText -> SendUseSkillToPosInfoPacket -> BuildActiveUseSkillToPosInfoPacket -> CRagConnection::SendPacket -> loopback TCP\",\"expected_packet_hex\":\""<<Hex(expected)<<"\",\"captured_packet_hex\":\""<<Hex(captured)<<"\",\"frame_length\":102,\"closed_connection_rejected\":true,\"invalid_skill_level_coordinate_and_text_rejected_without_send\":true,\"ui_caller_present\":false,\"outcome\":\"PASS\"}\n";
}
void ActionRequestBuilderReplay(){SetCase("action_request_builder_replay");using namespace ro::net;const MapGameplaySendProfile& profile=GetActiveMapGameplaySendProfile();Require(profile.id==MapGameplaySendProfileId::PacketVer23&&profile.actionRequest==0x0437,"replay requires active profile 23 / opcode 0x0437");struct Sample{u32 target;u8 action;const char* hex;};const Sample samples[]={{0x11223344,0x00,"37044433221100"},{0x11223344,0x07,"37044433221107"},{0,0x02,"37040000000002"},{0,0x03,"37040000000003"}};std::vector<u8> expectedStream;std::vector<std::string> expectedFrames;
    for(const Sample& sample:samples){std::array<u8,7> out{};int length=0;Require(BuildActiveActionRequestPacket(sample.target,sample.action,out.data(),static_cast<int>(out.size()),&length),"production action request builder rejected approved sample");Require(length==static_cast<int>(out.size()),"production action request builder returned wrong frame length");std::vector<u8> frame(out.begin(),out.end());Require(Hex(frame)==sample.hex,"production action request builder bytes differ from independent source sample");expectedFrames.emplace_back(sample.hex);expectedStream.insert(expectedStream.end(),frame.begin(),frame.end());}
    std::array<u8,7> sentinel{};sentinel.fill(0xa5);int outLength=123;Require(!BuildActiveActionRequestPacket(1,0,sentinel.data(),6,&outLength),"undersized action builder buffer was accepted");Require(std::all_of(sentinel.begin(),sentinel.end(),[](u8 b){return b==0xa5;}),"failed builder changed sentinel output storage");Require(!BuildActiveActionRequestPacket(1,0,nullptr,7,&outLength),"null action builder output was accepted");std::array<u8,7> valid{};Require(!BuildActiveActionRequestPacket(1,0,valid.data(),7,nullptr),"null action builder length output was accepted");
    CRagConnection* connection=CRagConnection::instance();LoopbackPeer p(*connection);Require(!SendActionRequestPacket(0,0),"zero-target single attack passed production helper guard");Require(!SendActionRequestPacket(0,7),"zero-target repeat attack passed production helper guard");Require(SelectSocket(p.peer.value,true,false,std::chrono::milliseconds(50))==0,"rejected action helper emitted unexpected prefix");for(const Sample& sample:samples)Require(SendActionRequestPacket(sample.target,sample.action),"production action request sender rejected approved sample");const std::vector<u8> captured=p.ReceiveExact(expectedStream.size());Require(captured==expectedStream,"mock peer production action sender stream differs from independent frame sequence");connection->Disconnect();Require(!SendActionRequestPacket(0x11223344,0),"production action sender accepted closed connection");std::cout<<"{\"case_id\":\"action_request_builder_replay\",\"contract_id\":\"PK03.03.PK46-action-request-serialization\","<<ConfiguredSourceProvenanceJson()<<",\"profile\":23,\"production_entry\":\"SendActionRequestPacket -> BuildActiveActionRequestPacket -> CRagConnection::SendPacket\",\"expected_frames_hex\":[\""<<expectedFrames[0]<<"\",\""<<expectedFrames[1]<<"\",\""<<expectedFrames[2]<<"\",\""<<expectedFrames[3]<<"\"],\"captured_stream_hex\":\""<<Hex(captured)<<"\",\"builder_failures\":3,\"zero_target_attack_and_repeat_rejected\":true,\"closed_connection_rejected\":true,\"outcome\":\"PASS\"}\n";}
}
void DisconnectAckDecisionReplay(){
    SetCase("disconnect_ack_decision_and_timeout","disconnect_ack_contract","PK03.04.DISCONNECT-ACK","GetExpiredDisconnectAction/DecideDisconnectAck");
    ClearPendingDisconnectAction();
    Require(BeginPendingDisconnectAction(PendingDisconnectAction::ReturnToCharSelect),"could not begin return-to-character request");
    Require(!BeginPendingDisconnectAction(PendingDisconnectAction::ExitToWindows),"duplicate disconnect request was accepted");
    MarkPendingDisconnectActionSent(0);
    GameModePacketLoopResult packetBudgetExhausted{};
    packetBudgetExhausted.receiveDrained=false;
    Require(!ApplyDisconnectTimeoutAfterPacketLoop(packetBudgetExhausted,15000),"packet-budget exhaustion applied the production timeout fallback");
    Require(GetPendingDisconnectAction()==PendingDisconnectAction::ReturnToCharSelect,"packet-budget exhaustion cleared the pending action");
    Require(GetExpiredDisconnectAction(true,14999)==PendingDisconnectAction::None,"request expired before the 15-second deadline");
    Require(GetExpiredDisconnectAction(true,15000)==PendingDisconnectAction::ReturnToCharSelect,"zero start tick did not expire at deadline");
    ClearPendingDisconnectAction();

    constexpr u32 wrappedStart=0xFFFFFF00u;
    constexpr u32 wrappedDeadline=wrappedStart+15000u;
    Require(BeginPendingDisconnectAction(PendingDisconnectAction::ExitToWindows),"could not begin exit request");
    MarkPendingDisconnectActionSent(wrappedStart);
    Require(GetExpiredDisconnectAction(true,wrappedDeadline-1u)==PendingDisconnectAction::None,"wrapped timer expired early");
    Require(GetExpiredDisconnectAction(true,wrappedDeadline)==PendingDisconnectAction::ExitToWindows,"wrapped timer did not expire at deadline");
    ClearPendingDisconnectAction();

    Require(DecideDisconnectAck(PendingDisconnectAction::ExitToWindows,0)==DisconnectAckDecision::ExitToWindows,"accepted exit acknowledgement was not recognized");
    Require(DecideDisconnectAck(PendingDisconnectAction::ReturnToCharSelect,0)==DisconnectAckDecision::Ignore,"return-to-character result 0 was treated as process exit");
    Require(DecideDisconnectAck(PendingDisconnectAction::ReturnToCharSelect,1)==DisconnectAckDecision::Refused,"return-to-character refusal was not recognized");
    Require(DecideDisconnectAck(PendingDisconnectAction::None,1)==DisconnectAckDecision::Ignore,"orphan refusal acknowledgement changed state");
    Require(DecideDisconnectAck(PendingDisconnectAction::ExitToWindows,2)==DisconnectAckDecision::Ignore,"unknown acknowledgement result was not ignored");
    Require(IsReturnToCharSelectAck(PendingDisconnectAction::ReturnToCharSelect,1),"valid 0x00B3 acknowledgement was not recognized");
    Require(!IsReturnToCharSelectAck(PendingDisconnectAction::ExitToWindows,1),"0x00B3 cleared a different pending action");
    std::cout<<"{\"case_id\":\"disconnect_ack_decision_and_timeout\",\"contract_id\":\"PK03.04.DISCONNECT-ACK\",\"zero_tick\":true,\"tick_wraparound\":true,\"packet_budget_not_drained\":true,\"result_matrix\":true,\"outcome\":\"PASS\"}\n";
}
void LoginStatusPrecedenceReplay(){
    SetCase("disconnect_timeout_login_status","disconnect_ack_contract","PK03.04.DISCONNECT-ACK","CLoginMode::SetLoginStatus -> ConsumeDeferredLoginStatus");
    LoginModeReplayAccess mode;
    LoginModeReplayAccess::AdoptDeferredStatus(mode,"Login: disconnect timed out.");
    // ConnectChar and PollNetwork update the visible status during the
    // fallback path. They must not consume the notice; only a stable Login or
    // CharSelect state does that.
    const std::string transientStatuses[]={
        "Login: char server connection failed.",
        "Login: received packet 0x0069 (6 bytes)",
        "Login: no char server to connect to."
    };
    for (const std::string& transient : transientStatuses) {
        LoginModeReplayAccess::SetStatus(mode,transient.c_str());
        Require(LoginModeReplayAccess::DeferredStatus(mode)=="Login: disconnect timed out.","transient LoginMode status consumed the deferred timeout notice");
    }
    const std::string stableLoginStatus=LoginModeReplayAccess::ConsumeStableStatus(mode,"Login: ready.");
    Require(stableLoginStatus=="Login: disconnect timed out.","timeout notice did not override the normal login status");
    Require(LoginModeReplayAccess::DeferredStatus(mode).empty(),"timeout notice was not consumed at the stable login screen");
    LoginModeReplayAccess::SetStatus(mode,stableLoginStatus.c_str());
    Require(g_windowMgr.GetLoginStatus()=="Login: disconnect timed out.","stable LoginMode status writer did not display the adopted timeout notice");
    std::string charSelectDeferred="Login: disconnect timed out.";
    Require(ConsumeDeferredLoginStatus(charSelectDeferred,"")=="Login: disconnect timed out.","timeout notice did not override CharSelect's normal empty status");
    Require(charSelectDeferred.empty(),"timeout notice was not consumed at CharSelect");
    std::string noDeferred;
    Require(ConsumeDeferredLoginStatus(noDeferred,"Login: ready.")=="Login: ready.","normal login status changed without a pending notice");
    std::cout<<"{\"case_id\":\"disconnect_timeout_login_status\",\"contract_id\":\"PK03.04.DISCONNECT-ACK\",\"production_entry\":\"CLoginMode::SetLoginStatus / ConsumeDeferredLoginStatus\",\"deferred_notice_survives_transient_setloginstatus_writes\":true,\"login_ready_precedence\":true,\"char_select_precedence\":true,\"one_shot\":true,\"outcome\":\"PASS\"}\n";
}
void LoginCharConnectionFailureReplay()
{
    SetCase("login_char_connection_failure","disconnect_ack_contract","PK03.04.DISCONNECT-ACK","CLoginMode::OnChangeState(LoginSubMode_ConnectChar) -> CRagConnection::Connect -> ShowConnectionFailedDialog");
    CRagConnection* connection=CRagConnection::instance();
    connection->Disconnect();

    char oldAddress[sizeof(g_session.m_charServerAddr)]{};
    std::memcpy(oldAddress,g_session.m_charServerAddr,sizeof(oldAddress));
    const int oldPort=g_session.m_charServerPort;
    std::memset(g_session.m_charServerAddr,0,sizeof(g_session.m_charServerAddr));
    std::memcpy(g_session.m_charServerAddr,"127.0.0.1",sizeof("127.0.0.1"));
    g_session.m_charServerPort=1; // loopback refusal; no server or account transaction

    LoginModeReplayAccess mode;
    LoginModeReplayAccess::AdoptDeferredStatus(mode,"Login: disconnect timed out.");
    mode.OnChangeState(CLoginMode::LoginSubMode_ConnectChar);
    const bool failedStatus=g_windowMgr.GetLoginStatus()=="Login: char server connection failed.";
    const bool schedulesLogin=mode.NextSubMode()==CLoginMode::LoginSubMode_Login;
    const bool socketClosed=!connection->IsOpen();
    const bool timeoutNoticePreserved=LoginModeReplayAccess::DeferredStatus(mode)=="Login: disconnect timed out.";
    Require(failedStatus,"failed char-server connect did not write its existing failure status");
    Require(schedulesLogin,"failed char-server connect did not schedule return to Login");
    Require(socketClosed,"failed char-server connect left the transport open");
    Require(timeoutNoticePreserved,"failed char-server connection consumed the deferred timeout notice");

    mode.OnChangeState(CLoginMode::LoginSubMode_Login);
    const bool timeoutNoticeDisplayed=g_windowMgr.GetLoginStatus()=="Login: disconnect timed out.";
    const bool timeoutNoticeConsumed=LoginModeReplayAccess::DeferredStatus(mode).empty();
    Require(timeoutNoticeDisplayed,"stable Login state did not display the timeout notice after the existing connection failure path");
    Require(timeoutNoticeConsumed,"stable Login state did not consume the timeout notice exactly once");

    connection->Disconnect();
    std::memcpy(g_session.m_charServerAddr,oldAddress,sizeof(oldAddress));
    g_session.m_charServerPort=oldPort;
    g_windowMgr.RemoveAllWindows();
    std::cout<<"{\"case_id\":\"login_char_connection_failure\",\"contract_id\":\"PK03.04.DISCONNECT-ACK\",\"production_entry\":\"CLoginMode::OnChangeState(ConnectChar) -> CRagConnection::Connect -> ShowConnectionFailedDialog -> OnChangeState(Login)\",\"failure_status\":true,\"existing_connection_failed_dialog_branch_reached\":true,\"returns_to_login\":true,\"timeout_notice_preserved_then_displayed\":true,\"timeout_notice_consumed_once\":true,\"socket_closed\":true,\"network_scope\":\"loopback refusal only\",\"outcome\":\"PASS\"}\n";
}
void LoginPollTransientStatusReplay()
{
    SetCase("login_poll_transient_status","disconnect_ack_contract","PK03.04-poll-0283-status-sample","CLoginMode::PollNetwork -> CRagConnection::RecvPacket -> CLoginMode::SetLoginStatus");
    CRagConnection* connection=CRagConnection::instance();
    connection->Disconnect();
    LoginModeReplayAccess mode;
    LoginModeReplayAccess::AdoptDeferredStatus(mode,"Login: disconnect timed out.");
    LoginModeReplayAccess::SetConnected(mode,true);
    LoopbackPeer peer(*connection);
    const std::vector<u8> ignoredPacket{0x83,0x02,0x00,0x00,0x00,0x00};
    peer.Send(ignoredPacket);
    LoginModeReplayAccess::RunPollNetwork(mode);
    const bool pollStatus=g_windowMgr.GetLoginStatus()=="Login: received packet 0x0283 (6 bytes)";
    const bool noticePreserved=LoginModeReplayAccess::DeferredStatus(mode)=="Login: disconnect timed out.";
    Require(pollStatus,"production PollNetwork did not write its transient receive status");
    Require(noticePreserved,"production PollNetwork consumed the deferred timeout notice");

    LoginModeReplayAccess::SetConnected(mode,false);
    connection->Disconnect();
    mode.OnChangeState(CLoginMode::LoginSubMode_Login);
    const bool timeoutDisplayed=g_windowMgr.GetLoginStatus()=="Login: disconnect timed out.";
    const bool noticeConsumed=LoginModeReplayAccess::DeferredStatus(mode).empty();
    Require(timeoutDisplayed,"stable Login did not supersede actual PollNetwork status with the timeout notice");
    Require(noticeConsumed,"stable Login did not consume the notice after actual PollNetwork status");
    g_windowMgr.RemoveAllWindows();
    std::cout<<"{\"case_id\":\"login_poll_transient_status\",\"contract_id\":\"PK03.04.DISCONNECT-ACK\",\"fixture_id\":\"PK03.04-poll-0283-status-sample\",\"production_entry\":\"CLoginMode::PollNetwork -> CRagConnection::RecvPacket -> CLoginMode::SetLoginStatus -> OnChangeState(Login)\",\"input_packet_hex\":\"830200000000\",\"fixture_provenance\":\"synthetic fixed-length receive sample; no server-emission or semantic claim\",\"poll_status_written\":true,\"deferred_notice_preserved\":true,\"stable_login_notice_displayed_and_consumed\":true,\"network_scope\":\"loopback mock peer only\",\"outcome\":\"PASS\"}\n";
}
void DisconnectTimeoutFallbackReplay()
{
    SetCase("disconnect_timeout_fallback","disconnect_ack_contract","PK03.04.DISCONNECT-ACK","ProcessGameModePacketLoop -> ApplyDisconnectTimeoutAfterPacketLoop -> ApplyDisconnectTimeoutFallback -> ReturnToLoginModeWithStatus -> CModeMgr::Switch");
    CRagConnection* singleton=CRagConnection::instance();
    singleton->Disconnect();
    ClearPendingDisconnectAction();
    CGameMode gameMode;
    CGameModePacketRouter& router=GetProductionGameModePacketRouter();
    router.Clear();
    RegisterDefaultGameModePacketHandlers(router);
    TestConnection connection;
    LoopbackPeer peer(connection);
    Require(BeginPendingDisconnectAction(PendingDisconnectAction::ReturnToCharSelect),"could not stage pending action for EOF timeout fallback");
    MarkPendingDisconnectActionSent(0);
    CloseSocket(peer.peer.value);
    peer.peer.value=INVALID_SOCKET;
    GameModePacketLoopResult loopResult{};
    const auto deadline=Clock::now()+kTimeout;
    do {
        loopResult=ProcessGameModePacketLoop(gameMode,connection,router,4);
        if(!loopResult.receiveDrained)std::this_thread::sleep_for(std::chrono::milliseconds(1));
    } while(!loopResult.receiveDrained&&Clock::now()<deadline);
    const bool eofDrainedWithoutPacket=loopResult.packetsProcessed==0&&loopResult.receiveDrained&&!connection.IsOpen();
    Require(eofDrainedWithoutPacket,"EOF without acknowledgement did not drain/close the receive connection");
    const bool pendingRetainedAfterEof=GetPendingDisconnectAction()==PendingDisconnectAction::ReturnToCharSelect;
    Require(pendingRetainedAfterEof,"EOF without acknowledgement cleared pending request context");
    Require(!ApplyDisconnectTimeoutAfterPacketLoop(loopResult,14999),"EOF caused timeout before the deadline");
    Require(GetPendingDisconnectAction()==PendingDisconnectAction::ReturnToCharSelect,"early EOF timeout check cleared the pending action");

    const int oldReturnFlag=g_session.m_pendingReturnToCharSelect;
    SetPendingLoginStatus("");
    g_session.m_pendingReturnToCharSelect=0;
    g_modeMgr.Switch(1,"map");
    Require(ApplyDisconnectTimeoutAfterPacketLoop(loopResult,15000),"expired return action did not apply the production packet-loop timeout fallback");
    const bool returnTimeoutRequestsCharSelect=g_session.m_pendingReturnToCharSelect==1&&SkillPacketReplayAccess::NextType()==0;
    Require(returnTimeoutRequestsCharSelect,
        "return timeout did not request character-select login mode");
    const bool fallbackClearedActionAndMap=GetPendingDisconnectAction()==PendingDisconnectAction::None&&!singleton->IsOpen();
    Require(fallbackClearedActionAndMap,
        "return timeout did not clear action and disconnect map singleton");
    std::string adoptedStatus;
    AdoptPendingLoginStatus(adoptedStatus);
    const bool returnTimeoutStatusAdopted=ConsumeDeferredLoginStatus(adoptedStatus,"Login: ready.")=="Login: disconnect timed out.";
    Require(returnTimeoutStatusAdopted,
        "LoginMode status adoption did not preserve the timeout message over the ready status");
    Require(adoptedStatus.empty(),"LoginMode did not consume its adopted timeout status once");

    constexpr u32 wrappedStart=0xFFFFFF00u;
    constexpr u32 wrappedDeadline=wrappedStart+15000u;
    Require(BeginPendingDisconnectAction(PendingDisconnectAction::ExitToWindows),"could not stage pending exit timeout");
    MarkPendingDisconnectActionSent(wrappedStart);
    GameModePacketLoopResult drainedPacketLoop{};
    drainedPacketLoop.receiveDrained=true;
    g_session.m_pendingReturnToCharSelect=1;
    g_modeMgr.Switch(1,"map");
    Require(ApplyDisconnectTimeoutAfterPacketLoop(drainedPacketLoop,wrappedDeadline),"exit action did not expire across tick wrap through the production packet-loop fallback");
    const bool exitTimeoutSkipsCharSelect=g_session.m_pendingReturnToCharSelect==0&&SkillPacketReplayAccess::NextType()==0;
    Require(exitTimeoutSkipsCharSelect,
        "exit timeout incorrectly requested character-select resume");
    std::string exitStatus;
    AdoptPendingLoginStatus(exitStatus);
    const bool exitTimeoutStatusAdopted=ConsumeDeferredLoginStatus(exitStatus,"Login: select a server before connecting.")=="Login: disconnect timed out.";
    Require(exitTimeoutStatusAdopted,
        "exit timeout status did not override the normal server-selection status");
    g_session.m_pendingReturnToCharSelect=oldReturnFlag;
    std::cout<<"{\"case_id\":\"disconnect_timeout_fallback\",\"contract_id\":\"PK03.04.DISCONNECT-ACK\",\"fixture_id\":\"PK03.04-018b-no-ack-eof\","<<ConfiguredSourceProvenanceJson()
        <<",\"build_preset\":\"mingw-qt-x64-ci; deploy OFF\",\"production_entry\":\"ProcessGameModePacketLoop -> ApplyDisconnectTimeoutAfterPacketLoop -> ApplyDisconnectTimeoutFallback -> ReturnToLoginModeWithStatus -> CModeMgr::Switch\",\"stage\":\"headless_timeout_mode_handoff\",\"assertion_scope\":\"production packet-loop timeout decision/fallback and LoginMode status-adoption helpers; full CGameMode::OnUpdate and CLoginMode::OnInit frames not instantiated\",\"eof_drained_without_packet\":"<<(eofDrainedWithoutPacket?"true":"false")<<",\"eof_preserves_pending_action\":"<<(pendingRetainedAfterEof?"true":"false")<<",\"return_timeout_requests_char_select\":"<<(returnTimeoutRequestsCharSelect?"true":"false")<<",\"fallback_cleared_action_and_map\":"<<(fallbackClearedActionAndMap?"true":"false")<<",\"exit_timeout_skips_char_select\":"<<(exitTimeoutSkipsCharSelect?"true":"false")<<",\"return_timeout_status_adopted\":"<<(returnTimeoutStatusAdopted?"true":"false")<<",\"exit_timeout_status_adopted\":"<<(exitTimeoutStatusAdopted?"true":"false")<<",\"outcome\":\"PASS\"}\n";
    ClearPendingDisconnectAction();
}
void DisconnectRequestReplay(){
    SetCase("disconnect_request_wire_bytes","disconnect_ack_contract","PK03.04.DISCONNECT-ACK","CGameMode::SendMsg -> CRagConnection::SendPacket");
    CRagConnection* connection=CRagConnection::instance();
    connection->Disconnect();
    ClearPendingDisconnectAction();
    CGameMode mode;

    {
        LoopbackPeer peer(*connection);
        Require(mode.SendMsg(CGameMode::GameMsg_RequestReturnToCharSelect,0,0,0)!=0,"return-to-character request failed to send");
        const std::vector<u8> expected={0xb2,0x00,0x01};
        const std::vector<u8> captured=peer.ReceiveExact(expected.size());
        Require(captured==expected,"return-to-character request bytes differ");
        Require(GetPendingDisconnectAction()==PendingDisconnectAction::ReturnToCharSelect,"successful return request was not pending");
        Require(mode.SendMsg(CGameMode::GameMsg_RequestReturnToCharSelect,0,0,0)==0,"duplicate return request was accepted");
        Require(mode.SendMsg(CGameMode::GameMsg_RequestExitToWindows,0,0,0)==0,"different duplicate disconnect request was accepted");
        Require(SelectSocket(peer.peer.value,true,false,std::chrono::milliseconds(50))==0,"duplicate request emitted wire bytes");
        connection->Disconnect();
        ClearPendingDisconnectAction();
    }

    Require(mode.SendMsg(CGameMode::GameMsg_RequestExitToWindows,0,0,0)==0,"exit request succeeded without a connection");
    Require(GetPendingDisconnectAction()==PendingDisconnectAction::None,"failed exit send left pending state");
    {
        LoopbackPeer peer(*connection);
        Require(mode.SendMsg(CGameMode::GameMsg_RequestExitToWindows,0,0,0)!=0,"exit request failed to send");
        const std::vector<u8> expected={0x8a,0x01,0x00,0x00};
        const std::vector<u8> captured=peer.ReceiveExact(expected.size());
        Require(captured==expected,"exit request bytes differ");
        Require(GetPendingDisconnectAction()==PendingDisconnectAction::ExitToWindows,"successful exit request was not pending");
        connection->Disconnect();
        ClearPendingDisconnectAction();
    }
    std::cout<<"{\"case_id\":\"disconnect_request_wire_bytes\",\"contract_id\":\"PK03.04.DISCONNECT-ACK\",\"return_request_hex\":\"b20001\",\"exit_request_hex\":\"8a010000\",\"duplicate_rejected\":true,\"failed_send_cleared\":true,\"outcome\":\"PASS\"}\n";
}
void ProductionDisconnectAckEofReplay(){
    SetCase("disconnect_ack_buffered_before_eof","disconnect_ack_contract","PK03.04.DISCONNECT-ACK","CGameMode::OnUpdate -> ProcessGameModePacketLoop -> CRagConnection::RecvPacket -> CGameModePacketRouter::Dispatch");
    TestConnection connection;
    LoopbackPeer peer(connection);
    const std::vector<u8> prefix={0xf8,0x01};
    const std::vector<u8> acknowledgement={0x8b,0x01,0x01,0x00};
    std::vector<u8> coalesced=prefix;
    coalesced.insert(coalesced.end(),acknowledgement.begin(),acknowledgement.end());
    Require(connection.QueueBytes(coalesced),"could not queue coalesced production frames");
    CloseSocket(peer.peer.value);
    peer.peer.value=INVALID_SOCKET;

    ClearPendingDisconnectAction();
    Require(BeginPendingDisconnectAction(PendingDisconnectAction::ReturnToCharSelect),"could not set pending action for acknowledgement replay");
    MarkPendingDisconnectActionSent(0);
    CGameMode mode;
    CGameModePacketRouter& router=GetProductionGameModePacketRouter();
    router.Clear();
    RegisterDefaultGameModePacketHandlers(router);

    const GameModePacketLoopResult first=ProcessGameModePacketLoop(mode,connection,router,1);
    Require(first.packetsProcessed==1&&!first.receiveDrained,"one-packet budget was reported as drained");
    Require(GetExpiredDisconnectAction(first.receiveDrained,15000)==PendingDisconnectAction::None,"budget exhaustion triggered timeout");
    const GameModePacketLoopResult second=ProcessGameModePacketLoop(mode,connection,router,1);
    Require(second.packetsProcessed==1&&!second.receiveDrained,"buffered acknowledgement was not delivered before EOF");
    Require(GetPendingDisconnectAction()==PendingDisconnectAction::None,"refusal handler did not clear the matching action");
    const GameModePacketLoopResult third=ProcessGameModePacketLoop(mode,connection,router,1);
    Require(third.packetsProcessed==0&&third.receiveDrained,"EOF was not reported as receive-drained");
    Require(!connection.IsOpen()&&connection.BufferedBytes()==0,"EOF did not close the socket after draining coalesced frames");
    Require(GetExpiredDisconnectAction(third.receiveDrained,15000)==PendingDisconnectAction::None,"handled acknowledgement left a timeout");
    ClearPendingDisconnectAction();
    std::cout<<"{\"case_id\":\"disconnect_ack_buffered_before_eof\",\"contract_id\":\"PK03.04.DISCONNECT-ACK\",\"input_hex\":\"f8018b010100\",\"packets_dispatched\":2,\"ack_before_eof\":true,\"budget_does_not_expire\":true,\"outcome\":\"PASS\"}\n";
}
void ProductionAcceptedExitAckReplay(){
    SetCase("disconnect_ack_accepted_exit","disconnect_ack_contract","PK03.04.DISCONNECT-ACK","CGameMode::OnUpdate -> ProcessGameModePacketLoop -> HandleDisconnectAck");
    CRagConnection* connection=CRagConnection::instance();
    connection->Disconnect();
    ClearPendingDisconnectAction();
    LoopbackPeer peer(*connection);
    CGameMode mode;
    CGameModePacketRouter& router=GetProductionGameModePacketRouter();
    router.Clear();
    RegisterDefaultGameModePacketHandlers(router);

    const u8 orphanBytes[]={0x8b,0x01,0x00,0x00};
    PacketView orphan{0x018b,4,orphanBytes};
    Require(router.Dispatch(mode,orphan),"default router did not register 0x018B");
    Require(connection->IsOpen(),"orphan result 0 closed the socket without a pending exit request");

    const u8 restartAckBytes[]={0xb3,0x00,0x01};
    PacketView restartAck{0x00b3,3,restartAckBytes};
    Require(BeginPendingDisconnectAction(PendingDisconnectAction::ExitToWindows),"could not set pending exit for 0x00B3 isolation test");
    Require(router.Dispatch(mode,restartAck),"default router did not register 0x00B3");
    Require(GetPendingDisconnectAction()==PendingDisconnectAction::ExitToWindows,"0x00B3 cleared a pending Windows-exit request");

    const std::vector<u8> acknowledgement={0x8b,0x01,0x00,0x00};
    peer.Send(acknowledgement);
    GameModePacketLoopResult result{};
    const auto deadline=Clock::now()+kTimeout;
    do {
        result=ProcessGameModePacketLoop(mode,*connection,router,1);
        if(result.packetsProcessed==0)std::this_thread::sleep_for(std::chrono::milliseconds(1));
    } while(result.packetsProcessed==0&&Clock::now()<deadline);
    Require(result.packetsProcessed==1,"accepted exit acknowledgement was not dispatched");
    Require(GetPendingDisconnectAction()==PendingDisconnectAction::None,"accepted exit acknowledgement did not clear pending state");
    Require(!connection->IsOpen(),"accepted exit acknowledgement did not disconnect");
    ClearPendingDisconnectAction();
    std::cout<<"{\"case_id\":\"disconnect_ack_accepted_exit\",\"contract_id\":\"PK03.04.DISCONNECT-ACK\",\"orphan_ignored\":true,\"0x00b3_isolated\":true,\"accepted_exit_dispatched\":true,\"outcome\":\"PASS\"}\n";
}
void LoginRefusalResultMappingReplay()
{
    SetCase("login_refusal_result_mapping", "PK44.LOGIN-REPLIES", "PK44-006a-auth-result-map-selected-source", "CLoginMode::OnRefuseLogin -> FormatLoginRefusalMessage");
    struct Sample { u8 code; const char* expected; };
    const Sample samples[] = {
        {0, "Login refused: ID not registered."},
        {1, "Login refused: incorrect password."},
        {2, "Login refused: account expired."},
        {3, "Login refused: rejected by the server."},
        {4, "Login refused: blocked by the GM team."},
        {5, "Login refused: client version is not accepted by the server."},
        {7, "Login refused: server is busy due to high population."},
        {8, "Login refused: too many accounts are connected from this company."},
        {9, "Login refused: ban by DBA."},
        {10, "Login refused: email not confirmed."},
        {11, "Login refused: ban by GM."},
        {12, "Login refused: account is being updated in the database."},
        {13, "Login refused: account is self-locked."},
        {14, "Login refused: account group is not permitted."},
        {15, "Login refused: account group is not permitted."},
        {99, "Login refused: account no longer exists."},
        {100, "Login refused: login information remains."},
        {101, "Login refused: account is under hacking investigation."},
        {102, "Login refused: account is under bug investigation."},
        {103, "Login refused: character deletion is in progress."},
        {104, "Login refused: spouse character deletion is in progress."},
    };
    Require(sizeof(PACKET_AC_REFUSE_LOGIN) == 23, "selected client 0x006A receive layout is not 23 bytes");

    std::vector<std::string> actualMessages;
    std::vector<std::string> inputFrames;
    for (const Sample& sample : samples) {
        std::vector<u8> packet(sizeof(PACKET_AC_REFUSE_LOGIN), 0);
        packet[0] = 0x6a;
        packet[2] = sample.code;
        const std::string actual = LoginModeReplayAccess::FormatRefusal(packet);
        Require(actual == sample.expected, "0x006A result code maps to a message different from the selected server meaning");
        actualMessages.push_back(actual);
        inputFrames.push_back(Hex(packet));
    }
    std::vector<u8> result6(sizeof(PACKET_AC_REFUSE_LOGIN), 0);
    result6[0] = 0x6a;
    result6[2] = 6;
    Require(LoginModeReplayAccess::FormatRefusal(result6).find("temporarily blocked") != std::string::npos,
        "0x006A result 6 without detail lost its temporary-block fallback");
    std::vector<u8> unknownResult(sizeof(PACKET_AC_REFUSE_LOGIN), 0);
    unknownResult[0] = 0x6a;
    unknownResult[2] = 16;
    Require(LoginModeReplayAccess::FormatRefusal(unknownResult) == "Login refused (code 16).",
        "unknown 0x006A result no longer preserves its numeric code");

    CRagConnection* connection = CRagConnection::instance();
    connection->Disconnect();
    LoopbackPeer peer(*connection);
    LoginModeReplayAccess mode;
    LoginModeReplayAccess::PrepareDisconnectState(mode);
    std::vector<u8> refusalPacket(sizeof(PACKET_AC_REFUSE_LOGIN), 0);
    refusalPacket[0] = 0x6a;
    refusalPacket[2] = 1;
    LoginModeReplayAccess::SetStatus(mode, "Login: awaiting replay packet.");
    peer.Send(refusalPacket);
    const auto receiveDeadline = Clock::now() + kTimeout;
    while (g_windowMgr.GetLoginStatus() != "Login refused: incorrect password."
           && Clock::now() < receiveDeadline) {
        LoginModeReplayAccess::RunPollNetwork(mode);
        if (g_windowMgr.GetLoginStatus() != "Login refused: incorrect password.") {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    Require(g_windowMgr.GetLoginStatus() == "Login refused: incorrect password.",
        "0x006A handler did not write the mapped refusal status");
    Require(LoginModeReplayAccess::DisconnectStateCleared(mode),
        "0x006A handler did not clear connection/hash state");
    Require(mode.NextSubMode() == CLoginMode::LoginSubMode_Login,
        "0x006A handler did not return to stable Login");
    Require(!connection->IsOpen(), "0x006A handler did not close the connection");

    std::cout << "{\"case_id\":\"login_refusal_result_mapping\",\"contract_id\":\"PK44.LOGIN-REPLIES\",\"fixture_id\":\"PK44-006a-auth-result-map-selected-source\",\"production_entry\":\"CRagConnection::RecvPacket -> CLoginMode::PollNetwork -> OnRefuseLogin -> FormatLoginRefusalMessage\",\"assertion_scope\":\"Windows WinMain.cpp ErrorMsg -> MessageBoxA path; visible modal not captured\",\"frame_length\":23,\"opcode\":\"0x006A\",\"result_byte_offset\":2,\"block_date_bytes_zero\":true,\"input_frames_hex\":[";
    for (size_t i = 0; i < inputFrames.size(); ++i) {
        if (i != 0) std::cout << ",";
        std::cout << "\"" << inputFrames[i] << "\"";
    }
    std::cout << "],\"result_codes\":[0,1,2,3,4,5,7,8,9,10,11,12,13,14,15,99,100,101,102,103,104],\"actual_messages\":[";
    for (size_t i = 0; i < actualMessages.size(); ++i) {
        if (i != 0) std::cout << ",";
        std::cout << "\"" << JsonEscape(actualMessages[i]) << "\"";
    }
    std::cout << "],\"result_6_empty_detail_fallback\":true,\"numeric_fallback_code16_preserved\":true,\"handler_status_and_cleanup_verified\":true,\"returns_to_login\":true,\"connection_closed\":true,\"outcome\":\"PASS\"}\n";
}

void LoginDisconnectResultReplay()
{
    SetCase("login_disconnect_result_mapping", "PK44.LOGIN-REPLIES", "PK44-0081-auth-result-selected-source", "CRagConnection::RecvPacket -> CLoginMode::PollNetwork -> OnDisconnectMsg");
    struct Sample { u8 code; const char* expected; };
    const Sample samples[] = {
        {1, "Disconnected by server: login service unavailable or access restricted."},
        {8, "Disconnected by server: the previous login session is still online."},
    };
    constexpr size_t kNotifyBanPacketLength = sizeof(u16) + sizeof(u8);
    Require(kNotifyBanPacketLength == 3, "selected 0x0081 fixed packet layout is not three bytes");
    CRagConnection* connection = CRagConnection::instance();
    LoginModeReplayAccess mode;
    std::vector<std::string> frames;
    std::vector<std::string> messages;
    for (const Sample& sample : samples) {
        connection->Disconnect();
        LoopbackPeer peer(*connection);
        std::vector<u8> packet(kNotifyBanPacketLength, 0);
        packet[0] = 0x81;
        packet[2] = sample.code;
        LoginModeReplayAccess::PrepareDisconnectState(mode);
        LoginModeReplayAccess::SetStatus(mode, "Login: awaiting replay packet.");
        peer.Send(packet);
        const auto receiveDeadline = Clock::now() + kTimeout;
        while (g_windowMgr.GetLoginStatus() != sample.expected && Clock::now() < receiveDeadline) {
            LoginModeReplayAccess::RunPollNetwork(mode);
            if (g_windowMgr.GetLoginStatus() != sample.expected) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
        const std::string message = g_windowMgr.GetLoginStatus();
        Require(message == sample.expected, "0x0081 result did not map to its selected source meaning");
        Require(LoginModeReplayAccess::DisconnectStateCleared(mode), "0x0081 handler did not clear connection/hash state");
        Require(mode.NextSubMode() == CLoginMode::LoginSubMode_Login,
            "0x0081 handler did not return to stable Login");
        Require(!connection->IsOpen(), "0x0081 handler did not close the connection");
        frames.push_back(Hex(packet));
        messages.push_back(message);
    }
    std::vector<u8> unassigned(kNotifyBanPacketLength, 0);
    unassigned[0] = 0x81;
    unassigned[2] = 2;
    Require(LoginModeReplayAccess::FormatDisconnect(unassigned) == "Disconnected by server (code 2).",
        "0x0081 result without an active selected-source sender lost its numeric fallback");

    std::cout << "{\"case_id\":\"login_disconnect_result_mapping\",\"contract_id\":\"PK44.LOGIN-REPLIES\",\"fixture_id\":\"PK44-0081-auth-result-selected-source\",\"production_entry\":\"CRagConnection::RecvPacket -> CLoginMode::PollNetwork -> OnDisconnectMsg -> FormatLoginDisconnectMessage\",\"assertion_scope\":\"Windows WinMain.cpp ErrorMsg -> MessageBoxA path; visible modal not captured\",\"frame_length\":3,\"opcode\":\"0x0081\",\"result_byte_offset\":2,\"input_frames_hex\":[\"" << frames[0] << "\",\"" << frames[1] << "\"],\"result_codes\":[1,8],\"actual_messages\":[\"" << JsonEscape(messages[0]) << "\",\"" << JsonEscape(messages[1]) << "\"],\"login_status_matches_expected\":true,\"account_hash_handshake_cleared\":true,\"returns_to_login\":true,\"connection_closed\":true,\"numeric_fallback_code2_preserved\":true,\"production_framing_dispatch\":true,\"outcome\":\"PASS\"}\n";
}

void LoginAcceptReplyReplay()
{
    SetCase("login_accept_reply", "PK44.LOGIN-REPLIES", "PK44-0069-accept-login-selected-source", "CLoginMode::OnAcceptLogin");
    constexpr size_t kAcceptLoginPrefixLength = 47;
    constexpr size_t kServerRecordLength = 32;
    constexpr size_t kServerRecordCount = 2;
    constexpr size_t kPacketLength = kAcceptLoginPrefixLength + kServerRecordLength * kServerRecordCount;
    Require(sizeof(SERVER_ADDR) == kServerRecordLength, "client server-address record size differs from selected source layout");

    std::vector<u8> packet(kPacketLength, 0);
    packet[0] = 0x69;
    packet[1] = 0x00;
    packet[2] = static_cast<u8>(kPacketLength & 0xff);
    packet[3] = static_cast<u8>(kPacketLength >> 8);
    const auto PutU32Le = [&packet](size_t offset, u32 value) {
        packet[offset] = static_cast<u8>(value & 0xff);
        packet[offset + 1] = static_cast<u8>((value >> 8) & 0xff);
        packet[offset + 2] = static_cast<u8>((value >> 16) & 0xff);
        packet[offset + 3] = static_cast<u8>((value >> 24) & 0xff);
    };
    PutU32Le(4, 0x11223344u);
    PutU32Le(8, 0x55667788u);
    PutU32Le(12, 0xaabbccddu);
    packet[46] = 11;
    std::array<u8, kServerRecordLength> serverRecord{};
    serverRecord[0] = 192;
    serverRecord[1] = 168;
    serverRecord[2] = 1;
    serverRecord[3] = 10;
    serverRecord[4] = 0xe9;
    serverRecord[5] = 0x17;
    std::memcpy(serverRecord.data() + 6, "char-main", 9);
    serverRecord[26] = 3;
    serverRecord[28] = 0x34;
    serverRecord[29] = 0x12;
    serverRecord[30] = 2;
    Require(sizeof(serverRecord) == kServerRecordLength, "independent selected-source server record fixture is not 32 bytes");
    std::memcpy(packet.data() + kAcceptLoginPrefixLength, serverRecord.data(), serverRecord.size());
    std::array<u8, kServerRecordLength> backupServerRecord{};
    backupServerRecord[0] = 203;
    backupServerRecord[1] = 0;
    backupServerRecord[2] = 113;
    backupServerRecord[3] = 7;
    backupServerRecord[4] = 0xea;
    backupServerRecord[5] = 0x17;
    std::memcpy(backupServerRecord.data() + 6, "char-backup", 11);
    backupServerRecord[26] = 9;
    backupServerRecord[28] = 0x78;
    backupServerRecord[29] = 0x56;
    backupServerRecord[30] = 1;
    std::memcpy(packet.data() + kAcceptLoginPrefixLength + kServerRecordLength,
        backupServerRecord.data(), backupServerRecord.size());

    CRagConnection* const connection = CRagConnection::instance();
    connection->Disconnect();
    LoopbackPeer peer(*connection);
    LoginModeReplayAccess mode;
    LoginModeReplayAccess::PrepareHandshakeOnly(mode);
    LoginModeReplayAccess::SetStatus(mode, "Login: awaiting replay packet.");
    peer.Send(packet);
    const auto receiveDeadline = Clock::now() + kTimeout;
    while (g_windowMgr.GetLoginStatus() != "Login: account accepted by server."
           && Clock::now() < receiveDeadline) {
        LoginModeReplayAccess::RunPollNetwork(mode);
        if (g_windowMgr.GetLoginStatus() != "Login: account accepted by server.") {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    Require(g_session.m_authCode == 0x11223344u && g_session.m_aid == 0x55667788u
            && g_session.m_userLevel == 0xaabbccddu && g_session.m_sex == 1,
        "0x0069 fixed-prefix fields or regional-sex normalization differ from the selected wire fixture");
    Require(LoginModeReplayAccess::AcceptLoginFieldsMatch(mode),
        "0x0069 server record fields were not preserved from the source-derived byte fixture");
    Require(mode.NextSubMode() == CLoginMode::LoginSubMode_ConnectChar,
        "0x0069 with multiple char servers did not transition to ConnectChar");
    Require(g_windowMgr.GetLoginStatus() == "Login: account accepted by server.",
        "0x0069 acceptance status was not written");
    Require(LoginModeReplayAccess::HandshakeCleared(mode), "0x0069 acceptance did not clear login handshake state");

    SocketOwner charListener(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
    Require(charListener.value != INVALID_SOCKET, "0x0069 transition mock char-server socket creation failed");
    sockaddr_in charAddress{};
    charAddress.sin_family = AF_INET;
    charAddress.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    charAddress.sin_port = 0;
    Require(bind(charListener.value, reinterpret_cast<sockaddr*>(&charAddress), sizeof(charAddress)) == 0,
        "0x0069 transition mock char-server bind failed");
    socklen_t charAddressLength = sizeof(charAddress);
    Require(getsockname(charListener.value, reinterpret_cast<sockaddr*>(&charAddress), &charAddressLength) == 0,
        "0x0069 transition mock char-server port query failed");
    Require(listen(charListener.value, 1) == 0,
        "0x0069 transition mock char-server listen failed");
    std::vector<u8> transitionPacket = packet;
    const size_t firstServerOffset = kAcceptLoginPrefixLength;
    transitionPacket[firstServerOffset] = 127;
    transitionPacket[firstServerOffset + 1] = 0;
    transitionPacket[firstServerOffset + 2] = 0;
    transitionPacket[firstServerOffset + 3] = 1;
    const u16 charPort = ntohs(charAddress.sin_port);
    transitionPacket[firstServerOffset + 4] = static_cast<u8>(charPort & 0xff);
    transitionPacket[firstServerOffset + 5] = static_cast<u8>(charPort >> 8);
    connection->Disconnect();
    LoopbackPeer transitionPeer(*connection);
    LoginModeReplayAccess transitionMode;
    LoginModeReplayAccess::PrepareHandshakeOnly(transitionMode);
    LoginModeReplayAccess::SetStatus(transitionMode, "Login: awaiting accepted char endpoint.");
    transitionPeer.Send(transitionPacket);
    const auto transitionDeadline = Clock::now() + kTimeout;
    while (g_windowMgr.GetLoginStatus() != "Login: account accepted by server."
           && Clock::now() < transitionDeadline) {
        LoginModeReplayAccess::RunPollNetwork(transitionMode);
        if (g_windowMgr.GetLoginStatus() != "Login: account accepted by server.") {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    Require(g_windowMgr.GetLoginStatus() == "Login: account accepted by server."
            && transitionMode.NextSubMode() == CLoginMode::LoginSubMode_ConnectChar,
        "production 0x0069 acceptance did not schedule the character-server transition");
    transitionMode.OnChangeState(CLoginMode::LoginSubMode_ConnectChar);
    Require(connection->IsOpen() && LoginModeReplayAccess::IsConnected(transitionMode),
        "accepted 0x0069 character-server endpoint did not establish the production connection");
    Require(SelectSocket(charListener.value, true, false,
            std::chrono::duration_cast<std::chrono::milliseconds>(kTimeout)) > 0,
        "accepted 0x0069 endpoint did not reach the mock character server");
    SocketOwner acceptedChar(accept(charListener.value, nullptr, nullptr));
    Require(acceptedChar.value != INVALID_SOCKET,
        "accepted 0x0069 endpoint did not produce a mock character-server socket");
    const std::vector<u8> expectedEnter{
        0x65, 0x00, 0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22,
        0x11, 0xdd, 0xcc, 0xbb, 0xaa, 0x00, 0x00, 0x01};
    const std::vector<u8> capturedEnter = ReceiveExactSocket(acceptedChar.value, expectedEnter.size());
    Require(capturedEnter == expectedEnter,
        "accepted 0x0069 endpoint did not send the selected-source 17-byte CA_ENTER frame");
    Require(g_windowMgr.GetLoginStatus() == "Login: waiting for character list...",
        "accepted 0x0069 endpoint did not enter the character-list wait state");
    connection->Disconnect();
    g_windowMgr.RemoveAllWindows();
    std::cout << "{\"case_id\":\"login_accept_to_char_server_transition\",\"contract_id\":\"PK44.LOGIN-REPLIES\",\"fixture_id\":\"PK44-0069-accept-to-0065-loopback-transition\",\"production_entry\":\"CRagConnection::RecvPacket -> CLoginMode::PollNetwork -> OnAcceptLogin -> OnChangeState(ConnectChar) -> Connect/SendPacket\",\"login_frame_hex\":\"" << Hex(transitionPacket) << "\",\"char_enter_expected_hex\":\"" << Hex(expectedEnter) << "\",\"char_enter_captured_hex\":\"" << Hex(capturedEnter) << "\",\"server_endpoint_from_0069_reached\":true,\"char_server_connected\":true,\"character_list_wait_status\":true,\"visible_acceptance_ui_not_verified\":true,\"network_scope\":\"loopback mock char server only\",\"outcome\":\"PASS\"}\n";

    std::vector<u8> partialRecordPacket(packet.begin(), packet.begin() + kAcceptLoginPrefixLength + kServerRecordLength);
    partialRecordPacket.push_back(0xee);
    partialRecordPacket[2] = static_cast<u8>(partialRecordPacket.size() & 0xff);
    partialRecordPacket[3] = static_cast<u8>(partialRecordPacket.size() >> 8);
    const u32 acceptedAuthCode = g_session.m_authCode;
    const u32 acceptedAid = g_session.m_aid;
    connection->Disconnect();
    LoopbackPeer malformedPeer(*connection);
    LoginModeReplayAccess malformedMode;
    LoginModeReplayAccess::PrepareHandshakeOnly(malformedMode);
    LoginModeReplayAccess::SetStatus(malformedMode, "Login: awaiting malformed replay packet.");
    malformedPeer.Send(partialRecordPacket);
    const std::string malformedStatus = "Login error: malformed AC_ACCEPT_LOGIN server list.";
    const auto malformedDeadline = Clock::now() + kTimeout;
    while (g_windowMgr.GetLoginStatus() != malformedStatus && Clock::now() < malformedDeadline) {
        LoginModeReplayAccess::RunPollNetwork(malformedMode);
        if (g_windowMgr.GetLoginStatus() != malformedStatus) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    Require(g_windowMgr.GetLoginStatus() == malformedStatus
            && malformedMode.NextSubMode() == CLoginMode::LoginSubMode_Login
            && LoginModeReplayAccess::MalformedAcceptLoginClearedList(malformedMode)
            && LoginModeReplayAccess::HandshakeCleared(malformedMode),
        "0x0069 partial server record was not rejected with cleared pending/list state");
    Require(g_session.m_authCode == acceptedAuthCode && g_session.m_aid == acceptedAid,
        "0x0069 malformed response overwrote accepted session credentials");
    connection->Disconnect();

    std::cout << "{\"case_id\":\"login_accept_reply\",\"contract_id\":\"PK44.LOGIN-REPLIES\",\"fixture_id\":\"PK44-0069-accept-login-two-servers-selected-source\",\"production_entry\":\"CRagConnection::RecvPacket -> CLoginMode::PollNetwork -> OnAcceptLogin\",\"frame_length\":" << packet.size() << ",\"opcode\":\"0x0069\",\"prefix_length\":47,\"server_record_length\":32,\"server_record_count\":2,\"input_frame_hex\":\"" << Hex(packet) << "\",\"auth_code\":287454020,\"account_id\":1432778632,\"login_id2\":2864434397,\"raw_sex\":11,\"normalized_sex\":1,\"first_server\":{\"ip_bytes_hex\":\"c0a8010a\",\"port\":6121,\"name\":\"char-main\",\"users\":3,\"type\":4660,\"new_flag\":2},\"second_server\":{\"ip_bytes_hex\":\"cb007107\",\"port\":6122,\"name\":\"char-backup\",\"users\":9,\"type\":22136,\"new_flag\":1},\"selected_server_index\":0,\"multiple_servers_preserved\":true,\"transitions_to_connect_char\":true,\"login_handshake_cleared\":true,\"production_framing_dispatch\":true,\"visible_ui_and_socket_connect_not_exercised\":true,\"outcome\":\"PASS\"}\n";
    std::cout << "{\"case_id\":\"login_accept_reply_partial_server_record\",\"contract_id\":\"PK44.LOGIN-REPLIES\",\"fixture_id\":\"PK44-0069-reject-partial-server-record\",\"production_entry\":\"CRagConnection::RecvPacket -> CLoginMode::PollNetwork -> OnAcceptLogin\",\"frame_length\":" << partialRecordPacket.size() << ",\"opcode\":\"0x0069\",\"prefix_length\":47,\"server_record_length\":32,\"complete_record_count\":1,\"partial_tail_bytes\":1,\"input_frame_hex\":\"" << Hex(partialRecordPacket) << "\",\"malformed_status\":\"" << malformedStatus << "\",\"returns_to_login\":true,\"server_list_cleared\":true,\"handshake_cleared\":true,\"session_credentials_preserved\":true,\"production_framing_dispatch\":true,\"outcome\":\"PASS\"}\n";

    constexpr size_t kServerRecordCapacity = 100;
    std::vector<u8> oversizedPacket(kAcceptLoginPrefixLength + kServerRecordLength * (kServerRecordCapacity + 1), 0);
    oversizedPacket[0] = 0x69;
    oversizedPacket[1] = 0x00;
    oversizedPacket[2] = static_cast<u8>(oversizedPacket.size() & 0xff);
    oversizedPacket[3] = static_cast<u8>(oversizedPacket.size() >> 8);
    connection->Disconnect();
    LoopbackPeer oversizedPeer(*connection);
    LoginModeReplayAccess oversizedMode;
    LoginModeReplayAccess::PrepareHandshakeOnly(oversizedMode);
    LoginModeReplayAccess::SetStatus(oversizedMode, "Login: awaiting oversized replay packet.");
    oversizedPeer.Send(oversizedPacket);
    const std::string oversizedStatus = "Login error: too many AC_ACCEPT_LOGIN server records.";
    const auto oversizedDeadline = Clock::now() + kTimeout;
    while (g_windowMgr.GetLoginStatus() != oversizedStatus && Clock::now() < oversizedDeadline) {
        LoginModeReplayAccess::RunPollNetwork(oversizedMode);
        if (g_windowMgr.GetLoginStatus() != oversizedStatus) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    Require(g_windowMgr.GetLoginStatus() == oversizedStatus
            && oversizedMode.NextSubMode() == CLoginMode::LoginSubMode_Login
            && LoginModeReplayAccess::MalformedAcceptLoginClearedList(oversizedMode)
            && LoginModeReplayAccess::HandshakeCleared(oversizedMode),
        "0x0069 server list beyond the 100-entry client capacity was not rejected");
    Require(g_session.m_authCode == acceptedAuthCode && g_session.m_aid == acceptedAid,
        "0x0069 oversized response overwrote accepted session credentials");
    connection->Disconnect();
    std::cout << "{\"case_id\":\"login_accept_reply_over_capacity_server_list\",\"contract_id\":\"PK44.LOGIN-REPLIES\",\"fixture_id\":\"PK44-0069-reject-101-server-records\",\"production_entry\":\"CRagConnection::RecvPacket -> CLoginMode::PollNetwork -> OnAcceptLogin\",\"frame_length\":" << oversizedPacket.size() << ",\"opcode\":\"0x0069\",\"prefix_length\":47,\"server_record_length\":32,\"client_capacity\":100,\"record_count\":101,\"malformed_status\":\"" << oversizedStatus << "\",\"returns_to_login\":true,\"server_list_cleared\":true,\"handshake_cleared\":true,\"session_credentials_preserved\":true,\"production_framing_dispatch\":true,\"outcome\":\"PASS\"}\n";
}

void LoginAcceptCharacterListReplay()
{
    SetCase("login_accept_character_list", "PK44.CHAR-LIST-ENTRY", "PK44-006b-two-character-records-selected-source", "CRagConnection::RecvPacket -> CLoginMode::PollNetwork -> OnAcceptChar");
    constexpr size_t kHeaderLength = 24;
    constexpr size_t kCharacterRecordLength = 108;
    constexpr size_t kCharacterRecordCount = 2;
    constexpr size_t kPacketLength = kHeaderLength + kCharacterRecordLength * kCharacterRecordCount;
    std::vector<u8> packet(kPacketLength, 0);
    packet[0] = 0x6b;
    packet[1] = 0x00;
    packet[2] = static_cast<u8>(kPacketLength & 0xff);
    packet[3] = static_cast<u8>(kPacketLength >> 8);
    u8* const record = packet.data() + kHeaderLength;
    const auto PutU32Le = [record](size_t offset, u32 value) {
        record[offset] = static_cast<u8>(value & 0xff);
        record[offset + 1] = static_cast<u8>((value >> 8) & 0xff);
        record[offset + 2] = static_cast<u8>((value >> 16) & 0xff);
        record[offset + 3] = static_cast<u8>((value >> 24) & 0xff);
    };
    PutU32Le(0, 0x12345678u);
    record[52] = 7; // selected server job
    record[70] = 18; // selected server headpalette
    std::memcpy(record + 74, "RuneA", 5);
    record[98] = 10;
    record[99] = 20;
    record[100] = 30;
    record[101] = 40;
    record[102] = 50;
    record[103] = 60;
    record[104] = 7; // selected server's 16-bit CharNum
    record[106] = 18; // selected server's 16-bit hairColor
    u8* const secondRecord = record + kCharacterRecordLength;
    secondRecord[0] = 0x21;
    secondRecord[1] = 0x43;
    secondRecord[2] = 0x65;
    secondRecord[3] = 0x87; // GID 0x87654321
    secondRecord[52] = 8;
    secondRecord[70] = 9;
    std::memcpy(secondRecord + 74, "RuneB", 5);
    secondRecord[104] = 3;
    secondRecord[106] = 7;

    CRagConnection* const connection = CRagConnection::instance();
    const auto receiveThroughPollNetwork = [connection](LoginModeReplayAccess& replayMode,
                                                         const std::vector<u8>& frame,
                                                         const char* expectedStatus) {
        connection->Disconnect();
        LoopbackPeer peer(*connection);
        LoginModeReplayAccess::SetStatus(replayMode, "Login: awaiting replay packet.");
        peer.Send(frame);
        const auto deadline = Clock::now() + kTimeout;
        while (g_windowMgr.GetLoginStatus() != expectedStatus && Clock::now() < deadline) {
            LoginModeReplayAccess::RunPollNetwork(replayMode);
            if (g_windowMgr.GetLoginStatus() != expectedStatus) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
        Require(g_windowMgr.GetLoginStatus() == expectedStatus,
            "production PollNetwork did not frame and dispatch the expected login response");
        connection->Disconnect();
    };

    LoginModeReplayAccess mode;
    receiveThroughPollNetwork(mode, packet, "Login: character list received from char server.");
    Require(LoginModeReplayAccess::AcceptedCharacterListFieldsMatch(mode),
        "selected-source 0x006B records were not both preserved and translated into client character records");
    Require(LoginModeReplayAccess::FirstCharacterSelected(mode),
        "selected-source 0x006B response did not select the first character by default");
    Require(mode.NextSubMode() == CLoginMode::LoginSubMode_CharSelect,
        "0x006B character response did not transition to CharSelect");
    Require(g_windowMgr.GetLoginStatus() == "Login: character list received from char server.",
        "0x006B character response status differs from expected");

    std::vector<u8> emptyPacket(kHeaderLength, 0);
    emptyPacket[0] = 0x6b;
    emptyPacket[1] = 0x00;
    emptyPacket[2] = static_cast<u8>(kHeaderLength);
    LoginModeReplayAccess emptyMode;
    receiveThroughPollNetwork(emptyMode, emptyPacket, "Login: no characters on account.");
    Require(emptyMode.GetCharCount() == 0
            && emptyMode.NextSubMode() == CLoginMode::LoginSubMode_CharSelect
            && g_windowMgr.GetLoginStatus() == "Login: no characters on account.",
        "valid selected-source empty 0x006B response did not enter empty character selection state");

    std::vector<u8> partialRecordPacket(kHeaderLength + kCharacterRecordLength + 1, 0);
    partialRecordPacket[0] = 0x6b;
    partialRecordPacket[2] = static_cast<u8>(partialRecordPacket.size() & 0xff);
    partialRecordPacket[3] = static_cast<u8>(partialRecordPacket.size() >> 8);
    LoginModeReplayAccess partialRecordMode;
    receiveThroughPollNetwork(partialRecordMode, partialRecordPacket,
        "Login error: unsupported char-list packet layout.");
    Require(partialRecordMode.GetCharCount() == 0
            && partialRecordMode.NextSubMode() == CLoginMode::LoginSubMode_Login,
        "0x006B partial-record layout did not clear the list and return to Login");

    constexpr size_t kClientCharacterCapacity = 12;
    std::vector<u8> overCapacityPacket(kHeaderLength + kCharacterRecordLength * (kClientCharacterCapacity + 1), 0);
    overCapacityPacket[0] = 0x6b;
    overCapacityPacket[2] = static_cast<u8>(overCapacityPacket.size() & 0xff);
    overCapacityPacket[3] = static_cast<u8>(overCapacityPacket.size() >> 8);
    LoginModeReplayAccess overCapacityMode;
    receiveThroughPollNetwork(overCapacityMode, overCapacityPacket,
        "Login error: too many character records in char-list response.");
    Require(overCapacityMode.GetCharCount() == 0
            && overCapacityMode.NextSubMode() == CLoginMode::LoginSubMode_Login,
        "0x006B record count beyond client capacity was not rejected and cleared");

    std::vector<u8> makePacket(sizeof(PACKET_HC_ACCEPT_MAKECHAR), 0);
    makePacket[0] = 0x6d;
    std::memcpy(makePacket.data() + sizeof(u16), record, kCharacterRecordLength);
    LoginModeReplayAccess createMode;
    receiveThroughPollNetwork(createMode, makePacket, "Login: character created successfully.");
    Require(LoginModeReplayAccess::AcceptedCharacterFieldsMatch(createMode),
        "selected-source 0x006D slot/hair-color values were not translated into the client character record");
    Require(createMode.NextSubMode() == CLoginMode::LoginSubMode_CharSelect,
        "0x006D character creation response did not transition to CharSelect");
    Require(g_windowMgr.GetLoginStatus() == "Login: character created successfully.",
        "0x006D character creation status differs from expected");

    std::cout << "{\"case_id\":\"login_accept_character_list\",\"contract_id\":\"PK44.CHAR-LIST-ENTRY\",\"fixture_id\":\"PK44-006b-two-character-records-selected-source\",\"production_entry\":\"CRagConnection::RecvPacket -> CLoginMode::PollNetwork -> OnAcceptChar\",\"frame_length\":" << packet.size() << ",\"opcode\":\"0x006B\",\"header_length\":24,\"record_length\":108,\"record_count\":2,\"input_frame_hex\":\"" << Hex(packet) << "\",\"records\":[{\"gid\":305419896,\"name\":\"RuneA\",\"job\":7,\"headpalette\":18,\"wire_slot_u16\":7,\"wire_hair_color_u16\":18,\"parsed_slot\":7,\"parsed_hair_color\":18,\"rename_flag_default\":1},{\"gid\":2271560481,\"name\":\"RuneB\",\"job\":8,\"headpalette\":9,\"wire_slot_u16\":3,\"wire_hair_color_u16\":7,\"parsed_slot\":3,\"parsed_hair_color\":7,\"rename_flag_default\":1}],\"first_character_selected\":true,\"char_select_transition\":true,\"visible_character_window_not_exercised\":true,\"outcome\":\"PASS\"}\n";
    std::cout << "{\"case_id\":\"login_accept_character_list_empty\",\"contract_id\":\"PK44.CHAR-LIST-ENTRY\",\"fixture_id\":\"PK44-006b-empty-list-selected-source\",\"production_entry\":\"CRagConnection::RecvPacket -> CLoginMode::PollNetwork -> OnAcceptChar\",\"frame_length\":24,\"opcode\":\"0x006B\",\"header_length\":24,\"record_count\":0,\"input_frame_hex\":\"" << Hex(emptyPacket) << "\",\"empty_list_status\":\"Login: no characters on account.\",\"char_select_transition\":true,\"outcome\":\"PASS\"}\n";
    std::cout << "{\"case_id\":\"login_accept_character_list_partial_record\",\"contract_id\":\"PK44.CHAR-LIST-ENTRY\",\"fixture_id\":\"PK44-006b-reject-partial-character-record\",\"production_entry\":\"CRagConnection::RecvPacket -> CLoginMode::PollNetwork -> OnAcceptChar\",\"frame_length\":" << partialRecordPacket.size() << ",\"opcode\":\"0x006B\",\"header_length\":24,\"record_length\":108,\"partial_tail_bytes\":1,\"input_frame_hex\":\"" << Hex(partialRecordPacket) << "\",\"unsupported_layout_status\":\"Login error: unsupported char-list packet layout.\",\"clears_character_list\":true,\"returns_to_login\":true,\"production_framing_dispatch\":true,\"outcome\":\"PASS\"}\n";
    std::cout << "{\"case_id\":\"login_accept_character_list_over_capacity\",\"contract_id\":\"PK44.CHAR-LIST-ENTRY\",\"fixture_id\":\"PK44-006b-reject-13-character-records\",\"production_entry\":\"CRagConnection::RecvPacket -> CLoginMode::PollNetwork -> OnAcceptChar\",\"frame_length\":" << overCapacityPacket.size() << ",\"opcode\":\"0x006B\",\"header_length\":24,\"record_length\":108,\"client_capacity\":12,\"record_count\":13,\"input_frame_hex\":\"" << Hex(overCapacityPacket) << "\",\"capacity_error_status\":\"Login error: too many character records in char-list response.\",\"clears_character_list\":true,\"returns_to_login\":true,\"production_framing_dispatch\":true,\"outcome\":\"PASS\"}\n";
    std::cout << "{\"case_id\":\"login_accept_make_character\",\"contract_id\":\"PK44.CHAR-CREATE\",\"fixture_id\":\"PK44-006d-make-character-selected-source\",\"production_entry\":\"CRagConnection::RecvPacket -> CLoginMode::PollNetwork -> OnAcceptMakeChar\",\"frame_length\":110,\"opcode\":\"0x006D\",\"character_record_length\":108,\"input_frame_hex\":\"" << Hex(makePacket) << "\",\"character_gid\":305419896,\"wire_slot_u16\":7,\"wire_hair_color_u16\":18,\"parsed_slot\":7,\"parsed_hair_color\":18,\"char_select_transition\":true,\"visible_character_window_not_exercised\":true,\"outcome\":\"PASS\"}\n";
}

void LoginSelectCharacterRequestReplay()
{
    SetCase("login_select_character_request", "PK44.LOGIN-REPLIES", "PK44-0066-select-character-selected-source", "CLoginMode::SendMsg -> OnChangeState(SelectChar) -> CRagConnection::SendPacket");
    CRagConnection* const connection = CRagConnection::instance();
    connection->Disconnect();
    LoopbackPeer peer(*connection);
    LoginModeReplayAccess mode;
    LoginModeReplayAccess::SeedCharacterForSelection(mode, 7);
    const msgresult_t accepted = mode.SendMsg(CLoginMode::LoginMsg_SelectCharacter, 0, 0, 0);
    Require(accepted != 0 && mode.NextSubMode() == CLoginMode::LoginSubMode_SelectChar,
        "character selection message did not select the valid local record");
    mode.OnChangeState(CLoginMode::LoginSubMode_SelectChar);
    const std::vector<u8> expected{0x66, 0x00, 0x07};
    const std::vector<u8> captured = peer.ReceiveExact(expected.size());
    Require(captured == expected,
        "selected-source 0x0066 outbound request did not encode the selected character slot");
    Require(g_windowMgr.GetLoginStatus() == "Login: selecting character...",
        "character selection request did not report its production status");
    std::cout << "{\"case_id\":\"login_select_character_request\",\"contract_id\":\"PK44.CHAR-LIST-ENTRY\",\"fixture_id\":\"PK44-0066-select-character-selected-source\",\"production_entry\":\"CLoginMode::SendMsg(LoginMsg_SelectCharacter) -> OnChangeState(SelectChar) -> CRagConnection::SendPacket\",\"expected_packet_hex\":\"660007\",\"captured_packet_hex\":\"" << Hex(captured) << "\",\"frame_length\":3,\"opcode\":\"0x0066\",\"selected_slot\":7,\"selection_message_accepted\":true,\"next_submode_select_char\":true,\"production_framing_dispatch\":true,\"onrun_state_dispatch_not_exercised\":true,\"qt_click_lifecycle_not_exercised\":true,\"outcome\":\"PASS\"}\n";
}

void LoginSelectCharacterRejectInvalidIndexReplay()
{
    SetCase("login_select_character_reject_invalid_index", "PK46.03", "PK46.03-loginmode-character-send-paths",
        "CLoginMode::SendMsg(LoginMsg_SelectCharacter) validates list index before state transition or send");
    CRagConnection* const connection = CRagConnection::instance();
    connection->Disconnect();
    LoopbackPeer peer(*connection);
    LoginModeReplayAccess mode;
    LoginModeReplayAccess::SeedCharacterForSelection(mode, 7);
    const int priorSubMode = mode.NextSubMode();
    Require(mode.SendMsg(CLoginMode::LoginMsg_SelectCharacter, 1, 0, 0) == 0,
        "character selection accepted an index past the one-entry list");
    Require(mode.NextSubMode() == priorSubMode,
        "rejected character selection changed the next submode");
    Require(SelectSocket(peer.peer.value, true, false, std::chrono::milliseconds(50)) == 0,
        "rejected character selection emitted a packet");
    connection->Disconnect();
    std::cout << "{\"case_id\":\"login_select_character_reject_invalid_index\",\"contract_id\":\"PK46.03\",\"fixture_id\":\"PK46.03-loginmode-character-send-paths\",\"character_count\":1,\"requested_index\":1,\"message_rejected\":true,\"next_submode_unchanged\":true,\"wire_bytes\":0,\"outcome\":\"PASS\"}" << std::endl;
}

void LoginCreateCharacterRequestReplay()
{
    SetCase("login_create_character_request", "PK44.CHAR-CREATE", "PK44-0067-make-character-selected-source", "CLoginMode::SendMsg(LoginMsg_CreateCharacter) -> CRagConnection::SendPacket");
    CRagConnection* const connection = CRagConnection::instance();
    connection->Disconnect();
    LoopbackPeer peer(*connection);
    LoginModeReplayAccess mode;
    LoginModeReplayAccess::SetConnected(mode, true);
    Require(mode.SendMsg(CLoginMode::LoginMsg_RequestMakeCharacter, 7, 0, 0) != 0,
        "character creation slot selection was rejected");
    const std::array<int, 8> params{1, 2, 3, 4, 5, 6, 18, 42};
    const char name[] = "RuneA";
    Require(mode.SendMsg(CLoginMode::LoginMsg_SetCharParam,
            reinterpret_cast<msgparam_t>(const_cast<int*>(params.data())), 0, 0) != 0,
        "character creation parameters were rejected");
    Require(mode.SendMsg(CLoginMode::LoginMsg_SetMakingCharName,
            reinterpret_cast<msgparam_t>(const_cast<char*>(name)), 0, 0) != 0,
        "character creation name was rejected");
    Require(mode.SendMsg(CLoginMode::LoginMsg_CreateCharacter, 0, 0, 0) != 0,
        "production character creation sender rejected the selected-source request");

    std::vector<u8> expected(37, 0);
    expected[0] = 0x67;
    std::memcpy(expected.data() + 2, "RuneA", 5);
    for (size_t index = 0; index < 6; ++index) {
        expected[26 + index] = static_cast<u8>(index + 1);
    }
    expected[32] = 7;
    expected[33] = 18;
    expected[35] = 42;
    const std::vector<u8> captured = peer.ReceiveExact(expected.size());
    Require(captured == expected,
        "selected-source 0x0067 outbound request differs from independent field-offset fixture");
    Require(g_windowMgr.GetLoginStatus() == "Login: requesting character creation...",
        "character creation sender did not publish its production status");
    g_windowMgr.RemoveAllWindows();
    std::cout << "{\"case_id\":\"login_create_character_request\",\"contract_id\":\"PK44.CHAR-CREATE\",\"fixture_id\":\"PK44-0067-make-character-selected-source\",\"production_entry\":\"CLoginMode::SendMsg(LoginMsg_RequestMakeCharacter/SetCharParam/SetMakingCharName/CreateCharacter) -> CRagConnection::SendPacket\",\"expected_packet_hex\":\"" << Hex(expected) << "\",\"captured_packet_hex\":\"" << Hex(captured) << "\",\"frame_length\":37,\"opcode\":\"0x0067\",\"name\":\"RuneA\",\"stats\":[1,2,3,4,5,6],\"slot\":7,\"hair_color_u16\":18,\"hair_style_u16\":42,\"production_send_capture\":true,\"qt_click_lifecycle_not_exercised\":true,\"outcome\":\"PASS\"}\n";
}

void LoginAccountRequestReplay()
{
    SetCase("login_account_request", "PK44.LOGIN-REQUESTS", "PK44-0064-raw-login-selected-profile", "CLoginMode::SendPlainAccountLogin -> CRagConnection::SendPacket");
    CRagConnection* const connection = CRagConnection::instance();
    connection->Disconnect();
    LoopbackPeer peer(*connection);
    const ServiceType originalService = g_serviceType;
    struct RestoreServiceType { ServiceType value; ~RestoreServiceType() { g_serviceType = value; } } restore{originalService};
    g_serviceType = ServiceAmerica;

    LoginModeReplayAccess mode;
    const char user[] = "packet-user";
    const char password[] = "packet-pass";
    Require(mode.SendMsg(CLoginMode::LoginMsg_SetUserId,
            reinterpret_cast<msgparam_t>(const_cast<char*>(user)), 0, 0) != 0,
        "account login username message was rejected");
    Require(mode.SendMsg(CLoginMode::LoginMsg_SetPassword,
            reinterpret_cast<msgparam_t>(const_cast<char*>(password)), 0, 0) != 0,
        "account login password message was rejected");
    LoginModeReplayAccess::RunPlainAccountLogin(mode);

    std::vector<u8> expected(55, 0);
    expected[0] = 0x64;
    expected[2] = 0x0e;
    expected[3] = 0x69;
    expected[4] = 0x32;
    expected[5] = 0x01;
    std::memcpy(expected.data() + 6, user, sizeof(user) - 1);
    std::memcpy(expected.data() + 30, password, sizeof(password) - 1);
    expected[54] = 1;
    const std::vector<u8> captured = peer.ReceiveExact(expected.size());
    Require(captured == expected,
        "selected 20080910 0x0064 outbound request differs from independent field-offset fixture");
    Require(g_windowMgr.GetLoginStatus() == "Login: waiting for account server response...",
        "plain account login sender did not publish its production status");
    std::cout << "{\"case_id\":\"login_account_request\",\"contract_id\":\"PK44.LOGIN-REQUESTS\",\"fixture_id\":\"PK44-0064-raw-login-selected-profile\",\"production_entry\":\"CLoginMode::SendMsg(SetUserId/SetPassword) -> SendPlainAccountLogin -> CRagConnection::SendPacket\",\"expected_packet_hex\":\"" << Hex(expected) << "\",\"captured_packet_hex\":\"" << Hex(captured) << "\",\"frame_length\":55,\"opcode\":\"0x0064\",\"client_version\":20080910,\"username\":\"packet-user\",\"password_fixture\":\"packet-pass\",\"clienttype\":1,\"production_send_capture\":true,\"connect_account_gate_not_exercised\":true,\"live_credentials_not_used\":true,\"outcome\":\"PASS\"}\n";
}

void LoginAccountAuxRequestReplay(bool channel)
{
    const char* const caseId = channel ? "login_account_channel_request" : "login_account_pcbang_request";
    const char* const fixtureId = channel
        ? "PK44-02b0-channel-login-selected-source-layout"
        : "PK44-0277-pcbang-login-selected-source-layout";
    const char* const senderPath = channel
        ? "CLoginMode::SendPlainAccountLogin -> SendLoginChannel -> CRagConnection::SendPacket"
        : "CLoginMode::SendPlainAccountLogin -> SendLoginPcBang -> CRagConnection::SendPacket";
    SetCase(caseId, "PK44.LOGIN-REQUESTS", fixtureId, senderPath);
    const char* oldDataDirValue = std::getenv("OPEN_MIDGARD_DATA_DIR");
    const bool hadDataDir = oldDataDirValue != nullptr;
    const std::string oldDataDir = oldDataDirValue ? oldDataDirValue : "";
    struct RestoreDataDir {
        bool hadValue;
        std::string value;
        ~RestoreDataDir() {
#if RO_PLATFORM_WINDOWS
            _putenv_s("OPEN_MIDGARD_DATA_DIR", hadValue ? value.c_str() : "");
#else
            if (hadValue) setenv("OPEN_MIDGARD_DATA_DIR", value.c_str(), 1);
            else unsetenv("OPEN_MIDGARD_DATA_DIR");
#endif
        }
    } restoreDataDir{hadDataDir, oldDataDir};

    const std::filesystem::path settingsDirectory = std::filesystem::temp_directory_path()
        / (std::string("open-midgard-packet-replay-account-variant-") + (channel ? "channel-" : "pcbang-") + std::to_string(GetTickCount()));
    std::filesystem::create_directories(settingsDirectory);
    struct RemoveSettingsDirectory {
        std::filesystem::path path;
        ~RemoveSettingsDirectory() { std::error_code error; std::filesystem::remove_all(path, error); }
    } removeSettingsDirectory{settingsDirectory};
#if RO_PLATFORM_WINDOWS
        Require(_putenv_s("OPEN_MIDGARD_DATA_DIR", settingsDirectory.string().c_str()) == 0,
        "could not point account-variant replay settings at the isolated temporary directory");
#else
    Require(setenv("OPEN_MIDGARD_DATA_DIR", settingsDirectory.string().c_str(), 1) == 0,
        "could not point account-variant replay settings at the isolated temporary directory");
#endif
    {
        std::ofstream settings(settingsDirectory / "open-midgard.ini", std::ios::binary | std::ios::trunc);
        Require(settings.is_open(), "could not write isolated account-variant settings fixture");
        settings << "[Login]\nAccountLoginPacket=" << (channel ? "channel" : "pcbang")
                 << "\nClientDateOverride=20080910\nClientTypeOverride=1\n";
        settings.close();
        Require(settings.good(), "could not finish isolated account-variant settings fixture");
    }

    const ServiceType originalService = g_serviceType;
    struct RestoreServiceType { ServiceType value; ~RestoreServiceType() { g_serviceType = value; } } restoreService{originalService};
    g_serviceType = ServiceAmerica;
    CRagConnection* const connection = CRagConnection::instance();
    connection->Disconnect();
    LoopbackPeer peer(*connection);
    LoginModeReplayAccess mode;
    const char user[] = "packet-user";
    const char password[] = "packet-pass";
    Require(mode.SendMsg(CLoginMode::LoginMsg_SetUserId,
            reinterpret_cast<msgparam_t>(const_cast<char*>(user)), 0, 0) != 0,
        "channel-login username message was rejected");
    Require(mode.SendMsg(CLoginMode::LoginMsg_SetPassword,
            reinterpret_cast<msgparam_t>(const_cast<char*>(password)), 0, 0) != 0,
        "channel-login password message was rejected");
    LoginModeReplayAccess::RunPlainAccountLogin(mode);

    const size_t packetSize = channel ? 85u : 84u;
    const u16 opcode = channel ? 0x02b0u : 0x0277u;
    std::vector<u8> expected(packetSize, 0);
    expected[0] = static_cast<u8>(opcode & 0xffu);
    expected[1] = static_cast<u8>(opcode >> 8);
    expected[2] = 0x0e;
    expected[3] = 0x69;
    expected[4] = 0x32;
    expected[5] = 0x01;
    std::memcpy(expected.data() + 6, user, sizeof(user) - 1);
    std::memcpy(expected.data() + 30, password, sizeof(password) - 1);
    expected[54] = 1;
    char expectedIp[16] = "127.0.0.1";
    char hostName[100] = {};
    if (gethostname(hostName, sizeof(hostName)) == 0) {
        hostent* const host = gethostbyname(hostName);
        if (host && host->h_addr_list && host->h_addr_list[0]) {
            const char* const resolvedIp = inet_ntoa(*reinterpret_cast<in_addr*>(host->h_addr_list[0]));
            if (resolvedIp && *resolvedIp) {
                std::strncpy(expectedIp, resolvedIp, sizeof(expectedIp) - 1);
            }
        }
    }
    std::memcpy(expected.data() + 55, expectedIp, std::strlen(expectedIp));
    if (channel) expected[84] = 0;
    const std::vector<u8> captured = peer.ReceiveExact(expected.size());
    Require(captured.size() == packetSize, "configured auxiliary account request had the wrong fixed length");
    const char* const capturedIp = reinterpret_cast<const char*>(captured.data() + 55);
    const size_t ipLength = strnlen(capturedIp, 16);
    Require(ipLength > 0 && ipLength < 16
            && inet_addr(capturedIp) != INADDR_NONE
            && capturedIp == std::string(expectedIp),
        "configured auxiliary account request IP field was not the expected terminated IPv4 address");
    const char expectedMac[] = "111111111111";
    std::memcpy(expected.data() + 71, expectedMac, sizeof(expectedMac));
    Require(captured == expected,
        "configured auxiliary account request differs from independent field-offset fixture");
    Require(g_windowMgr.GetLoginStatus() == "Login: waiting for account server response...",
        "configured auxiliary account login sender did not publish its production status");
    std::cout << "{\"case_id\":\"" << caseId << "\",\"contract_id\":\"PK44.LOGIN-REQUESTS\",\"fixture_id\":\"" << fixtureId
              << "\",\"production_entry\":\"" << senderPath << "\",\"expected_packet_hex\":\"" << Hex(expected)
              << "\",\"captured_packet_hex\":\"" << Hex(captured) << "\",\"frame_length\":" << packetSize
              << ",\"opcode\":\"" << (channel ? "0x02B0" : "0x0277") << "\",\"client_version\":20080910,\"client_type\":1"
              << ",\"ip_field_valid_ipv4\":true,\"mac_field\":\"111111111111\",\"gravity_flag\":" << (channel ? "0" : "null")
              << ",\"synthetic_packet_override\":true,\"live_credentials_not_used\":true,\"deployed_profile_intent_unverified\":true,\"outcome\":\"PASS\"}\n";
}

void Login4AccountRequestReplay()
{
    SetCase("login_account_027c_request", "PK44.LOGIN-REQUESTS", "PK44-027c-current-client-md5-mac-login", "CLoginMode::SendPlainAccountLogin -> SendLogin4 -> CRagConnection::SendPacket");
    const char* oldDataDirValue = std::getenv("OPEN_MIDGARD_DATA_DIR");
    const bool hadDataDir = oldDataDirValue != nullptr;
    const std::string oldDataDir = oldDataDirValue ? oldDataDirValue : "";
    struct RestoreDataDir {
        bool hadValue;
        std::string value;
        ~RestoreDataDir() {
#if RO_PLATFORM_WINDOWS
            _putenv_s("OPEN_MIDGARD_DATA_DIR", hadValue ? value.c_str() : "");
#else
            if (hadValue) setenv("OPEN_MIDGARD_DATA_DIR", value.c_str(), 1);
            else unsetenv("OPEN_MIDGARD_DATA_DIR");
#endif
        }
    } restoreDataDir{hadDataDir, oldDataDir};

    const std::filesystem::path settingsDirectory = std::filesystem::temp_directory_path()
        / (std::string("open-midgard-packet-replay-account-027c-") + std::to_string(GetTickCount()));
    std::filesystem::create_directories(settingsDirectory);
    struct RemoveSettingsDirectory {
        std::filesystem::path path;
        ~RemoveSettingsDirectory() { std::error_code error; std::filesystem::remove_all(path, error); }
    } removeSettingsDirectory{settingsDirectory};
#if RO_PLATFORM_WINDOWS
    Require(_putenv_s("OPEN_MIDGARD_DATA_DIR", settingsDirectory.string().c_str()) == 0,
        "could not point 0x027C replay settings at the isolated temporary directory");
#else
    Require(setenv("OPEN_MIDGARD_DATA_DIR", settingsDirectory.string().c_str(), 1) == 0,
        "could not point 0x027C replay settings at the isolated temporary directory");
#endif
    {
        std::ofstream settings(settingsDirectory / "open-midgard.ini", std::ios::binary | std::ios::trunc);
        Require(settings.is_open(), "could not write isolated 0x027C settings fixture");
        settings << "[Login]\nAccountLoginPacket=login4\nClientDateOverride=20080910\nClientTypeOverride=1\n";
        settings.close();
        Require(settings.good(), "could not finish isolated 0x027C settings fixture");
    }

    const ServiceType originalService = g_serviceType;
    struct RestoreServiceType { ServiceType value; ~RestoreServiceType() { g_serviceType = value; } } restoreService{originalService};
    g_serviceType = ServiceAmerica;
    CRagConnection* const connection = CRagConnection::instance();
    connection->Disconnect();
    LoopbackPeer peer(*connection);
    LoginModeReplayAccess mode;
    const char user[] = "packet-user";
    const char password[] = "packet-pass";
    Require(mode.SendMsg(CLoginMode::LoginMsg_SetUserId,
            reinterpret_cast<msgparam_t>(const_cast<char*>(user)), 0, 0) != 0,
        "0x027C username message was rejected");
    Require(mode.SendMsg(CLoginMode::LoginMsg_SetPassword,
            reinterpret_cast<msgparam_t>(const_cast<char*>(password)), 0, 0) != 0,
        "0x027C password message was rejected");
    LoginModeReplayAccess::RunPlainAccountLogin(mode);

    std::vector<u8> expected(60, 0);
    expected[0] = 0x7c;
    expected[1] = 0x02;
    expected[2] = 0x0e;
    expected[3] = 0x69;
    expected[4] = 0x32;
    expected[5] = 0x01;
    std::memcpy(expected.data() + 6, user, sizeof(user) - 1);
    const u8 expectedDigest[16] = {0xb5,0xd2,0x4b,0x16,0x37,0x27,0x74,0xb2,0xdc,0x52,0x2d,0x70,0x49,0x1e,0xf7,0x8f};
    std::memcpy(expected.data() + 30, expectedDigest, sizeof(expectedDigest));
    expected[46] = 1;
    std::memcpy(expected.data() + 47, "111111111111", 12);
    const std::vector<u8> captured = peer.ReceiveExact(expected.size());
    Require(captured == expected,
        "configured 0x027C request differs from the independent 60-byte field fixture");
    Require(g_windowMgr.GetLoginStatus() == "Login: waiting for account server response...",
        "configured 0x027C login sender did not publish its production status");
    std::cout << "{\"case_id\":\"login_account_027c_request\",\"contract_id\":\"PK44.LOGIN-REQUESTS\",\"fixture_id\":\"PK44-027c-current-client-md5-mac-login\",\"production_entry\":\"CLoginMode::SendPlainAccountLogin -> SendLogin4 -> CRagConnection::SendPacket\",\"expected_packet_hex\":\"" << Hex(expected)
              << "\",\"captured_packet_hex\":\"" << Hex(captured) << "\",\"frame_length\":60,\"opcode\":\"0x027C\",\"client_version\":20080910,\"username\":\"packet-user\",\"password_md5\":\"b5d24b16372774b2dc522d70491ef78f\",\"client_type\":1,\"mac_field\":\"111111111111\",\"zero_initialized_padding\":true,\"synthetic_packet_override\":true,\"deployed_profile_intent_unverified\":true,\"ref_mac_behavior_review_pending\":true,\"outcome\":\"PASS\"}\n";
}

void LoginConnectInfoGateReplay()
{
    SetCase("login_connect_info_gate", "PK44.LOGIN-REQUESTS", "PK44-0200-connect-info-opt-in", "CLoginMode::OnChangeState(ConnectAccount) -> SendConnectInfoChanged -> SendPlainAccountLogin");
    const char* oldDataDirValue = std::getenv("OPEN_MIDGARD_DATA_DIR");
    const bool hadDataDir = oldDataDirValue != nullptr;
    const std::string oldDataDir = oldDataDirValue ? oldDataDirValue : "";
    struct RestoreDataDir {
        bool hadValue;
        std::string value;
        ~RestoreDataDir() {
#if RO_PLATFORM_WINDOWS
            _putenv_s("OPEN_MIDGARD_DATA_DIR", hadValue ? value.c_str() : "");
#else
            if (hadValue) setenv("OPEN_MIDGARD_DATA_DIR", value.c_str(), 1);
            else unsetenv("OPEN_MIDGARD_DATA_DIR");
#endif
        }
    } restoreDataDir{hadDataDir, oldDataDir};

    const std::filesystem::path settingsDirectory = std::filesystem::temp_directory_path()
        / ("open-midgard-packet-replay-" + std::to_string(GetTickCount()));
    std::filesystem::create_directories(settingsDirectory);
    struct RemoveSettingsDirectory {
        std::filesystem::path path;
        ~RemoveSettingsDirectory() { std::error_code error; std::filesystem::remove_all(path, error); }
    } removeSettingsDirectory{settingsDirectory};
#if RO_PLATFORM_WINDOWS
    Require(_putenv_s("OPEN_MIDGARD_DATA_DIR", settingsDirectory.string().c_str()) == 0,
        "could not point replay settings at the isolated temporary directory");
#else
    Require(setenv("OPEN_MIDGARD_DATA_DIR", settingsDirectory.string().c_str(), 1) == 0,
        "could not point replay settings at the isolated temporary directory");
#endif

    const ServiceType originalService = g_serviceType;
    const std::string originalAccountAddress = g_accountAddr;
    const std::string originalAccountPort = g_accountPort;
    const int originalPasswordEncrypt = g_passwordEncrypt;
    struct RestoreLoginGlobals {
        ServiceType service;
        std::string address;
        std::string port;
        int passwordEncrypt;
        ~RestoreLoginGlobals() {
            g_serviceType = service;
            g_accountAddr = address;
            g_accountPort = port;
            g_passwordEncrypt = passwordEncrypt;
        }
    } restoreLoginGlobals{originalService, originalAccountAddress, originalAccountPort, originalPasswordEncrypt};
    g_serviceType = ServiceAmerica;
    g_passwordEncrypt = 0;

    const auto runConnect = [&](bool sendConnectInfo, const char* hashOverride, bool forcePasswordHash, bool useHashSourceExe) {
        if (useHashSourceExe) {
            std::ofstream hashSource(settingsDirectory / "synthetic-client.exe", std::ios::binary | std::ios::trunc);
            Require(hashSource.is_open(), "could not create synthetic executable-hash source");
            hashSource << "abc";
            hashSource.close();
            Require(hashSource.good(), "could not finish synthetic executable-hash source");
        }
        std::ofstream settings(settingsDirectory / "open-midgard.ini", std::ios::binary | std::ios::trunc);
        Require(settings.is_open(), "could not write isolated login-settings fixture");
        settings << "[Login]\n"
                 << "SendConnectInfoChanged=" << (sendConnectInfo ? 1 : 0) << "\n"
                 << "ClientHashOverrideMd5=" << (hashOverride ? hashOverride : "") << "\n"
                 << "ClientHashSourceExe=" << (useHashSourceExe ? "synthetic-client.exe" : "") << "\n"
                 << "AccountLoginPacket=plain\n"
                 << "ForcePasswordHashRequest=" << (forcePasswordHash ? 1 : 0) << "\n"
                 << "ClientDateOverride=20080910\n"
                 << "ClientTypeOverride=1\n";
        settings.close();
        Require(settings.good(), "could not finish isolated login-settings fixture");

        SocketOwner listener(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
        Require(listener.value != INVALID_SOCKET, "mock account listener socket creation failed");
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = 0;
        Require(bind(listener.value, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0,
            "mock account listener bind failed");
        socklen_t addressLength = sizeof(address);
        Require(getsockname(listener.value, reinterpret_cast<sockaddr*>(&address), &addressLength) == 0,
            "mock account listener port query failed");
        Require(listen(listener.value, 1) == 0, "mock account listener setup failed");
        g_accountAddr = "127.0.0.1";
        g_accountPort = std::to_string(ntohs(address.sin_port));

        CRagConnection* const connection = CRagConnection::instance();
        connection->Disconnect();
        LoginModeReplayAccess mode;
        const char user[] = "packet-user";
        const char password[] = "packet-pass";
        Require(mode.SendMsg(CLoginMode::LoginMsg_SetUserId,
                reinterpret_cast<msgparam_t>(const_cast<char*>(user)), 0, 0) != 0,
            "connect-account username message was rejected");
        Require(mode.SendMsg(CLoginMode::LoginMsg_SetPassword,
                reinterpret_cast<msgparam_t>(const_cast<char*>(password)), 0, 0) != 0,
            "connect-account password message was rejected");
        mode.OnChangeState(CLoginMode::LoginSubMode_ConnectAccount);
        Require(connection->IsOpen() && LoginModeReplayAccess::IsConnected(mode),
            "ConnectAccount production state did not connect to the local mock listener");
        Require(SelectSocket(listener.value, true, false,
                std::chrono::duration_cast<std::chrono::milliseconds>(kTimeout)) > 0,
            "mock account listener timed out waiting for the production ConnectAccount path");
        SocketOwner accepted(accept(listener.value, nullptr, nullptr));
        Require(accepted.value != INVALID_SOCKET, "mock account listener accept failed");

        std::vector<u8> expectedLogin(55, 0);
        expectedLogin[0] = 0x64;
        expectedLogin[2] = 0x0e;
        expectedLogin[3] = 0x69;
        expectedLogin[4] = 0x32;
        expectedLogin[5] = 0x01;
        std::memcpy(expectedLogin.data() + 6, user, sizeof(user) - 1);
        std::memcpy(expectedLogin.data() + 30, password, sizeof(password) - 1);
        expectedLogin[54] = 1;

        std::vector<u8> expectedStream;
        if (sendConnectInfo) {
            std::vector<u8> connectInfo(26, 0);
            connectInfo[0] = 0x00;
            connectInfo[1] = 0x02;
            std::memcpy(connectInfo.data() + 2, user, sizeof(user) - 1);
            expectedStream.insert(expectedStream.end(), connectInfo.begin(), connectInfo.end());
        }
        if (hashOverride) {
            const std::vector<u8> exeHash{
                0x04, 0x02, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
                0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
            expectedStream.insert(expectedStream.end(), exeHash.begin(), exeHash.end());
        } else if (useHashSourceExe) {
            const std::vector<u8> exeHash{
                0x04, 0x02, 0x90, 0x01, 0x50, 0x98, 0x3c, 0xd2, 0x4f,
                0xb0, 0xd6, 0x96, 0x3f, 0x7d, 0x28, 0xe1, 0x7f, 0x72};
            expectedStream.insert(expectedStream.end(), exeHash.begin(), exeHash.end());
        }
        if (forcePasswordHash) {
            expectedStream.push_back(0xdb);
            expectedStream.push_back(0x01);
        } else {
            expectedStream.insert(expectedStream.end(), expectedLogin.begin(), expectedLogin.end());
        }
        const std::vector<u8> captured = ReceiveExactSocket(accepted.value, expectedStream.size());
        Require(captured == expectedStream,
            "ConnectAccount gate emitted a different 0x0200/0x0064 packet sequence than expected");
        const char* const expectedStatus = forcePasswordHash
            ? "Login: waiting for account hash challenge..."
            : "Login: waiting for account server response...";
        Require(g_windowMgr.GetLoginStatus() == expectedStatus,
            "ConnectAccount production flow did not enter the expected account-response wait state");
        Require(SelectSocket(accepted.value, true, false, std::chrono::milliseconds(20)) == 0,
            "ConnectAccount emitted an unexpected extra packet after its expected request sequence");
        connection->Disconnect();
        g_windowMgr.RemoveAllWindows();
        return Hex(captured);
    };

    const std::string disabledStream = runConnect(false, nullptr, false, false);
    const std::string enabledStream = runConnect(true, nullptr, false, false);
    const std::string hashOverrideStream = runConnect(false, "00112233445566778899aabbccddeeff", false, false);
    const std::string passwordHashRequestStream = runConnect(false, nullptr, true, false);
    const std::string hashSourceExeStream = runConnect(false, nullptr, false, true);
    std::cout << "{\"case_id\":\"login_connect_info_gate\",\"contract_id\":\"PK44.LOGIN-REQUESTS\",\"fixture_id\":\"PK44-0200-connect-info-0204-hash-source-and-01db-password-hash-gates\",\"production_entry\":\"CLoginMode::OnChangeState(ConnectAccount) -> SendConnectInfoChanged -> SendExeHashCheck -> SendPasswordHashRequest/SendPlainAccountLogin\",\"disabled_stream_hex\":\"" << disabledStream << "\",\"enabled_stream_hex\":\"" << enabledStream << "\",\"synthetic_hash_override_stream_hex\":\"" << hashOverrideStream << "\",\"password_hash_request_stream_hex\":\"" << passwordHashRequestStream << "\",\"synthetic_hash_source_exe_stream_hex\":\"" << hashSourceExeStream << "\",\"disabled_gate_sends_only_0064\":true,\"enabled_gate_sends_0200_then_0064\":true,\"default_america_without_hash_source_omits_0204\":true,\"synthetic_hash_override_sends_0204_then_0064\":true,\"relative_synthetic_source_exe_sends_independent_abc_md5\":true,\"default_no_hash_request_sends_0064\":true,\"forced_hash_request_sends_01db_and_waits_for_challenge\":true,\"connect_account_path_uses_local_mock\":true,\"isolated_settings_fixture_removed\":true,\"no_live_credentials_or_server\":true,\"outcome\":\"PASS\"}\n";
}

void LoginCharacterServerEntryRequestReplay()
{
    SetCase("login_character_server_entry_request", "PK44.CHAR-RAW-AID-HANDSHAKE", "PK44-0065-connect-char-entry-request", "CLoginMode::OnChangeState(ConnectChar) -> CRagConnection::SendPacket");
    SocketOwner listener(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
    Require(listener.value != INVALID_SOCKET, "mock char-server listener socket creation failed");
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    Require(bind(listener.value, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0,
        "mock char-server listener bind failed");
    socklen_t addressLength = sizeof(address);
    Require(getsockname(listener.value, reinterpret_cast<sockaddr*>(&address), &addressLength) == 0,
        "mock char-server listener port query failed");
    Require(listen(listener.value, 1) == 0, "mock char-server listener setup failed");

    struct RestoreSessionEntryFields {
        u32 aid = g_session.m_aid;
        u32 authCode = g_session.m_authCode;
        u32 userLevel = g_session.m_userLevel;
        u8 sex = g_session.m_sex;
        char charServerAddr[sizeof(g_session.m_charServerAddr)]{};
        int charServerPort = g_session.m_charServerPort;
        RestoreSessionEntryFields() { std::memcpy(charServerAddr, g_session.m_charServerAddr, sizeof(charServerAddr)); }
        ~RestoreSessionEntryFields() {
            g_session.m_aid = aid;
            g_session.m_authCode = authCode;
            g_session.m_userLevel = userLevel;
            g_session.m_sex = sex;
            std::memcpy(g_session.m_charServerAddr, charServerAddr, sizeof(charServerAddr));
            g_session.m_charServerPort = charServerPort;
        }
    } restoreSession;
    g_session.m_aid = 0x11223344u;
    g_session.m_authCode = 0x55667788u;
    g_session.m_userLevel = 0x99aabbccu;
    g_session.m_sex = 1;

    const u16 charPort = ntohs(address.sin_port);
    LoginModeReplayAccess mode;
    LoginModeReplayAccess::SeedCharServerForConnect(
        mode, inet_addr("127.0.0.1"), static_cast<s16>(charPort));
    CRagConnection* const connection = CRagConnection::instance();
    connection->Disconnect();
    mode.OnChangeState(CLoginMode::LoginSubMode_ConnectChar);
    Require(connection->IsOpen() && LoginModeReplayAccess::IsConnected(mode),
        "ConnectChar production state did not connect to the local mock listener");
    Require(SelectSocket(listener.value, true, false,
            std::chrono::duration_cast<std::chrono::milliseconds>(kTimeout)) > 0,
        "mock char-server listener timed out waiting for ConnectChar");
    SocketOwner accepted(accept(listener.value, nullptr, nullptr));
    Require(accepted.value != INVALID_SOCKET, "mock char-server listener accept failed");

    const std::vector<u8> expected{
        0x65, 0x00, 0x44, 0x33, 0x22, 0x11, 0x88, 0x77, 0x66,
        0x55, 0xcc, 0xbb, 0xaa, 0x99, 0x00, 0x00, 0x01};
    const std::vector<u8> captured = ReceiveExactSocket(accepted.value, expected.size());
    Require(captured == expected,
        "ConnectChar production state emitted a different independent 0x0065 field fixture");
    Require(g_windowMgr.GetLoginStatus() == "Login: waiting for character list...",
        "ConnectChar did not enter its production character-list wait status");
    const std::vector<u8> charServerReply{
        0x44, 0x33, 0x22, 0x11,
        0x6b, 0x00, 0x18, 0x00,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    Require(send(accepted.value, reinterpret_cast<const char*>(charServerReply.data()),
                 static_cast<int>(charServerReply.size()), 0) == static_cast<int>(charServerReply.size()),
        "mock char server did not send raw AID echo plus selected-source empty character list");
    Require(SelectSocket(PacketStreamReplayAccess::Native(*connection), true, false,
            std::chrono::duration_cast<std::chrono::milliseconds>(kTimeout)) > 0,
        "mock char-server reply did not become readable");
    LoginModeReplayAccess::RunPollNetwork(mode);
    Require(g_windowMgr.GetLoginStatus() == "Login: no characters on account."
            && mode.NextSubMode() == CLoginMode::LoginSubMode_CharSelect,
        "ConnectChar did not consume the raw AID and dispatch the following framed character list");
    connection->Disconnect();
    g_windowMgr.RemoveAllWindows();
    std::cout << "{\"case_id\":\"login_character_server_entry_request\",\"contract_id\":\"PK44.CHAR-RAW-AID-HANDSHAKE\",\"fixture_id\":\"PK44-0065-connect-char-entry-request-and-raw-aid\",\"production_entry\":\"CLoginMode::OnChangeState(ConnectChar) -> CRagConnection::Connect/ExpectRawAccountIdEcho/SendPacket -> RecvPacket -> PollNetwork\",\"expected_packet_hex\":\"" << Hex(expected) << "\",\"captured_packet_hex\":\"" << Hex(captured) << "\",\"char_server_reply_hex\":\"" << Hex(charServerReply) << "\",\"frame_length\":17,\"opcode\":\"0x0065\",\"account_id\":287454020,\"raw_echo_hex\":\"44332211\",\"following_frame_hex\":\"6b0018000000000000000000000000000000000000000000\",\"empty_character_list_dispatched\":true,\"next_submode_char_select\":true,\"network_scope\":\"loopback mock char server only\",\"outcome\":\"PASS\"}\n";
}

void RawAccountIdPrefixBoundaryReplay()
{
    SetCase("raw_account_id_prefix_boundaries", "PK04.RAW-AID", "PK04-raw-aid-literal-prefixes", "CRagConnection::ExpectRawAccountIdEcho -> RecvPacket -> opcode-framed suffix");
    struct PrefixCase {
        u32 accountId;
        std::array<u8, 4> bytes;
        const char* label;
    };
    const std::array<PrefixCase, 3> cases{{
        {0x12345678u, {0x78, 0x56, 0x34, 0x12}, "ordinary"},
        {0xA1B2006Cu, {0x6c, 0x00, 0xb2, 0xa1}, "fixed_006c_collision"},
        {0xA1B20069u, {0x69, 0x00, 0xb2, 0xa1}, "variable_0069_collision"},
    }};
    const std::vector<u8> expectedFrame{
        0x6b, 0x00, 0x18, 0x00,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    const std::vector<u8> nextFramedPacket{0x6c, 0x00, 0x00};
    for (const PrefixCase& fixture : cases) {
        for (size_t split = 1; split <= fixture.bytes.size(); ++split) {
            const std::string caseId = std::string("raw_aid_") + fixture.label + "_split_" + std::to_string(split);
            SetCase(caseId, "PK04.RAW-AID", "PK04-raw-aid-literal-prefixes", "CRagConnection::ExpectRawAccountIdEcho -> RecvPacket -> opcode-framed suffix");
            TestConnection connection;
            LoopbackPeer peer(connection);
            Require(connection.ExpectRawAccountIdEcho(fixture.accountId),
                "production connection refused to arm the raw account-ID prefix");
            const std::vector<u8> prefix(fixture.bytes.begin(), fixture.bytes.begin() + split);
            peer.Send(prefix);
            std::vector<u8> received;
            if (split < fixture.bytes.size()) {
                PumpUntilBuffered(connection, static_cast<int>(split));
                Require(!Receive(connection, received) && connection.BufferedBytes() == static_cast<int>(split),
                    "partial raw account-ID prefix was delivered or consumed prematurely");
            } else {
                WaitReadable(connection, Clock::now() + kTimeout);
                Require(!Receive(connection, received) && connection.BufferedBytes() == 0,
                    "complete raw echo without a framed suffix produced a packet");
            }
            std::vector<u8> suffix(fixture.bytes.begin() + split, fixture.bytes.end());
            suffix.insert(suffix.end(), expectedFrame.begin(), expectedFrame.end());
            suffix.insert(suffix.end(), nextFramedPacket.begin(), nextFramedPacket.end());
            peer.Send(suffix);
            const std::vector<u8> captured = PumpUntilPacket(connection);
            Require(captured == expectedFrame,
                "validated raw AID prefix changed or consumed the adjacent framed response");
            const std::vector<u8> capturedNext = PumpUntilPacket(connection);
            Require(capturedNext == nextFramedPacket,
                "raw-prefix expectation was not cleared exactly once after matching the AID");
            Require(connection.IsOpen(), "valid raw AID prefix unexpectedly closed the connection");
            std::cout << "{\"case_id\":\"" << caseId << "\",\"contract_id\":\"PK04.RAW-AID\",\"fixture_id\":\"PK04-raw-aid-literal-prefixes\",\"production_entry\":\"ExpectRawAccountIdEcho -> RecvPacket\",\"account_id_hex\":\"" << Hex(std::vector<u8>(fixture.bytes.begin(), fixture.bytes.end())) << "\",\"account_id_literal\":\"" << fixture.label << "\",\"split_after_bytes\":" << split << ",\"incomplete_prefix_delivery\":false,\"following_frame_expected_hex\":\"" << Hex(expectedFrame) << "\",\"following_frame_captured_hex\":\"" << Hex(captured) << "\",\"next_frame_expected_hex\":\"" << Hex(nextFramedPacket) << "\",\"next_frame_captured_hex\":\"" << Hex(capturedNext) << "\",\"framed_suffix_preserved\":true,\"expectation_cleared_once\":true,\"connection_open\":true,\"outcome\":\"PASS\"}\n";
        }
    }

    SetCase("raw_account_id_mismatch_rejected", "PK04.RAW-AID", "PK04-raw-aid-mismatch-and-eof", "CRagConnection::ExpectRawAccountIdEcho -> RecvPacket mismatch close");
    {
        TestConnection connection;
        LoopbackPeer peer(connection);
        Require(connection.ExpectRawAccountIdEcho(0x12345678u), "mismatch fixture could not arm expected AID");
        const std::vector<u8> mismatchAndFrame{0x79,0x56,0x34,0x12,0x6b,0x00,0x18,0x00,
            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
        peer.Send(mismatchAndFrame);
        std::vector<u8> captured;
        const auto deadline = Clock::now() + kTimeout;
        while (connection.IsOpen() && Clock::now() < deadline) {
            WaitReadable(connection, deadline);
            if (Receive(connection, captured)) break;
        }
        Require(!connection.IsOpen() && connection.BufferedBytes() == 0 && captured.empty(),
            "mismatched four-byte account echo was not rejected before framed bytes");
        std::cout << "{\"case_id\":\"raw_account_id_mismatch_rejected\",\"contract_id\":\"PK04.RAW-AID\",\"fixture_id\":\"PK04-raw-aid-mismatch-and-eof\",\"expected_aid_hex\":\"78563412\",\"received_aid_hex\":\"79563412\",\"following_frame_dispatched\":false,\"connection_open\":false,\"buffered_bytes\":0,\"outcome\":\"PASS\"}\n";
    }

    for (size_t partial = 0; partial < 4; ++partial) {
        SetCase("raw_account_id_eof_" + std::to_string(partial), "PK04.RAW-AID", "PK04-raw-aid-mismatch-and-eof", "CRagConnection::ExpectRawAccountIdEcho -> RecvPacket EOF");
        TestConnection connection;
        LoopbackPeer peer(connection);
        Require(connection.ExpectRawAccountIdEcho(0x12345678u), "EOF fixture could not arm expected AID");
        if (partial > 0) {
            const std::vector<u8> prefix{0x78,0x56,0x34};
            peer.Send(std::vector<u8>(prefix.begin(), prefix.begin() + partial));
            PumpUntilBuffered(connection, static_cast<int>(partial));
        }
        CloseSocket(peer.peer.value);
        peer.peer.value = INVALID_SOCKET;
        WaitReadable(connection, Clock::now() + kTimeout);
        std::vector<u8> captured;
        Require(!Receive(connection, captured) && !connection.IsOpen() && connection.BufferedBytes() == 0,
            "EOF during raw AID prefix did not close and clear the receive state");
        std::cout << "{\"case_id\":\"raw_account_id_eof_" << partial << "\",\"contract_id\":\"PK04.RAW-AID\",\"fixture_id\":\"PK04-raw-aid-mismatch-and-eof\",\"partial_prefix_bytes\":" << partial << ",\"packet_delivered\":false,\"connection_open\":false,\"buffered_bytes\":0,\"outcome\":\"PASS\"}\n";
    }

    SetCase("raw_account_id_reconnect_clears_policy", "PK04.RAW-AID", "PK04-raw-aid-mismatch-and-eof", "CRagConnection::Connect/Disconnect lifecycle -> ordinary framed receive");
    {
        TestConnection connection;
        {
            LoopbackPeer peer(connection);
            Require(connection.ExpectRawAccountIdEcho(0x12345678u), "reconnect fixture could not arm expected AID");
            peer.Send({0x78,0x56});
            PumpUntilBuffered(connection, 2);
        }
        const std::vector<u8> expectedOrdinary{0x6c,0x00,0x00};
        LoopbackPeer reconnected(connection);
        reconnected.Send(expectedOrdinary);
        const std::vector<u8> captured = PumpUntilPacket(connection);
        Require(captured == expectedOrdinary && connection.IsOpen(),
            "new ordinary connection inherited the previous raw account-ID expectation");
        std::cout << "{\"case_id\":\"raw_account_id_reconnect_clears_policy\",\"contract_id\":\"PK04.RAW-AID\",\"fixture_id\":\"PK04-raw-aid-mismatch-and-eof\",\"reconnected_frame_expected_hex\":\"6c0000\",\"reconnected_frame_captured_hex\":\"" << Hex(captured) << "\",\"stale_prefix_policy_cleared\":true,\"outcome\":\"PASS\"}\n";
    }
}

void LoginCharacterRefusalReplay()
{
    SetCase("login_character_refusal", "PK44.CHAR-LIST-ENTRY", "PK44-006c-refuse-enter-selected-source", "CLoginMode::OnRefuseChar");
    CRagConnection* const connection = CRagConnection::instance();
    connection->Disconnect();
    LoopbackPeer peer(*connection);
    LoginModeReplayAccess mode;
    LoginModeReplayAccess::SetConnected(mode, true);
    const std::vector<u8> packet{0x6c, 0x00, 0x00};
    peer.Send(packet);
    const auto receiveDeadline = Clock::now() + kTimeout;
    while (g_windowMgr.GetLoginStatus() != "Char server refused entry (code 0)."
           && Clock::now() < receiveDeadline) {
        LoginModeReplayAccess::RunPollNetwork(mode);
        if (g_windowMgr.GetLoginStatus() != "Char server refused entry (code 0).") {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    Require(g_windowMgr.GetLoginStatus() == "Char server refused entry (code 0).",
        "production PollNetwork did not frame and dispatch selected-source 0x006C");
    Require(mode.NextSubMode() == CLoginMode::LoginSubMode_Login,
        "0x006C refusal did not schedule return to Login");
    Require(!LoginModeReplayAccess::IsConnected(mode), "0x006C refusal did not clear char-server connected state");
    Require(!connection->IsOpen(), "0x006C refusal did not close the active char-server connection");
    g_windowMgr.RemoveAllWindows();
    std::cout << "{\"case_id\":\"login_character_refusal\",\"contract_id\":\"PK44.CHAR-LIST-ENTRY\",\"fixture_id\":\"PK44-006c-refuse-enter-selected-source\",\"production_entry\":\"CRagConnection::RecvPacket -> CLoginMode::PollNetwork -> OnRefuseChar -> ShowLoginErrorDialog -> Disconnect -> Login\",\"input_frame_hex\":\"" << Hex(packet) << "\",\"frame_length\":3,\"result\":0,\"status\":\"Char server refused entry (code 0).\",\"connected_state_cleared\":true,\"returns_to_login\":true,\"connection_closed\":true,\"visible_modal_not_verified\":true,\"outcome\":\"PASS\"}\n";
}

void LoginZoneServerNotificationReplay()
{
    SetCase("login_zone_server_notification", "PK44.CHAR-LIST-ENTRY", "PK44-0071-zonesvr-selected-source", "CRagConnection::RecvPacket -> CLoginMode::PollNetwork -> OnNotifyZonesvr");
    CRagConnection* const connection = CRagConnection::instance();
    connection->Disconnect();
    LoopbackPeer peer(*connection);
    LoginModeReplayAccess mode;
    LoginModeReplayAccess::SetConnected(mode, true);
    std::vector<u8> packet(28, 0);
    packet[0] = 0x71;
    packet[1] = 0x00;
    packet[2] = 0x40;
    packet[3] = 0x30;
    packet[4] = 0x20;
    packet[5] = 0x10;
    std::memcpy(packet.data() + 6, "prontera.gat", 12);
    packet[22] = 10;
    packet[23] = 0;
    packet[24] = 0;
    packet[25] = 1;
    packet[26] = 0xe9;
    packet[27] = 0x17;

    const u32 oldGid = g_session.m_gid;
    char oldMap[sizeof(g_session.m_curMap)]{};
    std::memcpy(oldMap, g_session.m_curMap, sizeof(oldMap));
    const LocalGuildInfo oldLocalGuild = g_session.m_localGuild;
    const ro::guild::WindowState oldGuildWindow = g_session.m_guildWindow;
    g_session.m_localGuild.guildId = 99;
    g_session.m_localGuild.name = "stale-guild";
    g_session.m_guildWindow.id = 99;
    g_session.m_guildWindow.loaded = true;
    g_session.m_guildWindow.name = "stale-window";

    peer.Send(packet);
    const auto receiveDeadline = Clock::now() + kTimeout;
    while (connection->IsOpen() && Clock::now() < receiveDeadline) {
        LoginModeReplayAccess::RunPollNetwork(mode);
        if (connection->IsOpen()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    Require(g_session.m_gid == 0x10203040u && std::string(g_session.m_curMap) == "prontera",
        "0x0071 character ID or map extension parsing differs from the selected-source fixture");
    Require(LoginModeReplayAccess::ZoneEndpointMatches(mode),
        "0x0071 zone IP or port conversion differs from selected-source wire bytes");
    Require(g_session.m_localGuild.guildId == 0 && g_session.m_localGuild.name.empty()
            && g_session.m_guildWindow.id == 0 && !g_session.m_guildWindow.loaded
            && g_session.m_guildWindow.name.empty(),
        "0x0071 map-session transition retained stale local guild state");
    Require(mode.NextSubMode() == CLoginMode::LoginSubMode_ZoneConnect,
        "0x0071 did not transition to ZoneConnect");
    Require(!LoginModeReplayAccess::IsConnected(mode) && !connection->IsOpen(),
        "0x0071 did not clear char-server connection state");

    g_session.m_gid = oldGid;
    std::memcpy(g_session.m_curMap, oldMap, sizeof(oldMap));
    g_session.m_localGuild = oldLocalGuild;
    g_session.m_guildWindow = oldGuildWindow;
    g_windowMgr.RemoveAllWindows();
    std::cout << "{\"case_id\":\"login_zone_server_notification\",\"contract_id\":\"PK44.CHAR-LIST-ENTRY\",\"fixture_id\":\"PK44-0071-zonesvr-selected-source\",\"production_entry\":\"CRagConnection::RecvPacket -> CLoginMode::PollNetwork -> OnNotifyZonesvr\",\"input_frame_hex\":\"" << Hex(packet) << "\",\"frame_length\":28,\"character_id\":270544960,\"map_wire_name\":\"prontera.gat\",\"map_result\":\"prontera\",\"ip_wire_bytes_hex\":\"0a000001\",\"zone_ip\":\"10.0.0.1\",\"port_wire_bytes_hex\":\"e917\",\"zone_port\":6121,\"stale_guild_state_cleared\":true,\"zone_connect_transition\":true,\"char_connection_closed\":true,\"production_framing_dispatch\":true,\"visible_zone_ui_not_exercised\":true,\"outcome\":\"PASS\"}\n";
}

void LoginCharacterCreationRefusalReplay()
{
    SetCase("login_character_creation_refusal", "PK44.CHAR-CREATE", "PK44-006e-create-refusal-results-selected-source", "CRagConnection::RecvPacket -> CLoginMode::PollNetwork -> OnRefuseMakeChar");
    struct Sample { u8 code; const char* expected; };
    const Sample samples[] = {
        {0, "Login: character creation failed (character name already exists, code 0)."},
        {1, "Login: character creation failed (age restriction, code 1)."},
        {3, "Login: character creation failed (invalid character slot, code 3)."},
        {255, "Login: character creation failed (character creation denied, code 255)."},
        {2, "Login: character creation failed (unrecognized result, code 2)."},
        {11, "Login: character creation failed (unrecognized result, code 11)."},
    };
    std::vector<std::string> frameHex;
    std::vector<std::string> messages;
    CRagConnection* const connection = CRagConnection::instance();
    for (const Sample& sample : samples) {
        connection->Disconnect();
        LoopbackPeer peer(*connection);
        const std::vector<u8> packet{0x6e, 0x00, sample.code};
        LoginModeReplayAccess mode;
        LoginModeReplayAccess::SetStatus(mode, "Login: awaiting replay packet.");
        peer.Send(packet);
        const auto receiveDeadline = Clock::now() + kTimeout;
        while (g_windowMgr.GetLoginStatus() != sample.expected && Clock::now() < receiveDeadline) {
            LoginModeReplayAccess::RunPollNetwork(mode);
            if (g_windowMgr.GetLoginStatus() != sample.expected) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
        const std::string message = g_windowMgr.GetLoginStatus();
        Require(message == sample.expected, "0x006E result message differs from selected source meanings or unknown fallback");
        Require(mode.NextSubMode() == CLoginMode::LoginSubMode_CharSelect,
            "0x006E refusal did not return to character selection");
        frameHex.push_back(Hex(packet));
        messages.push_back(message);
    }
    g_windowMgr.RemoveAllWindows();
    std::cout << "{\"case_id\":\"login_character_creation_refusal\",\"contract_id\":\"PK44.CHAR-CREATE\",\"fixture_id\":\"PK44-006e-create-refusal-results-selected-source\",\"production_entry\":\"CRagConnection::RecvPacket -> CLoginMode::PollNetwork -> OnRefuseMakeChar\",\"input_frames_hex\":[";
    for (size_t index = 0; index < frameHex.size(); ++index) {
        if (index != 0) std::cout << ",";
        std::cout << "\"" << frameHex[index] << "\"";
    }
    std::cout << "],\"frame_length\":3,\"opcode\":\"0x006E\",\"result_codes\":[0,1,3,255,2,11],\"actual_messages\":[";
    for (size_t index = 0; index < messages.size(); ++index) {
        if (index != 0) std::cout << ",";
        std::cout << "\"" << JsonEscape(messages[index]) << "\"";
    }
    std::cout << "],\"production_framing_dispatch\":true,\"all_results_return_to_char_select\":true,\"visible_modal_not_verified\":true,\"outcome\":\"PASS\"}\n";
}

void LoginPasswordHashChallengeReplay()
{
    SetCase("login_password_hash_challenge", "PK44.LOGIN-REPLIES", "PK44-01dc-md5-challenge-selected-source", "CRagConnection::RecvPacket -> CLoginMode::PollNetwork -> OnAckHash -> active 0x01DD builder");
    const std::vector<u8> challenge = {
        0xdc, 0x01, 0x18, 0x00,
        0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29,
        0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33,
    };
    Require(sizeof(PACKET_CA_LOGIN_HASH) == 47, "selected client 0x01DD request size differs from source contract");
    Require(challenge.size() == 24 && challenge[2] == 0x18 && challenge[3] == 0,
        "synthetic 0x01DC fixture does not contain its four-byte header and 20-byte salt");

    struct ModeSample { int passwordFirst; const char* expectedDigest; };
    const ModeSample samples[] = {
        {0, "e30bf739e82f48e80e85d68a47981cad"},
        {1, "54ad6400ff9529c355b65b9afb05f42d"},
    };
    const int originalPasswordEncrypt2 = g_passwordEncrypt2;
    struct RestorePasswordEncrypt2 { int value; ~RestorePasswordEncrypt2() { g_passwordEncrypt2 = value; } } restore{originalPasswordEncrypt2};
    CRagConnection* connection = CRagConnection::instance();
    LoginModeReplayAccess mode;
    std::strncpy(mode.m_userId, "packet-test", sizeof(mode.m_userId) - 1);
    std::strncpy(mode.m_userPassword, "password", sizeof(mode.m_userPassword) - 1);
    std::vector<std::string> actualFrames;
    for (const ModeSample& sample : samples) {
        connection->Disconnect();
        g_passwordEncrypt2 = sample.passwordFirst;
        LoopbackPeer peer(*connection);
        LoginModeReplayAccess::SendHashRequest(mode);
        const std::vector<u8> hashRequest = peer.ReceiveExact(sizeof(PACKET_CA_REQ_HASH));
        Require(hashRequest == std::vector<u8>({0xdb, 0x01}), "production password-hash request bytes differ from 0x01DB");
        peer.Send(challenge);
        LoginModeReplayAccess::RunPollNetwork(mode);
        const std::vector<u8> request = peer.ReceiveExact(sizeof(PACKET_CA_LOGIN_HASH));
        Require(request.size() == 47 && request[0] == 0xdd && request[1] == 0x01,
            "0x01DC production handler did not send a 47-byte 0x01DD login request");
        const u32 clientVersion = static_cast<u32>(request[2])
            | (static_cast<u32>(request[3]) << 8)
            | (static_cast<u32>(request[4]) << 16)
            | (static_cast<u32>(request[5]) << 24);
        Require(clientVersion == 20080910u, "0x01DD request did not preserve the selected packetver-23 login date");
        Require(std::string(reinterpret_cast<const char*>(request.data() + 6)) == "packet-test",
            "0x01DD request did not preserve the synthetic account ID");
        Require(Hex(std::vector<u8>(request.begin() + 30, request.begin() + 46)) == sample.expectedDigest,
            "0x01DD digest does not match the selected password/challenge concatenation order");
        actualFrames.push_back(Hex(request));
        connection->Disconnect();
    }
    g_passwordEncrypt2 = originalPasswordEncrypt2;

    std::cout << "{\"case_id\":\"login_password_hash_challenge\",\"contract_id\":\"PK44.LOGIN-REPLIES\",\"fixture_id\":\"PK44-01dc-md5-challenge-selected-source\",\"production_entry\":\"CLoginMode::SendPasswordHashRequest -> 0x01DB; OnAckHash -> active 0x01DD builder -> CRagConnection::SendPacket\",\"request_01db_hex\":\"db01\",\"input_challenge_hex\":\"" << Hex(challenge)
        << "\",\"captured_01dd_frames_hex\":[\"" << actualFrames[0] << "\",\"" << actualFrames[1]
        << "\"],\"passwordencrypt2_modes\":[0,1],\"expected_digest_order_0\":\"salt+password\",\"expected_digest_order_1\":\"password+salt\",\"outcome\":\"PASS\"}\n";
}

void Login3ClientInfoHashRequestReplay()
{
    SetCase("login3_clientinfo_hash_request", "PK44.LOGIN-REQUESTS", "PK44-01fa-md5-clientinfo-selected-source", "ConnectAccount -> 0x01DB -> 0x01DC -> OnAckHash -> 0x01FA");
    const std::filesystem::path settingsDirectory = std::filesystem::temp_directory_path()
        / ("open-midgard-login3-replay-" + std::to_string(GetTickCount()));
    std::filesystem::create_directories(settingsDirectory);
    struct RemoveSettingsDirectory {
        std::filesystem::path path;
        ~RemoveSettingsDirectory() { std::error_code error; std::filesystem::remove_all(path, error); }
    } removeSettingsDirectory{settingsDirectory};
    const char* const originalDataDirValue = std::getenv("OPEN_MIDGARD_DATA_DIR");
    const bool hadOriginalDataDir = originalDataDirValue != nullptr;
    const std::string originalDataDir = originalDataDirValue ? originalDataDirValue : "";
    struct RestoreDataDir {
        bool hadValue;
        std::string value;
        ~RestoreDataDir() {
#if RO_PLATFORM_WINDOWS
            _putenv_s("OPEN_MIDGARD_DATA_DIR", hadValue ? value.c_str() : "");
#else
            if (hadValue) setenv("OPEN_MIDGARD_DATA_DIR", value.c_str(), 1);
            else unsetenv("OPEN_MIDGARD_DATA_DIR");
#endif
        }
    } restoreDataDir{hadOriginalDataDir, originalDataDir};
#if RO_PLATFORM_WINDOWS
    Require(_putenv_s("OPEN_MIDGARD_DATA_DIR", settingsDirectory.string().c_str()) == 0,
        "could not point login3 replay at its isolated settings directory");
#else
    Require(setenv("OPEN_MIDGARD_DATA_DIR", settingsDirectory.string().c_str(), 1) == 0,
        "could not point login3 replay at its isolated settings directory");
#endif
    std::ofstream settings(settingsDirectory / "open-midgard.ini", std::ios::binary | std::ios::trunc);
    Require(settings.is_open(), "could not write isolated login3 settings");
    settings << "[Login]\nAccountLoginPacket=login3\nClientDateOverride=20080910\n";
    settings.close();
    Require(settings.good(), "could not finish isolated login3 settings");
    const std::filesystem::path clientInfoPath = settingsDirectory / "clientinfo.xml";
    std::ofstream clientInfo(clientInfoPath, std::ios::binary | std::ios::trunc);
    Require(clientInfo.is_open(), "could not write isolated login3 client-info fixture");
    clientInfo << "<clientinfo><servicetype>america</servicetype><servertype>primary</servertype>"
                  "<connection><address>127.0.0.1</address><port>6900</port><version>18</version><langtype>1</langtype></connection>"
                  "<connection><address>127.0.0.1</address><port>6901</port><version>18</version><langtype>1</langtype></connection>"
                  "</clientinfo>";
    clientInfo.close();
    Require(clientInfo.good(), "could not finish isolated login3 client-info fixture");
    Require(InitClientInfo(clientInfoPath.string().c_str()), "could not parse isolated login3 client-info fixture");
    SelectClientInfo(1);
    Require(GetSelectedClientInfoIndex() == 1, "login3 client-info fixture did not select profile index one");

    SocketOwner listener(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
    Require(listener.value != INVALID_SOCKET, "login3 mock account listener socket creation failed");
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    Require(bind(listener.value, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0,
        "login3 mock account listener bind failed");
    socklen_t addressLength = sizeof(address);
    Require(getsockname(listener.value, reinterpret_cast<sockaddr*>(&address), &addressLength) == 0,
        "login3 mock account listener port query failed");
    Require(listen(listener.value, 1) == 0, "login3 mock account listener listen failed");

    const ServiceType originalService = g_serviceType;
    const std::string originalAccountAddress = g_accountAddr;
    const std::string originalAccountPort = g_accountPort;
    struct RestoreLoginGlobals {
        ServiceType service;
        std::string address;
        std::string port;
        ~RestoreLoginGlobals() {
            g_serviceType = service;
            g_accountAddr = address;
            g_accountPort = port;
        }
    } restoreLoginGlobals{originalService, originalAccountAddress, originalAccountPort};
    g_serviceType = ServiceAmerica;
    g_accountAddr = "127.0.0.1";
    g_accountPort = std::to_string(ntohs(address.sin_port));

    CRagConnection* const connection = CRagConnection::instance();
    connection->Disconnect();
    LoginModeReplayAccess mode;
    const char username[] = "packet-test";
    const char password[] = "password";
    Require(mode.SendMsg(CLoginMode::LoginMsg_SetUserId,
            reinterpret_cast<msgparam_t>(const_cast<char*>(username)), 0, 0) != 0,
        "login3 username fixture was rejected");
    Require(mode.SendMsg(CLoginMode::LoginMsg_SetPassword,
            reinterpret_cast<msgparam_t>(const_cast<char*>(password)), 0, 0) != 0,
        "login3 password fixture was rejected");
    mode.OnChangeState(CLoginMode::LoginSubMode_ConnectAccount);
    Require(connection->IsOpen() && LoginModeReplayAccess::IsConnected(mode),
        "login3 ConnectAccount did not open its mock account connection");
    Require(SelectSocket(listener.value, true, false,
            std::chrono::duration_cast<std::chrono::milliseconds>(kTimeout)) > 0,
        "login3 listener timed out waiting for ConnectAccount");
    SocketOwner accepted(accept(listener.value, nullptr, nullptr));
    Require(accepted.value != INVALID_SOCKET, "login3 mock account accept failed");
    const std::vector<u8> expectedHashRequest{0xdb, 0x01};
    const std::vector<u8> capturedHashRequest = ReceiveExactSocket(accepted.value, expectedHashRequest.size());
    Require(capturedHashRequest == expectedHashRequest,
        "explicit login3 profile did not automatically request the password-hash challenge");
    const std::vector<u8> challenge{
        0xdc, 0x01, 0x18, 0x00,
        0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29,
        0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33};
    const int challengeBytes = send(accepted.value, reinterpret_cast<const char*>(challenge.data()),
        static_cast<int>(challenge.size()), 0);
    Require(challengeBytes == static_cast<int>(challenge.size()),
        "login3 mock account server did not send the full selected-source challenge");
    const auto responseDeadline = Clock::now() + kTimeout;
    while (!LoginModeReplayAccess::HandshakeCleared(mode) && Clock::now() < responseDeadline) {
        LoginModeReplayAccess::RunPollNetwork(mode);
        if (!LoginModeReplayAccess::HandshakeCleared(mode)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    const std::vector<u8> expectedLogin{
        0xfa, 0x01, 0x0e, 0x69, 0x32, 0x01,
        'p','a','c','k','e','t','-','t','e','s','t',
        0,0,0,0,0,0,0,0,0,0,0,0,0,
        0xe3,0x0b,0xf7,0x39,0xe8,0x2f,0x48,0xe8,0x0e,0x85,0xd6,0x8a,0x47,0x98,0x1c,0xad,
        1, 1};
    Require(expectedLogin.size() == 48, "independent login3 byte fixture has the wrong size");
    const std::vector<u8> capturedLogin = ReceiveExactSocket(accepted.value, expectedLogin.size());
    Require(capturedLogin == expectedLogin,
        "production login3 sender differs from the 48-byte selected-source fixture");
    Require(LoginModeReplayAccess::HandshakeCleared(mode)
            && g_windowMgr.GetLoginStatus() == "Login: waiting for account server response...",
        "login3 response did not clear challenge state and wait for account reply");
    connection->Disconnect();
    std::cout << "{\"case_id\":\"login3_clientinfo_hash_request\",\"contract_id\":\"PK44.LOGIN-REQUESTS\",\"fixture_id\":\"PK44-01fa-md5-clientinfo-selected-source\",\"production_entry\":\"ConnectAccount -> 0x01DB -> 0x01DC -> OnAckHash -> 0x01FA\",\"request_01db_hex\":\"" << Hex(capturedHashRequest) << "\",\"challenge_hex\":\"" << Hex(challenge) << "\",\"expected_01fa_hex\":\"" << Hex(expectedLogin) << "\",\"captured_01fa_hex\":\"" << Hex(capturedLogin) << "\",\"clientinfo_from_selected_profile\":1,\"challenge_digest_matches_independent_vector\":true,\"challenge_request_automatic_for_login3\":true,\"handshake_cleared\":true,\"network_scope\":\"loopback mock account server only\",\"outcome\":\"PASS\"}\n";
}

void ElementalSnapshotReplay()
{
    SetCase("elemental_snapshot", "PK29.03", "selected-rathena-081d",
        "CRagConnection::RecvPacket -> default map router");
    CGameMode mode;
    CGameModePacketRouter router;
    RegisterDefaultGameModePacketHandlers(router);
    const auto oldElemental = g_session.m_elementalStatus;
    const auto homunGid = g_session.m_homunGID;
    const auto mercGid = g_session.m_merGID;
    TestConnection connection;
    LoopbackPeer peer(connection);
    const std::vector<u8> snapshot{0x1d, 0x08, 0x78, 0x56, 0x34, 0x12,
        1, 0, 0, 0x80, 2, 0, 0, 0x80, 3, 0, 0, 0, 4, 0, 0, 0};
    const std::vector<u8> next{0x80, 0, 0x99, 0, 0, 0, 0};
    Require(connection.QueueBytes({snapshot.begin(), snapshot.begin() + 9}), "could not queue partial elemental frame");
    std::vector<u8> frame;
    Require(!Receive(connection, frame) && connection.BufferedBytes() == 9,
        "partial elemental frame was consumed or lost");
    std::vector<u8> tail(snapshot.begin() + 9, snapshot.end());
    tail.insert(tail.end(), next.begin(), next.end());
    Require(connection.QueueBytes(tail) && Receive(connection, frame) && frame == snapshot,
        "elemental snapshot did not frame at 22 bytes");
    PacketView view{};
    int consumed = 0;
    Require(TryReadPacket(frame.data(), frame.size(), view, consumed) && router.Dispatch(mode, view),
        "elemental snapshot did not dispatch");
    const auto& status = g_session.m_elementalStatus;
    Require(status.gid == 0x12345678 && status.hp == 0x80000001 && status.maxHp == 0x80000002
        && status.sp == 3 && status.maxSp == 4 && g_session.m_homunGID == homunGid && g_session.m_merGID == mercGid,
        "elemental snapshot decoded incorrectly or changed another companion");
    Require(Receive(connection, frame) && frame == next && connection.BufferedBytes() == 0,
        "elemental snapshot disrupted the next map packet");
    g_session.m_elementalStatus = oldElemental;
}

void CompanionIdentityReplay()
{
    CGameMode mode;
    CGameModePacketRouter router;
    RegisterDefaultGameModePacketHandlers(router);
    const auto oldHomun = g_session.m_homunGID;
    const auto oldMerc = g_session.m_merGID;
    const auto oldHomunStatus = g_session.m_homunStatus;
    const auto oldMercStatus = g_session.m_merStatus;
    const auto oldHomunTarget = g_session.m_homunTarget;
    const auto oldMercTarget = g_session.m_merTarget;
    const auto oldSkills = g_session.GetMercSkillItems();
    g_session.m_homunGID = 0;
    g_session.m_merGID = 0;
    g_session.m_homunStatus = {};
    g_session.m_merStatus = {};
    Require(!g_windowMgr.MakeWindow(UIWindowMgr::WID_HOMUNINFOWND)
        && !g_windowMgr.MakeWindow(UIWindowMgr::WID_MERINFOWND),
        "companion status opened before the companion job was known");
    PLAYER_SKILL_INFO skill{};
    skill.SKID = 8201;
    g_session.SetMercSkillItem(skill);

    std::array<u8, 92> bytes{};
    bytes[0] = 0x30;
    bytes[1] = 0x02;
    bytes[4] = 0x11;
    bytes[7] = 0xf1;
    bytes[12] = 0x9b;
    bytes[13] = 0x02;
    bytes[14] = 0x22;
    bytes[17] = 0xf2;
    bytes[18] = 0xf9;
    bytes[19] = 0xff;
    bytes[34] = 'M';
    bytes[60] = 0x23;
    bytes[62] = 0x01;
    bytes[91] = 0x01;
    PacketView packet{};
    int consumed = 0;
    Require(TryReadPacket(bytes.data(), static_cast<int>(bytes.size()), packet, consumed) && consumed == 12,
        "homunculus identity did not frame at 12 bytes");
    Require(router.Dispatch(mode, packet) && g_session.m_homunGID == 0xf1000011u,
        "homunculus identity did not retain the unsigned GID");
    bytes[3] = 1;
    bytes[4] = 0x33;
    bytes[10] = 1;
    Require(router.Dispatch(mode, packet) && g_session.m_homunGID == 0xf1000011u
        && g_session.m_homunStatus.intimacy == 65536, "homunculus status lost the 32-bit update or replaced the GID");

    std::array<u8, 71> homun{};
    homun[0] = 0x2e;
    homun[1] = 0x02;
    std::fill_n(homun.begin() + 2, 24, '%');
    homun[26] = 1;
    homun[27] = 7;
    homun[29] = 10;
    homun[34] = 0xf0;
    homun[51] = 0xff;
    homun[52] = 0xff;
    homun[55] = 0x45;
    homun[62] = 0xf0;
    homun[69] = 5;
    g_windowMgr.ClearChatEvents();
    Require(!TryReadPacket(homun.data(), 69, packet, consumed), "homunculus accepted the short snapshot layout");
    Require(TryReadPacket(homun.data(), 71, packet, consumed) && consumed == 71 && router.Dispatch(mode, packet),
        "homunculus snapshot did not frame and dispatch");
    const auto& hs = g_session.m_homunStatus;
    Require(hs.name == std::string(24, '%') && hs.nameModified == 1 && hs.level == 7
        && hs.hunger == 10 && hs.accessory == 0xf000 && hs.combat.hp == -1 && hs.combat.sp == 0x45
        && hs.experience == -268435456 && hs.combat.attackRange == 5, "homunculus snapshot field layout differed");
    Require(hs.lastWarnedHunger == 10 && g_windowMgr.GetChatEvents().size() == 1
        && g_windowMgr.GetChatEvents().front().color == 0xff0000u, "homunculus hunger notice lost the selected route or color");
    homun[29] = 11;
    Require(router.Dispatch(mode, packet), "non-warning hunger snapshot was unhandled");
    homun[29] = 10;
    Require(router.Dispatch(mode, packet) && g_windowMgr.GetChatEvents().size() == 1,
        "repeated hunger threshold produced another warning");
    homun[29] = 5;
    Require(router.Dispatch(mode, packet) && hs.lastWarnedHunger == 5
        && g_windowMgr.GetChatEvents().size() == 2, "a different hunger threshold did not warn");

    std::array<u8, 8> parameter{0xa2, 0x02, 7, 0, 0xff, 0xff, 0xff, 0xff};
    Require(TryReadPacket(parameter.data(), 8, packet, consumed) && consumed == 8 && router.Dispatch(mode, packet)
        && g_session.m_merStatus.combat.sp == 0, "mercenary parameter bypassed the alive gate");
    bytes[20] = 0x22;
    bytes[21] = 0x80;
    Require(!TryReadPacket(bytes.data() + 12, 72, packet, consumed),
        "mercenary init accepted the shorter incompatible layout");
    Require(TryReadPacket(bytes.data() + 12, 80, packet, consumed) && consumed == 80,
        "mercenary init did not frame at the selected length");
    Require(router.Dispatch(mode, packet) && g_session.m_merGID == 0xf2000022u
        && g_session.GetMercSkillItems().empty(), "mercenary init did not set identity and clear the previous skills");
    const auto& ms = g_session.m_merStatus;
    Require(ms.alive && ms.name == "M" && ms.combat.attack == -7 && ms.combat.magicAttack == 0x8022
        && ms.combat.hp == 0x10023
        && ms.combat.attackRange == 256, "mercenary init field layout differed");
    Require(TryReadPacket(parameter.data(), 8, packet, consumed) && router.Dispatch(mode, packet)
        && ms.combat.sp == -1, "mercenary parameter lost the signed 32-bit value");
    parameter[2] = 0xff;
    Require(router.Dispatch(mode, packet) && ms.combat.sp == -1 && ms.combat.hp == 0x10023,
        "unknown mercenary parameter changed the snapshot");

    const auto homunCombat = hs.combat;
    const auto dispatchMercStat = [&](u8 type, u8 valueLow, u8 valueHigh) {
        parameter = {0xa2, 0x02, type, 0, valueLow, valueHigh, 0, 0};
        Require(TryReadPacket(parameter.data(), parameter.size(), packet, consumed)
            && consumed == parameter.size() && router.Dispatch(mode, packet),
            "selected mercenary stat update did not frame and dispatch");
    };
    dispatchMercStat(0x2b, 0x23, 0x81);
    dispatchMercStat(0x2d, 31, 0);
    dispatchMercStat(0x2f, 46, 0);
    dispatchMercStat(0x34, 17, 0);
    Require(ms.combat.magicAttack == 0x8123 && ms.combat.defense == 31
        && ms.combat.magicDefense == 46 && ms.combat.critical == 17
        && ms.combat.attack == -7 && ms.combat.sp == -1,
        "selected mercenary stat updates changed the wrong field or value");
    Require(hs.combat.magicAttack == homunCombat.magicAttack && hs.combat.defense == homunCombat.defense
        && hs.combat.magicDefense == homunCombat.magicDefense && hs.combat.critical == homunCombat.critical,
        "mercenary stat updates changed homunculus combat state");

    struct FeedActor : CGameActor {
        int emotions = 0;
        void SendMsg(CGameObject* sender, int message, msgparam_t first, msgparam_t second, msgparam_t third) override
        {
            Require(!sender && message == 21 && first == 4 && second == 0 && third == 0,
                "homunculus feeding sent the wrong actor message");
            ++emotions;
        }
    } homunActor, otherActor;
    homunActor.m_gid = g_session.m_homunGID;
    otherActor.m_gid = g_session.m_merGID;
    CWorld world;
    world.m_actorList.push_back(&homunActor);
    world.m_actorList.push_back(&otherActor);
    mode.m_world = &world;
    std::array<u8, 5> feed{0x2f, 0x02, 2, 0xf5, 1};
    g_windowMgr.ClearChatEvents();
    Require(!TryReadPacket(feed.data(), 4, packet, consumed), "short homunculus feed result framed");
    Require(TryReadPacket(feed.data(), 5, packet, consumed) && consumed == 5 && router.Dispatch(mode, packet)
        && hs.stateId == 29 && homunActor.emotions == 0 && g_windowMgr.GetChatEvents().empty(),
        "homunculus feed success did not set state 29 without failure effects");
    feed[2] = 0;
    Require(router.Dispatch(mode, packet), "homunculus feed failure did not dispatch");
    const auto& feedEvents = g_windowMgr.GetChatEvents();
    ITEM_INFO food{};
    food.SetItemId(501);
    const auto foodName = food.GetItemName();
    Require(feedEvents.size() == 2 && feedEvents[0].text == feedEvents[1].text
        && !foodName.empty() && feedEvents[0].text.find(foodName) != std::string::npos
        && feedEvents[0].color == 0xf51e1eu && feedEvents[1].color == 0xf51e1eu
        && feedEvents[0].source.route == ro::chat::NoticeRoute::Normal
        && feedEvents[1].source.route == ro::chat::NoticeRoute::Command
        && hs.stateId == 29 && homunActor.emotions == 1 && otherActor.emotions == 0,
        "homunculus feed failure lost the food name, notice routes, or actor emotion");
    g_session.m_homunGID = 0;
    Require(router.Dispatch(mode, packet) && g_windowMgr.GetChatEvents().size() == 4
        && homunActor.emotions == 1, "feed failure without a homunculus did not retain the notices");
    mode.m_world = nullptr;
    g_windowMgr.ClearChatEvents();

    g_session.m_homunGID = homunActor.m_gid;
    g_session.m_homunStatus.alive = false;
    g_session.m_merStatus.alive = false;
    const auto dispatchActor = [&](auto& bytes) {
        Require(TryReadPacket(bytes.data(), static_cast<int>(bytes.size()), packet, consumed)
            && consumed == static_cast<int>(bytes.size()) && router.Dispatch(mode, packet),
            "companion actor packet did not frame and dispatch");
    };
    const auto putGid = [](u8* dst, u32 gid) {
        for (unsigned i = 0; i < 4; ++i) dst[i] = static_cast<u8>(gid >> (8 * i));
    };
    const auto spawn = [&](u32 gid, u16 job) {
        std::array<u8, 53> entry{0x79, 0};
        putGid(entry.data() + 2, gid);
        entry[14] = static_cast<u8>(job);
        entry[15] = static_cast<u8>(job >> 8);
        dispatchActor(entry);
    };
    const auto vanish = [&](u32 gid, u8 reason) {
        std::array<u8, 7> removal{0x80};
        putGid(removal.data() + 2, gid);
        removal[6] = reason;
        dispatchActor(removal);
    };
    std::array<u8, 42> npcEntry{0x7c, 0, 9};
    putGid(npcEntry.data() + 3, homunActor.m_gid);
    npcEntry[21] = 0x71;
    npcEntry[22] = 0x17;
    dispatchActor(npcEntry);
    Require(!hs.alive, "dedicated NPC entry set the companion alive flag");
    g_session.m_merTarget = {homunActor.m_gid, 0, 0, false, true, true};
    vanish(homunActor.m_gid, 0);
    spawn(homunActor.m_gid, 6001);
    spawn(otherActor.m_gid, 6017);
    Require(hs.alive && ms.alive, "registered companion entries did not set alive flags");
    g_session.m_homunStatus.job = 6001;
    g_session.m_merStatus.job = 6017;
    auto* homunWindow = g_windowMgr.MakeWindow(UIWindowMgr::WID_HOMUNINFOWND);
    auto* mercWindow = g_windowMgr.MakeWindow(UIWindowMgr::WID_MERINFOWND);
    Require(homunWindow && mercWindow && homunWindow != mercWindow
        && g_windowMgr.MakeWindow(UIWindowMgr::WID_HOMUNINFOWND) == homunWindow,
        "companion status windows did not retain separate instances");
    Require(g_windowMgr.HasNativePopupAtPoint(mercWindow->m_x + 5, mercWindow->m_y + 5),
        "companion status did not claim pointer input from the Qt chat bridge");
    const auto homunDisplay = g_windowMgr.m_homunInfoWnd->GetDisplay();
    const auto mercDisplay = g_windowMgr.m_merInfoWnd->GetDisplay();
    Require(homunDisplay.gauges.size() == 4 && mercDisplay.gauges.size() == 2
        && homunDisplay.gauges[3].filledWidth == hs.hunger * 102 / 100
        && std::any_of(mercDisplay.text.begin(), mercDisplay.text.end(), [](const auto& row) {
            return row.value == "Archer Mercenary";
        }), "companion snapshots did not reach their respective status windows");
    vanish(homunActor.m_gid, 0);
    Require(hs.alive && g_windowMgr.m_homunInfoWnd == homunWindow,
        "ordinary companion removal reset the homunculus status window");
    Require(g_session.m_merTarget.gid == homunActor.m_gid && g_session.m_merTarget.attacking,
        "ordinary actor removal cleared the mercenary's target");
    spawn(homunActor.m_gid, 6001);
    vanish(homunActor.m_gid, 1);
    Require(!g_session.m_merTarget.gid && !g_session.m_merTarget.attacking
        && !g_session.m_merTarget.selected, "actor death did not clear the mercenary's matching target");
    Require(!hs.alive && ms.alive && g_session.m_homunGID == homunActor.m_gid
        && !g_windowMgr.m_homunInfoWnd && g_windowMgr.m_merInfoWnd == mercWindow,
        "homunculus death did not clear only the owned alive flag");
    spawn(homunActor.m_gid, 6001);
    Require(!hs.alive, "an update to an existing companion marked it alive again");
    vanish(homunActor.m_gid, 0);
    vanish(otherActor.m_gid, 0);

    const auto oldAid = g_session.m_aid;
    g_session.m_aid = 0x01234567;
    g_session.m_homunStatus.alive = true;
    homunWindow = g_windowMgr.MakeWindow(UIWindowMgr::WID_HOMUNINFOWND);
    Require(homunWindow && g_windowMgr.MakeWindow(UIWindowMgr::WID_HOMUNSKILLLISTWND),
        "could not reopen homunculus windows before skill 244");
    std::array<u8, 15> reset{0x1a, 0x01, 244};
    putGid(reset.data() + 6, g_session.m_aid);
    putGid(reset.data() + 10, otherActor.m_gid);
    reset[14] = 1;
    dispatchActor(reset);
    Require(hs.alive && g_windowMgr.m_homunInfoWnd == homunWindow && g_windowMgr.m_homunSkillListWnd,
        "another player's skill 244 cleared the local homunculus");
    putGid(reset.data() + 6, otherActor.m_gid);
    putGid(reset.data() + 10, g_session.m_aid);
    reset[14] = 0;
    dispatchActor(reset);
    Require(!hs.alive && ms.alive && g_session.m_homunGID == homunActor.m_gid && hs.stateId == 29
        && !g_windowMgr.m_homunInfoWnd && !g_windowMgr.m_homunSkillListWnd
        && g_windowMgr.m_merInfoWnd == mercWindow,
        "local skill 244 did not reset the homunculus status window and alive flag");
    Require(mercWindow->SendMsg(nullptr, 6, 135, 0, 0) && !g_windowMgr.m_merInfoWnd,
        "mercenary status close button did not delete its window");
    g_session.m_aid = oldAid;

    const auto oldElemental = g_session.m_elementalStatus;
    g_session.m_elementalStatus = {0x12345678, 1, 2, 3, 4};
    g_session.ResetCompanionMapState();
    Require(g_session.m_elementalStatus.gid == 0 && g_session.m_elementalStatus.maxSp == 0,
        "companion map reset retained the previous elemental snapshot");
    g_session.m_elementalStatus = oldElemental;
    mode.m_world = &g_world;
    constexpr u32 reentryHomun = 0xf1000041u;
    constexpr u32 reentryMerc = 0xf2000042u;
    npcEntry[2] = 8;
    putGid(npcEntry.data() + 3, reentryHomun);
    dispatchActor(npcEntry);
    npcEntry[2] = 9;
    putGid(npcEntry.data() + 3, reentryMerc);
    npcEntry[21] = 0x81; // 6017, the selected mercenary job.
    dispatchActor(npcEntry);
    Require(!hs.alive && !ms.alive && g_session.m_homunGID == 0 && g_session.m_merGID == 0,
        "nearby companion entries claimed ownership before the server identity packets");

    std::array<u8, 12> reentryHomunIdentity{0x30, 0x02};
    putGid(reentryHomunIdentity.data() + 4, reentryHomun);
    dispatchActor(reentryHomunIdentity);
    std::array<u8, 80> reentryMercIdentity{0x9b, 0x02};
    putGid(reentryMercIdentity.data() + 2, reentryMerc);
    dispatchActor(reentryMercIdentity);
    Require(hs.alive && hs.job == 6001 && g_session.m_homunGID == reentryHomun
        && ms.alive && ms.job == 6017 && g_session.m_merGID == reentryMerc,
        "selected companion spawn-before-identity sequence did not restore local ownership");
    vanish(reentryMerc, 0);
    Require(ms.alive && g_session.m_merGID == reentryMerc,
        "out-of-sight removal retired the active mercenary contract");

    for (u16 messageId : {u16{1266}, u16{1267}}) {
        dispatchActor(npcEntry);
        dispatchActor(reentryMercIdentity);
        g_session.SetMercSkillItem(skill);
        g_session.m_merTarget = {reentryHomun, 8, 9, true, true, true};
        const int pendingSkill = messageId == 1266 ? 8201 : 28;
        mode.m_skillUseInfo = {pendingSkill, 1, 5, 2};
        Require(g_windowMgr.MakeWindow(UIWindowMgr::WID_MERINFOWND)
            && g_windowMgr.MakeWindow(UIWindowMgr::WID_MERSKILLLISTWND),
            "could not open active mercenary windows before terminal notice");
        std::array<u8, 4> notice{0x91, 0x02, static_cast<u8>(messageId), static_cast<u8>(messageId >> 8)};
        dispatchActor(notice);
        Require(!ms.alive && ms.job == -1 && g_session.m_merGID == 0
            && g_session.m_merTarget.gid == 0 && !g_session.m_merTarget.attacking
            && g_session.GetMercSkillItems().empty()
            && !g_windowMgr.m_merInfoWnd && !g_windowMgr.m_merSkillListWnd,
            "terminal mercenary notice left active contract state or controls");
        Require(mode.m_skillUseInfo.id == (messageId == 1266 ? 0 : pendingSkill)
            && hs.alive && g_session.m_homunGID == reentryHomun,
            "terminal mercenary notice mishandled pending skill or homunculus state");
        vanish(reentryMerc, 0);
    }

    dispatchActor(npcEntry);
    dispatchActor(reentryMercIdentity);
    g_session.SetMercSkillItem(skill);
    g_session.m_merTarget = {reentryHomun, 8, 9, true, true, true};
    mode.m_skillUseInfo = {8201, 1, 5, 2};
    Require(g_windowMgr.MakeWindow(UIWindowMgr::WID_MERINFOWND)
        && g_windowMgr.MakeWindow(UIWindowMgr::WID_MERSKILLLISTWND),
        "could not open active mercenary windows before owner death");

    CPlayer owner;
    owner.m_gid = 0xf1000033;
    auto* oldPlayer = g_world.m_player;
    g_world.m_player = &owner;
    constexpr u32 reentryElemental = 0xf3000043;
    // MAIN 20080910 non-player spawn and walk carry object type 10 for BL_ELEM.
    npcEntry[2] = 10;
    putGid(npcEntry.data() + 3, reentryElemental);
    npcEntry[21] = 0x42;
    npcEntry[22] = 0x08; // EL_AGNI_S, job 2114.
    dispatchActor(npcEntry);
    auto* elemental = g_world.GetGameActorByAID(reentryElemental);
    Require(elemental && elemental->m_objectType == 10 && elemental->m_job == 2114 && !elemental->m_isPc,
        "selected elemental spawn did not create the expected non-player actor");
    std::array<u8, 67> elementalWalk{0xec, 0x02, 10};
    putGid(elementalWalk.data() + 3, reentryElemental);
    elementalWalk[7] = 150;
    elementalWalk[17] = 0x42;
    elementalWalk[18] = 0x08;
    elementalWalk[56] = 0x40; // Source (1, 0), destination (2, 0).
    elementalWalk[58] = 0x08;
    dispatchActor(elementalWalk);
    Require(g_world.GetGameActorByAID(reentryElemental) == elemental && elemental->m_objectType == 10
        && mode.m_actorPosList.at(reentryElemental).x == 1,
        "selected elemental walk did not update the same actor at its path start");
    std::array<u8, 22> elementalSnapshot{0x1d, 0x08};
    putGid(elementalSnapshot.data() + 2, reentryElemental);
    dispatchActor(elementalSnapshot);
    vanish(reentryElemental, 0);
    Require(g_session.m_elementalStatus.gid == reentryElemental && !g_world.GetGameActorByAID(reentryElemental),
        "ordinary visibility removal retired the elemental snapshot");
    vanish(reentryMerc, 0);
    Require(ms.alive && g_session.m_merGID == reentryMerc,
        "mercenary removal preceding owner death retired the contract early");
    vanish(owner.m_gid, 1);
    g_world.UnregisterActor(&owner);
    g_world.m_player = oldPlayer;
    Require(mode.m_isPlayerDead && !ms.alive && ms.job == -1 && g_session.m_merGID == 0
        && g_session.m_merTarget.gid == 0 && g_session.GetMercSkillItems().empty()
        && mode.m_skillUseInfo.id == 0 && !g_windowMgr.m_merInfoWnd && !g_windowMgr.m_merSkillListWnd
        && hs.alive && g_session.m_homunGID == reentryHomun && g_session.m_elementalStatus.gid == 0,
        "local owner death did not retire the mercenary and elemental independently of the homunculus");
    g_session.m_elementalStatus = oldElemental;

    vanish(reentryHomun, 0);
    mode.m_world = nullptr;

    g_session.m_homunGID = oldHomun;
    g_session.m_merGID = oldMerc;
    g_session.m_homunStatus = oldHomunStatus;
    g_session.m_merStatus = oldMercStatus;
    g_session.m_homunTarget = oldHomunTarget;
    g_session.m_merTarget = oldMercTarget;
    for (const auto& entry : oldSkills) g_session.SetMercSkillItem(entry);
}

void CompanionCommandReplay()
{
    CGameMode mode;
    auto* connection = CRagConnection::instance();
    connection->Disconnect();
    LoopbackPeer peer(*connection);

    Require(mode.SendMsg(198, 0x101, 0, 0) == 1
        && peer.ReceiveExact(5) == std::vector<u8>{0x2d, 0x02, 0, 0, 1},
        "homunculus command differed from selected wire layout");
    Require(mode.SendMsg(200, 0x102, 0, 0) == 1
        && peer.ReceiveExact(3) == std::vector<u8>{0x9f, 0x02, 2},
        "mercenary dismissal differed from selected wire layout");

    Require(mode.SendMsg(199, 0, 0, 0) == 0, "homunculus rename accepted a null name");
    const char shortName[] = "Al";
    const std::vector<u8> shortPacket = [] {
        std::vector<u8> bytes{0x31, 0x02, 'A', 'l'};
        bytes.resize(26);
        return bytes;
    }();
    Require(mode.SendMsg(199, reinterpret_cast<msgparam_t>(shortName), 0, 0) == 1
        && peer.ReceiveExact(26) == shortPacket, "homunculus rename did not zero-pad a short name");
    const char longName[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    std::vector<u8> longPacket{0x31, 0x02};
    longPacket.insert(longPacket.end(), longName, longName + 23);
    longPacket.push_back(0);
    Require(mode.SendMsg(199, reinterpret_cast<msgparam_t>(longName), 0, 0) == 1
        && peer.ReceiveExact(26) == longPacket, "homunculus rename did not terminate a truncated name");
    Require(SelectSocket(peer.peer.value, true, false, std::chrono::milliseconds(50)) == 0,
        "companion commands emitted unexpected trailing bytes");

    auto* oldMode = SkillPacketReplayAccess::Current();
    const auto oldModeType = SkillPacketReplayAccess::CurrentType();
    const auto oldHomunculus = g_session.m_homunStatus;
    const auto oldMercenary = g_session.m_merStatus;
    const auto oldMercGid = g_session.m_merGID;
    const auto oldMercTarget = g_session.m_merTarget;
    const auto oldMercSkills = g_session.GetMercSkillItems();
    SkillPacketReplayAccess::SetCurrent(&mode);
    g_session.m_homunStatus = {};
    g_session.m_homunStatus.job = 6001;
    g_session.m_homunStatus.alive = true;
    g_session.m_merStatus.job = 6017;
    g_session.m_merStatus.alive = true;
    auto* homun = static_cast<UICompanionStatusWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_HOMUNINFOWND));
    Require(homun && homun->GetNameEdit(), "homunculus status did not expose the one-time editor");

    const auto confirm = [&](UICompanionStatusWnd* window, int action, bool accept) {
        Require(window->SendMsg(nullptr, 6, action, 0, 0) && g_windowMgr.m_roomMessageBox
            && g_windowMgr.m_roomMessageBox->GetSourceOwner() == window
            && g_windowMgr.m_roomMessageBox->HasCancel(), "companion action lacked its owned confirmation");
        Require(SelectSocket(peer.peer.value, true, false, std::chrono::milliseconds(50)) == 0,
            "companion action sent before confirmation");
        g_windowMgr.m_roomMessageBox->Dismiss(accept);
    };
    const auto& buttons = homun->GetButtons();
    const auto feed = std::find_if(buttons.begin(), buttons.end(), [](const auto* button) { return button->m_id == 224; });
    Require(feed != buttons.end(), "homunculus status did not create the feed button");
    g_windowMgr.OnLBtnDown((*feed)->m_x + 2, (*feed)->m_y + 2);
    g_windowMgr.OnLBtnUp((*feed)->m_x + 2, (*feed)->m_y + 2);
    Require(g_windowMgr.m_roomMessageBox && g_windowMgr.m_roomMessageBox->GetMessageId() == 601,
        "clicking Feed did not open the selected prompt");
    g_windowMgr.m_roomMessageBox->Dismiss(false);
    Require(SelectSocket(peer.peer.value, true, false, std::chrono::milliseconds(50)) == 0,
        "cancelled feed emitted a packet");
    confirm(homun, 224, true);
    Require(peer.ReceiveExact(5) == std::vector<u8>{0x2d, 0x02, 0, 0, 1}
        && g_session.m_homunStatus.alive && g_windowMgr.m_homunInfoWnd == homun,
        "confirmed feed changed the owner or emitted the wrong packet");

    auto* edit = homun->GetNameEdit();
    edit->SetText("123456789012345678901234");
    Require(homun->SendMsg(nullptr, 6, 118, 0, 0) && g_windowMgr.m_roomMessageBox
        && !g_windowMgr.m_roomMessageBox->HasCancel() && g_windowMgr.m_roomMessageBox->GetMessageId() == 938,
        "24-byte rename did not show the source length error");
    g_windowMgr.m_roomMessageBox->Dismiss();
    edit->SetText("");
    g_windowMgr.OnLBtnDown(edit->m_x + 2, edit->m_y + 2);
    g_windowMgr.OnLBtnUp(edit->m_x + 2, edit->m_y + 2);
    for (const char letter : std::string_view(shortName)) g_windowMgr.OnChar(letter);
    Require(std::string_view(edit->GetText()) == shortName, "focused name editor did not receive character input");
    confirm(homun, 118, false);
    Require(homun->GetNameEdit() == edit && !g_session.m_homunStatus.nameModified,
        "cancelled rename consumed the one-time editor");
    confirm(homun, 118, true);
    Require(peer.ReceiveExact(26) == shortPacket && !homun->GetNameEdit()
        && g_session.m_homunStatus.name == shortName && g_session.m_homunStatus.nameModified,
        "confirmed rename did not send and consume the one-time editor");
    Require(homun->SendMsg(nullptr, 6, 223, 0, 0) && g_windowMgr.m_homunSkillListWnd,
        "homunculus skill button did not open its own list");
    confirm(homun, 225, true);
    Require(peer.ReceiveExact(5) == std::vector<u8>{0x2d, 0x02, 0, 0, 2}
        && !g_session.m_homunStatus.alive && !g_windowMgr.m_homunInfoWnd
        && !g_windowMgr.m_homunSkillListWnd && g_session.m_merStatus.alive,
        "confirmed homunculus dismissal did not close only the homunculus status");

    auto* merc = static_cast<UICompanionStatusWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_MERINFOWND));
    Require(merc, "mercenary status did not open for dismissal");
    g_session.m_merGID = 0xf1000034;
    g_session.m_merTarget = {0xf1000035, 8, 9, true, true, true};
    PLAYER_SKILL_INFO mercSkill{};
    mercSkill.SKID = 8201;
    g_session.SetMercSkillItem(mercSkill);
    mode.m_skillUseInfo = {8201, 1, 5, 2};
    Require(merc->SendMsg(nullptr, 6, 283, 0, 0) && g_windowMgr.m_merSkillListWnd,
        "mercenary skill button did not open its own list");
    confirm(merc, 284, true);
    Require(peer.ReceiveExact(3) == std::vector<u8>{0x9f, 0x02, 2}
        && !g_session.m_merStatus.alive && g_session.m_merStatus.job == -1
        && g_session.m_merGID == 0 && g_session.m_merTarget.gid == 0
        && g_session.GetMercSkillItems().empty() && mode.m_skillUseInfo.id == 0
        && !g_windowMgr.m_merInfoWnd && !g_windowMgr.m_merSkillListWnd,
        "confirmed mercenary dismissal left active contract state or controls");

    connection->Disconnect();
    g_session.m_merStatus = {};
    g_session.m_merStatus.job = 6017;
    g_session.m_merStatus.alive = true;
    g_session.m_merGID = 0xf1000034;
    g_session.m_merTarget = {0xf1000035, 8, 9, true, true, true};
    g_session.SetMercSkillItem(mercSkill);
    mode.m_skillUseInfo = {8201, 1, 5, 2};
    merc = static_cast<UICompanionStatusWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_MERINFOWND));
    Require(merc && merc->SendMsg(nullptr, 6, 283, 0, 0) && g_windowMgr.m_merSkillListWnd
        && merc->SendMsg(nullptr, 6, 284, 0, 0) && g_windowMgr.m_roomMessageBox,
        "could not open mercenary dismissal after disconnect");
    g_windowMgr.m_roomMessageBox->Dismiss(true);
    Require(g_session.m_merStatus.alive && g_session.m_merGID == 0xf1000034
        && g_session.m_merTarget.gid == 0xf1000035 && g_session.GetMercSkillItems().size() == 1
        && mode.m_skillUseInfo.id == 8201 && g_windowMgr.m_merInfoWnd == merc && g_windowMgr.m_merSkillListWnd,
        "failed dismissal send retired the mercenary contract");
    g_windowMgr.CloseCompanionWindows(false);
    g_session.ClearMercSkillItems();

    g_session.m_homunStatus = oldHomunculus;
    g_session.m_merStatus = oldMercenary;
    g_session.m_merGID = oldMercGid;
    g_session.m_merTarget = oldMercTarget;
    for (const auto& entry : oldMercSkills) g_session.SetMercSkillItem(entry);
    SkillPacketReplayAccess::Restore(oldMode, oldModeType);
}

void CompanionSkillListReplay()
{
    CGameMode mode;
    CGameModePacketRouter router;
    RegisterDefaultGameModePacketHandlers(router);
    const auto oldHomun = g_session.GetHomunSkillItems();
    const auto oldMerc = g_session.GetMercSkillItems();
    const auto oldHomunStatus = g_session.m_homunStatus;
    const auto oldMode = SkillPacketReplayAccess::Current();
    const auto oldModeType = SkillPacketReplayAccess::CurrentType();
    const auto oldHomunUse = g_session.GetHomunSkillUseLevel(8000);
    const auto oldMercUse = g_session.GetMerSkillUseLevel(8200);
    SkillPacketReplayAccess::SetCurrent(&mode);
    g_session.m_homunStatus.skillPoints = 0;

    const auto listPacket = [&](u16 opcode, u16 skillId, u8 count, u8 level = 4) {
        std::vector<u8> bytes(4 + 37 * count);
        bytes[0] = static_cast<u8>(opcode);
        bytes[1] = static_cast<u8>(opcode >> 8);
        bytes[2] = static_cast<u8>(bytes.size());
        for (u8 i = 0; i < count; ++i) {
            auto* row = bytes.data() + 4 + 37 * i;
            row[0] = static_cast<u8>(skillId + i);
            row[1] = static_cast<u8>((skillId + i) >> 8);
            row[2] = 1;
            row[6] = level;
            row[8] = 7;
            row[36] = 1;
        }
        PacketView view{};
        int consumed = 0;
        Require(TryReadPacket(bytes.data(), static_cast<int>(bytes.size()), view, consumed)
            && consumed == static_cast<int>(bytes.size()) && router.Dispatch(mode, view),
            "companion skill list packet did not frame and dispatch");
    };
    listPacket(0x0235, 8000, 3);
    listPacket(0x029d, 8200, 1);
    auto* homun = static_cast<UICompanionSkillListWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_HOMUNSKILLLISTWND));
    auto* merc = static_cast<UICompanionSkillListWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_MERSKILLLISTWND));
    Require(homun && merc && homun->GetRows().size() == 2 && merc->GetRows().size() == 1
        && homun->GetRows().front().id == 8000 && merc->GetRows().front().id == 8200,
        "companion list mixed caches or rendered the wrong rows");
    homun->OnWheel(-1);
    Require(homun->GetRows().front().id == 8001, "companion list did not scroll to the next row");
    homun->OnWheel(1);
    g_session.SetHomunSkillUseLevel(8000, 2);
    g_session.SetMerSkillUseLevel(8200, 3);
    homun->OnLBtnDown(homun->m_x + 10, homun->m_y + 22);
    homun->OnMouseMove(homun->m_x + 30, homun->m_y + 22);
    Require(mode.m_dragInfo.skillId == 8000 && mode.m_dragInfo.skillLevel == 2
        && mode.m_dragType == static_cast<int>(DragType::ShortcutSkill),
        "companion row did not carry its family skill and use level into shortcut drag");
    merc->OnLBtnDblClk(merc->m_x + 10, merc->m_y + 22);
    Require(mode.m_skillUseInfo.id == 8200 && mode.m_skillUseInfo.useType == 2,
        "double-click did not arm the mercenary skill");
    Require(!homun->GetRows().front().upgrade && !merc->GetRows().front().upgrade,
        "upgrade control ignored family or point gating");
    homun->Move(20, 20);
    merc->Move(330, 20);
    g_session.m_homunStatus.skillPoints = 1;
    const auto row = homun->GetRows().front();
    Require(row.upgrade && !merc->GetRows().front().upgrade, "homunculus upgrade control failed its family gate");
    const int upgradeX = homun->m_x + row.upgradeX + 1;
    const int upgradeY = homun->m_y + row.y + row.upgradeY + 1;
    auto* connection = CRagConnection::instance();
    connection->Disconnect();
    LoopbackPeer peer(*connection);
    g_windowMgr.OnLBtnDown(upgradeX, upgradeY);
    g_windowMgr.OnLBtnUp(upgradeX + row.upgradeSize, upgradeY);
    Require(SelectSocket(peer.peer.value, true, false, std::chrono::milliseconds(50)) == 0,
        "released-outside upgrade emitted a packet");
    const auto clickUpgrade = [&] {
        g_windowMgr.OnLBtnDown(upgradeX, upgradeY);
        g_windowMgr.OnLBtnUp(upgradeX, upgradeY);
    };
    clickUpgrade();
    Require(peer.ReceiveExact(4) == std::vector<u8>{0x12, 0x01, 0x40, 0x1f}
        && mode.m_isReqUpgradeSkillLevel && g_session.m_homunStatus.skillPoints == 1
        && homun->GetRows().front().level == 4,
        "homunculus upgrade did not send the selected request or updated state before the server");
    clickUpgrade();
    Require(SelectSocket(peer.peer.value, true, false, std::chrono::milliseconds(50)) == 0,
        "pending upgrade emitted a duplicate packet");
    const std::array<u8, 11> update{0x39, 0x02, 0x40, 0x1f, 5, 0, 11, 0, 1, 0, 0};
    PacketView updated{};
    int consumed = 0;
    Require(TryReadPacket(update.data(), static_cast<int>(update.size()), updated, consumed)
        && consumed == 11 && router.Dispatch(mode, updated)
        && homun->GetRows().front().level == 5 && homun->GetRows().front().spCost == 11
        && !homun->GetRows().front().upgrade && !mode.m_isReqUpgradeSkillLevel
        && merc->GetRows().front().level == 4, "companion update did not acknowledge only the matching row");
    const std::array<u8, 71> property{0x2e, 0x02};
    Require(TryReadPacket(property.data(), static_cast<int>(property.size()), updated, consumed)
        && consumed == 71 && router.Dispatch(mode, updated)
        && g_session.m_homunStatus.skillPoints == 0, "homunculus property did not refresh skill points");
    listPacket(0x0235, 8000, 3, 5);
    Require(homun->GetRows().front().level == 5 && !homun->GetRows().front().upgrade,
        "refreshed homunculus list bypassed the updated skill point gate");
    listPacket(0x0235, 8000, 0);
    Require(homun->GetRows().empty() && merc->GetRows().size() == 1,
        "empty homunculus list packet did not refresh only its family");

    g_windowMgr.CloseCompanionWindows(true);
    g_windowMgr.CloseCompanionWindows(false);
    g_session.ClearHomunSkillItems();
    g_session.ClearMercSkillItems();
    for (const auto& row : oldHomun) g_session.SetHomunSkillItem(row);
    for (const auto& row : oldMerc) g_session.SetMercSkillItem(row);
    g_session.m_homunStatus = oldHomunStatus;
    g_session.SetHomunSkillUseLevel(8000, oldHomunUse);
    g_session.SetMerSkillUseLevel(8200, oldMercUse);
    SkillPacketReplayAccess::Restore(oldMode, oldModeType);
    connection->Disconnect();
}

void LocalizedSystemNoticeReplay()
{
    CGameMode mode;
    CGameModePacketRouter router;
    RegisterDefaultGameModePacketHandlers(router);
    const auto oldMerc = g_session.m_merStatus;
    const auto oldGid = g_session.m_merGID;
    const auto oldTarget = g_session.m_merTarget;
    const auto oldSkills = g_session.GetMercSkillItems();
    g_session.m_merGID = 0x12345678;
    g_session.m_merStatus.job = 6017;
    g_session.m_merStatus.alive = true;
    auto* info = g_windowMgr.MakeWindow(UIWindowMgr::WID_MERINFOWND);
    auto* skills = g_windowMgr.MakeWindow(UIWindowMgr::WID_MERSKILLLISTWND);
    Require(info && skills, "could not open mercenary windows for the system notice");
    g_windowMgr.ClearChatEvents();

    const std::array<u8, 12> stream{
        0x91, 0x02, 0xf2, 0x04,  // Expiry
        0x91, 0x02, 0xf3, 0x04,  // Death
        0x91, 0x02, 0xc5, 0x04   // Red party notice
    };
    PacketView view{};
    int consumed = 0;
    Require(!TryReadPacket(stream.data(), 3, view, consumed), "short localized notice framed");
    for (std::size_t offset = 0; offset < stream.size(); offset += 4) {
        Require(TryReadPacket(stream.data() + offset, static_cast<int>(stream.size() - offset), view, consumed)
            && consumed == 4 && router.Dispatch(mode, view), "localized notice did not frame and dispatch");
    }
    const auto& events = g_windowMgr.GetChatEvents();
    Require(g_windowMgr.m_chatWndShow && events.size() == 6, "localized notices did not show both chat routes");
    for (std::size_t i = 0; i < events.size(); i += 2) {
        const auto color = i < 4 ? 0xffffffu : 0xff0000u;
        Require(!events[i].text.empty() && events[i].text == events[i + 1].text
            && events[i].color == color && events[i + 1].color == color
            && events[i].source.route == ro::chat::NoticeRoute::Normal
            && events[i + 1].source.route == ro::chat::NoticeRoute::Command,
            "localized notice lost the source text, color, or paired routes");
    }
    Require(events[0].text != events[2].text && !g_session.m_merStatus.alive && g_session.m_merGID == 0
        && !g_windowMgr.m_merInfoWnd && !g_windowMgr.m_merSkillListWnd,
        "mercenary system notices failed to retire the contract or shared a message");
    g_session.m_merStatus = oldMerc;
    g_session.m_merGID = oldGid;
    g_session.m_merTarget = oldTarget;
    for (const auto& entry : oldSkills) g_session.SetMercSkillItem(entry);
    g_windowMgr.ClearChatEvents();
}

bool ReplayTrySourceSkillOnActor(CGameMode& mode, CGameActor* target, bool shift, bool alt, bool control);
bool ReplayRequestPendingGroundSkill(CGameMode& mode, int x, int y, bool shift);

void PendingCompanionActorSkillReplay()
{
    struct RecordingPlayer : CPlayer {
        std::vector<int> commands;
        void SendMsg(CGameObject*, int message, msgparam_t, msgparam_t, msgparam_t) override
        {
            commands.push_back(message);
        }
    } player;
    CGameMode mode;
    CWorld world;
    world.m_player = &player;
    mode.m_world = &world;
    CGameActor target;
    target.m_gid = 0x12345678;
    target.m_job = 1002;
    const auto oldHomun = g_session.m_homunStatus;
    const auto oldMerc = g_session.m_merStatus;
    const auto oldHomunTarget = g_session.m_homunTarget;
    const auto oldMercTarget = g_session.m_merTarget;
    const auto oldNoShift = g_session.m_isNoShift;
    const auto oldWeight = g_session.m_weight;
    const auto oldMaxWeight = g_session.m_maxWeight;
    g_session.m_isNoShift = 0;
    g_session.m_weight = 0;
    g_session.m_maxWeight = 1000;
    g_session.m_homunStatus.alive = true;
    g_session.m_merStatus.alive = true;
    g_session.m_homunTarget = {};
    g_session.m_merTarget = {17, 1, 2, false, false, false};
    auto* connection = CRagConnection::instance();
    connection->Disconnect();
    LoopbackPeer peer(*connection);

    mode.m_skillUseInfo = {8004, 1, 10, 2};
    Require(ReplayTrySourceSkillOnActor(mode, &target, false, false, false)
        && player.commands.empty(), "inactive homunculus skill fell through to the player");
    Require(ReplayTrySourceSkillOnActor(mode, &target, false, true, false)
        && g_session.m_homunTarget.gid == target.m_gid && g_session.m_homunTarget.attacking
        && g_session.m_merTarget.gid == 17 && !g_session.m_merTarget.selected,
        "pending homunculus skill did not stop after its own actor target branch");
    mode.m_skillUseInfo = {8201, 1, 10, 2};
    Require(ReplayTrySourceSkillOnActor(mode, &target, false, true, false)
        && player.commands.empty() && g_session.m_merTarget.gid == target.m_gid,
        "inactive mercenary skill fell through to the player");
    target.m_isPc = 1;
    target.m_job = 0;
    mode.m_skillUseInfo = {8004, 1, 10, 2};
    Require(ReplayTrySourceSkillOnActor(mode, &target, false, false, false)
        && player.commands.empty(), "homunculus skill on a PC fell through to the player");
    Require(SelectSocket(peer.peer.value, true, false, std::chrono::milliseconds(50)) == 0,
        "inactive companion actor skills emitted player network bytes");

    target.m_isPc = 0;
    target.m_job = 1002;
    target.m_stateId = companion_ai::MOTION_STAND;
    auto* oldMode = SkillPacketReplayAccess::Current();
    const auto oldModeType = SkillPacketReplayAccess::CurrentType();
    const auto oldHomunGid = g_session.m_homunGID;
    const auto oldMercGid = g_session.m_merGID;
    const auto oldAid = g_session.m_aid;
    const auto oldMercSkills = g_session.GetMercSkillItems();
    SkillPacketReplayAccess::SetCurrent(&mode);
    CGameActor homunculus;
    homunculus.m_gid = 0xf1000041;
    homunculus.m_stateId = companion_ai::MOTION_STAND;
    CGameActor mercenary;
    mercenary.m_gid = 0xf1000042;
    mercenary.m_stateId = companion_ai::MOTION_STAND;
    g_session.m_homunGID = homunculus.m_gid;
    g_session.m_merGID = mercenary.m_gid;
    world.RegisterActor(&target);
    world.RegisterActor(&homunculus);
    world.RegisterActor(&mercenary);

    const auto requireCompanionSkill = [&](CGameActor& caster, int skill, u8 skillLow, u8 skillHigh, bool reserved) {
        mode.m_skillUseInfo = {skill, 3, 1, 2};
        Require(ReplayTrySourceSkillOnActor(mode, &target, reserved, false, false) && player.commands.empty(),
            "active companion skill fell through to the player");
        Require(SelectSocket(peer.peer.value, true, false, std::chrono::milliseconds(30)) == 0,
            "companion skill bypassed the native controller");
        mode.m_skillUseInfo = {};
        companion_ai::TickOwnedCompanionActor(caster);
        companion_ai::TickOwnedCompanionActor(caster);
        if (reserved) companion_ai::TickOwnedCompanionActor(caster);
        Require(peer.ReceiveExact(10) == std::vector<u8>{0x38, 0x04, 3, 0, skillLow, skillHigh,
            0x78, 0x56, 0x34, 0x12}, "native companion skill emitted an unexpected target request");
        Require(SelectSocket(peer.peer.value, true, false, std::chrono::milliseconds(30)) == 0,
            "companion skill dispatch emitted an extra request");
    };
    requireCompanionSkill(homunculus, 8004, 0x44, 0x1f, true);
    requireCompanionSkill(mercenary, 8201, 0x09, 0x20, false);

    g_session.m_aid = 0x11223344;
    player.m_gid = g_session.m_aid;
    player.m_isPc = 1;
    const std::array<const char*, 3> supportNames{"MER_KYRIE", "MER_BLESSING", "MER_INCAGI"};
    CGameModePacketRouter router;
    RegisterDefaultGameModePacketHandlers(router);
    std::array<u8, 4 + 37 * 4> supportList{0x9d, 0x02, 152, 0};
    for (int index = 0; index < 4; ++index) {
        auto* row = supportList.data() + 4 + 37 * index;
        row[0] = static_cast<u8>(8238 + index);
        row[1] = 0x20;
        row[2] = 16;
        row[6] = 3;
        row[10] = 9;
    }
    PacketView view{};
    int consumed = 0;
    Require(TryReadPacket(supportList.data(), supportList.size(), view, consumed)
        && consumed == supportList.size() && router.Dispatch(mode, view)
        && !g_session.GetMercSkillItemBySkillId(8241),
        "selected mercenary list did not frame or accepted an unusable skill");
    for (int skill = 8238; skill <= 8240; ++skill) {
        Require(std::string_view(SKILL_INFO::GetSkillIdNameBySkillId(skill)) == supportNames[skill - 8238],
            "mercenary support skill name did not match its selected ID");
        const auto* row = g_session.GetMercSkillItemBySkillId(skill);
        Require(row && row->m_isValid && row->level == 3 && row->attackRange == 9,
            "mercenary support skill was dropped from the selected skill list");
        mode.SendMsg(126, reinterpret_cast<msgparam_t>(row), 3, 0);
        Require(mode.m_skillUseInfo.id == skill && mode.m_skillUseInfo.level == 3 && mode.m_skillUseInfo.useType == 4,
            "mercenary support skill did not arm owner targeting from its skill row");
        Require(ReplayTrySourceSkillOnActor(mode, &player, false, false, false) && player.commands.empty(),
            "mercenary support skill entered the player route");
        mode.m_skillUseInfo = {};
        mercenary.m_stateId = kGameActorCastingStateId;
        companion_ai::TickOwnedCompanionActor(mercenary);
        companion_ai::TickOwnedCompanionActor(mercenary);
        Require(SelectSocket(peer.peer.value, true, false, std::chrono::milliseconds(30)) == 0,
            "busy mercenary sent a support skill before it could act");
        mercenary.m_stateId = companion_ai::MOTION_STAND;
        companion_ai::TickOwnedCompanionActor(mercenary);
        Require(peer.ReceiveExact(10) == std::vector<u8>{0x38, 0x04, 3, 0, static_cast<u8>(skill), 0x20,
            0x44, 0x33, 0x22, 0x11}, "mercenary support skill sent the wrong owner target");
    }

    C3dAttr attr;
    attr.m_width = attr.m_height = 20;
    attr.m_zoom = 5;
    world.m_attr = &attr;
    mercenary.m_pos.x = mercenary.m_pos.z = 0;
    mode.m_skillUseInfo = {8210, 2, 1, 1};
    Require(ReplayRequestPendingGroundSkill(mode, 11, 10, false) && player.commands.empty(),
        "active mercenary ground skill entered the player route");
    mode.m_skillUseInfo = {};
    companion_ai::TickOwnedCompanionActor(mercenary);
    std::vector<u8> groundRequest(102, 0);
    groundRequest[0] = 0x7e;
    groundRequest[5] = 2;
    groundRequest[9] = 0x12;
    groundRequest[10] = 0x20;
    groundRequest[12] = 11;
    groundRequest[20] = 10;
    Require(peer.ReceiveExact(groundRequest.size()) == groundRequest,
        "in-range ground skill emitted a move or the wrong skill request");
    Require(SelectSocket(peer.peer.value, true, false, std::chrono::milliseconds(30)) == 0,
        "mercenary ground skill emitted an extra request");

    mode.m_skillUseInfo = {8210, 2, 1, 1};
    Require(ReplayRequestPendingGroundSkill(mode, 16, 10, false) && player.commands.empty(),
        "out-of-range mercenary ground skill entered the player route");
    mode.m_skillUseInfo = {};
    companion_ai::TickOwnedCompanionActor(mercenary);
    Require(peer.ReceiveExact(9) == std::vector<u8>{0x32, 0x02, 0x42, 0x00, 0x00, 0xf1, 4, 0, 0xa0},
        "out-of-range ground skill sent an unexpected companion approach request");
    Require(SelectSocket(peer.peer.value, true, false, std::chrono::milliseconds(30)) == 0,
        "out-of-range mercenary sent a ground skill before approaching");
    mode.GetClientCoor(15, 10, mercenary.m_pos.x, mercenary.m_pos.z);
    mercenary.m_stateId = kGameActorCastingStateId;
    companion_ai::TickOwnedCompanionActor(mercenary);
    Require(SelectSocket(peer.peer.value, true, false, std::chrono::milliseconds(30)) == 0,
        "busy mercenary sent a ground skill after approaching");
    mercenary.m_stateId = companion_ai::MOTION_STAND;
    companion_ai::TickOwnedCompanionActor(mercenary);
    groundRequest[12] = 16;
    Require(peer.ReceiveExact(groundRequest.size()) == groundRequest,
        "mercenary dropped the ground skill before it could act");
    companion_ai::TickOwnedCompanionActor(mercenary);
    Require(SelectSocket(peer.peer.value, true, false, std::chrono::milliseconds(30)) == 0,
        "mercenary repeated the completed ground skill");

    mode.GetClientCoor(15, 10, homunculus.m_pos.x, homunculus.m_pos.z);
    mode.GetClientCoor(15, 10, player.m_pos.x, player.m_pos.z);
    mode.GetClientCoor(16, 10, target.m_pos.x, target.m_pos.z);
    homunculus.m_job = 6001;
    mercenary.m_job = 6017;
    g_session.m_homunStatus.combat.attackSpeed = 1;
    g_session.m_merStatus.combat.attackSpeed = 1;
    g_session.m_homunTarget = {};
    g_session.m_merTarget = {};
    g_session.ApplyCompanionActorClick(target.m_gid, false);
    for (CGameActor* caster : {&homunculus, &mercenary}) {
        companion_ai::TickOwnedCompanionActor(*caster);
        companion_ai::TickOwnedCompanionActor(*caster);
        Require(peer.ReceiveExact(11) == std::vector<u8>{0x33, 0x02, static_cast<u8>(caster->m_gid),
            0, 0, 0xf1, 0x78, 0x56, 0x34, 0x12, 0}, "Alt click did not send a companion attack");
    }
    g_session.ApplyCompanionActorClick(target.m_gid, true);
    Require(homunculus.m_homunResMsg.command == companion_ai::ATTACK_OBJECT_CMD
        && mercenary.m_merResMsg.command == companion_ai::ATTACK_OBJECT_CMD,
        "Alt+Shift did not reserve a companion attack");
    g_session.ApplyCompanionActorClick(target.m_gid, false);
    Require(!g_session.m_homunTarget.attacking && !g_session.m_merTarget.attacking,
        "second Alt click did not clear both manual attacks");
    Require(homunculus.m_homunMsg.command == companion_ai::STOP_CMD
        && mercenary.m_merMsg.command == companion_ai::STOP_CMD,
        "second Alt click did not replace the attack with a stop command");
    Require(SelectSocket(peer.peer.value, true, false, std::chrono::milliseconds(30)) == 0,
        "stopping a companion attack bypassed the native controller");
    for (CGameActor* caster : {&homunculus, &mercenary}) {
        companion_ai::TickOwnedCompanionActor(*caster);
        companion_ai::TickOwnedCompanionActor(*caster);
        Require(peer.ReceiveExact(11) == std::vector<u8>{0x33, 0x02, static_cast<u8>(caster->m_gid),
            0, 0, 0xf1, 0, 0, 0, 0, 0}, "Alt click cancellation did not clear the companion's server target");
    }
    if (SelectSocket(peer.peer.value, true, false, std::chrono::milliseconds(30)) != 0) {
        throw std::runtime_error("cancelling a manual attack emitted extra opcode " + Hex(peer.ReceiveExact(2)));
    }
    world.m_attr = nullptr;

    world.UnregisterActor(&target);
    world.UnregisterActor(&homunculus);
    world.UnregisterActor(&mercenary);
    SkillPacketReplayAccess::Restore(oldMode, oldModeType);
    g_session.m_homunGID = oldHomunGid;
    g_session.m_merGID = oldMercGid;
    g_session.m_aid = oldAid;
    g_session.ClearMercSkillItems();
    for (const auto& row : oldMercSkills) g_session.SetMercSkillItem(row);
    mode.m_skillUseInfo = {5, 1, 10, 2};
    Require(ReplayTrySourceSkillOnActor(mode, &target, false, false, false)
        && player.commands == std::vector<int>{41, 90},
        "ordinary player skill did not retain its existing actor dispatch");

    player.commands.clear();
    mode.m_skillUseInfo = {8210, 2, 1, 1};
    Require(ReplayRequestPendingGroundSkill(mode, 11, 10, false) && player.commands == std::vector<int>{65, 90},
        "missing mercenary actor did not preserve the ground skill fallback");

    connection->Disconnect();
    g_session.m_homunStatus = oldHomun;
    g_session.m_merStatus = oldMerc;
    g_session.m_homunTarget = oldHomunTarget;
    g_session.m_merTarget = oldMercTarget;
    g_session.m_isNoShift = oldNoShift;
    g_session.m_weight = oldWeight;
    g_session.m_maxWeight = oldMaxWeight;
    mode.m_world = nullptr;
}

std::vector<u8> MapCellUpdateFrame(u16 x, u16 y, u16 type, std::string_view name)
{
    Require(name.size() <= 16, "map cell fixture name exceeds its wire field");
    std::vector<u8> bytes(24);
    bytes[0] = 0x92;
    bytes[1] = 0x01;
    const std::array<u16, 3> fields{x, y, type};
    for (size_t index = 0; index < fields.size(); ++index) {
        bytes[2 + index * 2] = static_cast<u8>(fields[index]);
        bytes[3 + index * 2] = static_cast<u8>(fields[index] >> 8);
    }
    std::copy(name.begin(), name.end(), bytes.begin() + 8);
    return bytes;
}

void MapCellUpdateReplay()
{
    C3dAttr attr;
    attr.m_width = 3;
    attr.m_height = 3;
    attr.m_cells.resize(9);
    attr.m_cells[4].h1 = 12.0f;
    CWorld world;
    CWorld otherWorld;
    world.m_attr = &attr;
    otherWorld.m_attr = &attr;
    CGameMode mode;
    mode.m_world = &world;
    std::memcpy(mode.m_rswName, "prontera.rsw", sizeof("prontera.rsw"));

    CGameModePacketRouter router;
    RegisterDefaultGameModePacketHandlers(router);
    const auto dispatch = [&](u16 x, u16 y, u16 type, std::string_view name) {
        auto bytes = MapCellUpdateFrame(x, y, type, name);
        bytes.push_back(0);
        PacketView packet{};
        int consumed = 0;
        Require(TryReadPacket(bytes.data(), static_cast<int>(bytes.size()), packet, consumed)
            && consumed == 24 && router.Dispatch(mode, packet), "map cell update did not frame and dispatch");
    };

    CPathFinder pathFinder;
    pathFinder.SetMap(&world);
    CPathInfo path;
    const auto findPath = [&] { return pathFinder.FindPath(10, 0, 1, 8, 8, 2, 1, 150, &path); };
    const auto usesCenter = [&] {
        return std::any_of(path.m_cells.begin(), path.m_cells.end(),
            [](const PathCell& cell) { return cell.x == 1 && cell.y == 1; });
    };
    Require(findPath() && usesCenter(), "baseline route skipped the center");

    dispatch(1, 1, 5, "PRONTERA.GAT");
    dispatch(1, 1, 5, "prontera.gat");
    Require(world.GetMapCellType(1, 1) == 5 && !world.IsMapCellWalkable(1, 1)
        && otherWorld.GetMapCellType(1, 1) == 0 && attr.m_cells[4].h1 == 12.0f,
        "Ice Wall changed the base GAT or failed to update the effective cell");
    Require(findPath() && !usesCenter(), "movement path did not detour around Ice Wall");

    dispatch(1, 1, 0, "other.gat");
    dispatch(1, 1, 0, "");
    dispatch(1, 1, 7, "prontera.gat");
    dispatch(0xffff, 1, 5, "prontera.gat");
    dispatch(3, 0, 5, "prontera.gat");
    Require(world.GetMapCellType(1, 1) == 5 && world.IsMapCellWalkable(0, 1),
        "invalid update changed a map cell");

    for (const u16 type : {0, 2, 3, 4, 6}) {
        dispatch(1, 1, type, "prontera.gat");
        Require(world.GetMapCellType(1, 1) == type && findPath() && usesCenter(),
            "walkable cell type did not restore the direct path");
    }
    dispatch(1, 1, 1, "prontera.gat");
    Require(!world.IsMapCellWalkable(1, 1) && findPath() && !usesCenter(),
        "blocked cell did not prevent direct movement");
    dispatch(1, 1, 0, "prontera.gat");
    Require(findPath() && usesCenter(), "restoration to base did not restore the direct path");
    mode.m_world = nullptr;
}

void MapTypePropertyReplay()
{
    struct RecordingPlayer : CPlayer {
        int commands = 0;
        void SendMsg(CGameObject*, int, msgparam_t, msgparam_t, msgparam_t) override { ++commands; }
    } player;
    CGameMode mode;
    CWorld world;
    world.m_player = &player;
    mode.m_world = &world;
    CGameModePacketRouter router;
    RegisterDefaultGameModePacketHandlers(router);
    const auto oldType = g_session.m_curMapType;
    const auto oldTable = g_session.m_mapTypeInfo;
    const auto oldMinEffect = g_session.m_isMinEffect;
    const auto oldMinEffectPreference = g_session.m_isMinEffectOld;
    const auto oldParty = g_session.m_partyList;
    const auto oldWeight = g_session.m_weight;
    const auto oldMaxWeight = g_session.m_maxWeight;
    const auto oldNoShift = g_session.m_isNoShift;
    g_session.m_isMinEffectOld = false;
    g_session.m_weight = 0;
    g_session.m_maxWeight = 1000;
    g_session.m_isNoShift = 0;
    g_session.m_partyList.clear();

    std::string table = "// source loader uses fifteen values per type\r\n";
    for (int row = 0; row < 30; ++row) {
        for (int column = 0; column < 15; ++column) {
            const bool enabled = (row == 5 && column == 7) || (row == 7 && column == 8)
                || (row == 9 && column == 9);
            table += enabled ? "1 " : "0 ";
            if ((row * 15 + column + 1) % 10 == 0) table += "/* physical row */\r\n";
        }
    }
    const auto oldPath = std::filesystem::current_path();
    const auto tableRoot = std::filesystem::temp_directory_path()
        / ("open-midgard-map-type-" + std::to_string(Clock::now().time_since_epoch().count()));
    const auto tablePath = tableRoot / "data" / "mapinfotable.scp";
    std::filesystem::create_directories(tablePath.parent_path());
    std::ofstream tableFile(tablePath, std::ios::binary);
    tableFile << table;
    tableFile.close();
    Require(tableFile.good(), "could not write the map type resource fixture");
    const auto oldFolderFirst = g_readFolderFirst;
    std::filesystem::current_path(tableRoot);
    g_readFolderFirst = 1;
    const bool loaded = g_session.m_mapTypeInfo.Load();
    g_readFolderFirst = oldFolderFirst;
    std::filesystem::current_path(oldPath);
    std::filesystem::remove(tablePath);
    std::filesystem::remove(tablePath.parent_path());
    std::filesystem::remove(tableRoot);
    Require(loaded, "map type resource loader did not resolve the data path");
    Require(g_session.m_mapTypeInfo.Enabled(5, ro::SourceMapTypeTable::AllyParty)
        && g_session.m_mapTypeInfo.Enabled(7, ro::SourceMapTypeTable::AllyGuild)
        && !g_session.m_mapTypeInfo.Enabled(-1, ro::SourceMapTypeTable::Siege)
        && !g_session.m_mapTypeInfo.Enabled(30, ro::SourceMapTypeTable::Siege),
        "map property table did not retain the selected columns and bounds");
    const auto dispatch = [&](u16 type, u16 opcode = 0x01d6) {
        const std::array<u8, 4> bytes{static_cast<u8>(opcode), static_cast<u8>(opcode >> 8),
            static_cast<u8>(type), static_cast<u8>(type >> 8)};
        PacketView packet{};
        int consumed = 0;
        Require(!TryReadPacket(bytes.data(), 3, packet, consumed), "short map type packet framed");
        Require(TryReadPacket(bytes.data(), 4, packet, consumed) && consumed == 4 && router.Dispatch(mode, packet),
            "map type packet did not frame and dispatch");
    };
    dispatch(5);
    Require(g_session.m_curMapType == 5 && world.m_isPKZone && !world.m_isSiegeMode,
        "PK map type did not set the source world flag");
    dispatch(8);
    Require(world.m_isPKZone && world.m_isSiegeMode && g_session.m_isMinEffect,
        "siege map type did not retain prior flags and force minimal effects");
    dispatch(11);
    Require(world.m_isSiegeMode && !g_session.m_isMinEffect,
        "map type did not restore the stored effect preference");
    world.m_isSiegeMode = 0;
    dispatch(19);
    Require(world.m_isBattleFieldMode && world.m_isPKZone, "battlefield type did not update both world flags");
    dispatch(9);
    Require(world.m_isSiegeMode && g_session.m_isMinEffect, "map table siege property was not consumed");
    dispatch(0xffff);
    Require(g_session.m_curMapType == -1 && !g_session.m_isMinEffect, "unknown signed map type lost its safe state");

    CGameActor target;
    target.m_gid = 0x12345678;
    target.m_isPc = 1;
    target.m_gdid = player.m_gdid = 17;
    mode.m_skillUseInfo = {5, 1, 10, 2};
    world.m_isPKZone = world.m_isSiegeMode = world.m_isBattleFieldMode = 0;
    dispatch(7);
    Require(!ReplayTrySourceSkillOnActor(mode, &target, false, false, false) && !player.commands,
        "guild ally map allowed an ordinary hostile skill on the same guild");
    target.m_gdid = 18;
    Require(ReplayTrySourceSkillOnActor(mode, &target, false, false, false) && player.commands == 2,
        "guild ally map did not enable the hostile skill against another guild");
    FRIEND_INFO member{};
    member.AID = target.m_gid;
    g_session.m_partyList.push_back(member);
    player.commands = 0;
    dispatch(5);
    Require(!ReplayTrySourceSkillOnActor(mode, &target, false, false, false) && !player.commands,
        "party ally map enabled a hostile skill against its party");
    g_session.m_partyList.clear();
    Require(ReplayTrySourceSkillOnActor(mode, &target, false, false, false) && player.commands == 2,
        "party ally map did not enable a hostile skill against a nonmember");

    const auto oldService = g_serviceType;
    g_serviceType = ServiceKorea;
    dispatch(2, 0x0199);
    Require(world.m_isEventPVPMode && g_session.m_mapTypeInfo.Enabled(5, ro::SourceMapTypeTable::AllyParty),
        "event PvP property did not retain the Korean map table");
    g_serviceType = ServiceTaiwan;
    dispatch(3, 0x0199);
    Require(world.m_isSiegeMode && g_session.m_isMinEffect
        && !g_session.m_mapTypeInfo.Enabled(5, ro::SourceMapTypeTable::AllyParty),
        "siege property did not force minimum effects and clear the non-Korean map table");
    g_windowMgr.ClearChatEvents();
    dispatch(4, 0x0199);
    const auto& notices = g_windowMgr.GetChatEvents();
    Require(notices.size() == 1 && !notices.front().text.empty() && notices.front().color == 0xff9b9bu
        && !g_session.m_isMinEffect && g_session.m_curMapType == 5,
        "PK notice property did not retain the map type and restore the effect preference");
    g_windowMgr.ClearChatEvents();
    g_serviceType = oldService;

    const auto updateRow = [&](u16 row, const std::string& payload) {
        const auto length = static_cast<u16>(6 + payload.size());
        std::vector<u8> bytes{0xe7, 0x02, static_cast<u8>(length), static_cast<u8>(length >> 8),
            static_cast<u8>(row), static_cast<u8>(row >> 8)};
        bytes.insert(bytes.end(), payload.begin(), payload.end());
        bytes.push_back('1');
        PacketView packet{};
        int consumed = 0;
        Require(TryReadPacket(bytes.data(), static_cast<int>(bytes.size()), packet, consumed)
            && consumed == length && router.Dispatch(mode, packet), "map row update did not frame and dispatch");
    };
    updateRow(7, "0000000010");
    Require(g_session.m_mapTypeInfo.Enabled(7, ro::SourceMapTypeTable::AllyParty)
        && g_session.m_mapTypeInfo.Enabled(7, ro::SourceMapTypeTable::AllyGuild)
        && !g_session.m_mapTypeInfo.Enabled(7, ro::SourceMapTypeTable::Siege)
        && g_session.m_curMapType == 5, "map update lost decimal suffixes or read the next frame");
    updateRow(28, std::string(24, '0') + "1");
    Require(g_session.m_mapTypeInfo.Enabled(29, ro::SourceMapTypeTable::Siege),
        "map update did not advance across the source table stride");
    updateRow(29, std::string(16, '0'));
    updateRow(0xffff, "0");
    Require(g_session.m_mapTypeInfo.Enabled(29, ro::SourceMapTypeTable::Siege)
        && g_session.m_mapTypeInfo.Enabled(7, ro::SourceMapTypeTable::AllyGuild),
        "out-of-range map update changed existing properties");
    world.m_isSiegeMode = 0;
    dispatch(29);
    Require(world.m_isSiegeMode && g_session.m_isMinEffect,
        "map type dispatch did not use the row update");

    g_session.m_curMapType = oldType;
    g_session.m_mapTypeInfo = oldTable;
    g_session.m_isMinEffect = oldMinEffect;
    g_session.m_isMinEffectOld = oldMinEffectPreference;
    g_session.m_partyList = oldParty;
    g_session.m_weight = oldWeight;
    g_session.m_maxWeight = oldMaxWeight;
    g_session.m_isNoShift = oldNoShift;
    mode.m_world = nullptr;
}

void CompanionTargetReplay()
{
    CSession session;
    Require(!session.ApplyCompanionGroundClick(5, 8, true, false), "Alt ground click without a companion was consumed");
    session.m_merStatus.alive = true;
    session.m_merTarget = {17, 1, 2, false, false, true};
    Require(session.ApplyCompanionGroundClick(5, 8, true, false)
        && session.m_merTarget.moving && !session.m_merTarget.attacking
        && session.m_merTarget.gid == 0 && session.m_merTarget.cellX == 5 && session.m_merTarget.cellY == 8,
        "mercenary Alt ground click did not replace its target with a cell");
    session.m_homunStatus.alive = true;
    session.m_homunTarget = {18, 2, 3, false, true, true};
    Require(session.ApplyCompanionGroundClick(9, 11, true, true)
        && !session.m_homunTarget.selected && session.m_homunTarget.attacking && session.m_homunTarget.gid == 18,
        "Alt+Shift ground click did not clear an existing selection first");
    Require(session.ApplyCompanionGroundClick(9, 11, true, true)
        && session.m_homunTarget.gid == 18 && !session.m_homunTarget.moving,
        "reserved Alt+Shift move replaced the current target");
    Require(session.ApplyCompanionGroundClick(9, 11, true, false)
        && session.m_homunTarget.moving && session.m_homunTarget.cellX == 9 && session.m_homunTarget.cellY == 11
        && session.m_merTarget.moving && session.m_merTarget.cellX == 5 && session.m_merTarget.cellY == 8,
        "homunculus ground click did not take precedence over the mercenary");
    Require(!session.ApplyCompanionGroundClick(1, 1, false, false)
        && !session.m_homunTarget.moving && !session.m_merTarget.moving
        && !session.m_homunTarget.cellX && !session.m_merTarget.cellX,
        "ordinary ground click did not reset companion targets and continue");

    session.ApplyCompanionActorClick(23, false);
    Require(session.m_homunTarget.gid == 23 && session.m_merTarget.gid == 23
        && session.m_homunTarget.selected && session.m_merTarget.selected
        && session.m_homunTarget.attacking && session.m_merTarget.attacking,
        "Alt actor click did not mark both live companions");
    session.ApplyCompanionActorClick(23, false);
    Require(!session.m_homunTarget.attacking && !session.m_merTarget.attacking
        && session.m_homunTarget.selected && session.m_merTarget.selected,
        "same-actor click did not toggle attack while keeping selection");
    session.m_homunTarget.selected = false;
    session.ApplyCompanionActorClick(24, true);
    Require(session.m_homunTarget.selected && session.m_homunTarget.gid == 23
        && session.m_merTarget.gid == 23 && !session.m_merTarget.attacking,
        "reserved Alt+Shift actor click replaced the current target");
    session.m_merStatus.alive = false;
    session.ApplyCompanionActorClick(25, false);
    Require(session.m_homunTarget.gid == 25 && session.m_merTarget.gid == 23,
        "actor click selected a target for a dismissed mercenary");
    session.ClearDefeatedCompanionTarget(25);
    Require(!session.m_homunTarget.gid && !session.m_homunTarget.selected
        && !session.m_homunTarget.attacking && session.m_merTarget.gid == 23,
        "actor death cleared an unrelated companion target");
}

void CompanionSummonReplay()
{
    SetCase("companion_summon_requests", "PK28.03", "PK28-selected-companion-summon",
        "skill list or shortcut -> game-mode request -> selected map sender");
    const auto& profile = ro::net::GetActiveMapGameplaySendProfile();
    Require(profile.useSkillToId == 0x0438 && profile.useItem == 0x0439,
        "companion summon replay requires the selected map send profile");

    constexpr int callSkillId = 243;
    constexpr u16 scrollIndex = 64998;
    constexpr int shortcutIndex = 5;
    const auto oldSkills = g_session.GetSkillItems();
    const auto oldSlot = *g_session.GetShortcutSlotByAbsoluteIndex(shortcutIndex);
    const auto oldAid = g_session.m_aid;
    const auto homunculusGid = g_session.m_homunGID;
    const auto mercenaryGid = g_session.m_merGID;
    auto* oldMode = SkillPacketReplayAccess::Current();
    const auto oldModeType = SkillPacketReplayAccess::CurrentType();

    CGameMode mode;
    CPlayer player;
    CWorld world;
    world.m_player = &player;
    mode.m_world = &world;
    SkillPacketReplayAccess::SetCurrent(&mode);
    g_session.m_aid = 0x11223344;
    PLAYER_SKILL_INFO skill{};
    skill.m_isValid = 1;
    skill.SKID = callSkillId;
    skill.type = 4; // Selected self-target skill.
    skill.level = 1;
    skill.spcost = 10;
    g_session.SetSkillItem(skill);
    g_session.SetShortcutSlotByAbsoluteIndex(shortcutIndex, 1, callSkillId, 1);

    auto* connection = CRagConnection::instance();
    connection->Disconnect();
    LoopbackPeer peer(*connection);
    const std::vector<u8> call{0x38, 0x04, 1, 0, 0xf3, 0, 0x44, 0x33, 0x22, 0x11};
    Require(ArmPendingSkillUseFromSkillList(callSkillId) && peer.ReceiveExact(10) == call,
        "Call Homunculus from the skill list did not send a self-target skill request");
    Require(mode.SendMsg(CGameMode::GameMsg_RequestShortcutUse, shortcutIndex, 0, 0)
        && peer.ReceiveExact(10) == call, "Call Homunculus from a shortcut differed from skill-list use");

    Require(!g_session.GetInventoryItemByIndex(scrollIndex), "mercenary scroll fixture index was already occupied");
    ITEM_INFO scroll{};
    scroll.SetItemId(12153);
    scroll.m_itemIndex = scrollIndex;
    scroll.m_num = 1;
    g_session.SetInventoryItem(scroll);
    g_session.SetShortcutSlotByAbsoluteIndex(shortcutIndex, 0, 12153, 1);
    Require(mode.SendMsg(CGameMode::GameMsg_RequestShortcutUse, shortcutIndex, 0, 0)
        && peer.ReceiveExact(8) == std::vector<u8>{0x39, 0x04, 0xe6, 0xfd, 0x44, 0x33, 0x22, 0x11},
        "mercenary scroll did not send the selected inventory use request");
    Require(g_session.m_homunGID == homunculusGid && g_session.m_merGID == mercenaryGid,
        "companion request changed ownership before a server packet");

    connection->Disconnect();
    g_session.RemoveInventoryItem(scrollIndex, scroll.m_num);
    g_session.SetShortcutSlotByAbsoluteIndex(shortcutIndex, oldSlot.isSkill, oldSlot.id, oldSlot.count);
    g_session.ClearSkillItems();
    for (const auto& previous : oldSkills) g_session.SetSkillItem(previous);
    g_session.m_aid = oldAid;
    SkillPacketReplayAccess::Restore(oldMode, oldModeType);
    mode.m_world = nullptr;
}

void GuildMemberLoginNoticeReplay(){
    SetCase("guild_member_login_notice_01f2","PK06.GUILD-MEMBER-LOGIN","PK06-01f2-selected-source-literal","ProcessGameModePacketLoop -> CRagConnection::RecvPacket -> production router -> ReadWindowPacket");
    Require(ro::net::GetPacketSize(0x01f2)==20,"selected 0x01F2 receive size is not 20 bytes");
    Require(ro::net::GetPacketSize(0x016d)==14,"legacy 0x016D receive size changed");
    const auto previousState=g_session.m_guildWindow;
    g_session.m_guildWindow={};
    ro::guild::Member target;target.account=0x11223344;target.character=0x55667788;target.online=false;
    ro::guild::Member other;other.account=0x01020304;other.character=0x05060708;other.online=true;
    g_session.m_guildWindow.members={target,other};g_session.m_guildWindow.online=1;
    const std::vector<u8> online{0xf2,0x01,0x44,0x33,0x22,0x11,0x88,0x77,0x66,0x55,0x01,0x00,0x00,0x00,0x01,0x00,0x34,0x12,0x78,0x56};
    auto malformedState=g_session.m_guildWindow;
    auto shortFrame=online;shortFrame.pop_back();
    Require(!ro::guild::ReadWindowPacket(malformedState,0x01f2,shortFrame.data(),shortFrame.size()),"short 0x01F2 frame was accepted");
    auto invalidStatus=online;invalidStatus[10]=2;
    Require(!ro::guild::ReadWindowPacket(malformedState,0x01f2,invalidStatus.data(),invalidStatus.size()),"undocumented 0x01F2 status value was accepted");
    Require(!malformedState.members[0].online&&malformedState.members[0].gender==0&&malformedState.online==1,"rejected 0x01F2 input partially mutated member state");

    TestConnection connection;LoopbackPeer peer(connection);peer.Send(online);
    CGameMode mode;auto& router=GetProductionGameModePacketRouter();router.Clear();RegisterDefaultGameModePacketHandlers(router);
    const auto loginResult=ProcessGameModePacketLoop(mode,connection,router,4);
    Require(loginResult.packetsProcessed==1&&loginResult.packetsDispatched==1,"production router did not dispatch the 0x01F2 notice");
    const auto& onlineMember=g_session.m_guildWindow.members[0];
    Require(onlineMember.online&&onlineMember.gender==1&&onlineMember.hairStyle==0x1234&&onlineMember.hairColor==0x5678&&g_session.m_guildWindow.online==2,"0x01F2 online state or appearance fields decoded incorrectly");
    std::cout<<"{\"case_id\":\"guild_member_login_notice_01f2_online\",\"contract_id\":\"PK06.GUILD-MEMBER-LOGIN\",\"fixture_id\":\"PK06-01f2-selected-source-literal\",\"production_entry\":\"ProcessGameModePacketLoop -> CRagConnection::RecvPacket -> production router -> ReadWindowPacket\",\"input_frame_hex\":\""<<Hex(online)<<"\",\"packet_size\":20,\"member_online\":true,\"gender\":1,\"hair_style\":4660,\"hair_color\":22136,\"online_count\":2,\"outcome\":\"PASS\"}\n";

    const std::vector<u8> mapPosition{0xeb,0x01,0x44,0x33,0x22,0x11,0x05,0x00,0x06,0x00};
    peer.Send(mapPosition);const auto positionResult=ProcessGameModePacketLoop(mode,connection,router,4);
    Require(positionResult.packetsProcessed==1&&positionResult.packetsDispatched==1&&mode.m_guildPosList.count(0x11223344)==1,"production 0x01EB did not add the guild map-position marker");
    const std::vector<u8> offline{0xf2,0x01,0x44,0x33,0x22,0x11,0x88,0x77,0x66,0x55,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
    peer.Send(offline);const auto logoutResult=ProcessGameModePacketLoop(mode,connection,router,4);
    const auto& offlineMember=g_session.m_guildWindow.members[0];
    Require(logoutResult.packetsProcessed==1&&logoutResult.packetsDispatched==1,"production router did not dispatch the offline 0x01F2 notice");
    Require(!offlineMember.online&&offlineMember.gender==0&&offlineMember.hairStyle==0&&offlineMember.hairColor==0&&g_session.m_guildWindow.online==1&&mode.m_guildPosList.count(0x11223344)==0,"0x01F2 offline branch or guild map-position cleanup decoded incorrectly");
    Require(g_session.m_guildWindow.members.size()==2&&g_session.m_guildWindow.members[1].online,"0x01F2 update changed an unrelated member or inserted a phantom member");
    QVariantMap offlineRow;PopulateGuildMemberPresenceData(offlineRow,offlineMember);
    Require(!offlineRow.value("online").toBool()&&offlineRow.value("gender").toUInt()==0&&offlineRow.value("hairStyle").toUInt()==0&&offlineRow.value("hairColor").toUInt()==0,"offline member state did not propagate into the production Qt row model");
    std::cout<<"{\"case_id\":\"guild_member_login_notice_01f2_offline\",\"contract_id\":\"PK06.GUILD-MEMBER-LOGIN\",\"fixture_id\":\"PK06-01f2-selected-source-literal\",\"input_frame_hex\":\""<<Hex(offline)<<"\",\"member_online\":false,\"appearance_zeroed\":true,\"online_count\":1,\"other_member_unchanged\":true,\"map_position_removed\":true,\"qt_member_row_updated\":true,\"outcome\":\"PASS\"}\n";
    g_session.m_guildWindow=previousState;router.Clear();RegisterDefaultGameModePacketHandlers(router);
}

void PetStatusReplay(){
    SetCase("pet_status_state_decode","PK07.PET-STATUS","PK07-01a2-selected-20080910-fields","ProcessGameModePacketLoop -> CRagConnection::RecvPacket -> production router -> ro::pet::ReadStatus");
    Require(ro::net::GetActivePacketProfile().id==ro::net::PacketVersionId::PacketVer23,"pet status 35-byte fixture must run under selected profile 23");
    Require(ro::net::GetPacketSize(0x01a2)==35,"active pet status framing changed from selected 35-byte profile");
    const auto previousStatus=g_session.m_petStatus;
    g_session.m_petStatus={};
    const std::vector<u8> frame{0xa2,0x01,'P',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,10,0,20,0,30,0,0x34,0x12};
    auto rejected=g_session.m_petStatus;
    auto shortFrame=frame;shortFrame.pop_back();
    Require(!ro::pet::ReadStatus(rejected,shortFrame.data(),shortFrame.size()),"short pet status frame was accepted");
    auto wrongOpcode=frame;wrongOpcode[0]=0xa3;
    Require(!ro::pet::ReadStatus(rejected,wrongOpcode.data(),wrongOpcode.size()),"wrong pet status opcode was accepted");
    Require(!rejected.valid&&rejected.name.empty()&&rejected.level==0,"rejected pet status input partially mutated state");
    TestConnection connection;LoopbackPeer peer(connection);peer.Send(frame);
    CGameMode mode;auto& router=GetProductionGameModePacketRouter();router.Clear();RegisterDefaultGameModePacketHandlers(router);
    const auto result=ProcessGameModePacketLoop(mode,connection,router,4);
    const auto& status=g_session.m_petStatus;
    Require(result.packetsProcessed==1&&result.packetsDispatched==1,"production router did not dispatch pet status");
    Require(status.valid&&status.name=="P"&&status.modified==0&&status.level==10&&status.fullness==20&&status.relationship==30&&status.accessoryItemId==0x1234,"pet status fields were not decoded from selected 35-byte layout");
    auto preserved=status;
    Require(!ro::pet::ReadStatus(preserved,shortFrame.data(),shortFrame.size()),"short frame replaced an existing pet status snapshot");
    Require(!ro::pet::ReadStatus(preserved,wrongOpcode.data(),wrongOpcode.size()),"wrong opcode replaced an existing pet status snapshot");
    Require(preserved.valid&&preserved.name=="P"&&preserved.level==10&&preserved.accessoryItemId==0x1234,"rejected pet status input mutated an existing snapshot");
    auto signedFixture=frame;signedFixture[26]=0xff;signedFixture[27]=0x00;signedFixture[28]=0x80;signedFixture[29]=0xff;signedFixture[30]=0xff;
    ro::pet::Status signedStatus;Require(ro::pet::ReadStatus(signedStatus,signedFixture.data(),signedFixture.size()),"source-width signed pet fields were rejected");
    Require(signedStatus.modified==255&&signedStatus.level==-32768&&signedStatus.fullness==-1,"source pet flag/word extensions differ");
    std::cout<<"{\"case_id\":\"pet_status_state_decode\",\"contract_id\":\"PK07.PET-STATUS\",\"fixture_id\":\"PK07-01a2-selected-20080910-fields\",\"input_frame_hex\":\""<<Hex(frame)<<"\",\"packet_size\":35,\"name\":\""<<JsonEscape(status.name)<<"\",\"modified\":"<<static_cast<int>(status.modified)<<",\"level\":"<<status.level<<",\"fullness\":"<<status.fullness<<",\"relationship\":"<<status.relationship<<",\"accessory_item_id\":"<<status.accessoryItemId<<",\"malformed_size_and_opcode_atomic\":true,\"signed_wire_values\":[-1,-32768,-1],\"outcome\":\"PASS\"}\n";
    g_session.m_petStatus=previousStatus;router.Clear();RegisterDefaultGameModePacketHandlers(router);
}

void PartyRosterReplacementReplay()
{
    SetCase("party_roster_replacement_atomic", "production_party_replay", "PK14.05-0107-00fb-20080910",
        "ProcessGameModePacketLoop -> production router -> HandlePartyPosition/HandlePartyList -> CSession");

    const u32 savedAid = g_session.m_aid;
    const std::string savedMap(g_session.m_curMap);
    const std::string savedPartyName = g_session.m_partyName;
    const bool savedPartyMaster = g_session.m_amIPartyMaster;
    const bool savedExpShare = g_session.m_partyExpShare;
    const bool savedItemDiv = g_session.m_itemDivType;
    const bool savedItemCollect = g_session.m_itemCollectType;
    const std::vector<FRIEND_INFO> savedMembers(g_session.GetPartyList().begin(), g_session.GetPartyList().end());

    g_session.ClearParty();
    g_session.m_aid = 42;
    std::snprintf(g_session.m_curMap, sizeof(g_session.m_curMap), "%s", "prontera");
    g_session.m_partyName = "Old party";
    g_session.m_partyExpShare = true;
    g_session.m_itemDivType = true;
    g_session.m_itemCollectType = true;
    FRIEND_INFO self{};
    self.isValid = 1;
    self.AID = 1;
    self.characterName = "Self";
    self.mapName = "prontera.gat";
    self.role = 0;
    self.state = 0;
    g_session.AddMemberToParty(self);
    FRIEND_INFO peer{};
    peer.isValid = 1;
    peer.AID = 42;
    peer.characterName = "Peer";
    peer.mapName = "prontera.gat";
    peer.role = 1;
    peer.state = 0;
    peer.partyHp = 88;
    peer.partyMaxHp = 100;
    g_session.AddMemberToParty(peer);

    const std::vector<u8> position = Unhex("07012a00000032004600");
    const std::vector<u8> roster = Unhex(
        "fb007800"
        "546573740000000000000000000000000000000000000000"
        "2a000000506565720000000000000000000000000000000000000000"
        "70726f6e746572612e676174000000000000"
        "0100000053656c660000000000000000000000000000000000000000"
        "70726f6e746572612e676174000000000100");
    Require(position.size() == 10 && roster.size() == 120, "server-derived party fixture lengths differ from 20080910 layout");

    TestConnection connection;
    LoopbackPeer peerSocket(connection);
    CGameMode mode;
    CGameModePacketRouter& router = GetProductionGameModePacketRouter();
    router.Clear();
    RegisterDefaultGameModePacketHandlers(router);
    auto dispatch = [&](const std::vector<u8>& bytes, int expectedPackets) {
        peerSocket.Send(bytes);
        WaitKernelPending(connection, static_cast<int>(bytes.size()));
        const GameModePacketLoopResult result = ProcessGameModePacketLoop(mode, connection, router, 64);
        Require(result.packetsProcessed == expectedPackets, "production party replay processed an unexpected packet count");
    };

    std::vector<u8> ordered = position;
    ordered.insert(ordered.end(), roster.begin(), roster.end());
    dispatch(ordered, 2);
    const FRIEND_INFO* retained = g_session.FindPartyMemberByAid(42);
    Require(retained && retained->partyPositionValid && retained->partyX == 50 && retained->partyY == 70,
        "same-map online stationary member lost the production 0x0107 marker during 0x00FB replacement");
    Require(retained->partyHp == 88 && retained->partyMaxHp == 100, "roster replacement lost cached HP by AID");
    Require(g_session.IsPartyMarkerEligible(*retained, "prontera"), "shared minimap policy rejected a same-map online marker");
    Require(!g_session.IsPartyMarkerEligible(*retained, "payon"), "shared minimap policy exposed a wrong-map marker");
    Require(g_session.m_amIPartyMaster && g_session.m_partyName == "Test", "roster replacement lost local leader or party-name state");
    Require(g_session.m_partyExpShare && g_session.m_itemDivType && g_session.m_itemCollectType,
        "roster replacement cleared party options absent from 0x00FB");

    auto makeRoster = [](const std::vector<FRIEND_INFO>& entries) {
        std::vector<u8> packet(28 + entries.size() * 46, 0);
        packet[0] = 0xfb;
        packet[1] = 0x00;
        packet[2] = static_cast<u8>(packet.size() & 0xff);
        packet[3] = static_cast<u8>((packet.size() >> 8) & 0xff);
        const std::string name = "Next";
        std::copy(name.begin(), name.end(), packet.begin() + 4);
        for (size_t i = 0; i < entries.size(); ++i) {
            const FRIEND_INFO& entry = entries[i];
            const size_t offset = 28 + i * 46;
            for (int b = 0; b < 4; ++b) packet[offset + b] = static_cast<u8>((entry.AID >> (b * 8)) & 0xff);
            std::copy_n(entry.characterName.begin(), std::min<size_t>(entry.characterName.size(), 24), packet.begin() + offset + 4);
            std::copy_n(entry.mapName.begin(), std::min<size_t>(entry.mapName.size(), 16), packet.begin() + offset + 28);
            packet[offset + 44] = static_cast<u8>(entry.role);
            packet[offset + 45] = static_cast<u8>(entry.state);
        }
        return packet;
    };

    const std::string acceptedName = g_session.m_partyName;
    std::vector<u8> malformed = makeRoster({peer, self});
    malformed.push_back(0xee);
    malformed[2] = static_cast<u8>(malformed.size() & 0xff);
    malformed[3] = static_cast<u8>((malformed.size() >> 8) & 0xff);
    dispatch(malformed, 1);
    retained = g_session.FindPartyMemberByAid(42);
    Require(g_session.m_partyName == acceptedName && retained && retained->partyPositionValid
        && retained->partyX == 50 && retained->partyY == 70 && retained->partyHp == 88,
        "malformed trailing roster partially mutated the accepted party state");
    Require(g_session.m_amIPartyMaster && g_session.m_partyExpShare && g_session.m_itemDivType && g_session.m_itemCollectType,
        "malformed roster cleared leader or party options");

    std::vector<FRIEND_INFO> tooMany(13, peer);
    dispatch(makeRoster(tooMany), 1);
    Require(g_session.GetNumParty() == 2 && g_session.FindPartyMemberByAid(42)->partyPositionValid,
        "source-invalid 13-member roster mutated prior state");

    // The selected server also updates membership through fixed 0x01E9 records.
    // An offline member must lose its cached position before returning online.
    auto makeMemberUpdate = [](const FRIEND_INFO& entry) {
        std::vector<u8> packet(81, 0);
        packet[0] = 0xe9;
        packet[1] = 0x01;
        for (int b = 0; b < 4; ++b) {
            packet[2 + b] = static_cast<u8>((entry.AID >> (b * 8)) & 0xff);
            packet[6 + b] = static_cast<u8>((static_cast<u32>(entry.role) >> (b * 8)) & 0xff);
        }
        packet[14] = static_cast<u8>(entry.state);
        std::copy_n(entry.characterName.begin(), std::min<size_t>(entry.characterName.size(), 24), packet.begin() + 39);
        std::copy_n(entry.mapName.begin(), std::min<size_t>(entry.mapName.size(), 16), packet.begin() + 63);
        packet[79] = packet[80] = 1;
        return packet;
    };
    dispatch(makeMemberUpdate(peer), 1);
    retained = g_session.FindPartyMemberByAid(42);
    Require(retained && retained->partyPositionValid && retained->partyX == 50 && retained->partyY == 70,
        "incremental same-map online refresh lost stationary marker");
    FRIEND_INFO incrementalOffline = peer;
    incrementalOffline.state = 1;
    dispatch(makeMemberUpdate(incrementalOffline), 1);
    retained = g_session.FindPartyMemberByAid(42);
    Require(retained && !retained->partyPositionValid && retained->partyX == 0 && retained->partyY == 0,
        "incremental offline update retained stale marker coordinates");
    dispatch(makeMemberUpdate(peer), 1);
    retained = g_session.FindPartyMemberByAid(42);
    Require(retained && !retained->partyPositionValid,
        "incremental online update resurrected stale marker before fresh 0x0107");
    dispatch(position, 1);
    retained = g_session.FindPartyMemberByAid(42);
    Require(retained && retained->partyPositionValid && retained->partyX == 50 && retained->partyY == 70,
        "fresh position did not restore marker after incremental offline/online transition");

    FRIEND_INFO moved = peer;
    moved.mapName = "payon.gat";
    moved.role = 0;
    dispatch(makeRoster({moved, self}), 1);
    retained = g_session.FindPartyMemberByAid(42);
    Require(retained && !retained->partyPositionValid, "position survived a changed map in full roster replacement");

    std::snprintf(g_session.m_curMap, sizeof(g_session.m_curMap), "%s", "payon");
    dispatch(Unhex("07012a0000000a001400"), 1);
    dispatch(makeRoster({moved, self}), 1);
    retained = g_session.FindPartyMemberByAid(42);
    Require(retained && retained->partyPositionValid, "same-map online marker was not staged before offline transition check");
    FRIEND_INFO offline = moved;
    offline.state = 1;
    dispatch(makeRoster({offline, self}), 1);
    retained = g_session.FindPartyMemberByAid(42);
    Require(retained && !retained->partyPositionValid && !g_session.IsPartyMarkerEligible(*retained, "payon"),
        "position survived an offline roster state");

    dispatch(makeRoster({moved, self}), 1);
    dispatch(Unhex("07012a0000000b001500"), 1);
    FRIEND_INFO reusedName = moved;
    reusedName.AID = 43;
    reusedName.state = 0;
    dispatch(makeRoster({reusedName, self}), 1);
    retained = g_session.FindPartyMemberByAid(43);
    Require(retained && !retained->partyPositionValid && g_session.FindPartyMemberByAid(42) == nullptr,
        "reused character name retained prior AID position or stale member");

    dispatch(makeRoster({self}), 1);
    Require(g_session.FindPartyMemberByAid(43) == nullptr && g_session.GetNumParty() == 1,
        "full roster replacement retained a removed member");

    reusedName.role = 1;
    dispatch(makeRoster({reusedName, self}), 1);
    dispatch(Unhex("07012b0000000c001600"), 1);
    dispatch(Unhex("07012b000000ffffffff"), 1);
    retained = g_session.FindPartyMemberByAid(43);
    Require(retained && !retained->partyPositionValid, "0x0107 sentinel failed to remove marker after roster replacement");

    g_session.ClearParty();
    g_session.m_aid = savedAid;
    std::snprintf(g_session.m_curMap, sizeof(g_session.m_curMap), "%s", savedMap.c_str());
    for (FRIEND_INFO member : savedMembers) {
        const bool hadPosition = member.partyPositionValid;
        const int x = member.partyX;
        const int y = member.partyY;
        const u32 aid = member.AID;
        g_session.AddMemberToParty(member);
        if (hadPosition) g_session.SetPartyMemberPosition(aid, x, y);
    }
    g_session.m_partyName = savedPartyName;
    g_session.m_amIPartyMaster = savedPartyMaster;
    g_session.m_partyExpShare = savedExpShare;
    g_session.m_itemDivType = savedItemDiv;
    g_session.m_itemCollectType = savedItemCollect;

    std::cout << "{\"case_id\":\"party_roster_replacement_atomic\",\"contract_id\":\"PK14.05\",\"fixture_id\":\"PK14.05-0107-00fb-20080910\",\"outcome\":\"PASS\",\"assertions\":\"position, HP, leader, options, malformed atomicity, count bound, incremental offline/online, map, offline, AID identity, removal, sentinel\"}\n";
}

void PartyMarkerGameModeExitReplay()
{
    SetCase("party_marker_game_mode_exit", "production_lifecycle_replay", "PK14.06-session-marker-exit",
        "CGameMode::OnExit -> CSession::InvalidatePartyPositions -> UIRoMapWnd visual token");
    const u32 savedAid = g_session.m_aid;
    const std::string savedMap(g_session.m_curMap);
    const std::string savedPartyName = g_session.m_partyName;
    const bool savedPartyMaster = g_session.m_amIPartyMaster;
    const bool savedExpShare = g_session.m_partyExpShare;
    const bool savedItemDiv = g_session.m_itemDivType;
    const bool savedItemCollect = g_session.m_itemCollectType;
    const std::vector<FRIEND_INFO> savedMembers(g_session.GetPartyList().begin(), g_session.GetPartyList().end());

    g_session.ClearParty();
    g_session.m_aid = 42;
    std::snprintf(g_session.m_curMap, sizeof(g_session.m_curMap), "%s", "prontera");
    FRIEND_INFO member{};
    member.isValid = 1;
    member.AID = 42;
    member.characterName = "Peer";
    member.mapName = "prontera.gat";
    member.state = 0;
    g_session.AddMemberToParty(member);
    Require(g_session.SetPartyMemberPosition(42, 25, 35), "could not seed marker before production mode exit");
    const u64 revisionBefore = g_session.GetPartyPositionRevision();
    UIRoMapWnd minimap;
    UIRoMapWnd::DisplayData displayBefore{};
    Require(minimap.GetDisplayDataForQt(&displayBefore), "could not capture pre-exit Qt minimap state");

    CGameMode mode;
    mode.OnExit();
    const FRIEND_INFO* retained = g_session.FindPartyMemberByAid(42);
    UIRoMapWnd::DisplayData displayAfter{};
    Require(minimap.GetDisplayDataForQt(&displayAfter), "could not capture post-exit Qt minimap state");
    Require(retained && !retained->partyPositionValid && g_session.GetNumParty() == 1,
        "production game-mode exit did not invalidate coordinates while retaining party membership");
    Require(!g_session.IsPartyMarkerEligible(*retained, "prontera"), "invalidated marker remains eligible for native/Qt consumers");
    Require(g_session.GetPartyPositionRevision() > revisionBefore && displayAfter.imageRevision != displayBefore.imageRevision,
        "party marker revision or minimap visual token did not invalidate on mode exit");

    g_session.ClearParty();
    g_session.m_aid = savedAid;
    std::snprintf(g_session.m_curMap, sizeof(g_session.m_curMap), "%s", savedMap.c_str());
    for (FRIEND_INFO saved : savedMembers) {
        const bool hadPosition = saved.partyPositionValid;
        const int x = saved.partyX;
        const int y = saved.partyY;
        const u32 aid = saved.AID;
        g_session.AddMemberToParty(saved);
        if (hadPosition) g_session.SetPartyMemberPosition(aid, x, y);
    }
    g_session.m_partyName = savedPartyName;
    g_session.m_amIPartyMaster = savedPartyMaster;
    g_session.m_partyExpShare = savedExpShare;
    g_session.m_itemDivType = savedItemDiv;
    g_session.m_itemCollectType = savedItemCollect;

    std::cout << "{\"case_id\":\"party_marker_game_mode_exit\",\"contract_id\":\"PK14.06\",\"fixture_id\":\"PK14.06-session-marker-exit\",\"outcome\":\"PASS\",\"assertions\":\"production OnExit, membership retained, shared eligibility false, position revision and minimap token changed\"}\n";
}

bool ReplaySendMapMoveRequestPacket(int dstX, int dstY);
bool ReplayRequestUseInventoryItem(u16 itemIndex);
bool ReplaySendUseSkillToIdPacket(u16 skillId, u16 skillLevel, u32 targetGid);
bool ReplaySendTakeItemRequestPacket(u32 objectAid);

void MapMovementProductionSendReplay()
{
    SetCase("map_movement_production_send", "PK46.02", "PK01.02-final-00a7-walktoxy",
        "TryRequestMoveToCell -> SendMoveRequestPacket -> CRagConnection::SendPacket");
    const ro::net::MapGameplaySendProfile& profile = ro::net::GetActiveMapGameplaySendProfile();
    Require(profile.id == ro::net::MapGameplaySendProfileId::PacketVer23 && profile.walkToXY == 0x00A7,
        "replay fixture requires the default PacketVer23 movement profile");
    CRagConnection* const connection = CRagConnection::instance();
    connection->Disconnect();
    Require(!ReplaySendMapMoveRequestPacket(341, 683), "movement sender reported success without an open connection");
    LoopbackPeer peer(*connection);
    Require(ReplaySendMapMoveRequestPacket(341, 683), "production movement sender rejected an open loopback connection");
    const std::vector<u8> captured = peer.ReceiveExact(8);
    Require(captured.size() == 8 && captured[0] == 0xA7 && captured[1] == 0x00,
        "production movement sender did not emit the selected server's 0x00A7 eight-byte frame");
    Require(captured[5] == 0x55 && captured[6] == 0x6A && captured[7] == 0xB0,
        "production movement destination differs from the selected server's byte-5 coordinate field");
    connection->Disconnect();
    std::cout << "{\"case_id\":\"map_movement_production_send\",\"contract_id\":\"PK46.02\",\"fixture_id\":\"PK01.02-final-00a7-walktoxy\",\"captured_packet_hex\":\"" << Hex(captured) << "\",\"length\":8,\"destination_offset\":5,\"closed_connection_rejected\":true,\"outcome\":\"PASS\"}\n";
}

void UseItemProductionSendReplay()
{
    SetCase("use_item_production_send", "PK46.02", "PK46.02-0439-packetver23-use-item",
        "RequestUseInventoryItem -> BuildActiveUseItemPacket -> CRagConnection::SendPacket");
    const ro::net::MapGameplaySendProfile& profile = ro::net::GetActiveMapGameplaySendProfile();
    Require(profile.id == ro::net::MapGameplaySendProfileId::PacketVer23 && profile.useItem == 0x0439,
        "replay fixture requires the PacketVer23 0x0439 item-use profile");

    constexpr u16 itemIndex = 64999;
    constexpr u32 targetAid = 0x11223344;
    Require(g_session.GetInventoryItemByIndex(itemIndex) == nullptr,
        "item-use replay fixture index is already present");
    ITEM_INFO item{};
    item.m_itemIndex = itemIndex;
    item.m_num = 1;
    g_session.SetInventoryItem(item);
    const u32 savedAid = g_session.m_aid;
    g_session.m_aid = targetAid;

    CGameMode* const currentMode = g_modeMgr.GetCurrentGameMode();
    const int savedWaitingAck = currentMode ? currentMode->m_waitingUseItemAck : 0;
    const int savedLastCardIndex = currentMode ? currentMode->m_lastCardItemIndex : 0;
    CRagConnection* const connection = CRagConnection::instance();
    connection->Disconnect();
    Require(!ReplayRequestUseInventoryItem(itemIndex),
        "item-use sender reported success without an open connection");
    {
        LoopbackPeer peer(*connection);
        Require(ReplayRequestUseInventoryItem(itemIndex),
            "production item-use sender rejected an open loopback connection");
        const std::vector<u8> expected{0x39, 0x04, 0xE7, 0xFD, 0x44, 0x33, 0x22, 0x11};
        const std::vector<u8> captured = peer.ReceiveExact(expected.size());
        Require(captured == expected,
            "production item-use sender did not emit the selected eight-byte 0x0439 frame");
        std::cout << "{\"case_id\":\"use_item_production_send\",\"contract_id\":\"PK46.02\",\"fixture_id\":\"PK46.02-0439-packetver23-use-item\",\"captured_packet_hex\":\"" << Hex(captured) << "\",\"outcome\":\"PASS\"}\n";
    }
    connection->Disconnect();
    g_session.m_aid = savedAid;
    if (currentMode) {
        currentMode->m_waitingUseItemAck = savedWaitingAck;
        currentMode->m_lastCardItemIndex = savedLastCardIndex;
    }
    g_session.RemoveInventoryItem(itemIndex, item.m_num);
}

void SkillTargetProductionSendReplay()
{
    SetCase("skill_target_production_send", "PK46.02", "PK46.02-0438-packetver23-skill-target",
        "SendUseSkillToIdPacket -> BuildActiveUseSkillToIdPacket -> CRagConnection::SendPacket");
    const ro::net::MapGameplaySendProfile& profile = ro::net::GetActiveMapGameplaySendProfile();
    Require(profile.id == ro::net::MapGameplaySendProfileId::PacketVer23 && profile.useSkillToId == 0x0438,
        "replay fixture requires the PacketVer23 0x0438 targeted-skill profile");

    CRagConnection* const connection = CRagConnection::instance();
    connection->Disconnect();
    Require(!ReplaySendUseSkillToIdPacket(0x1234, 7, 0x55667788),
        "targeted-skill sender reported success without an open connection");
    {
        LoopbackPeer peer(*connection);
        Require(ReplaySendUseSkillToIdPacket(0x1234, 7, 0x55667788),
            "production targeted-skill sender rejected an open loopback connection");
        const std::vector<u8> expected{0x38, 0x04, 0x07, 0x00, 0x34, 0x12, 0x88, 0x77, 0x66, 0x55};
        const std::vector<u8> captured = peer.ReceiveExact(expected.size());
        Require(captured == expected,
            "production targeted-skill sender did not emit the selected ten-byte 0x0438 frame");
        std::cout << "{\"case_id\":\"skill_target_production_send\",\"contract_id\":\"PK46.02\",\"fixture_id\":\"PK46.02-0438-packetver23-skill-target\",\"captured_packet_hex\":\"" << Hex(captured) << "\",\"outcome\":\"PASS\"}\n";
    }
    connection->Disconnect();
}

void TakeItemProductionSendReplay()
{
    SetCase("take_item_production_send", "PK46.02", "PK46.02-00f5-packetver23-take-item",
        "SendTakeItemRequestPacket -> BuildActiveTakeItemPacket -> CRagConnection::SendPacket");
    const ro::net::MapGameplaySendProfile& profile = ro::net::GetActiveMapGameplaySendProfile();
    Require(profile.id == ro::net::MapGameplaySendProfileId::PacketVer23 && profile.takeItem == 0x00F5,
        "replay fixture requires the PacketVer23 0x00F5 take-item profile");

    CRagConnection* const connection = CRagConnection::instance();
    connection->Disconnect();
    Require(!ReplaySendTakeItemRequestPacket(0), "take-item sender accepted object ID zero");
    Require(!ReplaySendTakeItemRequestPacket(0x11223344),
        "take-item sender reported success without an open connection");
    {
        LoopbackPeer peer(*connection);
        Require(ReplaySendTakeItemRequestPacket(0x11223344),
            "production take-item sender rejected an open loopback connection");
        const std::vector<u8> expected{0xF5, 0x00, 0x00, 0x00, 0x44, 0x33, 0x22, 0x11};
        const std::vector<u8> captured = peer.ReceiveExact(expected.size());
        Require(captured == expected,
            "production take-item sender did not emit the selected eight-byte 0x00F5 frame");
        std::cout << "{\"case_id\":\"take_item_production_send\",\"contract_id\":\"PK46.02\",\"fixture_id\":\"PK46.02-00f5-packetver23-take-item\",\"captured_packet_hex\":\"" << Hex(captured) << "\",\"outcome\":\"PASS\"}\n";
    }
    connection->Disconnect();
}

bool ReplaySendChangeDirectionPacket(u8 headDir, u8 dir);

void ChangeDirectionProductionSendReplay()
{
    SetCase("change_direction_production_send", "PK46.02", "PK01.02-final-0085-changedir",
        "TryRequestChangeDirFromScreenPoint -> SendChangeDirRequestPacket -> CRagConnection::SendPacket");
    const ro::net::MapGameplaySendProfile& profile = ro::net::GetActiveMapGameplaySendProfile();
    Require(profile.id == ro::net::MapGameplaySendProfileId::PacketVer23 && profile.changeDir == 0x0085,
        "replay fixture requires the default PacketVer23 change-direction profile");
    CRagConnection* const connection = CRagConnection::instance();
    connection->Disconnect();
    Require(!ReplaySendChangeDirectionPacket(0x23, 6), "change-direction sender reported success without an open connection");
    LoopbackPeer peer(*connection);
    Require(ReplaySendChangeDirectionPacket(0x23, 6), "production change-direction sender rejected an open loopback connection");
    const std::vector<u8> captured = peer.ReceiveExact(11);
    Require(captured.size() == 11 && captured[0] == 0x85 && captured[1] == 0x00,
        "production change-direction sender did not emit the selected server's 0x0085 eleven-byte frame");
    Require(captured[7] == 0x23 && captured[8] == 0x00 && captured[10] == 0x06,
        "production direction fields differ from the selected server's byte-7/byte-10 positions");
    connection->Disconnect();
    std::cout << "{\"case_id\":\"change_direction_production_send\",\"contract_id\":\"PK46.02\",\"fixture_id\":\"PK01.02-final-0085-changedir\",\"captured_packet_hex\":\"" << Hex(captured) << "\",\"length\":11,\"head_dir_offset\":7,\"dir_offset\":10,\"closed_connection_rejected\":true,\"outcome\":\"PASS\"}\n";
}

bool ReplayRequestCartOffPacket();

void CartOffProductionSendReplay()
{
    SetCase("cart_off_production_send", "PK46.02", "PK01.02-final-012a-removeoption",
        "UIEquipWnd::SendMsg(kButtonIdRemoveOption) -> CGameMode::OnMessage -> RequestCartOff -> BuildActiveCartOffPacket -> CRagConnection::SendPacket");
    const ro::net::MapGameplaySendProfile& profile = ro::net::GetActiveMapGameplaySendProfile();
    Require(profile.id == ro::net::MapGameplaySendProfileId::PacketVer23 && profile.cartOff == 0x012A,
        "replay fixture requires the default PacketVer23 cart-off profile");
    CRagConnection* const connection = CRagConnection::instance();
    connection->Disconnect();
    Require(!ReplayRequestCartOffPacket(), "cart-off sender reported success without an open connection");
    LoopbackPeer peer(*connection);
    Require(ReplayRequestCartOffPacket(), "production cart-off sender rejected an open loopback connection");
    const std::vector<u8> captured = peer.ReceiveExact(2);
    Require(captured.size() == 2 && captured[0] == 0x2A && captured[1] == 0x01,
        "production cart-off sender did not emit the selected server's 0x012A two-byte frame");
    connection->Disconnect();
    std::cout << "{\"case_id\":\"cart_off_production_send\",\"contract_id\":\"PK46.02\",\"fixture_id\":\"PK01.02-final-012a-removeoption\",\"captured_packet_hex\":\"" << Hex(captured) << "\",\"outcome\":\"PASS\"}" << std::endl;
}

bool ReplayRequestNpcCloseDialog(u32 npcId);

void NpcCloseDialogProductionSendReplay()
{
    SetCase("npc_close_dialog_production_send", "PK46.02", "PK01.02-final-0146-npc-close-clicked",
        "UISayDialogWnd/UINpcInputWnd -> GameMsg_RequestNpcCloseDialog -> RequestNpcCloseDialog -> CRagConnection::SendPacket");
    CRagConnection* const connection = CRagConnection::instance();
    connection->Disconnect();
    Require(!ReplayRequestNpcCloseDialog(0x11223344), "NPC close sender reported success without an open connection");
    LoopbackPeer peer(*connection);
    Require(!ReplayRequestNpcCloseDialog(0), "NPC close sender accepted a zero NPC ID");
    Require(SelectSocket(peer.peer.value, true, false, std::chrono::milliseconds(50)) == 0,
        "zero-ID NPC close unexpectedly emitted bytes");
    Require(ReplayRequestNpcCloseDialog(0x11223344), "production NPC close sender rejected an open loopback connection");
    const std::vector<u8> captured = peer.ReceiveExact(6);
    Require(captured.size() == 6 && captured[0] == 0x46 && captured[1] == 0x01
        && captured[2] == 0x44 && captured[3] == 0x33 && captured[4] == 0x22 && captured[5] == 0x11,
        "production NPC close sender bytes differ from the selected six-byte server frame");
    connection->Disconnect();
    std::cout << "{\"case_id\":\"npc_close_dialog_production_send\",\"contract_id\":\"PK46.02\",\"fixture_id\":\"PK01.02-final-0146-npc-close-clicked\",\"captured_packet_hex\":\"" << Hex(captured) << "\",\"zero_id_rejected\":true,\"outcome\":\"PASS\"}" << std::endl;
}

bool ReplayRequestIncreaseStatus(u16 statusId);

void StatusIncreaseProductionSendReplay()
{
    SetCase("status_increase_production_send", "PK46.02", "PK01.02-final-00bb-statusup",
        "UIStatusWnd -> GameMsg_RequestIncreaseStatus -> RequestIncreaseStatus -> CRagConnection::SendPacket");
    CRagConnection* const connection = CRagConnection::instance();
    connection->Disconnect();
    Require(!ReplayRequestIncreaseStatus(13), "status sender reported success without an open connection");
    LoopbackPeer peer(*connection);
    Require(!ReplayRequestIncreaseStatus(12) && !ReplayRequestIncreaseStatus(19),
        "status sender accepted an ID outside the server enum range");
    Require(SelectSocket(peer.peer.value, true, false, std::chrono::milliseconds(50)) == 0,
        "invalid status ID unexpectedly emitted bytes");
    Require(ReplayRequestIncreaseStatus(13) && ReplayRequestIncreaseStatus(18),
        "production status sender rejected an open loopback connection");
    const std::vector<u8> captured = peer.ReceiveExact(10);
    const std::vector<u8> expected{0xBB,0x00,0x0D,0x00,0x01, 0xBB,0x00,0x12,0x00,0x01};
    Require(captured == expected,
        "production status sender bytes differ from the selected five-byte server frames");
    connection->Disconnect();
    std::cout << "{\"case_id\":\"status_increase_production_send\",\"contract_id\":\"PK46.02\",\"fixture_id\":\"PK01.02-final-00bb-statusup\",\"captured_packet_hex\":\"" << Hex(captured) << "\",\"invalid_ids_rejected\":true,\"outcome\":\"PASS\"}" << std::endl;
}

bool ReplayRequestNpcNext(u32 npcId);

void NpcNextProductionSendReplay()
{
    SetCase("npc_next_production_send", "PK46.02", "PK01.02-final-00b9-npc-next-clicked",
        "UISayDialogWnd -> GameMsg_RequestNpcNext -> RequestNpcNext -> CRagConnection::SendPacket");
    CRagConnection* const connection = CRagConnection::instance();
    connection->Disconnect();
    Require(!ReplayRequestNpcNext(0x11223344), "NPC next sender reported success without an open connection");
    LoopbackPeer peer(*connection);
    Require(!ReplayRequestNpcNext(0), "NPC next sender accepted a zero NPC ID");
    Require(SelectSocket(peer.peer.value, true, false, std::chrono::milliseconds(50)) == 0,
        "zero-ID NPC next unexpectedly emitted bytes");
    Require(ReplayRequestNpcNext(0x11223344), "production NPC next sender rejected an open loopback connection");
    const std::vector<u8> captured = peer.ReceiveExact(6);
    Require(captured.size() == 6 && captured[0] == 0xB9 && captured[1] == 0x00
        && captured[2] == 0x44 && captured[3] == 0x33 && captured[4] == 0x22 && captured[5] == 0x11,
        "production NPC next sender bytes differ from the selected six-byte server frame");
    connection->Disconnect();
    std::cout << "{\"case_id\":\"npc_next_production_send\",\"contract_id\":\"PK46.02\",\"fixture_id\":\"PK01.02-final-00b9-npc-next-clicked\",\"captured_packet_hex\":\"" << Hex(captured) << "\",\"zero_id_rejected\":true,\"outcome\":\"PASS\"}" << std::endl;
}

bool ReplayRequestNpcInputNumber(u32 npcId, u32 value);

void NpcInputNumberProductionSendReplay()
{
    SetCase("npc_input_number_production_send", "PK46.02", "PK01.02-final-0143-npc-amount-input",
        "UINpcInputWnd -> GameMsg_RequestNpcInputNumber -> RequestNpcInputNumber -> CRagConnection::SendPacket");
    CRagConnection* const connection = CRagConnection::instance();
    connection->Disconnect();
    Require(!ReplayRequestNpcInputNumber(0x11223344, 12345),
        "NPC number sender reported success without an open connection");
    LoopbackPeer peer(*connection);
    Require(!ReplayRequestNpcInputNumber(0, 12345), "NPC number sender accepted a zero NPC ID");
    Require(SelectSocket(peer.peer.value, true, false, std::chrono::milliseconds(50)) == 0,
        "zero-ID NPC number request unexpectedly emitted bytes");
    Require(ReplayRequestNpcInputNumber(0x11223344, 0x7FFFFFFF)
        && ReplayRequestNpcInputNumber(0x11223344, 0x80000000),
        "production NPC number sender rejected an open loopback connection");
    const std::vector<u8> captured = peer.ReceiveExact(20);
    const std::vector<u8> expected{
        0x43,0x01,0x44,0x33,0x22,0x11,0xFF,0xFF,0xFF,0x7F,
        0x43,0x01,0x44,0x33,0x22,0x11,0x00,0x00,0x00,0x80};
    Require(captured == expected,
        "production NPC number sender bytes differ from the independent signed-boundary fixtures");
    connection->Disconnect();
    std::cout << "{\"case_id\":\"npc_input_number_production_send\",\"contract_id\":\"PK46.02\",\"fixture_id\":\"PK01.02-final-0143-npc-amount-input\",\"captured_packet_hex\":\"" << Hex(captured) << "\",\"zero_id_rejected\":true,\"outcome\":\"PASS\"}" << std::endl;
}

bool ReplayRequestNpcInputString(u32 npcId, const char* text);

void NpcInputStringProductionSendReplay()
{
    SetCase("npc_input_string_production_send", "PK46.02", "PK01.02-final-01d5-npc-string-input",
        "UINpcInputWnd -> GameMsg_RequestNpcInputString -> RequestNpcInputString -> CRagConnection::SendPacket");
    CRagConnection* const connection = CRagConnection::instance();
    connection->Disconnect();
    Require(!ReplayRequestNpcInputString(0x11223344, "Hi"),
        "NPC string sender reported success without an open connection");
    LoopbackPeer peer(*connection);
    const std::string oversized(65527, 'x');
    Require(!ReplayRequestNpcInputString(0, "Hi")
        && !ReplayRequestNpcInputString(0x11223344, nullptr)
        && !ReplayRequestNpcInputString(0x11223344, "")
        && !ReplayRequestNpcInputString(0x11223344, oversized.c_str()),
        "NPC string sender accepted an invalid ID/text or a packet length above u16");
    Require(SelectSocket(peer.peer.value, true, false, std::chrono::milliseconds(50)) == 0,
        "rejected NPC string input unexpectedly emitted bytes");
    Require(ReplayRequestNpcInputString(0x11223344, "Hi"),
        "production NPC string sender rejected an open loopback connection");
    const std::vector<u8> captured = peer.ReceiveExact(11);
    const std::vector<u8> expected{0xD5,0x01,0x0B,0x00,0x44,0x33,0x22,0x11,0x48,0x69,0x00};
    Require(captured == expected,
        "production NPC string sender bytes differ from the independent variable-frame fixture");

    const int savedCodePage = g_codePage;
    g_codePage = 950;
    std::string boundary;
    for (int i = 0; i < 35; ++i) {
        boundary += "\xe4\xb8\xad"; // U+4E2D, encoded A4 A4 in CP950.
    }
    const bool rejected = !ReplayRequestNpcInputString(0x11223344, (boundary + "x").c_str());
    const bool accepted = ReplayRequestNpcInputString(0x11223344, boundary.c_str());
    g_codePage = savedCodePage;
    Require(rejected && accepted, "NPC string limit did not count encoded CP950 bytes");
    auto encodedExpected = std::vector<u8>{0xd5, 0x01, 79, 0, 0x44, 0x33, 0x22, 0x11};
    encodedExpected.insert(encodedExpected.end(), 70, 0xa4);
    encodedExpected.push_back(0);
    Require(peer.ReceiveExact(79) == encodedExpected,
        "NPC input did not send the exact encoded 70-byte boundary and terminator");
    connection->Disconnect();
    std::cout << "{\"case_id\":\"npc_input_string_production_send\",\"contract_id\":\"PK46.02\",\"fixture_id\":\"PK01.02-final-01d5-npc-string-input\",\"captured_packet_hex\":\"" << Hex(captured) << "\",\"invalid_and_oversize_rejected\":true,\"outcome\":\"PASS\"}" << std::endl;
}

bool ReplayRequestNpcContact(u32 npcId);

void NpcContactProductionSendReplay()
{
    SetCase("npc_contact_production_send", "PK46.02", "PK01.02-final-0090-contactnpc",
        "TryRequestNpcTalkFromScreenPoint -> RequestNpcContact -> CRagConnection::SendPacket");
    CRagConnection* const connection = CRagConnection::instance();
    connection->Disconnect();
    Require(!ReplayRequestNpcContact(0x11223344),
        "NPC contact sender reported success without an open connection");
    LoopbackPeer peer(*connection);
    Require(!ReplayRequestNpcContact(0), "NPC contact sender accepted a zero NPC ID");
    Require(SelectSocket(peer.peer.value, true, false, std::chrono::milliseconds(50)) == 0,
        "zero-ID NPC contact unexpectedly emitted bytes");
    Require(ReplayRequestNpcContact(0x11223344),
        "production NPC contact sender rejected an open loopback connection");
    const std::vector<u8> captured = peer.ReceiveExact(7);
    const std::vector<u8> expected{0x90,0x00,0x44,0x33,0x22,0x11,0x01};
    Require(captured == expected,
        "production NPC contact sender bytes differ from the selected seven-byte server frame");
    connection->Disconnect();
    std::cout << "{\"case_id\":\"npc_contact_production_send\",\"contract_id\":\"PK46.02\",\"fixture_id\":\"PK01.02-final-0090-contactnpc\",\"captured_packet_hex\":\"" << Hex(captured) << "\",\"zero_id_rejected\":true,\"outcome\":\"PASS\"}" << std::endl;
}

bool ReplayRequestNpcMenuSelection(u32 npcId, u8 choice);

void NpcMenuSelectionProductionSendReplay()
{
    SetCase("npc_menu_selection_production_send", "PK46.02", "PK01.02-final-00b8-npc-selectmenu",
        "UINpcMenuWnd -> GameMsg_RequestNpcSelectMenu -> RequestNpcMenuSelection -> CRagConnection::SendPacket");
    CRagConnection* const connection = CRagConnection::instance();
    connection->Disconnect();
    Require(!ReplayRequestNpcMenuSelection(0x11223344, 3),
        "NPC menu sender reported success without an open connection");
    LoopbackPeer peer(*connection);
    Require(!ReplayRequestNpcMenuSelection(0, 3) && !ReplayRequestNpcMenuSelection(0x11223344, 0),
        "NPC menu sender accepted a zero NPC ID or zero choice");
    Require(SelectSocket(peer.peer.value, true, false, std::chrono::milliseconds(50)) == 0,
        "rejected NPC menu selection unexpectedly emitted bytes");
    Require(ReplayRequestNpcMenuSelection(0x11223344, 3),
        "production NPC menu sender rejected an open loopback connection");
    const std::vector<u8> captured = peer.ReceiveExact(7);
    const std::vector<u8> expected{0xB8,0x00,0x44,0x33,0x22,0x11,0x03};
    Require(captured == expected,
        "production NPC menu sender bytes differ from the selected seven-byte server frame");
    connection->Disconnect();
    std::cout << "{\"case_id\":\"npc_menu_selection_production_send\",\"contract_id\":\"PK46.02\",\"fixture_id\":\"PK01.02-final-00b8-npc-selectmenu\",\"captured_packet_hex\":\"" << Hex(captured) << "\",\"invalid_input_rejected\":true,\"outcome\":\"PASS\"}" << std::endl;
}

bool ReplayRequestReturnToSavePoint();

void ReturnToSavePointProductionSendReplay()
{
    SetCase("return_save_point_production_send", "PK46.02", "PK01.02-final-00b2-restart-savepoint",
        "UIChooseWnd -> GameMsg_RequestReturnToSavePoint -> RequestReturnToSavePoint -> CRagConnection::SendPacket");
    CRagConnection* const connection = CRagConnection::instance();
    connection->Disconnect();
    Require(!ReplayRequestReturnToSavePoint(),
        "return-to-save-point sender reported success without an open connection");
    LoopbackPeer peer(*connection);
    Require(ReplayRequestReturnToSavePoint(),
        "production return-to-save-point sender rejected an open loopback connection");
    const std::vector<u8> captured = peer.ReceiveExact(3);
    const std::vector<u8> expected{0xB2,0x00,0x00};
    Require(captured == expected,
        "production return-to-save-point sender bytes differ from the selected three-byte server frame");
    connection->Disconnect();
    std::cout << "{\"case_id\":\"return_save_point_production_send\",\"contract_id\":\"PK46.02\",\"fixture_id\":\"PK01.02-final-00b2-restart-savepoint\",\"captured_packet_hex\":\"" << Hex(captured) << "\",\"outcome\":\"PASS\"}" << std::endl;
}

bool ReplayRequestUpgradeSkillLevel(u16 skillId);

void SkillUpgradeProductionSendReplay()
{
    SetCase("skill_upgrade_production_send", "PK46.02", "PK01.02-final-0112-skillup",
        "UISkillListWnd/QtSkillTree -> GameMsg_RequestUpgradeSkillLevel -> RequestUpgradeSkillLevel -> CRagConnection::SendPacket");
    const ro::net::MapGameplaySendProfile& profile = ro::net::GetActiveMapGameplaySendProfile();
    Require(profile.id == ro::net::MapGameplaySendProfileId::PacketVer23 && profile.skillUp == 0x0112,
        "replay fixture requires the default PacketVer23 skill-up profile");
    CRagConnection* const connection = CRagConnection::instance();
    connection->Disconnect();
    Require(!ReplayRequestUpgradeSkillLevel(0x1234),
        "skill-up sender reported success without an open connection");
    LoopbackPeer peer(*connection);
    Require(!ReplayRequestUpgradeSkillLevel(0), "skill-up sender accepted skill ID zero");
    Require(SelectSocket(peer.peer.value, true, false, std::chrono::milliseconds(50)) == 0,
        "zero-skill request unexpectedly emitted bytes");
    Require(ReplayRequestUpgradeSkillLevel(0x1234),
        "production skill-up sender rejected an open loopback connection");
    const std::vector<u8> captured = peer.ReceiveExact(4);
    const std::vector<u8> expected{0x12,0x01,0x34,0x12};
    Require(captured == expected,
        "production skill-up sender bytes differ from the selected four-byte server frame");
    connection->Disconnect();
    std::cout << "{\"case_id\":\"skill_upgrade_production_send\",\"contract_id\":\"PK46.02\",\"fixture_id\":\"PK01.02-final-0112-skillup\",\"captured_packet_hex\":\"" << Hex(captured) << "\",\"zero_skill_rejected\":true,\"outcome\":\"PASS\"}" << std::endl;
}

bool ReplayRequestIdentifyInventoryItem(s16 itemIndex);

void IdentifyItemCancelProductionSendReplay()
{
    SetCase("identify_item_cancel_production_send", "PK46.02", "PK01.02-final-0178-item-identify",
        "UIItemIdentifyWnd::CancelSelection -> GameMsg_RequestIdentifyInventoryItem(-1) -> RequestIdentifyInventoryItem -> CRagConnection::SendPacket");
    const ro::net::MapGameplaySendProfile& profile = ro::net::GetActiveMapGameplaySendProfile();
    Require(profile.id == ro::net::MapGameplaySendProfileId::PacketVer23 && profile.itemIdentify == 0x0178,
        "replay fixture requires the default PacketVer23 item-identify profile");
    CRagConnection* const connection = CRagConnection::instance();
    connection->Disconnect();
    Require(!ReplayRequestIdentifyInventoryItem(-1),
        "identify-cancel sender reported success without an open connection");
    LoopbackPeer peer(*connection);
    Require(ReplayRequestIdentifyInventoryItem(-1),
        "production identify-cancel sender rejected an open loopback connection");
    const std::vector<u8> captured = peer.ReceiveExact(4);
    const std::vector<u8> expected{0x78,0x01,0xFF,0xFF};
    Require(captured == expected,
        "production identify-cancel sender bytes differ from the selected four-byte server frame");
    connection->Disconnect();
    std::cout << "{\"case_id\":\"identify_item_cancel_production_send\",\"contract_id\":\"PK46.02\",\"fixture_id\":\"PK01.02-final-0178-item-identify\",\"captured_packet_hex\":\"" << Hex(captured) << "\",\"cancel_index_signed_minus_one\":true,\"outcome\":\"PASS\"}" << std::endl;
}

void IdentifyItemSelectionProductionSendReplay()
{
    SetCase("identify_item_selection_production_send", "PK46.02", "PK01.02-final-0178-item-identify",
        "UIItemIdentifyWnd::SubmitSelection -> GameMsg_RequestIdentifyInventoryItem -> production sender");
    constexpr unsigned int itemIndex = 30000;
    Require(g_session.GetInventoryItemByIndex(itemIndex) == nullptr,
        "identify replay fixture item index is already present");
    ITEM_INFO item{};
    item.m_itemIndex = itemIndex;
    item.m_num = 1;
    g_session.SetInventoryItem(item);
    CRagConnection* const connection = CRagConnection::instance();
    connection->Disconnect();
    Require(!ReplayRequestIdentifyInventoryItem(static_cast<s16>(itemIndex)),
        "identify sender reported success without an open connection");

    {
        LoopbackPeer peer(*connection);
        Require(ReplayRequestIdentifyInventoryItem(static_cast<s16>(itemIndex)),
            "valid un-identified inventory item was rejected by the production sender");
        const std::vector<u8> expected{0x78, 0x01, 0x30, 0x75};
        Require(peer.ReceiveExact(expected.size()) == expected,
            "item-identify selection did not emit the selected four-byte frame");
    }

    item.m_isIdentified = 1;
    g_session.SetInventoryItem(item);
    Require(!ReplayRequestIdentifyInventoryItem(static_cast<s16>(itemIndex)),
        "already-identified inventory item was accepted for identification");
    connection->Disconnect();
    g_session.RemoveInventoryItem(itemIndex, item.m_num);
    std::cout << "{\"case_id\":\"identify_item_selection_production_send\",\"contract_id\":\"PK46.02\",\"fixture_id\":\"PK01.02-final-0178-item-identify\",\"outcome\":\"PASS\",\"assertions\":\"send failure and identified-item guard; valid item emits four-byte frame\"}" << std::endl;
}

bool ReplayRequestShortcutSlotUse(int slot);
bool ReplayHasPendingSkillUse(const CGameMode& mode);
bool ReplayRequestShortcutSlotUpdate(int absoluteSlotIndex);

void ShortcutSlotUpdateProductionSendReplay()
{
    SetCase("shortcut_slot_update_production_send", "PK46.02", "PK01.02-final-02ba-hotkey",
        "UIShortCutWnd -> GameMsg_RequestShortcutUpdate -> RequestShortcutSlotUpdate -> CRagConnection::SendPacket");
    const int slotIndex = 5;
    const SHORTCUT_SLOT* const originalPtr = g_session.GetShortcutSlotByAbsoluteIndex(slotIndex);
    Require(originalPtr != nullptr, "shortcut fixture slot is outside the session array");
    const SHORTCUT_SLOT original = *originalPtr;
    g_session.SetShortcutSlotByAbsoluteIndex(slotIndex, 1, 0x11223344, 0x5566);

    CRagConnection* const connection = CRagConnection::instance();
    connection->Disconnect();
    Require(!ReplayRequestShortcutSlotUpdate(-1), "shortcut sender reported success without an open connection");
    LoopbackPeer peer(*connection);
    Require(!ReplayRequestShortcutSlotUpdate(-1), "shortcut sender accepted an invalid absolute slot");
    Require(SelectSocket(peer.peer.value, true, false, std::chrono::milliseconds(50)) == 0,
        "invalid shortcut slot unexpectedly emitted bytes");
    Require(ReplayRequestShortcutSlotUpdate(slotIndex), "production shortcut sender rejected an open loopback connection");
    const std::vector<u8> captured = peer.ReceiveExact(11);
    const std::vector<u8> expected{0xBA,0x02,0x05,0x00,0x01,0x44,0x33,0x22,0x11,0x66,0x55};
    Require(captured == expected,
        "production shortcut sender bytes differ from the selected eleven-byte server frame");
    connection->Disconnect();
    g_session.SetShortcutSlotByAbsoluteIndex(slotIndex, original.isSkill, original.id, original.count);
    std::cout << "{\"case_id\":\"shortcut_slot_update_production_send\",\"contract_id\":\"PK46.02\",\"fixture_id\":\"PK01.02-final-02ba-hotkey\",\"captured_packet_hex\":\"" << Hex(captured) << "\",\"slot_restored\":true,\"outcome\":\"PASS\"}" << std::endl;
}

void InspectPlayerEquipmentProductionSendReplay()
{
    SetCase("inspect_player_equipment_production_send", "PK46.02", "PK01.02-final-02d6-view-player-equip",
        "UIPlayerContextMenuWnd -> Commerce::PlayerAction(CheckEquipmentInfo) -> CRagConnection::SendPacket");
    CRagConnection* const connection = CRagConnection::instance();
    connection->Disconnect();
    Require(!ro::commerce::PlayerAction(CGameMode::PlayerContext_CheckEquipmentInfo, 0x11223344, "Target"),
        "equipment-view sender reported success without an open connection");
    LoopbackPeer peer(*connection);
    Require(!ro::commerce::PlayerAction(CGameMode::PlayerContext_CheckEquipmentInfo, 0, "Target"),
        "equipment-view sender accepted a zero target");
    Require(SelectSocket(peer.peer.value, true, false, std::chrono::milliseconds(50)) == 0,
        "zero-target equipment-view request unexpectedly emitted bytes");
    Require(ro::commerce::PlayerAction(CGameMode::PlayerContext_CheckEquipmentInfo, 0x11223344, "Target"),
        "production equipment-view sender rejected an open loopback connection");
    const std::vector<u8> captured = peer.ReceiveExact(6);
    const std::vector<u8> expected{0xD6,0x02,0x44,0x33,0x22,0x11};
    Require(captured == expected,
        "production equipment-view sender bytes differ from the selected six-byte server frame");
    connection->Disconnect();
    std::cout << "{\"case_id\":\"inspect_player_equipment_production_send\",\"contract_id\":\"PK46.02\",\"fixture_id\":\"PK01.02-final-02d6-view-player-equip\",\"captured_packet_hex\":\"" << Hex(captured) << "\",\"zero_target_rejected\":true,\"outcome\":\"PASS\"}" << std::endl;
}

void AddFriendRequestProductionSendReplay()
{
    SetCase("add_friend_request_production_send", "PK46.02", "PK01.02-final-0202-friends-list-add",
        "UIPlayerContextMenuWnd -> Commerce::PlayerAction(RegisterFriend) -> named -> send -> CRagConnection::SendPacket");
    const std::string maxName(23, 'A');
    const std::string tooLongName(24, 'A');
    const u32 targetAid = 0x11223344;
    CRagConnection* const connection = CRagConnection::instance();
    connection->Disconnect();
    Require(!ro::commerce::PlayerAction(CGameMode::PlayerContext_RegisterFriend, targetAid, maxName),
        "friend request sender reported success without an open connection");
    LoopbackPeer peer(*connection);
    Require(!ro::commerce::PlayerAction(CGameMode::PlayerContext_RegisterFriend, targetAid, "")
        && !ro::commerce::PlayerAction(CGameMode::PlayerContext_RegisterFriend, targetAid, tooLongName)
        && !ro::commerce::PlayerAction(CGameMode::PlayerContext_RegisterFriend, 0, maxName),
        "friend request sender accepted empty/overlength name or zero target");
    Require(SelectSocket(peer.peer.value, true, false, std::chrono::milliseconds(50)) == 0,
        "rejected friend request unexpectedly emitted bytes");
    Require(ro::commerce::PlayerAction(CGameMode::PlayerContext_RegisterFriend, targetAid, maxName),
        "production friend request sender rejected the maximum supported name");
    const std::vector<u8> captured = peer.ReceiveExact(26);
    std::vector<u8> expected(26, 0);
    expected[0] = 0x02;
    expected[1] = 0x02;
    std::fill(expected.begin() + 2, expected.begin() + 25, static_cast<u8>('A'));
    Require(captured == expected,
        "production friend request bytes differ from the selected 26-byte name frame");
    connection->Disconnect();
    std::cout << "{\"case_id\":\"add_friend_request_production_send\",\"contract_id\":\"PK46.02\",\"fixture_id\":\"PK01.02-final-0202-friends-list-add\",\"captured_packet_hex\":\"" << Hex(captured) << "\",\"max_name_accepted\":true,\"overlength_rejected\":true,\"outcome\":\"PASS\"}" << std::endl;
}

void GuildInfoRefreshProductionSendReplay()
{
    SetCase("guild_info_refresh_production_send", "PK46.02", "PK01.02-final-014f-guild-request-info",
        "UIGuildWnd::SetShow -> Refresh -> SendRefreshRequests -> CRagConnection::SendPacket");
    CRagConnection* const connection = CRagConnection::instance();
    const u32 originalGuildId = g_session.m_localGuild.guildId;
    g_session.m_localGuild.guildId = 0;
    {
        LoopbackPeer peer(*connection);
        UIGuildWnd::SendRefreshRequests();
        const std::vector<u8> captured = peer.ReceiveExact(2);
        Require(captured == std::vector<u8>{0x4D,0x01},
            "guild refresh without a guild did not send only the permissions request");
        connection->Disconnect();
    }
    g_session.m_localGuild.guildId = 0x11223344;
    {
        LoopbackPeer peer(*connection);
        UIGuildWnd::SendRefreshRequests();
        const std::vector<u8> captured = peer.ReceiveExact(32);
        std::vector<u8> expected{0x4D,0x01};
        for (u8 type = 0; type <= 4; ++type) {
            expected.insert(expected.end(), {0x4F,0x01,type,0,0,0});
        }
        Require(captured == expected,
            "guild refresh request order/subtypes differ from the five selected six-byte frames");
        connection->Disconnect();
    }
    g_session.m_localGuild.guildId = originalGuildId;
    std::cout << "{\"case_id\":\"guild_info_refresh_production_send\",\"contract_id\":\"PK46.02\",\"fixture_id\":\"PK01.02-final-014f-guild-request-info\",\"guildless_permissions_only\":true,\"subtypes\":[0,1,2,3,4],\"outcome\":\"PASS\"}" << std::endl;
}

void GuildInviteProductionSendReplay()
{
    SetCase("guild_invite_production_send", "PK46.02", "PK01.02-final-0168-guild-invite",
        "UIPlayerContextMenuWnd -> Commerce::PlayerAction(InviteJoinGuild) -> CRagConnection::SendPacket");
    const u32 savedGuildId = g_session.m_localGuild.guildId;
    const unsigned char savedMaster = g_session.m_localGuild.isMaster;
    const u32 savedPermissions = g_session.m_localGuild.permissions;
    const u32 savedAid = g_session.m_aid;
    const u32 savedGid = g_session.m_gid;
    const u32 targetAid = 0x11223344;
    const u32 localAid = 0x55667788;
    const u32 localGid = 0x99AABBCC;
    g_session.m_localGuild.guildId = 1;
    g_session.m_localGuild.isMaster = 0;
    g_session.m_localGuild.permissions = 0;
    g_session.m_aid = localAid;
    g_session.m_gid = localGid;

    CRagConnection* const connection = CRagConnection::instance();
    connection->Disconnect();
    Require(!ro::commerce::PlayerAction(CGameMode::PlayerContext_InviteJoinGuild, targetAid, "Target"),
        "guild invite sender reported success without an open connection");
    LoopbackPeer peer(*connection);
    Require(!ro::commerce::PlayerAction(CGameMode::PlayerContext_InviteJoinGuild, targetAid, "Target"),
        "guild invite sender bypassed missing invite permission");
    g_session.m_localGuild.permissions = 0x01;
    Require(!ro::commerce::PlayerAction(CGameMode::PlayerContext_InviteJoinGuild, 0, "Target")
        && !ro::commerce::PlayerAction(CGameMode::PlayerContext_InviteJoinGuild, targetAid, ""),
        "guild invite sender accepted zero target or empty name");
    Require(SelectSocket(peer.peer.value, true, false, std::chrono::milliseconds(50)) == 0,
        "rejected guild invite unexpectedly emitted bytes");
    Require(ro::commerce::PlayerAction(CGameMode::PlayerContext_InviteJoinGuild, targetAid, "Target"),
        "permitted production guild invite sender rejected an open loopback connection");
    const std::vector<u8> captured = peer.ReceiveExact(14);
    const std::vector<u8> expected{0x68,0x01,0x44,0x33,0x22,0x11,0x88,0x77,0x66,0x55,0xCC,0xBB,0xAA,0x99};
    Require(captured == expected,
        "production guild invite sender bytes differ from the selected fourteen-byte server frame");
    connection->Disconnect();
    g_session.m_localGuild.guildId = savedGuildId;
    g_session.m_localGuild.isMaster = savedMaster;
    g_session.m_localGuild.permissions = savedPermissions;
    g_session.m_aid = savedAid;
    g_session.m_gid = savedGid;
    std::cout << "{\"case_id\":\"guild_invite_production_send\",\"contract_id\":\"PK46.02\",\"fixture_id\":\"PK01.02-final-0168-guild-invite\",\"permission_gate_checked\":true,\"captured_packet_hex\":\"" << Hex(captured) << "\",\"session_restored\":true,\"outcome\":\"PASS\"}" << std::endl;
}

void RejectWhisperProductionSendReplay()
{
    SetCase("reject_whisper_production_send", "PK46.02", "PK01.02-final-00cf-pm-ignore",
        "UIPlayerContextMenuWnd -> Commerce::PlayerAction(RejectWhispering) -> CRagConnection::SendPacket");
    const std::string maxName(23, 'R');
    const std::string tooLongName(24, 'R');
    const u32 targetAid = 0x11223344;
    CRagConnection* const connection = CRagConnection::instance();
    connection->Disconnect();
    Require(!ro::commerce::PlayerAction(CGameMode::PlayerContext_RejectWhispering, targetAid, maxName),
        "reject-whisper sender reported success without an open connection");
    LoopbackPeer peer(*connection);
    Require(!ro::commerce::PlayerAction(CGameMode::PlayerContext_RejectWhispering, targetAid, "")
        && !ro::commerce::PlayerAction(CGameMode::PlayerContext_RejectWhispering, targetAid, tooLongName)
        && !ro::commerce::PlayerAction(CGameMode::PlayerContext_RejectWhispering, 0, maxName),
        "reject-whisper sender accepted empty/overlength name or zero target");
    Require(SelectSocket(peer.peer.value, true, false, std::chrono::milliseconds(50)) == 0,
        "rejected whisper-ignore request unexpectedly emitted bytes");
    Require(ro::commerce::PlayerAction(CGameMode::PlayerContext_RejectWhispering, targetAid, maxName),
        "production reject-whisper sender rejected the maximum supported name");
    const std::vector<u8> captured = peer.ReceiveExact(27);
    std::vector<u8> expected(27, 0);
    expected[0] = 0xCF;
    std::fill(expected.begin() + 2, expected.begin() + 25, static_cast<u8>('R'));
    Require(captured == expected,
        "production reject-whisper bytes differ from the selected 27-byte server frame");
    connection->Disconnect();
    std::cout << "{\"case_id\":\"reject_whisper_production_send\",\"contract_id\":\"PK46.02\",\"fixture_id\":\"PK01.02-final-00cf-pm-ignore\",\"captured_packet_hex\":\"" << Hex(captured) << "\",\"max_name_accepted\":true,\"overlength_rejected\":true,\"outcome\":\"PASS\"}" << std::endl;
}

void TradeRequestProductionSendReplay()
{
    SetCase("trade_request_production_send", "PK46.02", "PK01.02-final-00e4-trade-request",
        "UIPlayerContextMenuWnd -> Commerce::PlayerAction(RequestDeal) -> CRagConnection::SendPacket");
    const ro::commerce::State savedCommerceState = ro::commerce::state;
    const u32 savedAid = g_session.m_aid;
    const u32 targetAid = 0x11223344;
    g_session.m_aid = 0x55667788;
    ro::commerce::Reset();
    ro::commerce::UpdateTradeContext(1234, {{targetAid, {"Target", 2}}});

    CRagConnection* const connection = CRagConnection::instance();
    connection->Disconnect();
    Require(!ro::commerce::PlayerAction(CGameMode::PlayerContext_RequestDeal, targetAid, "Target"),
        "trade request reported success without an open connection");
    Require(!ro::commerce::state.tradeWaiting,
        "failed trade request entered the pending/waiting state");
    LoopbackPeer peer(*connection);
    Require(ro::commerce::PlayerAction(CGameMode::PlayerContext_RequestDeal, targetAid, "Target"),
        "production trade request rejected the server's maximum accepted distance");
    const std::vector<u8> captured = peer.ReceiveExact(6);
    const std::vector<u8> expected{0xE4,0x00,0x44,0x33,0x22,0x11};
    Require(captured == expected,
        "production trade request bytes differ from the selected six-byte server frame");
    Require(ro::commerce::state.tradeWaiting && ro::commerce::state.partnerId == targetAid
        && ro::commerce::state.partner == "Target",
        "successful trade request did not enter the expected pending state");
    connection->Disconnect();
    ro::commerce::state = savedCommerceState;
    g_session.m_aid = savedAid;
    std::cout << "{\"case_id\":\"trade_request_production_send\",\"contract_id\":\"PK46.02\",\"fixture_id\":\"PK01.02-final-00e4-trade-request\",\"captured_packet_hex\":\"" << Hex(captured) << "\",\"pending_state_after_success\":true,\"failed_send_not_pending\":true,\"outcome\":\"PASS\"}" << std::endl;
}

void MailComposerProductionSendReplay()
{
    SetCase("mail_composer_production_send", "PK46.02", "PK01.02-final-0246-mail-window-open",
        "UIPlayerContextMenuWnd -> Commerce::PlayerAction(SendMail) -> CRagConnection::SendPacket");
    const ro::commerce::State savedCommerceState = ro::commerce::state;
    const u32 savedAid = g_session.m_aid;
    const u32 targetAid = 0x11223344;
    g_session.m_aid = 0x55667788;
    ro::commerce::Reset();

    CRagConnection* const connection = CRagConnection::instance();
    connection->Disconnect();
    Require(!ro::commerce::PlayerAction(CGameMode::PlayerContext_SendMail, targetAid, "Target"),
        "mail composer sender reported success without an open connection");
    Require(ro::commerce::state.mail.visible && ro::commerce::state.mailTo == "Target"
        && !ro::commerce::state.mailPending,
        "mail composer local state differs after failed open request");
    LoopbackPeer peer(*connection);
    ro::commerce::state.tradeWaiting = true;
    Require(!ro::commerce::PlayerAction(CGameMode::PlayerContext_SendMail, targetAid, "Target"),
        "mail composer request bypassed the busy transaction guard");
    ro::commerce::state.tradeWaiting = false;
    Require(SelectSocket(peer.peer.value, true, false, std::chrono::milliseconds(50)) == 0,
        "busy mail composer request unexpectedly emitted bytes");
    Require(ro::commerce::PlayerAction(CGameMode::PlayerContext_SendMail, targetAid, "Target"),
        "production mail composer sender rejected an open loopback connection");
    const std::vector<u8> captured = peer.ReceiveExact(4);
    const std::vector<u8> expected{0x46,0x02,0x00,0x00};
    Require(captured == expected,
        "production mail composer bytes differ from the selected four-byte server frame");
    Require(ro::commerce::state.mail.visible && ro::commerce::state.mailTo == "Target",
        "successful mail composer request did not retain its local draft state");
    connection->Disconnect();
    ro::commerce::state = savedCommerceState;
    g_session.m_aid = savedAid;
    std::cout << "{\"case_id\":\"mail_composer_production_send\",\"contract_id\":\"PK46.02\",\"fixture_id\":\"PK01.02-final-0246-mail-window-open\",\"captured_packet_hex\":\"" << Hex(captured) << "\",\"busy_guard_checked\":true,\"draft_retained_on_failure\":true,\"outcome\":\"PASS\"}" << std::endl;
}

bool ReplayRequestCloseStorage();

void StorageCloseProductionSendReplay()
{
    SetCase("storage_close_production_send", "PK46.02", "PK01.02-final-0193-close-kafra",
        "UIStorageWnd::OnButton(Close) -> GameMsg_RequestStorageClose -> RequestCloseStorage -> CRagConnection::SendPacket");
    const bool savedStorageOpen = g_session.m_storageOpen;
    g_session.m_storageOpen = false;
    CRagConnection* const connection = CRagConnection::instance();
    connection->Disconnect();
    Require(!ReplayRequestCloseStorage(),
        "storage close sender reported success without an open connection");
    LoopbackPeer peer(*connection);
    Require(!ReplayRequestCloseStorage(), "storage close sender bypassed the storage-open guard");
    Require(SelectSocket(peer.peer.value, true, false, std::chrono::milliseconds(50)) == 0,
        "closed-storage request unexpectedly emitted bytes");
    g_session.m_storageOpen = true;
    Require(ReplayRequestCloseStorage(),
        "production storage close sender rejected an open loopback connection");
    const std::vector<u8> captured = peer.ReceiveExact(2);
    const std::vector<u8> expected{0x93,0x01};
    Require(captured == expected,
        "production storage close sender bytes differ from the selected two-byte server frame");
    connection->Disconnect();
    g_session.m_storageOpen = savedStorageOpen;
    std::cout << "{\"case_id\":\"storage_close_production_send\",\"contract_id\":\"PK46.02\",\"fixture_id\":\"PK01.02-final-0193-close-kafra\",\"captured_packet_hex\":\"" << Hex(captured) << "\",\"storage_guard_checked\":true,\"session_restored\":true,\"outcome\":\"PASS\"}" << std::endl;
}

void MailSendProductionReplay()
{
    SetCase("mail_send_production_replay", "PK46.02", "PK01.02-final-0248-mail-send",
        "Commerce::Action(mail) -> variable 0x0248 builder -> CRagConnection::SendPacket");
    const ro::commerce::State savedCommerceState = ro::commerce::state;
    ro::commerce::Reset();
    auto makeMail = [](std::string recipient, std::string subject, std::string body) {
        ro::commerce::Command command;
        command.action = "mail";
        command.text = std::move(recipient);
        command.subject = std::move(subject);
        command.body = std::move(body);
        return command;
    };
    const ro::commerce::Command shortMail = makeMail("Recipient", "Subject", "Hi");
    CRagConnection* const connection = CRagConnection::instance();
    connection->Disconnect();
    Require(!ro::commerce::Action(shortMail), "mail sender reported success without an open connection");
    Require(!ro::commerce::state.mailPending,
        "failed mail send entered the pending-result state");
    LoopbackPeer peer(*connection);
    Require(!ro::commerce::Action(makeMail("", "Subject", "Body"))
        && !ro::commerce::Action(makeMail(std::string(24, 'R'), "Subject", "Body"))
        && !ro::commerce::Action(makeMail("Recipient", "", "Body"))
        && !ro::commerce::Action(makeMail("Recipient", std::string(40, 'S'), "Body"))
        && !ro::commerce::Action(makeMail("Recipient", "Subject", std::string(255, 'B'))),
        "mail sender accepted empty or over-limit recipient/subject/body");
    ro::commerce::state.mailPending = true;
    Require(!ro::commerce::Action(shortMail), "mail sender accepted a second request while one was pending");
    ro::commerce::state.mailPending = false;
    Require(SelectSocket(peer.peer.value, true, false, std::chrono::milliseconds(50)) == 0,
        "rejected mail request unexpectedly emitted bytes");

    const std::string recipient(23, 'R');
    const std::string subject(39, 'S');
    const std::string body(254, 'B');
    Require(ro::commerce::Action(makeMail(recipient, subject, body)),
        "production mail sender rejected the maximum accepted field sizes");
    const std::vector<u8> captured = peer.ReceiveExact(324);
    std::vector<u8> expected(324, 0);
    expected[0] = 0x48;
    expected[1] = 0x02;
    expected[2] = 0x44;
    expected[3] = 0x01;
    std::fill(expected.begin() + 4, expected.begin() + 27, static_cast<u8>('R'));
    std::fill(expected.begin() + 28, expected.begin() + 67, static_cast<u8>('S'));
    expected[68] = 255;
    std::fill(expected.begin() + 69, expected.begin() + 323, static_cast<u8>('B'));
    Require(captured == expected,
        "production mail sender bytes differ from the independent maximum-size frame fixture");
    Require(ro::commerce::state.mailPending,
        "successful mail send did not enter the pending-result state");
    ro::commerce::state.mailPending = false;
    Require(ro::commerce::Action(shortMail),
        "production mail sender rejected short valid text fields");
    const std::vector<u8> capturedShort = peer.ReceiveExact(72);
    std::vector<u8> expectedShort(72, 0);
    expectedShort[0] = 0x48;
    expectedShort[1] = 0x02;
    expectedShort[2] = 0x48;
    std::copy(shortMail.text.begin(), shortMail.text.end(), expectedShort.begin() + 4);
    std::copy(shortMail.subject.begin(), shortMail.subject.end(), expectedShort.begin() + 28);
    expectedShort[68] = 3;
    expectedShort[69] = 'H';
    expectedShort[70] = 'i';
    Require(capturedShort == expectedShort,
        "short production mail frame differs in field padding, length, or body termination");
    Require(ro::commerce::state.mailPending,
        "successful short mail send did not enter the pending-result state");
    connection->Disconnect();
    ro::commerce::state = savedCommerceState;
    std::cout << "{\"case_id\":\"mail_send_production_replay\",\"contract_id\":\"PK46.02\",\"fixture_id\":\"PK01.02-final-0248-mail-send\",\"captured_lengths\":[324,72],\"recipient_subject_body_limits_checked\":true,\"short_field_padding_and_body_terminator_checked\":true,\"pending_after_success\":true,\"outcome\":\"PASS\"}" << std::endl;
}

bool ReplaySendMapLoadEndAckPacket();

void MailResultProductionReceiveReplay()
{
    SetCase("mail_result_production_receive", "PK46.02", "PK01.02-final-0249-mail-result",
        "ProcessGameModePacketLoop -> production packet router -> Commerce::Receive");
    const ro::commerce::State savedCommerceState = ro::commerce::state;
    ro::commerce::Reset();
    CGameMode mode;
    TestConnection connection;
    CGameModePacketRouter& router = GetProductionGameModePacketRouter();
    router.Clear();
    RegisterDefaultGameModePacketHandlers(router);

    ro::commerce::state.mail.visible = true;
    ro::commerce::state.mailPending = true;
    const std::vector<u8> failure{0x49, 0x02, 0x01};
    Require(connection.QueueBytes(std::vector<u8>(failure.begin(), failure.begin() + 2)),
        "could not queue partial mail-result frame");
    const GameModePacketLoopResult partial = ProcessGameModePacketLoop(mode, connection, router, 1);
    Require(partial.packetsProcessed == 0 && ro::commerce::state.mailPending
        && ro::commerce::state.mail.visible,
        "partial mail-result frame changed pending or composer state");
    Require(connection.QueueBytes(std::vector<u8>(failure.begin() + 2, failure.end())),
        "could not complete mail-result frame");
    const GameModePacketLoopResult failed = ProcessGameModePacketLoop(mode, connection, router, 1);
    Require(failed.packetsProcessed == 1 && failed.packetsDispatched == 1
        && !ro::commerce::state.mailPending && ro::commerce::state.mail.visible
        && ro::commerce::state.status == "Mail could not be sent.",
        "production mail failure result did not clear pending state and retain the composer");

    ro::commerce::state.mailPending = true;
    const std::vector<u8> success{0x49, 0x02, 0x00};
    Require(connection.QueueBytes(success), "could not queue successful mail-result frame");
    const GameModePacketLoopResult sent = ProcessGameModePacketLoop(mode, connection, router, 1);
    Require(sent.packetsProcessed == 1 && sent.packetsDispatched == 1
        && !ro::commerce::state.mailPending && !ro::commerce::state.mail.visible
        && ro::commerce::state.status == "Mail sent.",
        "production mail success result did not clear pending state and close the composer");
    ro::commerce::state = savedCommerceState;
    std::cout << "{\"case_id\":\"mail_result_production_receive\",\"contract_id\":\"PK46.02\",\"fixture_id\":\"PK01.02-final-0249-mail-result\",\"partial_frame_buffered\":true,\"failure_retained_composer\":true,\"success_closed_composer\":true,\"outcome\":\"PASS\"}" << std::endl;
}

void MapLoadEndAckProductionSendReplay()
{
    SetCase("map_load_end_ack_production_send", "PK46.02", "PK46.02-007d-selected-server-literal",
        "HandleMapChange -> SendLoadEndAckPacket -> CRagConnection::SendPacket");
    CRagConnection* const connection = CRagConnection::instance();
    connection->Disconnect();
    Require(!ReplaySendMapLoadEndAckPacket(), "load-end acknowledgement reported success without an open connection");
    LoopbackPeer peer(*connection);
    Require(ReplaySendMapLoadEndAckPacket(), "production load-end acknowledgement sender rejected an open connection");
    const std::vector<u8> expected{0x7D, 0x00};
    const std::vector<u8> captured = peer.ReceiveExact(expected.size());
    Require(captured == expected, "production load-end acknowledgement differs from the selected server literal 0x007D frame");
    connection->Disconnect();
    std::cout << "{\"case_id\":\"map_load_end_ack_production_send\",\"contract_id\":\"PK46.02\",\"fixture_id\":\"PK46.02-007d-selected-server-literal\",\"expected_packet_hex\":\"7d00\",\"captured_packet_hex\":\"" << Hex(captured) << "\",\"closed_connection_rejected\":true,\"outcome\":\"PASS\"}\n";
}

void ActionFailureProductionReceiveReplay()
{
    SetCase("action_failure_production_receive", "PK45.03", "PK45.03-action-failure",
        "ProcessGameModePacketLoop -> production packet router -> HandleActionFailure");
    const u32 savedPlayerJob = g_session.m_playerJob;
    g_session.m_playerJob = 25;
    CGameMode mode;
    CWorld world;
    CPlayer player;
    world.m_player = &player;
    mode.m_world = &world;
    TestConnection connection;
    CGameModePacketRouter& router = GetProductionGameModePacketRouter();
    router.Clear();
    RegisterDefaultGameModePacketHandlers(router);

    auto seedRetryState = [&]() {
        mode.m_lastMonGid = 11;
        mode.m_lastLockOnMonGid = 12;
        mode.m_hasAttackChaseHint = 1;
        mode.m_attackChaseTargetGid = 13;
        mode.m_hasSkillChase = 1;
        mode.m_hasGroundSkillChase = 1;
        mode.m_isLeftButtonHeld = 1;
        mode.m_hasHeldMoveTarget = 1;
        player.m_proceedType.type = 3;
        player.m_proceedTargetGid = 13;
        player.m_skillId = 10000;
        player.m_isAttackRequest = 1;
    };
    auto sendFailureType = [&](u16 type) {
        const std::vector<u8> frame{0x3B, 0x01, static_cast<u8>(type), static_cast<u8>(type >> 8)};
        Require(connection.QueueBytes(frame), "could not queue action-failure frame");
        const GameModePacketLoopResult result = ProcessGameModePacketLoop(mode, connection, router, 1);
        Require(result.packetsProcessed == 1 && result.packetsDispatched == 1,
            "production router did not dispatch a complete action-failure frame");
    };
    auto retryStateCleared = [&]() {
        return mode.m_lastMonGid == 0 && mode.m_lastLockOnMonGid == 0
            && mode.m_hasAttackChaseHint == 0 && mode.m_attackChaseTargetGid == 0
            && mode.m_hasSkillChase == 0 && mode.m_hasGroundSkillChase == 0
            && !mode.m_isLeftButtonHeld && !mode.m_hasHeldMoveTarget
            && player.m_proceedType.type == 0 && player.m_proceedTargetGid == 0 && !player.m_isAttackRequest;
    };
    auto requireRetryStateRetained = [&]() {
        Require(mode.m_lastMonGid == 11 && mode.m_lastLockOnMonGid == 12
            && mode.m_hasAttackChaseHint == 1 && mode.m_attackChaseTargetGid == 13
            && mode.m_hasSkillChase == 1 && mode.m_hasGroundSkillChase == 1
            && mode.m_isLeftButtonHeld && mode.m_hasHeldMoveTarget && player.m_proceedType.type == 3,
            "non-clearing action-failure type changed retry state");
    };

    for (u16 type = 0; type <= 2; ++type) {
        seedRetryState();
        sendFailureType(type);
        Require(retryStateCleared(), "action-failure type 0-2 did not clear retry state");
        sendFailureType(type);
        Require(retryStateCleared(), "repeated action-failure delivery was not idempotent");
    }
    seedRetryState();
    sendFailureType(3);
    requireRetryStateRetained();
    seedRetryState();
    sendFailureType(4);
    requireRetryStateRetained();
    g_session.m_playerJob = savedPlayerJob;
    std::cout << "{\"case_id\":\"action_failure_production_receive\",\"contract_id\":\"PK45.03\",\"fixture_id\":\"PK45.03-action-failure\",\"types_0_2_clear_retry_state\":true,\"repeated_delivery_idempotent\":true,\"type_3_and_unknown_type_preserve_retry_state\":true,\"outcome\":\"PASS\"}" << std::endl;
}

void PartyInviteProductionSendReplay()
{
    SetCase("party_invite_production_send", "PK46.02", "PK46.02-map-party-invite-request",
        "QtCommerce player-context action -> CGameMode::SendMsg -> RequestPartyInvite -> CRagConnection::SendPacket");
    const auto savedPartyList = g_session.m_partyList;
    g_session.m_partyList.clear();
    CRagConnection* const connection = CRagConnection::instance();
    connection->Disconnect();
    LoopbackPeer peer(*connection);
    CGameMode mode;
    const u32 targetAid = 0x11223344;
    Require(mode.SendMsg(CGameMode::GameMsg_RequestPartyInvite, targetAid, 0, 0) == 0,
        "party invite accepted while the local party list was empty");
    Require(mode.SendMsg(CGameMode::GameMsg_RequestPartyInvite, 0, 0, 0) == 0,
        "party invite accepted a zero target AID");
    Require(SelectSocket(peer.peer.value, true, false, std::chrono::milliseconds(50)) == 0,
        "rejected party invite request emitted bytes");

    g_session.m_partyList.emplace_back();
    Require(mode.SendMsg(CGameMode::GameMsg_RequestPartyInvite, targetAid, 0, 0) != 0,
        "valid party invite was rejected with a local party and target");
    const std::vector<u8> captured = peer.ReceiveExact(6);
    const std::vector<u8> expected{0xFC, 0x00, 0x44, 0x33, 0x22, 0x11};
    Require(captured == expected,
        "production party invite bytes differ from the independent 0x00FC fixture");
    connection->Disconnect();
    g_session.m_partyList = savedPartyList;
    std::cout << "{\"case_id\":\"party_invite_production_send\",\"contract_id\":\"PK46.02\",\"fixture_id\":\"PK46.02-map-party-invite-request\",\"expected_packet_hex\":\"fc0044332211\",\"empty_party_and_zero_aid_rejected\":true,\"outcome\":\"PASS\"}" << std::endl;
}

void PartyInviteReplyProductionSendReplay()
{
    SetCase("party_invite_reply_production_send", "PK46.02", "PK46.02-map-party-invite-reply-request",
        "UIJoinPartyAcceptWnd reply action -> CGameMode::SendMsg -> ReplyPartyInvite -> CRagConnection::SendPacket");
    CRagConnection* const connection = CRagConnection::instance();
    connection->Disconnect();
    LoopbackPeer peer(*connection);
    CGameMode mode;
    const u32 partyId = 0x11223344;
    Require(mode.SendMsg(CGameMode::GameMsg_RequestPartyInviteReply, 0, 1, 0) == 0,
        "party-invite reply accepted a zero party ID");
    Require(SelectSocket(peer.peer.value, true, false, std::chrono::milliseconds(50)) == 0,
        "zero-party-ID reply emitted bytes");

    Require(mode.SendMsg(CGameMode::GameMsg_RequestPartyInviteReply, partyId, 1, 0) != 0,
        "party-invite acceptance reply was rejected");
    const std::vector<u8> accepted = peer.ReceiveExact(10);
    const std::vector<u8> expectedAccept{0xFF,0x00,0x44,0x33,0x22,0x11,0x01,0x00,0x00,0x00};
    Require(accepted == expectedAccept,
        "party-invite acceptance reply differs from the independent ten-byte fixture");

    Require(mode.SendMsg(CGameMode::GameMsg_RequestPartyInviteReply, partyId, 0, 0) != 0,
        "party-invite rejection reply was rejected");
    const std::vector<u8> rejected = peer.ReceiveExact(10);
    const std::vector<u8> expectedReject{0xFF,0x00,0x44,0x33,0x22,0x11,0x00,0x00,0x00,0x00};
    Require(rejected == expectedReject,
        "party-invite rejection reply differs from the independent ten-byte fixture");
    connection->Disconnect();
    std::cout << "{\"case_id\":\"party_invite_reply_production_send\",\"contract_id\":\"PK46.02\",\"fixture_id\":\"PK46.02-map-party-invite-reply-request\",\"accept_packet_hex\":\"ff004433221101000000\",\"reject_packet_hex\":\"ff004433221100000000\",\"zero_party_id_rejected\":true,\"outcome\":\"PASS\"}" << std::endl;
}

void PartyCreateProductionSendReplay()
{
    SetCase("party_create_production_send", "PK46.02", "PK46.02-map-party-create-request",
        "UIPartyOptionWnd submit -> CGameMode::SendMsg -> RequestCreateParty -> CRagConnection::SendPacket");
    const auto savedPartyList = g_session.m_partyList;
    g_session.m_partyList.clear();
    CRagConnection* const connection = CRagConnection::instance();
    connection->Disconnect();
    CGameMode mode;
    const std::string overlongName(26, 'P');
    Require(mode.SendMsg(CGameMode::GameMsg_RequestPartyCreate,
            reinterpret_cast<msgparam_t>(const_cast<char*>(overlongName.c_str())), 0x07, 0) == 0,
        "party creation reported success without an open connection");
    Require(mode.m_pendingPartyCreateName.empty(),
        "failed party-create send retained a pending name");

    LoopbackPeer peer(*connection);
    const std::string whitespaceName(" \t\r\n ");
    Require(mode.SendMsg(CGameMode::GameMsg_RequestPartyCreate,
            reinterpret_cast<msgparam_t>(const_cast<char*>(whitespaceName.c_str())), 0x07, 0) == 0,
        "party creation accepted an empty normalized name");
    Require(SelectSocket(peer.peer.value, true, false, std::chrono::milliseconds(50)) == 0,
        "empty normalized party name emitted bytes");

    Require(mode.SendMsg(CGameMode::GameMsg_RequestPartyCreate,
            reinterpret_cast<msgparam_t>(const_cast<char*>(overlongName.c_str())), 0x07, 0) != 0,
        "party creation rejected a valid name after normalization");
    const std::vector<u8> captured = peer.ReceiveExact(26);
    std::vector<u8> expected(26, 0);
    expected[0] = 0xF9;
    expected[1] = 0x00;
    std::fill(expected.begin() + 2, expected.begin() + 25, static_cast<u8>('P'));
    Require(captured == expected,
        "party-create packet differs in opcode, truncation, padding, or terminator");
    Require(mode.m_pendingPartyCreateName == std::string(23, 'P')
        && mode.m_hasPendingPartyCreateOptions
        && mode.m_pendingPartyCreateExpShare
        && mode.m_pendingPartyCreateItemShare
        && mode.m_pendingPartyCreateItemSharingType,
        "successful party-create send did not retain normalized name and option state");
    connection->Disconnect();
    g_session.m_partyList = savedPartyList;
    std::cout << "{\"case_id\":\"party_create_production_send\",\"contract_id\":\"PK46.02\",\"fixture_id\":\"PK46.02-map-party-create-request\",\"captured_length\":26,\"name_truncated_to_23\":true,\"empty_name_rejected\":true,\"options_pending_after_send\":true,\"outcome\":\"PASS\"}" << std::endl;
}

void PartyOptionChangeProductionSendReplay()
{
    SetCase("party_option_change_production_send", "PK46.02", "PK46.02-map-party-option-change-request",
        "UIPartyOptionWnd submit -> CGameMode::SendMsg -> RequestChangePartyOptions -> CRagConnection::SendPacket");
    const bool savedExpShare = g_session.m_partyExpShare;
    const bool savedItemCollectType = g_session.m_itemCollectType;
    const bool savedItemDivType = g_session.m_itemDivType;
    g_session.m_partyExpShare = true;
    g_session.m_itemCollectType = false;
    g_session.m_itemDivType = true;
    CRagConnection* const connection = CRagConnection::instance();
    connection->Disconnect();
    CGameMode mode;
    Require(mode.SendMsg(CGameMode::GameMsg_RequestPartyChangeOptions, 0, 0, 0) == 0,
        "party-option change reported success without an open connection");
    Require(g_session.m_partyExpShare && !g_session.m_itemCollectType && g_session.m_itemDivType,
        "failed party-option send optimistically changed local option state");

    LoopbackPeer peer(*connection);
    for (unsigned int bits = 0; bits < 8; ++bits) {
        const bool expShare = (bits & 0x01u) != 0;
        const bool itemShare = (bits & 0x02u) != 0;
        const bool shared = (bits & 0x04u) != 0;
        const u16 itemOption = static_cast<u16>((itemShare ? 1u : 0u) | (shared ? 2u : 0u));
        Require(mode.SendMsg(CGameMode::GameMsg_RequestPartyChangeOptions,
                static_cast<msgparam_t>(bits), 0, 0) != 0,
            "production party-option sender rejected one of the eight option combinations");
        const std::vector<u8> captured = peer.ReceiveExact(6);
        const std::vector<u8> expected{0x02, 0x01,
            static_cast<u8>(expShare ? 1 : 0), 0x00,
            static_cast<u8>(itemOption & 0xFF), static_cast<u8>(itemOption >> 8)};
        Require(captured == expected,
            "party-option production frame differs from the independent field-combination fixture");
        Require(g_session.m_partyExpShare == expShare
            && g_session.m_itemCollectType == itemShare
            && g_session.m_itemDivType == shared,
            "successful party-option send did not update the local option cache");
    }
    connection->Disconnect();
    g_session.m_partyExpShare = savedExpShare;
    g_session.m_itemCollectType = savedItemCollectType;
    g_session.m_itemDivType = savedItemDivType;
    std::cout << "{\"case_id\":\"party_option_change_production_send\",\"contract_id\":\"PK46.02\",\"fixture_id\":\"PK46.02-map-party-option-change-request\",\"option_combinations_checked\":8,\"failed_send_kept_prior_cache\":true,\"successful_send_updates_cache\":true,\"outcome\":\"PASS\"}" << std::endl;
}

void PartyDisbandProductionSendReplay()
{
    SetCase("party_disband_production_send", "PK46.02", "PK46.02-map-party-leave-request",
        "UIPartyOptionWnd master disband -> CGameMode::SendMsg -> remove-member frames -> 0x0100 leave");
    const auto savedPartyList = g_session.m_partyList;
    const u32 savedAid = g_session.m_aid;
    const bool savedMaster = g_session.m_amIPartyMaster;
    g_session.m_aid = 0x55667788;
    g_session.m_partyList.clear();
    g_session.m_amIPartyMaster = true;
    CRagConnection* const connection = CRagConnection::instance();
    connection->Disconnect();
    LoopbackPeer peer(*connection);
    CGameMode mode;
    Require(mode.SendMsg(CGameMode::GameMsg_RequestPartyDisband, 0, 0, 0) == 0,
        "party disband accepted an empty local party");
    Require(SelectSocket(peer.peer.value, true, false, std::chrono::milliseconds(50)) == 0,
        "empty-party disband emitted bytes");

    FRIEND_INFO leader{};
    leader.AID = g_session.m_aid;
    leader.characterName = "Leader";
    FRIEND_INFO member{};
    member.AID = 0x11223344;
    member.characterName = "Target";
    FRIEND_INFO invalid{};
    invalid.characterName = "Invalid";
    g_session.m_partyList.push_back(leader);
    g_session.m_partyList.push_back(member);
    g_session.m_partyList.push_back(invalid);
    g_session.m_amIPartyMaster = false;
    Require(mode.SendMsg(CGameMode::GameMsg_RequestPartyDisband, 0, 0, 0) == 0,
        "non-master party disband was accepted");
    Require(SelectSocket(peer.peer.value, true, false, std::chrono::milliseconds(50)) == 0,
        "non-master party disband emitted bytes");

    g_session.m_amIPartyMaster = true;
    Require(mode.SendMsg(CGameMode::GameMsg_RequestPartyDisband, 0, 0, 0) != 0,
        "master party disband failed with a valid member");
    std::vector<u8> expected(32, 0);
    expected[0] = 0x03;
    expected[1] = 0x01;
    expected[2] = 0x44;
    expected[3] = 0x33;
    expected[4] = 0x22;
    expected[5] = 0x11;
    std::copy(member.characterName.begin(), member.characterName.end(), expected.begin() + 6);
    expected[31] = 0x01;
    const std::vector<u8> captured = peer.ReceiveExact(expected.size());
    Require(captured == expected,
        "master disband did not send the valid member removal followed by 0x0100 leave");
    connection->Disconnect();
    g_session.m_partyList = savedPartyList;
    g_session.m_aid = savedAid;
    g_session.m_amIPartyMaster = savedMaster;
    std::cout << "{\"case_id\":\"party_disband_production_send\",\"contract_id\":\"PK46.02\",\"fixture_id\":\"PK46.02-map-party-leave-request\",\"empty_and_nonmaster_guards\":true,\"valid_member_remove_then_leave\":true,\"captured_length\":32,\"outcome\":\"PASS\"}" << std::endl;
}

void PartyInviteResultProductionReceiveReplay()
{
    SetCase("party_invite_result_production_receive", "PK46.02", "PK46.02-map-party-invite-request",
        "ProcessGameModePacketLoop -> production packet router -> HandlePartyInviteAck");
    g_windowMgr.ClearChatEvents();
    CGameMode mode;
    TestConnection connection;
    CGameModePacketRouter& router = GetProductionGameModePacketRouter();
    router.Clear();
    RegisterDefaultGameModePacketHandlers(router);

    std::vector<u8> basic(27, 0);
    basic[0] = 0xFD;
    std::copy_n("Alice", 5, basic.begin() + 2);
    basic[26] = 2;
    Require(connection.QueueBytes(basic), "could not queue basic party-invite result");
    const GameModePacketLoopResult basicResult = ProcessGameModePacketLoop(mode, connection, router, 1);
    Require(basicResult.packetsProcessed == 1 && basicResult.packetsDispatched == 1,
        "production router did not dispatch the 0x00FD party-invite result");
    const auto& basicEvents = g_windowMgr.GetChatEvents();
    Require(basicEvents.size() == 1 && basicEvents.back().text == "Alice accepted the party invitation."
        && basicEvents.back().color == 0x0000C000,
        "basic party-invite success result did not produce the expected system notice");

    std::vector<u8> extended(30, 0);
    extended[0] = 0xC5;
    extended[1] = 0x02;
    std::copy_n("Alice", 5, extended.begin() + 2);
    extended[26] = 5;
    Require(connection.QueueBytes(std::vector<u8>(extended.begin(), extended.begin() + 27)),
        "could not queue partial extended party-invite result");
    const GameModePacketLoopResult partial = ProcessGameModePacketLoop(mode, connection, router, 1);
    Require(partial.packetsProcessed == 0 && g_windowMgr.GetChatEvents().size() == 1,
        "partial 0x02C5 result was dispatched before its four-byte code arrived");
    Require(connection.QueueBytes(std::vector<u8>(extended.begin() + 27, extended.end())),
        "could not complete extended party-invite result");
    const GameModePacketLoopResult extendedResult = ProcessGameModePacketLoop(mode, connection, router, 1);
    Require(extendedResult.packetsProcessed == 1 && extendedResult.packetsDispatched == 1,
        "production router did not dispatch the complete 0x02C5 party-invite result");
    const auto& resultEvents = g_windowMgr.GetChatEvents();
    Require(resultEvents.size() == 2 && resultEvents.back().text == "The character blocked party invitations."
        && resultEvents.back().color == 0x00FF4040,
        "extended party-invite denial result did not produce the expected system notice");

    std::vector<u8> unknown(27, 0);
    unknown[0] = 0xFD;
    unknown[26] = 6;
    Require(connection.QueueBytes(unknown), "could not queue unknown party-invite result");
    const GameModePacketLoopResult unknownResult = ProcessGameModePacketLoop(mode, connection, router, 1);
    Require(unknownResult.packetsProcessed == 1 && unknownResult.packetsDispatched == 1
        && g_windowMgr.GetChatEvents().size() == 2,
        "unknown party-invite result fabricated a system notice");
    g_windowMgr.ClearChatEvents();
    std::cout << "{\"case_id\":\"party_invite_result_production_receive\",\"contract_id\":\"PK46.02\",\"fixture_id\":\"PK46.02-map-party-invite-request\",\"basic_success_notice\":true,\"extended_partial_frame_buffered\":true,\"extended_denial_notice\":true,\"unknown_result_ignored\":true,\"outcome\":\"PASS\"}" << std::endl;
}

void PartyOptionChangeResultProductionReceiveReplay()
{
    SetCase("party_option_change_result_production_receive", "PK46.02", "PK46.02-map-party-option-change-request",
        "ProcessGameModePacketLoop -> production packet router -> HandlePartyOptionChanged");
    const bool savedExpShare = g_session.m_partyExpShare;
    const bool savedItemCollectType = g_session.m_itemCollectType;
    const bool savedItemDivType = g_session.m_itemDivType;
    g_session.m_partyExpShare = true;
    g_session.m_itemCollectType = false;
    g_session.m_itemDivType = true;
    g_windowMgr.ClearChatEvents();
    CGameMode mode;
    TestConnection connection;
    CGameModePacketRouter& router = GetProductionGameModePacketRouter();
    router.Clear();
    RegisterDefaultGameModePacketHandlers(router);

    const std::vector<u8> denied{0x01,0x01,0x02,0x00,0x00,0x00};
    Require(connection.QueueBytes(denied), "could not queue denied party-option result");
    const GameModePacketLoopResult deniedResult = ProcessGameModePacketLoop(mode, connection, router, 1);
    Require(deniedResult.packetsProcessed == 1 && deniedResult.packetsDispatched == 1
        && g_session.m_partyExpShare && !g_session.m_itemCollectType && g_session.m_itemDivType,
        "denied party-option result changed the local option cache");
    Require(g_windowMgr.GetChatEvents().size() == 1
        && g_windowMgr.GetChatEvents().back().text == "Cannot change EXP sharing right now.",
        "denied party-option result did not queue its system notice");

    const std::vector<u8> disabled{0x01,0x01,0x00,0x00,0x00,0x00};
    Require(connection.QueueBytes(std::vector<u8>(disabled.begin(), disabled.begin() + 5)),
        "could not queue partial party-option result");
    const GameModePacketLoopResult partial = ProcessGameModePacketLoop(mode, connection, router, 1);
    Require(partial.packetsProcessed == 0 && g_session.m_partyExpShare,
        "partial party-option result mutated EXP state");
    Require(connection.QueueBytes(std::vector<u8>(disabled.begin() + 5, disabled.end())),
        "could not complete party-option result");
    const GameModePacketLoopResult disabledResult = ProcessGameModePacketLoop(mode, connection, router, 1);
    Require(disabledResult.packetsProcessed == 1 && !g_session.m_partyExpShare
        && !g_session.m_itemCollectType && g_session.m_itemDivType,
        "six-byte party-option success did not update EXP while retaining item cache");

    const std::vector<u8> enabled{0x01,0x01,0x01,0x00,0x00,0x00};
    Require(connection.QueueBytes(enabled), "could not queue successful party-option result");
    const GameModePacketLoopResult enabledResult = ProcessGameModePacketLoop(mode, connection, router, 1);
    Require(enabledResult.packetsProcessed == 1 && g_session.m_partyExpShare
        && !g_session.m_itemCollectType && g_session.m_itemDivType,
        "party-option EXP enable result changed the unrelated item cache");

    g_session.m_partyExpShare = savedExpShare;
    g_session.m_itemCollectType = savedItemCollectType;
    g_session.m_itemDivType = savedItemDivType;
    g_windowMgr.ClearChatEvents();
    std::cout << "{\"case_id\":\"party_option_change_result_production_receive\",\"contract_id\":\"PK46.02\",\"fixture_id\":\"PK46.02-map-party-option-change-request\",\"denial_keeps_cache\":true,\"partial_frame_buffered\":true,\"six_byte_response_updates_exp_only\":true,\"outcome\":\"PASS\"}" << std::endl;
}

void PartyMemberRemovedProductionReceiveReplay()
{
    SetCase("party_member_removed_production_receive", "PK46.02", "PK46.02-map-party-member-removal-notification",
        "ProcessGameModePacketLoop -> production packet router -> HandlePartyMemberRemoved -> CSession");
    const std::vector<FRIEND_INFO> savedMembers(g_session.GetPartyList().begin(), g_session.GetPartyList().end());
    const std::string savedPartyName = g_session.m_partyName;
    g_session.ClearParty();
    g_session.m_partyName = "Replay party";
    FRIEND_INFO member{};
    member.isValid = 1;
    member.AID = 77;
    member.characterName = "Removed";
    g_session.m_partyList.push_back(member);
    g_windowMgr.ClearChatEvents();
    CGameMode mode;
    TestConnection connection;
    CGameModePacketRouter& router = GetProductionGameModePacketRouter();
    router.Clear();
    RegisterDefaultGameModePacketHandlers(router);

    std::vector<u8> removed(31, 0);
    removed[0] = 0x05;
    removed[1] = 0x01;
    std::copy_n("Removed", 7, removed.begin() + 6);
    Require(connection.QueueBytes(removed), "could not queue party-member removal notification");
    const GameModePacketLoopResult removedResult = ProcessGameModePacketLoop(mode, connection, router, 1);
    Require(removedResult.packetsProcessed == 1 && removedResult.packetsDispatched == 1
        && g_session.GetPartyList().empty() && g_session.m_partyName == "Replay party",
        "production 0x0105 notification did not remove only the named party member");

    member.characterName = "Retained";
    g_session.m_partyList.push_back(member);
    removed[30] = 2;
    std::fill(removed.begin() + 6, removed.begin() + 30, 0);
    std::copy_n("Retained", 8, removed.begin() + 6);
    Require(connection.QueueBytes(removed), "could not queue rejected party-leave notification");
    const GameModePacketLoopResult rejectedResult = ProcessGameModePacketLoop(mode, connection, router, 1);
    Require(rejectedResult.packetsProcessed == 1 && g_session.GetPartyList().size() == 1
        && g_windowMgr.GetChatEvents().size() == 1,
        "0x0105 cannot-leave result changed membership or omitted its system notice");

    g_session.ClearParty();
    g_session.m_partyList.insert(g_session.m_partyList.end(), savedMembers.begin(), savedMembers.end());
    g_session.m_partyName = savedPartyName;
    g_windowMgr.ClearChatEvents();
    std::cout << "{\"case_id\":\"party_member_removed_production_receive\",\"contract_id\":\"PK46.02\",\"fixture_id\":\"PK46.02-map-party-member-removal-notification\",\"outcome\":\"PASS\",\"assertions\":\"production route removes named member; cannot-leave result preserves membership and records system notice\"}" << std::endl;
}

void UnequipAcknowledgementCorrelationReplay()
{
    SetCase("unequip_ack_zero_index_correlation", "PK46.02", "PK46.02-map-unequip-item-request",
        "ProcessGameModePacketLoop -> production packet router -> HandleUnequipItemAck");
    CGameMode mode;
    const u32 savedPendingIndex = mode.m_waitingTakeoffEquipAck;
    mode.m_waitingTakeoffEquipAck = 123;
    TestConnection connection;
    CGameModePacketRouter& router = GetProductionGameModePacketRouter();
    router.Clear();
    RegisterDefaultGameModePacketHandlers(router);

    const std::vector<u8> uncorrelatedFailure{0xac, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    Require(connection.QueueBytes(uncorrelatedFailure), "could not queue zero-index unequip failure acknowledgement");
    const GameModePacketLoopResult zeroIndexResult = ProcessGameModePacketLoop(mode, connection, router, 1);
    Require(zeroIndexResult.packetsProcessed == 1 && zeroIndexResult.packetsDispatched == 1
        && mode.m_waitingTakeoffEquipAck == 123,
        "zero-index unequip failure acknowledgement cleared an unrelated pending item index");

    const std::vector<u8> matchingFailure{0xac, 0x00, 123, 0x00, 0x00, 0x00, 0x00};
    Require(connection.QueueBytes(matchingFailure), "could not queue matching unequip failure acknowledgement");
    const GameModePacketLoopResult matchingResult = ProcessGameModePacketLoop(mode, connection, router, 1);
    Require(matchingResult.packetsProcessed == 1 && mode.m_waitingTakeoffEquipAck == 0,
        "matching unequip acknowledgement did not clear the pending item index");

    mode.m_waitingTakeoffEquipAck = savedPendingIndex;
    std::cout << "{\"case_id\":\"unequip_ack_zero_index_correlation\",\"contract_id\":\"PK46.02\",\"fixture_id\":\"PK46.02-map-unequip-item-request\",\"outcome\":\"PASS\",\"assertions\":\"zero-index ack preserves unrelated pending item; matching ack clears it\"}" << std::endl;
}

void TradeLockProductionSendReplay()
{
    SetCase("trade_lock_production_send", "PK46.02", "PK01.02-final-00eb-trade-lock",
        "Commerce::Action(lockTrade) -> production CRagConnection::SendPacket");
    const ro::commerce::State savedState = ro::commerce::state;
    ro::commerce::Reset();
    ro::commerce::state.tradeActive = true;
    CRagConnection* const connection = CRagConnection::instance();
    connection->Disconnect();
    const ro::commerce::Command lock{"lockTrade"};
    Require(!ro::commerce::Action(lock) && !ro::commerce::state.lockPending,
        "failed trade-lock send entered pending state");

    LoopbackPeer peer(*connection);
    Require(ro::commerce::Action(lock) && ro::commerce::state.lockPending,
        "valid trade-lock action did not send or enter pending state");
    const std::vector<u8> captured = peer.ReceiveExact(2);
    Require(captured == std::vector<u8>({0xeb, 0x00}),
        "production trade-lock action did not emit the selected two-byte frame");
    Require(!ro::commerce::Action(lock) && ro::commerce::state.lockPending,
        "duplicate trade-lock action was not suppressed while pending");

    connection->Disconnect();
    ro::commerce::state = savedState;
    std::cout << "{\"case_id\":\"trade_lock_production_send\",\"contract_id\":\"PK46.02\",\"fixture_id\":\"PK01.02-final-00eb-trade-lock\",\"outcome\":\"PASS\",\"assertions\":\"send failure does not pend; successful action emits 2-byte frame and pends; duplicate is suppressed\"}" << std::endl;
}

void TradeAcceptDeclineProductionSendReplay()
{
    SetCase("trade_accept_decline_production_send", "PK46.02", "PK01.02-final-00e6-trade-ack",
        "Commerce::Action(tradeReply) -> production CRagConnection::SendPacket");
    const ro::commerce::State savedState = ro::commerce::state;
    CRagConnection* const connection = CRagConnection::instance();
    connection->Disconnect();
    ro::commerce::Reset();
    ro::commerce::state.tradeIncoming = true;
    ro::commerce::state.partnerId = 77;
    ro::commerce::state.tradePeers.emplace(77, ro::commerce::TradePeer{"Peer", 1});
    ro::commerce::Command reply{};
    reply.action = "tradeReply";
    reply.amount = 1;
    Require(!ro::commerce::Action(reply) && ro::commerce::state.tradeIncoming
        && !ro::commerce::state.tradeWaiting,
        "failed trade-accept send changed invitation state");

    {
        LoopbackPeer peer(*connection);
        Require(ro::commerce::Action(reply) && !ro::commerce::state.tradeIncoming
            && ro::commerce::state.tradeWaiting,
            "successful trade acceptance did not enter pending state");
        Require(peer.ReceiveExact(3) == std::vector<u8>({0xe6, 0x00, 0x03}),
            "trade acceptance did not emit the selected three-byte 0x00E6 frame");
    }

    connection->Disconnect();
    ro::commerce::Reset();
    ro::commerce::state.tradeIncoming = true;
    reply.amount = 0;
    {
        LoopbackPeer peer(*connection);
        Require(ro::commerce::Action(reply) && !ro::commerce::state.tradeIncoming,
            "successful trade decline did not clear the incoming invitation");
        Require(peer.ReceiveExact(3) == std::vector<u8>({0xe6, 0x00, 0x04}),
            "trade decline did not emit the selected three-byte 0x00E6 frame");
    }
    connection->Disconnect();
    ro::commerce::state = savedState;
    std::cout << "{\"case_id\":\"trade_accept_decline_production_send\",\"contract_id\":\"PK46.02\",\"fixture_id\":\"PK01.02-final-00e6-trade-ack\",\"outcome\":\"PASS\",\"assertions\":\"failed accept preserves invite; accept emits type 3 and enters pending; decline emits type 4 and clears invite\"}" << std::endl;
}

void TradeCommitProductionSendReplay()
{
    SetCase("trade_commit_production_send", "PK46.02", "PK01.02-final-00ef-trade-commit",
        "Commerce::Action(commitTrade) -> production CRagConnection::SendPacket");
    const ro::commerce::State savedState = ro::commerce::state;
    ro::commerce::Reset();
    ro::commerce::state.tradeActive = true;
    ro::commerce::state.localLocked = true;
    ro::commerce::state.remoteLocked = false;
    const ro::commerce::Command commit{"commitTrade"};
    Require(!ro::commerce::Action(commit) && !ro::commerce::state.commitPending,
        "trade commit was sent before the remote side locked");

    ro::commerce::state.remoteLocked = true;
    CRagConnection* const connection = CRagConnection::instance();
    connection->Disconnect();
    Require(!ro::commerce::Action(commit) && !ro::commerce::state.commitPending,
        "failed trade-commit send entered pending state");

    {
        LoopbackPeer peer(*connection);
        Require(ro::commerce::Action(commit) && ro::commerce::state.commitPending,
            "valid two-sided trade commit did not enter pending state");
        Require(peer.ReceiveExact(2) == std::vector<u8>({0xef, 0x00}),
            "trade commit did not emit the selected two-byte frame");
        Require(!ro::commerce::Action(commit) && ro::commerce::state.commitPending,
            "duplicate trade commit was not suppressed while pending");
    }

    connection->Disconnect();
    ro::commerce::state = savedState;
    std::cout << "{\"case_id\":\"trade_commit_production_send\",\"contract_id\":\"PK46.02\",\"fixture_id\":\"PK01.02-final-00ef-trade-commit\",\"outcome\":\"PASS\",\"assertions\":\"remote lock required; failed send does not pend; valid send emits 2-byte frame and duplicate suppressed\"}" << std::endl;
}

void TradeCancelProductionSendReplay()
{
    SetCase("trade_cancel_production_send", "PK46.02", "PK01.02-final-00ed-trade-cancel",
        "Commerce::Action(close trade) -> production CRagConnection::SendPacket");
    const ro::commerce::State savedState = ro::commerce::state;
    ro::commerce::Reset();
    ro::commerce::state.tradeWaiting = true;
    ro::commerce::state.partnerId = 77;
    ro::commerce::Command close{};
    close.action = "close";
    close.to = "trade";
    CRagConnection* const connection = CRagConnection::instance();
    connection->Disconnect();
    Require(!ro::commerce::Action(close) && ro::commerce::state.tradeWaiting,
        "failed trade-cancel send cleared the pending outgoing trade");

    {
        LoopbackPeer peer(*connection);
        Require(ro::commerce::Action(close) && !ro::commerce::state.tradeWaiting
            && !ro::commerce::state.tradeActive,
            "successful trade cancellation did not reset outgoing trade state");
        Require(peer.ReceiveExact(2) == std::vector<u8>({0xed, 0x00}),
            "outgoing trade cancellation did not emit the selected two-byte 0x00ED frame");
    }
    connection->Disconnect();
    ro::commerce::state = savedState;
    std::cout << "{\"case_id\":\"trade_cancel_production_send\",\"contract_id\":\"PK46.02\",\"fixture_id\":\"PK01.02-final-00ed-trade-cancel\",\"outcome\":\"PASS\",\"assertions\":\"failed send retains outgoing trade; successful send emits two bytes and resets it\"}" << std::endl;
}

void TradeAddItemProductionSendReplay()
{
    SetCase("trade_add_item_production_send", "PK46.02", "PK01.02-final-00e8-trade-item",
        "Commerce::Action(offerTrade) -> production CRagConnection::SendPacket");
    const ro::commerce::State savedState = ro::commerce::state;
    constexpr unsigned int itemIndex = 65000;
    Require(g_session.GetInventoryItemByIndex(itemIndex) == nullptr,
        "trade replay fixture item index is already present");
    ITEM_INFO item{};
    item.m_itemIndex = itemIndex;
    item.m_num = 5;
    g_session.SetInventoryItem(item);
    ro::commerce::Reset();
    ro::commerce::state.tradeActive = true;
    ro::commerce::Command offer{};
    offer.action = "offerTrade";
    offer.index = itemIndex;
    offer.amount = 2;
    CRagConnection* const connection = CRagConnection::instance();
    connection->Disconnect();
    Require(!ro::commerce::Action(offer) && !ro::commerce::state.tradeAddPending,
        "failed trade-item send entered pending state");

    {
        LoopbackPeer peer(*connection);
        Require(ro::commerce::Action(offer) && ro::commerce::state.tradeAddPending
            && ro::commerce::state.pendingTrade.item.m_itemIndex == itemIndex
            && ro::commerce::state.pendingTrade.amount == 2,
            "successful trade-item offer did not retain the pending item and amount");
        const std::vector<u8> expected{0xe8, 0x00, 0xe8, 0xfd, 0x02, 0x00, 0x00, 0x00};
        Require(peer.ReceiveExact(expected.size()) == expected,
            "trade-item offer did not emit the selected eight-byte frame");
        Require(!ro::commerce::Action(offer) && ro::commerce::state.tradeAddPending,
            "duplicate trade-item offer was not suppressed while awaiting acknowledgement");
    }

    connection->Disconnect();
    ro::commerce::state = savedState;
    g_session.RemoveInventoryItem(itemIndex, item.m_num);
    std::cout << "{\"case_id\":\"trade_add_item_production_send\",\"contract_id\":\"PK46.02\",\"fixture_id\":\"PK01.02-final-00e8-trade-item\",\"outcome\":\"PASS\",\"assertions\":\"failed send does not pend; valid offer emits eight bytes and records item; duplicate while pending suppressed\"}" << std::endl;
}

void WarpSkillSelectionProductionSendReplay()
{
    SetCase("warp_skill_selection_production_send", "PK46.02", "PK01.02-final-011b-use-skill-map",
        "CGameMode::SendWarpMapSelection -> production SendUseSkillMapPacket");
    CGameMode mode;
    mode.m_warpSkillId = 0x1234;
    mode.m_warpMapDestinations = {"prontera", "payon"};
    CRagConnection* const connection = CRagConnection::instance();
    connection->Disconnect();
    Require(!mode.SendWarpMapSelection("prontera") && mode.m_warpSkillId == 0x1234
        && mode.m_warpMapDestinations.size() == 2,
        "failed warp-map send cleared the pending skill selection");
    Require(!mode.SendWarpMapSelection("geffen") && mode.m_warpSkillId == 0x1234,
        "warp-map sender accepted a destination outside the selected list");

    {
        LoopbackPeer peer(*connection);
        Require(mode.SendWarpMapSelection("prontera") && mode.m_warpSkillId == 0
            && mode.m_warpMapDestinations.empty(),
            "successful warp-map send did not clear the pending skill selection");
        std::vector<u8> expected(20, 0);
        expected[0] = 0x1b;
        expected[1] = 0x01;
        expected[2] = 0x34;
        expected[3] = 0x12;
        std::copy_n("prontera", 8, expected.begin() + 4);
        Require(peer.ReceiveExact(expected.size()) == expected,
            "warp-skill selection did not emit the selected 20-byte frame");
    }
    connection->Disconnect();
    std::cout << "{\"case_id\":\"warp_skill_selection_production_send\",\"contract_id\":\"PK46.02\",\"fixture_id\":\"PK01.02-final-011b-use-skill-map\",\"outcome\":\"PASS\",\"assertions\":\"failed send and invalid destination preserve pending selection; valid 20-byte send clears it\"}" << std::endl;
}

void CloseVendingProductionSendReplay()
{
    SetCase("close_vending_production_send", "PK46.02", "PK01.02-final-012e-close-vending",
        "Commerce::Action(close vending) -> production CRagConnection::SendPacket");
    const ro::commerce::State savedState = ro::commerce::state;
    ro::commerce::Reset();
    ro::commerce::state.vendingActive = true;
    ro::commerce::state.vendingPending = true;
    ro::commerce::state.draft.emplace_back();
    ro::commerce::Command close{};
    close.action = "close";
    close.to = "vending";
    CRagConnection* const connection = CRagConnection::instance();
    connection->Disconnect();
    Require(!ro::commerce::Action(close) && ro::commerce::state.vendingActive
        && ro::commerce::state.vendingPending && ro::commerce::state.draft.size() == 1,
        "failed vending-close send mutated the active shop state");

    {
        LoopbackPeer peer(*connection);
        Require(ro::commerce::Action(close) && !ro::commerce::state.vendingActive
            && !ro::commerce::state.vendingPending && ro::commerce::state.draft.empty(),
            "successful vending close did not reset the local shop state");
        Require(peer.ReceiveExact(2) == std::vector<u8>({0x2e, 0x01}),
            "vending close did not emit the selected two-byte 0x012E frame");
    }
    connection->Disconnect();
    ro::commerce::state = savedState;
    std::cout << "{\"case_id\":\"close_vending_production_send\",\"contract_id\":\"PK46.02\",\"fixture_id\":\"PK01.02-final-012e-close-vending\",\"outcome\":\"PASS\",\"assertions\":\"failed send preserves active shop; successful close emits two-byte request and clears local state\"}" << std::endl;
}

void VendingPurchaseProductionSendReplay()
{
    SetCase("vending_purchase_production_send", "PK46.02", "PK01.02-final-0134-vending-buy",
        "Commerce::Action(buy) -> production CRagConnection::SendPacket");
    const ro::commerce::State savedState = ro::commerce::state;
    ro::commerce::Reset();
    ro::commerce::state.sellerId = 0x11223344;
    ro::commerce::Offer listed{};
    listed.item.m_itemIndex = 0x1234;
    listed.amount = 7;
    ro::commerce::state.shopItems.push_back(listed);
    ro::commerce::Command buy{};
    buy.action = "buy";
    buy.index = 0x1234;
    buy.amount = 2;
    CRagConnection* const connection = CRagConnection::instance();
    connection->Disconnect();
    Require(!ro::commerce::Action(buy), "vending purchase succeeded without an open connection");
    Require(!ro::commerce::state.shopItems.empty()
        && ro::commerce::state.shopItems.front().amount == 7,
        "failed vending purchase changed the local shop stock");
    buy.amount = 8;
    Require(!ro::commerce::Action(buy), "vending purchase exceeded listed stock");
    buy.amount = 2;

    {
        LoopbackPeer peer(*connection);
        Require(ro::commerce::Action(buy), "valid vending purchase was rejected by the production action");
        const std::vector<u8> expected{
            0x34, 0x01, 0x0c, 0x00, 0x44, 0x33, 0x22, 0x11,
            0x02, 0x00, 0x34, 0x12
        };
        Require(peer.ReceiveExact(expected.size()) == expected,
            "vending purchase did not emit the selected 12-byte transaction frame");
        Require(ro::commerce::state.shopItems.front().amount == 7,
            "vending purchase optimistically changed local stock before a server response");
    }
    connection->Disconnect();
    ro::commerce::state = savedState;
    std::cout << "{\"case_id\":\"vending_purchase_production_send\",\"contract_id\":\"PK46.02\",\"fixture_id\":\"PK01.02-final-0134-vending-buy\",\"outcome\":\"PASS\",\"assertions\":\"stock guard enforced; failed send preserves stock; valid purchase emits 12-byte frame without optimistic decrement\"}" << std::endl;
}

void ExitToWindowsProductionSendReplay()
{
    SetCase("exit_to_windows_production_send", "PK46.02", "PK01.02-final-018a-quit-game",
        "CGameMode::SendMsg(GameMsg_RequestExitToWindows) -> production sender");
    ClearPendingDisconnectAction();
    CGameMode mode;
    CRagConnection* const connection = CRagConnection::instance();
    connection->Disconnect();
    Require(mode.SendMsg(CGameMode::GameMsg_RequestExitToWindows, 0, 0, 0) == 0
        && GetPendingDisconnectAction() == PendingDisconnectAction::None,
        "failed exit request left a pending disconnect action");

    {
        LoopbackPeer peer(*connection);
        Require(mode.SendMsg(CGameMode::GameMsg_RequestExitToWindows, 0, 0, 0) == 1
            && GetPendingDisconnectAction() == PendingDisconnectAction::ExitToWindows,
            "successful exit request did not enter the pending disconnect state");
        Require(peer.ReceiveExact(sizeof(PACKET_CZ_QUITGAME))
                == std::vector<u8>({0x8a, 0x01, 0x00, 0x00}),
            "exit-to-Windows request did not emit the selected four-byte frame");
        Require(mode.SendMsg(CGameMode::GameMsg_RequestExitToWindows, 0, 0, 0) == 0
            && GetPendingDisconnectAction() == PendingDisconnectAction::ExitToWindows,
            "duplicate exit request was not suppressed while pending");
    }
    connection->Disconnect();
    ClearPendingDisconnectAction();
    std::cout << "{\"case_id\":\"exit_to_windows_production_send\",\"contract_id\":\"PK46.02\",\"fixture_id\":\"PK01.02-final-018a-quit-game\",\"outcome\":\"PASS\",\"assertions\":\"failed send clears pending action; successful send emits four-byte frame; duplicate while pending suppressed\"}" << std::endl;
}

void ChangeCartProductionSendReplay()
{
    SetCase("change_cart_production_send", "PK46.02", "PK01.02-final-01af-change-cart",
        "UISelectCartWnd::ActivateOption -> GameMsg_RequestChangeCart -> production sender");
    CGameMode mode;
    CRagConnection* const connection = CRagConnection::instance();
    connection->Disconnect();
    Require(mode.SendMsg(CGameMode::GameMsg_RequestChangeCart, 3, 0, 0) == 0,
        "change-cart sender reported success without an open connection");

    {
        LoopbackPeer peer(*connection);
        Require(mode.SendMsg(CGameMode::GameMsg_RequestChangeCart, 0, 0, 0) == 0
            && mode.SendMsg(CGameMode::GameMsg_RequestChangeCart, 6, 0, 0) == 0,
            "change-cart sender accepted an out-of-range cart type");
        for (u16 type = 1; type <= 5; ++type) {
            Require(mode.SendMsg(CGameMode::GameMsg_RequestChangeCart, type, 0, 0) == 1,
                "valid change-cart type was rejected");
            const std::vector<u8> expected{0xaf, 0x01, static_cast<u8>(type), 0x00};
            Require(peer.ReceiveExact(expected.size()) == expected,
                "change-cart sender emitted an incorrect four-byte frame");
        }
    }
    connection->Disconnect();
    std::cout << "{\"case_id\":\"change_cart_production_send\",\"contract_id\":\"PK46.02\",\"fixture_id\":\"PK01.02-final-01af-change-cart\",\"outcome\":\"PASS\",\"assertions\":\"closed connection fails; types 0 and 6 rejected; types 1 through 5 emit four-byte frames\"}" << std::endl;
}

void FriendReplyProductionSendReplay()
{
    SetCase("friend_reply_production_send", "PK46.02", "PK01.02-final-0208-friend-reply",
        "Commerce::Action(prompt friend) -> production CRagConnection::SendPacket");
    const ro::commerce::State savedState = ro::commerce::state;
    ro::commerce::Reset();
    ro::commerce::state.promptKind = "friend";
    ro::commerce::state.promptId = 0x11223344;
    ro::commerce::state.promptChar = 0x55667788;
    ro::commerce::Command reply{};
    reply.action = "prompt";
    reply.amount = 1;
    CRagConnection* const connection = CRagConnection::instance();
    connection->Disconnect();
    Require(!ro::commerce::Action(reply) && ro::commerce::state.promptKind == "friend",
        "failed friend-reply send cleared the pending prompt");

    {
        LoopbackPeer peer(*connection);
        Require(ro::commerce::Action(reply) && ro::commerce::state.promptKind.empty(),
            "successful friend acceptance did not clear the prompt");
        const std::vector<u8> expected{
            0x08, 0x02, 0x44, 0x33, 0x22, 0x11, 0x88, 0x77,
            0x66, 0x55, 0x01, 0x00, 0x00, 0x00
        };
        Require(peer.ReceiveExact(expected.size()) == expected,
            "friend acceptance did not emit the selected 14-byte frame");
    }

    connection->Disconnect();
    ro::commerce::state.promptKind = "friend";
    ro::commerce::state.promptId = 0x01020304;
    ro::commerce::state.promptChar = 0xA1B2C3D4;
    reply.amount = 0;
    {
        LoopbackPeer peer(*connection);
        Require(ro::commerce::Action(reply) && ro::commerce::state.promptKind.empty(),
            "successful friend decline did not clear the prompt");
        const std::vector<u8> expected{
            0x08, 0x02, 0x04, 0x03, 0x02, 0x01, 0xd4, 0xc3,
            0xb2, 0xa1, 0x00, 0x00, 0x00, 0x00
        };
        Require(peer.ReceiveExact(expected.size()) == expected,
            "friend decline did not emit the selected 14-byte frame");
    }
    connection->Disconnect();
    ro::commerce::state = savedState;
    std::cout << "{\"case_id\":\"friend_reply_production_send\",\"contract_id\":\"PK46.02\",\"fixture_id\":\"PK01.02-final-0208-friend-reply\",\"outcome\":\"PASS\",\"assertions\":\"failed send preserves prompt; accept/decline emit 14-byte frames with IDs and answer values\"}" << std::endl;
}

bool ReplaySendGlobalChatMessage(const char* playerName, const std::string& message);

void GlobalChatProductionSendReplay()
{
    SetCase("global_chat_production_send", "PK46.02", "PK01.02-final-global-chat",
        "GameMode global-chat caller -> SendGlobalChatMessage -> production CRagConnection::SendPacket");
    CRagConnection* const connection = CRagConnection::instance();
    connection->Disconnect();
    Require(!ReplaySendGlobalChatMessage("Alice", ""),
        "global chat sender accepted an empty message");
    Require(!ReplaySendGlobalChatMessage("", "hello"),
        "global chat sender accepted an empty player name");
    const std::string oversized(65523, 'x');
    Require(!ReplaySendGlobalChatMessage("Alice", oversized),
        "global chat sender accepted a frame larger than the wire length field");
    Require(!ReplaySendGlobalChatMessage("Alice", "hello"),
        "global chat sender reported success without an open connection");

    const u16 opcode = ro::net::GetActiveMapGameplaySendProfile().globalMessage;
    const std::string payload = "Alice : hello";
    std::vector<u8> expected(4 + payload.size() + 1, 0);
    expected[0] = static_cast<u8>(opcode & 0xff);
    expected[1] = static_cast<u8>(opcode >> 8);
    expected[2] = static_cast<u8>(expected.size() & 0xff);
    expected[3] = static_cast<u8>(expected.size() >> 8);
    std::copy(payload.begin(), payload.end(), expected.begin() + 4);
    {
        LoopbackPeer peer(*connection);
        Require(ReplaySendGlobalChatMessage("Alice", "hello"),
            "valid global chat message was rejected by the production sender");
        Require(peer.ReceiveExact(expected.size()) == expected,
            "global chat sender emitted an incorrect profile opcode or variable frame");
    }
    connection->Disconnect();
    std::cout << "{\"case_id\":\"global_chat_production_send\",\"contract_id\":\"PK46.02\",\"fixture_id\":\"PK01.02-final-global-chat\",\"outcome\":\"PASS\",\"assertions\":\"empty name/message, oversized frame and disconnected send rejected; profile opcode, length, payload and terminator captured\"}" << std::endl;
}

#include "MapReceivePreambleReplay.inc"
#include "MapTransitionLifetimeReplay.inc"
#include "ModeLoopHandoffReplay.inc"
#include "CartItemWireFieldsReplay.inc"
#include "InventoryMapEntryReplay.inc"
#include "InventoryMutationAckReplay.inc"
#include "ItemUseFeedbackReplay.inc"
#include "RecoveryDeltaReplay.inc"
#include "PetLifecycleReplay.inc"
#include "CorpsePacketReplay.inc"
#include "ChatWindowStateReplay.inc"
#include "ChatNoticeRoutingReplay.inc"
#include "ChatNoticeProducerReplay.inc"
#include "ChatPrimaryViewReplay.inc"
#include "ChatPrimarySpeechReplay.inc"
#include "ChatDetachedViewReplay.inc"
#include "ChatBattleOptionsReplay.inc"
#include "ChatScrollBarReplay.inc"
#include "ChatSourceLabelsReplay.inc"
#include "ChatRestoreReplay.inc"
#include "ChatRoomReplay.inc"
#include "ChatRoomAckReplay.inc"
#include "ChatRoomEntryReplay.inc"
#include "ChatRoomSettingsReplay.inc"
#include "ChatRoomActionsReplay.inc"
#include "WhisperReplay.inc"
#include "AutoMessageReplay.inc"
#include "FriendOptionsReplay.inc"
#include "ChatSubmissionReplay.inc"
#include "InventoryItemFieldsReplay.inc"
#include "CartItemLifecycleReplay.inc"
#include "CharacterNameReplay.inc"
#include "RentalItemReplay.inc"
#include "NpcShopTransactionReplay.inc"
#include "NpcClassChangeReplay.inc"
#include "CompassMarkerReplay.inc"
#include "NpcDialogueActionsReplay.inc"
#include "NpcProgressReplay.inc"
#include "NpcShowEfstReplay.inc"
#include "StatusSummary214Replay.inc"
#include "StatusResourceWidthsReplay.inc"
#include "Pk24ActorOrderingReplay.inc"
#include "MemoPacketReplay.inc"
#include "MonsterEstimateReplay.inc"
#include "AutoSpellChoiceReplay.inc"
#include "SkillChoiceLifecycleReplay.inc"
#include "ui/UITransBalloonText.h"
#include "StarSkillReplay.inc"
#include "PetFeedActionReplay.inc"
#include "PetFollowWireReplay.inc"

int main(int argc,char** argv){try{
#if RO_ENABLE_QT6_UI
    qputenv("QT_QPA_PLATFORM","offscreen");
#ifdef RO_PACKET_QT_PLATFORM_PLUGINS
    if(qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM_PLUGIN_PATH"))
        qputenv("QT_QPA_PLATFORM_PLUGIN_PATH",RO_PACKET_QT_PLATFORM_PLUGINS);
#endif
    // Real acknowledgement failures open chat and measure its text. Supply
    // Qt's font/runtime prerequisites even though this is a packet harness.
    QGuiApplication app(argc,argv);
#else
    (void)argc;(void)argv;
#endif
    Require(CConnection::Startup(),"socket startup failed");
    if(argc>1 && std::string(argv[1])=="--pet-lifecycle-only") {PetLifecycleReplay();CConnection::Cleanup();return 0;}
    if(argc>1 && std::string(argv[1])=="--world-entry-handoff-only") {ModeLoopHandoffReplay();CConnection::Cleanup();return 0;}
    if(argc>1 && std::string(argv[1])=="--map-cell-load-only") {MapCellLoadLifecycleReplay();CConnection::Cleanup();return 0;}
    if(argc>1 && std::string(argv[1])=="--standing-resurrection-only") {StandingResurrectionReplay();CConnection::Cleanup();return 0;}
    if(argc>1 && std::string(argv[1])=="--siege-emblem-only") {SiegeActorEmblemReplay();CConnection::Cleanup();return 0;}
    if(argc>1 && std::string(argv[1])=="--corpse-gr2-native") { CorpseNativePacketReplay();CConnection::Cleanup();return 0; }
    if(argc>1 && std::string(argv[1])=="--whisper-only") {WhisperReplay();CConnection::Cleanup();return 0;}
    if(argc>1 && std::string(argv[1])=="--auto-message-only") {AutoMessageReplay();CConnection::Cleanup();return 0;}
    if(argc>1 && std::string(argv[1])=="--chat-submission-only") {ChatSubmissionReplay();CConnection::Cleanup();return 0;}
    if(argc>1 && std::string(argv[1])=="--friend-options-only") {FriendOptionsReplay();CConnection::Cleanup();return 0;}
    if(argc>1 && std::string(argv[1])=="--whisper-preferences-write") {WhisperPreferencesReplay("write");CConnection::Cleanup();return 0;}
    if(argc>1 && std::string(argv[1])=="--whisper-preferences-read") {WhisperPreferencesReplay("read");CConnection::Cleanup();return 0;}
    if(argc>1 && std::string(argv[1])=="--whisper-preferences-defaults") {WhisperPreferencesReplay("defaults");CConnection::Cleanup();return 0;}
    if(argc>1 && std::string(argv[1])=="--chat-room-only") {ChatRoomReplay();ChatRoomAckReplay();ChatRoomEntryReplay();ChatRoomSettingsReplay();ChatRoomActionsReplay();CConnection::Cleanup();return 0;}
    if(argc>1 && std::string(argv[1])=="--chat-restore-only") {ChatWindowRestoreReplay();CConnection::Cleanup();return 0;}
    if(argc>1 && std::string(argv[1])=="--chat-window-write") {ChatWindowSettingsProcess(true);CConnection::Cleanup();return 0;}
    if(argc>1 && std::string(argv[1])=="--chat-window-read") {ChatWindowSettingsProcess(false);CConnection::Cleanup();return 0;}
    if(argc>1 && std::string(argv[1])=="--chat-labels-only") {ChatSourceLabelsReplay();CConnection::Cleanup();return 0;}
    if(argc>1 && std::string(argv[1])=="--chat-scroll-only") {ChatScrollBarReplay();CConnection::Cleanup();return 0;}
    if(argc>1 && std::string(argv[1])=="--chat-options-only") {ChatBattleOptionsReplay();CConnection::Cleanup();return 0;}
    if(argc>1 && std::string(argv[1])=="--chat-options-write") {ChatBattleOptionsSettingsProcess(true);CConnection::Cleanup();return 0;}
    if(argc>1 && std::string(argv[1])=="--chat-options-read") {ChatBattleOptionsSettingsProcess(false);CConnection::Cleanup();return 0;}
    if(argc>1 && std::string(argv[1])=="--chat-detached-only") { ChatDetachedViewReplay(); CConnection::Cleanup();return 0; }
    if(argc>1 && std::string(argv[1])=="--chat-primary-only") { ChatPrimaryViewReplay(); ChatPrimarySpeechReplay(); CConnection::Cleanup();return 0; }
    if(argc>1 && std::string(argv[1])=="--chat-notice-only") { ChatNoticeRoutingReplay(); ChatNoticeProducerReplay(); CConnection::Cleanup();return 0; }
    if(argc>1 && std::string(argv[1])=="--chat-state-only") { ChatWindowStateReplay();CConnection::Cleanup();return 0; }
    if(argc>1 && std::string(argv[1])=="--corpse-only") { CorpsePacketReplay();CConnection::Cleanup();return 0; }
    if(argc>1 && std::string(argv[1])=="--npc-class-change-only") { NpcClassChangeReplay();CConnection::Cleanup();return 0; }
    if(argc>1 && std::string(argv[1])=="--compass-only") { CompassMarkerReplay();CConnection::Cleanup();return 0; }
    if(argc>1 && std::string(argv[1])=="--npc-dialogue-actions-only") { NpcDialogueActionsReplay();CConnection::Cleanup();return 0; }
    if(argc>1 && std::string(argv[1])=="--npc-progress-only") { NpcProgressReplay();CConnection::Cleanup();return 0; }
    if(argc>1 && std::string(argv[1])=="--npc-show-efst-only") { NpcShowEfstReplay();CConnection::Cleanup();return 0; }
    if(argc>1 && std::string(argv[1])=="--status-summary-0214-only") { StatusSummary214Replay();CConnection::Cleanup();return 0; }
    if(argc>1 && std::string(argv[1])=="--status-resource-widths-only") { StatusResourceWidthsReplay();CConnection::Cleanup();return 0; }
    if(argc>1 && std::string(argv[1])=="--actor-resource-ordering-only") { Pk24ActorOrderingReplay();CConnection::Cleanup();return 0; }
    if(argc>1 && std::string(argv[1])=="--memo-result-only") { MemoPacketReplay();CConnection::Cleanup();return 0; }
    if(argc>1 && std::string(argv[1])=="--monster-estimate-only") { MonsterEstimateReplay();CConnection::Cleanup();return 0; }
    if(argc>1 && std::string(argv[1])=="--autospell-choice-only") { AutoSpellChoiceProductionReplay();CConnection::Cleanup();return 0; }
    if(argc>1 && std::string(argv[1])=="--skill-choice-lifecycle-only") { SkillChoiceLifecycleProductionReplay();CConnection::Cleanup();return 0; }
    if(argc>1 && std::string(argv[1])=="--star-skill-only") { StarSkillProductionReplay();CConnection::Cleanup();return 0; }
    if(argc>1 && std::string(argv[1])=="--pet-feed-action-only") { PetFeedActionReplay();CConnection::Cleanup();return 0; }
    if(argc>1 && std::string(argv[1])=="--pet-follow-wire-only") { PetFollowWireReplay();CConnection::Cleanup();return 0; }
    if (argc > 1 && std::string(argv[1]) == "--npc-show-efst-map-exit") {
        // OnExit clears process-wide windows and session state.
        NpcShowEfstMapExitReplay();
        CConnection::Cleanup();
        return 0;
    }
    NpcShopTransactionReplay();
    MapMovementProductionSendReplay();
    UseItemProductionSendReplay();
    SkillTargetProductionSendReplay();
    TakeItemProductionSendReplay();
    ChangeDirectionProductionSendReplay();
    CartOffProductionSendReplay();
    NpcCloseDialogProductionSendReplay();
    StatusIncreaseProductionSendReplay();
    NpcNextProductionSendReplay();
    NpcInputNumberProductionSendReplay();
    NpcInputStringProductionSendReplay();
    NpcContactProductionSendReplay();
    NpcMenuSelectionProductionSendReplay();
    ReturnToSavePointProductionSendReplay();
    SkillUpgradeProductionSendReplay();
    IdentifyItemCancelProductionSendReplay();
    IdentifyItemSelectionProductionSendReplay();
    ShortcutSlotUpdateProductionSendReplay();
    InspectPlayerEquipmentProductionSendReplay();
    AddFriendRequestProductionSendReplay();
    GuildInfoRefreshProductionSendReplay();
    GuildInviteProductionSendReplay();
    RejectWhisperProductionSendReplay();
    TradeRequestProductionSendReplay();
    TradeLockProductionSendReplay();
    TradeAcceptDeclineProductionSendReplay();
    TradeCommitProductionSendReplay();
    TradeCancelProductionSendReplay();
    TradeAddItemProductionSendReplay();
    WarpSkillSelectionProductionSendReplay();
    CloseVendingProductionSendReplay();
    VendingPurchaseProductionSendReplay();
    ExitToWindowsProductionSendReplay();
    ChangeCartProductionSendReplay();
    FriendReplyProductionSendReplay();
    GlobalChatProductionSendReplay();
    MailComposerProductionSendReplay();
    MailSendProductionReplay();
    MailResultProductionReceiveReplay();
    StorageCloseProductionSendReplay();
    ActionFailureProductionReceiveReplay();
    PartyInviteProductionSendReplay();
    PartyInviteReplyProductionSendReplay();
    PartyCreateProductionSendReplay();
    PartyOptionChangeProductionSendReplay();
    PartyDisbandProductionSendReplay();
    PartyInviteResultProductionReceiveReplay();
    PartyOptionChangeResultProductionReceiveReplay();
    UnequipAcknowledgementCorrelationReplay();
    PartyMemberRemovedProductionReceiveReplay();
    MapLoadEndAckProductionSendReplay();
    Require(ro::net::GetPacketSize(0x006c)==3,"0x006c fixed receive size differs from active client table");
    Require(ro::net::GetPacketSize(0x01a2)==35,"active 20080910 pet status receive size must be 35 bytes");
    Require(ro::net::GetPacketSize(0x018b)==4,"0x018b disconnect acknowledgement size differs from contract");
    Require(ro::net::GetPacketSize(0x8482)==0 && ro::net::GetPacketSize(0x8483)==0,
        "fake raw account-ID prefix identities remain registered as opcodes");
    const std::vector<u8> fixed{0x6c,0x00,0x00}; // opaque length sample: map registry clif_packetdb.hpp line 18
    {
        std::vector<u8> petStatus{0xa2,0x01,'P'};petStatus.insert(petStatus.end(),23,0);
        petStatus.insert(petStatus.end(),{0,10,0,20,0,30,0,0x34,0x12});
        std::vector<u8> coalesced=petStatus;coalesced.insert(coalesced.end(),fixed.begin(),fixed.end());
        SetCase("pet_status_35_byte_followed_by_fixed_packet");TestConnection c;LoopbackPeer peer(c);peer.Send(coalesced);WaitKernelPending(c,static_cast<int>(coalesced.size()));
        const auto actualPet=ReadOne(c);Require(actualPet==petStatus,"production framing changed the selected 35-byte pet status frame");
        Require(c.BufferedBytes()==static_cast<int>(fixed.size()),"35-byte pet frame did not leave the following fixed frame aligned");
        const auto actualNext=ReadOne(c);Require(actualNext==fixed&&c.BufferedBytes()==0,"fixed frame following pet status was lost or misaligned");
        std::cout<<"{\"case_id\":\"pet_status_35_byte_followed_by_fixed_packet\",\"contract_id\":\"PK07.PET-STATUS\",\"fixture_id\":\"PK07-01a2-selected-20080910-fields\",\"production_entry\":\"CRagConnection::Connect/RecvPacket -> TryReadPacket\",\"pet_frame_hex\":\""<<Hex(actualPet)<<"\",\"pet_frame_size\":"<<actualPet.size()<<",\"following_frame_hex\":\""<<Hex(actualNext)<<"\",\"following_frame_size\":"<<actualNext.size()<<",\"queue_empty\":"<<(c.BufferedBytes()==0?"true":"false")<<",\"outcome\":\"PASS\"}\n";
    }
    const std::vector<u8> variable{0x69,0x00,0x06,0x00,0x11,0x22}; // variable framing only
    FixedEveryBoundary(fixed);FixedCoalesced(fixed);VariableEveryBoundary(variable,fixed);
    RejectUnframedInput({0xff,0xff},fixed,"unknown_opcode_closes_without_rescan",CRagConnection::ReceiveError::UnknownOpcode);
    RejectFragmentedUnframedInput({0xff},{0xff,0x6c,0x00,0x00},"unknown_opcode_fragmented_header_closes_without_rescan",CRagConnection::ReceiveError::UnknownOpcode);
    RejectUnframedInput({0x69,0x00,0x03,0x00},fixed,"undersized_variable_closes_without_rescan",CRagConnection::ReceiveError::InvalidVariableLength);
    RejectUnframedInput({0x69,0x00,0x00,0x80},fixed,"oversized_variable_closes_without_rescan",CRagConnection::ReceiveError::InvalidVariableLength);
    RejectFragmentedUnframedInput({0x69,0x00,0x03},{0x00,0x6c,0x00,0x00},"undersized_variable_fragmented_header_closes_without_rescan",CRagConnection::ReceiveError::InvalidVariableLength);
    ProtocolErrorReconnectReset(fixed);
    EofCase(fixed,0,"eof_empty",kFixedFixture,kFixedSha);EofCase(fixed,1,"eof_partial_fixed_header",kFixedFixture,kFixedSha);
    EofCase(variable,5,"eof_partial_variable_body",kVariableFixture,kVariableSha);
    ProductionRouterDispatchReplay();
    PartyRosterReplacementReplay();
    PetStatusReplay();
    PetLifecycleReplay();
    DisconnectAckEveryBoundaryReplay();
    DisconnectAckRefusalDispatchReplay(PendingDisconnectAction::ReturnToCharSelect,"disconnect_ack_refuses_return_to_char_select");
    DisconnectAckRefusalDispatchReplay(PendingDisconnectAction::ExitToWindows,"disconnect_ack_refuses_exit_to_windows");
    OutboundTransportCapture(fixed);
    GroundSkillTextRequestReplay();
    ActionRequestBuilderReplay();
    DisconnectAckDecisionReplay();
    LoginStatusPrecedenceReplay();
    DisconnectTimeoutFallbackReplay();
    DisconnectRequestReplay();
    DisconnectChooserActivationReplay();
    ProductionDisconnectAckEofReplay();
    {SetCase("production_reconnect_reset");TestConnection c;{LoopbackPeer p(c);p.Send({0x6c});PumpUntilBuffered(c,1);}c.Disconnect();Require(!c.IsOpen()&&c.BufferedBytes()==0,"disconnect failed to clear receive queue");{LoopbackPeer p(c);p.Send(fixed);auto out=PumpUntilPacket(c);Require(out==fixed,"production reconnect failed after disconnect");VerifyRoute(out,0x006c);Report("production_reconnect_reset",kFixedFixture,kFixedSha,{fixed},Hex(fixed),Hex(out),0,c.BufferedBytes(),c);}}
    ProductionAcceptedExitAckReplay();
    LoginRefusalResultMappingReplay();
    LoginDisconnectResultReplay();
    LoginAcceptReplyReplay();
    LoginAcceptCharacterListReplay();
    LoginSelectCharacterRequestReplay();
    LoginSelectCharacterRejectInvalidIndexReplay();
    LoginCreateCharacterRequestReplay();
    LoginAccountRequestReplay();
    LoginAccountAuxRequestReplay(true);
    LoginAccountAuxRequestReplay(false);
    Login4AccountRequestReplay();
    LoginConnectInfoGateReplay();
    LoginCharacterServerEntryRequestReplay();
    RawAccountIdPrefixBoundaryReplay();
    LoginCharacterRefusalReplay();
    LoginZoneServerNotificationReplay();
    LoginCharacterCreationRefusalReplay();
    LoginPasswordHashChallengeReplay();
    LoginPollTransientStatusReplay();
    LoginCharConnectionFailureReplay();
    Login3ClientInfoHashRequestReplay();
    ElementalSnapshotReplay();
    CompanionIdentityReplay();
    CompanionCommandReplay();
    CompanionSkillListReplay();
    LocalizedSystemNoticeReplay();
    PendingCompanionActorSkillReplay();
    MapCellUpdateReplay();
    MapTypePropertyReplay();
    CompanionTargetReplay();
    CompanionSummonReplay();
    GuildMemberLoginNoticeReplay();
    PartyMarkerGameModeExitReplay();
    SelectedInventoryCartRouteIsolationReplay();
    MapReceivePreambleReplay();
    MapTransitionLifetimeReplay();
    StandingResurrectionReplay();
    SiegeActorEmblemReplay();
    ModeLoopHandoffReplay();
    CartItemWireFieldsReplay();
    InventoryMapEntryReplay();
    InventoryMutationAckReplay();
    ItemUseFeedbackReplay();
    RecoveryDeltaReplay();
    InventoryItemFieldsReplay();
    CartStackDeltaReplay();
    CartClearContentsReplay();
    CartActorPresenceReplay();
    CharacterNameReplyIsolationReplay();
    CharacterNameRequestReplyReplay();
    RentalItemPacketReplay();
    CorpsePacketReplay();
    ChatWindowStateReplay();
    ChatNoticeRoutingReplay();
    ChatNoticeProducerReplay();
    ChatPrimaryViewReplay();
    ChatPrimarySpeechReplay();
    ChatDetachedViewReplay();
    ChatBattleOptionsReplay();
    ChatScrollBarReplay();
    ChatSourceLabelsReplay();
    ChatWindowRestoreReplay();
    ChatRoomReplay();
    ChatRoomAckReplay();
    ChatRoomEntryReplay();
    ChatRoomSettingsReplay();
    ChatRoomActionsReplay();
    WhisperReplay();
    AutoMessageReplay();
    FriendOptionsReplay();
    ChatSubmissionReplay();
    MapCellLoadLifecycleReplay();
    CConnection::Cleanup();std::cout<<"PASS packet stream replay: framing, production router/update loop, disconnect acknowledgement, request bytes, timeout boundaries, EOF drain and reconnect\n";return 0;
}catch(const std::exception& e){CConnection::Cleanup();std::cerr<<"{\"case_id\":\""<<JsonEscape(g_currentCase)<<"\",\"stage\":\""<<JsonEscape(g_currentStage)<<"\",\"fixture_id\":\""<<JsonEscape(g_currentFixture)<<"\",\"production_entry\":\""<<JsonEscape(g_currentProductionEntry)<<"\","<<ConfiguredSourceProvenanceJson()<<",\"invocation_count\":"<<g_recvInvocations<<",\"expected_packet_hex\":null,\"actual_packet_hex\":null,\"expected_queue_remainder\":null,\"actual_queue_remainder\":null,\"outcome\":\"FAIL\",\"error\":\""<<JsonEscape(e.what())<<"\"}\n";return 1;}}
