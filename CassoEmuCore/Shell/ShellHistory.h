#pragma once

#include "Debugger/Reverse/HistoryMarks.h"
#include "Debugger/Reverse/HistoryThumbnails.h"
#include "Debugger/Reverse/ReplayControl.h"
#include "Debugger/Reverse/ReverseCommand.h"
#include "Debugger/Reverse/ReverseOutcome.h"
#include "Shell/SavedHistory.h"
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
struct HistorySettings;
struct HistoryStatus;
struct KeyframeBranch;
struct ReverseResult;





////////////////////////////////////////////////////////////////////////////////
//
//  ShellHistory
//
//  The machine's history, with or without the debugger: recording, the
//  moves through history and their landings, the caption's replay note, the
//  rewind key, the switches between branches, saved history, and the
//  pictures of both timelines, the debugger's and the emulator window's
//  rewind bar. Each replay that rebuilds something from history runs on a
//  separate scratch machine: the timelines' pictures, the heat map's
//  rebuilds and its look-ups of an address's last access, and the call
//  record's rebuilds.
//
//  Recording and moves run on the CPU thread; the comments mark the
//  UI-thread entry points.
//
////////////////////////////////////////////////////////////////////////////////

class ShellHistory
{
public:
    ShellHistory (IDebuggerHost      & host,
                  MachineHost        & machine,
                  CpuManager         & cpuManager,
                  HistorySettings    & settings,
                  DebugEventLogs     & eventLogs,
                  IShellDebugContext & context,
                  DebugViewModel     & viewModel);
    ~ShellHistory();

    ShellHistory             (const ShellHistory &) = delete;
    ShellHistory & operator= (const ShellHistory &) = delete;

    //  Recording. The settings are read once, when the CPU thread starts;
    //  recording starts once a machine is built and power cycled and stops
    //  before a machine is torn down and at exit; the options sheet's changes
    //  apply at once. All on the CPU thread.
    void  LoadSettings   ();
    void  StartRecording ();
    void  StopRecording  ();
    void  ApplyOptions   (bool isRecording, int budgetMb);

    //  Saved history, on the thread that owns the machine. A save's history
    //  section is empty with history off, and while a disk that left a bay is
    //  retained for history's keyframes; behind live, the machine first goes to
    //  the live end, and ReturnFromSave brings it back. A loaded state's
    //  section waits for the next StartRecording, which starts from it when
    //  this build can load it. A state loaded with no history is marked in
    //  the history recording: PrepareStateLoad forks it behind live, and
    //  FinishStateLoad marks the load, its history cycles going on from those
    //  before it.
    HRESULT  BuildSavedHistory (std::vector<Byte> & outSection, uint64_t & outResumePosition);
    void     ReturnFromSave    (uint64_t resumePosition);
    void     SetPendingHistory (std::vector<Byte> section) { m_pendingHistory = std::move (section); }
    void     PrepareStateLoad  ();
    void     FinishStateLoad   (bool isLoaded);

    //  UI thread: the settings as saved, and a change from an options page or
    //  the menu, saved and posted to the CPU thread, which applies the
    //  recording and budget at once. Turning history off deletes all of the
    //  machine's history, in memory and saved, so it is confirmed first;
    //  SaveOptions returns false when the user cancels, and history stays on.
    //  A test sets the answer.
    using ConfirmFn = std::function<bool()>;

    static constexpr const wchar_t *  kpszTurnOffText   = L"Turning history off deletes all of this machine's history, in memory and saved.";
    static constexpr const wchar_t *  kpszTurnOffTitle  = L"Turn history off";
    static constexpr const wchar_t *  kpszTurnOffButton = L"&Turn off";

    const HistorySettings &  GetSettings       () const { return m_settings; }
    bool                     SaveOptions       (bool isRecording, int budgetMb);
    void                     SetConfirmTurnOff (ConfirmFn confirm) { m_confirmTurnOff = std::move (confirm); }

    //  UI thread: the History menu's commands, and whether each is enabled:
    //  rewind and step back while history is on and the machine is past the
    //  oldest point, go live behind live, and history on always.
    void         RunMenuCommand       (WORD id);
    bool         IsMenuCommandEnabled (WORD id) const;
    static bool  IsMenuItemEnabled    (WORD id, bool isOn, bool isBehindLive, bool isAtOldest);

    //  CPU thread, ahead of each frame: the machine run from the past made
    //  live again.
    void  RecordFrame ();

    bool                  HasReverseHost () const { return m_reverseHost != nullptr; }
    bool                  IsRecording    () const;
    bool                  IsBehindLive   () const;
    ReverseHost         * GetReverseHost ()       { return m_reverseHost.get(); }
    const ReverseHost   * GetReverseHost () const { return m_reverseHost.get(); }
    void                  SyncInputGate  ();
    HRESULT               Diverge        ();

    //  Moves through history. PostCommand is the UI thread's way in; the
    //  command runs on the CPU thread, and its landing is reported as a
    //  step's stop. The session's request returns false while recording is
    //  off, and a SEEK or LIVE typed at the console has its landing reported
    //  there.
    void  PostCommand (ReverseCommand command, uint64_t argument = 0);
    void  RunCommand  (ReverseCommand command, uint64_t argument);
    bool  TryRequest  (ReverseCommand command, uint64_t argument);
    void  SetStopTest (IReverseStopTest * stopTest) { m_reverseStopTest = stopTest; }

    //  The rewind key. StartRewind and StopRewind are the UI thread's, on its
    //  press and release; a paused machine is run, so frames go on. Each CPU-
    //  thread frame while the key is down steps the machine back one frame in
    //  place of running one, at the video rate whatever the speed, and once
    //  the key is up or the machine lands on the oldest point it pauses on the
    //  frame on screen. IsRewinding is the UI thread's.
    void  StartRewind    ();
    void  StopRewind     ();
    bool  RunRewindFrame ();
    bool  IsRewinding    () const { return m_isRewindKeyDown.load (std::memory_order_acquire); }

    //  UI thread: pauses a running machine and waits a moment for the CPU
    //  thread to stop it, as the Pause command does, for a move through
    //  history made from the timeline.
    void  PauseMachine ();

    //  UI thread: a switch to a branch of history, and the picture of the
    //  branch whose fork row is under the pointer, drawn over the screen.
    void                      SwitchBranch            (uint32_t branchId);
    HistoryThumbnails::Image  GetHoveredBranchPicture ();
    static void               LabelBranch             (const KeyframeBranch & branch, std::wstring & outRanFor, std::wstring & outLeftAt);

    //  The running command's progress, and the UI thread's request to stop
    //  it.
    bool            IsReplaying       () const { return m_isReplaying.load (std::memory_order_acquire); }
    ReplayProgress  GetReplayProgress ();
    void            StopReplay        ()       { m_replayControl.isStopRequested.store (true, std::memory_order_release); }

    //  CPU thread: the machine's position in history, with the last reverse
    //  command's outcome while the machine has not moved since it landed.
    HistoryStatus   GetStatus ();

    //  The caption's replay note and the replay indicator's text: updated on
    //  the CPU thread, read on any.
    void          UpdateCaption          ();
    std::wstring  GetCaption             ();
    std::wstring  GetReplayIndicatorText ();

    //  The timelines: a seek to a cycle from the UI thread, whether it has
    //  yet to land, and the pictures and marks, serviced on the CPU thread
    //  between frames.
    void                 SeekCycle         (uint64_t cycle, bool isInterim);
    bool                 IsSeekBusy        () const;
    void                 ServiceThumbnails ();
    HistoryThumbnails  & GetThumbnails     () { return m_thumbnails; }
    HistoryThumbnails  & GetBarThumbnails  () { return m_barThumbnails; }

    //  The debugger's heat map and call record, synced with history while
    //  the debugger is attached; and history's cuts reported to the console.
    void                  SyncHeat        (bool isAttached);
    void                  ServiceCalls    ();
    void                  ReportCuts      ();
    ScratchCallReplayer & GetCallReplayer () { return m_callReplayer; }

private:
    bool         TryPublishPlayhead ();
    void         PublishMarks       ();
    bool         ConfirmTurnOff     ();
    void         FinishRewind       ();
    void         NoteMoving         ();
    void         NoteMoved          (ReverseCommand command);
    void         NoteLanded         (const ReverseResult & result);
    HRESULT      StartFromSaved     ();
    HRESULT      RestoreMediaIds    (const std::vector<SavedBayMedium> & media);
    std::string  GetLandingText     (ReverseOutcome outcome);

    IDebuggerHost                 & m_host;
    MachineHost                   & m_machine;
    CpuManager                    & m_cpuManager;
    HistorySettings               & m_settings;   // the emulator's preferences have it; saved through the host
    DebugEventLogs                & m_eventLogs;
    IShellDebugContext            & m_context;
    DebugViewModel                & m_viewModel;

    //  Null until the CPU thread first starts recording. The stop test is the
    //  debugger's, for reverse continue, or null. CPU thread only.
    std::unique_ptr<ReverseHost>    m_reverseHost;
    IReverseStopTest              * m_reverseStopTest   = nullptr;
    bool                            m_isOn              = false;   // the settings, read when the CPU thread starts and changed by the options
    int                             m_budgetMb          = 0;
    std::optional<ReverseOutcome>   m_lastOutcome;                 // the last reverse command's, for the history band
    uint64_t                        m_lastPosition      = 0;       // where it landed
    size_t                          m_reportedCuts      = 0;       // history's cuts already reported to the console
    std::atomic<bool>               m_isRewindKeyDown   = false;   // the UI thread's: the rewind key is down
    bool                            m_isRewinding       = false;   // a rewind is under way, on the CPU thread
    std::atomic<uint32_t>           m_hoveredBranch     = 0;       // the branch whose fork row is under the pointer
    std::atomic<bool>               m_isAtOldestForUi   = false;   // the machine is at the oldest point, as last published
    ConfirmFn                       m_confirmTurnOff;              // a test's answer to turning history off; null shows the dialog
    std::vector<Byte>               m_pendingHistory;              // a loaded state's history section, for the next StartRecording
    uint64_t                        m_cyclesBeforeLoad  = 0;       // the history cycles before a state with no history was loaded
    std::mutex                      m_replayCaptionMutex;
    std::wstring                    m_replayCaption;               // " [Replaying @ ...]" behind live, else empty
    std::wstring                    m_replayIndicatorText;         // "Replaying" and the host's time behind live, else empty

    //  Pictures of history for each timeline, drawn on a scratch machine of
    //  the running one's configuration, and the marks drawn on both. Each
    //  renderer is declared ahead of its pictures, so it outlives the
    //  pictures drawn through it. The heat and call replayers are declared
    //  before the debugger, whose heat map and call history have references
    //  to them.
    ScratchMachineRenderer          m_renderer;
    HistoryThumbnails               m_thumbnails        { m_renderer };
    ScratchMachineRenderer          m_barRenderer;
    HistoryThumbnails               m_barThumbnails     { m_barRenderer };
    HistoryMarks                    m_marks;
    ScratchHeatReplayer             m_heatReplayer;
    ScratchHeatReplayer             m_heatFinder;
    ScratchCallReplayer             m_callReplayer;

    std::atomic<bool>               m_isReplaying       { false };          // set by the CPU thread while a reverse command runs
    std::atomic<bool>               m_isLandingReplyDue { false };          // a SEEK or LIVE typed at the console: its landing is reported there
    std::atomic<uint64_t>           m_replayStartedAt   { 0 };              // the tick count the running reverse command began at
    ReplayControl                   m_replayControl;                        // the running command's progress, and the UI thread's request to stop it
    std::atomic<uint64_t>           m_seekCyclePosted   { UINT64_MAX };     // the cycle of the timeline's last seek
    std::atomic<uint64_t>           m_seekCyclePostedAt { 0 };              // the tick count of that seek
    std::atomic<uint64_t>           m_seekCycleLanded   { UINT64_MAX };     // the target of the last seek to a cycle the CPU thread ran
};
