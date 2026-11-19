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
//  InProcessByteStream::CreateVirtualPair
//
////////////////////////////////////////////////////////////////////////////////

void InProcessByteStream::CreateVirtualPair (std::shared_ptr<InProcessByteStream> & outFirst, std::shared_ptr<InProcessByteStream> & outSecond, bool isOneThread, std::shared_ptr<VirtualClock> clock)
{
    std::shared_ptr<Channel>  channel = std::make_shared<Channel>();



    channel->isVirtual   = true;
    channel->isOneThread = isOneThread;
    channel->clock       = clock ? std::move (clock) : std::make_shared<VirtualClock>();

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

    if (m_channel->isVirtual)
    {
        hr = WaitVirtual (guard, timeoutMs);
        CHR (hr);
    }
    else
    {
        m_channel->arrived.wait_for (guard, std::chrono::milliseconds (timeoutMs), [this, &queue]()
        {
            return !queue.empty() || m_channel->closed;
        });
    }

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





////////////////////////////////////////////////////////////////////////////////
//
//  InProcessByteStream::WaitVirtual
//
//  Waits for a byte, a close, or the timeout in virtual time. When the far
//  end's reader is blocked too and neither queue has a byte, the time jumps
//  to the earlier deadline. The real-time limit catches two ends that wait on
//  each other with no deadline, which is a test's mistake, not a timeout.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT InProcessByteStream::WaitVirtual (std::unique_lock<std::mutex> & guard, DWORD timeoutMs)
{
    HRESULT             hr        = S_OK;
    Channel           & channel   = *m_channel;
    std::deque<Byte>  & queue     = channel.queues[m_side];
    int                 other     = 1 - m_side;
    uint64_t            deadline  = (timeoutMs == INFINITE) ? UINT64_MAX : channel.clock->nowMs + timeoutMs;
    auto                realLimit = std::chrono::steady_clock::now() + std::chrono::milliseconds (kDeadlockMs);
    bool                isIdle    = false;
    uint64_t            next      = 0;



    channel.blocked[m_side]  = true;
    channel.deadline[m_side] = deadline;
    channel.arrived.notify_all();

    while (queue.empty() && !channel.closed && channel.clock->nowMs < deadline)
    {
        //  A far reader whose deadline has passed is about to return, so it
        //  isn't waiting on this end.
        isIdle = channel.blocked[other] && channel.deadline[other] > channel.clock->nowMs && channel.queues[other].empty();
        next   = channel.isOneThread ? deadline : isIdle ? (std::min) (deadline, channel.deadline[other]) : UINT64_MAX;

        //  One thread with no deadline waits forever for a byte only it
        //  could write.
        if (channel.isOneThread && deadline == UINT64_MAX)
        {
            hr = HRESULT_FROM_WIN32 (ERROR_TIMEOUT);
            break;
        }

        if (next != UINT64_MAX)
        {
            channel.clock->nowMs = (std::max) (channel.clock->nowMs.load(), next);
            channel.arrived.notify_all();
            continue;
        }

        if (channel.arrived.wait_until (guard, realLimit) == std::cv_status::timeout)
        {
            hr = HRESULT_FROM_WIN32 (ERROR_TIMEOUT);
            break;
        }
    }

    channel.blocked[m_side] = false;

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InProcessByteStream::GetTickMs
//
////////////////////////////////////////////////////////////////////////////////

uint64_t InProcessByteStream::GetTickMs()
{
    std::lock_guard<std::mutex>  guard (m_channel->lock);



    return m_channel->isVirtual ? m_channel->clock->nowMs.load() : GetTickCount64();
}





////////////////////////////////////////////////////////////////////////////////
//
//  InProcessByteStream::Advance
//
////////////////////////////////////////////////////////////////////////////////

void InProcessByteStream::Advance (uint64_t ms)
{
    if (!m_channel->isVirtual)
    {
        std::this_thread::sleep_for (std::chrono::milliseconds (ms));
        return;
    }

    {
        std::lock_guard<std::mutex>  guard (m_channel->lock);

        m_channel->clock->nowMs += ms;
    }

    m_channel->arrived.notify_all();
}





////////////////////////////////////////////////////////////////////////////////
//
//  InProcessByteStream::AdvanceWhenBlocked
//
////////////////////////////////////////////////////////////////////////////////

void InProcessByteStream::AdvanceWhenBlocked (uint64_t ms)
{
    {
        std::unique_lock<std::mutex>  guard (m_channel->lock);

        m_channel->arrived.wait_for (guard, std::chrono::milliseconds (kDeadlockMs), [this]() { return m_channel->blocked[1 - m_side] || m_channel->closed; });
        m_channel->clock->nowMs += ms;
    }

    m_channel->arrived.notify_all();
}





////////////////////////////////////////////////////////////////////////////////
//
//  InProcessByteStream::IsFarBlocked
//
////////////////////////////////////////////////////////////////////////////////

bool InProcessByteStream::IsFarBlocked()
{
    std::lock_guard<std::mutex>  guard (m_channel->lock);



    return m_channel->blocked[1 - m_side];
}





////////////////////////////////////////////////////////////////////////////////
//
//  InProcessByteStream::SetOneThread
//
////////////////////////////////////////////////////////////////////////////////

void InProcessByteStream::SetOneThread (bool isOneThread)
{
    {
        std::lock_guard<std::mutex>  guard (m_channel->lock);

        m_channel->isOneThread = isOneThread;
    }

    m_channel->arrived.notify_all();
}





////////////////////////////////////////////////////////////////////////////////
//
//  InProcessByteStream::Pause
//
//  On virtual time, the time moves forward by the pause, rounded up to a
//  millisecond.
//
////////////////////////////////////////////////////////////////////////////////

void InProcessByteStream::Pause (uint64_t microseconds)
{
    if (!m_channel->isVirtual)
    {
        IByteStream::Pause (microseconds);
        return;
    }

    Advance ((microseconds + 999) / 1000);
}
