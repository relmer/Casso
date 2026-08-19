#include "Pch.h"

#include "CommandLineHelp.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLineHelp::LongPrefix / ShortPrefix
//
////////////////////////////////////////////////////////////////////////////////

std::string CommandLineHelp::LongPrefix (char flagPrefix)
{
    return (flagPrefix == '/') ? std::string ("/") : std::string ("--");
}


std::string CommandLineHelp::ShortPrefix (char flagPrefix)
{
    return (flagPrefix == '/') ? std::string ("/") : std::string ("-");
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLineHelp::UsageLineFor
//
//  One line per mode: the shape of the command line, then what it does.
//
//  THE SAME LINE HEADS THE MODE'S OWN PAGE, which is why it is a function
//  rather than three literals on the general page. A reader who typed
//  `CassoCli run --help` because the general page told them to has to meet the
//  same sentence there, and two copies of it are two sentences that can end up
//  describing different grammars.
//
//  The assembler is the fallback rather than a named mode, so it is what
//  anything other than `run` and `disk` answers to -- including Subcommand::None
//  and Subcommand::Help, which is what a help request resolves to before any
//  grammar has been chosen.
//
////////////////////////////////////////////////////////////////////////////////

std::string CommandLineHelp::UsageLineFor (CommandLineOptions::Subcommand mode)
{
    std::string  line;



    if (mode == CommandLineOptions::Subcommand::Run)
    {
        line = "  CassoCli run <binary | source> [options]   Load <binary> or assemble <source>, then execute";
    }
    else if (mode == CommandLineOptions::Subcommand::Disk)
    {
        line = "  CassoCli disk <verb> <image> [...]         Read and write disk images";
    }
    else
    {
        line = "  CassoCli <source> [options]                Assemble a source file";
    }

    return line;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLineHelp::BuildExampleCommands
//
//  The loop end to end: assemble, place the program, place a greeting that runs
//  it, set the greeting, launch the emulator.
//
//  THE COMMANDS ARE SEPARATE FROM THE PROSE THAT EXPLAINS THEM because two
//  pages want different amounts of it. The general page is a table of contents
//  and shows the loop; the disk page, where every flag in it is described,
//  shows the loop and then says why two of its steps look the way they do.
//  Splitting the block is what lets both have it without either owning a copy.
//
//  ONE FLAG HERE KEEPS THE `--` SPELLING WHATEVER THE READER ASKED FOR, and it
//  is the one belonging to a different program. `--disk1` is the emulator's
//  flag, not this tool's, and the emulator accepts only that spelling; printing
//  `/disk1` because the reader typed `/?` would be a promise this executable is
//  not the one keeping.
//
////////////////////////////////////////////////////////////////////////////////

std::string CommandLineHelp::BuildExampleCommands (char flagPrefix)
{
    std::string  lp = LongPrefix (flagPrefix);
    std::string  sp = ShortPrefix (flagPrefix);



    return std::string (kExampleHeading) + "\n"
        "  CassoCli prog.a65 " + sp + "oprog.bin\n"
        "  CassoCli disk put mydisk.dsk prog.bin " + lp + "as PROG "
                 + lp + "type B " + lp + "addr $6000\n"
        "  CassoCli disk put mydisk.dsk greet.bas " + lp + "as STARTUP " + lp + "basic\n"
        "  CassoCli disk boot mydisk.dsk STARTUP\n"
        "  Casso.exe --disk1 mydisk.dsk\n";
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLineHelp::BuildGeneralHelp
//
//  What the tool is, what each mode is for, where each mode's own help lives,
//  and the loop the whole thing exists to run. Nothing else.
//
//  IT IS ONE SCREEN, AND THAT IS THE POINT OF IT. The single page it replaces
//  was four screens: every flag of three grammars, three blocks of exit
//  statuses, and the example at the bottom where a reader who had scrolled past
//  the flags never reached it. A reader arrives knowing which of the three
//  things they want to do, so the page they land on says what the three things
//  are and how to ask about one -- and the detail waits behind that request
//  instead of arriving unasked.
//
//  NO EXIT STATUSES HERE. They differ per mode -- an assembly error is 3 under
//  the assembler and 1 under `run`, and status 1 means "written anyway" in one
//  mode and "nothing ran" in another -- so each mode states its own under
//  itself, and a general page could only state them wrongly or at four times
//  this length.
//
//  `?` IS THE ROUTE TO THE ASSEMBLER'S PAGE and carries no prefix, because it
//  is as65's own convention: a lone question mark is how that assembler is
//  asked for its usage, and assembling IS as65 mode. Every other route is
//  spelled with the prefix the reader typed.
//
////////////////////////////////////////////////////////////////////////////////

std::string CommandLineHelp::BuildGeneralHelp (const std::string & banner, char flagPrefix)
{
    //  Route, then what waits at the end of it. Padded to a fixed description
    //  column below rather than written into each literal, because `/help` is
    //  three characters narrower than `--help` and a table spaced for one
    //  prefix comes out ragged in the other.
    const size_t       kDescriptionColumn = 25;
    const std::string  lp                 = LongPrefix (flagPrefix);
    const std::string  routes[][2]        =
    {
        { "?",                   "Assembly options, output formats and listings" },
        { "run "  + lp + "help", "Run options: load address, entry point, limits" },
        { "disk " + lp + "help", "Disk verbs, their options, and a worked example" },
        { lp + "version",        "Version information" },
    };

    std::string        text               = banner;



    text += "\nUsage:\n";
    text += UsageLineFor (CommandLineOptions::Subcommand::As65) + "\n";
    text += UsageLineFor (CommandLineOptions::Subcommand::Run)  + "\n";
    text += UsageLineFor (CommandLineOptions::Subcommand::Disk) + "\n";

    text += "\nHelp:\n";

    for (const auto & route : routes)
    {
        std::string  line = "  CassoCli " + route[0];

        while (line.size() < kDescriptionColumn)
        {
            line += " ";
        }

        text += line + route[1] + "\n";
    }

    text += "\n";
    text += BuildExampleCommands (flagPrefix);

    return text;
}
