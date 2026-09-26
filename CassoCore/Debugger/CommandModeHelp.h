#pragma once

#include "Debugger/DebugCommand.h"
#include "Debugger/CassoCommandReference.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CommandModeHelp
//
//  What HELP lists in each command mode, and which of Casso's commands each
//  mode reaches. Help lists only what can be typed in the mode, written as it
//  is typed there: the mode's own commands, then the Casso commands it
//  reaches. Every list is grouped by category and alphabetical within each,
//  one line per command, syntax then description.
//
//  A MODE REACHES A CASSO COMMAND through its marker -- `/` in Monitor, `!`
//  in WinDbg, a bare name in the others -- when the command is one of Casso's
//  engine commands, or when the mode does not already use its name for
//  something else. AppleWin and Casso modes reach every command, since the
//  command table is theirs. The parsers ask the same question, so nothing
//  help lists fails to run. Help's Casso section leaves out a command the
//  mode has a form of its own for: the mode's form is listed instead.
//
////////////////////////////////////////////////////////////////////////////////

class CommandModeHelp
{
public:
    struct Entry
    {
        const char    * word;          // the command word, or empty for a form such as "addr"
        HelpCategory    category;
        const char    * syntax;
        const char    * description;
        const char    * casso;         // the Casso commands this stands for, space-separated; empty for none
    };

    //  The mode's own commands; empty for AppleWin and Casso modes, whose
    //  commands are Casso's own.
    static std::span<const Entry>  GetEntries     (CommandMode mode);

    //  The entry for one of the mode's words, matched without regard to case.
    static const Entry           * Find           (CommandMode mode, const std::string & word);

    //  The mode's title as its own documentation writes it: WinDbg, GSSquared.
    static const char            * GetTitle       (CommandMode mode);

    //  What goes ahead of a Casso command's name in the mode: "/", "!" or "".
    static const char            * GetMarker      (CommandMode mode);

    //  Whether the mode runs a Casso command -- a name or alias from the
    //  AppleWin command table -- typed after its marker.
    static bool                    IsCassoCommandReachable (CommandMode mode, const std::string & name);

    //  HELP with no word: the lines it prints.
    static std::vector<std::string>  BuildHelp    (CommandMode mode);

    //  HELP word: the line describing it, or, for a Casso command the mode
    //  cannot run, which modes run it. False for a word that is no command.
    static bool                    TryDescribe    (CommandMode mode, const std::string & word, std::string & line);

private:
    struct Row
    {
        HelpCategory  category;
        std::string   syntax;
        std::string   description;
    };

    static bool         IsCoveredByMode   (CommandMode mode, const std::string & cassoName);
    static bool         IsWordOfMode      (CommandMode mode, const std::string & name);
    static bool         IsHexWord         (const std::string & name);
    static std::string  GetShownSyntax    (CommandMode mode, const CassoCommandReference::Entry & entry);
    static std::string  GetModesThatRun   (const std::string & name);
    static void         AppendSection     (const std::string & heading, std::vector<Row> rows, size_t width, std::vector<std::string> & lines);
    static std::string  ToUpper           (const std::string & text);
};
