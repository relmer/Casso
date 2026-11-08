#pragma once

#include "Debugger/DebugCommand.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CommandModeNames
//
//  The one list of command modes and output formats by name, so `MODE`,
//  `OUTPUT`, `--mode`, `--output`, the channel and JSON all accept and write
//  the same words. Names are matched without regard to case and written in
//  lowercase; GetUpperName is the form a text reply shows.
//
////////////////////////////////////////////////////////////////////////////////

class CommandModeNames
{
public:
    //  One row of the name table, defined beside it.
    struct Entry;

    static bool          TryParse        (const std::string & name, CommandMode & mode);
    static bool          TryParse        (const std::string & name, OutputFormat & format);
    static const char  * GetName         (CommandMode mode);
    static const char  * GetName         (OutputFormat format);
    static std::string   GetUpperName    (CommandMode mode);
    static std::string   GetUpperName    (OutputFormat format);
    static OutputFormat  GetOutputFormat (CommandMode mode);

    //  "applewin, monitor, gssquared, windbg, and casso", for an error that
    //  lists them.
    static std::string   GetList         ();

    //  The same list with each name as a text reply shows it, the conjunction
    //  left alone: "APPLEWIN, MONITOR, GSSQUARED, WINDBG, and CASSO".
    static std::string   GetUpperList    ();
};
