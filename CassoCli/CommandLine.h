#pragma once

#include "CommandLineOptions.h"
#include "CommandLineParser.h"
#include "Dialect.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLine
//
//  The command line itself: what an argv means, and what the tool says about
//  its own arguments.
//
//  The grammar lives in CassoCore/CommandLineParser, where the UnitTest project
//  can reach it. What is here is the executable's side of the same subject --
//  the host filesystem probe the parser needs to tell a source file from a
//  mistyped subcommand, the usage text, and the two messages a refused
//  invocation earns.
//
//  IT DOES NOTHING WITH WHAT IT PARSED. Assembling is AssemblerMode's, running
//  is RunMode's, and writing files is ArtifactWriter's; a class that parsed the
//  arguments AND acted on them would be the reason every one of those was
//  unreachable from a test.
//
//  The usage text is written one logical line per item and folded to the
//  reader's terminal at print time -- see UsageText for why the continuation
//  column is found rather than authored.
//
////////////////////////////////////////////////////////////////////////////////

class CommandLine
{
public:
    static CommandLineOptions  Parse                     (int argc, char * argv[]);

    //  The page the request asked for, written with the prefix it was typed
    //  with. Both live on the options, so the caller hands over the whole
    //  parse rather than picking two fields out of it and deciding again.
    static void                PrintUsage                (const CommandLineOptions & options);
    static void                PrintVersion              ();

    //  What the tool is called, which build this is, and who holds the
    //  copyright. Public because the disk help is assembled in the library,
    //  which does not know the build's version, so that page is handed this
    //  at print time rather than building one of its own.
    static std::string         BuildBanner               ();

    //  A first word that named no subcommand: the full usage, then what the
    //  word was instead -- LAST, so it is the line left on screen.
    static void                PrintUnrecognizedArgument (const std::string & word, char prefix);

    //  An argument a recognized subcommand's grammar did not know: the full
    //  usage, then which argument, for the same reason.
    static void                PrintUnrecognizedFlag     (const std::string & flag, CommandLineOptions::Subcommand subcommand, char prefix);

    //  A CPU flag the active dialect does not take. The sentence is composed
    //  in core, where the dialect's own data is; this prints it and says what
    //  the process is about to return.
    static int                 PrintCpuFlagRefusal       (const std::string & refusal);

private:
    //  How wide the reader's terminal is, or 80 when there is no terminal.
    static size_t  UsageWidth          ();

    //  One logical line of usage, folded to that width. EVERY line of help goes
    //  through here, so none of them is hand-wrapped to a width the reader may
    //  not have.
    static void    PrintUsageLine      (const std::string & line);

    //  A run of usage composed elsewhere -- core builds the dialect flag lines
    //  -- folded row by row.
    static void    PrintUsageBlock     (const std::string & block);

    //  A top-level heading, underlined to its own width.
    static void    PrintSectionHeading (const std::string & name);


    //  One page per grammar. Each opens with the banner and its own usage
    //  line, generates its flags from that dialect's table, and closes with
    //  the exit codes that mode actually spends.
    static void    PrintAssemblePage   (char prefix);
    static void    PrintMerlinPage     (char prefix);
    static void    PrintRunPage        (char prefix);

    static void    PrintPageBanner     (CommandLineOptions::Subcommand mode);
    static void    PrintDialectFlags   (DialectId dialect, char prefix);
    static void    PrintExitCodes      (const char * codes);
};
