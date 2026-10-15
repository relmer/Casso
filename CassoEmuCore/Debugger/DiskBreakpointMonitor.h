#pragma once

#include "Pch.h"

#include "Debugger/DiskFieldTracker.h"
#include "Debugger/IDebugTarget.h"
#include "Machines/Apple2/Common/IDisk2EventSink.h"

class BreakpointTable;
class IDebugExpressionContext;
struct Breakpoint;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskBreakpointMonitor
//
//  The debugger's listener on the Disk II controller while a disk breakpoint
//  other than a call is enabled. The controller's events, which come during
//  an instruction, are queued; at the next instruction boundary Poll takes
//  them and the fields the recent nibbles completed, matches each against
//  the table's disk entries and their IF conditions, counts every match,
//  and stores the report of the first that stops until the session takes it.
//
////////////////////////////////////////////////////////////////////////////////

class DiskBreakpointMonitor : public IDisk2EventSink
{
public:
    DiskBreakpointMonitor (BreakpointTable & table, IDebugTarget & target) : m_table (table), m_target (target) {}

    //  Attaches or detaches the listener as the table's disk entries are
    //  enabled or not, follows the mark sets they use, and primes.
    void  Refresh();

    //  Starts the trackers again from the record as it is, reporting nothing
    //  before it: when a breakpoint is armed, and when the machine's cycle
    //  count at a boundary has jumped further than an instruction takes, as a move
    //  through history, a state file load or a keyframe the replayer loads
    //  is.
    void  Prime();

    //  More cycles than any instruction and an interrupt before it take.
    static constexpr uint64_t  kMaxStepCycles = 64;

    //  At an instruction boundary: true when a disk entry stops. Every
    //  matching entry counts its hit.
    bool  Poll (const IDebugExpressionContext & context);

    bool                 HasStop   () const { return m_report.has_value(); }
    const DiskReport &   GetReport () const { return *m_report; }
    void                 ClearStop ()       { m_report.reset(); }

    //  While paused an insert or eject is reported in a line instead of
    //  stopping; the session sets the function and whether it is paused.
    void  SetPaused (bool isPaused) { m_isPaused = isPaused; }
    void  SetNotice (std::function<void (const std::string &)> notice) { m_notice = std::move (notice); }

    bool  IsArmed() const { return m_isArmed; }

    //  IDisk2EventSink
    void  OnMotorCommandOn  () override;
    void  OnMotorEngaged    () override                                      {}
    void  OnMotorCommandOff () override;
    void  OnMotorDisengaged () override;
    void  OnHeadStep        (int prevQt, int newQt) override;
    void  OnHeadBump        (int atQt) override;
    void  OnAddressMark     (int, int, int) override                         {}
    void  OnDataMarkRead    (int, int, int, std::span<const Byte>) override  {}
    void  OnDataMarkWrite   (int, int, int, std::span<const Byte>) override  {}
    void  OnDriveSelect     (int drive) override;
    void  OnDiskInserted    (int drive) override;
    void  OnDiskEjected     (int drive) override;
    void  OnWriteMode       (int drive, bool isOn) override;

private:
    //  An event the controller reported, or a blocked write the record has.
    struct Event
    {
        DiskEvent                      kind          = DiskEvent::Head;
        int                            drive         = 0;
        int                            quarterTrack  = 0;
        int                            previousQt    = 0;
        bool                           isBump        = false;
        bool                           isChange      = true;      // a drive select that changed the drive
        int                            previousDrive = 0;
        std::optional<DiskMotorEvent>  motor;
        Word                           instruction   = 0;
    };

    //  A candidate the entries are tested against: an event, or a field
    //  from the tracker for one mark set.
    struct Candidate
    {
        std::optional<Event>           event;
        std::optional<DiskFieldEvent>  field;
        size_t                         markSet  = 0;
        uint16_t                       readMask = 0;     // the field's track's sectors read so far this visit, it included
        uint16_t                       anyMask  = 0;     // and those passed over as well
    };

    //  A drive's visit to one track: the physical sectors whose data fields
    //  were read in full there, and those read or passed over.
    struct Visit
    {
        int       track    = -1;
        uint16_t  readMask = 0;
        uint16_t  anyMask  = 0;
    };

    void  Queue          (Event event);
    void  FindBlockedWrites (const Disk2NibbleRecord & record);
    bool  IsMatch        (const Breakpoint & entry, const Candidate & candidate) const;
    bool  IsFieldMatch   (const DiskBreakSpec & spec, const Candidate & candidate) const;
    void  NoteVisit      (Candidate & candidate);
    bool  IsEventMatch   (const DiskBreakSpec & spec, const Event & event) const;
    void  MakeReport     (int id, const Candidate & candidate, const DebugDiskState & state);
    size_t  FindMarkSet  (const DiskBreakSpec & spec) const;

    static DiskFieldTracker::Marks  GetMarks     (const DiskBreakSpec & spec);
    static bool                     IsDrive      (const DiskBreakSpec & spec, int drive);
    static bool                     IsPosition   (const DiskBreakSpec & spec, int quarterTrack);

    BreakpointTable                             & m_table;
    IDebugTarget                                & m_target;
    std::vector<DiskFieldTracker>                 m_trackers;
    std::vector<Event>                            m_events;
    std::array<Visit, 2>                          m_visits;
    std::optional<DiskReport>                     m_report;
    std::function<void (const std::string &)>     m_notice;
    uint64_t                                      m_recordSeen        = 0;
    uint64_t                                      m_lastCycle         = 0;
    int                                           m_selectedDrive     = 0;
    bool                                          m_isBlockReported   = false;
    bool                                          m_isArmed           = false;
    bool                                          m_isPaused          = false;
};
