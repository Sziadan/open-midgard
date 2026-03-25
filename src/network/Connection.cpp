#include "Connection.h"
#include "GronPacket.h"
#include "DebugLog.h"

#include <algorithm>
#include <cstring>
#include <map>

#pragma comment(lib, "ws2_32.lib")

namespace {

constexpr char kBuildMarker[] = "2026-03-23 20:14 actor-trace-f";

struct RecentRecvPacket {
    u16 packetId;
    int packetSize;
};

RecentRecvPacket g_recentRecvPackets[8] = {};
int g_recentRecvPacketCount = 0;
int g_recentRecvPacketIndex = 0;
int g_recvTracePacketsRemaining = 0;

void RememberRecvPacket(u16 packetId, int packetSize)
{
    g_recentRecvPackets[g_recentRecvPacketIndex] = RecentRecvPacket{packetId, packetSize};
    g_recentRecvPacketIndex = (g_recentRecvPacketIndex + 1) % static_cast<int>(std::size(g_recentRecvPackets));
    if (g_recentRecvPacketCount < static_cast<int>(std::size(g_recentRecvPackets))) {
        ++g_recentRecvPacketCount;
    }
}

void FormatQueueSample(CPacketQueue& queue, char* outText, size_t outTextSize)
{
    if (!outText || outTextSize == 0) {
        return;
    }

    outText[0] = '\0';

    char raw[16] = {};
    const int sampleBytes = (std::min)(queue.GetSize(), static_cast<int>(sizeof(raw)));
    if (sampleBytes <= 0 || !queue.Peek(raw, sampleBytes)) {
        return;
    }

    size_t cursor = 0;
    for (int i = 0; i < sampleBytes && cursor + 4 < outTextSize; ++i) {
        cursor += std::snprintf(
            outText + cursor,
            outTextSize - cursor,
            "%02X%s",
            static_cast<unsigned char>(raw[i]),
            (i + 1 < sampleBytes) ? " " : "");
    }
}

void FormatRecentPacketHistory(char* outText, size_t outTextSize)
{
    if (!outText || outTextSize == 0) {
        return;
    }

    outText[0] = '\0';
    size_t cursor = 0;
    for (int i = 0; i < g_recentRecvPacketCount && cursor + 16 < outTextSize; ++i) {
        const int index = (g_recentRecvPacketIndex - g_recentRecvPacketCount + i + static_cast<int>(std::size(g_recentRecvPackets)))
            % static_cast<int>(std::size(g_recentRecvPackets));
        const RecentRecvPacket& packet = g_recentRecvPackets[index];
        cursor += std::snprintf(
            outText + cursor,
            outTextSize - cursor,
            "%s0x%04X/%d",
            (i > 0) ? " " : "",
            packet.packetId,
            packet.packetSize);
    }
}

void LogFirstSeenUnknownRecvPacket(u16 packetId, int bufferedBytes, CPacketQueue& queue)
{
    static std::map<u16, bool> loggedPacketIds;
    if (loggedPacketIds.insert(std::make_pair(packetId, true)).second) {
        char sample[3 * 16 + 1] = {};
        char history[8 * 16 + 1] = {};
        FormatQueueSample(queue, sample, sizeof(sample));
        FormatRecentPacketHistory(history, sizeof(history));
        DbgLog("[Net] dropping unknown recv packet id=0x%04X buffered=%d recent=%s sample=%s\n",
            packetId,
            bufferedBytes,
            history,
            sample);
    }
}

void LogFirstSeenRecvPacket0077(int bufferedBytes, s16 knownSize, CPacketQueue& queue)
{
    static bool logged = false;
    if (logged) {
        return;
    }

    logged = true;

    char sample[3 * 16 + 1] = {};
    char history[8 * 16 + 1] = {};
    FormatQueueSample(queue, sample, sizeof(sample));
    FormatRecentPacketHistory(history, sizeof(history));
    DbgLog("[Net] observed recv packet id=0x0077 buffered=%d knownSize=%d recent=%s sample=%s\n",
        bufferedBytes,
        static_cast<int>(knownSize),
        history,
        sample);
}

void LogFirstSeenInvalidVariablePacket(u16 packetId, int packetSize, CPacketQueue& queue)
{
    static std::map<u16, bool> loggedPacketIds;
    if (loggedPacketIds.insert(std::make_pair(packetId, true)).second) {
        char sample[3 * 16 + 1] = {};
        char history[8 * 16 + 1] = {};
        FormatQueueSample(queue, sample, sizeof(sample));
        FormatRecentPacketHistory(history, sizeof(history));
        DbgLog("[Net] dropping invalid variable recv packet id=0x%04X size=%d recent=%s sample=%s\n",
            packetId,
            packetSize,
            history,
            sample);
    }
}

void ResetConnectionRecvTrace(const char* ip, int port)
{
    g_recentRecvPacketCount = 0;
    g_recentRecvPacketIndex = 0;
    g_recvTracePacketsRemaining = 24;
    DbgLog("[Net] connect trace armed ip=%s port=%d budget=%d\n",
        ip ? ip : "",
        port,
        g_recvTracePacketsRemaining);
}

void LogConnectionRecvTrace(u16 packetId, int packetSize)
{
    if (g_recvTracePacketsRemaining <= 0) {
        return;
    }

    --g_recvTracePacketsRemaining;
    DbgLog("[Net] recv trace id=0x%04X size=%d remaining=%d\n",
        packetId,
        packetSize,
        g_recvTracePacketsRemaining);
}

} // namespace

//===========================================================================
// CConnection Implementation
//===========================================================================
bool CConnection::Startup()
{
    ro::net::InitializePacketSize();

    DbgLog("[Build] marker=%s pkt0078=%d\n", kBuildMarker, ro::net::GetPacketSize(0x0078));

    WSADATA wsa;
    return WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
}

void CConnection::Cleanup()
{
    WSACleanup();
}

CConnection::CConnection() : m_socket((u32)INVALID_SOCKET), m_bBlock(0), m_dwTime(0)
{
    std::memset(&m_addr, 0, sizeof(m_addr));
}

CConnection::~CConnection()
{
    Disconnect();
}

bool CConnection::Connect(const char* ip, int port)
{
    Disconnect();
    ResetConnectionRecvTrace(ip, port);

    m_socket = (u32)socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_socket == (u32)INVALID_SOCKET) return false;

    m_addr.sin_family = AF_INET;
    m_addr.sin_port = htons((u16)port);
    m_addr.sin_addr.s_addr = inet_addr(ip);

    u_long nonBlock = 1;
    ioctlsocket((SOCKET)m_socket, FIONBIO, &nonBlock);

    const int connectRet = connect((SOCKET)m_socket, (sockaddr*)&m_addr, sizeof(m_addr));
    if (connectRet == 0) {
        m_dwTime = GetTickCount();
        return true;
    }

    const int lastError = WSAGetLastError();
    if (lastError != WSAEWOULDBLOCK && lastError != WSAEINPROGRESS && lastError != WSAEALREADY) {
        Disconnect();
        return false;
    }

    fd_set writeSet;
    fd_set errorSet;
    FD_ZERO(&writeSet);
    FD_ZERO(&errorSet);
    FD_SET((SOCKET)m_socket, &writeSet);
    FD_SET((SOCKET)m_socket, &errorSet);

    timeval tv{};
    tv.tv_sec = 3;
    tv.tv_usec = 0;

    const int selectRet = select(0, nullptr, &writeSet, &errorSet, &tv);
    if (selectRet <= 0 || FD_ISSET((SOCKET)m_socket, &errorSet)) {
        Disconnect();
        return false;
    }

    int soError = 0;
    int soLen = sizeof(soError);
    if (getsockopt((SOCKET)m_socket, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&soError), &soLen) != 0 || soError != 0) {
        Disconnect();
        return false;
    }

    m_dwTime = GetTickCount();
    return true;
}

void CConnection::Disconnect()
{
    if (m_socket != (u32)INVALID_SOCKET) {
        closesocket((SOCKET)m_socket);
        m_socket = (u32)INVALID_SOCKET;
    }
    m_sendQueue.Clear();
    m_recvQueue.Clear();
    m_blockQueue.Clear();
}

int CConnection::FlushSendQueue()
{
    if (m_socket == (u32)INVALID_SOCKET) {
        return -1;
    }

    int totalSent = 0;
    char tmp[1024];
    while (m_sendQueue.GetSize() > 0) {
        const int toSend = (std::min)(m_sendQueue.GetSize(), static_cast<int>(sizeof(tmp)));
        if (toSend <= 0 || !m_sendQueue.Peek(tmp, toSend)) {
            break;
        }

        const int sent = send((SOCKET)m_socket, tmp, toSend, 0);
        if (sent > 0) {
            m_sendQueue.Pop(tmp, sent);
            totalSent += sent;
            continue;
        }

        if (sent == 0) {
            break;
        }

        const int lastError = WSAGetLastError();
        if (lastError == WSAEWOULDBLOCK || lastError == WSAEINPROGRESS || lastError == WSAEALREADY) {
            break;
        }

        Disconnect();
        return -1;
    }

    return totalSent;
}

int CConnection::Send(const char* buf, int len)
{
    if (m_socket == (u32)INVALID_SOCKET) return -1;
    if (!m_sendQueue.Push(buf, len)) return -1;

    const int flushed = FlushSendQueue();
    if (flushed < 0) {
        return -1;
    }
    return flushed;
}

int CConnection::Recv(char* buf, int len, bool peek)
{
    FlushSendQueue();

    // Try to fill the queue first
    char tmp[4096];
    int n = recv((SOCKET)m_socket, tmp, sizeof(tmp), 0);
    if (n > 0) {
        m_recvQueue.Push(tmp, n);
    }

    if (peek) return m_recvQueue.Peek(buf, len) ? len : 0;
    return m_recvQueue.Pop(buf, len) ? len : 0;
}

//===========================================================================
// CRagConnection Implementation
//===========================================================================
CRagConnection::CRagConnection() = default;
CRagConnection::~CRagConnection() = default;

CRagConnection* CRagConnection::instance()
{
    static CRagConnection s_instance;
    return &s_instance;
}

bool CRagConnection::SendPacket(const char* data, int len)
{
    if (!data || len <= 0) return false;
    return Send(data, len) >= 0;
}

bool CRagConnection::RecvPacket(std::vector<u8>& outPacket)
{
    outPacket.clear();

    if (FlushSendQueue() < 0) {
        return false;
    }

    // Pump socket data into queue (non-fatal when no data available).
    char tmp[4096];
    int n = recv((SOCKET)m_socket, tmp, sizeof(tmp), 0);
    if (n > 0) {
        m_recvQueue.Push(tmp, n);
    }

    while (true) {
        if (m_recvQueue.GetSize() < 2) {
            return false;
        }

        char hdr[4] = {};
        if (!m_recvQueue.Peek(hdr, 2)) {
            return false;
        }

        const u16 packetId = static_cast<u16>(
            static_cast<unsigned char>(hdr[0]) |
            (static_cast<u16>(static_cast<unsigned char>(hdr[1])) << 8));

        const s16 knownSize = ro::net::GetPacketSize(packetId);
        if (packetId == 0x0077) {
            LogFirstSeenRecvPacket0077(m_recvQueue.GetSize(), knownSize, m_recvQueue);
        }

        if (knownSize == 0) {
            // Unknown packet id: drop this header and keep scanning so one unsupported
            // packet doesn't stall the whole login/game receive pipeline.
            LogFirstSeenUnknownRecvPacket(packetId, m_recvQueue.GetSize(), m_recvQueue);
            m_recvQueue.RemoveData(2);
            continue;
        }

        int packetSize = knownSize;
        if (knownSize == ro::net::kVariablePacketSize) {
            if (m_recvQueue.GetSize() < 4 || !m_recvQueue.Peek(hdr, 4)) {
                return false;
            }

            packetSize = static_cast<int>(
                static_cast<unsigned char>(hdr[2]) |
                (static_cast<u16>(static_cast<unsigned char>(hdr[3])) << 8));
            if (packetSize < 4 || packetSize > 0x7FFF) {
                LogFirstSeenInvalidVariablePacket(packetId, packetSize, m_recvQueue);
                m_recvQueue.RemoveData(2);
                continue;
            }
        }

        if (m_recvQueue.GetSize() < packetSize) {
            return false;
        }

        outPacket.resize(static_cast<size_t>(packetSize));
        const bool popped = m_recvQueue.Pop(reinterpret_cast<char*>(outPacket.data()), packetSize);
        if (popped) {
            RememberRecvPacket(packetId, packetSize);
            LogConnectionRecvTrace(packetId, packetSize);
        }
        return popped;
    }
}

bool CRagConnection::RecvPacket(char* buffer)
{
    if (!buffer) {
        return false;
    }

    std::vector<u8> packet;
    if (!RecvPacket(packet)) {
        return false;
    }

    std::memcpy(buffer, packet.data(), packet.size());
    return true;
}
