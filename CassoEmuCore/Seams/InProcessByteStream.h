#pragma once

#include "Pch.h"

#include "Seams/IByteStream.h"





////////////////////////////////////////////////////////////////////////////////
//
//  InProcessByteStream
//
//  One end of a pair of linked streams in one process: what one end writes,
//  the other reads. Used where the emulated machine and the link run in the
//  same process, as in every test. Either end may be used from any thread.
//  Closing either end closes both; bytes written before it remain readable.
//
////////////////////////////////////////////////////////////////////////////////

class InProcessByteStream : public IByteStream
{
public:
    static void  CreatePair (std::shared_ptr<InProcessByteStream> & outFirst, std::shared_ptr<InProcessByteStream> & outSecond);

    HRESULT  Write            (const Byte * data, size_t count) override;
    HRESULT  Read             (Byte * buffer, size_t capacity, DWORD timeoutMs, size_t & outRead) override;
    size_t   GetReadableCount () override;
    bool     IsClosed         () const override;
    void     Close            () override;

private:
    struct Channel
    {
        std::mutex               lock;
        std::condition_variable  arrived;
        std::deque<Byte>         queues[2];
        bool                     closed = false;
    };

    InProcessByteStream (std::shared_ptr<Channel> channel, int side) : m_channel (std::move (channel)), m_side (side) {}

    std::shared_ptr<Channel>  m_channel;
    int                       m_side = 0;
};
