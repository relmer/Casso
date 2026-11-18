#pragma once

#include "Pch.h"

#include "Debugger/Reverse/HistoryWallTimes.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryPlayhead
//
//  The machine's position in a history strip, live or replaying history, as
//  the machine thread sets it every turn, and the line the user drags along
//  the strip. A line let go remains where it was dropped, labeled for that
//  point, until the seek there has landed. The labels under the pointer are
//  a cycle and the host's clock there.
//
//  Threads. The machine thread calls SetPosition, SetTime, SetBegin,
//  NoteSeekLanded and the wall times' Sync and Clear; the UI thread calls
//  the rest. The values the machine thread sets are atomics, and only the
//  UI thread reads or writes the dragged line's fields.
//
////////////////////////////////////////////////////////////////////////////////

class HistoryPlayhead
{
public:
    using LabelFn = std::function<void (uint64_t cycle, uint64_t wallTime, std::wstring & outTop, std::wstring & outBottom)>;
    using ScrubFn = std::function<void (uint64_t cycle, bool isFinal)>;

    static constexpr double  kSeekPatienceMs = 1000.0;

    void  SetLabeler (LabelFn labeler) { m_labeler = std::move (labeler); }
    void  SetOnScrub (ScrubFn onScrub) { m_onScrub = std::move (onScrub); }

    //  Machine thread: the machine's position, and whether it is behind live;
    //  its cycle and the host's clock, and the cycle history ends at; the
    //  cycle and the time history begins at; and that a seek to `cycle` the
    //  strip requested has run, called after SetTime sets where it landed.
    void                SetPosition    (uint64_t position, bool isBehindLive);
    void                SetTime        (uint64_t cycle, uint64_t wallTime, uint64_t endCycle);
    void                SetBegin       (uint64_t cycle, uint64_t wallTime);
    void                NoteSeekLanded (uint64_t cycle);
    HistoryWallTimes  & GetWallTimes   () { return m_wallTimes; }

    uint64_t      GetPosition     () const { return m_position.load (std::memory_order_acquire); }
    bool          IsBehindLive    () const { return m_isBehindLive.load (std::memory_order_acquire); }
    uint64_t      GetEndCycle     () const { return m_endCycle.load (std::memory_order_acquire); }
    bool          IsAtDrop        (double nowMs);
    void          GetLineTime     (double nowMs, uint64_t & outCycle, uint64_t & outWallTime);
    void          DropLine        (uint64_t cycle, bool isFinal, double nowMs);
    bool          Label           (uint64_t cycle, uint64_t wallTime, std::wstring & outTop, std::wstring & outBottom) const;
    bool          LabelAt         (uint64_t cycle, std::wstring & outTop, std::wstring & outBottom) const;
    std::wstring  GetLeadingLabel () const;

private:
    LabelFn                m_labeler;
    ScrubFn                m_onScrub;
    HistoryWallTimes       m_wallTimes;            // the host's clock at any cycle, for the labels under the pointer

    std::atomic<uint64_t>  m_position     = 0;
    std::atomic<bool>      m_isBehindLive = false;
    std::atomic<uint64_t>  m_cycle        = 0;
    std::atomic<uint64_t>  m_wallTime     = 0;
    std::atomic<uint64_t>  m_endCycle     = 0;
    std::atomic<uint64_t>  m_beginCycle   = 0;
    std::atomic<uint64_t>  m_beginWall    = 0;
    std::atomic<uint64_t>  m_landedCycle  = 0;     // the cycle the last seek that ran requested
    std::atomic<uint64_t>  m_landings     = 0;     // how many seeks have run

    //  The UI thread's: where the line was dragged, drawn there until the
    //  seek there lands.
    bool                   m_isDropped    = false;
    uint64_t               m_dropCycle    = 0;
    uint64_t               m_dropLandings = 0;
    double                 m_dropAtMs     = 0.0;
};
