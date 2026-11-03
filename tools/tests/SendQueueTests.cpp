#include "network/PacketQueue.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>
#include <algorithm>

constexpr int INVALID_SOCKET=-1;
struct CConnection {
    int m_socket=1;
    CPacketQueue m_sendQueue;
    int FlushSendQueue();
    int Send(const char*,int);
    void Disconnect(){m_socket=INVALID_SOCKET;m_sendQueue.Clear();}
};
struct Attempt { std::vector<char> bytes; };
std::vector<Attempt> attempts;
std::vector<char> delivered;
std::vector<int> results;
int socketError=0;
int send(int,const char* data,int size,int) {
    attempts.push_back({std::vector<char>(data,data+size)});
    const int result=results.empty()?size:results.front();
    if(!results.empty())results.erase(results.begin());
    if(result>0){assert(result<=size);delivered.insert(delivered.end(),data,data+result);}
    return result;
}
int GetLastSocketErrorCode(){return socketError;}
bool IsSocketWouldBlockError(int code){return code==10035;}
void LogSocketClosure(const char*,int){}
#include "SendQueue.inc"

int main(){
    CConnection connection;
    std::vector<char> emblem(1349);
    for(int i=0;i<1349;++i)emblem[i]=static_cast<char>(i*73);
    emblem[0]=0x53;emblem[1]=1;emblem[2]=char(1349);emblem[3]=char(1349>>8);
    assert(connection.Send(emblem.data(),emblem.size())==1349);
    assert(attempts.size()==1 && attempts[0].bytes==emblem && delivered==emblem);
    assert(connection.m_sendQueue.GetSize()==0 && connection.m_sendQueue.ReadData()==nullptr);

    // A short OS write followed by would-block must retain only the unsent tail.
    attempts.clear();delivered.clear();results={400,-1};socketError=10035;
    assert(connection.Send(emblem.data(),emblem.size())==400);
    assert(connection.m_sendQueue.GetSize()==949);
    assert(attempts[0].bytes==emblem && attempts[1].bytes==std::vector<char>(emblem.begin()+400,emblem.end()));
    assert(connection.FlushSendQueue()==949 && delivered==emblem && connection.m_sendQueue.GetSize()==0);

    // Keep a blocked packet ahead of newly queued data, even after buffer growth.
    attempts.clear();delivered.clear();results={-1};
    assert(connection.Send(emblem.data(),emblem.size())==0);
    std::vector<char> following(70000,'x'),expected=emblem;
    expected.insert(expected.end(),following.begin(),following.end());
    assert(connection.Send(following.data(),following.size())==expected.size());
    assert(attempts.back().bytes==expected && delivered==expected);

    results={0};assert(connection.Send(emblem.data(),emblem.size())==-1);
    assert(connection.m_socket==INVALID_SOCKET && connection.m_sendQueue.GetSize()==0);
    connection.m_socket=1;results={-1};socketError=10054;
    assert(connection.Send(emblem.data(),emblem.size())==-1 && connection.m_socket==INVALID_SOCKET);
    std::cout<<"PASS: production send path offers the complete 1349-byte emblem in one socket write; partial writes, would-block retries, ordering, buffer growth and disconnects preserve queue state\n";
}
