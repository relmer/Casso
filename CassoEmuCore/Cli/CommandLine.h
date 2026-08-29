#pragma once

#include "CommandLineOptions.h"
#include "CommandLineParser.h"
#include "Dialect.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLine
//
//  The command line itself: what an argv means, and what the tool says about
//  its own arguments.
//
//  The grammar lives in CassoCore/CommandLineParser, where the UnitTest project
//  can reach it. What is here is the executable's side of the same subject --
//  the host filesystem probe the parser needs to tell a source file from a
//  mistyped subcommand, the usage text, and the two messages a refused
//  invocation earns.
//
//  IT DOES NOTHING WITH WHAT IT PARSED. Assembling is AssemblerMode's, running
//  is RunMode's, and writing files is ArtifactWriter's; a class that parsed the
//  arguments AND acted on them would be the reason every one of those was
//  unreachable from a test.
//
//  The usage text is written one logical line per item and folded to the
//  reader's terminal at print time -- see UsageText for why the continuation
//  column is found rather than authored.
//
////////////////////////////////////////////////////////////////////////////////

class CommandLine
{
public:
    //
    //  What separates a usage page from the reason it was printed.
    //
    //  TWO BLANK LINES, named once so the four places that print a page and
    //  then a reason cannot drift into three gaps. The page ends in a newline
    //  of its own, so the two blank lines are two more.
    //
    //  One was not enough. A page closes on a run of indented continuation
    //  lines -- the exit-code table's wrapped entries, the disk page's worked
    //  example -- and a single blank under those reads as the paragraph break
    //  the page uses everywhere else, which left the reason looking like one
    //  more line of help rather than the answer to what went wrong.
    //
    static constexpr const char *  kGapBeforeTheReason = "\n\n";

    //
    //  Empties EVERY buffer the tool writes usage through, so what goes to
    //  stderr next lands under it rather than inside it.
    //
    //  TWO BUFFERS, WHICH IS THE WHOLE POINT. std::println writes to the C
    //  stdout FILE*, and std::cout is a separate C++ stream over the same
    //  descriptor; the pages go out through the first and the disk runner's
    //  output through the second. Flushing only std::cout left std::println's
    //  buffer full, so a refusal written to the unbuffered stderr overtook the
    //  page still sitting in it -- `as65 -asdf` printed the reason in the
    //  middle of the flag table, and the blank lines meant to separate them
    //  landed halfway up the page too.
    //
    static void                FlushOutput               ();

    static CommandLineOptions  Parse                     (int argc, char * argv[]);

    //  The page the request asked for, written with the prefix it was typed
    //  with. Both live on the options, so the caller hands over the whole
    //  parse rather than picking two fields out of it and deciding again.
    static void                PrintUsage                (const CommandLineOptions & options);

    ////////////////////////////////////////////////////////////////////////////
    //
    //  UsageOnErrorStream
    //
    //  Sends every line of usage to the error stream while it is alive.
    //
    //  A PAGE PRINTED AS PART OF A REFUSAL BELONGS WHERE ITS REASON GOES.
    //  Splitting them was correct on paper and wrong on a screen: the page went
    //  to stdout, stdout was flushed, and only then was a word of the reason
    //  written, and a terminal reading the two pipes on two threads still
    //  spliced the reason into the middle of the examples. Nothing a writer
    //  does can order two streams for a reader; one stream can only arrive in
    //  the order it was written.
    //
    //  A refusal is not output anybody pipes into another tool, so putting its
    //  page on the error stream costs nothing and is what most tools do.
    //
    ////////////////////////////////////////////////////////////////////////////

    //  Where usage is going right now. Exposed so the guard above can be
    //  asserted without a test rebinding the process's own handles, which
    //  takes the test runner's reporting down with it.
    //  A block of composed usage, folded to the terminal row by row.
    //
    //  PUBLIC BECAUSE THE DISK RUNNER COMPOSES A PAGE IN CORE and the
    //  printing edge has to fold it. Printed verbatim instead, `disk` alone
    //  emitted a single 623-character line and left the terminal to break it
    //  at column zero, losing the indent on every continuation.
    static void                PrintUsageBlock (const std::string & block);

    static std::FILE *         GetUsageStream();

    //  The blank line that separates whatever the tool just said from the
    //  shell prompt.
    //
    //  IT GOES WHERE THE TALKING WENT. Written to a fixed stream it is the
    //  same cross-stream splice as the page and its reason: a terminal reading
    //  two pipes can render it anywhere, and a blank line in the middle of a
    //  help page is what that looks like. So it follows whichever stream the
    //  message actually used, and falls back to the error stream when there
    //  was no message -- which is the case that matters, because `disk get`
    //  writes a file's bytes to stdout and a newline appended to those is a
    //  corrupted file.
    static void                PrintTrailingBlankLine();

    class UsageOnErrorStream
    {
    public:
        UsageOnErrorStream();
        ~UsageOnErrorStream();

        UsageOnErrorStream (const UsageOnErrorStream &)             = delete;
        UsageOnErrorStream & operator= (const UsageOnErrorStream &) = delete;
    };
    static void                PrintVersion              ();

    //  What the tool is called, which build this is, and who holds the
    //  copyright. Public because the disk help is assembled in the library,
    //  which does not know the build's version, so that page is handed this
    //  at print time rather than building one of its own.
    static std::string         BuildBanner               ();

    //  A first word that named no subcommand: the full usage, then what the
    //  word was instead -- LAST, so it is the line left on screen.
    static void                PrintUnrecognizedArgument (const std::string & word, char prefix);

    //  An argument a recognized subcommand's grammar did not know: the full
    //  usage, then which argument, for the same reason.
    static void                PrintUnrecognizedFlag     (const std::string & flag, CommandLineOptions::Subcommand subcommand, char prefix);

    //  A CPU flag the active dialect does not take. The sentence is composed
    //  in core, where the dialect's own data is; this prints it and says what
    //  the process is about to return.
    static void                PrintCpuFlagRefusal       (const std::string & refusal);

    //  The page belonging to one mode, whichever mode that is.
    //
    //  Public because a REFUSAL prints it too, not only a help request: the
    //  answer to "you typed this wrong" is the grammar, so main and the disk
    //  edge reach for the same page a --help would have opened.
    //  `diskCommand` narrows the disk page to the block for one command,
    //  which is what a reader who already named one is asking about. None
    //  prints the whole page, and is right for a reader who has not.
    static void                PrintPageFor              (CommandLineOptions::Subcommand mode, char prefix,
                                                          CommandLineOptions::DiskOptions::Command diskCommand
                                                              = CommandLineOptions::DiskOptions::Command::None);

private:
    //  How wide the reader's terminal is, or 80 when there is no terminal.
    static size_t  GetUsageWidth       ();

    //  One logical line of usage, folded to that width. EVERY line of help goes
    //  through here, so none of them is hand-wrapped to a width the reader may
    //  not have.
    static void    PrintUsageLine      (const std::string & line);

    //  A run of usage composed elsewhere -- core builds the dialect flag lines
    //  -- folded row by row.

    //  A top-level heading, underlined to its own width.
    static void    PrintSectionHeading (const std::string & name);


    //  One page per grammar. Each opens with the banner and its own usage
    //  line, generates its flags from that dialect's table, and closes with
    //  the exit codes that mode actually spends.
    static void    PrintAssemblePage   (char prefix);
    static void    PrintMerlinPage     (char prefix);
    static void    PrintRunPage        (char prefix);

    static void    PrintPageBanner     (CommandLineOptions::Subcommand mode);
    static void    PrintDialectFlags   (DialectId dialect, char prefix);
    static void    PrintExitCodes      (const std::string & codes);

    //  How much memory this machine has fitted, for the one help line that
    //  mentions it. The library composes the sentence and cannot ask the OS
    //  itself, so the asking happens here, at the platform edge. Zero when
    //  the OS declines to say.
    static unsigned  GetInstalledGigabytes  ();
};
