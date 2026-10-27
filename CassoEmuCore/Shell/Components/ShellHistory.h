#pragma once

#include "Pch.h"

#include "Debugger/Reverse/DivergenceGate.h"
#include "Debugger/Reverse/HeldInputWatch.h"
#include "Debugger/Reverse/HistoryThumbnails.h"
#include "Debugger/Reverse/ReplayControl.h"
#include "Debugger/Reverse/ReverseCommand.h"
#include "Debugger/Reverse/ReverseOutcome.h"
#include "Shell/SavedHistory.h"
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

    // Saved history, on the thread that owns the machine. A save's history
    // section is empty with history off, and while a disk that left a bay is
    // retained for history's keyframes; behind live, the machine first goes to
    // the live end, and ReturnFromSave brings it back. A loaded state's
    // section waits for the next StartRecording, which starts from it when
    // this build can load it. A state loaded with no history is marked in
    // the history recording: PrepareStateLoad forks it behind live, and
    // FinishStateLoad marks the load, its history cycles going on from those
    // before it.
    HRESULT  BuildSavedHistory (std::vector<Byte> & outSection, uint64_t & outResumePosition);
    void     ReturnFromSave    (uint64_t resumePosition);
    void     SetPendingHistory (std::vector<Byte> section) { m_pendingHistory = std::move (section); }
    void     PrepareStateLoad  ();
    void     FinishStateLoad   (bool isLoaded);

    // UI thread: the settings as saved, and a change from an options page or
    // the menu, saved and posted to the CPU thread, which applies the
    // recording and budget at once; the interval applies from the next start
    // of history. Turning history off deletes all of the machine's history,
    // in memory and saved, so it is confirmed first; SaveOptions returns false
    // when the user cancels, and history stays on. A test sets the answer.
    using ConfirmFn = std::function<bool()>;

    static constexpr const wchar_t *  kpszTurnOffText   = L"Turning history off deletes all of this machine's history, in memory and saved.";
    static constexpr const wchar_t *  kpszTurnOffTitle  = L"Turn history off";
    static constexpr const wchar_t *  kpszTurnOffButton = L"&Turn off";

    const HistorySettings &  GetSettings       () const { return m_settings; }
    bool                     SaveOptions       (bool isRecording, int budgetMb, int intervalFrames);
    void                     SetConfirmTurnOff (ConfirmFn confirm) { m_confirmTurnOff = std::move (confirm); }

    // UI thread: the History menu's commands, and whether each is enabled:
    // rewind and step back while history is on and the machine is past the
    // oldest point, go live behind live, and history on always.
    void         RunMenuCommand       (WORD id);
    bool         IsMenuCommandEnabled (WORD id) const;
    static bool  IsMenuItemEnabled    (WORD id, bool isOn, bool isBehindLive, bool isAtOldest);

    // CPU thread, ahead of each frame: the network fence, and the machine run
    // from the past made live again.
    void  RecordFrame    ();

    // Moves through history. PostCommand is the UI thread's way in; the
    // command runs on the CPU thread, and its landing is reported as a
    // step's stop.
    void  PostCommand    (ReverseCommand command, uint64_t argument = 0);
    void  RunCommand     (ReverseCommand command, uint64_t argument);

    // A change behind live forks history first, with no question: the
    // command queue's gate, on the posting thread; the fork, on the CPU
    // thread; and a debugger edit's guard, on the CPU thread.
    bool  AllowCommand     (WORD id, const std::string & payload);
    void  Diverge          ();
    bool  GuardEdit        (const DebugCommand & command);
    bool  IsBehindLiveForUi();

    // Input the guest reads, deferred behind live until the replayed guest
    // reads it.
    void  HoldInputBehindLive           (HeldInput input);
    void  RunForHeldInput               ();
    void  PublishHeldInput              ();
    void  StopForHeldInputRead          ();
    void  OnHeldInputRead               ();
    void  ResumeAfterHeldInput          ();
    void  ForkForHeldInput              ();
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

    // The rewind key. StartRewind and StopRewind are the UI thread's, on its
    // press and release; a paused machine is run, so frames go on. Each CPU-
    // thread frame while the key is down steps the machine back one frame in
    // place of running one, at the video rate whatever the speed, and once
    // the key is up or the machine lands on the oldest point it pauses on the
    // frame on screen.
    void  StartRewind    ();
    void  StopRewind     ();
    bool  RunRewindFrame ();

    // UI thread: pauses a running machine and waits a moment for the CPU
    // thread to stop it, as the Pause command does, for a move through
    // history made from the timeline.
    void  PauseMachine   ();

    // UI thread: a switch to a branch of history, and the picture of the
    // branch whose fork row is under the pointer, drawn over the screen.
    void                      SwitchBranch            (uint32_t branchId);
    HistoryThumbnails::Image  GetHoveredBranchPicture ();
    static void               LabelBranch             (const KeyframeBranch & branch, std::wstring & outRanFor, std::wstring & outLeftAt);

    // The caption's replay note and the replay indicator's text: updated on
    // the CPU thread, read on any.
    void          UpdateReplayCaption       ();
    std::wstring  GetReplayCaption          ();
    std::wstring  GetReplayIndicatorText    ();

    // UI thread: whether the rewind key is down.
    bool          IsRewinding               () const { return m_isRewindKeyDown.load (std::memory_order_acquire); }

    // The timelines, the debugger's and the emulator window's rewind bar: a
    // seek to a cycle from the UI thread, whether it has yet to land, and the
    // pictures and marks, serviced on the CPU thread between frames.
    void                 SeekCycle         (uint64_t cycle, bool isInterim);
    bool                 IsSeekBusy        () const;
    void                 ServiceThumbnails ();
    HistoryThumbnails &  GetThumbnails     () { return m_thumbnails; }
    HistoryThumbnails &  GetBarThumbnails  () { return m_barThumbnails; }

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
    void     FenceForNetwork    ();
    bool     TryPublishPlayhead ();
    void     PublishMarks       ();
    bool     ConfirmTurnOff     ();
    void     FinishRewind       ();
    HRESULT  StartFromSaved     ();
    HRESULT  RestoreMediaIds    (const std::vector<SavedBayMedium> & media);

    IDebuggerHost     & m_host;
    MachineHost       & m_machine;
    CpuManager        & m_cpuManager;
    HistorySettings   & m_settings;   // the emulator's preferences have it; saved through the host
    IHistoryClient    * m_client = nullptr;

    // The input deferred behind live and the lines it would change.
    DivergenceGate                m_divergenceGate;
    HeldInputWatch                m_heldInputWatch;
    std::atomic<bool>             m_isResumeOwedAfterHeldRead = false;     // a replay stopped at a deferred read was running
    size_t                        m_pasteLengthBeforeHold     = 0;         // the paste buffer before a paste deferred behind live
    bool                          m_isJoyportSyncOwed         = false;     // a players' change made behind live, for the Joyport once live
    std::mutex                    m_replayCaptionMutex;
    std::wstring                  m_replayCaption;                         // " [Replaying @ ...]" behind live, else empty
    std::wstring                  m_replayIndicatorText;                   // "Replaying" and the host's time behind live, else empty

    // The history of this machine, recording while a machine is built; null
    // until the CPU thread first starts it. CPU thread only.
    std::unique_ptr<ReverseHost>  m_reverseHost;
    bool                          m_isOn              = false;   // the settings, read when the CPU thread starts and changed by the options
    bool                          m_isNetworkFenced   = false;   // the Uthernet II was in use at the last check, so history is off
    int                           m_budgetMb          = 0;
    int                           m_intervalFrames    = 0;
    std::optional<ReverseOutcome> m_lastOutcome;                 // the last reverse command's, for the history band
    uint64_t                      m_lastPosition      = 0;       // where it landed
    std::atomic<bool>             m_isRewindKeyDown   = false;   // the UI thread's: the rewind key is down
    bool                          m_isRewinding       = false;   // a rewind is under way, on the CPU thread
    std::atomic<uint32_t>         m_hoveredBranch     = 0;       // the branch whose fork row is under the pointer
    std::atomic<bool>             m_isAtOldestForUi   = false;   // the machine is at the oldest point, as last published
    ConfirmFn                     m_confirmTurnOff;              // a test's answer to turning history off; null shows the dialog
    std::vector<Byte>             m_pendingHistory;              // a loaded state's history section, for the next StartRecording
    uint64_t                      m_cyclesBeforeLoad  = 0;       // the history cycles before a state with no history was loaded

    // Pictures of history for each timeline, drawn on a scratch machine of
    // the running one's configuration, and the marks drawn on both. Each
    // renderer is declared ahead of its pictures, so it outlives the pictures
    // drawn through it.
    ScratchMachineRenderer        m_renderer;
    HistoryThumbnails             m_thumbnails        { m_renderer };
    ScratchMachineRenderer        m_barRenderer;
    HistoryThumbnails             m_barThumbnails     { m_barRenderer };
    HistoryMarks                  m_marks;

    std::atomic<bool>             m_isReplaying       { false };      // set by the CPU thread while a reverse command runs
    std::atomic<uint64_t>         m_replayStartedAt   { 0 };          // the tick count the running reverse command began at
    ReplayControl                 m_replayControl;                    // the running command's progress, and the UI thread's request to stop it
    std::atomic<uint64_t>         m_seekCyclePosted   { UINT64_MAX }; // the cycle the timeline last asked a seek to
    std::atomic<uint64_t>         m_seekCyclePostedAt { 0 };          // the tick count it asked at
    std::atomic<uint64_t>         m_seekCycleLanded   { UINT64_MAX }; // the cycle the last seek to a cycle the CPU thread ran asked for
};
