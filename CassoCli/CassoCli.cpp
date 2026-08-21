#include "Pch.h"

#include "CommandLine.h"
#include "As65Mode.h"
#include "MerlinMode.h"
#include "RunMode.h"
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
    CommandLineOptions  options  = CommandLine::Parse (argc, argv);
    int                 exitCode = 0;



    // A first word that named nothing gets a targeted message, not the usage
    // block -- see PrintUnrecognizedArgument. Checked before the usage arm
    // because it is a strictly better answer for the same condition.
    if (!options.unrecognizedArgument.empty())
    {
        CommandLine::PrintUnrecognizedArgument (options.unrecognizedArgument);
        exitCode = 1;
    }
    else if (!options.outputFormatConflict.empty())
    {
        // Two formats named is two files asked for, and one gets written. Same
        // reasoning as the arm below: the sentence naming both flags is a
        // better answer than usage text that lists them among twenty others.
        exitCode = CommandLine::PrintCpuFlagRefusal (options.outputFormatConflict);
    }
    else if (!options.cpuFlagRefusal.empty())
    {
        // Checked before the usage arm for the same reason the line above is: a
        // refusal that names the directive to write instead is a strictly better
        // answer than a wall of usage text, and printing usage would bury it.
        exitCode = CommandLine::PrintCpuFlagRefusal (options.cpuFlagRefusal);
    }
    else if (options.showHelp || options.subcommand == CommandLineOptions::Subcommand::None
                             || options.subcommand == CommandLineOptions::Subcommand::Help)
    {
        // No subcommand is a usage ERROR (exit 1); an explicit --help or
        // `help` is the user asking, and succeeds.
        CommandLine::PrintUsage (options.flagPrefix);
        exitCode = options.showHelp ? 0 : 1;
    }
    else if (options.showVersion || options.subcommand == CommandLineOptions::Subcommand::Version)
    {
        CommandLine::PrintVersion();
    }
    else if (options.subcommand == CommandLineOptions::Subcommand::Run)
    {
        exitCode = RunMode::Run (options);
    }
    else if (options.subcommand == CommandLineOptions::Subcommand::As65)
    {
        As65Mode  mode;

        exitCode = mode.Run (options);
    }
    else if (options.subcommand == CommandLineOptions::Subcommand::Merlin)
    {
        MerlinMode  mode;

        exitCode = mode.Run (options);
    }
    else
    {
        // A subcommand the parser knows but this dispatch does not.
        CommandLine::PrintUsage (options.flagPrefix);
    }

    return exitCode;
}
