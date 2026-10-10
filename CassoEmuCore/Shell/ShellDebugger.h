#pragma once

#include "Pch.h"

#include "Shell/CpuCommandDispatcher.h"
#include "Ui/Debugger/DebuggerWindow.h"

class EmulatorShell;





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebugger
//
//  The debugger the emulator holds: its window and session, reverse
//  execution and the replays of history, the input held behind live, and
//  the debug channel. The emulator's shell owns one and reaches it through
//  a narrow interface; per-instruction work stays in the machine and the
//  session's hooks.
//
////////////////////////////////////////////////////////////////////////////////

class ShellDebugger : public IDebugCommandTarget,
                      private IDebuggerWindowHost
{
public:
    // Stage 1 of the extraction only: the shell reaches the debugger's state
    // directly until the narrow interface replaces it.
    friend class EmulatorShell;

    explicit ShellDebugger (EmulatorShell & shell);
    ~ShellDebugger() override;

    // Attaches the driver the slice loop reports to, or detaches it with null.
    // The caller owns it and must detach before destroying it.
    void SetDebugRunDriver (CpuManagerRunDriver * driver) { m_debugRunDriver = driver; }

    // Attaches the session machine events and the main window's step go to,
    // or detaches it with null. The caller owns it, as with the driver.
    void SetDebugSession   (DebugSession * session)        { m_debugSession = session; }

    // True when a message's top-level window is the debugger window or one
    // of its floating panes, whose keys the emulator's accelerators leave alone.
    static bool  IsDebuggerMessageRoot (HWND root, HWND rootOwner, HWND debugger);

    // IDebuggerWindowHost: the operating system's pickers, the shell's.
    IHostDialogs &  GetHostDialogs () noexcept override;

    // A debugger command line from a client, run on the CPU thread, marshaled
    // via IDM_DEBUG_COMMAND, in `mode` when given. Handed to whoever attached a
    // command handler; a machine with no debugger attached drops it.
    void RunDebugCommand (uint32_t clientId, const std::string & line, std::optional<CommandMode> mode) override;

    // The user paused or resumed the machine, told to an attached session.
    // CPU thread only, marshaled via IDM_DEBUG_PAUSE_CHANGED.
    void NotifyDebugPauseChanged (bool paused) override;

    // Tells an attached session the machine was reset or replaced. CPU thread
    // only; each is a no-op when no session is attached.
    void NotifyDebugReset          (bool isPowerCycle);
    void NotifyDebugMachineChanged (const std::string & machineName);

    // Reverse execution. Recording starts once a machine is built and power
    // cycled, from the settings read at start, and stops before a machine is
    // torn down and at exit; all three run on the CPU thread, as does
    // ApplyReverseOptions, which takes Tools > Options' settings at once.
    // PostReverseCommand is the UI thread's way in: the command runs on the
    // CPU thread, and its outcome is announced as a step's stop, with the
    // StopEvent's history set. ReverseContinue stops where the stop test
    // given to SetReverseStopTest fires; without one it fails and the machine
    // stays where it is.
    void  StartReverseRecording ();
    void  StopReverseRecording  ();
    void  PostReverseCommand    (ReverseCommand command, uint64_t argument = 0);
    void  RunReverseCommand     (ReverseCommand command, uint64_t argument) override;
    void  ApplyReverseOptions   (bool isRecording, int budgetMb) override;
    void  SetReverseStopTest    (IReverseStopTest * stopTest) { m_reverseStopTest = stopTest; }

    // CPU thread: where the machine stands in history, with the last reverse
    // command's outcome while the machine has not moved since it landed.
    HistoryStatus  GetHistoryStatus  ();

    const ReverseHost *  GetReverseHost() const { return m_reverseHost.get(); }

    // Behind live, a change to the machine first asks whether to discard the
    // history recorded after where it stands. The command gate asks on the
    // UI thread before a state-changing command is queued; a debugger edit is
    // held back on the CPU thread, asked about on the UI thread, and run again
    // on a yes. A yes queues IDM_DEBUG_DIVERGE ahead of the change. Input the
    // guest reads asks only once the guest reads it; see below.
    bool  AllowCommand          (WORD id, const std::string & payload);
    bool  AskToDiverge          ();
    void  DivergeHistory        () override;
    bool  GuardHistoryEdit      (const std::string & line, CommandMode mode);
    void  OnConfirmDiverge      ();
    bool  IsBehindLiveForUi     ();

    // Behind live, input the guest reads -- keys, a paste, the game port, the
    // guest mouse -- is held without a question, and the lines it would
    // change are published to the watch the CPU thread attaches to the
    // devices. The first read the held input would change stops the replay,
    // puts the machine back before that read, and asks on the UI thread; a
    // yes makes the input live there, a no drops it, and either way a replay
    // that was running runs on. The //c's 80/40 switch asks at once.
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

    // CPU thread: the caption's replay note, kept in step with the machine.
    void  UpdateReplayCaption   ();

    // Where debugger commands go. The session machine events go to is set
    // with SetDebugSession.
    using DebugCommandHandler = std::function<void (uint32_t clientId, const std::string & line, std::optional<CommandMode> mode)>;

    void SetDebugCommandHandler (DebugCommandHandler handler)      { m_debugCommandHandler = std::move (handler); }


    // Shows the debugger window, creating it the first time, and opens the
    // debug channel. UI thread. activate=false leaves the foreground and the
    // keys where they are, for a window the launch opens.
    void    OpenDebuggerWindow (bool activate = true);

    // True when a message went to the debugger window or a floating pane.
    bool    IsDebuggerMessage  (const MSG & msg) const;

    // The window's requests, carried out on the CPU thread (ICpuCommandTarget).
    void    OpenDebugChannel   () override;
    void    CloseDebugChannel  (bool isDetach) override;
    void    PauseDebugRun      () override;
    void    SetDebugView       (const std::string & view, std::optional<Word> address) override;
    void    SetDebugTraceView  (std::optional<uint64_t> first) override;
    void    GoToDebugMemory    (int window, const std::string & text) override;
    void    ScrollDebugCode    (int lines, int view) override;
    void    RunDebugActions    () override;

    // Whether the heat map pane is shown, and so recording.
    void    SetDebugHeatMapShown   (bool shown) override;
    void    SetDebugHeatMapOptions (const std::string & text) override;
    void    ResetDebugHeatMap      () override;
    void    SetDebugHeatMapHover   (std::optional<Word> address) override;
    void    RunDebugHeatMapAccess  (const HeatAccessRequest & request) override;
    void    SetDebugHeatMapIgnore  (const std::vector<std::pair<Word, Word>> & spans) override;

    // Draws a stopped machine's picture again when the beam mark changed.
    void    RedrawDebugFrame     () override;

    // Gathers what the window's next snapshot reads when it is showing and
    // due, and hands it to be built off the CPU thread. CPU thread.
    void    PublishDebuggerView  ();
    void    GatherDebugView      (DebugViewInput & input, DebuggerViewSnapshot & live);

    // A built snapshot, for the UI thread to take. Any thread.
    void    PublishDebugSnapshot (std::shared_ptr<const DebuggerViewSnapshot> snapshot);

    // IDebuggerWindowHost, called by the window on the UI thread.
    void    RunDebuggerCommand       (const std::string & line) override;
    void    RunDebuggerCommandInMode (const std::string & line, CommandMode mode) override;
    void    RunDebuggerAction        (const DebuggerAction & action) override;
    void    PauseDebugger            () override;
    void    RunEmulatorCommand       (int commandId) override;
    void    SetDebuggerCodeLines     (int lines, int view) override;
    void    SetDebuggerCodeAddress   (std::optional<Word> address, int view) override;
    void    SetDebuggerCodeTop       (Word top, int view) override;
    void    SetDebuggerFollowView    (int view) override;
    void    CloseDebuggerCodeView    (int view) override;
    static std::string  GetCodeViewSuffix (int view);
    void    SetDebuggerMemoryWindow  (int id, std::optional<Word> address) override;
    void    SetDebuggerTraceTop      (std::optional<uint64_t> first) override;
    void    SetDebuggerHeatMapShown  (bool shown) override;
    void    GoToDebuggerMemory       (int window, const std::string & text) override;
    void    ScrollDebuggerCode       (int lines, int view) override;
    void    OnDebuggerWindowClosed   () override;
    void    DetachDebugger           () override;
    std::string  GetDebuggerKeyScheme () override;
    void         SetDebuggerKeyScheme (const std::string & name) override;
    std::string  GetDebuggerTheme     () override;
    bool         IsBeamOverlayOn      () override { return m_isBeamOverlayOn.load (memory_order_acquire); }
    void         SetBeamOverlayOn     (bool on) override;
    void         SetDebuggerTheme     (const std::string & name) override;
    ReverseOptions  GetReverseOptions () override;
    void            SetReverseOptions (const ReverseOptions & options) override;
    bool            IsReplayingHistory () override { return m_isReplayingHistory.load (memory_order_acquire); }
    ReplayProgress  GetReplayProgress  () override;
    void            StopReplay         () override { m_replayControl.isStopRequested.store (true, memory_order_release); }
    std::string  GetDebuggerLayout    () override;
    void         SetDebuggerLayout    (const std::string & text) override;
    std::string  GetDebuggerClosedPanes () override;
    void         SetDebuggerClosedPanes (const std::string & text) override;
    std::string  GetDebuggerCommandBarDock () override;
    void         SetDebuggerCommandBarDock (const std::string & text) override;
    std::string  GetDebuggerTimelineDock   () override;
    void         SetDebuggerTimelineDock   (const std::string & text) override;

    HistoryThumbnails *  GetHistoryThumbnails () override { return &m_historyThumbnails; }
    void                 SeekHistoryCycle     (uint64_t cycle, bool isInterim) override;
    bool                 IsHistorySeekBusy    () const override;
    std::string  GetDebuggerFocusedPane () override;
    void         SetDebuggerFocusedPane (const std::string & text) override;
    int          GetDebuggerTextZoomPercent () override;
    void         SetDebuggerTextZoomPercent (int percent) override;
    std::string  GetDebuggerDisassemblyOptions () override;
    void         SetDebuggerDisassemblyOptions (const std::string & text) override;
    std::string  GetDebuggerHeatMapOptions () override;
    void         SetDebuggerHeatMapOptions (const std::string & text) override;
    void         ResetDebuggerHeatMap      () override;
    void         SendDebuggerHeatMapRequest (const std::string & words) override;
    std::string  GetDebuggerHeatMapRanges  () override;
    void         SetDebuggerHeatMapRanges  (const std::string & text) override;
    std::string  GetDebuggerOpenViews () override;
    void         SetDebuggerOpenViews (const std::string & text) override;
    std::string  GetDebuggerPlacementKey () const;
    bool         TryGetDebuggerPlacement (RECT & rectPx) override;
    void         SetDebuggerPlacement    (const RECT & rectPx) override;
    SourceLookup FindDebuggerSource   (const DebugSourceFile & record, const std::wstring & debugFilePath,
                                       const std::string & programKey) override;
    SourceLookup MatchDroppedDebuggerSource (const std::vector<DebugSourceFile> & files, const std::wstring & path,
                                             const std::string & programKey, int & recordIndex) override;
    bool         DoesDebuggerFileExist      (const std::wstring & path) override;

    // The driver of a debugger run, when one is attached. Null on a machine
    // nobody is debugging, which is what keeps the slice loop's cost to a
    // comparison. Owned by whoever attached the session, not by the shell.
    CpuManagerRunDriver  * m_debugRunDriver      = nullptr;
    DebugSession         * m_debugSession        = nullptr;
    DebugCommandHandler    m_debugCommandHandler;
    uint32_t               m_debugCommandClient  = 0;   // the client of the command being run, for the history guard

    // The divergence question's state: the input held behind live and the
    // lines it would change, and a held debugger edit's command for the UI
    // thread to ask about.
    DivergenceGate                m_divergenceGate;
    HeldInputWatch                m_heldInputWatch;                        // the held lines, published here, taken by the CPU thread
    std::atomic<bool>             m_isResumeOwedAfterHeldRead = false;     // a replay stopped at a held read was running
    size_t                        m_pasteLengthBeforeHold     = 0;         // the paste buffer before a paste held behind live
    bool                          m_isJoyportSyncOwed         = false;     // a players' change made behind live, for the Joyport once live
    std::mutex                    m_divergeMutex;
    std::optional<std::string>    m_pendingDivergeCommand;
    std::mutex                    m_replayCaptionMutex;
    std::wstring                  m_replayCaption;                 // " [Replaying @ ...]" behind live, else empty

    // Reverse execution's history of this machine, recording while a machine
    // is built; null until the CPU thread first starts it. The stop test is
    // the debugger's, for reverse continue, or null. CPU thread only.
    std::unique_ptr<ReverseHost>  m_reverseHost;
    IReverseStopTest            * m_reverseStopTest       = nullptr;
    bool                          m_isReverseOn           = false;   // the settings, read when the CPU thread starts and changed by Tools > Options
    int                           m_reverseBudgetMb       = 0;
    int                           m_reverseIntervalFrames = 0;
    std::optional<ReverseOutcome> m_lastReverseOutcome;                // the last reverse command's, for the history band
    uint64_t                      m_lastReversePosition   = 0;       // where it landed

    // Pictures of history for the debugger's timeline, drawn on a scratch
    // machine of the running one's configuration. The renderer is declared
    // first, so it outlives the pictures drawing through it.
    ScratchMachineRenderer        m_historyRenderer;
    HistoryThumbnails             m_historyThumbnails     { m_historyRenderer };

    // The fading heat map's rebuilds after a move through history, replayed
    // on a scratch machine of its own. Declared before the debugger, whose
    // heat map holds it.
    ScratchHeatReplayer           m_heatReplayer;

    // The heat map's look-ups of an address's last access in history, on a
    // scratch machine of its own, replayed on the CPU thread when asked.
    ScratchHeatReplayer           m_heatFinder;

    // The debugger's call record, rebuilt from history when it starts
    // mid-run, on a scratch machine of its own. Declared before the
    // debugger, whose call history holds it.
    ScratchCallReplayer           m_callReplayer;

    void            ServiceHistoryThumbnails();
    bool            TryPublishHistoryPlayhead();
    void            SyncHeatHistory         (bool isAttached);
    void            ServiceCallHistory();

public:

    //  Reachable by a test subclass, which drives the debugger and its call
    //  history as the CPU thread does and takes its views as the window
    //  does: the debug channel's lifecycle, the debugger attached over a
    //  transport the test holds rather than the pipe OpenDebugger opens, the
    //  call record's rebuilder, whose jobs a test runs on a queue of its own,
    //  and what Initialize and the debugger window would set up otherwise --
    //  the framebuffers a reverse command draws into, and whether the window
    //  is showing, since its view is built only while it is.

    // Opens and closes the debug channel. CPU thread only. Opening an open
    // channel does nothing; a channel that cannot open is reported and the
    // emulator goes on without one.
    HRESULT OpenDebugger    ();
    void    CloseDebugger   ();
    void    ServiceDebugger ();
    bool    IsDebuggerOpen  () const { return m_debugController != nullptr; }

    // IDebuggerWindowHost: the window's next view and console lines.
    bool    TakeDebuggerUpdate (std::shared_ptr<const DebuggerViewSnapshot> & snapshot,
                                std::vector<std::string>                     & consoleLines) override;

    // Makes the controller the shell's debugger: the shell's debug pointers
    // at it, and the session's requests routed to the CPU thread.
    void                    AttachDebugger      (std::unique_ptr<DebuggerController> controller);
    ScratchCallReplayer   & GetCallReplayer     ()             { return m_callReplayer; }
    void                    PrepareFramebuffers ();
    void                    SetDebugWindowShown (bool isShown) { m_isDebugWindowShown.store (isShown); }

private:

    EmulatorShell  & m_shell;

    // The debug channel, when `--debugger` opened it. Built and torn down on
    // the CPU thread, and only ever touched there.
    bool                                  m_openDebuggerAtStart = false;
    Win32FileSystem                       m_debugFiles;
    std::unique_ptr<Win32NamedPipeApi>    m_pipeApi;
    std::unique_ptr<Win32PipeTransport>   m_pipeTransport;
    std::unique_ptr<DebuggerController>   m_debugController;

    // The debugger window and what it is shown. The view state and the build
    // clock belong to the CPU thread; the snapshot and console lines cross to
    // the UI thread under the mutex.
    std::unique_ptr<DebuggerWindow>  m_debuggerWindow;
    DebuggerViewState                m_debugViewState;
    ULONGLONG                        m_debugViewBuiltAt      = 0;
    bool                             m_isDebugViewDirty      = true;
    bool                             m_wasPausedAtDebugBuild = false;
    bool                             m_wasHeatRebuilding     = false;   // the last build's heat map awaited its rebuilt heat
    bool                             m_wasCallRebuilding     = false;   // the last build's call stack awaited its rebuilt record

    // While the window is closed and BRKUNINIT keeps the heat map on, how
    // often it is folded, and when it last was.
    static constexpr ULONGLONG       kHiddenHeatFoldMs       = 1000;
    ULONGLONG                        m_heatFoldedAt          = 0;

    std::atomic<bool>                              m_isDebugWindowShown { false };
    bool                                           m_isDetachPending    = false;
    std::mutex                                     m_debugViewMutex;
    std::shared_ptr<const DebuggerViewSnapshot>    m_debugViewSnapshot;
    bool                                           m_isDebugViewFresh   = false;
    std::vector<std::string>                       m_debugConsolePending;
    std::vector<DebuggerAction>                    m_debugActionsPending;

    // The panes are built off the CPU thread from what it gathers each frame.
    // Each build holds the view state lock while it reads the view state, and
    // so does every change the window makes to it on the CPU thread; the
    // device panels and the heat map, which drive the live machine, are built
    // on the CPU thread from members no build reads. The session view is kept
    // between frames so the symbols and the debug file are shared until they
    // change. The queue is declared last so it is destroyed first, waiting for
    // a build still running.
    static constexpr size_t                        kDebugBuildQueueCapacity = 2;
    static constexpr DWORD                         kDebugBuildThreads       = 3;
    std::mutex                                     m_debugViewStateLock;
    DebugSessionView                               m_debugSessionView;
    DebugViewPublisher                             m_debugViewPublisher { m_debugViewState, m_debugViewStateLock,
                                                                          [this] (std::shared_ptr<const DebuggerViewSnapshot> snapshot)
                                                                          {
                                                                              PublishDebugSnapshot (std::move (snapshot));
                                                                          } };
    ParallelWorkPool                               m_debugBuildPool;
    ThreadPoolWorkQueue                            m_debugBuildQueue;

    // The debugger's mark of where the beam is, drawn over the picture
    // while the debugger has the machine stopped.
    atomic<bool>                  m_isBeamOverlayOn{false};
    atomic<bool>                  m_isReplayingHistory{false};      // set by the CPU thread while a reverse command runs
    atomic<uint64_t>              m_replayStartedAt{0};             // the tick count the running reverse command began at
    ReplayControl                 m_replayControl;                  // the running command's progress, and the UI thread's request to stop it
    atomic<uint64_t>              m_seekCyclePosted{UINT64_MAX};    // the cycle the timeline last asked a seek to
    atomic<uint64_t>              m_seekCyclePostedAt{0};           // the tick count it asked at
    atomic<uint64_t>              m_seekCycleLanded{UINT64_MAX};    // the cycle the last seek to a cycle the CPU thread ran asked for
};