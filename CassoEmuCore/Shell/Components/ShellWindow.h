#pragma once

#include "Pch.h"




class EmulatorShell;
class WindowManager;





////////////////////////////////////////////////////////////////////////////////
//
//  ShellWindow
//
//  The main window's placement and size: the saved per-monitor placement,
//  the startup show state, the first size reconcile, the client size a
//  framebuffer or the drive row asks for, the caption's title and the app
//  icon, the accelerators, OLE and the drag-drop target, and the message
//  filter intents arrive through.
//
//  The window's message handlers stay on the shell, which routes them, and
//  read and write the placement flags directly, so the shell is a friend.
//
////////////////////////////////////////////////////////////////////////////////

class ShellWindow
{
    friend class EmulatorShell;

public:
    explicit ShellWindow (EmulatorShell & shell);
    ~ShellWindow();

    // The saved per-monitor placement.
    WindowManager &  GetWindowManager () { return *m_windowManager; }

    // The show state Windows handed wWinMain. Set before Initialize; the
    // first ShowWindow honors it when the launcher asked for something
    // particular, and falls back to the saved placement when it did not.
    void  SetStartupShowCommand (int nCmdShow) { m_startShowCmd = nCmdShow; }

    // Text put in front of the window caption, so one of several open windows
    // can be told from the others at a glance. Undocumented; set from --title
    // and read by UpdateWindowTitle. Set before the window exists, so it does
    // not refresh the caption itself.
    void SetWindowTitlePrefix (const wstring & prefix) { m_titlePrefix = prefix; }

    void UpdateWindowTitle();

    void    ReconcileInitialClientSize ();

    void    InstallDragDropTarget           ();

    // The emulator viewport (CRT output area) in *screen* pixels: the middle
    // rect from ComputeViewportRect at the current back-buffer size, mapped
    // through the main window's client origin. The Settings live-preview
    // compositor (#8) intersects this with the (composited) sheet window to
    // punch a see-through hole revealing the running emulator behind the sheet.
    RECT    GetEmulatorContentScreenRect  ();

    SIZE    GetClientSizeForFramebufferPx (int framebufferWidthDp, int framebufferHeightDp);

    static bool  TryGetCursorMonitorWorkArea (RECT & outWork, HMONITOR & outMonitor);

    static void  CenterInWorkArea (
        const RECT & work,
        int          windowW,
        int          windowH,
        LONG       & outX,
        LONG       & outY);

    static HRESULT  LoadIconAsPremulBgra (
        HINSTANCE               hInstance,
        int                     iconResourceId,
        int                     sizePx,
        std::vector<uint32_t> & outPixels,
        int                   & outW,
        int                   & outH);

    // The drive row's width, which the window is never narrower than.
    int     GetDriveRowWidthPx ();

    // Attach the Casso app icon (IDI_CASSO) to a child DxuiWindow so it shows the
    // Casso motif in Alt-Tab / the taskbar. The borderless Dxui panels do not
    // inherit the WNDCLASS icon, and Alt-Tab reads WM_GETICON, so the big+small
    // icons are handed over explicitly (as the main window does).
    void    ApplyAppIconToWindow (HWND target);

    // Opens the integrity-level hole a stated intent arrives through.
    //
    // SEPARATE FROM InstallDragDropTarget, WHICH ALSO INSTALLS IT. That one is
    // called only where OLE initialization succeeded, so on a machine where it
    // did not, every intent from a normal-integrity CassoCli to an elevated
    // Casso would be dropped by the system without a word. Installing it here
    // as well costs a call and removes the dependency.
    void    InstallIntentMessageFilter ();



private:
    EmulatorShell  & m_shell;

    HACCEL     m_accelTable            = nullptr;

    bool       m_initialSizeReconciled = false;

    bool       m_startMaximized        = false;

    // The drag-drop target registers a single IDropTarget on the main HWND,
    // for the drives and the recorder.
    DxuiDragDropTarget     m_dragDropTarget;

    // Set true once OleInitialize has succeeded on the UI thread so
    // shutdown can pair the call with OleUninitialize. RegisterDragDrop
    // requires OLE (STA) on the registering thread.
    bool                                 m_fOleInitialized = false;

    // SW_SHOWDEFAULT means "the launcher expressed no preference", which is
    // what a normal double-click amounts to.
    int                                  m_startShowCmd    = SW_SHOWDEFAULT;

    // --title (undocumented)
    wstring                              m_titlePrefix;

    // Whether the window is maximized as far as the last SAVED placement
    // is concerned. A change here is a user maximizing or restoring, which
    // is worth persisting even though it never enters the OS drag loop.
    // Set when the user issues a maximize / restore and cleared by the
    // OnSize that carries it out, which is where the new placement is
    // actually readable.
    bool                                 m_userStateChange = false;

    // The per-monitor placement persistence, backed by GlobalUserPrefs JSON.
    std::unique_ptr<WindowManager>            m_windowManager;
};
