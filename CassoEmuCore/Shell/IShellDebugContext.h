#pragma once

class CpuManagerRunDriver;
class DebuggerController;
class DebugSession;
class IDxuiTheme;
struct DebuggerAction;





////////////////////////////////////////////////////////////////////////////////
//
//  IShellDebugContext
//
//  The shell debugger's parts' access to the debugger they are parts of: the
//  controller, session and run driver attached now, each null while none is;
//  the console, the window's action queue and the user's pause; and the
//  window and theme a question to the user is put in.
//
////////////////////////////////////////////////////////////////////////////////

class IShellDebugContext
{
public:
    virtual ~IShellDebugContext() = default;

    virtual DebuggerController   * GetDebugController      () = 0;
    virtual DebugSession         * GetDebugSession         () = 0;
    virtual CpuManagerRunDriver  * GetDebugRunDriver       () = 0;

    //  Lines added to the window's console, and the views built again.
    virtual void                   AddConsoleLines         (const std::vector<std::string> & lines) = 0;
    virtual void                   RunDebuggerAction       (const DebuggerAction & action) = 0;
    virtual void                   NotifyDebugPauseChanged (bool paused) = 0;

    //  The emulator's window and theme, or the debugger window's when it is
    //  open, and with isActiveOnly, only while it is the active window.
    virtual void                   GetQuestionOwner        (bool isActiveOnly, HWND & owner, const IDxuiTheme * & theme) = 0;
};
