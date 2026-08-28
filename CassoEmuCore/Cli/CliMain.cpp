#include "Pch.h"

#include "CommandLine.h"
#include "CommandLineParser.h"
#include "CommandLineHelp.h"
#include "As65Mode.h"
#include "DiskCommand.h"
#include "MerlinMode.h"
#include "RunMode.h"
#include "CliMain.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CliMain
//
//  Parses the command line and dispatches to a subcommand: the whole of what
//  CassoCli.exe does, minus the four lines that call it.
//
//  IT LIVES IN THE LIBRARY BECAUSE A TEST CAN REACH IT THERE. The criterion
//  is testability rather than portability, and this function is nothing but
//  decisions: which arm a parse lands in, which page it prints, what it
//  returns. All of it sat in the executable, which the test assembly does not
//  link, so none of it could be checked -- and the exit codes the help had
//  been documenting for a release turned out not to be the ones it returned.
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

int CliMain (int argc, char * argv[])
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
        CommandLine::PrintCpuFlagRefusal (options.outputFormatConflict);
        exitCode = CommandLineParser::ExitCodeForRefusal (options.subcommand);
    }
    else if (!options.cpuFlagRefusal.empty())
    {
        // Checked before the usage arm for the same reason the line above is: a
        // refusal that names the directive to write instead is a strictly better
        // answer than a wall of usage text, and printing usage would bury it.
        CommandLine::PrintCpuFlagRefusal (options.cpuFlagRefusal);
        exitCode = CommandLineParser::ExitCodeForRefusal (options.subcommand);
    }
    else if (options.showHelp || options.subcommand == CommandLineOptions::Subcommand::None
                             || options.subcommand == CommandLineOptions::Subcommand::Help)
    {
        // ASKING FOR HELP SUCCEEDS; BEING SHOWN IT DOES NOT.
        //
        // Both print the same page, and the difference is who started it:
        // a script that invokes the tool wrongly has to fail, and one that
        // asks how the tool works must not. showHelp cannot tell them
        // apart, because it is set either way; the VERDICT can, because a
        // command line carrying nothing at all is refused and an explicit
        // request is clean.
        //
        // It read showHelp, so a bare `CassoCli` exited 0 while the banner
        // above this function said it exited 1. Nothing could see that
        // until this function moved somewhere a test can call it.
        CommandLine::PrintUsage (options);
        exitCode = (options.parseVerdict == CommandLineOptions::ParseVerdict::Refused) ? 1 : 0;
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
        //  Narrowed to the one disk command when the reader named one:
        //  the runner already answers its own refusals that way, and which
        //  side caught the mistake is not something the reader can see.
        CommandLine::PrintPageFor (options.subcommand, options.flagPrefix, options.disk.command);
        CommandLine::FlushOutput();

        std::cerr << CommandLine::kGapBeforeTheReason << options.refusalMessage;

        exitCode = CommandLineParser::ExitCodeForRefusal (options.subcommand);
    }
    else if (options.inputFile.empty()
             && (options.subcommand == CommandLineOptions::Subcommand::As65
              || options.subcommand == CommandLineOptions::Subcommand::Merlin
              || options.subcommand == CommandLineOptions::Subcommand::Run))
    {
        // THE MODE'S PAGE, AND THEN WHAT IT DID NOT GET.
        //
        // It answered "Error: No input file specified", which told a reader
        // who had just discovered the mode the one thing they had already
        // worked out; then for a while it printed the page and said nothing
        // at all, which left them to work out WHY a page had appeared.
        //
        // `disk cat` names the operand it wanted. There is no reason for
        // these three to answer differently: a mode with nothing after it has
        // its command chosen and its operand missing, exactly as `disk cat`
        // does. The name is read back out of the same usage line the page
        // prints, so a refusal cannot ask for something the usage does not
        // show.
        //
        // Still non-zero, and for the same reason a bare `CassoCli` is: a
        // script that invokes the tool wrongly has to fail. Asking for the
        // page BY NAME is what exits 0.
        std::vector<std::string>  required = CommandLineHelp::RequiredOperandsIn (
                                                 CommandLineHelp::UsageLineFor (options.subcommand));

        {
            CommandLine::UsageOnErrorStream  toTheErrorStream;

            CommandLine::PrintPageFor (options.subcommand, options.flagPrefix);
            std::fflush (stderr);
        }

        if (!required.empty())
        {
            std::cerr << CommandLine::kGapBeforeTheReason
                      << "Error: required parameter " << required[0] << " missing\n";
        }

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
             && options.disk.command == CommandLineOptions::DiskOptions::Command::Help)
    {
        // ASKED FOR HERE RATHER THAN FETCHED FROM THE RUNNER, so it folds.
        //
        // `disk --help` resolves to a command of the disk grammar, so it used to
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

    //  ONE BLANK LINE BEFORE THE SHELL PROMPT, whatever the tool just said.
    //
    //  Every arm above ends on its own last line and the prompt landed against
    //  it, which reads as though the prompt were part of the output. Written
    //  once here rather than at the end of each arm, because there are ten of
    //  them and the one that forgets is the one nobody notices.
    //
    //  ON THE STREAM THE TOOL SPOKE ON, which is the only way it is genuinely
    //  last. Pinned to stderr it was the same cross-stream splice as a
    //  refusal's page and its reason: correct in each stream, and rendered by
    //  a terminal wherever its two readers happened to be. A blank line in the
    //  middle of a help page is what that looked like.
    //
    //  When nothing was said, it falls back to stderr, so it cannot land in a
    //  redirected file or, worse, in the middle of an extracted binary:
    //  `disk get` writes a file's bytes to stdout, and a newline appended to
    //  those is a corrupted file.
    CommandLine::FlushOutput();
    CommandLine::PrintTrailingBlankLine();

    //  The HRESULT says what went wrong; the exit code is what a script reads.
    //  Only the second crosses the process boundary, and it is never derived
    //  from the first -- an assembly that warned succeeded and exits 5.
    (void) hr;

    return exitCode;
}
