#pragma once

#include "Debugger/DebugCommand.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CommandModeHelp
//
//  What HELP lists in each command mode other than AppleWin: that mode's own
//  commands as its user types them, and how the mode reaches Casso's engine
//  commands. AppleWin mode's help is the command table itself.
//
////////////////////////////////////////////////////////////////////////////////

class CommandModeHelp
{
public:
    struct Entry
    {
        const char  * word;
        const char  * syntax;
        const char  * description;
    };

    //  The mode's own commands, in the order HELP lists them; empty for
    //  AppleWin mode.
    static std::span<const Entry>  GetEntries     (CommandMode mode);

    //  The entry for one of the mode's words, matched without regard to case.
    static const Entry           * Find           (CommandMode mode, const std::string & word);

    //  The mode's name as its own documentation writes it: WinDbg, GSSquared.
    static const char            * GetTitle       (CommandMode mode);

    //  How the mode reaches the engine commands, as "after !, as in !BPL".
    static const char            * GetEngineRoute (CommandMode mode);
};
