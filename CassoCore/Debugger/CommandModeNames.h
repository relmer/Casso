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

    static bool         TryParse        (const std::string & name, CommandMode & mode);
    static bool          TryParse        (const std::string & name, OutputFormat & format);
    static const char  * GetName         (CommandMode mode);
    static const char  * GetName         (OutputFormat format);
    static std::string   GetUpperName    (CommandMode mode);
    static std::string   GetUpperName    (OutputFormat format);
    static OutputFormat  GetOutputFormat (CommandMode mode);

    //  "applewin, monitor, and gssquared", for an error that lists them.
    //  The names in a sentence: "applewin, ..., and casso", or with each name
    //  in capitals as a command line types it.
    static std::string   GetList         (bool isUpper = false);
};
