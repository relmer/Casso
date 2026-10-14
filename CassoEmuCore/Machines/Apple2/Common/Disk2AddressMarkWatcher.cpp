#include "Pch.h"

#include "Machines/Apple2/Common/Disk2AddressMarkWatcher.h"
#include "Core/StateReader.h"
#include "Core/StateWriter.h"
#include "Devices/Disk/DiskFieldFormat.h"
#include "Machines/Apple2/Common/IDisk2EventSink.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ObserveNibble
//
//  A gap since the last nibble drops whatever was in progress, then the
//  nibble counts toward a read stall and goes to both the address-field
//  and the data-field machines. An address field decoded from this nibble
//  resets the count it was just added to. Each call is wait-free; the
//  watcher MUST NOT mutate the nibble stream or back-pressure the
//  controller (FR-008).
//
//  PRECONDITION: callers MUST gate on the LSS "byte ready" rising edge
//  (Disk2NibbleEngine::ConsumeFreshNibble) and pass each assembled nibble
//  exactly once. Feeding every CPU poll of $C0EC here floods the state
//  machines with repeated bytes and partial-assembly garbage, and no real
//  prologue ever matches. Spec-006 regression: see Disk2Controller::Read.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2AddressMarkWatcher::ObserveNibble (uint8_t nibble, uint64_t cycle) noexcept
{
    bool  isGap = cycle != 0 && m_lastCycle != 0 && cycle - m_lastCycle > kGapCycles;



    if (cycle != 0)
    {
        m_lastCycle = cycle;
    }

    if (isGap)
    {
        m_addrState         = AddrState::Idle;
        m_read.state        = FieldState::Idle;
        m_isGapSinceAddress = true;
    }

    CountForStall();
    StepAddrMarkState (nibble);
    StepDataMarkState (nibble);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ObserveWrittenNibble
//
//  Feeds one nibble the program loaded into the write latch to the
//  write-side data-field machine.
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
//  A field half read on the track the head left has no end to read.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2AddressMarkWatcher::NoteHeadMoved (int quarterTrack) noexcept
{
    m_quarterTrack      = quarterTrack;
    m_stallNibbles      = 0;
    m_isStallFired      = false;
    m_addrState         = AddrState::Idle;
    m_read.state        = FieldState::Idle;
    m_isAddressPending  = false;
    m_isGapSinceAddress = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CountForStall
//
//  Adds one nibble read toward a read stall and fires OnReadStall when the
//  count is kStallNibbles. No further nibble is counted until an address
//  field or a head step resets the count, so one stall fires once however
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
//  Walks the 4-and-4 address-field machine one nibble at a time. The
//  prologue is D5 AA 96 or D5 AA B5; eight field nibbles follow (vol, trk,
//  sec and chk in hi/lo pairs). On the ChkLo terminal the watcher verifies
//  the XOR checksum (chk == vol XOR trk XOR sec) and fires OnAddressMark on
//  a match and OnAddressChecksumError otherwise. D5 and AA are in neither
//  translate table, so a prologue never appears inside a data field's body.
//
//  Any nibble out of the prologue's sequence resets to Idle, with one
//  overlap shortcut: an unexpected D5 restarts at SawD5 instead of Idle so
//  back-to-back marks (D5 D5 AA 96 ...) re-anchor correctly.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2AddressMarkWatcher::StepAddrMarkState (uint8_t nibble) noexcept
{
    uint8_t  vol   = 0;
    uint8_t  trk   = 0;
    uint8_t  sec   = 0;
    uint8_t  chk   = 0;



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
            else if (nibble != kAddrMarkPrologue0)
            {
                m_addrState = AddrState::Idle;
            }

            break;

        case AddrState::SawAA:
            if (nibble == kAddrMarkPrologue2 || nibble == kAddrMarkPrologue13)
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

        case AddrState::VolHi:  m_volHi = nibble;  m_addrState = AddrState::VolLo;  break;
        case AddrState::VolLo:  m_volLo = nibble;  m_addrState = AddrState::TrkHi;  break;
        case AddrState::TrkHi:  m_trkHi = nibble;  m_addrState = AddrState::TrkLo;  break;
        case AddrState::TrkLo:  m_trkLo = nibble;  m_addrState = AddrState::SecHi;  break;
        case AddrState::SecHi:  m_secHi = nibble;  m_addrState = AddrState::SecLo;  break;
        case AddrState::SecLo:  m_secLo = nibble;  m_addrState = AddrState::ChkHi;  break;
        case AddrState::ChkHi:  m_chkHi = nibble;  m_addrState = AddrState::ChkLo;  break;

        case AddrState::ChkLo:
            vol         = Decode4and4 (m_volHi, m_volLo);
            trk         = Decode4and4 (m_trkHi, m_trkLo);
            sec         = Decode4and4 (m_secHi, m_secLo);
            chk         = Decode4and4 (m_chkHi, nibble);
            m_addrState = AddrState::Idle;

            if (chk != (uint8_t) (vol ^ trk ^ sec))
            {
                if (m_eventSinks.IsAny())
                {
                    m_eventSinks.ForEach ([&] (IDisk2EventSink & sink) { sink.OnAddressChecksumError ((int) trk, (int) sec, (int) vol); });
                }

                break;
            }

            m_cachedSector      = (int) sec;
            m_cachedTrack       = (int) trk;
            m_cachedVolume      = (int) vol;
            m_isAddressPending  = true;
            m_isGapSinceAddress = false;
            m_stallNibbles      = 0;
            m_isStallFired      = false;

            if (m_eventSinks.IsAny())
            {
                m_eventSinks.ForEach ([&] (IDisk2EventSink & sink) { sink.OnAddressMark ((int) trk, (int) sec, (int) vol); });
            }

            break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  StepField
//
//  One nibble of a data field, read or written: the D5 AA AD prologue,
//  then the body until DE AA. A D5 inside the body, which no translate
//  table has, or a body past the longest length, ends the field cut off.
//  Back to Idle at either end.
//
////////////////////////////////////////////////////////////////////////////////

Disk2AddressMarkWatcher::FieldEnd Disk2AddressMarkWatcher::StepField (DataField & field, uint8_t nibble) noexcept
{
    FieldEnd  end = FieldEnd::Open;



    switch (field.state)
    {
        case FieldState::Idle:
            field.state = (nibble == kAddrMarkPrologue0) ? FieldState::SawD5 : FieldState::Idle;
            break;

        case FieldState::SawD5:
            if (nibble == kAddrMarkPrologue1)
            {
                field.state = FieldState::SawAA;
            }
            else if (nibble != kAddrMarkPrologue0)
            {
                field.state = FieldState::Idle;
            }

            break;

        case FieldState::SawAA:
            if (nibble == kDataMarkPrologue2)
            {
                field.state = FieldState::Body;
                field.count = 0;
            }
            else
            {
                field.state = (nibble == kAddrMarkPrologue0) ? FieldState::SawD5 : FieldState::Idle;
            }

            break;

        case FieldState::Body:
            if (nibble == kAddrMarkPrologue0)
            {
                field.state = FieldState::SawD5;
                end         = FieldEnd::CutOff;
                break;
            }

            field.nibbles[field.count] = nibble;
            field.count++;

            if (field.count >= 2 && field.nibbles[field.count - 2] == kSectorEpilogue0 && nibble == kSectorEpilogue1)
            {
                field.count -= 2;
                field.state  = FieldState::Idle;
                end          = (field.count == kBodyNibbles62 || field.count == kBodyNibbles53) ? FieldEnd::Complete : FieldEnd::CutOff;
            }
            else if (field.count >= kMaxFieldNibbles)
            {
                field.state = FieldState::Idle;
                end         = FieldEnd::CutOff;
            }

            break;
    }

    return end;
}





////////////////////////////////////////////////////////////////////////////////
//
//  StepDataMarkState
//
//  The read side's data field: its start notes the address it follows,
//  its end decodes it or reports it cut off.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2AddressMarkWatcher::StepDataMarkState (uint8_t nibble) noexcept
{
    FieldState  before = m_read.state;
    FieldEnd    end    = StepField (m_read, nibble);



    if (before == FieldState::SawAA && m_read.state == FieldState::Body)
    {
        BeginReadField();
    }

    if (end == FieldEnd::Complete)
    {
        FinishReadField();
    }
    else if (end == FieldEnd::CutOff)
    {
        FireCutOff();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BeginReadField
//
//  A data field takes the address field read just before it. Without one,
//  it is an orphan unless a gap came after the last address field.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2AddressMarkWatcher::BeginReadField() noexcept
{
    m_readTrack        = m_isAddressPending ? m_cachedTrack  : -1;
    m_readSector       = m_isAddressPending ? m_cachedSector : -1;
    m_readVolume       = m_isAddressPending ? m_cachedVolume : -1;
    m_isReadOrphan     = !m_isAddressPending && !m_isGapSinceAddress;
    m_isAddressPending = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FinishReadField
//
//  A complete field's bytes, as a sector read, a data checksum error or an
//  orphan.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2AddressMarkWatcher::FinishReadField() noexcept
{
    std::array<Byte, 256>  bytes  = {};
    bool                   isGood = false;



    if (!m_eventSinks.IsAny() || !TryDecode (m_read, bytes, isGood))
    {
        return;
    }

    m_eventSinks.ForEach ([&] (IDisk2EventSink & sink)
    {
        if (m_isReadOrphan)
        {
            sink.OnDataWithoutAddress (bytes);
        }
        else if (!isGood)
        {
            sink.OnDataChecksumError (m_readTrack, m_readSector, m_readVolume, bytes);
        }
        else
        {
            sink.OnDataMarkRead (m_readTrack, m_readSector, m_readVolume, bytes);
        }
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  FireCutOff
//
////////////////////////////////////////////////////////////////////////////////

void Disk2AddressMarkWatcher::FireCutOff() noexcept
{
    if (m_eventSinks.IsAny())
    {
        m_eventSinks.ForEach ([&] (IDisk2EventSink & sink) { sink.OnDataFieldCutOff (m_readTrack, m_readSector, m_readVolume); });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryDecode
//
//  The field's length picks the encoding; the last body nibble is the
//  checksum. False for a field of neither length.
//
////////////////////////////////////////////////////////////////////////////////

bool Disk2AddressMarkWatcher::TryDecode (const DataField & field, std::array<Byte, 256> & outBytes, bool & outIsGood)
{
    DataFieldDecode  decode;
    DiskFieldKind    kind   = DiskFieldKind::Sixteen;



    if (field.count != kBodyNibbles62 && field.count != kBodyNibbles53)
    {
        return false;
    }

    kind = (field.count == kBodyNibbles62) ? DiskFieldKind::Sixteen : DiskFieldKind::Thirteen;

    DiskFieldFormat::DecodeData (kind, std::span<const Byte> (field.nibbles.data(), field.count - 1), field.nibbles[field.count - 1], decode);

    outBytes  = decode.bytes;
    outIsGood = decode.isChecksumGood;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  StepWriteState
//
//  The write-side data field, run on the nibbles the program writes. A
//  complete field fires OnDataMarkWrite with its bytes and the address
//  field read last, which is the one RWTS found before it switched to
//  writing. Written address fields (a format) fire nothing.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2AddressMarkWatcher::StepWriteState (uint8_t nibble) noexcept
{
    std::array<Byte, 256>  bytes  = {};
    bool                   isGood = false;



    if (StepField (m_write, nibble) != FieldEnd::Complete || !m_eventSinks.IsAny() || !TryDecode (m_write, bytes, isGood))
    {
        return;
    }

    m_eventSinks.ForEach ([&] (IDisk2EventSink & sink) { sink.OnDataMarkWrite (m_cachedTrack, m_cachedSector, m_cachedVolume, bytes); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  SaveField
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Disk2AddressMarkWatcher::SaveField (StateWriter & writer, const DataField & field)
{
    writer.WriteByte   ((Byte) field.state);
    writer.WriteUInt32 (field.count);
    writer.WriteBytes  (field.nibbles.data(), field.count);

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadField
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Disk2AddressMarkWatcher::LoadField (StateReader & reader, DataField & field)
{
    HRESULT   hr    = S_OK;
    Byte      state = 0;
    uint32_t  count = 0;



    reader.ReadByte   (state);
    reader.ReadUInt32 (count);

    CBREx (state <= (Byte) FieldState::Body && count <= kMaxFieldNibbles, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    reader.ReadBytes (field.nibbles.data(), count);

    field.state = (FieldState) state;
    field.count = count;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SaveState
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Disk2AddressMarkWatcher::SaveState (StateWriter & writer) const
{
    HRESULT  hr = S_OK;



    writer.BeginSection (kStateTag, kStateVersion);

    writer.WriteByte   ((Byte) m_addrState);
    writer.WriteByte   (m_volHi);
    writer.WriteByte   (m_volLo);
    writer.WriteByte   (m_trkHi);
    writer.WriteByte   (m_trkLo);
    writer.WriteByte   (m_secHi);
    writer.WriteByte   (m_secLo);
    writer.WriteByte   (m_chkHi);

    writer.WriteUInt32 ((uint32_t) m_cachedSector);
    writer.WriteUInt32 ((uint32_t) m_cachedTrack);
    writer.WriteUInt32 ((uint32_t) m_cachedVolume);
    writer.WriteBool   (m_isAddressPending);
    writer.WriteBool   (m_isGapSinceAddress);

    hr = SaveField (writer, m_read);
    CHR (hr);

    writer.WriteUInt32 ((uint32_t) m_readTrack);
    writer.WriteUInt32 ((uint32_t) m_readSector);
    writer.WriteUInt32 ((uint32_t) m_readVolume);
    writer.WriteBool   (m_isReadOrphan);

    hr = SaveField (writer, m_write);
    CHR (hr);

    writer.WriteUInt64 (m_lastCycle);
    writer.WriteUInt32 (m_stallNibbles);
    writer.WriteBool   (m_isStallFired);
    writer.WriteUInt32 ((uint32_t) m_quarterTrack);

    hr = writer.EndSection();
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadState
//
//  Reads into a watcher of its own and checks each machine's state against
//  its range before committing anything. A version 1 state, from before the
//  fields' nibbles were saved, loads its address-field machine and caches,
//  and both data fields start at Idle.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Disk2AddressMarkWatcher::LoadState (StateReader & reader)
{
    HRESULT                  hr           = S_OK;
    uint16_t                 version      = 0;
    Disk2AddressMarkWatcher  loaded;
    Byte                     addrState    = 0;
    uint32_t                 cachedSector = 0;
    uint32_t                 cachedTrack  = 0;
    uint32_t                 cachedVolume = 0;
    uint32_t                 readTrack    = 0;
    uint32_t                 readSector   = 0;
    uint32_t                 readVolume   = 0;
    uint32_t                 quarterTrack = 0;
    Byte                     skippedByte  = 0;
    uint32_t                 skippedWord  = 0;



    hr = reader.BeginSection (kStateTag, kStateVersion, version);
    CHR (hr);

    reader.ReadByte (addrState);
    reader.ReadByte (loaded.m_volHi);
    reader.ReadByte (loaded.m_volLo);
    reader.ReadByte (loaded.m_trkHi);
    reader.ReadByte (loaded.m_trkLo);
    reader.ReadByte (loaded.m_secHi);
    reader.ReadByte (loaded.m_secLo);
    reader.ReadByte (loaded.m_chkHi);

    if (version >= 2)
    {
        reader.ReadUInt32 (cachedSector);
        reader.ReadUInt32 (cachedTrack);
        reader.ReadUInt32 (cachedVolume);
        reader.ReadBool   (loaded.m_isAddressPending);
        reader.ReadBool   (loaded.m_isGapSinceAddress);

        hr = LoadField (reader, loaded.m_read);
        CHR (hr);

        reader.ReadUInt32 (readTrack);
        reader.ReadUInt32 (readSector);
        reader.ReadUInt32 (readVolume);
        reader.ReadBool   (loaded.m_isReadOrphan);

        hr = LoadField (reader, loaded.m_write);
        CHR (hr);

        reader.ReadUInt64 (loaded.m_lastCycle);
    }
    else
    {
        // Version 1: the data machine and its peek window, the caches, then
        // the write machine and its window.
        reader.ReadByte   (skippedByte);
        reader.ReadUInt32 (skippedWord);
        reader.ReadByte   (skippedByte);
        reader.ReadByte   (skippedByte);
        reader.ReadByte   (skippedByte);
        reader.ReadUInt32 (cachedSector);
        reader.ReadUInt32 (cachedTrack);
        reader.ReadUInt32 (cachedVolume);
        reader.ReadByte   (skippedByte);
        reader.ReadUInt32 (skippedWord);
        reader.ReadByte   (skippedByte);
        reader.ReadByte   (skippedByte);
        reader.ReadByte   (skippedByte);
    }

    reader.ReadUInt32 (loaded.m_stallNibbles);
    reader.ReadBool   (loaded.m_isStallFired);
    reader.ReadUInt32 (quarterTrack);

    hr = reader.EndSection();
    CHR (hr);

    CBREx (addrState              <= (Byte) AddrState::ChkLo, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
    CBREx (loaded.m_stallNibbles  <= kStallNibbles,           HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    loaded.m_addrState    = (AddrState) addrState;
    loaded.m_cachedSector = (int) cachedSector;
    loaded.m_cachedTrack  = (int) cachedTrack;
    loaded.m_cachedVolume = (int) cachedVolume;
    loaded.m_readTrack    = (int) readTrack;
    loaded.m_readSector   = (int) readSector;
    loaded.m_readVolume   = (int) readVolume;
    loaded.m_quarterTrack = (int) quarterTrack;
    loaded.m_eventSinks   = m_eventSinks;

    *this = loaded;

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
