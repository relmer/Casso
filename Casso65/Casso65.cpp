#include "Pch.h"

#include "CommandLine.h"
#include "Casso65.h"


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
        PrintUsage (options.flagPrefix);
        return (options.showHelp) ? 0 : 1;
    }

    if (options.showVersion || options.subcommand == CommandLineOptions::Subcommand::Version)
    {
        PrintVersion ();
        return 0;
    }

    if (options.subcommand == CommandLineOptions::Subcommand::Run)
    {
        return DoRun (options);
    }

    if (options.subcommand == CommandLineOptions::Subcommand::As65)
    {
        return DoAs65 (options);
    }

    PrintUsage (options.flagPrefix);
    return 0;
}
