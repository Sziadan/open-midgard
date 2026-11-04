#include "Connection.h"
#include "GronPacket.h"
#include "MapSendProfile.h"
#include "Packet.h"
#include "core/ClientInfoLocale.h"
#include "core/Globals.h"
#include "gamemode/ActionRequestSender.h"
#include "gamemode/GameMode.h"
#include "gamemode/GameModePacket.h"
#include "gamemode/LoginMode.h"
#include "gamemode/Mode.h"
#include "session/Session.h"
#include "ui/UIChooseWnd.h"
#include "ui/UIWindowMgr.h"
#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#if !RO_PLATFORM_WINDOWS
#include <sys/ioctl.h>
#include <unistd.h>
#endif
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
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
void WaitKernelPending(const TestConnection& c,int bytes){auto deadline=Clock::now()+kTimeout;while(Clock::now()<deadline){int pending=0;
#if RO_PLATFORM_WINDOWS
    Require(ioctlsocket(c.Native(),FIONREAD,reinterpret_cast<u_long*>(&pending))==0,"FIONREAD failed");
#else
    Require(ioctl(c.Native(),FIONREAD,&pending)==0,"FIONREAD failed");
#endif
    if(pending>=bytes)return;std::this_thread::sleep_for(std::chrono::milliseconds(1));}throw std::runtime_error("coalesced bytes did not reach receive socket before deadline");}
std::string Hex(const std::vector<u8>& bytes){static const char* d="0123456789abcdef";std::string out;for(u8 b:bytes){out.push_back(d[b>>4]);out.push_back(d[b&15]);}return out;}
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
void BaselineResync(const std::vector<u8>& fixed,const std::vector<u8>& prefix,const char* id){SetCase(id);TestConnection c;LoopbackPeer p(c);std::vector<u8> input=prefix;input.insert(input.end(),fixed.begin(),fixed.end());p.Send(input);auto out=PumpUntilPacket(c);Require(out==fixed,"baseline receive policy no longer scans to following fixed packet");VerifyRoute(out,0x006c);Report(id,kFixedFixture,kFixedSha,{input},Hex(fixed),Hex(out),0,c.BufferedBytes(),c,"open","baseline_behavior_to_revisit_in_PK05");}
void EofCase(const std::vector<u8>& bytes,size_t count,const char* id,const char* fixture,const char* sha){SetCase(id);TestConnection c;LoopbackPeer p(c);if(count){p.Send(std::vector<u8>(bytes.begin(),bytes.begin()+count));PumpUntilBuffered(c,static_cast<int>(count));}std::vector<u8> out;if(count)Require(!Receive(c,out),"truncated packet returned at EOF case");CloseSocket(p.peer.value);p.peer.value=INVALID_SOCKET;WaitReadable(c,Clock::now()+kTimeout);Require(!Receive(c,out),"EOF should not return a packet");Require(!c.IsOpen()&&c.BufferedBytes()==0,"EOF did not close socket and clear partial queue");Report(id,fixture,sha,{std::vector<u8>(bytes.begin(),bytes.begin()+count)},"",Hex(out),0,c.BufferedBytes(),c,"closed");}
void OutboundTransportCapture(const std::vector<u8>& frame){SetCase("outbound_transport_capture");TestConnection c;LoopbackPeer p(c);Require(!c.SendPacket(nullptr,static_cast<int>(frame.size())),"null outbound buffer was accepted");Require(!c.SendPacket(reinterpret_cast<const char*>(frame.data()),0),"zero-length outbound packet was accepted");Require(c.SendPacket(reinterpret_cast<const char*>(frame.data()),static_cast<int>(frame.size())),"production SendPacket rejected structural sample");auto captured=p.ReceiveExact(frame.size());Require(captured==frame,"mock peer captured different outbound bytes");std::cout<<"{\"case_id\":\"outbound_transport_capture\",\"contract_id\":\"PK03.03.transport-only\",\"fixture_id\":\"PK03.02-map-006c-synthetic\",\"expected_fixture_sha256\":\"a8649967f8ca13630d18e21c07eaad14b309a54534f1cd9f633b692c3b2f341f\","<<ConfiguredSourceProvenanceJson()<<",\"production_entry\":\"CRagConnection::SendPacket -> CConnection::Send/FlushSendQueue\",\"expected_packet_hex\":\""<<Hex(frame)<<"\",\"captured_packet_hex\":\""<<Hex(captured)<<"\",\"invalid_null_rejected\":true,\"invalid_empty_rejected\":true,\"outcome\":\"PASS\"}\n";}
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

int main(){try{
    Require(CConnection::Startup(),"socket startup failed");
    Require(ro::net::GetPacketSize(0x006c)==3,"0x006c fixed receive size differs from active client table");
    Require(ro::net::GetPacketSize(0x018b)==4,"0x018b disconnect acknowledgement size differs from contract");
    Require(ro::net::GetPacketSize(0x8482)==0 && ro::net::GetPacketSize(0x8483)==0,
        "fake raw account-ID prefix identities remain registered as opcodes");
    const std::vector<u8> fixed{0x6c,0x00,0x00}; // opaque length sample: map registry clif_packetdb.hpp line 18
    const std::vector<u8> variable{0x69,0x00,0x06,0x00,0x11,0x22}; // variable framing only
    FixedEveryBoundary(fixed);FixedCoalesced(fixed);VariableEveryBoundary(variable,fixed);
    BaselineResync(fixed,{0xff,0xff},"unknown_opcode_baseline_resync");
    BaselineResync(fixed,{0x69,0x00,0x03,0x00},"undersized_variable_baseline_resync");
    BaselineResync(fixed,{0x69,0x00,0x00,0x80},"oversized_variable_baseline_resync");
    EofCase(fixed,0,"eof_empty",kFixedFixture,kFixedSha);EofCase(fixed,1,"eof_partial_fixed_header",kFixedFixture,kFixedSha);
    EofCase(variable,5,"eof_partial_variable_body",kVariableFixture,kVariableSha);
    ProductionRouterDispatchReplay();
    DisconnectAckEveryBoundaryReplay();
    DisconnectAckRefusalDispatchReplay(PendingDisconnectAction::ReturnToCharSelect,"disconnect_ack_refuses_return_to_char_select");
    DisconnectAckRefusalDispatchReplay(PendingDisconnectAction::ExitToWindows,"disconnect_ack_refuses_exit_to_windows");
    OutboundTransportCapture(fixed);
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
    CConnection::Cleanup();std::cout<<"PASS packet stream replay: framing, production router/update loop, disconnect acknowledgement, request bytes, timeout boundaries, EOF drain and reconnect\n";return 0;
}catch(const std::exception& e){CConnection::Cleanup();std::cerr<<"{\"case_id\":\""<<JsonEscape(g_currentCase)<<"\",\"stage\":\""<<JsonEscape(g_currentStage)<<"\",\"fixture_id\":\""<<JsonEscape(g_currentFixture)<<"\",\"production_entry\":\""<<JsonEscape(g_currentProductionEntry)<<"\","<<ConfiguredSourceProvenanceJson()<<",\"invocation_count\":"<<g_recvInvocations<<",\"expected_packet_hex\":null,\"actual_packet_hex\":null,\"expected_queue_remainder\":null,\"actual_queue_remainder\":null,\"outcome\":\"FAIL\",\"error\":\""<<JsonEscape(e.what())<<"\"}\n";return 1;}}
