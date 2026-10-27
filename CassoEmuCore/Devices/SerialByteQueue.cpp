#include "Pch.h"

#include "SerialByteQueue.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Push
//
////////////////////////////////////////////////////////////////////////////////

void SerialByteQueue::Push (const Byte * bytes, size_t count)
{
    lock_guard<mutex>  guard (m_lock);



    if (bytes != nullptr && count != 0)
    {
        m_bytes.insert (m_bytes.end(), bytes, bytes + count);
        m_count.store (m_bytes.size(), memory_order_release);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryPop
//
////////////////////////////////////////////////////////////////////////////////

bool SerialByteQueue::TryPop (Byte & outValue)
{
    lock_guard<mutex>  guard (m_lock);
    bool               popped = false;



    if (!m_bytes.empty())
    {
        outValue = m_bytes.front();
        m_bytes.pop_front();
        m_count.store (m_bytes.size(), memory_order_release);
        popped = true;
    }

    return popped;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Clear
//
////////////////////////////////////////////////////////////////////////////////

void SerialByteQueue::Clear()
{
    lock_guard<mutex>  guard (m_lock);



    m_bytes.clear();
    m_count.store (0, memory_order_release);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SerialByteQueue::CopyTo
//
////////////////////////////////////////////////////////////////////////////////

void SerialByteQueue::CopyTo (std::vector<Byte> & outBytes) const
{
    lock_guard<mutex>  guard (m_lock);



    outBytes.assign (m_bytes.begin(), m_bytes.end());
}
