#include "Pch.h"

#include "CommandLine.h"
#include "HostFile.h"
#include "UsageText.h"
#include "AssemblerExitCode.h"
#include "CommandLineHelp.h"
#include "DialectHelp.h"
#include "DialectRegistry.h"
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
    "  {0}entry <addr>{1}         Entry point address",
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
    constexpr size_t            kNoTerminal = 80;
    constexpr size_t            kNarrowest  = 40;
    CONSOLE_SCREEN_BUFFER_INFO  info        = {};
    HANDLE                      out         = GetStdHandle (STD_OUTPUT_HANDLE);
    size_t                      width       = kNoTerminal;



    if (out != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo (out, &info))
    {
        int  columns = info.srWindow.Right - info.srWindow.Left + 1;

        if (columns > (int) kNarrowest)
        {
            width = (size_t) columns - 1;
        }
    }

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
    for (const std::string & row : UsageText::Wrap (line, UsageWidth()))
    {
        std::println ("{}", row);
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
    std::println ("");
    std::println ("{}", name);
    std::println ("{}", std::string (name.size(), '-'));
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
    std::print ("{}", BuildBanner());
    std::println ("");
    std::println ("Usage:");
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
//  offered a withdrawn `--raw`, called the default a padded 64 KB image after
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
    std::println ("");
    std::println ("Exit codes:");
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
    std::println ("");
    PrintUsageLine ("  <source>   An assembly source file. Given no extension, .a65, .asm and .s are tried in that order.");

    PrintSectionHeading ("AS65 compatibility");
    PrintUsageLine ("  This assembler is an implementation of AS65 and keeps 100% compatibility with AS65's command-line patterns, so any AS65 command line assembles here unchanged behind the `as65` word.");
    std::println ("");
    PrintUsageLine (std::format ("  Single-letter switches chain into one argument, so {0}tlfile means {0}t {0}lfile. A switch taking a NUMBER can be followed inside the group, so {0}h80t means {0}h80 {0}t. One taking a NAME cannot, because the name would swallow whatever came after it.", sp));
    std::println ("");
    PrintUsageLine (std::format ("  A switch value attaches directly to its switch, with no space before it: {0}dDEBUG rather than {0}d DEBUG, {0}w133 rather than {0}w 133.", sp));
    std::println ("");
    PrintUsageLine (std::format ("  {0}o is the one switch where the space before its value is optional: {0}o prog.bin is taken as readily as {0}oprog.bin.", sp));
    std::println ("");
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
    std::println ("");
    PrintUsageLine (std::format ("  CassoCli as65 rom.a65 {0}orom.bin {1}flat {0}z", sp, (prefix == '/') ? "/" : "--"));
    PrintUsageLine ("      Writes rom.bin as a full 64 KB image, with every byte the source did not fill set to $00 instead of $FF. That is what a ROM burner takes, and what a byte-for-byte comparison against a reference image needs.");
    std::println ("");
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
    std::println ("");
    PrintUsageLine ("  <source>   A Merlin assembly source file. Given no extension, .a65, .asm and .s are tried in that order.");

    PrintSectionHeading ("Merlin directives");
    PrintUsageLine ("  Merlin source answers in itself most of what a switch answers elsewhere, so these are directives written IN the source rather than options typed at the shell:");
    std::println ("");
    PrintUsageLine ("    XC       Select the 65C02.");
    PrintUsageLine (std::format ("    DSK      Name the output file. {0}o overrides it.", sp));
    PrintUsageLine ("    ORG      Set the origin.");

    PrintSectionHeading ("Merlin options");
    PrintDialectFlags (DialectId::Merlin, prefix);

    PrintSectionHeading ("Supported subset");
    PrintUsageLine ("  The absolute subset that needs no linker. Where support ends is reported by name rather than as a syntax error. See docs/merlin-subset.md.");

    PrintExitCodes (CommandLineParser::BuildAssembleExitCodes (InstalledGigabytes()));
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
//  by DiskCommandRunner as the Help verb of the disk grammar, beside every other
//  disk verb's output, which is what lets it be built and tested next to the
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
    std::println ("");
    PrintUsageLine ("  <binary>   An assembled image to load and execute.");
    PrintUsageLine ("  <source>   An assembly source file to assemble and then execute.");

    PrintSectionHeading ("Run options");

    for (const char * fmt : s_kRunOptionLines)
    {
        PrintUsageLine (std::vformat (fmt, std::make_format_args (lp, pad)));
    }

    PrintUsageLine (std::format ("  {0}v                     Verbose output", sp));
    std::println ("");

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
    return std::string ("CassoCli - 6502 Assembler and Emulator  v" VERSION_STRING " (")
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
    std::string  expected;



    PrintUsageBlock (CommandLineHelp::BuildGeneralHelp (BuildBanner(), prefix));
    std::cout.flush();

    std::cerr << "\nCassoCli: '" << word << "' is not a subcommand.\n";

    if (CommandLineParser::IsAssemblySource (word))
    {
        std::cerr << "  It looks like a source file. Assembling now names its dialect:\n"
                  << "      CassoCli as65 " << word << "\n";
    }
    else
    {
        // Swept from the table, so a subcommand added to the tool is offered
        // here without anyone remembering to add it.
        for (const CommandLineParser::SubcommandName & entry : CommandLineParser::GetAllSubcommands())
        {
            expected += std::string (expected.empty() ? "" : ", ") + entry.name;
        }

        std::cerr << "  Expected one of: " << expected << ".\n";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLine::PrintUnrecognizedFlag
//
//  The subcommand was fine; something after it was not. The full usage, then
//  the line naming the argument, last for the same reason as above.
//
////////////////////////////////////////////////////////////////////////////////

void CommandLine::PrintUnrecognizedFlag (const std::string & flag, CommandLineOptions::Subcommand subcommand, char prefix)
{
    std::string  mode = "this";



    for (const CommandLineParser::SubcommandName & entry : CommandLineParser::GetAllSubcommands())
    {
        if (entry.token == subcommand)
        {
            mode = entry.name;
            break;
        }
    }

    PrintUsageBlock (CommandLineHelp::BuildGeneralHelp (BuildBanner(), prefix));
    std::cout.flush();

    std::cerr << "\nCassoCli: '" << flag << "' is not an option of the " << mode << " subcommand.\n";
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLine::PrintCpuFlagRefusal
//
//  A CPU flag the active dialect does not take.
//
//  The sentence arrives composed, because naming the in-source directive that
//  replaces the flag is the dialect's own knowledge and this is the printing
//  edge. The exit code is the same "no output" every other way of producing
//  nothing earns: a script asks whether it got a file, and it did not.
//
////////////////////////////////////////////////////////////////////////////////

int CommandLine::PrintCpuFlagRefusal (const std::string & refusal)
{
    std::cerr << "CassoCli: " << refusal << "\n";

    return AssemblerExitCode::ToProcessCode (AssemblyExitCode::NoOutput);
}
