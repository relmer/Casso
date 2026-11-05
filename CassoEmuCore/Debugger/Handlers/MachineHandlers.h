#pragma once

#include "Debugger/IDebugCommandHandler.h"

class DebugSession;
class IMachineControl;





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHandlers
//
//  RESET, PASTE, SCREEN, SCREENSHOT and EXIT, through the session's machine
//  control (IMachineControl). Without one, as in a batch run, each is not
//  available.
//
////////////////////////////////////////////////////////////////////////////////

class MachineHandlers : public IDebugCommandHandler
{
public:
    bool  TryExecute (DebugSession & session, const DebugCommand & command, Reply & reply) override;

    //  PASTE's text: the quotes around it removed, and \n or \r a return, \t
    //  a tab, \" a quote and \\ a backslash. False for an escape it doesn't
    //  take.
    static bool         TryUnescape (const std::string & text, std::wstring & outText);

    //  SCREENSHOT's file when none is given: casso-screenshot- and the local
    //  date and time, in the session's directory.
    static std::string  GetDefaultScreenshotName (const SYSTEMTIME & when);

private:
    static IMachineControl *  GetControl (DebugSession & session, const DebugCommand & command, Reply & reply);

    static void  Reset      (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  Paste      (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  Screen     (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  Screenshot (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  Exit       (DebugSession & session, const DebugCommand & command, Reply & reply);
};
