#pragma once

#include "Debugger/Events/DebugLogDelta.h"
#include "Debugger/Project/DebugProject.h"
#include "Debugger/Reverse/HistoryThumbnails.h"
#include "Debugger/Reverse/ReplayControl.h"
#include "Debugger/Source/SourceService.h"
#include "Seams/IHostDialogs.h"
#include "Ui/Debugger/DebuggerActions.h"
#include "Ui/Debugger/DebuggerViewState.h"
#include "Ui/Debugger/ReverseBudget.h"
#include "Ui/UiCommandTypes.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IDebuggerWindowHost
//
//  What the window asks of whoever owns the debugger. Called on the UI thread;
//  the host carries each request to the CPU thread, where the session lives.
//
////////////////////////////////////////////////////////////////////////////////

class IDebuggerWindowHost
{
public:
    virtual ~IDebuggerWindowHost() = default;

    virtual void  RunDebuggerCommand      (const std::string & line)       = 0;
    virtual void  PauseDebugger           ()                               = 0;
    //  A line read in `mode` rather than in the session's own mode.
    virtual void  RunDebuggerCommandInMode (const std::string & line, CommandMode mode) = 0;
    //  A control's action, run directly with its echo shown (FR-135). A host
    //  with no session to run it on, as a test's is, has nothing to do.
    virtual void  RunDebuggerAction       (const DebuggerAction &)          {}
    //  One of the emulator's own menu commands (a resource IDM_ id), such as
    //  Reset or Restart under debugger, which the debugger's menu bar offers
    //  too. A host with no emulator, as a test's is, has nothing to do.
    virtual void  RunEmulatorCommand      (int commandId)                   { (void) commandId; }

    //  The emulator's speed, which the status bar has and its popup sets.
    virtual SpeedMode  GetEmulatorSpeed   ()                                { return SpeedMode::Authentic; }
    //  How many lines the code pane has room for, measured by the window.
    virtual void  SetDebuggerCodeLines    (int lines, int view)             = 0;
    //  A code view: 0 is the first, 1 to 3 the others. An address given for
    //  a closed view opens it there.
    virtual void  SetDebuggerCodeAddress  (std::optional<Word> address, int view) = 0;
    //  Opens a code view with `top` on its first line rather than centered.
    virtual void  SetDebuggerCodeTop      (Word top, int view)             = 0;
    virtual void  SetDebuggerFollowView   (int view)                       = 0;
    virtual void  CloseDebuggerCodeView   (int view)                       = 0;
    virtual void  SetDebuggerMemoryWindow (int id, std::optional<Word> address) = 0;

    //  Where the trace pane reads from: an entry, or the newest when empty.
    virtual void  SetDebuggerTraceTop     (std::optional<uint64_t> first) = 0;

    //  An event log pane's request for the machine: "open", "close", or
    //  "earlier" for the rows before its oldest. And every delta for the
    //  panes made since the last call. A host with no machine, as a test's
    //  is, sends nothing and has none.
    virtual void  SendDebuggerEventLogRequest (DebugLogKind kind, const std::string & request) { (void) kind; (void) request; }
    virtual void  TakeDebuggerEventLogs       (DebugLogDeltas & deltas)                         { (void) deltas; }

    //  An event log pane's options text, saved the same way. A host with no
    //  preferences, as a test's is, has none.
    virtual std::string  GetDebuggerEventLogOptions (DebugLogKind kind)                          { (void) kind; return {}; }
    virtual void         SetDebuggerEventLogOptions (DebugLogKind kind, const std::string & text) { (void) kind; (void) text; }

    //  Whether the heat map pane is shown; the machine records its accesses
    //  only while it is. A host with no machine, as a test's is, records none.
    virtual void  SetDebuggerHeatMapShown (bool shown)                     { (void) shown; }
    virtual void  SetDebuggerHeatColumnsShown (bool shown)                 { (void) shown; }

    //  Whether a BASIC pane is visible; the Applesoft program is read for
    //  the snapshots only while one is.
    virtual void  SetDebuggerBasicShown   (bool isVisible)                 { (void) isVisible; }

    //  A memory window's Go to text, resolved on the CPU thread; the window
    //  acts on it when a snapshot carries the answer.
    virtual void  GoToDebuggerMemory      (int window, const std::string & text) = 0;

    //  Scrolls the code pane by instructions, through the whole address space.
    virtual void  ScrollDebuggerCode      (int lines, int view) = 0;

    //  The newest snapshot, if one arrived since the last call, and every
    //  console line written since then.
    virtual bool  TakeDebuggerUpdate      (std::shared_ptr<const DebuggerViewSnapshot> & snapshot,
                                           std::vector<std::string>                     & consoleLines) = 0;

    virtual void  OnDebuggerWindowClosed  ()                               = 0;

    //  Detach: the close that follows leaves the machine running with the
    //  debugger's CPU hook removed. A host with no machine, as a test's is,
    //  has nothing to do.
    virtual void  DetachDebugger          ()                               {}

    //  The file pickers the R and W prompt opens.
    virtual IHostDialogs &  GetHostDialogs () noexcept                     = 0;

    //  The keyboard scheme preference, by name. Saving it is the host's, so
    //  the choice survives the window.
    virtual std::string  GetDebuggerKeyScheme ()                           = 0;
    virtual void         SetDebuggerKeyScheme (const std::string & name)   = 0;

    //  The window's own theme, by DebuggerThemes name, kept the same way. A
    //  host that keeps no preferences, as a test's is, follows the emulator.
    virtual std::string  GetDebuggerTheme     ()                           { return {}; }

    //  Whether the emulator's screen marks where the video beam is while the
    //  machine is stopped. A host with no screen, as a test's is, has none.
    virtual bool         IsBeamOverlayOn      ()                           { return false; }
    virtual void         SetBeamOverlayOn     (bool on)                    { (void) on; }
    virtual void         SetDebuggerTheme     (const std::string &)        {}

    //  Reverse execution's settings, kept the same way. A host that keeps no
    //  preferences, as a test's is, has the defaults and keeps nothing.
    virtual ReverseOptions  GetReverseOptions ()                        { return {}; }
    virtual void            SetReverseOptions (const ReverseOptions &)  {}

    //  The panes' text size as a whole percentage, kept the same way. A host
    //  that keeps no preferences, as a test's is, has 100%.
    virtual int             GetDebuggerTextZoomPercent ()               { return 100; }
    virtual void            SetDebuggerTextZoomPercent (int percent)    { (void) percent; }

    //  Whether the CPU thread is replaying history to reach a point in it
    //  right now, which no snapshot can report since none is built until it
    //  ends. A host with no machine, as a test's is, never is.
    virtual bool            IsReplayingHistory ()                       { return false; }

    //  The replay's progress, read the same way, and a request to stop it,
    //  which the CPU thread sees between stretches of history without
    //  waiting for the command queue.
    virtual ReplayProgress  GetReplayProgress  ()                       { return ReplayProgress { IsReplayingHistory() }; }
    virtual void            StopReplay         ()                       {}

    //  The pane arrangement as DxuiPaneLayout text, kept the same way.
    virtual std::string  GetDebuggerLayout    ()                           = 0;
    virtual void         SetDebuggerLayout    (const std::string & text)   = 0;

    //  The fixed panes the user closed, in DebuggerLayout's text for them,
    //  kept in a setting of their own. A host that keeps no preferences, as a
    //  test's is, has none closed.
    virtual std::string  GetDebuggerClosedPanes ()                         { return {}; }
    virtual void         SetDebuggerClosedPanes (const std::string &)      {}

    //  Where the command bar is docked, in DxuiToolbarDock's text, kept the
    //  same way. A host with no preferences keeps it across the top.
    virtual std::string  GetDebuggerCommandBarDock ()                      { return {}; }
    virtual void         SetDebuggerCommandBarDock (const std::string &)   {}

    //  Where the history timeline is docked, kept the same way.
    virtual std::string  GetDebuggerTimelineDock   ()                      { return {}; }
    virtual void         SetDebuggerTimelineDock   (const std::string &)   {}

    //  The history timeline's pictures, a seek to a cycle in history for a
    //  stopped machine, isInterim while a drag of the timeline goes on, and
    //  whether the last seek asked for has yet to land. A host that records
    //  no history, as a test's does not, has no pictures and moves nowhere.
    virtual HistoryThumbnails *  GetHistoryThumbnails ()                              { return nullptr; }
    virtual void                 SeekHistoryCycle     (uint64_t cycle, bool isInterim) { (void) cycle; (void) isInterim; }
    virtual bool                 IsHistorySeekBusy    () const                        { return false; }

    //  The pane that had the keys when the window closed, by its layout id,
    //  kept the same way. A host with no preferences has none saved.
    virtual std::string  GetDebuggerFocusedPane ()                         { return {}; }
    virtual void         SetDebuggerFocusedPane (const std::string &)      {}

    //  The window's state with no setting of its own, by name, saved with the
    //  debug project: "console", "find" and "columns".
    virtual std::string  GetDebuggerWindowState (const std::string & name)                       { (void) name; return {}; }
    virtual void         SetDebuggerWindowState (const std::string & name, const std::string & text) { (void) name; (void) text; }

    //  The disassembly views' viewing options, in DisassemblyOptions' text for
    //  them, kept the same way; none kept gives the defaults.
    virtual std::string  GetDebuggerDisassemblyOptions ()                  { return {}; }
    virtual void         SetDebuggerDisassemblyOptions (const std::string &) {}

    //  The heat map's options, in HeatMapOptions' text, kept the same way and
    //  passed on to the machine; and its Reset, which zeroes what the machine
    //  counted. A host with no machine, as a test's is, keeps nothing.
    virtual std::string  GetDebuggerHeatMapOptions ()                      { return {}; }
    virtual void         SetDebuggerHeatMapOptions (const std::string &)   {}
    virtual void         ResetDebuggerHeatMap      ()                      {}

    //  A heat map request for the machine, in the words after "heatmap" of
    //  its command: the cell the mouse is over ("hover 2000", "hover none"),
    //  or an access to show or go back to (HeatAccessJump::FormatWords).
    virtual void         SendDebuggerHeatMapRequest (const std::string & words) { (void) words; }

    //  The heat map's sets of ranges and the set it shows, in HeatMapRangeSets'
    //  text, kept the same way. Empty until the ranges pane first opens, so the
    //  pane opens closed until then.
    virtual std::string  GetDebuggerHeatMapRanges  ()                      { return {}; }
    virtual void         SetDebuggerHeatMapRanges  (const std::string &)   {}

    //  Which optional views were open, in DebuggerViewState's text for them,
    //  kept the same way.
    virtual std::string  GetDebuggerOpenViews ()                           = 0;
    virtual void         SetDebuggerOpenViews (const std::string & text)   = 0;

    //  The debug project open, a copy; false with none. Its startup commands
    //  replaced. And whether a project opened since the last call, its
    //  window strings now the ones above, for the window to read again.
    virtual bool  TryGetDebuggerProject      (DebugProject & project)                                   { (void) project; return false; }
    virtual void  SetDebuggerStartupCommands (const std::vector<DebugProject::StartupCommand> & commands) { (void) commands; }
    virtual bool  TakeDebuggerProjectState   ()                                                          { return false; }

    //  Where the user last put the debugger window, for this monitor
    //  arrangement. False when there is nothing to restore.
    virtual bool  TryGetDebuggerPlacement (RECT & rectPx)                  = 0;
    virtual void  SetDebuggerPlacement    (const RECT & rectPx)            = 0;

    //  A debug file's source file, found by the rules of FR-058, and a file
    //  the user dropped, matched against the debug file's records. Where a
    //  file is found goes into the preferences, which the host keeps.
    virtual SourceLookup  FindDebuggerSource         (const DebugSourceFile & record, const std::wstring & debugFilePath,
                                                      const std::string & programKey) = 0;
    virtual SourceLookup  MatchDroppedDebuggerSource (const std::vector<DebugSourceFile> & files, const std::wstring & path,
                                                      const std::string & programKey, int & recordIndex) = 0;

    //  Whether a file is on disk, for the debug file beside a source. A host
    //  with no file system, as a test's is, has none.
    virtual bool  DoesDebuggerFileExist (const std::wstring & path)        { (void) path; return false; }
};





////////////////////////////////////////////////////////////////////////////////
//
//  NullDebuggerWindowHost
//
//  A host with nothing behind it: every request does nothing, every query
//  returns nothing, and its file pickers are always backed out of. For a
//  window whose own state is all that is read, as a test's is.
//
////////////////////////////////////////////////////////////////////////////////

class NullDebuggerWindowHost : public IDebuggerWindowHost
{
public:
    void  RunDebuggerCommand       (const std::string &)              override {}
    void  RunDebuggerCommandInMode (const std::string &, CommandMode) override {}
    void  PauseDebugger            ()                                 override {}
    void  SetDebuggerCodeLines     (int, int)                         override {}
    void  SetDebuggerCodeAddress   (std::optional<Word>, int)         override {}
    void  SetDebuggerCodeTop       (Word, int)                        override {}
    void  SetDebuggerFollowView    (int)                              override {}
    void  CloseDebuggerCodeView    (int)                              override {}
    void  SetDebuggerMemoryWindow  (int, std::optional<Word>)         override {}
    void  SetDebuggerTraceTop      (std::optional<uint64_t>)          override {}
    void  GoToDebuggerMemory       (int, const std::string &)         override {}
    void  ScrollDebuggerCode       (int, int)                         override {}
    void  OnDebuggerWindowClosed   ()                                 override {}
    void  SetDebuggerKeyScheme     (const std::string &)              override {}
    void  SetDebuggerLayout        (const std::string &)              override {}
    void  SetDebuggerOpenViews     (const std::string &)              override {}
    void  SetDebuggerPlacement     (const RECT &)                     override {}

    bool  TakeDebuggerUpdate      (std::shared_ptr<const DebuggerViewSnapshot> &, std::vector<std::string> &) override { return false; }
    bool  TryGetDebuggerPlacement (RECT &)                                                                    override { return false; }

    IHostDialogs &  GetHostDialogs       () noexcept override { return m_dialogs; }
    std::string     GetDebuggerKeyScheme ()          override { return {}; }
    std::string     GetDebuggerLayout    ()          override { return {}; }
    std::string     GetDebuggerOpenViews ()          override { return {}; }

    SourceLookup  FindDebuggerSource         (const DebugSourceFile &, const std::wstring &, const std::string &)                     override { return {}; }
    SourceLookup  MatchDroppedDebuggerSource (const std::vector<DebugSourceFile> &, const std::wstring &, const std::string &, int &) override { return {}; }

private:
    NullHostDialogs  m_dialogs;
};
