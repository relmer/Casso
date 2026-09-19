#pragma once

#include "Debugger/DebugCommand.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinCommandFamily / CommandAvailability
//
////////////////////////////////////////////////////////////////////////////////

enum class AppleWinCommandFamily
{
    Assembler,
    Cpu,
    Bookmarks,
    Breakpoints,
    Config,
    Cycles,
    Data,
    Disk,
    Flags,
    Help,
    Memory,
    Output,
    Symbols,
    Watch,
    ZeroPage,
    Startup,
    Video,
    Engine,
    Cursor,
    Window,
    MiniMemory,
    Views,
    Appearance,
    Unsupported,
};

enum class CommandAvailability
{
    Headless,       // works in batch, the channel and the window
    WindowOnly,     // only affects the debugger window's display
    NotAvailable,   // accepted and reported with a reason
};





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinCommand
//
//  One name in AppleWin mode. aliasOf is the name this one stands for, or
//  null. reason is the second line of the not-available error.
//
////////////////////////////////////////////////////////////////////////////////

struct AppleWinCommand
{
    const char             * name;
    DebugVerb                verb;
    AppleWinCommandFamily    family;
    CommandAvailability      availability;
    const char             * aliasOf;
    const char             * reason;
};





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinCommandTable
//
////////////////////////////////////////////////////////////////////////////////

class AppleWinCommandTable
{
public:
    //  Null when the name is not an AppleWin-mode command. Case is ignored.
    static const AppleWinCommand *        Find   (const std::string & name);
    static std::span<const AppleWinCommand> GetAll ();

    //  Whether the name is one of Casso's engine commands, the Engine family:
    //  the commands every mode reaches through its own marker. A parser hands
    //  such a line, marker stripped, to AppleWinParser, so a command added to
    //  the family is reachable in every mode with no parser change.
    static bool                           IsEngineCommand (const std::string & name);
};
