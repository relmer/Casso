#include "Pch.h"

#include "Machines/Apple2/Common/Disk2AddressMarkWatcher.h"
#include "Core/StateReader.h"
#include "Core/StateWriter.h"
#include "Machines/Apple2/Common/IDisk2EventSink.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ObserveNibble
//
//  Counts the nibble toward a read stall, then feeds it to both the
//  address-mark and the data-mark state machines. An address mark
//  decoded from this nibble resets the count it was just added to.
//  Each call is wait-free; the watcher MUST NOT mutate the
//  nibble stream or back-pressure the controller (FR-008). Fires sink
//  notifications synchronously from the per-state helpers when a
//  terminal-accept condition is met.
//
//  PRECONDITION: callers MUST gate on the LSS "byte ready" rising
//  edge (Disk2NibbleEngine::ConsumeFreshNibble) and pass each
//  assembled nibble exactly once. Feeding every CPU poll of $C0EC
//  here floods the state machines with repeated bytes and partial-
//  assembly garbage, and zero real D5 AA 96 / D5 AA AD prologues
//  ever match. Spec-006 regression: see Disk2Controller::Read.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2AddressMarkWatcher::ObserveNibble (uint8_t nibble) noexcept
{
    CountForStall();
    StepAddrMarkState (nibble);
    StepDataMarkState (nibble);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ObserveWrittenNibble
//
//  Feeds one nibble the program loaded into the write latch to the
//  write-side data-mark machine.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2AddressMarkWatcher::ObserveWrittenNibble (uint8_t nibble) noexcept
{
    StepWriteState (nibble);
}





////////////////////////////////////////////////////////////////////////////////
//
//  NoteHeadMoved
//
////////////////////////////////////////////////////////////////////////////////

void Disk2AddressMarkWatcher::NoteHeadMoved (int quarterTrack) noexcept
{
    m_quarterTrack = quarterTrack;
    m_stallNibbles = 0;
    m_isStallFired = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CountForStall
//
//  Adds one nibble read toward a read stall and fires OnReadStall when the
//  count is kStallNibbles. No further nibble is counted until an address
//  mark or a head step resets the count, so one stall fires once however
//  long it lasts.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2AddressMarkWatcher::CountForStall() noexcept
{
    if (m_isStallFired)
    {
        return;
    }

    m_stallNibbles++;

    if (m_stallNibbles >= kStallNibbles)
    {
        m_isStallFired = true;

        if (m_eventSinks.IsAny())
        {
            m_eventSinks.ForEach ([&] (IDisk2EventSink & sink) { sink.OnReadStall (m_quarterTrack); });
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  StepAddrMarkState
//
//  Walks the 4-and-4 address-mark state machine one nibble at a time.
//  Prologue is D5 AA 96; eight field nibbles follow (vol/trk/sec/chk in
//  hi/lo pairs). On the ChkLo terminal the watcher verifies the
//  Beneath-Apple-DOS XOR checksum (chk == vol XOR trk XOR sec) and
//  fires OnAddressMark only on a checksum hit. Random nibble streams
//  occasionally produce a spurious D5 AA 96 triplet; the checksum is
//  the false-positive guard (research.md §1.3).
//
//  Any nibble that breaks the expected sequence resets to Idle, with
//  one overlap shortcut: an unexpected D5 restarts at SawD5 instead of
//  Idle so back-to-back marks (D5 D5 AA 96 ...) re-anchor correctly.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2AddressMarkWatcher::StepAddrMarkState (uint8_t nibble) noexcept
{
    uint8_t  vol   = 0;
    uint8_t  trk   = 0;
    uint8_t  sec   = 0;
    uint8_t  chk   = 0;
    uint8_t  chkLo = 0;



    switch (m_addrState)
    {
        case AddrState::Idle:
            if (nibble == kAddrMarkPrologue0)
            {
                m_addrState = AddrState::SawD5;
            }

            break;

        case AddrState::SawD5:
            if (nibble == kAddrMarkPrologue1)
            {
                m_addrState = AddrState::SawAA;
            }
            else if (nibble == kAddrMarkPrologue0)
            {
                // Stay; D5 D5 still gives us a fresh prologue lead-in.
            }
            else
            {
                m_addrState = AddrState::Idle;
            }

            break;

        case AddrState::SawAA:
            if (nibble == kAddrMarkPrologue2)
            {
                m_addrState = AddrState::VolHi;
            }
            else if (nibble == kAddrMarkPrologue0)
            {
                m_addrState = AddrState::SawD5;
            }
            else
            {
                m_addrState = AddrState::Idle;
            }

            break;

        case AddrState::VolHi:
            m_volHi     = nibble;
            m_addrState = AddrState::VolLo;
            break;

        case AddrState::VolLo:
            m_volLo     = nibble;
            m_addrState = AddrState::TrkHi;
            break;

        case AddrState::TrkHi:
            m_trkHi     = nibble;
            m_addrState = AddrState::TrkLo;
            break;

        case AddrState::TrkLo:
            m_trkLo     = nibble;
            m_addrState = AddrState::SecHi;
            break;

        case AddrState::SecHi:
            m_secHi     = nibble;
            m_addrState = AddrState::SecLo;
            break;

        case AddrState::SecLo:
            m_secLo     = nibble;
            m_addrState = AddrState::ChkHi;
            break;

        case AddrState::ChkHi:
            m_chkHi     = nibble;
            m_addrState = AddrState::ChkLo;
            break;

        case AddrState::ChkLo:
            chkLo       = nibble;
            vol         = Decode4and4 (m_volHi, m_volLo);
            trk         = Decode4and4 (m_trkHi, m_trkLo);
            sec         = Decode4and4 (m_secHi, m_secLo);
            chk         = Decode4and4 (m_chkHi, chkLo);

            if (chk == (uint8_t) (vol ^ trk ^ sec))
            {
                m_cachedSector = (int) sec;
                m_cachedTrack  = (int) trk;
                m_cachedVolume = (int) vol;
                m_stallNibbles = 0;
                m_isStallFired = false;

                if (m_eventSinks.IsAny())
                {
                    m_eventSinks.ForEach ([&] (IDisk2EventSink & sink) { sink.OnAddressMark ((int) trk, (int) sec, (int) vol); });
                }
            }

            m_addrState = AddrState::Idle;
            break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  StepDataMarkState
//
//  Walks the 6-and-2 data-mark state machine one nibble at a time.
//  Prologue is D5 AA AD; body is 342 6-and-2 nibbles + 1 checksum
//  nibble; epilogue is DE AA EB. The watcher counts body nibbles only
//  to bound the search window — it does NOT verify the body checksum
//  (research.md §1.3: that would require 342 nibbles of buffering and
//  the false-positive cost of skipping it is just a spurious "DATA
//  READ" entry, which is harmless).
//
//  Epilogue detection uses a 3-nibble rolling window (m_peek2,
//  m_peek1, m_peek0). Once we're in Body, every incoming nibble shifts
//  the window forward; a window match of DE AA EB fires
//  OnDataMarkRead. The cached sector number from the most recent
//  OnAddressMark accompanies the fire; if no address mark preceded
//  this data mark the cached value is -1 and the UI formats it as
//  "S?" (research.md / spec FR-008).
//
//  Slack guard: if the body runs past kDataNibbleCount +
//  kDataNibbleCountSlack without an epilogue hit we silently reset to
//  Idle. A protected loader may produce no epilogue at all; this guard
//  keeps the state machine from latching forever.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2AddressMarkWatcher::StepDataMarkState (uint8_t nibble) noexcept
{
    m_peek2 = m_peek1;
    m_peek1 = m_peek0;
    m_peek0 = nibble;



    switch (m_dataState)
    {
        case DataState::Idle:
            if (nibble == kAddrMarkPrologue0)
            {
                m_dataState = DataState::SawD5;
            }

            break;

        case DataState::SawD5:
            if (nibble == kAddrMarkPrologue1)
            {
                m_dataState = DataState::SawAA;
            }
            else if (nibble == kAddrMarkPrologue0)
            {
                // Stay; D5 D5 still gives us a fresh prologue lead-in.
            }
            else
            {
                m_dataState = DataState::Idle;
            }

            break;

        case DataState::SawAA:
            if (nibble == kDataMarkPrologue2)
            {
                m_dataState    = DataState::Body;
                m_dataNibCount = 0;
            }
            else if (nibble == kAddrMarkPrologue0)
            {
                m_dataState = DataState::SawD5;
            }
            else
            {
                m_dataState = DataState::Idle;
            }

            break;

        case DataState::Body:
            m_dataNibCount++;

            if (m_peek2 == kSectorEpilogue0 &&
                m_peek1 == kSectorEpilogue1 &&
                m_peek0 == kSectorEpilogue2)
            {
                if (m_eventSinks.IsAny())
                {
                    m_eventSinks.ForEach ([&] (IDisk2EventSink & sink) { sink.OnDataMarkRead (m_cachedTrack, m_cachedSector, m_cachedVolume, 256); });
                }

                m_dataState = DataState::Idle;
            }
            else if (m_dataNibCount > (kDataNibbleCount + kDataNibbleCountSlack))
            {
                m_dataState = DataState::Idle;
            }

            break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  StepWriteState
//
//  The write-side twin of StepDataMarkState, run on the nibbles the
//  program writes: D5 AA AD opens a data field, DE AA EB within the slack
//  window closes it and fires OnDataMarkWrite with the track, sector and
//  volume of the address mark read last, which is the one RWTS found
//  before it switched to writing. Written address fields (a format) fire
//  nothing.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2AddressMarkWatcher::StepWriteState (uint8_t nibble) noexcept
{
    m_writePeek2 = m_writePeek1;
    m_writePeek1 = m_writePeek0;
    m_writePeek0 = nibble;



    switch (m_writeState)
    {
        case WriteState::Idle:
            if (nibble == kAddrMarkPrologue0)
            {
                m_writeState = WriteState::SawD5;
            }

            break;

        case WriteState::SawD5:
            if (nibble == kAddrMarkPrologue1)
            {
                m_writeState = WriteState::SawAA;
            }
            else if (nibble != kAddrMarkPrologue0)
            {
                m_writeState = WriteState::Idle;
            }

            break;

        case WriteState::SawAA:
            if (nibble == kDataMarkPrologue2)
            {
                m_writeState    = WriteState::Body;
                m_writeNibCount = 0;
            }
            else if (nibble == kAddrMarkPrologue0)
            {
                m_writeState = WriteState::SawD5;
            }
            else
            {
                m_writeState = WriteState::Idle;
            }

            break;

        case WriteState::Body:
            m_writeNibCount++;

            if (m_writePeek2 == kSectorEpilogue0 &&
                m_writePeek1 == kSectorEpilogue1 &&
                m_writePeek0 == kSectorEpilogue2)
            {
                if (m_eventSinks.IsAny())
                {
                    m_eventSinks.ForEach ([&] (IDisk2EventSink & sink) { sink.OnDataMarkWrite (m_cachedTrack, m_cachedSector, m_cachedVolume, 256); });
                }

                m_writeState = WriteState::Idle;
            }
            else if (m_writeNibCount > (kDataNibbleCount + kDataNibbleCountSlack))
            {
                m_writeState = WriteState::Idle;
            }

            break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SaveState
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Disk2AddressMarkWatcher::SaveState (StateWriter & writer) const
{
    writer.BeginSection (kStateTag, kStateVersion);

    writer.WriteByte   ((Byte) m_addrState);
    writer.WriteByte   (m_volHi);
    writer.WriteByte   (m_volLo);
    writer.WriteByte   (m_trkHi);
    writer.WriteByte   (m_trkLo);
    writer.WriteByte   (m_secHi);
    writer.WriteByte   (m_secLo);
    writer.WriteByte   (m_chkHi);

    writer.WriteByte   ((Byte) m_dataState);
    writer.WriteUInt32 (m_dataNibCount);
    writer.WriteByte   (m_peek0);
    writer.WriteByte   (m_peek1);
    writer.WriteByte   (m_peek2);

    writer.WriteUInt32 ((uint32_t) m_cachedSector);
    writer.WriteUInt32 ((uint32_t) m_cachedTrack);
    writer.WriteUInt32 ((uint32_t) m_cachedVolume);

    writer.WriteByte   ((Byte) m_writeState);
    writer.WriteUInt32 (m_writeNibCount);
    writer.WriteByte   (m_writePeek0);
    writer.WriteByte   (m_writePeek1);
    writer.WriteByte   (m_writePeek2);

    writer.WriteUInt32 (m_stallNibbles);
    writer.WriteBool   (m_isStallFired);
    writer.WriteUInt32 ((uint32_t) m_quarterTrack);

    return writer.EndSection();
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadState
//
//  Reads into locals and checks each machine's state against its range
//  before committing anything.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Disk2AddressMarkWatcher::LoadState (StateReader & reader)
{
    HRESULT   hr            = S_OK;
    uint16_t  version       = 0;
    Byte      addrState     = 0;
    Byte      volHi         = 0;
    Byte      volLo         = 0;
    Byte      trkHi         = 0;
    Byte      trkLo         = 0;
    Byte      secHi         = 0;
    Byte      secLo         = 0;
    Byte      chkHi         = 0;
    Byte      dataState     = 0;
    uint32_t  dataNibCount  = 0;
    Byte      peek0         = 0;
    Byte      peek1         = 0;
    Byte      peek2         = 0;
    uint32_t  cachedSector  = 0;
    uint32_t  cachedTrack   = 0;
    uint32_t  cachedVolume  = 0;
    Byte      writeState    = 0;
    uint32_t  writeNibCount = 0;
    Byte      writePeek0    = 0;
    Byte      writePeek1    = 0;
    Byte      writePeek2    = 0;
    uint32_t  stallNibbles  = 0;
    bool      isStallFired  = false;
    uint32_t  quarterTrack  = 0;



    hr = reader.BeginSection (kStateTag, kStateVersion, version);
    CHR (hr);

    reader.ReadByte   (addrState);
    reader.ReadByte   (volHi);
    reader.ReadByte   (volLo);
    reader.ReadByte   (trkHi);
    reader.ReadByte   (trkLo);
    reader.ReadByte   (secHi);
    reader.ReadByte   (secLo);
    reader.ReadByte   (chkHi);

    reader.ReadByte   (dataState);
    reader.ReadUInt32 (dataNibCount);
    reader.ReadByte   (peek0);
    reader.ReadByte   (peek1);
    reader.ReadByte   (peek2);

    reader.ReadUInt32 (cachedSector);
    reader.ReadUInt32 (cachedTrack);
    reader.ReadUInt32 (cachedVolume);

    reader.ReadByte   (writeState);
    reader.ReadUInt32 (writeNibCount);
    reader.ReadByte   (writePeek0);
    reader.ReadByte   (writePeek1);
    reader.ReadByte   (writePeek2);

    reader.ReadUInt32 (stallNibbles);
    reader.ReadBool   (isStallFired);
    reader.ReadUInt32 (quarterTrack);

    hr = reader.EndSection();
    CHR (hr);

    CBREx (addrState    <= (Byte) AddrState::ChkLo, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
    CBREx (dataState    <= (Byte) DataState::Body,  HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
    CBREx (writeState   <= (Byte) WriteState::Body, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
    CBREx (stallNibbles <= kStallNibbles,           HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    m_addrState     = (AddrState) addrState;
    m_volHi         = volHi;
    m_volLo         = volLo;
    m_trkHi         = trkHi;
    m_trkLo         = trkLo;
    m_secHi         = secHi;
    m_secLo         = secLo;
    m_chkHi         = chkHi;
    m_dataState     = (DataState) dataState;
    m_dataNibCount  = dataNibCount;
    m_peek0         = peek0;
    m_peek1         = peek1;
    m_peek2         = peek2;
    m_cachedSector  = (int) cachedSector;
    m_cachedTrack   = (int) cachedTrack;
    m_cachedVolume  = (int) cachedVolume;
    m_writeState    = (WriteState) writeState;
    m_writeNibCount = writeNibCount;
    m_writePeek0    = writePeek0;
    m_writePeek1    = writePeek1;
    m_writePeek2    = writePeek2;
    m_stallNibbles  = stallNibbles;
    m_isStallFired  = isStallFired;
    m_quarterTrack  = (int) quarterTrack;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ResetState
//
//  Every machine back to Idle and the caches to unknown, as at power-on.
//  The listeners are set again after the reset.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2AddressMarkWatcher::ResetState() noexcept
{
    ObserverSlots<IDisk2EventSink>  sinks = m_eventSinks;



    *this        = Disk2AddressMarkWatcher();
    m_eventSinks = sinks;
}
