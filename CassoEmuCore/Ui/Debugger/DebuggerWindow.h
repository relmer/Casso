#pragma once

#include "Window/DxuiWindow.h"
#include "Core/DxuiFocusManager.h"
#include "Widgets/DxuiButton.h"
#include "Widgets/DxuiDockSite.h"
#include "Widgets/DxuiLabel.h"
#include "Widgets/DxuiListView.h"
#include "Widgets/DxuiTextInput.h"
#include "Seams/IHostDialogs.h"
#include "Ui/Debugger/DebuggerKeySchemes.h"
#include "Ui/Debugger/DebuggerViewState.h"
#include "Ui/Debugger/Panes/DebuggerPaneFrame.h"
#include "Ui/Debugger/Panes/MemoryPane.h"
#include "Ui/Debugger/Panes/SourcePane.h"

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
    virtual void  SetDebuggerCodeAddress  (std::optional<Word> address)    = 0;
    virtual void  SetDebuggerMemoryWindow (int id, std::optional<Word> address) = 0;

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
//  THE CURRENT LINE AND BREAKPOINTS ARE MARKED WITH GLYPHS. A list row cannot
//  be colored, so the code pane's first column carries a triangle for the PC and
//  a bullet for a breakpoint.
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

protected:
    void     OnCreate        () override;
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
    bool     IsPaneShown        (const std::wstring & pane) const;
    std::wstring  GetPaneOfFocus () const;
    void     ShowDockToMenu     (const std::wstring & pane, POINT clientPx);
    bool     RouteDockKey       (const DxuiKeyEvent & ev);
    void     ApplyMemoryWindows ();
    void     AddMemoryWindow    ();
    void     RemoveMemoryWindow ();
    bool     RouteMemoryMouse   (const DxuiMouseEvent & ev);
    bool     RouteSourceMouse   (const DxuiMouseEvent & ev);
    void     NoteViewFocus      (bool isSource);
    void     ApplySource        ();
    bool     ForwardToList    (DxuiListView * list, const DxuiMouseEvent & ev);
    void     OfferPress       (IDxuiControl * control, const DxuiMouseEvent & ev, bool & handled);

    std::vector<DxuiListView *>  GetLists          () const;
    std::vector<DxuiButton *>    GetToolbarButtons () const;
    std::vector<DxuiButton *>    GetMemoryButtons  () const;
    std::vector<MemoryPane *>    GetOpenMemoryPanes () const;
    MemoryPane *                 GetActiveMemoryPane () const;
    MemoryPane *                 GetFocusedMemoryPane () const;
    std::vector<IDxuiControl *>  GetPressTargets   () const;
    DxuiTextInput *              GetFocusedBox     () const;

    const CassoTheme     * m_theme        = nullptr;
    IDebuggerWindowHost  * m_host         = nullptr;
    DxuiDpiScaler          m_scaler;
    int                    m_widthDip     = 0;
    int                    m_heightDip    = 0;
    DxuiFocusManager       m_focusMgr;
    DebuggerKeyScheme      m_keyScheme    = DebuggerKeySchemes::kDefault;
    bool                   m_swallowSpace = false;

    std::shared_ptr<const DebuggerViewSnapshot>     m_snapshot;
    std::vector<std::string>                        m_console;

    DxuiDockSite                                                                   * m_dockSite           = nullptr;
    std::unique_ptr<DebuggerPaneFrame>                                               m_sourceFrame;
    std::unique_ptr<DebuggerPaneFrame>                                               m_consoleFrame;
    std::array<bool, DebuggerViewState::kMaxMemoryWindows>                           m_memoryOpen         = {};
    DxuiButton                                                                     * m_stepButton         = nullptr;
    DxuiButton                                                                     * m_stepOverButton     = nullptr;
    DxuiButton                                                                     * m_stepOutButton      = nullptr;
    DxuiButton                                                                     * m_runButton          = nullptr;
    DxuiButton                                                                     * m_runToCursorButton  = nullptr;
    DxuiButton                                                                     * m_pauseButton        = nullptr;
    DxuiButton                                                                     * m_followPcButton     = nullptr;
    DxuiButton                                                                     * m_keysButton         = nullptr;
    DxuiLabel                                                                      * m_flagsLabel         = nullptr;
    DxuiListView                                                                   * m_codeList           = nullptr;
    DxuiListView                                                                   * m_registerList       = nullptr;
    DxuiListView                                                                   * m_breakpointList     = nullptr;
    DxuiListView                                                                   * m_watchList          = nullptr;
    DxuiListView                                                                   * m_stackList          = nullptr;
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
};
