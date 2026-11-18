#pragma once

#include "Ui/Debugger/DebuggerViewSnapshot.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SourceMacroPlaces
//
//  Where the PC is inside macro expansions: the invocation outside every
//  macro, each level one macro further in, and the body line itself, -1.
//
////////////////////////////////////////////////////////////////////////////////

class SourceMacroPlaces
{
public:
    //  Whether a macro's body can be offered: not when it is in the file that
    //  could not be found.
    static bool  CanShowBody (bool hasText, int depth, int bodyFileId, int fileId);

    //  The level the banner's button goes to: from outside every macro to the
    //  body line, and from a body back out one invocation at a time.
    static int   GetNextMacroLevel (int level, int deepest);

    //  The level a request resolves to, -1 being the deepest.
    static int   GetMacroLevel (const DebuggerViewSnapshot::SourceState & state, int level);

    //  The file and line at a level, -1 being the body line.
    static std::pair<int, int>  GetPlace (const DebuggerViewSnapshot::SourceState & state, int level);
};
