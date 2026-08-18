#include "Pch.h"

#include "CommandLine.h"
#include "DiskCommand.h"
#include "CassoCli.h"





////////////////////////////////////////////////////////////////////////////////
//
//  main
//
//  Parses the command line and dispatches to a subcommand.
//
//  No subcommand is a usage ERROR and exits 1, while an explicit --help or
//  `help` is the user asking and exits 0. Both print the same text, but a
//  script that invokes the tool wrongly must fail, and one that asks for help
//  must not.
//
//  Dispatch is a flat chain over the parsed subcommand, so the parser owns all
//  the grammar and this function owns none of it.
//
//  The final arm catches a subcommand the parser recognizes but this dispatch
//  does not -- a genuinely unreachable state kept as a defined behavior rather
//  than a fallthrough returning an uninitialized code.
//
//  A COMMAND LINE THE PARSER REFUSED EXITS 2 EVEN THOUGH IT PRINTS USAGE. A bad
//  --cpu target ends parsing and leaves showHelp set so the user is shown the
//  grammar, and that arrangement used to report success: the tool said "unknown
//  --cpu target", printed the whole help, and told the calling script it had
//  worked. Printing usage is how the tool answers the mistake, not evidence
//  there was none.
//
//  The exit code is the only thing this function produces; every subcommand
//  reports through it, which is what makes the tool usable from a build script.
//
////////////////////////////////////////////////////////////////////////////////

int main (int argc, char * argv[])
{
    CommandLineOptions  options  = ParseCommandLine (argc, argv);
    int                 exitCode = 0;
    bool                refused  = options.parseVerdict == CommandLineOptions::ParseVerdict::Refused;



    // No subcommand is a usage ERROR (exit 1); an explicit --help or `help`
    // is the user asking, and succeeds. A refusal outranks both: the usage
    // text is being printed AT the user, not FOR them.
    if (options.showHelp || options.subcommand == CommandLineOptions::Subcommand::None
                        || options.subcommand == CommandLineOptions::Subcommand::Help)
    {
        PrintUsage (options);
        exitCode = refused ? 2 : (options.showHelp ? 0 : 1);
    }
    else if (options.showVersion || options.subcommand == CommandLineOptions::Subcommand::Version)
    {
        PrintVersion();
    }
    else if (options.subcommand == CommandLineOptions::Subcommand::Run)
    {
        exitCode = DoRun (options);
    }
    else if (options.subcommand == CommandLineOptions::Subcommand::Disk)
    {
        exitCode = DiskCommand::Run (options);
    }
    else if (options.subcommand == CommandLineOptions::Subcommand::As65)
    {
        exitCode = DoAs65 (options);
    }
    else
    {
        // A subcommand the parser knows but this dispatch does not.
        PrintUsage (options);
    }

    return exitCode;
}
