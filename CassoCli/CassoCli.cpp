#include "Pch.h"

#include "CommandLine.h"
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
//  The exit code is the only thing this function produces; every subcommand
//  reports through it, which is what makes the tool usable from a build script.
//
////////////////////////////////////////////////////////////////////////////////

int main (int argc, char * argv[])
{
    CommandLineOptions  options  = ParseCommandLine (argc, argv);
    int                 exitCode = 0;



    // No subcommand is a usage ERROR (exit 1); an explicit --help or `help`
    // is the user asking, and succeeds.
    if (options.showHelp || options.subcommand == CommandLineOptions::Subcommand::None
                        || options.subcommand == CommandLineOptions::Subcommand::Help)
    {
        PrintUsage (options.flagPrefix);
        exitCode = options.showHelp ? 0 : 1;
    }
    else if (options.showVersion || options.subcommand == CommandLineOptions::Subcommand::Version)
    {
        PrintVersion();
    }
    else if (options.subcommand == CommandLineOptions::Subcommand::Run)
    {
        exitCode = DoRun (options);
    }
    else if (options.subcommand == CommandLineOptions::Subcommand::As65)
    {
        exitCode = DoAs65 (options);
    }
    else
    {
        // A subcommand the parser knows but this dispatch does not.
        PrintUsage (options.flagPrefix);
    }

    return exitCode;
}
