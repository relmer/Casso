#pragma once

#include "Pch.h"

class HeldHostInputs;
class IFileSystem;
class IHostDialogs;
class WindowPlacementProfile;
struct CassoTheme;





////////////////////////////////////////////////////////////////////////////////
//
//  IDebuggerHost
//
//  What the debugger asks of the emulator that holds it: the main window and
//  its look, the CPU thread's command queue, the picture, the files, dialogs
//  and saved preferences, and the host input the debugger holds back behind
//  live. The emulator's shell implements it; a test implements it with a
//  window of nothing and a notebook.
//
////////////////////////////////////////////////////////////////////////////////

class IDebuggerHost
{
public:

    virtual ~IDebuggerHost() = default;

    //  The emulator's window: its handle, instance and theme, which the
    //  debugger window and its questions are owned by and drawn in; the
    //  caption prefix `--title` gave; and the window's icon, put on another.
    virtual HWND                    GetMainWindow       () const                 = 0;
    virtual HINSTANCE               GetInstance         () const                 = 0;
    virtual const CassoTheme      & GetChromeTheme      () const                 = 0;
    virtual const std::wstring    & GetTitlePrefix      () const                 = 0;
    virtual void                    ApplyAppIcon        (HWND window)            = 0;

    //  Draws the emulator's window now, and its caption again after the
    //  replay note in it changed. UI thread.
    virtual void                    PresentUiFrame      ()                       = 0;
    virtual void                    UpdateWindowTitle   ()                       = 0;

    //  A timer on the emulator's window, whose tick the window hands back to
    //  the debugger. UI thread.
    virtual HRESULT                 SetUiTimer          (UINT_PTR id, UINT ms)   = 0;
    virtual HRESULT                 KillUiTimer         (UINT_PTR id)            = 0;

    //  Queues a command for the CPU thread, as a menu item would.
    virtual void                    PostCommand         (WORD id, const std::string & payload) = 0;

    //  The picture: drawn and handed to the window now, drawn again when the
    //  beam mark changed on a stopped machine, and its buffers made for a
    //  test that never opened the window. CPU thread.
    virtual void                    DrawFrame           ()                       = 0;
    virtual void                    RedrawStoppedFrame  ()                       = 0;
    virtual void                    PrepareFramebuffers ()                       = 0;

    //  The UI thread's file system, the operating system's pickers, and the
    //  saved window placements.
    virtual IFileSystem           & GetUiFileSystem     ()                       = 0;
    virtual IHostDialogs          & GetHostDialogs      ()                       = 0;
    virtual WindowPlacementProfile  GetWindowPlacements ()                       = 0;

    //  Saves the preferences, the debugger's settings among them, after a
    //  short delay; and a machine switch's position, which the machine's own
    //  preferences keep.
    virtual void                    SaveSettings        ()                       = 0;
    virtual void                    PersistSwitchState  (const char * key, bool value) = 0;

    //  Host input held behind live: the keys and buttons held down, and a
    //  paste cut back to where it stood before the held part.
    virtual HeldHostInputs        & GetHeldHostInputs   ()                       = 0;
    virtual void                    TruncatePaste       (size_t length)          = 0;
};
