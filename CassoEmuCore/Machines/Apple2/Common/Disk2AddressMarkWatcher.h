#pragma once

#include "Pch.h"

#include "Core/IMachineState.h"
#include "Core/ObserverSlots.h"
#include "../CassoCore/DiskFieldKind.h"

class IDisk2EventSink;





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2AddressMarkWatcher
//
//  Passive nibble-stream observer that decodes Disk II address and data
//  fields. Lives inside Disk2Controller and receives every nibble the CPU
//  read from the data latch, and every nibble the program wrote.
//
//  Read-only invariant (FR-008): the watcher MUST NOT mutate the nibble
//  stream and must not back-pressure the controller.
//
//  A field is recognized by the nibbles on the track, not by a DOS
//  version: an address field opens with D5 AA 96 (16-sector) or D5 AA B5
//  (13-sector), and a data field with D5 AA AD. A data field's body is
//  buffered up to the DE AA epilogue, and its length selects the encoding:
//  343 nibbles (342 and the checksum) are 6-and-2, 411 are 5-and-3. Fields
//  in any other format fire nothing.
//
//  The controller passes the watcher only the nibbles the CPU read, so when
//  the CPU pauses longer than kGapCycles between them, nibbles went by
//  unread. A field in progress at a gap is dropped without an event: the
//  program stopped reading, which is not a fault on the disk.
//
//  The errors and warnings, each through its own sink call:
//
//    * an address field whose checksum fails,
//    * a data field whose checksum fails,
//    * a data field cut off before its epilogue: a D5 inside the body, a
//      body of neither length, or no epilogue at all,
//    * a data field with no address field since the last data field and
//      no gap in between, and
//    * a read stall: kStallRevolutions revolutions of nibbles read with no
//      address field decoded and no head step. OnReadStall fires once, and
//      again only after an address field or a step resets the count.
//
//  A data field's bytes are decoded only while a sink is attached; the
//  watcher's state does not depend on them, so a machine with sinks and one
//  without save the same state.
//
//  The watcher's state is part of the controller's saved state, so
//  loading a keyframe puts every machine back where it was, and a replay
//  from there fires the same events.
//
////////////////////////////////////////////////////////////////////////////////

class Disk2AddressMarkWatcher
{
public:
    // Disk II 4-and-4 address-field prologue (Beneath Apple DOS p. 3-18).
    // The data-field prologue shares the lead-in; only the third byte
    // differs.
    static constexpr uint8_t  kAddrMarkPrologue0  = 0xD5;
    static constexpr uint8_t  kAddrMarkPrologue1  = 0xAA;
    static constexpr uint8_t  kAddrMarkPrologue2  = 0x96;
    static constexpr uint8_t  kAddrMarkPrologue13 = 0xB5;
    static constexpr uint8_t  kDataMarkPrologue2  = 0xAD;
    static constexpr uint8_t  kSectorEpilogue0    = 0xDE;
    static constexpr uint8_t  kSectorEpilogue1    = 0xAA;
    static constexpr uint8_t  kSectorEpilogue2    = 0xEB;

    // A data field's body and checksum nibble: 342 and 1 for 6-and-2, 410
    // and 1 for 5-and-3. The buffer has room for the longer and its two
    // epilogue nibbles.
    static constexpr uint32_t kBodyNibbles62       = 343;
    static constexpr uint32_t kBodyNibbles53       = 411;
    static constexpr uint32_t kMaxFieldNibbles     = kBodyNibbles53 + 2;

    // A track at 300 RPM passes about 6,400 nibbles a revolution (51,200
    // four-microsecond bit cells, 8 to a nibble).
    static constexpr uint32_t kNibblesPerRevolution = 6400;
    static constexpr uint32_t kStallRevolutions     = 2;
    static constexpr uint32_t kStallNibbles         = kNibblesPerRevolution * kStallRevolutions;

    // A nibble passes every 32 cycles, a sync nibble every 40, and a read
    // loop polls every 7 or so, so reads of nibbles in a row are at most
    // about 47 cycles apart and a nibble the CPU missed puts them at least
    // 57 apart.
    static constexpr uint64_t kGapCycles            = 52;

    static constexpr uint32_t kStateTag             = IMachineState::MakeTag ('D', '2', 'A', 'M');
    static constexpr uint16_t kStateVersion         = 2;

    Disk2AddressMarkWatcher() = default;

    // Called by Disk2Controller::Read whenever the data latch returns a
    // fresh nibble to the CPU, with the CPU's cycle then; 0 is a cycle
    // not known, which marks no gap. The watcher inspects but does not
    // modify the nibble.
    void   ObserveNibble (uint8_t nibble, uint64_t cycle = 0) noexcept;

    // Called by Disk2Controller::Write with each nibble the program loads
    // into the write latch.
    void   ObserveWrittenNibble (uint8_t nibble) noexcept;

    // The head moved to quarterTrack: the read-stall count starts again,
    // and a data field in progress is dropped.
    void   NoteHeadMoved (int quarterTrack) noexcept;

    // Every machine's state, inside the controller's state section.
    HRESULT  SaveState  (StateWriter & writer) const;
    HRESULT  LoadState  (StateReader & reader);
    void     ResetState () noexcept;

    // Sets one slot to the controller's sink for that slot. Pass nullptr
    // to detach. Safe to call from the UI thread between CPU slices,
    // matching the audio-sink attach pattern from spec-005.
    void   SetEventSink (IDisk2EventSink * sink) noexcept                      { m_eventSinks.Set (ObserverSlot::Panel, sink); }
    void   SetEventSink (ObserverSlot slot, IDisk2EventSink * sink) noexcept   { m_eventSinks.Set (slot, sink); }

    // The sector, track and volume of the latest good address field (-1
    // until the first). A data field's events use them when the field
    // follows its address field; the UI formats -1 as "?" (FR-008).
    int    GetCachedSector() const noexcept { return m_cachedSector; }
    int    GetCachedTrack()  const noexcept { return m_cachedTrack;  }
    int    GetCachedVolume() const noexcept { return m_cachedVolume; }

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

    enum class FieldState : uint8_t
    {
        Idle    = 0,
        SawD5   = 1,
        SawAA   = 2,
        Body    = 3,
    };

    // One data field's nibbles, read or written, from after its prologue.
    struct DataField
    {
        FieldState                             state   = FieldState::Idle;
        uint32_t                               count   = 0;
        std::array<uint8_t, kMaxFieldNibbles>  nibbles = {};
    };

    enum class FieldEnd : uint8_t
    {
        Open,
        Complete,
        CutOff,
    };

    void      StepAddrMarkState (uint8_t nibble) noexcept;
    void      StepDataMarkState (uint8_t nibble) noexcept;
    void      StepWriteState    (uint8_t nibble) noexcept;
    void      CountForStall     () noexcept;
    void      BeginReadField    () noexcept;
    void      FinishReadField   () noexcept;
    void      FireCutOff        () noexcept;

    static FieldEnd  StepField  (DataField & field, uint8_t nibble) noexcept;
    static bool      TryDecode  (const DataField & field, std::array<Byte, 256> & outBytes, bool & outIsGood);

    static HRESULT   SaveField  (StateWriter & writer, const DataField & field);
    static HRESULT   LoadField  (StateReader & reader, DataField & field);

    static uint8_t Decode4and4 (uint8_t hi, uint8_t lo) noexcept
    {
        return (uint8_t) (((hi << 1) | 1) & lo);
    }

    ObserverSlots<IDisk2EventSink>  m_eventSinks;

    AddrState           m_addrState         = AddrState::Idle;
    uint8_t             m_volHi             = 0;
    uint8_t             m_volLo             = 0;
    uint8_t             m_trkHi             = 0;
    uint8_t             m_trkLo             = 0;
    uint8_t             m_secHi             = 0;
    uint8_t             m_secLo             = 0;
    uint8_t             m_chkHi             = 0;

    int                 m_cachedSector      = -1;
    int                 m_cachedTrack       = -1;
    int                 m_cachedVolume      = -1;

    // An address field was read and no data field has started since, and
    // whether a gap came after the last address field.
    bool                m_isAddressPending  = false;
    bool                m_isGapSinceAddress = true;

    // The data field being read, and the address it follows: -1 for a
    // field with no address field before it, which is an orphan unless a
    // gap came after the last address field.
    DataField           m_read;
    int                 m_readTrack         = -1;
    int                 m_readSector        = -1;
    int                 m_readVolume        = -1;
    bool                m_isReadOrphan      = false;

    DataField           m_write;

    uint64_t            m_lastCycle         = 0;

    uint32_t            m_stallNibbles      = 0;
    bool                m_isStallFired      = false;
    int                 m_quarterTrack      = 0;
};
