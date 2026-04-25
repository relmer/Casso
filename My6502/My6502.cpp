#include "Pch.h"

#include "CommandLine.h"
#include "My6502.h"


////////////////////////////////////////////////////////////////////////////////

//
//  main
//
////////////////////////////////////////////////////////////////////////////////

int main (int argc, char * argv[])
{
    CommandLineOptions options = ParseCommandLine (argc, argv);

    if (options.showHelp || options.subcommand == CommandLineOptions::Subcommand::None
                        || options.subcommand == CommandLineOptions::Subcommand::Help)
    {
        PrintUsage ();
        return 0;
    }

    if (options.showVersion || options.subcommand == CommandLineOptions::Subcommand::Version)
    {
        PrintVersion ();
        return 0;
    }

    if (options.subcommand == CommandLineOptions::Subcommand::Assemble)
    {
        return DoAssemble (options);
    }

    if (options.subcommand == CommandLineOptions::Subcommand::Run)
    {
        return DoRun (options);
    }

    PrintUsage ();
    return 0;
}
