#pragma once

#include "Pch.h"

#include "Config/Win32FileSystem.h"
#include "Core/ParallelWorkPool.h"
#include "Core/ThreadPoolWorkQueue.h"
#include "Debugger/DebugSessionView.h"
#include "Debugger/Project/DebugProjectStore.h"
#include "Debugger/Reverse/ReplayControl.h"
#include "Shell/CpuCommandDispatcher.h"
#include "Shell/DebugEventLogs.h"
#include "Shell/DebuggerSliceHooks.h"
#include "Shell/IHistoryClient.h"
#include "Shell/ScratchCallReplayer.h"
#include "Shell/ScratchHeatReplayer.h"
#include "Ui/Debugger/DebuggerActions.h"
#include "Ui/Debugger/DebuggerViewState.h"
#include "Ui/Debugger/DebuggerWindow.h"
#include "Ui/Debugger/DebugViewPublisher.h"

class CpuManager;
class CpuManagerRunDriver;
class DebuggerController;
struct DebuggerSettings;
class DebugSession;
struct DebugSourceFile;
struct DebugViewInput;
class IDebuggerHost;
class IReverseStopTest;
class MachineHost;
class ShellHistory;
struct SourceLookup;
class Win32NamedPipeApi;
class Win32PipeTransport;





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebugger
//
//  The debugger in the emulator: its window and session, the replays of
//  history its views are rebuilt from, and the debug channel. The machine's
//  history is ShellHistory's; the debugger follows it as its IHistoryClient.
//  The emulator's shell constructs one and accesses it through a narrow
//  interface; per-instruction work stays in the machine and the session's
//  hooks.
//
////////////////////////////////////////////////////////////////////////////////

class ShellDebugger : public IDebugCommandTarget,
                      public IHistoryClient,
                      private IDebuggerWindowHost
{
public:
    ShellDebugger (IDebuggerHost & host, MachineHost & machine, CpuManager & cpuManager, DebuggerSettings & settings, ShellHistory & history);
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

    // History's commands from the command queue, run by ShellHistory: a move
    // through history, the options' settings, and the cut a change behind
    // live makes. ReverseContinue stops where the stop test given to
    // SetReverseStopTest fires; without one it fails and the machine stays
    // where it is.
    void  RunReverseCommand     (ReverseCommand command, uint64_t argument) override;
    void  ApplyReverseOptions   (bool isRecording, int budgetMb) override;
    void  DivergeHistory        () override;
    void  SetReverseStopTest    (IReverseStopTest * stopTest) { m_reverseStopTest = stopTest; }

    // IHistoryClient: the debugger's views follow the machine's history.
    void                OnHistoryRestarted (ReverseController * controller) override;
    void                OnHistoryStopping  () override;
    bool                HasNoCallSince     (uint64_t historyStartCycle) override;
    void                GetCallerLinks     (std::vector<CallerLink> & outLinks) override;
    IReverseStopTest  * GetReverseStopTest () override { return m_reverseStopTest; }
    void                OnHistoryMoving    () override;
    void                OnHistoryMoved     (ReverseCommand command) override;
    void                OnHistoryLanded    (const ReverseResult & result) override;
    bool                IsDebugStepRunning () override;
    void                OnPauseChanged     (bool paused) override { NotifyDebugPauseChanged (paused); }

    // UI thread: the prompt to add the last project's items to a more
    // specific project just opened.
    void  OnConfirmProjectMerge ();

    // CPU thread, once a frame: what the slice loop reads of the debugger.
    DebuggerSliceHooks  GetSliceHooks       ();

    // CPU thread: a step of the attached session, as a main window's step
    // key asks for; false when no session is attached. And the views built
    // again on the next publish, after the machine changed under them.
    bool  StepSession      ();
    void  MarkViewDirty    ()   { m_isDebugViewDirty = true; }

    // The lock on the view state that the window's setters change and a
    // build reads. Any thread.
    std::mutex & GetViewStateLock () noexcept { return m_debugViewStateLock; }

    // Whether the beam mark is drawn: turned on and the session stopped.
    bool  IsBeamMarkShown  ();

    // UI thread: the debugger window's frame, drawn with the emulator's, and
    // the window cut off from the debugger and destroyed at shutdown.
    void  RenderWindowFrame   ();
    void  DestroyWindow       ();

    // Where debugger commands go. The session machine events go to is set
    // with SetDebugSession.
    using DebugCommandHandler = std::function<void (uint32_t clientId, const std::string & line, std::optional<CommandMode> mode)>;

    void SetDebugCommandHandler (DebugCommandHandler handler)      { m_debugCommandHandler = std::move (handler); }


    // Shows the debugger window, creating it the first time, and opens the
    // debug channel. UI thread. activate=false leaves the foreground and the
    // keys where they are, for a window the launch opens.
    void    OpenDebuggerWindow (bool activate = true);

    // Opens the window with an event log pane in front. UI thread.
    void    OpenDebuggerLog    (DebugLogKind kind);

    // True when a message went to the debugger window or a floating pane.
    bool    IsDebuggerMessage  (const MSG & msg) const;

    // The window's requests, carried out on the CPU thread (ICpuCommandTarget).
    void    OpenDebugChannel   () override;
    void    CloseDebugChannel  (bool isDetach) override;
    void    PauseDebugRun      () override;
    void    SetDebugView       (const std::string & view, std::optional<Word> address) override;
    void    SetDebugTraceView  (std::optional<uint64_t> first) override;
    void    SetDebugEventLog   (DebugLogKind kind, const std::string & request) override;
    void    GoToDebugMemory    (int window, const std::string & text) override;
    void    ScrollDebugCode    (int lines, int view) override;
    void    RunDebugActions    () override;

    // Whether the heat map pane is shown, and so recording.
    void    SetDebugHeatMapShown   (bool shown) override;
    void    SetDebugHeatColumnsShown (bool shown) override;
    void    SetDebugBasicShown     (bool isVisible) override;
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
    SpeedMode  GetEmulatorSpeed      () override;
    void    SetDebuggerCodeLines     (int lines, int view) override;
    void    SetDebuggerCodeAddress   (std::optional<Word> address, int view) override;
    void    SetDebuggerCodeTop       (Word top, int view) override;
    void    SetDebuggerFollowView    (int view) override;
    void    CloseDebuggerCodeView    (int view) override;
    static std::string  GetCodeViewSuffix (int view);
    void    SetDebuggerMemoryWindow  (int id, std::optional<Word> address) override;
    void    SetDebuggerTraceTop      (std::optional<uint64_t> first) override;
    void    SendDebuggerEventLogRequest (DebugLogKind kind, const std::string & request) override;
    void    TakeDebuggerEventLogs       (DebugLogDeltas & deltas) override;
    std::string  GetDebuggerEventLogOptions (DebugLogKind kind) override;
    void         SetDebuggerEventLogOptions (DebugLogKind kind, const std::string & text) override;
    void    SetDebuggerHeatMapShown  (bool shown) override;
    void    SetDebuggerHeatColumnsShown (bool shown) override;
    void    SetDebuggerBasicShown    (bool isVisible) override;
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
    bool            IsReplayingHistory () override;
    ReplayProgress  GetReplayProgress  () override;
    void            StopReplay         () override;
    std::string  GetDebuggerLayout    () override;
    void         SetDebuggerLayout    (const std::string & text) override;
    std::string  GetDebuggerClosedPanes () override;
    void         SetDebuggerClosedPanes (const std::string & text) override;
    std::string  GetDebuggerCommandBarDock () override;
    void         SetDebuggerCommandBarDock (const std::string & text) override;
    std::string  GetDebuggerTimelineDock   () override;
    void         SetDebuggerTimelineDock   (const std::string & text) override;

    HistoryThumbnails *  GetHistoryThumbnails () override;
    void                 SeekHistoryCycle     (uint64_t cycle, bool isInterim) override;
    bool                 IsHistorySeekBusy    () const override;
    std::string  GetDebuggerFocusedPane () override;
    void         SetDebuggerFocusedPane (const std::string & text) override;
    bool         IsDebuggerEchoingActions  () override;
    void         SetDebuggerEchoingActions (bool isEchoing) override;
    std::string  GetDebuggerWindowState (const std::string & name) override;
    std::map<std::string, std::string>  GetProjectWindowStrings () const;
    void         SetProjectWindowStrings (const std::map<std::string, std::string> & strings);
    void         SetDebuggerWindowState (const std::string & name, const std::string & text) override;
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
    bool         TakeDebuggerProjectState () override;
    bool         TryGetDebuggerProject    (DebugProject & project) override;
    void         SetDebuggerStartupCommands (const std::vector<DebugProject::StartupCommand> & commands) override;
    std::string  GetDebuggerPlacementKey () const;
    bool         TryGetDebuggerPlacement (RECT & rectPx) override;
    void         SetDebuggerPlacement    (const RECT & rectPx) override;
    SourceLookup FindDebuggerSource   (const DebugSourceFile & record, const std::wstring & debugFilePath,
                                       const std::string & programKey) override;
    SourceLookup MatchDroppedDebuggerSource (const std::vector<DebugSourceFile> & files, const std::wstring & path,
                                             const std::string & programKey, int & recordIndex) override;
    bool         DoesDebuggerFileExist      (const std::wstring & path) override;

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
    void                    SetProjectStore     (std::unique_ptr<DebugProjectStore> store) { m_projectStore = std::move (store); }
    void                    ServiceProject      ();
    void                    SaveProject         ();
    bool                    HasMergeOffer       ();

private:

    IDebuggerHost     & m_host;
    MachineHost       & m_machine;
    CpuManager        & m_cpuManager;
    DebuggerSettings  & m_settings;   // the emulator's preferences hold it; saved through the host
    ShellHistory      & m_history;    // the machine's history, which the debugger follows

    // The driver of a debugger run, when one is attached. Null on a machine
    // nobody is debugging, which is what keeps the slice loop's cost to a
    // comparison. Owned by whoever attached the session, not by the shell.
    CpuManagerRunDriver  * m_debugRunDriver      = nullptr;
    DebugSession         * m_debugSession        = nullptr;
    DebugCommandHandler    m_debugCommandHandler;
    uint32_t               m_debugCommandClient  = 0;   // the client of the command being run, for the history guard

    // Reverse continue's stop test, the debugger's, or null. CPU thread only.
    IReverseStopTest            * m_reverseStopTest       = nullptr;

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

    // The Disk II event log and the input log, recording from the start of
    // the session on the History slot. Declared after the history it reads,
    // so it lets go of it first.
    DebugEventLogs                m_eventLogs             { m_machine };

    // Debug projects (FR-166 to FR-171), the CPU thread's: the store, the
    // project open and its file's text as last written, and the disk drive 1
    // had at the last power cycle. A project change is made on the service
    // tick, outside any command. The window strings of a project just opened
    // and the merge prompt's text are passed to the UI thread under the mutex.
    std::unique_ptr<DebugProjectStore>                        m_projectStore;
    DebugProject                                              m_project;
    bool                                                      m_hasProject           = false;
    bool                                                      m_isMergeOwed          = false;
    std::optional<DebugProject>                               m_projectToOpen;
    std::string                                               m_projectSaved;
    std::wstring                                              m_bootDisk;
    bool                                                      m_isBootDiskNoted      = false;
    ULONGLONG                                                 m_projectCheckedAt     = 0;
    std::mutex                                                m_projectMutex;
    std::optional<DebugProject>                               m_projectWindowPending;
    std::optional<DebugProject>                               m_projectCopy;   // a copy of the open project, for the window
    std::optional<std::vector<DebugProject::StartupCommand>>  m_startupPending;
    std::vector<std::string>                                  m_mergeLines;
    std::string                                               m_mergeFrom;
    std::string                                               m_mergeInto;

    // The scope a project's own symbol files called for once it was applied,
    // as when a machine's project loads a debug file. Switching to it is part
    // of the same restore, so the user is not offered a merge.
    std::optional<ProjectScope>                               m_restoredScope;

    void            OpenProjectStore    ();
    ProjectScope    GetProjectScope     ();
    void            NoteBootDisk        ();
    void            SyncProject         ();
    void            ApplyProject        (const DebugProject & project);
    void            RunStartupCommands  ();
    void            OfferProjectMerge   (const DebugProject & old);
    static bool     HasMergeItems       (const DebugProject & project);
    void            RunMergeLines       ();

    static std::optional<int>  GetMergedId (const Reply & reply);

    void            RunProjectRequest   (const std::string & text, Reply & reply);
    void            CaptureProject      (DebugProject & project);
    void            AddConsoleLines     (const std::vector<std::string> & lines);
    void            PublishProject      ();

    void            SyncHeatHistory         (bool isAttached);
    void            ServiceCallHistory();
    void            ServiceEventLogs();

    // The debug channel, when `--debugger` opened it. Built and torn down on
    // the CPU thread, and only ever touched there.
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
    DebugLogDeltas                                 m_eventLogsPending;
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
};