#include "Pch.h"

#include "Debugger/DiskFieldTracker.h"
#include "Devices/Disk/DiskFieldFormat.h"





static constexpr Byte  kFieldMark      = 0xD5;     // in no translate table: a new field begins
static constexpr int   kChecksumNibble = 6;        // the address field's first checksum nibble





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFieldTracker::Reset
//
////////////////////////////////////////////////////////////////////////////////

void DiskFieldTracker::Reset()
{
    m_drives = {};
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFieldTracker::Prime
//
////////////////////////////////////////////////////////////////////////////////

void DiskFieldTracker::Prime (const Disk2NibbleRecord & record)
{
    uint64_t  index = 0;



    Reset();

    for (index = record.GetCount() - record.GetSize(); index < record.GetCount(); index++)
    {
        Step (record.GetEntry (index), nullptr);
    }

    m_fedTo = record.GetCount();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFieldTracker::Feed
//
//  Entries overwritten in the ring since the last call, or a record that
//  started again, break whatever field was under way, so the tracker starts
//  again from the oldest entry there is.
//
////////////////////////////////////////////////////////////////////////////////

void DiskFieldTracker::Feed (const Disk2NibbleRecord & record, std::vector<DiskFieldEvent> & outEvents)
{
    uint64_t  oldest = record.GetCount() - record.GetSize();
    bool      isLost = record.GetCount() < m_fedTo || m_fedTo < oldest;
    uint64_t  index  = isLost ? oldest : m_fedTo;



    if (isLost)
    {
        Reset();
    }

    for (; index < record.GetCount(); index++)
    {
        Step (record.GetEntry (index), &outEvents);
    }

    m_fedTo = record.GetCount();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFieldTracker::Step
//
//  One entry: a written nibble looks for a data prologue after an address
//  field; a read one moves the field under way along, or, with none under
//  way, is tested with the two before it for a prologue.
//
////////////////////////////////////////////////////////////////////////////////

void DiskFieldTracker::Step (const Disk2NibbleRecord::Entry & entry, std::vector<DiskFieldEvent> * outEvents)
{
    int           drive     = ((entry.flags & Disk2NibbleRecord::kDrive2) != 0) ? 1 : 0;
    bool          isWritten = (entry.flags & Disk2NibbleRecord::kWritten) != 0;
    DriveState  & state     = m_drives[drive];



    if ((entry.flags & Disk2NibbleRecord::kNewDisk) != 0)
    {
        state = DriveState();
    }

    if (isWritten)
    {
        StepWritten (state, drive, entry, outEvents);
    }
    else
    {
        state.window[0]    = state.window[1];
        state.window[1]    = state.window[2];
        state.window[2]    = { entry.nibble, entry.address, entry.flags };
        state.inWindow     = std::min (state.inWindow + 1, kWindow);
        state.sinceAddress = std::min (state.sinceAddress + 1, kMaxAddressToData + 1);

        switch (state.phase)
        {
        case Phase::Address:  StepAddress (state, drive, entry, outEvents); break;
        case Phase::Data:     StepData    (state, entry, outEvents);        break;
        case Phase::Idle:                                                   break;
        }
    }

    if (!isWritten && state.phase == Phase::Idle)
    {
        FindPrologue (state);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFieldTracker::FindPrologue
//
//  The last nibbles read, with none missed between them: an address
//  prologue starts an address field, and a data prologue a data field when
//  an address field was read just before it. A custom mark may be shorter
//  than three nibbles, so a prologue is matched against the end of the
//  window.
//
////////////////////////////////////////////////////////////////////////////////

void DiskFieldTracker::FindPrologue (DriveState & state)
{
    std::array<Byte, kWindow>  nibbles       = { state.window[0].nibble, state.window[1].nibble, state.window[2].nibble };
    DiskFieldKind              format        = DiskFieldKind::Sixteen;
    bool                       isNear        = state.lastAddress.has_value() && state.sinceAddress <= kMaxAddressToData;
    int                        addressLength = MatchAddress (nibbles, state.inWindow, format);
    int                        dataLength    = (addressLength == 0 && isNear) ? MatchData (nibbles, state.inWindow) : 0;
    int                        length        = std::max (addressLength, dataLength);
    int                        first         = kWindow - length;
    bool                       isWhole       = length > 0 && !IsBroken (state, first);



    if (isWhole && addressLength > 0)
    {
        state.phase     = Phase::Address;
        state.format    = format;
        state.collected = 0;
    }

    if (isWhole && dataLength > 0)
    {
        state.phase     = Phase::Data;
        state.format    = state.lastAddress->format;
        state.collected = 0;
        state.running   = 0;
    }

    state.prologueAt = isWhole ? state.window[first].address : state.prologueAt;
    state.isTainted  = isWhole ? IsSpinUp (state, first) : state.isTainted;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFieldTracker::IsBroken
//
//  Whether a nibble was missed between the window's entries from `first` on;
//  IsSpinUp, whether any of them passed during the motor's spin-up.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskFieldTracker::IsBroken (const DriveState & state, int first)
{
    bool  isBroken = false;



    for (int i = first + 1; i < kWindow; i++)
    {
        isBroken = isBroken || (state.window[i].flags & Disk2NibbleRecord::kAfterGap) != 0;
    }

    return isBroken;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFieldTracker::IsSpinUp
//
////////////////////////////////////////////////////////////////////////////////

bool DiskFieldTracker::IsSpinUp (const DriveState & state, int first)
{
    bool  isSpinUp = false;



    for (int i = first; i < kWindow; i++)
    {
        isSpinUp = isSpinUp || (state.window[i].flags & Disk2NibbleRecord::kSpinUp) != 0;
    }

    return isSpinUp;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFieldTracker::StepAddress
//
//  Eight 4-and-4 nibbles. A nibble missed before the checksum ends the field
//  unreported; one missed in the checksum pair ends it with the checksum
//  unread.
//
////////////////////////////////////////////////////////////////////////////////

void DiskFieldTracker::StepAddress (DriveState & state, int drive, const Disk2NibbleRecord::Entry & entry, std::vector<DiskFieldEvent> * outEvents)
{
    bool  isGap    = (entry.flags & Disk2NibbleRecord::kAfterGap) != 0;
    bool  isBroken = isGap && state.collected < kChecksumNibble;
    bool  isUnread = isGap && !isBroken;



    state.isTainted = state.isTainted || (entry.flags & Disk2NibbleRecord::kSpinUp) != 0;

    if (isBroken)
    {
        state.phase = Phase::Idle;
    }

    if (!isGap)
    {
        state.addressBytes[state.collected++] = entry.nibble;
    }

    if (isUnread || state.collected == kAddressNibbles)
    {
        FinishAddress (state, drive, entry.address, !isUnread, outEvents);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFieldTracker::FinishAddress
//
////////////////////////////////////////////////////////////////////////////////

void DiskFieldTracker::FinishAddress (DriveState & state, int drive, Word lastInstruction, bool isChecksumRead, std::vector<DiskFieldEvent> * outEvents)
{
    const std::array<Byte, kAddressNibbles>  & bytes = state.addressBytes;
    DiskFieldEvent                             event;



    event.kind               = DiskFieldEventKind::AddressField;
    event.format             = state.format;
    event.drive              = drive;
    event.volume             = DiskFieldFormat::Decode44 (bytes[0], bytes[1]);
    event.track              = DiskFieldFormat::Decode44 (bytes[2], bytes[3]);
    event.sector             = DiskFieldFormat::Decode44 (bytes[4], bytes[5]);
    event.expectedChecksum   = DiskFieldFormat::ComputeAddressChecksum (event.volume, event.track, event.sector);
    event.storedChecksum     = isChecksumRead ? DiskFieldFormat::Decode44 (bytes[6], bytes[7]) : event.expectedChecksum;
    event.isChecksumRead     = isChecksumRead;
    event.addressInstruction = state.prologueAt;
    event.lastInstruction    = lastInstruction;

    state.phase        = Phase::Idle;
    state.sinceAddress = 0;
    state.lastAddress  = state.isTainted ? std::nullopt : std::optional<DiskFieldEvent> (event);

    if (outEvents != nullptr && !state.isTainted)
    {
        outEvents->push_back (event);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFieldTracker::StepData
//
//  The body's nibbles with the running checksum, then the checksum nibble.
//  A nibble missed, or a D5, which begins the next field, ends the field
//  unreported.
//
////////////////////////////////////////////////////////////////////////////////

void DiskFieldTracker::StepData (DriveState & state, const Disk2NibbleRecord::Entry & entry, std::vector<DiskFieldEvent> * outEvents)
{
    bool            isGap   = (entry.flags & Disk2NibbleRecord::kAfterGap) != 0;
    bool            isMark  = entry.nibble == kFieldMark;
    int             length  = DiskFieldFormat::GetBodyLength (state.format);
    Byte            value   = DiskFieldFormat::TranslateNibble (state.format, entry.nibble);
    Byte            body    = (value == DiskFieldFormat::kNotInTable) ? 0 : value;
    DiskFieldEvent  event;



    state.isTainted = state.isTainted || (entry.flags & Disk2NibbleRecord::kSpinUp) != 0;

    if (isGap || isMark)
    {
        state.phase = Phase::Idle;
    }
    else if (state.collected < length)
    {
        state.running = static_cast<Byte> (state.running ^ body);
        state.collected++;
    }
    else
    {
        event                      = *state.lastAddress;
        event.kind                 = DiskFieldEventKind::DataChecksum;
        event.storedDataChecksum   = value;
        event.expectedDataChecksum = state.running;
        event.isDataChecksumGood   = value == state.running;
        event.dataInstruction      = state.prologueAt;
        event.lastInstruction      = entry.address;
        event.isPassed             = event.addressInstruction == event.lastInstruction;

        state.phase = Phase::Idle;
        state.lastAddress.reset();

        if (outEvents != nullptr && !state.isTainted)
        {
            outEvents->push_back (event);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFieldTracker::StepWritten
//
//  A data prologue written after an address field was read is the start of
//  a sector write; the event has the address field's numbers, and the
//  instruction that wrote the prologue's last nibble.
//
////////////////////////////////////////////////////////////////////////////////

void DiskFieldTracker::StepWritten (DriveState & state, int drive, const Disk2NibbleRecord::Entry & entry, std::vector<DiskFieldEvent> * outEvents)
{
    bool            isPrologue = false;
    DiskFieldEvent  event;



    state.written[0] = state.written[1];
    state.written[1] = state.written[2];
    state.written[2] = entry.nibble;
    state.inWritten  = std::min (state.inWritten + 1, kWindow);

    isPrologue = state.lastAddress.has_value() && MatchData (state.written, state.inWritten) > 0;

    if (isPrologue)
    {
        event                 = *state.lastAddress;
        event.kind            = DiskFieldEventKind::WritePrologue;
        event.drive           = drive;
        event.dataInstruction = entry.address;
        event.lastInstruction = entry.address;

        state.lastAddress.reset();
    }

    if (isPrologue && outEvents != nullptr)
    {
        outEvents->push_back (event);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFieldTracker::MatchAddress
//
//  The length of the address prologue the window ends with, or 0. With
//  custom address marks the field has the tracker's format; with the
//  standard ones the third nibble sets it, 96 for 16 sectors and B5 for 13.
//
////////////////////////////////////////////////////////////////////////////////

int DiskFieldTracker::MatchAddress (const std::array<Byte, kWindow> & nibbles, int inWindow, DiskFieldKind & outFormat) const
{
    DiskMarkPattern  sixteen    = DiskFieldFormat::GetAddressPrologue (DiskFieldKind::Sixteen);
    DiskMarkPattern  thirteen   = DiskFieldFormat::GetAddressPrologue (DiskFieldKind::Thirteen);
    bool             isCustom   = m_marks.address.has_value() && IsAtEnd (*m_marks.address, nibbles, inWindow);
    bool             isSixteen  = !m_marks.address.has_value() && IsAtEnd (sixteen,  nibbles, inWindow);
    bool             isThirteen = !m_marks.address.has_value() && IsAtEnd (thirteen, nibbles, inWindow);



    outFormat = isCustom ? m_marks.format : (isThirteen ? DiskFieldKind::Thirteen : DiskFieldKind::Sixteen);

    return isCustom ? m_marks.address->GetLength() : (isSixteen || isThirteen) ? kWindow : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFieldTracker::MatchData
//
////////////////////////////////////////////////////////////////////////////////

int DiskFieldTracker::MatchData (const std::array<Byte, kWindow> & nibbles, int inWindow) const
{
    DiskMarkPattern  pattern = m_marks.data.value_or (DiskFieldFormat::GetDataPrologue());



    return IsAtEnd (pattern, nibbles, inWindow) ? pattern.GetLength() : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFieldTracker::IsAtEnd
//
//  Whether the window's last nibbles, as many as the pattern has, match it.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskFieldTracker::IsAtEnd (const DiskMarkPattern & pattern, const std::array<Byte, kWindow> & nibbles, int inWindow)
{
    int  length = pattern.GetLength();



    return length > 0 && length <= inWindow && pattern.Matches (std::span<const Byte> (nibbles).last (length));
}