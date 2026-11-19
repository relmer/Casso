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
//  A pair created with virtual time measures its timeouts in virtual
//  milliseconds. When a reader is blocked at each end and neither queue has a
//  byte, nothing more can arrive until a timeout passes, so the time jumps to
//  the earlier of the two deadlines at once. A timeout then costs nothing, and
//  a test on the pair passes at the same point on every run. A pair that one
//  thread uses for both ends has nothing else to wait for, so a read with
//  nothing queued jumps to that read's deadline. Pairs created with one
//  VirtualClock share its time, as the pairs a factory opens one after
//  another for one link do.
//
////////////////////////////////////////////////////////////////////////////////

class InProcessByteStream : public IByteStream
{
public:
    //  The virtual time that pairs share, in milliseconds. It starts an hour
    //  in, as a PC's tick count does, since code that reads it may take zero
    //  to mean "never".
    struct VirtualClock
    {
        static constexpr uint64_t  kStartMs = 3'600'000;

        std::atomic<uint64_t>  nowMs { kStartMs };
    };

    //  Past this many real milliseconds with both ends blocked and neither
    //  deadline finite, a virtual-time read fails with ERROR_TIMEOUT, since
    //  the two ends are waiting on each other.
    static constexpr DWORD  kDeadlockMs = 10000;

    static void  CreatePair        (std::shared_ptr<InProcessByteStream> & outFirst, std::shared_ptr<InProcessByteStream> & outSecond);
    static void  CreateVirtualPair (std::shared_ptr<InProcessByteStream> & outFirst, std::shared_ptr<InProcessByteStream> & outSecond, bool isOneThread = false, std::shared_ptr<VirtualClock> clock = nullptr);

    HRESULT   Write            (const Byte * data, size_t count) override;
    HRESULT   Read             (Byte * buffer, size_t capacity, DWORD timeoutMs, size_t & outRead) override;
    size_t    GetReadableCount () override;
    bool      IsClosed         () const override;
    void      Close            () override;
    uint64_t  GetTickMs        () override;
    void      Pause            (uint64_t microseconds) override;

    //  Moves a virtual-time pair's time forward, as when a test acts after a
    //  pause. On a real-time pair it waits that long.
    void      Advance          (uint64_t ms);

    //  Waits for a reader to block at the far end, then moves the time
    //  forward, as when a test pauses while the far end waits.
    void      AdvanceWhenBlocked (uint64_t ms);

    //  Whether a reader is blocked at the far end.
    bool      IsFarBlocked     ();

    //  Whether one thread uses both ends of a virtual-time pair, as when a
    //  line with no Apple at first gets one later.
    void      SetOneThread     (bool isOneThread);

private:
    struct Channel
    {
        std::mutex                     lock;
        std::condition_variable        arrived;
        std::deque<Byte>               queues[2];
        bool                           closed      = false;
        bool                           isVirtual   = false;
        bool                           isOneThread = false;   // one thread uses both ends
        std::shared_ptr<VirtualClock>  clock;   // set for a virtual-time pair
        bool                           blocked[2]  = { false, false };   // a reader waits at that end
        uint64_t                       deadline[2] = { 0, 0 };
    };

    InProcessByteStream (std::shared_ptr<Channel> channel, int side) : m_channel (std::move (channel)), m_side (side) {}

    HRESULT  WaitVirtual (std::unique_lock<std::mutex> & guard, DWORD timeoutMs);

    std::shared_ptr<Channel>  m_channel;
    int                       m_side = 0;
};
