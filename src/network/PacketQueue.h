#pragma once
//===========================================================================
// PacketQueue.h  –  Linear packet queue used by network send/recv buffers
//===========================================================================
#include <vector>

class CPacketQueue
{
public:
    CPacketQueue();
    explicit CPacketQueue(int capacity);
    ~CPacketQueue();

    void Init(int capacity);

    bool GetData(int len, char* dst);
    bool PeekData(int len, char* dst) const;
    bool RemoveData(int len);
    int  GetSize() const;
    void InsertData(int len, const char* src);

    // Compatibility wrappers used by current connection code.
    void Clear() { Init(static_cast<int>(m_buffer.size())); }
    bool Push(const char* data, int len)
    {
        if (!data || len <= 0) return false;
        InsertData(len, data);
        return true;
    }
    bool Pop(char* data, int len) { return GetData(len, data); }
    bool Peek(char* data, int len) const { return PeekData(len, data); }

private:
    std::vector<char> m_buffer;
    char*             m_buf;
    int               m_frontPos;
    int               m_rearPos;
};
