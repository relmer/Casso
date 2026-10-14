#pragma once

#include "Pch.h"

#include "Core/IMachineState.h"
#include "Core/ObserverSlots.h"

class IDisk2EventSink;





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
//  terminal). A third machine runs on each nibble the program writes
//  (ObserveWrittenNibble) and fires OnDataMarkWrite on a written data
//  field.
//
//  A read stall is kStallRevolutions revolutions of nibbles read with no
//  address mark decoded and no head step: OnReadStall fires once, and
//  again only after an address mark or a step resets the count.
//
//  The watcher's state is part of the controller's saved state, so
//  loading a keyframe puts every machine back where it was, and a replay
//  from there fires the same events.
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

    // A track at 300 RPM passes about 6,400 nibbles a revolution (51,200
    // four-microsecond bit cells, 8 to a nibble).
    static constexpr uint32_t kNibblesPerRevolution  = 6400;
    static constexpr uint32_t kStallRevolutions      = 2;
    static constexpr uint32_t kStallNibbles          = kNibblesPerRevolution * kStallRevolutions;

    static constexpr uint32_t kStateTag              = IMachineState::MakeTag ('D', '2', 'A', 'M');
    static constexpr uint16_t kStateVersion          = 1;

    Disk2AddressMarkWatcher() = default;

    // Called by Disk2Controller::Read whenever the data latch returns
    // a fresh nibble to the CPU. The watcher inspects but does not
    // modify the nibble.
    void   ObserveNibble (uint8_t nibble) noexcept;

    // Called by Disk2Controller::Write with each nibble the program
    // loads into the write latch.
    void   ObserveWrittenNibble (uint8_t nibble) noexcept;

    // The head moved to quarterTrack: the read-stall count starts again.
    void   NoteHeadMoved (int quarterTrack) noexcept;

    // Every machine's state, inside the controller's state section.
    HRESULT  SaveState  (StateWriter & writer) const;
    HRESULT  LoadState  (StateReader & reader);
    void     ResetState () noexcept;

    // Sets one slot to the controller's sink for that slot. Pass
    // nullptr to detach. Safe to call from the UI thread between CPU
    // slices, matching the audio-sink attach pattern from spec-005.
    void   SetEventSink (IDisk2EventSink * sink) noexcept                      { m_eventSinks.Set (ObserverSlot::Panel, sink); }
    void   SetEventSink (ObserverSlot slot, IDisk2EventSink * sink) noexcept   { m_eventSinks.Set (slot, sink); }

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

    enum class WriteState : uint8_t
    {
        Idle    = 0,
        SawD5   = 1,
        SawAA   = 2,
        Body    = 3,
    };

    void   StepAddrMarkState (uint8_t nibble) noexcept;
    void   StepDataMarkState (uint8_t nibble) noexcept;
    void   StepWriteState    (uint8_t nibble) noexcept;
    void   CountForStall     () noexcept;

    static uint8_t Decode4and4 (uint8_t hi, uint8_t lo) noexcept
    {
        return (uint8_t) (((hi << 1) | 1) & lo);
    }

    ObserverSlots<IDisk2EventSink>  m_eventSinks;

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

    WriteState          m_writeState    = WriteState::Idle;
    uint32_t            m_writeNibCount = 0;
    uint8_t             m_writePeek0    = 0;
    uint8_t             m_writePeek1    = 0;
    uint8_t             m_writePeek2    = 0;

    uint32_t            m_stallNibbles  = 0;
    bool                m_isStallFired  = false;
    int                 m_quarterTrack  = 0;
};
