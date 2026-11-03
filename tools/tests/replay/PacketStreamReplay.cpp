#include "Connection.h"
#include "GronPacket.h"
#include "MapSendProfile.h"
#include "gamemode/ActionRequestSender.h"
#include "gamemode/GameMode.h"
#include "gamemode/GameModePacket.h"
#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
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

namespace {
using Clock=std::chrono::steady_clock;
constexpr auto kTimeout=std::chrono::seconds(2);
constexpr const char* kFixedFixture="PK03.02-map-006c-synthetic";
constexpr const char* kFixedSha="a8649967f8ca13630d18e21c07eaad14b309a54534f1cd9f633b692c3b2f341f";
constexpr const char* kVariableFixture="PK03.02-variable-0069-synthetic";
constexpr const char* kVariableSha="d5eb153194bad9d1ab1c303bb77da191fedc51db5728e646153b92cf302df42b";
int g_decodeCount=0;int g_recvInvocations=0;std::string g_currentCase="startup";
void SetCase(const std::string& name){g_currentCase=name;g_recvInvocations=0;}
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
class TestConnection final:public CRagConnection{
public:SOCKET Native()const{return m_socket;}int BufferedBytes()const{return m_recvQueue.GetSize();}
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
#ifndef RO_PACKET_REPLAY_SOURCE_COMMIT
#define RO_PACKET_REPLAY_SOURCE_COMMIT "unknown"
#endif
#ifndef RO_PACKET_REPLAY_SOURCE_TREE_DIRTY
#define RO_PACKET_REPLAY_SOURCE_TREE_DIRTY 1
#endif
std::string BuildProvenanceJson(){return "\"client_runtime_base_commit\":\"af41f6484cb7ff31825296babe4b913aa36c4c67\",\"source_commit\":\""+std::string(RO_PACKET_REPLAY_SOURCE_COMMIT)+"\",\"source_tree_dirty\":"+(RO_PACKET_REPLAY_SOURCE_TREE_DIRTY?"true":"false");}
void Report(const std::string& id,const char* fixture,const char* sha,const std::vector<std::vector<u8>>& chunks,const std::string& expected,const std::string& actual,int expectedQueue,int actualQueue,const TestConnection& connection,const char* expectedState="open",const char* classification="asserted"){
    if(expectedQueue>=0)Require(actualQueue==expectedQueue,"reported queue remainder differs from asserted expectation");
    const char* actualState=connection.IsOpen()?"open":"closed";Require(std::string(expectedState)==actualState,"reported connection state differs from expectation");
    std::cout<<"{\"case_id\":\""<<id<<"\",\"contract_id\":\"PK01.P02.FRAMING\",\"fixture_id\":\""<<fixture<<"\",\"fixture_sha256\":\""<<sha<<"\","<<BuildProvenanceJson()<<",\"build_preset\":\"mingw-qt-x64-ci; deploy OFF\",\"production_entry\":\"CRagConnection::Connect/RecvPacket -> TryReadPacket\",\"stage\":\"generic_transport\",\"profile\":23,\"input_chunks_hex\":"<<ChunkJson(chunks)<<",\"invocation_count\":"<<g_recvInvocations<<",\"expected_delivery\":\""<<(expected.empty()?"no_packet":"packet")<<"\",\"expected_packet_hex\":\""<<expected<<"\",\"actual_packet_hex\":\""<<actual<<"\",\"expected_connection_state\":\""<<expectedState<<"\",\"actual_connection_state\":\""<<actualState<<"\",\"expected_queue_remainder\":"<<(expectedQueue<0?"null":std::to_string(expectedQueue))<<",\"queue_remainder\":"<<actualQueue<<",\"classification\":\""<<classification<<"\",\"outcome\":\"PASS\"}\n";
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

    SetCase("production_router_dispatch");
    TestConnection connection;
    LoopbackPeer peer(connection);
    peer.Send(combined);
    WaitKernelPending(connection,static_cast<int>(combined.size()));

    CGameMode gameMode;
    CGameModePacketRouter router;
    RegisterDefaultGameModePacketHandlers(router);
    std::vector<std::vector<u8>> received;
    std::vector<bool> dispatched;
    struct ExpectedPacket{const std::vector<u8>* bytes;u16 id;};
    const ExpectedPacket expectedPackets[]={{&first,0x01F8},{&second,0x01B0}};
    for(const ExpectedPacket& expected:expectedPackets){
        std::vector<u8> packet=ReadOne(connection);
        PacketView view=VerifyRoute(packet,expected.id);
        const bool didDispatch=router.Dispatch(gameMode,view);
        Require(didDispatch,"default production packet router did not dispatch registered packet");
        received.push_back(std::move(packet));
        dispatched.push_back(didDispatch);
        Require(received.back()==*expected.bytes,"router replay returned packet bytes in the wrong order");
    }
    Require(connection.BufferedBytes()==0,"production router replay left coalesced bytes queued");

    std::vector<u8> unknown={0x34,0x12};
    PacketView unknownView{0x1234,static_cast<u16>(unknown.size()),unknown.data()};
    const bool unknownDispatched=router.Dispatch(gameMode,unknownView);
    Require(!unknownDispatched,"production packet router dispatched an unregistered opcode");
    std::cout<<"{\"case_id\":\"production_router_dispatch\",\"contract_id\":\"PK01.P02.FRAMING\",\"fixture_id\":\""<<fixtureId
        <<"\",\"fixture_sha256\":\""<<fixtureSha
        <<"\","<<BuildProvenanceJson()<<",\"build_preset\":\"mingw-qt-x64-ci; deploy OFF\""
        <<",\"production_entry\":\"CRagConnection::RecvPacket -> TryReadPacket -> RegisterDefaultGameModePacketHandlers -> CGameModePacketRouter::Dispatch\""
        <<",\"fixture_provenance\":\"synthetic receive-profile length samples; no server-emission claim\""
        <<",\"stage\":\"synthetic_router_replay\",\"assertion_scope\":\"receive bytes and dispatch acceptance only; handler identity/effect not asserted\",\"input_chunks\":"<<ChunkJson({combined})
        <<",\"invocation_count\":"<<g_recvInvocations<<",\"expected_packet_hexes\":[\""<<Hex(first)<<"\",\""<<Hex(second)<<"\"]"
        <<",\"actual_packet_hexes\":[\""<<Hex(received[0])<<"\",\""<<Hex(received[1])<<"\"]"
        <<",\"expected_router_results\":[true,true,false],\"actual_router_results\":["
        <<(dispatched[0]?"true":"false")<<","<<(dispatched[1]?"true":"false")<<","<<(unknownDispatched?"true":"false")<<"]"
        <<",\"expected_connection_state\":\"open\",\"actual_connection_state\":\""<<(connection.IsOpen()?"open":"closed")
        <<"\",\"expected_queue_remainder\":0,\"actual_queue_remainder\":"<<connection.BufferedBytes()<<",\"outcome\":\"PASS\"}\n";
}
void FixedEveryBoundary(const std::vector<u8>& frame){for(size_t split=1;split<frame.size();++split){std::string caseId="fixed_fragment_"+std::to_string(split);SetCase(caseId);TestConnection c;LoopbackPeer p(c);std::vector<u8> out;
    p.Send(std::vector<u8>(frame.begin(),frame.begin()+split));PumpUntilBuffered(c,static_cast<int>(split));Require(out.empty(),"partial prefix returned bytes");Report(caseId+"_prefix",kFixedFixture,kFixedSha,{std::vector<u8>(frame.begin(),frame.begin()+split)},"","",split,c.BufferedBytes(),c);
    p.Send(std::vector<u8>(frame.begin()+split,frame.end()));out=PumpUntilPacket(c);Require(out==frame,"fragmented fixed packet changed");VerifyRoute(out,0x006c);Report(caseId,kFixedFixture,kFixedSha,{std::vector<u8>(frame.begin(),frame.begin()+split),std::vector<u8>(frame.begin()+split,frame.end())},Hex(frame),Hex(out),0,c.BufferedBytes(),c);}}
void FixedCoalesced(const std::vector<u8>& frame){SetCase("fixed_coalesced");TestConnection c;LoopbackPeer p(c);std::vector<u8> both=frame;both.insert(both.end(),frame.begin(),frame.end());p.Send(both);WaitKernelPending(c,static_cast<int>(both.size()));auto first=ReadOne(c);VerifyRoute(first,0x006c);Require(first==frame&&c.BufferedBytes()<=3,"first coalesced packet/remainder mismatch");Report("fixed_coalesced_first",kFixedFixture,kFixedSha,{both},Hex(frame),Hex(first),-1,c.BufferedBytes(),c);auto second=ReadOne(c);VerifyRoute(second,0x006c);Require(second==frame&&c.BufferedBytes()==0,"second coalesced packet/remainder mismatch");Report("fixed_coalesced_second",kFixedFixture,kFixedSha,{both},Hex(frame),Hex(second),0,c.BufferedBytes(),c);}
void VariableEveryBoundary(const std::vector<u8>& variable,const std::vector<u8>& fixed){for(size_t split=1;split<variable.size();++split){std::string caseId="variable_fragment_"+std::to_string(split);SetCase(caseId);TestConnection c;LoopbackPeer p(c);std::vector<u8> out;
    p.Send(std::vector<u8>(variable.begin(),variable.begin()+split));PumpUntilBuffered(c,static_cast<int>(split));Require(out.empty(),"partial variable prefix returned bytes");Report(caseId+"_prefix",kVariableFixture,kVariableSha,{std::vector<u8>(variable.begin(),variable.begin()+split)},"","",split,c.BufferedBytes(),c);
    std::vector<u8> rest(variable.begin()+split,variable.end());rest.insert(rest.end(),fixed.begin(),fixed.end());p.Send(rest);out=PumpUntilPacket(c);VerifyRoute(out,0x0069);Require(out==variable,"variable frame changed");Report(caseId,kVariableFixture,kVariableSha,{std::vector<u8>(variable.begin(),variable.begin()+split),rest},Hex(variable),Hex(out),-1,c.BufferedBytes(),c);auto next=PumpUntilPacket(c);VerifyRoute(next,0x006c);Require(next==fixed,"fixed frame after variable frame lost");Report(caseId+"_adjacent_fixed",kFixedFixture,kFixedSha,{rest},Hex(fixed),Hex(next),0,c.BufferedBytes(),c);}}
void BaselineResync(const std::vector<u8>& fixed,const std::vector<u8>& prefix,const char* id){SetCase(id);TestConnection c;LoopbackPeer p(c);std::vector<u8> input=prefix;input.insert(input.end(),fixed.begin(),fixed.end());p.Send(input);auto out=PumpUntilPacket(c);Require(out==fixed,"baseline receive policy no longer scans to following fixed packet");VerifyRoute(out,0x006c);Report(id,kFixedFixture,kFixedSha,{input},Hex(fixed),Hex(out),0,c.BufferedBytes(),c,"open","baseline_behavior_to_revisit_in_PK05");}
void EofCase(const std::vector<u8>& bytes,size_t count,const char* id,const char* fixture,const char* sha){SetCase(id);TestConnection c;LoopbackPeer p(c);if(count){p.Send(std::vector<u8>(bytes.begin(),bytes.begin()+count));PumpUntilBuffered(c,static_cast<int>(count));}std::vector<u8> out;if(count)Require(!Receive(c,out),"truncated packet returned at EOF case");CloseSocket(p.peer.value);p.peer.value=INVALID_SOCKET;WaitReadable(c,Clock::now()+kTimeout);Require(!Receive(c,out),"EOF should not return a packet");Require(!c.IsOpen()&&c.BufferedBytes()==0,"EOF did not close socket and clear partial queue");Report(id,fixture,sha,{std::vector<u8>(bytes.begin(),bytes.begin()+count)},"",Hex(out),0,c.BufferedBytes(),c,"closed");}
void OutboundTransportCapture(const std::vector<u8>& frame){SetCase("outbound_transport_capture");TestConnection c;LoopbackPeer p(c);Require(!c.SendPacket(nullptr,static_cast<int>(frame.size())),"null outbound buffer was accepted");Require(!c.SendPacket(reinterpret_cast<const char*>(frame.data()),0),"zero-length outbound packet was accepted");Require(c.SendPacket(reinterpret_cast<const char*>(frame.data()),static_cast<int>(frame.size())),"production SendPacket rejected structural sample");auto captured=p.ReceiveExact(frame.size());Require(captured==frame,"mock peer captured different outbound bytes");std::cout<<"{\"case_id\":\"outbound_transport_capture\",\"contract_id\":\"PK03.03.transport-only\",\"fixture_id\":\"PK03.02-map-006c-synthetic\",\"fixture_sha256\":\"a8649967f8ca13630d18e21c07eaad14b309a54534f1cd9f633b692c3b2f341f\","<<BuildProvenanceJson()<<",\"production_entry\":\"CRagConnection::SendPacket -> CConnection::Send/FlushSendQueue\",\"expected_packet_hex\":\""<<Hex(frame)<<"\",\"captured_packet_hex\":\""<<Hex(captured)<<"\",\"invalid_null_rejected\":true,\"invalid_empty_rejected\":true,\"outcome\":\"PASS\"}\n";}
void ActionRequestBuilderReplay(){SetCase("action_request_builder_replay");using namespace ro::net;const MapGameplaySendProfile& profile=GetActiveMapGameplaySendProfile();Require(profile.id==MapGameplaySendProfileId::PacketVer23&&profile.actionRequest==0x0437,"replay requires active profile 23 / opcode 0x0437");struct Sample{u32 target;u8 action;const char* hex;};const Sample samples[]={{0x11223344,0x00,"37044433221100"},{0x11223344,0x07,"37044433221107"},{0,0x02,"37040000000002"},{0,0x03,"37040000000003"}};std::vector<u8> expectedStream;std::vector<std::string> expectedFrames;
    for(const Sample& sample:samples){std::array<u8,7> out{};int length=0;Require(BuildActiveActionRequestPacket(sample.target,sample.action,out.data(),static_cast<int>(out.size()),&length),"production action request builder rejected approved sample");Require(length==static_cast<int>(out.size()),"production action request builder returned wrong frame length");std::vector<u8> frame(out.begin(),out.end());Require(Hex(frame)==sample.hex,"production action request builder bytes differ from independent source sample");expectedFrames.emplace_back(sample.hex);expectedStream.insert(expectedStream.end(),frame.begin(),frame.end());}
    std::array<u8,7> sentinel{};sentinel.fill(0xa5);int outLength=123;Require(!BuildActiveActionRequestPacket(1,0,sentinel.data(),6,&outLength),"undersized action builder buffer was accepted");Require(std::all_of(sentinel.begin(),sentinel.end(),[](u8 b){return b==0xa5;}),"failed builder changed sentinel output storage");Require(!BuildActiveActionRequestPacket(1,0,nullptr,7,&outLength),"null action builder output was accepted");std::array<u8,7> valid{};Require(!BuildActiveActionRequestPacket(1,0,valid.data(),7,nullptr),"null action builder length output was accepted");
    CRagConnection* connection=CRagConnection::instance();LoopbackPeer p(*connection);Require(!SendActionRequestPacket(0,0),"zero-target single attack passed production helper guard");Require(!SendActionRequestPacket(0,7),"zero-target repeat attack passed production helper guard");Require(SelectSocket(p.peer.value,true,false,std::chrono::milliseconds(50))==0,"rejected action helper emitted unexpected prefix");for(const Sample& sample:samples)Require(SendActionRequestPacket(sample.target,sample.action),"production action request sender rejected approved sample");const std::vector<u8> captured=p.ReceiveExact(expectedStream.size());Require(captured==expectedStream,"mock peer production action sender stream differs from independent frame sequence");connection->Disconnect();Require(!SendActionRequestPacket(0x11223344,0),"production action sender accepted closed connection");std::cout<<"{\"case_id\":\"action_request_builder_replay\",\"contract_id\":\"PK03.03.PK46-action-request-serialization\","<<BuildProvenanceJson()<<",\"profile\":23,\"production_entry\":\"SendActionRequestPacket -> BuildActiveActionRequestPacket -> CRagConnection::SendPacket\",\"expected_frames_hex\":[\""<<expectedFrames[0]<<"\",\""<<expectedFrames[1]<<"\",\""<<expectedFrames[2]<<"\",\""<<expectedFrames[3]<<"\"],\"captured_stream_hex\":\""<<Hex(captured)<<"\",\"builder_failures\":3,\"zero_target_attack_and_repeat_rejected\":true,\"closed_connection_rejected\":true,\"outcome\":\"PASS\"}\n";}
}
int main(){try{
    Require(CConnection::Startup(),"socket startup failed");
    Require(ro::net::GetPacketSize(0x006c)==3,"0x006c fixed receive size differs from active client table");
    const std::vector<u8> fixed{0x6c,0x00,0x00}; // opaque length sample: map registry clif_packetdb.hpp line 18
    const std::vector<u8> variable{0x69,0x00,0x06,0x00,0x11,0x22}; // variable framing only
    FixedEveryBoundary(fixed);FixedCoalesced(fixed);VariableEveryBoundary(variable,fixed);
    BaselineResync(fixed,{0xff,0xff},"unknown_opcode_baseline_resync");
    BaselineResync(fixed,{0x69,0x00,0x03,0x00},"undersized_variable_baseline_resync");
    BaselineResync(fixed,{0x69,0x00,0x00,0x80},"oversized_variable_baseline_resync");
    EofCase(fixed,0,"eof_empty",kFixedFixture,kFixedSha);EofCase(fixed,1,"eof_partial_fixed_header",kFixedFixture,kFixedSha);
    EofCase(variable,5,"eof_partial_variable_body",kVariableFixture,kVariableSha);
    ProductionRouterDispatchReplay();
    OutboundTransportCapture(fixed);
    ActionRequestBuilderReplay();
    {SetCase("production_reconnect_reset");TestConnection c;{LoopbackPeer p(c);p.Send({0x6c});PumpUntilBuffered(c,1);}c.Disconnect();Require(!c.IsOpen()&&c.BufferedBytes()==0,"disconnect failed to clear receive queue");{LoopbackPeer p(c);p.Send(fixed);auto out=PumpUntilPacket(c);Require(out==fixed,"production reconnect failed after disconnect");VerifyRoute(out,0x006c);Report("production_reconnect_reset",kFixedFixture,kFixedSha,{fixed},Hex(fixed),Hex(out),0,c.BufferedBytes(),c);}}
    CConnection::Cleanup();std::cout<<"PASS packet stream replay: fixed and variable fragmentation, production decoder packets="<<g_decodeCount<<", production router dispatch, coalescing, baseline resync characterization, EOF and production reconnect\n";return 0;
}catch(const std::exception& e){CConnection::Cleanup();std::string error=e.what();std::replace(error.begin(),error.end(),'"','\'');std::cerr<<"{\"case_id\":\""<<g_currentCase<<"\","<<BuildProvenanceJson()<<",\"production_entry\":\"CRagConnection::Connect/RecvPacket -> TryReadPacket\",\"invocation_count\":"<<g_recvInvocations<<",\"outcome\":\"FAIL\",\"error\":\""<<error<<"\"}\n";return 1;}}
