#include "Pch.h"

#include "CommandLine.h"
#include "HostFile.h"
#include "UsageText.h"
#include "As65ExitStatus.h"
#include "CommandLineHelp.h"
#include "DialectHelp.h"
#include "DialectRegistry.h"
#include "Devices/Disk/DiskCommandRunner.h"
#include "Version.h"



#if defined(_M_X64) || defined(__x86_64__)
    static const char * s_arch = "x64";
#elif defined(_M_ARM64) || defined(__aarch64__)
    static const char * s_arch = "ARM64";
#else
    static const char * s_arch = "Unknown";
#endif



//  The run options, as format strings over {0} the long prefix and {1} the pad
//  that keeps a `/` page in the same columns as a `--` one. File scope rather
//  than a local, because the loop that prints them wants the flag variables of
//  its own function and a declaration block cannot hold both.
static const char *  s_kRunOptionLines[] =
{
    "  {0}load <addr>{1}          Load address (default: $8000)",
    "  {0}exec <addr>{1}          Where execution starts. Defaults to the load address",
    "  {0}reset-vector{1}         Use reset vector at $FFFC/$FFFD",
    "  {0}stop <addr>{1}          Stop when PC reaches address",
    "  {0}max-cycles <n>{1}       Maximum cycles before stopping",
};





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLine::Parse
//
//  Thin adapter over the core parser: supplies the one thing the grammar needs
//  from the platform -- whether a candidate source file exists -- and lets
//  CassoCore own every parsing decision.
//
//  The parser is deliberately unable to reach the filesystem itself, which is
//  what lets the UnitTest project exercise the whole grammar. This is the only
//  place that gap is closed.
//
////////////////////////////////////////////////////////////////////////////////

CommandLineOptions CommandLine::Parse (int argc, char * argv[])
{
    return CommandLineParser::Parse (argc, argv, HostFile::Exists);
}





//  Where usage text goes. stdout normally; the error stream while a
//  UsageOnErrorStream is alive, so a refused command line's page and its
//  reason arrive in the order they were written.
static std::FILE *  s_pUsageStream = stdout;


//  Which stream this invocation actually said something on, or null if it
//  said nothing a person reads. Recorded so the closing blank line can join
//  it rather than pick a stream of its own.
static std::FILE *  s_pSpokenOn = nullptr;





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLine::UsageStream
//
//  Where usage is going right now.
//
////////////////////////////////////////////////////////////////////////////////

std::FILE * CommandLine::UsageStream()
{
    return s_pUsageStream;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLine::PrintTrailingBlankLine
//
//  One blank line between what the tool said and the shell prompt, on the
//  stream it said it on.
//
////////////////////////////////////////////////////////////////////////////////

void CommandLine::PrintTrailingBlankLine()
{
    //  Nothing a person read means nothing to separate, and stdout may be
    //  holding a binary this must not touch.
    std::FILE *  to = (s_pSpokenOn != nullptr) ? s_pSpokenOn : stderr;



    std::println (to, "");
    std::fflush (to);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLine::UsageOnErrorStream::UsageOnErrorStream
//
//  Points usage at the error stream for as long as this is alive.
//
////////////////////////////////////////////////////////////////////////////////

CommandLine::UsageOnErrorStream::UsageOnErrorStream()
{
    s_pUsageStream = stderr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLine::UsageOnErrorStream::~UsageOnErrorStream
//
//  Puts it back, and flushes on the way out so the reason the caller is
//  about to write through std::cerr cannot overtake the page.
//
////////////////////////////////////////////////////////////////////////////////

CommandLine::UsageOnErrorStream::~UsageOnErrorStream()
{
    std::fflush (stderr);

    s_pUsageStream = stdout;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLine::FlushOutput
//
//  Empties every buffer the tool writes usage through.
//
//  BOTH OF THEM. std::println writes to the C stdout FILE* and std::cout is a
//  separate C++ stream over the same descriptor, so flushing one leaves the
//  other holding a page. See the header for what that looked like.
//
////////////////////////////////////////////////////////////////////////////////

void CommandLine::FlushOutput()
{
    std::cout.flush();
    fflush (stdout);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLine::UsageWidth
//
//  How wide the reader's terminal is, or 80 when there is no terminal to ask.
//
//  A redirected stream has no width, and guessing a wide one there would put
//  long lines into a file someone will read in an editor at 80. The last column
//  is left unused: writing INTO it makes a console wrap on its own, which
//  produces a blank line between every row on some terminals.
//
////////////////////////////////////////////////////////////////////////////////

size_t CommandLine::UsageWidth()
{
    CONSOLE_SCREEN_BUFFER_INFO  info       = {};
    HANDLE                      out        = GetStdHandle (STD_OUTPUT_HANDLE);
    HANDLE                      console    = INVALID_HANDLE_VALUE;
    bool                        hasConsole = false;
    int                         columns    = 0;



    //  THE PLATFORM CALL IS ALL THIS DOES. What the numbers MEAN is decided in
    //  the library, where a test can reach it -- see UsageText::WidthFrom. This
    //  used to hold both, so "is the help folding to my terminal?" could only be
    //  answered by a person looking at one.
    //
    //  A FILE IS THE ONLY THING WITH NO WIDTH TO ASK ABOUT. It will be opened in
    //  an editor, where 200-column lines are the wrong answer, so a redirect to
    //  disk keeps the 80 fallback and asks nothing.
    bool  toFile = GetFileType (out) == FILE_TYPE_DISK;

    hasConsole = !toFile && out != nullptr && out != INVALID_HANDLE_VALUE
              && GetConsoleScreenBufferInfo (out, &info);

    //  CONOUT$ WHEN THE HANDLE ITSELF WILL NOT ANSWER, which is most of the
    //  time and was the whole bug.
    //
    //  GetConsoleScreenBufferInfo needs a READABLE console handle. A stdout that
    //  has been through a shell -- piped, or handed on by a host that sits
    //  between the terminal and the process -- is write-only or is a pipe, and
    //  the call fails on it however wide the window behind it is. Measured:
    //  under `> CON`, `| findstr` and a plain redirect alike, the stdout handle
    //  answers nothing and CONOUT$ answers 112.
    //
    //  So the terminal is asked directly. CONOUT$ opens the process's OWN
    //  console, so a tool with no console at all still gets nothing and still
    //  falls back to 80 -- and a pipe into a pager now folds to the terminal
    //  the pager is about to draw on, which is the answer that was wanted.
    if (!hasConsole && !toFile)
    {
        console = CreateFileW (L"CONOUT$", GENERIC_READ | GENERIC_WRITE,
                               FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                               OPEN_EXISTING, 0, nullptr);

        if (console != INVALID_HANDLE_VALUE)
        {
            hasConsole = GetConsoleScreenBufferInfo (console, &info) != FALSE;
            CloseHandle (console);
        }
    }

    if (hasConsole)
    {
        columns = info.srWindow.Right - info.srWindow.Left + 1;

        //  The window is the right question and the buffer is the fallback: a
        //  host that leaves srWindow empty still fills dwSize, and a zero from
        //  the first would otherwise read as a terminal too narrow to fold to.
        if (columns <= (int) UsageText::kNarrowestTerminal)
        {
            columns = info.dwSize.X;
        }
    }

    //  _dupenv_s rather than getenv, which the CRT deprecates here. The buffer
    //  is ours to free, so the width is taken before it goes.
    char   *  columnsEnv = nullptr;
    size_t    envSize    = 0;
    size_t    width      = 0;

    _dupenv_s (&columnsEnv, &envSize, "COLUMNS");
    width = UsageText::WidthFrom (columnsEnv, hasConsole, columns);
    free (columnsEnv);

    return width;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLine::PrintUsageLine
//
//  One logical line of usage, folded to the terminal. Every line of help goes
//  through here, so none of them is hand-wrapped to a width the reader may not
//  have.
//
////////////////////////////////////////////////////////////////////////////////

void CommandLine::PrintUsageLine (const std::string & line)
{
    s_pSpokenOn = s_pUsageStream;

    for (const std::string & row : UsageText::Wrap (line, UsageWidth()))
    {
        std::println (s_pUsageStream, "{}", row);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLine::PrintUsageBlock
//
//  A block of usage composed elsewhere -- core builds the dialect flag lines --
//  folded row by row. Split here rather than in core so the composing code stays
//  free of the terminal.
//
////////////////////////////////////////////////////////////////////////////////

void CommandLine::PrintUsageBlock (const std::string & block)
{
    size_t  start = 0;



    while (start <= block.size())
    {
        size_t  end = block.find ('\n', start);

        if (end == std::string::npos)
        {
            end = block.size();
        }

        // A block conventionally ends in a newline, which would otherwise print
        // as a trailing blank row that was never in the text.
        if (end == start && end == block.size())
        {
            break;
        }

        PrintUsageLine (block.substr (start, end - start));
        start = end + 1;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLine::PrintSectionHeading
//
//  A top-level heading, underlined to its own width. Written once so the four
//  sections cannot drift into three styles.
//
////////////////////////////////////////////////////////////////////////////////

void CommandLine::PrintSectionHeading (const std::string & name)
{
    std::println (s_pUsageStream, "");
    std::println (s_pUsageStream, "{}", name);
    std::println (s_pUsageStream, "{}", std::string (name.size(), '-'));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLine::PrintPageBanner
//
//  What the tool is and which build this is, above every page.
//
//  A mode's page is reached DIRECTLY -- `CassoCli as65 --help` -- so a reader
//  can meet the whole of the help without ever passing the general page. The
//  version and the architecture are exactly what a bug report needs, and a page
//  that omits them makes the reader go and ask a second question.
//
////////////////////////////////////////////////////////////////////////////////

void CommandLine::PrintPageBanner (CommandLineOptions::Subcommand mode)
{
    std::print (s_pUsageStream, "{}", BuildBanner());
    std::println (s_pUsageStream, "");
    std::println (s_pUsageStream, "Usage:");
    PrintUsageLine (CommandLineHelp::UsageLineFor (mode));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLine::PrintDialectFlags
//
//  One dialect's flags, GENERATED from the table its parser walks.
//
//  Nothing here writes a flag's letter, its value or its description: those are
//  columns of s_kAs65Flags and s_kMerlinFlags, and DialectHelp reads the same
//  rows the walk does. That is the whole reason the tables exist. The help this
//  replaced was hand-written and had drifted in five places at once -- it
//  offered a withdrawn `--raw`, called the default a padded 64KB image after
//  the default became the assembled bytes, called `-h` unimplemented after it
//  was implemented, gave `-g` a filename it does not take, and wrote `-d` with
//  a space that this grammar does not accept.
//
////////////////////////////////////////////////////////////////////////////////

void CommandLine::PrintDialectFlags (DialectId dialect, char prefix)
{
    PrintUsageBlock (DialectHelp::ComposeFlagLines (dialect, prefix));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLine::PrintExitCodes
//
//  One mode's exit codes, at the end of that mode's own page.
//
//  THEY DIFFER, WHICH IS WHY EACH PAGE CARRIES ITS OWN. An assembly error exits
//  3 under the assembler and 1 under `run`, and a shared block near the top
//  could only state one of those. The wording lives in the library beside the
//  code that assigns it, so a status changed in one place is a status whose
//  description is right there to change with it.
//
////////////////////////////////////////////////////////////////////////////////

void CommandLine::PrintExitCodes (const std::string & codes)
{
    std::println (s_pUsageStream, "");
    std::println (s_pUsageStream, "Exit codes:");
    PrintUsageBlock (codes);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLine::InstalledGigabytes
//
//  How much memory is fitted, for the one help line that mentions it.
//
//  GetPhysicallyInstalledSystemMemory READS THE FIRMWARE'S OWN TABLES, so it
//  answers 32 on a machine with 32 GB in it. GlobalMemoryStatusEx answers what
//  the kernel is allowed to hand out, which is always less: the firmware
//  reserved a slice before Windows counted. Both are rounded on the way out,
//  because neither lands on the number printed on the module.
//
//  Zero when the OS declines to answer, and the sentence says "your machine"
//  instead of inventing a size.
//
////////////////////////////////////////////////////////////////////////////////

unsigned CommandLine::InstalledGigabytes()
{
    ULONGLONG        kilobytes = 0;
    MEMORYSTATUSEX   status    = { sizeof (MEMORYSTATUSEX) };
    unsigned         answer    = 0;



    if (GetPhysicallyInstalledSystemMemory (&kilobytes))
    {
        answer = CommandLineParser::RoundToInstalledSize (kilobytes * 1024ull);
    }
    else if (GlobalMemoryStatusEx (&status))
    {
        answer = CommandLineParser::RoundToInstalledSize (status.ullTotalPhys);
    }

    return answer;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLine::PrintAssemblePage
//
//  Everything that applies while AS65 source is being assembled.
//
//  IT OPENS WITH THE COMPATIBILITY PROMISE, because that is what a reader
//  arriving from as65 needs before any individual flag makes sense, and because
//  the grammar rules below it are properties of every command line on the page
//  rather than caveats about the flags they used to sit under.
//
////////////////////////////////////////////////////////////////////////////////

void CommandLine::PrintAssemblePage (char prefix)
{
    const char *  sp = (prefix == '/') ? "/" : "-";



    PrintPageBanner (CommandLineOptions::Subcommand::As65);
    std::println (s_pUsageStream, "");
    PrintUsageLine ("  <source>   An assembly source file. Given no extension, .a65, .asm and .s are tried in that order.");

    PrintSectionHeading ("AS65 compatibility");
    PrintUsageLine ("  This assembler is an implementation of AS65 and keeps 100% compatibility with AS65's command-line patterns, so any AS65 command line assembles here unchanged behind the `as65` word.");
    std::println (s_pUsageStream, "");
    PrintUsageLine (std::format ("  Single-letter switches chain into one argument, so {0}tlfile means {0}t {0}lfile. A switch taking a NUMBER can be followed inside the group, so {0}h80t means {0}h80 {0}t. One taking a NAME cannot, because the name would swallow whatever came after it.", sp));
    std::println (s_pUsageStream, "");
    PrintUsageLine (std::format ("  A switch value attaches directly to its switch, with no space before it: {0}dDEBUG rather than {0}d DEBUG, {0}w133 rather than {0}w 133.", sp));
    std::println (s_pUsageStream, "");
    PrintUsageLine (std::format ("  {0}o is the one switch where the space before its value is optional: {0}o prog.bin is taken as readily as {0}oprog.bin.", sp));
    std::println (s_pUsageStream, "");
    //  The longest-match rule that makes `-s2` one switch is deliberately NOT
    //  here. It only matters to a reader who thinks `-2` might be a switch of
    //  its own, and nothing in this page has given them that idea -- so stating
    //  it plants the question it then answers.
    PrintUsageLine ("  A question mark with no switch character in front of it prints this page, which is AS65's own usage request. It has to be the only thing after `as65`: with anything beside it, ? is an ordinary argument.");

    PrintSectionHeading ("AS65 options");
    PrintDialectFlags (DialectId::As65, prefix);

    PrintSectionHeading ("Examples");
    PrintUsageLine (std::format ("  CassoCli as65 prog.a65 {0}x {0}dFAST=1", sp));
    PrintUsageLine ("      Assembles prog.a65 with the 65C02 opcodes available and the symbol FAST defined as 1, then writes the assembled bytes to prog.bin beside the source.");
    std::println (s_pUsageStream, "");
    PrintUsageLine (std::format ("  CassoCli as65 rom.a65 {0}orom.bin {1}flat {0}z", sp, (prefix == '/') ? "/" : "--"));
    PrintUsageLine ("      Writes rom.bin as a full 64KB image, with every byte the source did not fill set to $00 instead of $FF. That is what a ROM burner takes, and what a byte-for-byte comparison against a reference image needs.");
    std::println (s_pUsageStream, "");
    PrintUsageLine (std::format ("  CassoCli as65 prog.a65 {0}lprog.lst {0}c {0}t", sp));
    PrintUsageLine ("      Writes prog.lst alongside prog.bin: each source line with the bytes it generated and the cycles it costs, then the symbol table at the end.");

    PrintExitCodes (CommandLineParser::BuildAssembleExitCodes (InstalledGigabytes()));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLine::PrintMerlinPage
//
//  Merlin's own page. Its flags are few because its SOURCE answers most of what
//  as65 answers with a switch -- the object's name, the CPU -- so what remains
//  on the command line is what only the invocation can say.
//
//  WHAT IS COMPATIBLE HERE IS THE SOURCE LANGUAGE, not the command line. Glen
//  Bredon's Merlin is an Apple II program with an interactive editor and no host
//  command line to match, so the switches below are Casso's own and the page
//  says which subset of the LANGUAGE is supported instead.
//
////////////////////////////////////////////////////////////////////////////////

void CommandLine::PrintMerlinPage (char prefix)
{
    const char *  sp = (prefix == '/') ? "/" : "-";



    PrintPageBanner (CommandLineOptions::Subcommand::Merlin);
    std::println (s_pUsageStream, "");
    PrintUsageLine ("  <source>   A Merlin assembly source file. Given no extension, .a65, .asm and .s are tried in that order.");

    PrintSectionHeading ("Merlin directives");
    PrintUsageLine ("  Merlin uses source directives instead of cmdline switches for many options. Some important ones are:");
    std::println (s_pUsageStream, "");
    PrintUsageLine ("    XC       Select the 65C02.");
    PrintUsageLine (std::format ("    DSK      Sets the output file. {0}o overrides it.", sp));
    PrintUsageLine ("    ORG      Set the origin.");
    std::println (s_pUsageStream, "");
    PrintUsageLine ("  For more details, see docs\\Assembler.md and the Merlin documentation.");

    PrintSectionHeading ("Merlin options");
    PrintDialectFlags (DialectId::Merlin, prefix);

    PrintSectionHeading ("Supported subset");
    PrintUsageLine ("  Casso assembles the Merlin sources that produce a finished binary. Anything outside that is refused explicitly, with what it would take to support it. See docs\\merlin-subset.md.");

    PrintExitCodes (CommandLineParser::BuildAssembleExitCodes (InstalledGigabytes(), false));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLine::PrintUsage
//
//  The page the request asked for, and only that page.
//
//  ONE PAGE FOR FOUR GRAMMARS RAN TO FOUR SCREENS. Every switch of AS65, of
//  Merlin, of `run` and of `disk`, printed together whichever one the reader had
//  come for. A reader arrives already knowing which of the four things they mean
//  to do -- they typed the subcommand -- so the general page names the four and
//  says how to ask about one, and the detail waits behind that question.
//
//  `disk` HAS NO ARM HERE, and its absence is the design. Its page is answered
//  by DiskCommandRunner as the Help command of the disk grammar, beside every other
//  disk command's output, which is what lets it be built and tested next to the
//  code it describes.
//
//  EVERY PAGE IS WRITTEN WITH THE PREFIX THE READER CHOSE. `/?` means the page
//  reads `/flag` throughout, and `--help` means it reads `-`/`--`.
//
////////////////////////////////////////////////////////////////////////////////

void CommandLine::PrintUsage (const CommandLineOptions & options)
{
    char  prefix = options.flagPrefix;



    switch (options.helpPage)
    {
    case CommandLineOptions::HelpPage::Assemble:
        PrintAssemblePage (prefix);
        break;

    case CommandLineOptions::HelpPage::Merlin:
        PrintMerlinPage (prefix);
        break;

    case CommandLineOptions::HelpPage::Run:
        PrintRunPage (prefix);
        break;

    default:
        PrintUsageBlock (CommandLineHelp::BuildGeneralHelp (BuildBanner(), prefix));
        break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLine::PrintPageFor
//
//  The page belonging to one mode, chosen from the mode rather than from a
//  help request.
//
//  A REFUSAL PRINTS A PAGE TOO, and it has to be the same page a help request
//  would have printed. `disk get img.dsk A B` used to answer with two lines
//  naming the surplus argument and nothing else, so a reader who had the command's
//  operands wrong was told they were wrong and not what the right ones are. The
//  answer to "you typed this wrong" is the grammar.
//
//  The disk page is the runner's, because the disk grammar's help is composed
//  where its commands are. Everything else is one of the three above.
//
////////////////////////////////////////////////////////////////////////////////

void CommandLine::PrintPageFor (CommandLineOptions::Subcommand mode, char prefix,
                                CommandLineOptions::DiskOptions::Command diskCommand)
{
    switch (mode)
    {
    case CommandLineOptions::Subcommand::As65:
        PrintAssemblePage (prefix);
        break;

    case CommandLineOptions::Subcommand::Merlin:
        PrintMerlinPage (prefix);
        break;

    case CommandLineOptions::Subcommand::Run:
        PrintRunPage (prefix);
        break;

    case CommandLineOptions::Subcommand::Disk:
        if (diskCommand != CommandLineOptions::DiskOptions::Command::None
         && diskCommand != CommandLineOptions::DiskOptions::Command::Help)
        {
            PrintUsageBlock (DiskHelpPage::BuildCommandHelp (diskCommand, prefix));
        }
        else
        {
            PrintUsageBlock (DiskHelpPage::BuildHelpText (prefix, BuildBanner()));
        }

        break;

    default:
        PrintUsageBlock (CommandLineHelp::BuildGeneralHelp (BuildBanner(), prefix));
        break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLine::PrintRunPage
//
//  A page of its own, reached by asking `run` for help in any form. It closes
//  with the statuses `run` itself spends, which are not the assembler's: an
//  assembly error is 3 under the assembler and 1 here, because here nothing ran.
//
////////////////////////////////////////////////////////////////////////////////

void CommandLine::PrintRunPage (char prefix)
{
    const char *  sp  = (prefix == '/') ? "/"  : "-";
    const char *  lp  = (prefix == '/') ? "/"  : "--";
    const char *  pad = (prefix == '/') ? " "  : "";



    PrintPageBanner (CommandLineOptions::Subcommand::Run);
    std::println (s_pUsageStream, "");
    PrintUsageLine ("  <binary>   An assembled image to load and execute.");
    PrintUsageLine ("  <source>   An assembly source file to assemble and then execute.");

    PrintSectionHeading ("Run options");

    for (const char * fmt : s_kRunOptionLines)
    {
        PrintUsageLine (std::vformat (fmt, std::make_format_args (lp, pad)));
    }

    PrintUsageLine (std::format ("  {0}v                     Verbose output", sp));
    std::println (s_pUsageStream, "");

    //  The two dialects get a row each rather than sharing one, because what
    //  differs between them is which assembler options come along -- and that
    //  belongs beside the name that admits them.
    PrintUsageLine (std::format ("  {:<22} Assemble the source as AS65 (the default). Allows AS65 {}x and {}d.",
                                 CommandLineParser::FormatLongOption ("--as65", prefix), sp, sp));
    PrintUsageLine (std::format ("  {:<22} Assemble the source as Merlin. Allows {}d; the CPU comes from the source's XC directive.",
                                 CommandLineParser::FormatLongOption ("--merlin", prefix), sp));

    PrintSectionHeading ("Examples");
    PrintUsageLine (std::format ("  CassoCli run prog.a65 {0}stop $6010 {0}max-cycles 10000", lp));
    PrintUsageLine ("      Assembles prog.a65, loads it at $8000, and runs until the PC reaches $6010 or ten thousand cycles have passed, whichever comes first.");

    PrintExitCodes (std::string (CommandLineParser::kRunExitStatusHelpText));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLine::PrintVersion
//
////////////////////////////////////////////////////////////////////////////////

void CommandLine::PrintVersion()
{
    std::cout << "CassoCli v" VERSION_STRING " (" << s_arch << ")  " VERSION_BUILD_TIMESTAMP "\n";
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLine::BuildBanner
//
//  What the tool is and which build this is.
//
//  Returned rather than printed because the disk help is assembled in the core
//  library, which does not know this build's version, and a page reached
//  directly by `CassoCli disk --help` should still say which binary answered.
//  So the executable builds the line and the library places it.
//
////////////////////////////////////////////////////////////////////////////////

std::string CommandLine::BuildBanner()
{
    //  OPENS ON A BLANK LINE. Every page starts with this, so one here is
    //  one gap between the command the reader typed and what came back --
    //  written once rather than at the head of four pages.
    return std::string ("\nCassoCli - 6502 Assembler and Emulator  v" VERSION_STRING " (")
         + s_arch
         + ")  " VERSION_BUILD_TIMESTAMP "\n"
           "Copyright (c) 2025-" VERSION_YEAR_STRING " by Robert Elmer\n";
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLine::PrintUnrecognizedArgument
//
//  The full usage, and THEN the message. Usage is long, and what a reader sees
//  is the bottom of the screen, so the line that says what went wrong goes
//  last. `CassoCli input.a65` used to assemble, and the people it stops are
//  build scripts -- which nobody reads again until the day they fail -- so the
//  replacement is written out literally, ready to paste back.
//
//  Usage goes to stdout and the message to stderr, which is why stdout is
//  flushed between them: the order on the screen has to be the order here.
//
////////////////////////////////////////////////////////////////////////////////

void CommandLine::PrintUnrecognizedArgument (const std::string & word, char prefix)
{
    UsageOnErrorStream  toTheErrorStream;
    std::string         expected;



    PrintUsageBlock (CommandLineHelp::BuildGeneralHelp (BuildBanner(), prefix));
    std::fflush (stderr);

    std::cerr << kGapBeforeTheReason << "Error: '" << word << "' is not a mode\n";

    if (CommandLineParser::IsAssemblySource (word))
    {
        std::cerr << "       it looks like a source file; assembling states its dialect:\n"
                  << "       CassoCli as65 " << word << "\n";
    }
    else
    {
        // Swept from the table, so a subcommand added to the tool is offered
        // here without anyone remembering to add it.
        for (const CommandLineParser::SubcommandName & entry : CommandLineParser::GetAllSubcommands())
        {
            expected += std::string (expected.empty() ? "" : ", ") + entry.name;
        }

        std::cerr << "       expected one of: " << expected << "\n";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLine::PrintUnrecognizedFlag
//
//  The subcommand was fine; something after it was not. The full usage, then
//  the line naming the argument, last for the same reason as above.
//
//  THAT MODE'S PAGE, NOT THE GENERAL ONE. The general page is a table of
//  contents: it names the four modes and where each mode's flags are written
//  down, and it lists no flag of any of them. So a reader who had typed a flag
//  wrong was handed the one page in the tool that could not tell them the right
//  one. The reader has already said which grammar they are in by naming the
//  subcommand, and that grammar's flags are what they were reaching for.
//
////////////////////////////////////////////////////////////////////////////////

void CommandLine::PrintUnrecognizedFlag (const std::string & flag, CommandLineOptions::Subcommand subcommand, char prefix)
{
    UsageOnErrorStream  toTheErrorStream;



    PrintPageFor (subcommand, prefix);
    std::fflush (stderr);

    //  THE MODE IS NOT NAMED. It is on the page printed directly above and
    //  in the command the reader just typed, so "is not an option of the
    //  merlin mode" spent a clause saying what they can see -- and said it
    //  in lower case, which is not how the assembler is written.
    std::cerr << kGapBeforeTheReason << "Error: unknown option: " << flag << "\n";
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLine::PrintCpuFlagRefusal
//
//  A CPU flag the active dialect does not take.
//
//  The sentence arrives composed, because naming the in-source directive that
//  replaces the flag is the dialect's own knowledge and this is the printing
//  edge.
//
//  IT NO LONGER PICKS THE EXIT CODE. It used to return kNoOutput, reasoning
//  that a refusal produces no file and a script only asks whether it got one.
//  That reads as65's table backwards: 2 is "unable to open input or output
//  file", and a command line refused before anything is opened never touched a
//  file at all. as65 spends 1 on "incorrect parameter specified on the
//  commandline", which is what both callers of this have. An unknown flag
//  already exited 1 while a rejected flag COMBINATION exited 2, so the tool
//  disagreed with itself about the same class of mistake. The caller now maps
//  the code through ExitCodeForRefusal like every other refusal does.
//
////////////////////////////////////////////////////////////////////////////////

void CommandLine::PrintCpuFlagRefusal (const std::string & refusal)
{
    std::cerr << "Error: " << refusal << "\n";
}
