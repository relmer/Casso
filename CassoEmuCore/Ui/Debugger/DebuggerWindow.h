#pragma once

#include "Window/DxuiWindow.h"
#include "Core/DxuiFocusManager.h"
#include "Widgets/DxuiButton.h"
#include "Widgets/DxuiLabel.h"
#include "Widgets/DxuiListView.h"
#include "Widgets/DxuiTextInput.h"
#include "Seams/IHostDialogs.h"
#include "Ui/Debugger/DebuggerViewState.h"

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
    virtual void  SetDebuggerMemoryAddress (Word address)                  = 0;

    //  The newest snapshot, if one arrived since the last call, and every
    //  console line written since then.
    virtual bool  TakeDebuggerUpdate      (std::shared_ptr<const DebuggerViewSnapshot> & snapshot,
                                           std::vector<std::string>                     & consoleLines) = 0;

    virtual void  OnDebuggerWindowClosed  ()                               = 0;

    //  The file pickers the R and W prompt opens.
    virtual IHostDialogs &  GetHostDialogs () noexcept                     = 0;
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
    LPCWSTR  GetCursorForPoint (POINT clientPx) const override;

private:
    static constexpr int  kPreferredWidthDip  = 1100;
    static constexpr int  kPreferredHeightDip = 760;
    static constexpr int  kMinWidthDip        = 760;
    static constexpr int  kMinHeightDip       = 520;
    static constexpr int  kConsoleLineLimit   = 2000;

    void     ConfigureWidgets ();
    void     LayoutWidgets    ();
    void     ApplySnapshot    ();
    void     SubmitCommandBox ();
    void     SubmitPokeBox    ();
    void     SubmitMemoryBox  ();
    void     AppendConsole    (const std::vector<std::string> & lines);
    bool     ForwardToList    (DxuiListView * list, const DxuiMouseEvent & ev);
    void     OfferPress       (IDxuiControl * control, const DxuiMouseEvent & ev, bool & handled);

    std::vector<DxuiListView *>  GetLists () const;

    const CassoTheme     * m_theme     = nullptr;
    IDebuggerWindowHost  * m_host      = nullptr;
    DxuiDpiScaler          m_scaler;
    int                    m_widthDip  = 0;
    int                    m_heightDip = 0;
    DxuiFocusManager       m_focusMgr;

    std::shared_ptr<const DebuggerViewSnapshot>     m_snapshot;
    std::vector<std::string>                        m_console;

    DxuiButton                                    * m_stepButton        = nullptr;
    DxuiButton                                    * m_stepOverButton    = nullptr;
    DxuiButton                                    * m_runButton         = nullptr;
    DxuiButton                                    * m_runToCursorButton = nullptr;
    DxuiButton                                    * m_pauseButton       = nullptr;
    DxuiButton                                    * m_followPcButton    = nullptr;
    DxuiLabel                                     * m_flagsLabel        = nullptr;
    DxuiListView                                  * m_codeList          = nullptr;
    DxuiListView                                  * m_registerList      = nullptr;
    DxuiListView                                  * m_breakpointList    = nullptr;
    DxuiListView                                  * m_watchList         = nullptr;
    DxuiListView                                  * m_stackList         = nullptr;
    DxuiListView                                  * m_memoryList        = nullptr;
    DxuiListView                                  * m_consoleList       = nullptr;
    DxuiTextInput                                 * m_commandBox        = nullptr;
    DxuiTextInput                                 * m_memoryBox         = nullptr;
    DxuiTextInput                                 * m_pokeBox           = nullptr;
    DxuiButton                                    * m_pokeButton        = nullptr;
};
