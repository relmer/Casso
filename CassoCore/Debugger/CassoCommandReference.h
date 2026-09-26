#pragma once

#include "Debugger/DebugCommand.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HelpCategory
//
//  The groups every command mode's help lists its commands under, in the
//  order it lists them.
//
////////////////////////////////////////////////////////////////////////////////

enum class HelpCategory
{
    RunningAndStepping,
    Breakpoints,
    RegistersAndFlags,
    Memory,
    DisassemblyAndData,
    SymbolsAndSource,
    Disks,
    DisplayAndPanels,
    SessionAndSettings,
};





////////////////////////////////////////////////////////////////////////////////
//
//  CassoCommandReference
//
//  What help says about each of Casso's own commands -- every command the
//  AppleWin command table runs, AppleWin's names and Casso's engine commands
//  alike: its category, its syntax and what it does. One entry per command;
//  a name that is another's alias (AppleWinCommand::aliasOf) has no entry of
//  its own and is shown on its command's line. Names that are only accepted
//  and reported -- not available, like BENCHMARK and SOURCE1, or of no effect
//  in a window that shows every pane at once, like CODE and WIN -- have none
//  either, since help lists only what runs.
//
////////////////////////////////////////////////////////////////////////////////

class CassoCommandReference
{
public:
    struct Entry
    {
        const char    * name;          // as the AppleWin command table has it
        HelpCategory    category;
        const char    * syntax;        // the name and its arguments, as typed
        const char    * description;   // one line, sentence case, no final period
    };

    static std::span<const Entry>  GetAll  ();

    //  The entry for a command, found by its name or any alias of it, without
    //  regard to case; null for a name with none.
    static const Entry           * Find    (const std::string & name);

    //  The category's heading as help prints it: "Running and stepping".
    static const char            * GetCategoryTitle (HelpCategory category);
};
