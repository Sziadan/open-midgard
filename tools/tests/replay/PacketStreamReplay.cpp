#include "Connection.h"
#include "GronPacket.h"
#include "gamemode/GameModePacket.h"
#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <iostream>
#if !RO_PLATFORM_WINDOWS
#include <sys/ioctl.h>
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
    explicit LoopbackPeer(TestConnection& connection){
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
void Report(const std::string& id,const char* fixture,const char* sha,const std::vector<std::vector<u8>>& chunks,const std::string& expected,const std::string& actual,int expectedQueue,int actualQueue,const TestConnection& connection,const char* expectedState="open",const char* classification="asserted"){
    if(expectedQueue>=0)Require(actualQueue==expectedQueue,"reported queue remainder differs from asserted expectation");
    const char* actualState=connection.IsOpen()?"open":"closed";Require(std::string(expectedState)==actualState,"reported connection state differs from expectation");
    std::cout<<"{\"case_id\":\""<<id<<"\",\"contract_id\":\"PK01.P02.FRAMING\",\"fixture_id\":\""<<fixture<<"\",\"fixture_sha256\":\""<<sha<<"\",\"client_commit\":\"af41f6484cb7ff31825296babe4b913aa36c4c67\",\"build_preset\":\"mingw-qt-x64-ci; deploy OFF\",\"production_entry\":\"CRagConnection::Connect/RecvPacket -> TryReadPacket\",\"stage\":\"generic_transport\",\"profile\":23,\"input_chunks_hex\":"<<ChunkJson(chunks)<<",\"invocation_count\":"<<g_recvInvocations<<",\"expected_delivery\":\""<<(expected.empty()?"no_packet":"packet")<<"\",\"expected_packet_hex\":\""<<expected<<"\",\"actual_packet_hex\":\""<<actual<<"\",\"expected_connection_state\":\""<<expectedState<<"\",\"actual_connection_state\":\""<<actualState<<"\",\"expected_queue_remainder\":"<<(expectedQueue<0?"null":std::to_string(expectedQueue))<<",\"queue_remainder\":"<<actualQueue<<",\"classification\":\""<<classification<<"\",\"outcome\":\"PASS\"}\n";
}
std::vector<u8> ReadOne(TestConnection& c){return PumpUntilPacket(c);}
void VerifyRoute(const std::vector<u8>& packet,u16 id){PacketView view{};int consumed=0;Require(TryReadPacket(packet.data(),static_cast<int>(packet.size()),view,consumed),"production packet decoder rejected framed packet");Require(view.packetId==id&&view.packetLength==packet.size()&&consumed==static_cast<int>(packet.size()),"production packet decoder returned wrong boundary");++g_decodeCount;}
void FixedEveryBoundary(const std::vector<u8>& frame){for(size_t split=1;split<frame.size();++split){std::string caseId="fixed_fragment_"+std::to_string(split);SetCase(caseId);TestConnection c;LoopbackPeer p(c);std::vector<u8> out;
    p.Send(std::vector<u8>(frame.begin(),frame.begin()+split));PumpUntilBuffered(c,static_cast<int>(split));Require(out.empty(),"partial prefix returned bytes");Report(caseId+"_prefix",kFixedFixture,kFixedSha,{std::vector<u8>(frame.begin(),frame.begin()+split)},"","",split,c.BufferedBytes(),c);
    p.Send(std::vector<u8>(frame.begin()+split,frame.end()));out=PumpUntilPacket(c);Require(out==frame,"fragmented fixed packet changed");VerifyRoute(out,0x006c);Report(caseId,kFixedFixture,kFixedSha,{std::vector<u8>(frame.begin(),frame.begin()+split),std::vector<u8>(frame.begin()+split,frame.end())},Hex(frame),Hex(out),0,c.BufferedBytes(),c);}}
void FixedCoalesced(const std::vector<u8>& frame){SetCase("fixed_coalesced");TestConnection c;LoopbackPeer p(c);std::vector<u8> both=frame;both.insert(both.end(),frame.begin(),frame.end());p.Send(both);WaitKernelPending(c,static_cast<int>(both.size()));auto first=ReadOne(c);VerifyRoute(first,0x006c);Require(first==frame&&c.BufferedBytes()<=3,"first coalesced packet/remainder mismatch");Report("fixed_coalesced_first",kFixedFixture,kFixedSha,{both},Hex(frame),Hex(first),-1,c.BufferedBytes(),c);auto second=ReadOne(c);VerifyRoute(second,0x006c);Require(second==frame&&c.BufferedBytes()==0,"second coalesced packet/remainder mismatch");Report("fixed_coalesced_second",kFixedFixture,kFixedSha,{both},Hex(frame),Hex(second),0,c.BufferedBytes(),c);}
void VariableEveryBoundary(const std::vector<u8>& variable,const std::vector<u8>& fixed){for(size_t split=1;split<variable.size();++split){std::string caseId="variable_fragment_"+std::to_string(split);SetCase(caseId);TestConnection c;LoopbackPeer p(c);std::vector<u8> out;
    p.Send(std::vector<u8>(variable.begin(),variable.begin()+split));PumpUntilBuffered(c,static_cast<int>(split));Require(out.empty(),"partial variable prefix returned bytes");Report(caseId+"_prefix",kVariableFixture,kVariableSha,{std::vector<u8>(variable.begin(),variable.begin()+split)},"","",split,c.BufferedBytes(),c);
    std::vector<u8> rest(variable.begin()+split,variable.end());rest.insert(rest.end(),fixed.begin(),fixed.end());p.Send(rest);out=PumpUntilPacket(c);VerifyRoute(out,0x0069);Require(out==variable,"variable frame changed");Report(caseId,kVariableFixture,kVariableSha,{std::vector<u8>(variable.begin(),variable.begin()+split),rest},Hex(variable),Hex(out),-1,c.BufferedBytes(),c);auto next=PumpUntilPacket(c);VerifyRoute(next,0x006c);Require(next==fixed,"fixed frame after variable frame lost");Report(caseId+"_adjacent_fixed",kFixedFixture,kFixedSha,{rest},Hex(fixed),Hex(next),0,c.BufferedBytes(),c);}}
void BaselineResync(const std::vector<u8>& fixed,const std::vector<u8>& prefix,const char* id){SetCase(id);TestConnection c;LoopbackPeer p(c);std::vector<u8> input=prefix;input.insert(input.end(),fixed.begin(),fixed.end());p.Send(input);auto out=PumpUntilPacket(c);Require(out==fixed,"baseline receive policy no longer scans to following fixed packet");VerifyRoute(out,0x006c);Report(id,kFixedFixture,kFixedSha,{input},Hex(fixed),Hex(out),0,c.BufferedBytes(),c,"open","baseline_behavior_to_revisit_in_PK05");}
void EofCase(const std::vector<u8>& bytes,size_t count,const char* id,const char* fixture,const char* sha){SetCase(id);TestConnection c;LoopbackPeer p(c);if(count){p.Send(std::vector<u8>(bytes.begin(),bytes.begin()+count));PumpUntilBuffered(c,static_cast<int>(count));}std::vector<u8> out;if(count)Require(!Receive(c,out),"truncated packet returned at EOF case");CloseSocket(p.peer.value);p.peer.value=INVALID_SOCKET;WaitReadable(c,Clock::now()+kTimeout);Require(!Receive(c,out),"EOF should not return a packet");Require(!c.IsOpen()&&c.BufferedBytes()==0,"EOF did not close socket and clear partial queue");Report(id,fixture,sha,{std::vector<u8>(bytes.begin(),bytes.begin()+count)},"",Hex(out),0,c.BufferedBytes(),c,"closed");}
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
    {SetCase("production_reconnect_reset");TestConnection c;{LoopbackPeer p(c);p.Send({0x6c});PumpUntilBuffered(c,1);}c.Disconnect();Require(!c.IsOpen()&&c.BufferedBytes()==0,"disconnect failed to clear receive queue");{LoopbackPeer p(c);p.Send(fixed);auto out=PumpUntilPacket(c);Require(out==fixed,"production reconnect failed after disconnect");VerifyRoute(out,0x006c);Report("production_reconnect_reset",kFixedFixture,kFixedSha,{fixed},Hex(fixed),Hex(out),0,c.BufferedBytes(),c);}}
    CConnection::Cleanup();std::cout<<"PASS packet stream replay: fixed and variable fragmentation, production decoder packets="<<g_decodeCount<<", coalescing, baseline resync characterization, EOF and production reconnect\n";return 0;
}catch(const std::exception& e){CConnection::Cleanup();std::string error=e.what();std::replace(error.begin(),error.end(),'"','\'');std::cerr<<"{\"case_id\":\""<<g_currentCase<<"\",\"client_commit\":\"af41f6484cb7ff31825296babe4b913aa36c4c67\",\"production_entry\":\"CRagConnection::Connect/RecvPacket -> TryReadPacket\",\"invocation_count\":"<<g_recvInvocations<<",\"outcome\":\"FAIL\",\"error\":\""<<error<<"\"}\n";return 1;}}


