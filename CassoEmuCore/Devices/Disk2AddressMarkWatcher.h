#pragma once

#include "Pch.h"

#include "IDisk2EventSink.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2AddressMarkWatcher
//
//  Passive nibble-stream observer that decodes Disk II address marks
//  and detects 6-and-2 data-mark frames. Lives inside Disk2Controller
//  and receives every nibble that the CPU read from the data latch.
//
//  Read-only invariant (FR-008): the watcher MUST NOT mutate the
//  nibble stream, must not back-pressure the controller, must not
//  buffer beyond the small 3-nibble peek-ahead window required by
//  the data-mark epilogue check. With no sink attached the watcher
//  still observes (the cached sector number is maintained) but never
//  fires.
//
//  Both state machines run on every ObserveNibble call. They share
//  only the cached most-recent sector number (written by the
//  address-mark accept terminal, read by the data-mark accept
//  terminal).
//
////////////////////////////////////////////////////////////////////////////////

class Disk2AddressMarkWatcher
{
public:
    // Disk II 4-and-4 address-mark prologue / epilogue (Beneath Apple
    // DOS p. 3-18). Same prologue lead-in for the 6-and-2 data mark;
    // only the third byte distinguishes them.
    static constexpr uint8_t  kAddrMarkPrologue0 = 0xD5;
    static constexpr uint8_t  kAddrMarkPrologue1 = 0xAA;
    static constexpr uint8_t  kAddrMarkPrologue2 = 0x96;
    static constexpr uint8_t  kDataMarkPrologue2 = 0xAD;
    static constexpr uint8_t  kSectorEpilogue0   = 0xDE;
    static constexpr uint8_t  kSectorEpilogue1   = 0xAA;
    static constexpr uint8_t  kSectorEpilogue2   = 0xEB;

    // 256 data bytes -> 342 6-and-2 nibbles + 1 checksum nibble.
    // Allow a small slack for protected disks that pad the body
    // slightly; if no epilogue is seen within the slack window we
    // silently reset (corrupt frame guard, no event fired).
    static constexpr uint32_t kDataNibbleCount       = 342;
    static constexpr uint32_t kDataNibbleCountSlack  = 16;

    Disk2AddressMarkWatcher() = default;

    // Called by Disk2Controller::Read whenever the data latch returns
    // a fresh nibble to the CPU. The watcher inspects but does not
    // modify the nibble.
    void   ObserveNibble (uint8_t nibble) noexcept;

    // Propagates the controller's sink pointer. Pass nullptr to
    // detach. Safe to call from the UI thread between CPU slices,
    // matching the audio-sink attach pattern from spec-005.
    void   SetEventSink (IDisk2EventSink * sink) noexcept   { m_eventSink = sink; }

    // Test seam: most recent fully-decoded sector / track / volume
    // numbers from the latest address mark (-1 until the first
    // successful address mark). Read by tests and by the data-mark
    // accept path so a data mark seen without a preceding address
    // mark still fires; the UI formats -1 as "?" (FR-008).
    int    GetCachedSector() const noexcept                    { return m_cachedSector; }
    int    GetCachedTrack() const noexcept                    { return m_cachedTrack;  }
    int    GetCachedVolume() const noexcept                    { return m_cachedVolume; }

private:
    enum class AddrState : uint8_t
    {
        Idle    = 0,
        SawD5   = 1,
        SawAA   = 2,
        VolHi   = 3,
        VolLo   = 4,
        TrkHi   = 5,
        TrkLo   = 6,
        SecHi   = 7,
        SecLo   = 8,
        ChkHi   = 9,
        ChkLo   = 10,
    };

    enum class DataState : uint8_t
    {
        Idle    = 0,
        SawD5   = 1,
        SawAA   = 2,
        Body    = 3,
    };

    void   StepAddrMarkState (uint8_t nibble) noexcept;
    void   StepDataMarkState (uint8_t nibble) noexcept;

    static uint8_t Decode4and4 (uint8_t hi, uint8_t lo) noexcept
    {
        return (uint8_t) (((hi << 1) | 1) & lo);
    }

    IDisk2EventSink *  m_eventSink     = nullptr;

    AddrState           m_addrState     = AddrState::Idle;
    uint8_t             m_volHi         = 0;
    uint8_t             m_volLo         = 0;
    uint8_t             m_trkHi         = 0;
    uint8_t             m_trkLo         = 0;
    uint8_t             m_secHi         = 0;
    uint8_t             m_secLo         = 0;
    uint8_t             m_chkHi         = 0;

    DataState           m_dataState     = DataState::Idle;
    uint32_t            m_dataNibCount  = 0;
    uint8_t             m_peek0         = 0;   // most recent
    uint8_t             m_peek1         = 0;   // 1 back
    uint8_t             m_peek2         = 0;   // 2 back (oldest in window)

    int                 m_cachedSector  = -1;
    int                 m_cachedTrack   = -1;
    int                 m_cachedVolume  = -1;
};
