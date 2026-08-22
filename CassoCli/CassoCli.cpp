#include "Pch.h"

#include "CommandLine.h"
#include "CommandLineParser.h"
#include "As65Mode.h"
#include "DiskCommand.h"
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
    HRESULT             hr       = S_OK;
    int                 exitCode = 0;



    // A first word that named nothing gets a targeted message, not the usage
    // block -- see PrintUnrecognizedArgument. Checked before the usage arm
    // because it is a strictly better answer for the same condition.
    if (!options.unrecognizedArgument.empty())
    {
        CommandLine::PrintUnrecognizedArgument (options.unrecognizedArgument, options.flagPrefix);
        exitCode = 1;
    }
    else if (!options.unrecognizedFlag.empty())
    {
        // The subcommand was recognized, so the help that follows the message
        // is that mode's alone. Refused rather than warned about and run: a
        // typo that still produced an output file was a typo nobody saw.
        CommandLine::PrintUnrecognizedFlag (options.unrecognizedFlag, options.subcommand, options.flagPrefix);
        exitCode = CommandLineParser::ExitCodeForRefusal (options.subcommand);
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
        CommandLine::PrintUsage (options);
        exitCode = options.showHelp ? 0 : 1;
    }
    else if (options.parseVerdict == CommandLineOptions::ParseVerdict::Refused)
    {
        // A REFUSED COMMAND LINE RUNS NOTHING.
        //
        // The parser said what was wrong and this prints it. The arm was
        // missing entirely, so `CassoCli as65 prog.a65 extra.a65` reported the
        // surplus argument and then went on to assemble anyway, complaining it
        // could not read a source file the refusal had said nothing about.
        //
        // BELOW THE HELP ARM, because a command line refused for naming no
        // subcommand at all is answered with the usage page and that arm owns
        // it.
        //
        // THE MODE'S OWN PAGE COMES FIRST AND THE REASON LAST. The answer to
        // "you typed this wrong" is the grammar, and a reader sees the bottom
        // of the screen, so a 98-line page ahead of the message would leave
        // the message on it and the page above it. Usage goes to stdout and
        // the reason to stderr, so stdout is flushed between them to make the
        // order on the screen the order written here.
        CommandLine::PrintPageFor (options.subcommand, options.flagPrefix);
        std::cout.flush();

        std::cerr << CommandLine::kGapBeforeTheReason << options.refusalMessage;

        exitCode = CommandLineParser::ExitCodeForRefusal (options.subcommand);
    }
    else if (options.showVersion || options.subcommand == CommandLineOptions::Subcommand::Version)
    {
        CommandLine::PrintVersion();
    }
    else if (options.subcommand == CommandLineOptions::Subcommand::Run)
    {
        hr = RunMode::Run (options, exitCode);
    }
    else if (options.subcommand == CommandLineOptions::Subcommand::Disk
             && options.disk.verb == CommandLineOptions::DiskOptions::Verb::Help)
    {
        // ASKED FOR HERE RATHER THAN FETCHED FROM THE RUNNER, so it folds.
        //
        // `disk --help` resolves to a verb of the disk grammar, so it used to
        // arrive as the runner's `output` and go to the console the way a
        // catalog listing does: verbatim, because a listing is a table and
        // reflowing one would destroy it. The page is prose and wants the
        // opposite, and it was the one page in the tool that stayed at the
        // width it was composed at however wide the terminal was.
        CommandLine::PrintPageFor (options.subcommand, options.flagPrefix);
    }
    else if (options.subcommand == CommandLineOptions::Subcommand::Disk)
    {
        // The only arm that reports its own status rather than an HRESULT:
        // `disk` distinguishes "carried out with something worth saying" from
        // "nothing was done", and those are the runner's to assign.
        exitCode = DiskCommand::Run (options);
    }
    else if (options.subcommand == CommandLineOptions::Subcommand::As65)
    {
        As65Mode  mode;

        hr = mode.Run (options, exitCode);
    }
    else if (options.subcommand == CommandLineOptions::Subcommand::Merlin)
    {
        MerlinMode  mode;

        hr = mode.Run (options, exitCode);
    }
    else
    {
        // A subcommand the parser knows but this dispatch does not.
        CommandLine::PrintUsage (options);
    }

    //  The HRESULT says what went wrong; the exit code is what a script reads.
    //  Only the second crosses the process boundary, and it is never derived
    //  from the first -- an assembly that warned succeeded and exits 5.
    (void) hr;

    return exitCode;
}
