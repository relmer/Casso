#pragma once

#include "Debugger/Reverse/HistoryThumbnails.h"
#include "Debugger/Reverse/ReplayControl.h"
#include "Debugger/Reverse/ReverseCommand.h"
#include "Debugger/Reverse/ReverseOutcome.h"
#include "Shell/ScratchCallReplayer.h"
#include "Shell/ScratchHeatReplayer.h"
#include "Shell/ScratchMachineRenderer.h"

class CpuManager;
class DebugEventLogs;
class DebugViewModel;
class IDebuggerHost;
class IReverseStopTest;
class IShellDebugContext;
class MachineHost;
class ReverseHost;
struct DebuggerSettings;
struct HistoryStatus;





////////////////////////////////////////////////////////////////////////////////
//
//  ShellHistory
//
//  Reverse execution in the emulator: the machine's history, the reverse
//  commands run on it, the timeline's seeks, the caption's replay note and
//  the timeline's pictures. Each replay that rebuilds something from history
//  runs on a separate scratch machine: the timeline's pictures, the heat map's rebuilds and its look-ups of an
//  address's last access, and the call record's rebuilds.
//
//  The reverse commands run on the CPU thread; PostCommand and the timeline's
//  seeks are the UI thread's way in.
//
////////////////////////////////////////////////////////////////////////////////

class ShellHistory
{
public:
    ShellHistory (IDebuggerHost      & host,
                  MachineHost        & machine,
                  CpuManager         & cpuManager,
                  DebuggerSettings   & settings,
                  DebugEventLogs     & eventLogs,
                  IShellDebugContext & context,
                  DebugViewModel     & viewModel);
    ~ShellHistory();

    ShellHistory             (const ShellHistory &) = delete;
    ShellHistory & operator= (const ShellHistory &) = delete;

    //  Recording, from the settings read when the CPU thread starts, or as
    //  Tools > Options changes them.
    void            LoadSettings       ();
    void            StartRecording     ();
    void            StopRecording      ();
    void            ApplyOptions       (bool isRecording, int budgetMb);
    void            RecordFrame        (bool isUserMaximumSpeed);

    bool                  HasReverseHost   () const { return m_reverseHost != nullptr; }
    bool                  IsRecording      () const;
    bool                  IsBehindLive     () const;
    const ReverseHost   * GetReverseHost   () const { return m_reverseHost.get(); }
    void                  SyncInputGate    ();
    HRESULT               Diverge          ();

    //  A reverse command: posted from the UI thread, run on the CPU thread,
    //  or requested by the session, which returns false while recording is
    //  off. A SEEK or LIVE typed at the console has its landing
    //  reported there.
    void            PostCommand        (ReverseCommand command, uint64_t argument = 0);
    void            RunCommand         (ReverseCommand command, uint64_t argument);
    bool            TryRequest         (ReverseCommand command, uint64_t argument);
    void            SetStopTest        (IReverseStopTest * stopTest) { m_reverseStopTest = stopTest; }

    //  The running command's progress, and the UI thread's request to stop
    //  it.
    bool            IsReplaying        () const { return m_isReplayingHistory.load (std::memory_order_acquire); }
    ReplayProgress  GetReplayProgress  ();
    void            StopReplay         ()       { m_replayControl.isStopRequested.store (true, std::memory_order_release); }

    //  The machine's position in history, with the last command's outcome
    //  while the machine has not moved since it landed.
    HistoryStatus   GetStatus          ();

    //  The caption's replay note, updated with the machine on the CPU thread
    //  and read on any thread.
    void            UpdateCaption      ();
    std::wstring    GetCaption         ();

    //  The timeline: a seek to a cycle, whether the last one is pending, and
    //  its pictures, drawn each frame.
    void                  SeekCycle         (uint64_t cycle, bool isInterim);
    bool                  IsSeekBusy        () const;
    void                  ServiceThumbnails ();
    HistoryThumbnails   & GetThumbnails     () { return m_historyThumbnails; }

    //  The debugger's heat map and call record, synced with history while
    //  the debugger is attached; and history's cuts reported to the console.
    void                  SyncHeat          (bool isAttached);
    void                  ServiceCalls      ();
    void                  ReportCuts        ();
    ScratchCallReplayer & GetCallReplayer   () { return m_callReplayer; }

private:
    bool            TryPublishPlayhead ();
    std::string     GetLandingText     (ReverseOutcome outcome);

    IDebuggerHost                 & m_host;
    MachineHost                   & m_machine;
    CpuManager                    & m_cpuManager;
    DebuggerSettings              & m_settings;
    DebugEventLogs                & m_eventLogs;
    IShellDebugContext            & m_context;
    DebugViewModel                & m_viewModel;

    //  Null until the CPU thread first starts recording. The stop test is the
    //  debugger's, for reverse continue, or null.
    std::unique_ptr<ReverseHost>    m_reverseHost;
    IReverseStopTest              * m_reverseStopTest       = nullptr;
    bool                            m_isReverseOn           = false;   // the settings, read when the CPU thread starts and changed by Tools > Options
    int                             m_reverseBudgetMb       = 0;
    int                             m_reverseIntervalFrames = 0;
    std::optional<ReverseOutcome>   m_lastReverseOutcome;              // the last reverse command's, for the history band
    uint64_t                        m_lastReversePosition   = 0;       // where it landed
    size_t                          m_reportedCuts          = 0;       // history's cuts already reported to the console
    std::mutex                      m_replayCaptionMutex;
    std::wstring                    m_replayCaption;                   // " [Replaying @ ...]" behind live, else empty

    //  The renderer is declared first, so it outlives the pictures drawing
    //  through it. The heat and call replayers are declared before the
    //  debugger, whose heat map and call history have references to them.
    ScratchMachineRenderer          m_historyRenderer;
    HistoryThumbnails               m_historyThumbnails     { m_historyRenderer };
    ScratchHeatReplayer             m_heatReplayer;
    ScratchHeatReplayer             m_heatFinder;
    ScratchCallReplayer             m_callReplayer;

    std::atomic<bool>               m_isReplayingHistory    { false };          // set by the CPU thread while a reverse command runs
    std::atomic<bool>               m_isLandingReplyDue     { false };          // a SEEK or LIVE typed at the console: its landing is reported there
    std::atomic<uint64_t>           m_replayStartedAt       { 0 };              // the tick count the running reverse command began at
    ReplayControl                   m_replayControl;                            // the running command's progress, and the UI thread's request to stop it
    std::atomic<uint64_t>           m_seekCyclePosted       { UINT64_MAX };     // the cycle of the timeline's last seek
    std::atomic<uint64_t>           m_seekCyclePostedAt     { 0 };              // the tick count of that seek
    std::atomic<uint64_t>           m_seekCycleLanded       { UINT64_MAX };     // the target of the last seek to a cycle the CPU thread ran
};
