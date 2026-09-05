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
    static constexpr const char *  kExampleHeading = "Examples";

    //  What the heading used to carry after a comma. It reads as the first
    //  line UNDER the rule now, the way each command's block describes itself,
    //  rather than as a heading two lines wide that no underline could match.
    static constexpr const char *  kExampleLeadIn =
        "  The whole loop from source to a program running in the emulator:";

    //  The prefix a chosen style writes its flags with. One place, because the
    //  help writes flags in dozens of sentences and a style decided in each of
    //  them is a style that will disagree with itself.
    static std::string  GetLongPrefix  (char flagPrefix);
    static std::string  GetShortPrefix (char flagPrefix);

    //  One mode's usage line: what the command line looks like and what it
    //  does. The general page prints all of them; a mode's own page prints the
    //  one that is its, from here, so the two cannot describe the same
    //  invocation differently.
    static std::string  GetUsageLine (CommandLineOptions::Subcommand mode);

    //  The operands a grammar line shows as REQUIRED, in the order it shows
    //  them.
    //
    //  A <token> outside any [optional] group, and not the value of an option
    //  that precedes it. `<binary | source>` is one operand and not two: the
    //  angle brackets bound it, not the spaces.
    //
    //  SHARED SO A REFUSAL CANNOT NAME SOMETHING THE USAGE DOES NOT SHOW.
    //  Every mode prints a grammar line and every mode has to complain about
    //  the same operands it just printed; reading them back out of that line
    //  is what keeps the two from being written twice and drifting.
    static std::vector<std::string>  GetRequiredOperands (const std::string & grammar);

    //  The five commands of the worked loop, without the prose that explains
    //  the two traps in it. The general page shows the loop; the disk page
    //  shows the loop and then explains it.
    static std::string  BuildExampleCommands (char flagPrefix);

    //  The whole general page, banner included.
    static std::string  BuildGeneralHelp (const std::string & banner, char flagPrefix);

    //  The widest line BuildEmulatorHelp will produce.
    //
    //  A PROMISE WITH A REASON BEHIND IT, and a test that measures it. The
    //  emulator's usage text is shown in a themed message box rather than
    //  printed to a console: the box is a fixed width in a proportional font,
    //  and a line past this budget wraps somewhere the author did not choose,
    //  which turns an indented description into two ragged ones. Descriptions
    //  are stored as sentences and wrapped to this here, so the table stays a
    //  table and only one place knows the width.
    //
    //  44 IS THE BOX'S OWN NUMBER, not a guess: DxuiMessageBox sizes itself by
    //  dividing what is left of its 400 DIP after the margins and the icon
    //  column by an average advance, and lands on 44. Budgeting wider than that
    //  does not clip -- the box has room -- but every line over it counts as
    //  two in the height estimate, and the dialog comes out with a band of
    //  empty space under the text.
    static constexpr size_t  kEmulatorLineColumns = 44;

    //  The emulator GUI's usage text: how the command line is written, then
    //  every option it takes, composed from the parser's own table.
    static std::string  BuildEmulatorHelp (char flagPrefix);

    //  `text` broken at spaces so no line exceeds `columns`, each continuation
    //  carrying `indent`. A word longer than the budget takes a line of its own
    //  rather than being cut.
    static std::string  WrapToColumns (const std::string & text,
                                       const std::string & indent,
                                       size_t              columns);
};
