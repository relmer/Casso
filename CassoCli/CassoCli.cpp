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
//  ASKING FOR THE USAGE TEXT EXITS 0; BEING SHOWN IT EXITS 2. Both print the
//  same page, and the difference is who wanted it there. An explicit --help or
//  `help` is the user asking. A bare `CassoCli` and an option this grammar does
//  not have are the tool answering a command line it could not act on, and each
//  of those produced nothing -- which is what this tool's own table calls 2.
//
//  The comment here used to say a missing subcommand exits 1, and the arm that
//  would have done it was unreachable: the parser sets showHelp for an empty
//  command line, so the "user asked" branch claimed it and reported success.
//
//  Dispatch is a flat chain over the parsed subcommand, so the parser owns all
//  the grammar and this function owns none of it.
//
//  The final arm catches a subcommand the parser recognizes but this dispatch
//  does not -- a genuinely unreachable state kept as a defined behavior rather
//  than a fallthrough returning an uninitialized code.
//
//  A COMMAND LINE THE PARSER REFUSED EXITS 2 EVEN THOUGH IT PRINTS USAGE. An
//  option this grammar does not have ends parsing and leaves showHelp set so
//  the user is shown the grammar, and that arrangement used to report success:
//  the tool printed a diagnostic, printed the whole help, and told the calling
//  script it had worked. Printing usage is how the tool answers the mistake,
//  not evidence there was none.
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
    bool                asked    = options.showHelp && !refused;



    // The usage page is reached two ways and they do not report the same
    // thing: `asked` is the user requesting it, and everything else landing
    // here is the page being printed AT them because nothing could be done.
    if (options.showHelp || options.subcommand == CommandLineOptions::Subcommand::None
                        || options.subcommand == CommandLineOptions::Subcommand::Help)
    {
        PrintUsage (options);
        exitCode = asked ? 0 : 2;
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
