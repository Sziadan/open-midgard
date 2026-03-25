#include "PacketQueue.h"

#include <algorithm>
#include <cstring>

namespace {
constexpr int kDefaultQueueSize = 64 * 1024;
}

CPacketQueue::CPacketQueue()
    : m_buf(nullptr), m_frontPos(0), m_rearPos(0)
{
    Init(kDefaultQueueSize);
}

CPacketQueue::CPacketQueue(int capacity)
    : m_buf(nullptr), m_frontPos(0), m_rearPos(0)
{
    Init(capacity);
}

CPacketQueue::~CPacketQueue() = default;

void CPacketQueue::Init(int capacity)
{
    if (capacity <= 0) {
        capacity = kDefaultQueueSize;
    }

    if (static_cast<int>(m_buffer.size()) < capacity) {
        m_buffer.resize(static_cast<size_t>(capacity), 0);
    }

    m_frontPos = 0;
    m_rearPos  = 0;
    m_buf      = m_buffer.empty() ? nullptr : m_buffer.data();
}

bool CPacketQueue::GetData(int len, char* dst)
{
    if (!dst || len <= 0) return false;
    if (GetSize() < len || !m_buf) return false;

    std::memcpy(dst, m_buf + m_frontPos, static_cast<size_t>(len));
    m_frontPos += len;
    return true;
}

bool CPacketQueue::PeekData(int len, char* dst) const
{
    if (!dst || len <= 0) return false;
    if (GetSize() < len || !m_buf) return false;

    std::memcpy(dst, m_buf + m_frontPos, static_cast<size_t>(len));
    return true;
}

bool CPacketQueue::RemoveData(int len)
{
    if (len <= 0) return false;
    if (GetSize() < len) return false;

    m_frontPos += len;
    return true;
}

int CPacketQueue::GetSize() const
{
    return m_rearPos - m_frontPos;
}

void CPacketQueue::InsertData(int len, const char* src)
{
    if (!src || len <= 0) return;
    if (!m_buf) Init(kDefaultQueueSize);

    int capacity = static_cast<int>(m_buffer.size());

    // If there is no contiguous room at the back, compact first.
    if (m_rearPos + len > capacity) {
        if (GetSize() + len > capacity) {
            // Grow by doubling until it fits.
            int newCapacity = capacity;
            while (GetSize() + len > newCapacity) {
                newCapacity *= 2;
            }
            m_buffer.resize(static_cast<size_t>(newCapacity), 0);
            m_buf      = m_buffer.data();
            capacity   = newCapacity;
        }

        const int used = GetSize();
        std::memmove(m_buf, m_buf + m_frontPos, static_cast<size_t>(used));
        m_frontPos = 0;
        m_rearPos  = used;
    }

    std::memcpy(m_buf + m_rearPos, src, static_cast<size_t>(len));
    m_rearPos += len;
}
