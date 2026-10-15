#include "Pch.h"

#include "Debugger/BreakpointTable.h"
#include "Debugger/DiskBreakpointMonitor.h"
#include "Debugger/DiskLogicalMap.h"





static constexpr int  kQuartersPerTrack = 4;
static constexpr int  kHalfTrackOffset  = 2;
static constexpr int  kEpilogueNibbles  = 3;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskBreakpointMonitor::Refresh
//
//  One tracker per mark set the enabled entries use; the listener attached
//  while any of them is enabled. A change of either primes the trackers.
//
////////////////////////////////////////////////////////////////////////////////

void DiskBreakpointMonitor::Refresh()
{
    std::vector<DiskFieldTracker::Marks>  sets;
    std::vector<DiskFieldTracker::Marks>  current;
    bool                                  isArmed = false;
    DebugDiskState                        state;



    for (const Breakpoint & entry : m_table.GetAll())
    {
        DiskFieldTracker::Marks  marks   = entry.disk.has_value() ? GetMarks (*entry.disk) : DiskFieldTracker::Marks();
        bool                     isEvent = entry.enabled && entry.kind == BreakpointKind::Disk && !entry.IsDiskCall();



        isArmed = isArmed || isEvent;

        if (isEvent && std::find (sets.begin(), sets.end(), marks) == sets.end())
        {
            sets.push_back (marks);
        }
    }

    for (const DiskFieldTracker & tracker : m_trackers)
    {
        current.push_back (tracker.GetMarks());
    }

    if (sets != current)
    {
        m_trackers.clear();

        for (const DiskFieldTracker::Marks & marks : sets)
        {
            m_trackers.emplace_back (marks);
        }
    }

    if (!m_isReplay)
    {
        m_target.SetDiskEventSink (isArmed ? this : nullptr);
    }

    m_selectedDrive = m_target.TryGetDiskState (state) ? state.selectedDrive : 0;

    if (isArmed && (!m_isArmed || sets != current))
    {
        Prime();
    }

    m_isArmed = isArmed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskBreakpointMonitor::Prime
//
////////////////////////////////////////////////////////////////////////////////

void DiskBreakpointMonitor::Prime()
{
    const Disk2NibbleRecord  * record = m_target.GetDiskRecord();



    m_events.clear();
    m_isBlockReported = false;
    m_recordSeen      = (record != nullptr) ? record->GetCount() : 0;
    m_lastCycle       = m_target.GetCycleCount();

    for (DiskFieldTracker & tracker : m_trackers)
    {
        if (record != nullptr)
        {
            tracker.Prime (*record);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskBreakpointMonitor::Poll
//
//  The events the last instruction made, the blocked writes and the fields
//  the record completed, each tested against every enabled disk entry.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskBreakpointMonitor::Poll (const IDebugExpressionContext & context)
{
    const Disk2NibbleRecord      * record = m_target.GetDiskRecord();
    std::vector<Candidate>         candidates;
    std::vector<DiskFieldEvent>    fields;
    DebugDiskState                 state;
    int                            hitId  = -1;
    std::optional<int32_t>         value;
    uint64_t                       cycle  = m_target.GetCycleCount();
    bool                           isJump = cycle < m_lastCycle || cycle - m_lastCycle > kMaxStepCycles;



    if (!m_isArmed)
    {
        return m_report.has_value();
    }

    if (isJump)
    {
        Prime();
    }

    m_lastCycle = cycle;
    m_target.TryGetDiskState (state);

    if (record != nullptr)
    {
        FindBlockedWrites (*record);
    }

    for (Event & event : m_events)
    {
        event.quarterTrack = (event.kind == DiskEvent::WriteMode) ? state.quarterTrack[event.drive] : event.quarterTrack;
        candidates.push_back ({ event, std::nullopt, 0 });
    }

    m_events.clear();

    for (size_t set = 0; record != nullptr && set < m_trackers.size(); set++)
    {
        fields.clear();
        m_trackers[set].Feed (*record, fields);

        for (DiskFieldEvent & field : fields)
        {
            Candidate  candidate { std::nullopt, field, set };



            candidate.field->isWrongTrack = field.kind == DiskFieldEventKind::AddressField && field.track != state.quarterTrack[field.drive] / kQuartersPerTrack;
            NoteVisit (candidate);
            candidates.push_back (candidate);
        }
    }

    for (const Candidate & candidate : candidates)
    {
        auto  isMatch = [&] (const Breakpoint & entry) { return IsMatch (entry, candidate); };
        bool  isStop  = m_isReplay ? m_table.IsDiskStop (isMatch, context) : m_table.TryMatchDisk (isMatch, context, hitId, value);



        if (isStop && !m_report.has_value() && m_isReplay)
        {
            m_report = DiskReport();
        }

        if (isStop && !m_report.has_value())
        {
            MakeReport (hitId, candidate, state);
        }
    }

    return m_report.has_value();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskBreakpointMonitor::FindBlockedWrites
//
//  The first latch load write protection dropped in each stretch of write
//  mode.
//
////////////////////////////////////////////////////////////////////////////////

void DiskBreakpointMonitor::FindBlockedWrites (const Disk2NibbleRecord & record)
{
    constexpr Byte  kBlocked = Disk2NibbleRecord::kWritten | Disk2NibbleRecord::kDropped;
    uint64_t        index    = std::max (m_recordSeen, record.GetCount() - record.GetSize());



    for (; index < record.GetCount(); index++)
    {
        const Disk2NibbleRecord::Entry  & entry     = record.GetEntry (index);
        bool                              isBlocked = (entry.flags & kBlocked) == kBlocked && !m_isBlockReported;
        Event                             event;



        if (isBlocked)
        {
            event.kind        = DiskEvent::WriteBlocked;
            event.drive       = ((entry.flags & Disk2NibbleRecord::kDrive2) != 0) ? 1 : 0;
            event.instruction = entry.address;
            m_events.push_back (event);
            m_isBlockReported = true;
        }
    }

    m_recordSeen = record.GetCount();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskBreakpointMonitor::IsMatch
//
//  An RWTS or driver entry stops before its routine, never on an event.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskBreakpointMonitor::IsMatch (const Breakpoint & entry, const Candidate & candidate) const
{
    bool  isEventEntry = entry.disk.has_value() && !entry.IsDiskCall();



    return isEventEntry && (candidate.field.has_value() ? IsFieldMatch (*entry.disk, candidate)
                                                        : IsEventMatch (*entry.disk, *candidate.event));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskBreakpointMonitor::IsFieldMatch
//
//  A field matches only the entries whose marks its tracker follows. A
//  sector read counts a data field the search passed over only with PASSED
//  (the read test, FR-145).
//
////////////////////////////////////////////////////////////////////////////////

bool DiskBreakpointMonitor::IsFieldMatch (const DiskBreakSpec & spec, const Candidate & candidate) const
{
    const DiskFieldEvent  & field    = *candidate.field;
    int                     track    = 0;
    int                     first    = -1;
    int                     second   = -1;
    bool                    isData   = field.kind == DiskFieldEventKind::DataChecksum && (spec.isPassed || !field.isPassed);
    bool                    isSet    = FindMarkSet (spec) == candidate.markSet && IsDrive (spec, field.drive);
    bool                    isPlace  = (!spec.track.has_value() || *spec.track == field.track) && (!spec.sector.has_value() || *spec.sector == field.sector);
    bool                    isVolume = !spec.volume.has_value() || *spec.volume == field.volume;
    bool                    isBadSum = field.storedChecksum != field.expectedChecksum;



    switch (spec.event)
    {
    case DiskEvent::SectorRead:
        return isSet && isPlace && field.kind == DiskFieldEventKind::DataChecksum && (spec.isPassed || !field.isPassed);

    case DiskEvent::AddressField:
        return isSet && isPlace && isVolume && field.kind == DiskFieldEventKind::AddressField &&
               (!spec.isBadChecksum || isBadSum) && (!spec.isWrongTrack || field.isWrongTrack);

    case DiskEvent::WritePrologue:
        return isSet && isPlace && field.kind == DiskFieldEventKind::WritePrologue;

    case DiskEvent::DosSector:
        return isSet && isData && (!spec.track.has_value() || *spec.track == field.track) &&
               spec.sector.has_value() && field.sector == DiskLogicalMap::GetDosPhysical (*spec.sector);

    //  A block stops once both its sectors are read during one visit to its
    //  track, on the second of them.
    case DiskEvent::ProDosBlock:
        DiskLogicalMap::GetBlockPhysical (spec.block.value_or (0), track, first, second);

        return isSet && isData && field.track == track && (field.sector == first || field.sector == second) &&
               ((spec.isPassed ? candidate.anyMask : candidate.readMask) & ((1 << first) | (1 << second))) == ((1 << first) | (1 << second));

    default:
        return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskBreakpointMonitor::NoteVisit
//
//  A data field on another track than the drive's visit starts a new visit.
//
////////////////////////////////////////////////////////////////////////////////

void DiskBreakpointMonitor::NoteVisit (Candidate & candidate)
{
    const DiskFieldEvent  & field = *candidate.field;
    Visit                 & visit = m_visits[field.drive];
    uint16_t                bit   = static_cast<uint16_t> (1 << (field.sector & 0x0F));



    if (field.kind == DiskFieldEventKind::DataChecksum)
    {
        visit          = (visit.track == field.track) ? visit : Visit { field.track, 0, 0 };
        visit.anyMask  = static_cast<uint16_t> (visit.anyMask | bit);
        visit.readMask = static_cast<uint16_t> (visit.readMask | (field.isPassed ? 0 : bit));
    }

    candidate.readMask = visit.readMask;
    candidate.anyMask  = visit.anyMask;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskBreakpointMonitor::IsEventMatch
//
////////////////////////////////////////////////////////////////////////////////

bool DiskBreakpointMonitor::IsEventMatch (const DiskBreakSpec & spec, const Event & event) const
{
    bool  isDrive = IsDrive (spec, event.drive) && event.kind == spec.event;
    bool  isMotor = event.motor.has_value() &&
                    (spec.motor == DiskMotorFilter::Any                                        ||
                     (spec.motor == DiskMotorFilter::On      && *event.motor == DiskMotorEvent::On)  ||
                     (spec.motor == DiskMotorFilter::Off     && *event.motor == DiskMotorEvent::Off) ||
                     (spec.motor == DiskMotorFilter::Stopped && *event.motor == DiskMotorEvent::Stopped));



    switch (spec.event)
    {
    case DiskEvent::Head:          return isDrive && (!event.isBump || spec.isBump) && IsPosition (spec, event.quarterTrack);
    case DiskEvent::WriteMode:     return isDrive && IsPosition (spec, event.quarterTrack);
    case DiskEvent::WriteBlocked:  return isDrive;
    case DiskEvent::Motor:         return isDrive && isMotor;
    case DiskEvent::DriveSelect:   return isDrive && (spec.isEvery || event.isChange);
    case DiskEvent::Inserted:      return isDrive;
    case DiskEvent::Ejected:       return isDrive;
    default:                       return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskBreakpointMonitor::MakeReport
//
////////////////////////////////////////////////////////////////////////////////

void DiskBreakpointMonitor::MakeReport (int id, const Candidate & candidate, const DebugDiskState & state)
{
    DiskReport         report;
    Breakpoint         entry;
    std::vector<Byte>  epilogue;
    bool               isRandom = false;



    m_table.TryFind (id, entry);

    report.breakpointId = id;
    report.event        = entry.disk.has_value() ? entry.disk->event : DiskEvent::SectorRead;
    report.slot         = state.slot;
    report.drive        = candidate.field.has_value() ? candidate.field->drive : candidate.event->drive;
    report.quarterTrack = state.quarterTrack[report.drive];
    report.isMotorOn    = state.isMotorOn;
    report.isProtected  = state.isProtected[report.drive];
    report.phases       = state.phases;
    report.imageName    = state.imageName[report.drive];

    if (candidate.event.has_value())
    {
        report.quarterTrack         = (candidate.event->kind == DiskEvent::Head) ? candidate.event->quarterTrack : report.quarterTrack;
        report.previousQuarterTrack = candidate.event->previousQt;
        report.isBump               = candidate.event->isBump;
        report.motorEvent           = candidate.event->motor;
        report.instruction          = candidate.event->instruction;
        report.previousDrive        = candidate.event->previousDrive;
    }

    if (candidate.field.has_value())
    {
        report.field       = candidate.field;
        report.instruction = candidate.field->lastInstruction;
    }

    if (report.event == DiskEvent::DosSector && entry.disk->sector.has_value())
    {
        report.logicalSector   = entry.disk->sector;
        report.physicalSectors = { (Byte) DiskLogicalMap::GetDosPhysical (*entry.disk->sector) };
        report.order           = DiskSectorOrder::Dos33;
    }

    if (report.event == DiskEvent::ProDosBlock && entry.disk->block.has_value())
    {
        int  track  = 0;
        int  first  = 0;
        int  second = 0;



        DiskLogicalMap::GetBlockPhysical (*entry.disk->block, track, first, second);
        report.block           = entry.disk->block;
        report.physicalSectors = { (Byte) first, (Byte) second };
        report.order           = DiskSectorOrder::ProDos;
    }

    if (report.event == DiskEvent::SectorRead && m_target.PeekDiskNibbles (report.drive, kEpilogueNibbles, epilogue, isRandom))
    {
        std::copy_n (epilogue.begin(), std::min (epilogue.size(), report.epilogue.size()), report.epilogue.begin());
        report.isEpilogueRandom = isRandom;
    }

    m_report = report;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskBreakpointMonitor::Queue
//
//  Each controller event is queued with the instruction making it, which
//  Poll tests at the next instruction boundary.
//
////////////////////////////////////////////////////////////////////////////////

void DiskBreakpointMonitor::Queue (Event event)
{
    event.instruction = m_target.GetInstructionAddress();
    m_events.push_back (event);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskBreakpointMonitor::OnMotorCommandOn
//
////////////////////////////////////////////////////////////////////////////////

void DiskBreakpointMonitor::OnMotorCommandOn()
{
    Queue ({ DiskEvent::Motor, m_selectedDrive, 0, 0, false, true, 0, DiskMotorEvent::On });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskBreakpointMonitor::OnMotorCommandOff
//
////////////////////////////////////////////////////////////////////////////////

void DiskBreakpointMonitor::OnMotorCommandOff()
{
    Queue ({ DiskEvent::Motor, m_selectedDrive, 0, 0, false, true, 0, DiskMotorEvent::Off });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskBreakpointMonitor::OnMotorDisengaged
//
////////////////////////////////////////////////////////////////////////////////

void DiskBreakpointMonitor::OnMotorDisengaged()
{
    Queue ({ DiskEvent::Motor, m_selectedDrive, 0, 0, false, true, 0, DiskMotorEvent::Stopped });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskBreakpointMonitor::OnHeadStep
//
////////////////////////////////////////////////////////////////////////////////

void DiskBreakpointMonitor::OnHeadStep (int prevQt, int newQt)
{
    Queue ({ DiskEvent::Head, m_selectedDrive, newQt, prevQt });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskBreakpointMonitor::OnHeadBump
//
////////////////////////////////////////////////////////////////////////////////

void DiskBreakpointMonitor::OnHeadBump (int atQt)
{
    Queue ({ DiskEvent::Head, m_selectedDrive, atQt, atQt, true });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskBreakpointMonitor::OnWriteMode
//
////////////////////////////////////////////////////////////////////////////////

void DiskBreakpointMonitor::OnWriteMode (int drive, bool isOn)
{
    m_isBlockReported = false;

    if (isOn)
    {
        Queue ({ DiskEvent::WriteMode, drive });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskBreakpointMonitor::OnDriveSelect
//
////////////////////////////////////////////////////////////////////////////////

void DiskBreakpointMonitor::OnDriveSelect (int drive)
{
    Queue ({ DiskEvent::DriveSelect, drive, 0, 0, false, drive != m_selectedDrive, m_selectedDrive });
    m_selectedDrive = drive;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskBreakpointMonitor::OnDiskInserted
//
//  While the machine is paused a disk change stops nothing: each disk entry
//  on it is reported in a line instead. OnDiskEjected does the same.
//
////////////////////////////////////////////////////////////////////////////////

void DiskBreakpointMonitor::OnDiskInserted (int drive)
{
    for (const Breakpoint & entry : m_table.GetAll())
    {
        bool  isNotice = m_isPaused && m_notice && entry.enabled && entry.disk.has_value() && IsEventMatch (*entry.disk, { DiskEvent::Inserted, drive });



        if (isNotice)
        {
            m_notice (std::format ("Disk breakpoint #{}: disk inserted in drive {} while the machine was paused", entry.id, drive + 1));
        }
    }

    if (!m_isPaused)
    {
        Queue ({ DiskEvent::Inserted, drive });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskBreakpointMonitor::OnDiskEjected
//
////////////////////////////////////////////////////////////////////////////////

void DiskBreakpointMonitor::OnDiskEjected (int drive)
{
    for (const Breakpoint & entry : m_table.GetAll())
    {
        bool  isNotice = m_isPaused && m_notice && entry.enabled && entry.disk.has_value() && IsEventMatch (*entry.disk, { DiskEvent::Ejected, drive });



        if (isNotice)
        {
            m_notice (std::format ("Disk breakpoint #{}: disk ejected from drive {} while the machine was paused", entry.id, drive + 1));
        }
    }

    if (!m_isPaused)
    {
        Queue ({ DiskEvent::Ejected, drive });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskBreakpointMonitor::GetMarks
//
//  The tracker an entry's fields come from: its own marks, or the standard
//  ones. With the standard address marks each field's prologue sets its
//  length, so the format is part of the set only with custom ones.
//
////////////////////////////////////////////////////////////////////////////////

DiskFieldTracker::Marks DiskBreakpointMonitor::GetMarks (const DiskBreakSpec & spec)
{
    DiskFieldTracker::Marks  marks;



    marks.address = spec.addressMarks;
    marks.data    = spec.dataMarks;
    marks.format  = spec.addressMarks.has_value() ? spec.format : DiskFieldKind::Sixteen;
    return marks;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskBreakpointMonitor::FindMarkSet
//
////////////////////////////////////////////////////////////////////////////////

size_t DiskBreakpointMonitor::FindMarkSet (const DiskBreakSpec & spec) const
{
    DiskFieldTracker::Marks  marks = GetMarks (spec);
    size_t                   found = SIZE_MAX;



    for (size_t set = 0; set < m_trackers.size() && found == SIZE_MAX; set++)
    {
        found = (m_trackers[set].GetMarks() == marks) ? set : found;
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskBreakpointMonitor::IsDrive
//
////////////////////////////////////////////////////////////////////////////////

bool DiskBreakpointMonitor::IsDrive (const DiskBreakSpec & spec, int drive)
{
    return spec.drive == DiskDriveFilter::Either || (spec.drive == DiskDriveFilter::Drive1 && drive == 0) || (spec.drive == DiskDriveFilter::Drive2 && drive == 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskBreakpointMonitor::IsPosition
//
//  A T range matches only whole-track positions in it; HALF matches the
//  half tracks.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskBreakpointMonitor::IsPosition (const DiskBreakSpec & spec, int quarterTrack)
{
    bool  isInRange = quarterTrack >= spec.qtFirst && quarterTrack <= spec.qtLast && (!spec.isWholeTracks || quarterTrack % kQuartersPerTrack == 0);



    switch (spec.head)
    {
    case DiskHeadFilter::Range:  return isInRange;
    case DiskHeadFilter::Half:   return quarterTrack % kQuartersPerTrack == kHalfTrackOffset;
    default:                     return true;
    }
}
