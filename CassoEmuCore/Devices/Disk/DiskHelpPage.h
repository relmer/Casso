#pragma once

#include "Pch.h"

#include "CommandLineOptions.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskHelpPage
//
//  THE DISK GRAMMAR'S HELP, COMPOSED WHERE ITS COMMANDS ARE. One block per
//  command, each carrying its grammar, its options and its example; every
//  flag written with the prefix the reader asked in. Stateless: the banner
//  and the prefix arrive as arguments, so a test needs no runner and no
//  file seam to render any page.
//
////////////////////////////////////////////////////////////////////////////////

class DiskHelpPage
{
public:
    //
    //  What each status means when a `disk` command returns it.
    //
    //  STATED UNDER `disk` BECAUSE IT IS DISK'S. One combined block used to
    //  stand at the top of the help claiming to describe all three modes, and
    //  it described none of them accurately: an assembly error is 2 under the
    //  assembler and 1 under `run`, and status 1 means "the output was written
    //  anyway" in one mode and "nothing ran" in another. What the three modes
    //  share is only the shape 0/1/2 -- not the meanings, which is exactly what
    //  a script branching on a number needs.
    //
    //  Beside the runner that assigns each one, so the wording and the
    //  behavior are edited together and a test can read both.
    //
    static constexpr const char *  kExitStatusHelpText =
        "    0  Success\n"
        "    1  Success, with a warning: a listing cut short by damage or a file"
        " delivered with unreadable sectors as zeros\n"
        "    2  Error, and nothing was done: a command or option refused, an image"
        " that cannot be read or holds no filesystem, a file that is not on the"
        " volume, a startup program a booting DOS 3.3 cannot run, or a write the"
        " volume or the host refused. The image is"
        " byte-for-byte as it was";

    //
    //  The disk section of the help, in the three pieces the usage text places
    //  separately: what the subcommand DOES, what its OPTIONS are, and the
    //  worked EXAMPLE that goes at the end.
    //
    //  Split because the surrounding help groups by kind and not by subcommand:
    //  a reader looking for options wants every subcommand's options together,
    //  and an example buried among them is one nobody reaches.
    //
    //  EVERY OPTION TAKES THE PREFIX THE READER ASKED FOR. Someone who typed
    //  `/?` is shown `/out`; someone who typed `--help` is shown `--out`. Both
    //  are accepted, so neither is a lie -- which is the whole reason the
    //  parser had to learn `/` before this could be honest.
    //
    //  Assembled here rather than beside the printing code for the reason
    //  kInUseHelpText already gives: the test assembly does not link the console
    //  executable, so help written there is help nothing can check.
    //
struct DiskCommandHelp
{
    //  WHICH COMMAND THIS ROW DESCRIBES, so a caller with a Command in hand
    //  can find its block without matching on the heading text.
    CommandLineOptions::DiskOptions::Command  command;

    const char *  forms;         // every accepted spelling, the plain one first
    const char *  summary;       // one line, for the list at the top
    const char *  grammar;       // where the operands go
    const char *  options;       // the options this command takes, or nothing
    const char *  discussion;    // what no option row can state, or nothing
    const char *  example;       // one line that does something real
};

    //  Every command the page describes, so a test can walk what the help
    //  claims instead of quoting sentences out of it.
    static std::span<const DiskCommandHelp>  GetCommandHelp();

    //  One command's block: its heading, grammar, options, and example.
    //
    //  A COMMAND MISSING AN OPERAND GETS THIS AND NOT THE WHOLE PAGE. The
    //  reader has already said which command they want; answering with eight
    //  of them is answering a question they did not ask, and the one they did
    //  ask is four screens down. Empty when the command has no block.
    static std::string  BuildOneBlock    (const DiskCommandHelp & entry, char flagPrefix);

    //  A paragraph with its runs of spaces collapsed to one.
    //
    //  THE WRAPPER READS A COLUMN BOUNDARY OUT OF THE TEXT: the last run of
    //  two or more spaces is where a continuation line is indented to, which
    //  is right for a two-column option row and wrong for a sentence. One
    //  stray double space mid-paragraph and every line after it hangs off that
    //  column instead of the left margin. Prose has no columns, so it has no
    //  business carrying a gutter.
    static std::string  FormatAsProse           (const std::string & text);

    static std::string  BuildCommandHelp (CommandLineOptions::DiskOptions::Command command,
                                          char flagPrefix);

    static std::string  ApplyPrefixes      (const std::string & text, char flagPrefix);

    static std::string  BuildSubcommandHelp (char flagPrefix);

    static std::string  BuildCommandBlocks  (char flagPrefix);

    static std::string  BuildOptionsHelp    (char flagPrefix);

    static std::string  BuildExampleHelp    (char flagPrefix);

    //  All three together, which is what a test reads when the question is
    //  about the disk help as a whole rather than about where a piece lands.
    //
    //  THE BANNER IS AN ARGUMENT for the reason CommandLineHelp::BuildGeneralHelp
    //  takes one: what the tool is called, which version this is and who holds
    //  the copyright are the EXECUTABLE's knowledge -- the version and the
    //  architecture come from its build -- and this page is assembled in the
    //  library. It heads every other help page, so it heads this one; it
    //  defaults to empty so a test asking what the disk page SAYS is not made to
    //  supply a version first.
    static std::string  BuildHelpText (char flagPrefix = '-', const std::string & banner = "");

    //  Every command the grammar accepts, aliases included and comma-separated,
    //  for the refusal a word that is none of them earns.
    //
    //  Read from the parser's own table rather than retyped, because a retyped
    //  list is a list that goes stale: the aliases were added to the grammar
    //  and the refusal went on naming the five original commands, so a user who
    //  mistyped `catalgo` was told to try `list, get, put, delete, boot` and
    //  never learned that `catalog` was there all along.
    static std::string  DescribeAcceptedCommands();
};
