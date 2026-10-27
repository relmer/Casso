#pragma once

#include "Pch.h"

class InputJournal;
class KeyframeStore;
enum class InputKind : uint8_t;





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryMarkKind
//
//  The kinds of mark on the history timeline.
//
////////////////////////////////////////////////////////////////////////////////

enum class HistoryMarkKind
{
    Gap,
    Reset,
    PowerCycle,
    DiskChange,
    WriteProtect,
    StateLoad,
    Fork,
};





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryMark
//
//  One mark: its kind, the instruction position and the history cycle it is
//  drawn at, and for a fork, how many branches begin there.
//
////////////////////////////////////////////////////////////////////////////////

struct HistoryMark
{
    HistoryMarkKind  kind     = HistoryMarkKind::Reset;
    uint64_t         position = 0;
    uint64_t         cycle    = 0;
    int              count    = 1;
};





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryMarks
//
//  The marks on the active path of history, built from the input journal's
//  records and the keyframes: a reset, a power cycle, a disk mounted,
//  ejected or swapped, a write-protect change and a state file loaded, each
//  at its position; and a gap where recording paused, at the keyframe that
//  follows it.
//
//  Update reads only the records added since the last call, so a journal
//  of many thousands of keystrokes and paddle moves is read once. A cut at
//  the journal's end, which a fork makes, starts the scan over.
//
//  A record is stamped with the CPU's cycle count, which a power cycle sets
//  back, while the timeline is laid out along history's cycles, which never
//  go back. So a record's cycle on the timeline is estimated from its
//  position, by the rate of cycles to positions between the keyframes on
//  either side of it, exact at a keyframe and within one stretch elsewhere.
//
////////////////////////////////////////////////////////////////////////////////

class HistoryMarks
{
public:
    static bool      TryGetKind    (InputKind kind, HistoryMarkKind & outKind);
    static uint64_t  EstimateCycle (const KeyframeStore & keyframes, uint64_t position, uint64_t endPosition, uint64_t endCycle);

    //  Reads the records added since the last call and returns true with
    //  every mark in position order when any mark or keyframe has come or
    //  gone; false, with outMarks untouched, when none has.
    bool             Update        (const KeyframeStore & keyframes, const InputJournal & journal, uint64_t endPosition, uint64_t endCycle, std::vector<HistoryMark> & outMarks);
    void             Clear         ();

private:
    struct RecordMark
    {
        HistoryMarkKind  kind     = HistoryMarkKind::Reset;
        uint64_t         position = 0;
        size_t           index    = 0;
    };

    bool  ScanJournal      (const InputJournal & journal);
    bool  DropOutside      (const KeyframeStore & keyframes, const InputJournal & journal);
    bool  HasKeyframeMoved (const KeyframeStore & keyframes);

    std::deque<RecordMark>  m_records;
    size_t                  m_scanned       = 0;
    uint64_t                m_cutCount      = 0;
    bool                    m_hasScanned    = false;
    size_t                  m_keyframeCount = 0;
    uint64_t                m_oldest        = 0;
    uint64_t                m_newest        = 0;
};
