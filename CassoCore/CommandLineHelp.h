#pragma once

#include "CommandLineOptions.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLineHelp
//
//  The usage text more than one page needs: what each mode is for, how to reach
//  that mode's own page, and the worked example.
//
//  THE HELP IS TIERED, and this class exists so the tiers cannot drift apart.
//  The general page is a table of contents -- the banner, one line per mode, the
//  route to each mode's page, and the loop the tool exists to run -- and every
//  flag is described on the page of the mode that takes it. Anything two pages
//  both need is a function here, called twice, rather than a sentence written
//  twice.
//
//  IT LIVES IN THE LIBRARY SO A TEST CAN READ IT, for the reason
//  DiskCommandRunner's help already gives: the test assembly does not link the
//  console executable, so help assembled beside the printing code is help
//  nothing can check. The general page is the one with a size promise on it --
//  it has to stay one screen -- and a promise nothing measures is a promise that
//  quietly lapses.
//
//  The banner arrives as an argument rather than being built here because it
//  carries the build's own version and architecture, which the executable knows
//  and the library does not. It is still part of the page, so it is counted with
//  it rather than printed around it.
//
////////////////////////////////////////////////////////////////////////////////

class CommandLineHelp
{
public:
    //  The line the worked example starts on, so a reader and a test look for
    //  the same thing.
    static constexpr const char *  kExampleHeading =
        "Examples -- the whole loop, from source to a program running in the emulator:";

    //  The prefix a chosen style spells its flags with. One place, because the
    //  help spells flags in dozens of sentences and a style decided in each of
    //  them is a style that will disagree with itself.
    static std::string  LongPrefix  (char flagPrefix);
    static std::string  ShortPrefix (char flagPrefix);

    //  One mode's usage line: what the command line looks like and what it
    //  does. The general page prints all of them; a mode's own page prints the
    //  one that is its, from here, so the two cannot describe the same
    //  invocation differently.
    static std::string  UsageLineFor (CommandLineOptions::Subcommand mode);

    //  The five commands of the worked loop, without the prose that explains
    //  the two traps in it. The general page shows the loop; the disk page
    //  shows the loop and then explains it.
    static std::string  BuildExampleCommands (char flagPrefix);

    //  The whole general page, banner included.
    static std::string  BuildGeneralHelp (const std::string & banner, char flagPrefix);
};
