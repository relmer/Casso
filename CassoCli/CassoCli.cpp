#include "Pch.h"

#include "CommandLine.h"
#include "CassoCli.h"





////////////////////////////////////////////////////////////////////////////////
//
//  main
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
