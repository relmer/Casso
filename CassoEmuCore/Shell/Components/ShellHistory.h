#pragma once

#include "Pch.h"

#include "Debugger/Reverse/DeferredHistoryEdits.h"
#include "Debugger/Reverse/DivergenceGate.h"
#include "Debugger/Reverse/HeldInputWatch.h"
#include "Debugger/Reverse/HistoryThumbnails.h"
#include "Debugger/Reverse/ReplayControl.h"
#include "Debugger/Reverse/ReverseCommand.h"
#include "Debugger/Reverse/ReverseOutcome.h"
#include "Shell/ScratchMachineRenderer.h"

class Apple2eKeyboard;
class CpuManager;
class IDebuggerHost;
class IHistoryClient;
class MachineHost;
class ReverseHost;
struct DebugCommand;
struct HistorySettings;
struct HistoryStatus;





////////////////////////////////////////////////////////////////////////////////
//
//  ShellHistory
//
//  The machine's history as the emulator has it, with or without the
//  debugger: recording, the moves through history and their landings, the
//  input deferred behind live and the cut a change makes there, the replay
//  note in the caption, and the timeline's pictures. The emulator's shell
//  constructs one; the debugger follows it through IHistoryClient. Recording
//  and moves run on the CPU thread; the comments mark the UI-thread entry
//  points.
//
////////////////////////////////////////////////////////////////////////////////

class ShellHistory
{
public:
    // A guest mouse press that was deferred behind live and let go of before
    // the machine was live is pressed for this long once live, so the guest
    // reads a click rather than nothing. The timer is on the emulator's window.
    static constexpr UINT_PTR  kClickReleaseTimerId = 0xCA56;
    static constexpr UINT      kClickHoldMs         = 100;

    ShellHistory (IDebuggerHost & host, MachineHost & machine, CpuManager & cpuManager, HistorySettings & settings);
    ~ShellHistory();

    ShellHistory             (const ShellHistory &) = delete;
    ShellHistory & operator= (const ShellHistory &) = delete;

    // The debugger that follows history, or null. CPU thread.
    void  SetClient (IHistoryClient * client) { m_client = client; }

    // Recording. The settings are read once, when the CPU thread starts;
    // recording starts once a machine is built and power cycled and stops
    // before a machine is torn down and at exit; the options sheet's changes
    // apply at once. All on the CPU thread.
    void  LoadSettings   ();
    void  StartRecording ();
    void  StopRecording  ();
    void  ApplyOptions   (bool isRecording, int budgetMb);

    // UI thread: the settings as saved, and a change from an options page,
    // saved and posted to the CPU thread, which applies the recording and
    // budget at once; the interval applies from the next start of history.
    const HistorySettings &  GetSettings () const { return m_settings; }
    void                     SaveOptions (bool isRecording, int budgetMb, int intervalFrames);

    // CPU thread, ahead of each frame: the network fence, and the machine run
    // from the past made live again.
    void  RecordFrame    ();

    // Moves through history. PostCommand is the UI thread's way in; the
    // command runs on the CPU thread, and its landing is reported as a
    // step's stop.
    void  PostCommand    (ReverseCommand command, uint64_t argument = 0);
    void  RunCommand     (ReverseCommand command, uint64_t argument);

    // A change behind live: the command queue's gate, on the posting thread;
    // the question, on the UI thread; the cut, on the CPU thread; and a
    // debugger edit deferred until the answer.
    bool  AllowCommand     (WORD id, const std::string & payload);
    bool  AskToDiverge     ();
    void  Diverge          ();
    bool  GuardEdit        (const DebugCommand & command);
    void  OnConfirmDiverge ();
    bool  IsBehindLiveForUi();

    // Input the guest reads, deferred behind live until the replayed guest
    // reads it.
    void  HoldInputBehindLive           (HeldInput input);
    void  PublishHeldInput              ();
    void  StopForHeldInputRead          ();
    void  OnHeldInputRead               ();
    void  ResumeAfterHeldInput          ();
    void  OnConfirmInputDiverge         ();
    void  ApplyHeldInputs               (const std::vector<HeldInput> & inputs);
    void  ApplyHeldMouseTarget          (uint32_t target);
    void  PressGuestMouseHeldBehindLive ();
    void  ReleaseGuestMouseAfterClick   ();
    void  ToggleHeldEightyColumnSwitch  ();
    void  ToggleEightyColumnSwitch      (Apple2eKeyboard * iieKbd);

    DivergenceGate &  GetDivergenceGate        ()               { return m_divergenceGate; }
    HeldInputWatch &  GetHeldInputWatch        ()               { return m_heldInputWatch; }
    void              OweJoyportSync           ()               { m_isJoyportSyncOwed = true; }
    bool              TakeJoyportSyncOwed      ();
    size_t            GetPasteLengthBeforeHold () const         { return m_pasteLengthBeforeHold; }
    void              SetPasteLengthBeforeHold (size_t length)  { m_pasteLengthBeforeHold = length; }

    // The caption's replay note: updated on the CPU thread, read on any.
    void          UpdateReplayCaption ();
    std::wstring  GetReplayCaption    ();

    // The timeline: a seek to a cycle from the UI thread, whether it has yet
    // to land, and the pictures, serviced on the CPU thread between frames.
    void                 SeekCycle         (uint64_t cycle, bool isInterim);
    bool                 IsSeekBusy        () const;
    void                 ServiceThumbnails ();
    HistoryThumbnails &  GetThumbnails     () { return m_thumbnails; }

    // A reverse command under way, for the debugger's status bar. Any thread.
    bool            IsReplaying       () const { return m_isReplaying.load (std::memory_order_acquire); }
    ReplayProgress  GetReplayProgress ();
    void            StopReplay        ()       { m_replayControl.isStopRequested.store (true, std::memory_order_release); }

    // CPU thread: the machine's position in history, with the last reverse
    // command's outcome while the machine has not moved since it landed.
    HistoryStatus   GetStatus ();

    bool  IsRecording  () const;
    bool  IsBehindLive () const;

    ReverseHost        *  GetReverseHost ()       { return m_reverseHost.get(); }
    const ReverseHost  *  GetReverseHost () const { return m_reverseHost.get(); }

private:
    void  FenceForNetwork ();
    bool  TryPublishPlayhead ();

    IDebuggerHost     & m_host;
    MachineHost       & m_machine;
    CpuManager        & m_cpuManager;
    HistorySettings   & m_settings;   // the emulator's preferences have it; saved through the host
    IHistoryClient    * m_client = nullptr;

    // The input deferred behind live and the lines it would change, and the
    // debugger edits deferred until the user answers.
    DivergenceGate                m_divergenceGate;
    HeldInputWatch                m_heldInputWatch;
    std::atomic<bool>             m_isResumeOwedAfterHeldRead = false;     // a replay stopped at a deferred read was running
    size_t                        m_pasteLengthBeforeHold     = 0;         // the paste buffer before a paste deferred behind live
    bool                          m_isJoyportSyncOwed         = false;     // a players' change made behind live, for the Joyport once live
    DeferredHistoryEdits          m_deferredEdits;
    std::mutex                    m_replayCaptionMutex;
    std::wstring                  m_replayCaption;                         // " [Replaying @ ...]" behind live, else empty

    // The history of this machine, recording while a machine is built; null
    // until the CPU thread first starts it. CPU thread only.
    std::unique_ptr<ReverseHost>  m_reverseHost;
    bool                          m_isOn              = false;   // the settings, read when the CPU thread starts and changed by the options
    bool                          m_isNetworkFenced   = false;   // the Uthernet II was in use at the last check, so history is off
    int                           m_budgetMb          = 0;
    int                           m_intervalFrames    = 0;
    std::optional<ReverseOutcome> m_lastOutcome;                 // the last reverse command's, for the history band
    uint64_t                      m_lastPosition      = 0;       // where it landed

    // Pictures of history for the timeline, drawn on a scratch machine of
    // the running one's configuration. The renderer is declared first, so it
    // outlives the pictures drawn through it.
    ScratchMachineRenderer        m_renderer;
    HistoryThumbnails             m_thumbnails        { m_renderer };

    std::atomic<bool>             m_isReplaying       { false };      // set by the CPU thread while a reverse command runs
    std::atomic<uint64_t>         m_replayStartedAt   { 0 };          // the tick count the running reverse command began at
    ReplayControl                 m_replayControl;                    // the running command's progress, and the UI thread's request to stop it
    std::atomic<uint64_t>         m_seekCyclePosted   { UINT64_MAX }; // the cycle the timeline last asked a seek to
    std::atomic<uint64_t>         m_seekCyclePostedAt { 0 };          // the tick count it asked at
    std::atomic<uint64_t>         m_seekCycleLanded   { UINT64_MAX }; // the cycle the last seek to a cycle the CPU thread ran asked for
};
