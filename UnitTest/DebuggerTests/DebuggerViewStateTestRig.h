#pragma once

#include "ControllerRig.h"
#include "Ui/Debugger/DebuggerViewSnapshot.h"





namespace DebuggerViewStateTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MachineRig
    //
    //  The debugger as the window sees it: a controller over a real machine, so
    //  the panes read real memory, and a transport so a channel client can share
    //  the session with the window.
    //
    ////////////////////////////////////////////////////////////////////////////////

    //  The code pane puts the PC in the middle, so a row is found by its
    //  address rather than by counting from the top.
    inline const DebuggerViewSnapshot::CodeLine & LineAt (const DebuggerViewSnapshot & snapshot, Word address)
    {
        static DebuggerViewSnapshot::CodeLine  none;



        for (const DebuggerViewSnapshot::CodeLine & line : snapshot.code)
        {
            if (line.address == address)
            {
                return line;
            }
        }

        Microsoft::VisualStudio::CppUnitTestFramework::Assert::Fail (L"the code pane does not show that address");
        return none;
    }





    using MachineRig = ControllerRig;
}
