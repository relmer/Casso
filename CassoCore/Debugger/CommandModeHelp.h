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
    static const char            * GetNumberNote  (CommandMode mode);

    //  Whether the mode runs a Casso command -- a name or alias from the
    //  AppleWin command table -- typed after its marker.
    static bool                    IsCassoCommandReachable (CommandMode mode, const std::string & name);

    //  A Casso command as the mode types it: the mode's own word for it,
    //  or its marker and the name.
    static std::string             GetTypedName   (CommandMode mode, const std::string & cassoName);

    //  HELP ALL: every command the mode can type, the lines it prints.
    static std::vector<std::string>  BuildHelp    (CommandMode mode);

    //  HELP with no word: the sections and how to ask for each.
    static std::vector<std::string>  BuildSectionIndex (CommandMode mode);

    //  The section a word asks for -- the first word of its heading, such as
    //  "breakpoints" or "memory" -- matched without regard to case.
    static bool                    TryFindSection (const std::string & word, HelpCategory & category);

    //  HELP section: that section's commands alone, in the HELP ALL layout.
    static std::vector<std::string>  BuildSection (CommandMode mode, HelpCategory category);

    //  HELP text: every command whose syntax or description contains the
    //  text, ignoring case, with * and ? as wildcards, or matches /regex/.
    //  A search with no match says so. False, with the reason, for a regular
    //  expression that does not compile.
    static bool                    TrySearch      (CommandMode mode, const std::string & text, std::vector<std::string> & lines, std::string & error);

    //  HELP with a word or text after it: the section, the command, or the
    //  search, tried in that order. False, with the reason, for a
    //  regular expression that does not compile.
    static bool                    TryBuildWordHelp (CommandMode mode, const std::string & text, std::vector<std::string> & lines, std::string & error);

    //  docs/Debugger-Commands.md: every mode's HELP ALL, as Markdown.
    static std::string             BuildReference ();

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

    static bool         TryDescribeOtherModesWord (CommandMode mode, const std::string & word, std::string & line);
    static bool         IsCoveredByMode   (CommandMode mode, const std::string & cassoName);
    static std::string  GetCoveringForms  (CommandMode mode, const std::string & cassoName);
    static bool         IsWordOfMode      (CommandMode mode, const std::string & name);
    static bool         IsHexWord         (const std::string & name);
    static std::string  GetShownSyntax    (CommandMode mode, const CassoCommandReference::Entry & entry);
    static std::string  GetModesThatRun   (const std::string & name);
    static std::vector<std::string>  BuildListing (CommandMode mode, const std::function<bool (const Row &)> & keep);
    static const char * GetSectionWord    (HelpCategory category);
    static void         AppendSection     (const std::string & heading, std::vector<Row> rows, size_t width, std::vector<std::string> & lines);
    static std::string  ToUpper           (const std::string & text);
};
