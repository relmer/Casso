#include "Pch.h"

#include "Seams/InProcessByteStream.h"





////////////////////////////////////////////////////////////////////////////////
//
//  InProcessByteStream::CreatePair
//
//  Each side reads its own queue; a write goes to the other side's.
//
////////////////////////////////////////////////////////////////////////////////

void InProcessByteStream::CreatePair (std::shared_ptr<InProcessByteStream> & outFirst, std::shared_ptr<InProcessByteStream> & outSecond)
{
    std::shared_ptr<Channel>  channel = std::make_shared<Channel>();



    outFirst  = std::shared_ptr<InProcessByteStream> (new InProcessByteStream (channel, 0));
    outSecond = std::shared_ptr<InProcessByteStream> (new InProcessByteStream (channel, 1));
}





////////////////////////////////////////////////////////////////////////////////
//
//  InProcessByteStream::Write
//
////////////////////////////////////////////////////////////////////////////////

HRESULT InProcessByteStream::Write (const Byte * data, size_t count)
{
    HRESULT  hr = S_OK;



    CBRAEx (data != nullptr || count == 0, E_INVALIDARG);

    {
        std::lock_guard<std::mutex>  guard (m_channel->lock);

        CBREx (!m_channel->closed, HRESULT_FROM_WIN32 (ERROR_BROKEN_PIPE));

        m_channel->queues[1 - m_side].insert (m_channel->queues[1 - m_side].end(), data, data + count);
    }

    m_channel->arrived.notify_all();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InProcessByteStream::Read
//
//  Waits up to the timeout for the first byte, then takes every byte that has
//  arrived, up to the capacity. Bytes written before a close remain
//  readable after it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT InProcessByteStream::Read (Byte * buffer, size_t capacity, DWORD timeoutMs, size_t & outRead)
{
    HRESULT                       hr     = S_OK;
    std::unique_lock<std::mutex>  guard  (m_channel->lock);
    std::deque<Byte>            & queue  = m_channel->queues[m_side];



    outRead = 0;

    CBRAEx (buffer != nullptr || capacity == 0, E_INVALIDARG);

    m_channel->arrived.wait_for (guard, std::chrono::milliseconds (timeoutMs), [this, &queue]()
    {
        return !queue.empty() || m_channel->closed;
    });

    while (outRead < capacity && !queue.empty())
    {
        buffer[outRead++] = queue.front();
        queue.pop_front();
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InProcessByteStream::GetReadableCount
//
////////////////////////////////////////////////////////////////////////////////

size_t InProcessByteStream::GetReadableCount()
{
    std::lock_guard<std::mutex>  guard (m_channel->lock);



    return m_channel->queues[m_side].size();
}





////////////////////////////////////////////////////////////////////////////////
//
//  InProcessByteStream::IsClosed
//
////////////////////////////////////////////////////////////////////////////////

bool InProcessByteStream::IsClosed() const
{
    std::lock_guard<std::mutex>  guard (m_channel->lock);



    return m_channel->closed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InProcessByteStream::Close
//
////////////////////////////////////////////////////////////////////////////////

void InProcessByteStream::Close()
{
    {
        std::lock_guard<std::mutex>  guard (m_channel->lock);

        m_channel->closed = true;
    }

    m_channel->arrived.notify_all();
}
