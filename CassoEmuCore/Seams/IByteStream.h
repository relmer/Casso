#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IByteStream
//
//  A two-way stream of bytes between the PC and an Apple II: a COM port, a
//  TCP connection, or, inside one process, a pair of queues. The Apple II
//  link's protocol reads and writes through this alone, so its tests run on
//  the in-process pair and open no port or socket.
//
//  Writes never block for long: a serial or TCP write is buffered by the
//  system. Reads wait up to the timeout given for the first byte, then
//  return what has arrived.
//
////////////////////////////////////////////////////////////////////////////////

class IByteStream
{
public:
    virtual ~IByteStream() = default;

    virtual HRESULT  Write            (const Byte * data, size_t count) = 0;
    virtual HRESULT  Read             (Byte * buffer, size_t capacity, DWORD timeoutMs, size_t & outRead) = 0;
    virtual size_t   GetReadableCount () = 0;
    virtual bool     IsClosed         () const = 0;
    virtual void     Close            () = 0;

    //  Whether the far end reports itself ready, as DSR on a serial port that
    //  a null modem cable connects to the Apple's DTR; false when the stream
    //  has no such line.
    virtual bool     TryGetPeerReady  (bool & outReady) { outReady = false; return false; }

    //  The time on the link, in milliseconds, which every timeout on it is
    //  measured against: GetTickCount64 for a real port or socket, and the
    //  pair's virtual time for an in-process pair a test runs on it.
    virtual uint64_t GetTickMs        () { return GetTickCount64(); }

    //  A pause in what is sent, as between a bootstrap's characters and
    //  lines. A sleep is as coarse as the system timer, about 15 ms, so a
    //  shorter pause spins. A virtual-time pair moves its time instead.
    virtual void     Pause            (uint64_t microseconds)
    {
        static constexpr uint64_t  kSleepUs = 15'000;

        auto  until = std::chrono::steady_clock::now() + std::chrono::microseconds (microseconds);

        if (microseconds >= kSleepUs)
        {
            std::this_thread::sleep_until (until);
        }

        while (std::chrono::steady_clock::now() < until)
        {
            YieldProcessor();
        }
    }
};
