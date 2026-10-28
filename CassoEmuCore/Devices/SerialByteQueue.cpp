#include "Pch.h"

#include "SerialByteQueue.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Push
//
//  A run of bytes, each without errors.
//
////////////////////////////////////////////////////////////////////////////////

void SerialByteQueue::Push (const Byte * bytes, size_t count)
{
    lock_guard<mutex>  guard (m_lock);
    size_t             i     = 0;



    if (bytes != nullptr && count != 0)
    {
        for (i = 0; i < count; i++)
        {
            m_entries.push_back ({ bytes[i], 0 });
        }

        m_count.store (m_entries.size(), memory_order_release);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Push
//
//  One byte with the receive errors it arrived with.
//
////////////////////////////////////////////////////////////////////////////////

void SerialByteQueue::PushEntry (Byte value, Byte errors)
{
    lock_guard<mutex>  guard (m_lock);



    m_entries.push_back ({ value, errors });
    m_count.store (m_entries.size(), memory_order_release);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryPop
//
////////////////////////////////////////////////////////////////////////////////

bool SerialByteQueue::TryPop (Byte & outValue)
{
    Byte  errors = 0;



    return TryPop (outValue, errors);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryPop
//
////////////////////////////////////////////////////////////////////////////////

bool SerialByteQueue::TryPop (Byte & outValue, Byte & outErrors)
{
    lock_guard<mutex>  guard (m_lock);
    bool               popped = false;



    if (!m_entries.empty())
    {
        outValue  = m_entries.front().value;
        outErrors = m_entries.front().errors;
        m_entries.pop_front();
        m_count.store (m_entries.size(), memory_order_release);
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



    m_entries.clear();
    m_count.store (0, memory_order_release);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SerialByteQueue::CopyTo
//
////////////////////////////////////////////////////////////////////////////////

void SerialByteQueue::CopyTo (std::vector<Entry> & outEntries) const
{
    lock_guard<mutex>  guard (m_lock);



    outEntries.assign (m_entries.begin(), m_entries.end());
}
