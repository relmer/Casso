#pragma once

#include "CommandLineOptions.h"
#include "CommandLineParser.h"





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

    //  Everything, which is what `--help` asks for.
    static void                PrintUsage                (char prefix);

    //  One mode's section, for an invocation that got that far and no further.
    //  Subcommand::None prints the general section, for one that did not.
    static void                PrintUsage                (char prefix, CommandLineOptions::Subcommand scope);

    static void                PrintVersion              ();

    //  A first word that named no subcommand: what it was instead, then the
    //  general help.
    static void                PrintUnrecognizedArgument (const std::string & word, char prefix);

    //  An argument a recognized subcommand's grammar did not know: which one,
    //  then that mode's help.
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

    static void    PrintUsageHeader    (const char * sp, const char * lp);
    static void    PrintUsageGeneral   (const char * lp, const char * sp, const char * pad);
    static void    PrintUsageAssembler (const char * sp);
    static void    PrintUsageMerlin    (const char * sp, char prefix);
    static void    PrintUsageRun       (const char * lp, const char * sp, const char * pad);
};
