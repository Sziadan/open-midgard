#pragma once
//===========================================================================
// Connection.h  –  Winsock TCP connection classes
// Clean C++17 rewrite.
//===========================================================================
#include <winsock2.h>
#include <ws2tcpip.h>
#include "Types.h"
#include "PacketQueue.h"
#include <vector>

//===========================================================================
// CConnection  –  Base TCP socket wrapper
//===========================================================================
class CConnection
{
public:
    CConnection();
    virtual ~CConnection();

    static bool Startup();
    static void Cleanup();

    bool Connect(const char* ip, int port);
    void Disconnect();
    
    int  Send(const char* buf, int len);
    int  Recv(char* buf, int len, bool peek = false);

protected:
    virtual void OnConnect() {}
    virtual void OnClose() {}

    int FlushSendQueue();

    u32          m_socket;   // SOCKET
    sockaddr_in  m_addr;
    u8           m_bBlock;
    u32          m_dwTime;
    CPacketQueue m_sendQueue;
    CPacketQueue m_recvQueue;
    CPacketQueue m_blockQueue;
};

//===========================================================================
// CRagConnection  –  Ragnarok-protocol layer
//===========================================================================
class CRagConnection : public CConnection
{
public:
    CRagConnection();
    virtual ~CRagConnection() override;

    static CRagConnection* instance();

    bool RecvPacket(std::vector<u8>& outPacket);
    // Send a fully assembled packet (data includes the 2-byte PacketType header).
    bool SendPacket(const char* data, int len);
    bool RecvPacket(char* buffer);

protected:
    // ... Any additional methods ...
};
