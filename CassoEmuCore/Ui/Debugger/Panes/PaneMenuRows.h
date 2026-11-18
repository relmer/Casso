#pragma once

#include "Ui/Debugger/Panes/IDebuggerPane.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PaneMenuRows
//
//  Context menu rows that several panes add for a code address.
//
////////////////////////////////////////////////////////////////////////////////

class PaneMenuRows
{
public:
    //  Add bookmark for an address, or Remove bookmark when it has one.
    static void  AddBookmark (const LatestSnapshot & latest, IPaneCommands & commands, Word address, PaneMenuItems & items);
};
