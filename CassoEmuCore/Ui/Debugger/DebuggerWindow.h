#pragma once

#include "Window/DxuiWindow.h"
#include "Window/DxuiDockedWindow.h"
#include "Core/DxuiFocusManager.h"
#include "Widgets/DxuiButton.h"
#include "Widgets/DxuiDockSite.h"
#include "Widgets/DxuiToolbar.h"
#include "Ui/Debugger/DebuggerCommands.h"
#include "Widgets/DxuiLabel.h"
#include "Widgets/DxuiListView.h"
#include "Widgets/DxuiTextInput.h"
#include "Seams/IHostDialogs.h"
#include "Ui/Debugger/DebuggerKeySchemes.h"
#include "Ui/Debugger/DebuggerViewState.h"
#include "Ui/Debugger/Panes/CallStackPane.h"
#include "Ui/Debugger/Panes/DebuggerPaneFrame.h"
#include "Ui/Debugger/Panes/DiagnosticsPane.h"
#include "Ui/Debugger/Panes/MemoryPane.h"
#include "Ui/Debugger/Panes/SourcePane.h"
#include "Ui/Debugger/Panes/TracePane.h"

struct CassoTheme;





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
    //  How many lines the code pane has room for, measured by the window.
    virtual void  SetDebuggerCodeLines    (int lines)                       = 0;
    virtual void  SetDebuggerCodeAddress  (std::optional<Word> address)    = 0;
    virtual void  SetDebuggerMemoryWindow (int id, std::optional<Word> address) = 0;

    //  Where the trace pane reads from: an entry, or the newest when empty.
    virtual void  SetDebuggerTraceTop     (std::optional<uint64_t> first) = 0;

    //  The newest snapshot, if one arrived since the last call, and every
    //  console line written since then.
    virtual bool  TakeDebuggerUpdate      (std::shared_ptr<const DebuggerViewSnapshot> & snapshot,
                                           std::vector<std::string>                     & consoleLines) = 0;

    virtual void  OnDebuggerWindowClosed  ()                               = 0;

    //  The file pickers the R and W prompt opens.
    virtual IHostDialogs &  GetHostDialogs () noexcept                     = 0;

    //  The keyboard scheme preference, by name. Saving it is the host's, so
    //  the choice survives the window.
    virtual std::string  GetDebuggerKeyScheme ()                           = 0;
    virtual void         SetDebuggerKeyScheme (const std::string & name)   = 0;

    //  The pane arrangement as DxuiPaneLayout text, kept the same way.
    virtual std::string  GetDebuggerLayout    ()                           = 0;
    virtual void         SetDebuggerLayout    (const std::string & text)   = 0;

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
};





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow
//
//  The debugger beside the emulator: code, registers, memory, stack, watches
//  and breakpoints, a command box, and step and run controls.
//
//  IT DRAWS SNAPSHOTS AND SENDS COMMANDS. It holds no session and reads no
//  machine state; the CPU thread builds a DebuggerViewSnapshot and the window
//  paints the latest one it has. Every control produces a command line through
//  DebuggerViewState, so what a click does is exactly what typing the command
//  would do.
//
//  THE CURRENT LINE AND BREAKPOINTS ARE MARKED IN A GUTTER. The code pane's
//  first column carries the PC's arrow and a breakpoint's dot, and a click
//  there sets or clears the breakpoint.
//
////////////////////////////////////////////////////////////////////////////////

class DebuggerWindow : public DxuiWindow
{
public:
    DebuggerWindow() = default;
    ~DebuggerWindow() override;

    HRESULT  Create      (HINSTANCE hInstance, HWND hwndOwner, const CassoTheme * theme, IDebuggerWindowHost * host);
    void     RenderFrame ();

    //  Detaches the host, for a shell tearing down before the window.
    void     DetachHost  () { m_host = nullptr; }

    //  Writes the placement if the user put the window somewhere else, for a
    //  shell shutting down while the window is still open.
    void     SavePlacementIfMoved ();

protected:
    void     OnCreate        () override;
    void     OnWindowPlaced  () override;
    void     OnWindowClose   () override;
    void     Layout          (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    bool     OnMouse         (const DxuiMouseEvent & ev) override;
    bool     OnKey           (const DxuiKeyEvent   & ev) override;
    bool     OnMappedCommand (int commandId) override;
    bool     OnFilesDropped  (const std::vector<std::wstring> & paths) override;
    LPCWSTR  GetCursorForPoint (POINT clientPx) const override;

private:
    static constexpr int    kPreferredWidthDip  = 1100;
    static constexpr int    kPreferredHeightDip = 840;
    static constexpr int    kMinWidthDip        = 760;
    static constexpr int    kMinHeightDip       = 520;
    static constexpr int    kConsoleLineLimit   = 2000;

    //  Pane metrics (FR-026a): the monospace face at a size whose line height
    //  a row barely exceeds, small cell padding, and eight rows owed to every
    //  pane at the default size.
    static constexpr float  kPaneFontDip        = 12.0f;
    static constexpr int    kPaneRowDip         = 16;
    static constexpr int    kPaneHeaderDip      = 22;
    static constexpr int    kPaneEdgeDip        = 6;
    static constexpr int    kPanePadDip         = 4;
    static constexpr int    kPaneRows           = 8;
    static constexpr int    kRegisterRows       = 6;
    static constexpr int    kMarkerColumnDip    = 20;

    static void  MakeDense (DxuiListView * list);

    void     ConfigureWidgets ();
    void     LayoutWidgets    ();
    void     ApplySnapshot    ();
    void     SubmitCommandBox ();
    void     SubmitPokeBox    ();
    void     SubmitMemoryBox  ();
    void     AppendConsole    (const std::vector<std::string> & lines);
    void     RunCommand       (const std::string & line);
    void     ApplyKeyScheme   (DebuggerKeyScheme scheme);
    DebuggerKeyScheme  GetSavedKeyScheme () const;
    void     CycleKeyScheme   ();
    bool     RouteBoxKey      (const DxuiKeyEvent & ev, bool & handled);
    void     ConfigureDockSite  ();
    void     ConfigureCommandBar ();
    void     SetCommandBarMenus  ();
    void     RunCommandBarEntry  (int id);
    bool     IsCommandBarEntryEnabled (int id) const;
    bool     RouteCommandBarMouse (const DxuiMouseEvent & ev);
    static std::shared_ptr<DxuiCommand>  MakeMenuCommand (const std::wstring & label, bool checked, std::function<void()> chosen);
    bool     IsPaneShown        (const std::wstring & pane) const;
    std::wstring  GetPaneOfFocus () const;
    void     ShowDockToMenu     (const std::wstring & pane, POINT clientPx);
    bool     RouteDockKey       (const DxuiKeyEvent & ev);
    void     ApplySavedPlacement ();

    //  Floating panes (FR-040): each floats in a DxuiDockedWindow of its own,
    //  its controls moved there whole. The window's routing serves every
    //  window, filtered to the controls of the one the event came from.
    std::vector<IDxuiControl *>  GetPaneControls   (const std::wstring & pane) const;
    IDxuiControl *               GetPaneContent    (const std::wstring & pane) const;
    std::wstring                 GetPaneTitle      (const std::wstring & pane) const;
    std::wstring                 GetPaneOfControl  (const IDxuiControl * control) const;
    bool                         IsRoutable        (const IDxuiControl * control) const;
    IDxuiControl *               GetFocused        () const;
    void                         SetFocusedControl (IDxuiControl * control);
    HWND                         GetRoutingHwnd    () const;
    void                         RequestFloat      (const std::wstring & pane, POINT clientPx);
    void                         SyncFloats        ();
    void                         FloatControls     (const std::wstring & pane);
    void                         DockControls      (const std::wstring & pane);
    void                         SaveLayout        ();
    bool                         RouteFloatMouse   (const std::wstring & pane, const DxuiMouseEvent & ev);
    bool                         RouteFloatKey     (const std::wstring & pane, const DxuiKeyEvent & ev);
    void                         OnFloatDrag       (const std::wstring & pane, POINT screenPx, bool ended);

    static std::wstring                          GetMonitorKey (const RECT & rectPx);
    static std::vector<DxuiPaneLayout::Monitor>  GetMonitors   ();    void     ApplyMemoryWindows ();
    void     AddMemoryWindow    ();
    void     RemoveMemoryWindow ();
    bool     RouteMemoryMouse   (const DxuiMouseEvent & ev);
    bool     RouteSourceMouse   (const DxuiMouseEvent & ev);
    void     NoteViewFocus      (bool isSource);
    void     ApplySource        ();
    void     ApplyDiagnostics   ();
    DiagnosticsPane *  GetDiagnosticsPane (const std::wstring & pane) const;
    bool     ForwardToList    (DxuiListView * list, const DxuiMouseEvent & ev);
    bool     ClickGutter      (const DxuiMouseEvent & ev);
    void     ShowCode         (std::optional<Word> address);

    //  The debugger's colors, from the active theme: a breakpoint's red, the
    //  PC's arrow and row, the row another pane brought into view, a branch's
    //  destination, and the annotations' comment color.
    bool      IsDarkTheme          () const;
    uint32_t  GetBreakpointArgb    () const;
    uint32_t  GetPcMarkerArgb      () const;
    uint32_t  GetPcRowArgb         () const;
    uint32_t  GetNavigatedRowArgb  () const;
    uint32_t  GetTargetRowArgb     () const;
    uint32_t  GetAnnotationArgb    () const;
    uint32_t  GetChangedArgb       () const;
    void     OfferPress       (IDxuiControl * control, const DxuiMouseEvent & ev, bool & handled);

    std::vector<DxuiListView *>  GetLists          () const;
    std::vector<DxuiButton *>    GetMemoryButtons  () const;
    std::vector<MemoryPane *>    GetOpenMemoryPanes () const;
    MemoryPane *                 GetActiveMemoryPane () const;
    MemoryPane *                 GetFocusedMemoryPane () const;
    std::vector<IDxuiControl *>  GetPressTargets   () const;
    DxuiTextInput *              GetFocusedBox     () const;

    const CassoTheme     * m_theme         = nullptr;
    IDebuggerWindowHost  * m_host          = nullptr;
    DxuiDpiScaler          m_scaler;
    int                    m_widthDip      = 0;
    int                    m_heightDip     = 0;
    DxuiFocusManager       m_focusMgr;
    DebuggerKeyScheme      m_keyScheme     = DebuggerKeySchemes::kDefault;
    bool                   m_swallowSpace  = false;
    RECT                   m_openedRect    = {};
    bool                   m_placed        = false;
    int                    m_codeLinesSent = 0;
    std::optional<Word>    m_navigatedTo;

    std::shared_ptr<const DebuggerViewSnapshot>     m_snapshot;
    std::vector<std::string>                        m_console;

    DxuiToolbar                                                                    * m_commandBar         = nullptr;
    std::unique_ptr<DebuggerCommands>                                                m_commands;
    std::vector<std::shared_ptr<DxuiCommand>>                                        m_menuCommands;
    DxuiDockSite                                                                   * m_dockSite           = nullptr;
    HINSTANCE                                                                        m_hInstance          = nullptr;
    std::map<std::wstring, std::unique_ptr<DxuiDockedWindow>>                        m_floats;
    std::map<std::wstring, IDxuiControl *>                                           m_floatFocus;
    std::wstring                                                                     m_routingPane;
    bool                                                                             m_syncFloats         = false;
    std::unique_ptr<DebuggerPaneFrame>                                               m_sourceFrame;
    std::unique_ptr<DebuggerPaneFrame>                                               m_consoleFrame;
    std::array<bool, DebuggerViewState::kMaxMemoryWindows>                           m_memoryOpen         = {};
    DxuiListView                                                                   * m_codeList           = nullptr;
    DxuiListView                                                                   * m_registerList       = nullptr;
    DxuiListView                                                                   * m_breakpointList     = nullptr;
    DxuiListView                                                                   * m_watchList          = nullptr;
    DxuiListView                                                                   * m_stackList          = nullptr;
    DxuiListView                                                                   * m_callStackList      = nullptr;
    DxuiButton                                                                     * m_callStackButton    = nullptr;
    std::unique_ptr<CallStackPane>                                                   m_callStackPane;
    std::unique_ptr<DebuggerPaneFrame>                                               m_callStackFrame;
    DxuiListView                                                                   * m_traceList          = nullptr;
    std::unique_ptr<TracePane>                                                       m_tracePane;
    std::array<std::unique_ptr<MemoryPane>, DebuggerViewState::kMaxMemoryWindows>    m_memoryPanes;
    DxuiButton                                                                     * m_groupButton        = nullptr;
    DxuiButton                                                                     * m_addMemoryButton    = nullptr;
    DxuiButton                                                                     * m_removeMemoryButton = nullptr;
    MemoryPane                                                                     * m_activePane         = nullptr;
    std::string                                                                      m_machine;
    DxuiListView                                                                   * m_consoleList        = nullptr;
    DxuiTextInput                                                                  * m_commandBox         = nullptr;
    DxuiTextInput                                                                  * m_memoryBox          = nullptr;
    DxuiTextInput                                                                  * m_pokeBox            = nullptr;
    DxuiButton                                                                     * m_pokeButton         = nullptr;
    DxuiTextView                                                                   * m_sourceView         = nullptr;
    DxuiActionBanner                                                               * m_sourceBanner       = nullptr;
    std::unique_ptr<SourcePane>                                                      m_sourcePane;
    bool                                                                             m_sourceShown        = false;
    bool                                                                             m_sourceBannerShown  = false;
    std::wstring                                                                     m_sourceBannerKey;
    uint64_t                                                                         m_sourceClickMs      = 0;
    POINT                                                                            m_sourceClickAt      = {};
    std::vector<std::unique_ptr<DiagnosticsPane>>                                    m_diagPanes;
    std::set<std::string>                                                            m_diagOpen;
};
