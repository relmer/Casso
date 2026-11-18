#pragma once

#include "Pch.h"

#include "Config/Win32FileSystem.h"
#include "Debugger/Reverse/ReverseCommand.h"
#include "Shell/CpuCommandDispatcher.h"
#include "Shell/DebugEventLogs.h"
#include "Shell/DebuggerSliceHooks.h"
#include "Shell/DebugViewModel.h"
#include "Shell/DebugWindowMailbox.h"
#include "Shell/HistoryDivergence.h"
#include "Shell/IShellDebugContext.h"
#include "Shell/ShellDebugProject.h"
#include "Shell/ShellDebuggerPrefs.h"
#include "Shell/ShellDebugView.h"
#include "Shell/ShellHistory.h"
#include "Ui/Debugger/DebuggerActions.h"
#include "Ui/Debugger/DebuggerViewState.h"
#include "Ui/Debugger/DebuggerWindow.h"

class CpuManager;
class CpuManagerRunDriver;
class DebuggerController;
struct DebuggerSettings;
struct HistorySettings;
class DebugSession;
struct DebugSourceFile;
class IDebuggerHost;
class MachineHost;
struct SourceLookup;
class Win32NamedPipeApi;
class Win32PipeTransport;





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebugger
//
//  The debugger the emulator has: its window and session, the debug channel,
//  and the views built for the window. Its parts are components:
//  DebugViewModel has the window's view state, DebugWindowMailbox what
//  crosses to the UI thread, ShellHistory reverse execution and the replays
//  of history, HistoryDivergence the forks and the input deferred behind
//  live, ShellDebugProject the debug projects, and ShellDebugView the
//  window's views. The parts use the
//  debugger through IShellDebugContext. The emulator's shell has one and
//  calls it through a narrow interface; per-instruction work stays in the
//  machine and the session's hooks.
//
////////////////////////////////////////////////////////////////////////////////

class ShellDebugger : public IDebugCommandTarget,
                      private IDebuggerWindowHost,
                      private IShellDebugContext
{
public:
    ShellDebugger (IDebuggerHost & host, MachineHost & machine, CpuManager & cpuManager, DebuggerSettings & settings, HistorySettings & historySettings);
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
    void NotifyDebugMachineChanged (const std::string & machineName, bool isStateLoaded = false);

    // A machine just built is power cycled with its drives empty and then
    // has its disks mounted, so the disk now in drive 1 is the one it boots,
    // and the project's scope.
    void NotifyDebugDisksMounted   ();

    // The debugger's parts: the window's view state and its views, reverse
    // execution's history, the forks and the input deferred behind live, and
    // the debug projects.
    DebugViewModel     & GetViewModel  () { return m_viewModel; }
    ShellDebugView     & GetView       () { return m_view; }
    ShellHistory       & GetHistory    () { return m_history; }
    HistoryDivergence  & GetDivergence () { return m_divergence; }
    ShellDebugProject  & GetProject    () { return m_project; }

    // IDebugCommandTarget: the reverse commands, Tools > Options' settings
    // and the cut after a yes, on the CPU thread.
    void  RunReverseCommand   (ReverseCommand command, uint64_t argument) override { m_history.RunCommand (command, argument); }
    void  ApplyReverseOptions (bool isRecording, int budgetMb) override           { m_history.ApplyOptions (isRecording, budgetMb); }
    void  DivergeHistory      () override                                          { m_divergence.Diverge(); }

    // CPU thread, once a frame: what the slice loop reads of the debugger.
    DebuggerSliceHooks  GetSliceHooks ();

    // CPU thread: a step of the attached session, for the main window's step
    // key; false when no session is attached.
    bool  StepSession      ();

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
    void    OpenDebugChannel     () override;
    void    CloseDebugChannel    (bool isDetach) override;
    void    PauseDebugRun        () override;
    void    SetDebugFollowView   (int view) override                                 { m_viewModel.SetFollowView (view); }
    void    CloseDebugCodeView   (int view) override                                 { m_viewModel.CloseCodeView (view); }
    void    OpenDebugCodeViewAt  (int view, Word top) override                       { m_viewModel.OpenCodeViewAt (view, top); }
    void    SetDebugCodeLines    (int lines, int view) override                      { m_viewModel.SetCodeLines (lines, view); }
    void    SetDebugCodeShown    (int lines, int view) override                      { m_viewModel.SetCodeShown (lines, view); }
    void    ShowDebugCode        (int view, std::optional<Word> address) override    { m_viewModel.ShowCode (view, address); }
    void    SetDebugMemoryWindow (int id, std::optional<Word> address) override      { m_viewModel.SetMemoryWindow (id, address); }
    void    SetDebugTraceView    (std::optional<uint64_t> first) override            { m_viewModel.SetTraceView (first); }
    void    SetDebugEventLog     (DebugLogKind kind, const DebugLogRequest & request) override { m_eventLogs.ApplyRequest (kind, request); }
    void    GoToDebugMemory      (int window, const std::string & text) override;
    void    SelectDebugCode      (int view, Word address) override                   { m_viewModel.SelectCode (view, address); }
    void    ScrollDebugCode      (int lines, int view) override                      { m_viewModel.ScrollCode (lines, view); }
    void    HoldDebugCode        (bool isHeld, int view) override                    { m_viewModel.HoldCode (isHeld, view); }
    void    RunDebugActions      () override;

    // Whether the heat map pane is shown, and so recording.
    void  SetDebugHeatMapShown     (bool shown) override                             { m_viewModel.SetHeatMapShown (shown); }
    void  SetDebugHeatColumnsShown (bool shown) override                             { m_viewModel.SetHeatColumnsShown (shown); }
    void  SetDebugTraceShown       (bool shown) override                             { m_viewModel.SetTraceShown (shown); }
    void  SetDebugSoundMuted       (int channel, bool isMuted) override;
    void  SetDebugBasicShown       (bool isVisible) override                         { m_viewModel.SetBasicShown (isVisible); }
    void  SetDebugHeatMapOptions   (const HeatMapOptions & options) override         { m_viewModel.SetHeatMapOptions (options); }
    void  ResetDebugHeatMap        () override;
    void  SetDebugHeatMapHover     (std::optional<Word> address) override            { m_viewModel.SetHeatMapHover (address); }
    void  RunDebugHeatMapAccess    (const HeatAccessRequest & request) override;
    void  SetDebugHeatMapIgnore    (const std::vector<std::pair<Word, Word>> & spans) override;

    // Draws a stopped machine's picture again when the beam mark changed.
    void    RedrawDebugFrame     () override;

    // IDebuggerWindowHost, called by the window on the UI thread.
    void                RunDebuggerCommand          (const std::string & line) override;
    void                RunDebuggerCommandInMode    (const std::string & line, CommandMode mode) override;
    void                RunDebuggerAction           (const DebuggerAction & action) override;
    void                PauseDebugger               () override;
    void                RunEmulatorCommand          (int commandId) override;
    SpeedMode           GetEmulatorSpeed            () override;
    void                SetDebuggerCodeLines        (int lines, int view) override;
    void                SetDebuggerCodeShown        (int lines, int view) override;
    void                SetDebuggerCodeAddress      (std::optional<Word> address, int view) override;
    void                SetDebuggerCodeTop          (Word top, int view) override;
    void                SetDebuggerFollowView       (int view) override;
    void                CloseDebuggerCodeView       (int view) override;
    void                SetDebuggerMemoryWindow     (int id, std::optional<Word> address) override;
    void                SetDebuggerTraceTop         (std::optional<uint64_t> first) override;
    void                SendDebuggerEventLogRequest (DebugLogKind kind, const DebugLogRequest & request) override;
    void                TakeDebuggerEventLogs       (DebugLogDeltas & deltas) override { m_mailbox.TakeEventLogs (deltas); }
    std::string         GetDebuggerEventLogOptions  (DebugLogKind kind) override { return m_prefs.GetDebuggerEventLogOptions (kind); }
    void                SetDebuggerEventLogOptions  (DebugLogKind kind, const std::string & text) override { m_prefs.SetDebuggerEventLogOptions (kind, text); }
    void                SetDebuggerHeatMapShown     (bool shown) override;
    void                SetDebuggerHeatColumnsShown (bool shown) override;
    void                SetDebuggerTraceShown       (bool shown) override;
    void                SetDebuggerSoundMuted       (int channel, bool isMuted) override;
    void                SetDebuggerBasicShown       (bool isVisible) override;
    void                GoToDebuggerMemory          (int window, const std::string & text) override;
    void                SelectDebuggerCode          (int view, Word address) override;
    void                ScrollDebuggerCode          (int lines, int view) override;
    void                HoldDebuggerCode            (int view, bool isHeld) override;
    void                OnDebuggerWindowClosed      () override;
    void                DetachDebugger              () override;
    std::string         GetDebuggerKeyScheme        () override { return m_prefs.GetDebuggerKeyScheme(); }
    void                SetDebuggerKeyScheme        (const std::string & name) override { m_prefs.SetDebuggerKeyScheme (name); }
    std::string         GetDebuggerTheme            () override { return m_prefs.GetDebuggerTheme(); }
    bool                IsBeamOverlayOn             () override { return m_isBeamOverlayOn.load (memory_order_acquire); }
    void                SetBeamOverlayOn            (bool on) override;
    void                SetDebuggerTheme            (const std::string & name) override { m_prefs.SetDebuggerTheme (name); }
    ReverseOptions      GetReverseOptions           () override;
    void                SetReverseOptions           (const ReverseOptions & options) override;
    bool                IsReplayingHistory          () override { return m_history.IsReplaying(); }
    ReplayProgress      GetReplayProgress           () override { return m_history.GetReplayProgress(); }
    void                StopReplay                  () override { m_history.StopReplay(); }
    std::string         GetDebuggerLayout           () override { return m_prefs.GetDebuggerLayout(); }
    void                SetDebuggerLayout           (const std::string & text) override { m_prefs.SetDebuggerLayout (text); }
    std::string         GetDebuggerClosedPanes      () override { return m_prefs.GetDebuggerClosedPanes(); }
    void                SetDebuggerClosedPanes      (const std::string & text) override { m_prefs.SetDebuggerClosedPanes (text); }
    std::string         GetDebuggerCommandBarDock   () override { return m_prefs.GetDebuggerCommandBarDock(); }
    void                SetDebuggerCommandBarDock   (const std::string & text) override { m_prefs.SetDebuggerCommandBarDock (text); }
    std::string         GetDebuggerTimelineDock     () override { return m_prefs.GetDebuggerTimelineDock(); }
    void                SetDebuggerTimelineDock     (const std::string & text) override { m_prefs.SetDebuggerTimelineDock (text); }

    HistoryThumbnails                   * GetHistoryThumbnails             () override { return &m_history.GetThumbnails(); }
    void                                  SeekHistoryCycle                 (uint64_t cycle, bool isInterim) override { m_history.SeekCycle (cycle, isInterim); }
    bool                                  IsHistorySeekBusy                () const override { return m_history.IsSeekBusy(); }
    std::string                           GetDebuggerFocusedPane           () override { return m_prefs.GetDebuggerFocusedPane(); }
    void                                  SetDebuggerFocusedPane           (const std::string & text) override { m_prefs.SetDebuggerFocusedPane (text); }
    bool                                  IsDebuggerEchoingActions         () override { return m_prefs.IsDebuggerEchoingActions(); }
    void                                  SetDebuggerEchoingActions        (bool isEchoing) override { m_prefs.SetDebuggerEchoingActions (isEchoing); }
    bool                                  IsDebuggerLinkingToFileExplorer  () override { return m_prefs.IsDebuggerLinkingToFileExplorer(); }
    void                                  SetDebuggerLinkingToFileExplorer (bool isFileExplorer) override { m_prefs.SetDebuggerLinkingToFileExplorer (isFileExplorer); }
    std::string                           GetDebuggerWindowState           (const std::string & name) override { return m_prefs.GetDebuggerWindowState (name); }
    void                                  SetDebuggerWindowState           (const std::string & name, const std::string & text) override { m_prefs.SetDebuggerWindowState (name, text); }
    bool                                  IsDebuggerSkipNoticeShown        () override { return m_prefs.IsDebuggerSkipNoticeShown(); }
    void                                  SetDebuggerSkipNoticeShown       (bool isShown) override { m_prefs.SetDebuggerSkipNoticeShown (isShown); }
    int                                   GetDebuggerTextZoomPercent       () override { return m_prefs.GetDebuggerTextZoomPercent(); }
    void                                  SetDebuggerTextZoomPercent       (int percent) override { m_prefs.SetDebuggerTextZoomPercent (percent); }
    std::string                           GetDebuggerDisassemblyOptions    () override { return m_prefs.GetDebuggerDisassemblyOptions(); }
    void                                  SetDebuggerDisassemblyOptions    (const std::string & text) override { m_prefs.SetDebuggerDisassemblyOptions (text); }
    std::string                           GetDebuggerHeatMapOptions        () override { return m_prefs.GetDebuggerHeatMapOptions(); }
    void                                  SetDebuggerHeatMapOptions        (const HeatMapOptions & options) override { m_prefs.SetDebuggerHeatMapOptions (options); }
    std::string                           GetDebuggerDiskDrives            () override { return m_prefs.GetDebuggerDiskDrives(); }
    void                                  SetDebuggerDiskDrives            (const std::string & text) override { m_prefs.SetDebuggerDiskDrives (text); }
    std::string                           GetDebuggerProfilerPeriod        () override { return m_prefs.GetDebuggerProfilerPeriod(); }
    void                                  SetDebuggerProfilerPeriod        (const std::string & text) override { m_prefs.SetDebuggerProfilerPeriod (text); }
    void                                  ResetDebuggerHeatMap             () override;
    void                                  SetDebuggerHeatMapHover          (std::optional<Word> address) override;
    void                                  RunDebuggerHeatMapAccess         (const HeatAccessRequest & request) override;
    void                                  SetDebuggerHeatMapIgnore         (const std::vector<std::pair<Word, Word>> & spans) override;
    std::string                           GetDebuggerHeatMapRanges         () override { return m_prefs.GetDebuggerHeatMapRanges(); }
    void                                  SetDebuggerHeatMapRanges         (const std::string & text) override { m_prefs.SetDebuggerHeatMapRanges (text); }
    std::string                           GetDebuggerOpenViews             () override { return m_prefs.GetDebuggerOpenViews(); }
    void                                  SetDebuggerOpenViews             (const std::string & text) override { m_prefs.SetDebuggerOpenViews (text); }
    bool                                  TakeDebuggerProjectState         () override { return m_project.TakeWindowState(); }
    bool                                  TryGetDebuggerProject            (DebugProject & project) override { return m_project.TryGetCopy (project); }
    void                                  SetDebuggerStartupCommands       (const std::vector<DebugProject::StartupCommand> & commands) override { m_project.SetStartupCommands (commands); }
    bool                                  TryGetDebuggerPlacement          (RECT & rectPx) override { return m_prefs.TryGetDebuggerPlacement (rectPx); }
    void                                  SetDebuggerPlacement             (const RECT & rectPx) override { m_prefs.SetDebuggerPlacement (rectPx); }
    SourceLookup FindDebuggerSource   (const DebugSourceFile & record, const std::wstring & debugFilePath,
                                       const std::string & programKey) override
    {
        return m_prefs.FindDebuggerSource (record, debugFilePath, programKey);
    }

    SourceLookup MatchDroppedDebuggerSource (const std::vector<DebugSourceFile> & files, const std::wstring & path,
                                             const std::string & programKey, int & recordIndex) override
    {
        return m_prefs.MatchDroppedDebuggerSource (files, path, programKey, recordIndex);
    }

    bool         DoesDebuggerFileExist      (const std::wstring & path) override;

public:

    //  Reachable by a test subclass, which drives the debugger and its call
    //  history as the CPU thread does and takes its views as the window
    //  does: the debug channel's lifecycle, the debugger attached over a
    //  transport the test has rather than the pipe OpenDebugger opens, and
    //  what Initialize and the debugger window would set up otherwise --
    //  the framebuffers a reverse command draws into, and whether the window
    //  is showing, since its view is built only while it is.

    // Opens and closes the debug channel. CPU thread only. Opening an open
    // channel does nothing; a channel that cannot open is reported and the
    // emulator goes on without one.
    HRESULT OpenDebugger    ();
    void    CloseDebugger   ();
    void    ServiceDebugger ();
    bool    IsDebuggerOpen  () const { return m_debugController != nullptr; }

    // IDebuggerWindowHost: the window's next view and console lines, and a
    // file a console link opens.
    bool    TakeDebuggerUpdate (std::shared_ptr<const DebuggerViewSnapshot> & snapshot,
                                std::vector<std::string>                     & consoleLines) override
    {
        return m_mailbox.TakeUpdate (snapshot, consoleLines);
    }

    void    OpenDebuggerFile   (const std::wstring & path) override;

    // Makes the controller the shell's debugger: the shell's debug pointers
    // at it, and the session's requests routed to the CPU thread.
    void                    AttachDebugger      (std::unique_ptr<DebuggerController> controller);
    void                    PrepareFramebuffers ();
    void                    ServiceHitLogs      ();

private:
    // IShellDebugContext, for the parts.
    DebuggerController   * GetDebugController () override { return m_debugController.get(); }
    DebugSession         * GetDebugSession    () override { return m_debugSession; }
    CpuManagerRunDriver  * GetDebugRunDriver  () override { return m_debugRunDriver; }
    void                   AddConsoleLines    (const std::vector<std::string> & lines) override;
    void                   GetQuestionOwner   (bool isActiveOnly, HWND & owner, const IDxuiTheme * & theme) override;

    void         ServicePendingScript      ();
    void         ServiceEventLogs          ();

    IDebuggerHost     & m_host;
    MachineHost       & m_machine;
    CpuManager        & m_cpuManager;
    DebuggerSettings  & m_settings;          // the emulator's preferences have it; saved through the host
    HistorySettings   & m_historySettings;   // likewise

    // The driver of a debugger run, when one is attached. Null on a machine
    // nobody is debugging, which is what keeps the slice loop's cost to a
    // comparison. Owned by whoever attached the session, not by the shell.
    CpuManagerRunDriver  * m_debugRunDriver      = nullptr;
    DebugSession         * m_debugSession        = nullptr;
    DebugCommandHandler    m_debugCommandHandler;
    uint32_t               m_debugCommandClient  = 0;   // the client of the command being run, for the history guard

    // Reverse execution, with its scratch machines, declared before the
    // debugger, whose heat map and call history have references to them;
    // and the forks and input behind live.
    ShellHistory                  m_history               { m_host, m_machine, m_cpuManager, m_historySettings, m_eventLogs, *this, m_viewModel };
    HistoryDivergence             m_divergence            { m_host, m_machine, m_cpuManager, m_history, *this, m_viewModel };

    // The Disk II event log and the input log, recording from the start of
    // the session on the History slot. Declared after the history it reads,
    // so it lets go of it first.
    DebugEventLogs                m_eventLogs             { m_machine };

    // The debug projects, and the debugger window's preferences, written
    // under the project lock.
    ShellDebugProject             m_project               { m_host, m_machine, m_settings, m_debugFiles, *this, m_viewModel };
    ShellDebuggerPrefs            m_prefs                 { m_host, m_cpuManager, m_settings, m_project.GetMutex() };

    // The debug channel, when `--debugger` opened it. Built and torn down on
    // the CPU thread, and only ever touched there.
    Win32FileSystem                       m_debugFiles;
    std::unique_ptr<Win32NamedPipeApi>    m_pipeApi;
    std::unique_ptr<Win32PipeTransport>   m_pipeTransport;
    std::unique_ptr<DebuggerController>   m_debugController;

    // The debugger window and its view state, which the CPU thread accesses;
    // the snapshot and console lines cross to the UI thread
    // through the mailbox.
    std::unique_ptr<DebuggerWindow>  m_debuggerWindow;
    DebugViewModel                   m_viewModel;
    bool                             m_isDetachPending       = false;
    DebugWindowMailbox               m_mailbox;

    // The debugger's mark of where the beam is, drawn over the picture
    // while the debugger has the machine stopped.
    atomic<bool>                     m_isBeamOverlayOn       {false};

    // The window's views, built off the CPU thread. Declared last so it is
    // destroyed first, which blocks until a running build finishes.
    ShellDebugView                   m_view                  { m_machine, m_cpuManager, m_history, m_eventLogs, *this, m_viewModel, m_mailbox };
};
