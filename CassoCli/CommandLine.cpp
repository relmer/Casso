#include "Pch.h"

#include "CommandLine.h"
#include "HostFile.h"
#include "UsageText.h"
#include "AssemblerExitCode.h"
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
//  CommandLine::PrintUsageHeader
//
////////////////////////////////////////////////////////////////////////////////

void CommandLine::PrintUsageHeader (const char * sp, const char * lp)
{
    std::string  subcommands;



    // Swept from the parser's own table rather than listed here. A subcommand
    // the tool accepts and the usage line omits is unfindable, and the fallback
    // that used to make the first word optional is gone -- so this line is now
    // the only place a reader learns that the first word is obligatory.
    for (const CommandLineParser::SubcommandName & entry : CommandLineParser::GetAllSubcommands())
    {
        subcommands += std::string (subcommands.empty() ? "" : " | ") + entry.name + " <file>";
    }

    std::println ("CassoCli - 6502 Assembler and Emulator  v" VERSION_STRING " ({})  " VERSION_BUILD_TIMESTAMP, s_arch);
    std::println ("Copyright (c) 2025-" VERSION_YEAR_STRING " by Robert Elmer");
    std::println ("");
    PrintUsageLine (std::format ("Usage:  CassoCli {} [options] | {}? | {}version", subcommands, sp, lp));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLine::PrintUsageGeneral
//
////////////////////////////////////////////////////////////////////////////////

void CommandLine::PrintUsageGeneral (const char * lp, const char * sp, const char * pad)
{
    // "--help, -?" = 10 chars, "--version" = 9 chars => +1 space for version
    // "/help, /?"  =  9 chars, "/version"  = 8 chars => +1 space for version
    // pad compensates: -- (2 chars) vs / (1 char) in long prefix
    PrintSectionHeading ("General");
    PrintUsageLine (std::format ("  Assembles AS65 or Merlin source for the 6502 and the 65C02. The subcommand names the dialect; the CPU is chosen with {0}x under AS65 and by the XC directive inside Merlin source.", sp));
    std::println ("");
    PrintUsageLine ("  See docs/Assembler.md for additional information.");
    std::println ("");
    PrintUsageLine (std::format ("  {0}help, {1}?{2}             Show this help", lp, sp, pad));
    PrintUsageLine (std::format ("  {0}version{1}              Show version information", lp, pad));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLine::PrintUsageAssembler
//
//  Prints the AS65-mode flag reference, using whichever prefix the user
//  typed.
//
//  The prefix is substituted rather than hard-coded because both `/` and `-`
//  are accepted, and usage text showing the form the reader did NOT type reads
//  as though their invocation was wrong. The flag table is a format-string
//  array for exactly that reason: one placeholder per flag, filled at print
//  time, so neither form can be forgotten when a flag is added.
//
//  The CPU and source lines sit outside the table because they take a
//  long-form or positional argument and carry no prefix to substitute.
//
////////////////////////////////////////////////////////////////////////////////

void CommandLine::PrintUsageAssembler (const char * sp)
{
    PrintSectionHeading ("AS65 mode");
    PrintUsageLine ("  <source>               Assembly source file (tries .a65, .asm, .s if no extension is given)");
    std::println ("");
    PrintUsageLine ("  AS65's command line has habits of its own, kept for compatibility:");
    PrintUsageLine (std::format ("    Single letters concatenate, with the value-taking flag last, so {0}tlfile is {0}t {0}lfile.", sp));
    PrintUsageLine (std::format ("    A value ATTACHES to its flag, {0}ofile rather than {0}o file, though {0}o and {0}l accept a separated one too.", sp));
    PrintUsageLine (std::format ("    {0}s2 is one flag, not {0}s followed by a 2.", sp));

    const char * lines[] =
    {
        //  ONE LOGICAL LINE PER ROW. The gutter between a flag and its
        //  description is what tells the wrapper where a continuation belongs,
        //  so a row is written whole and folded to the reader's terminal rather
        //  than broken here at a width nobody may have.
        "",
        "  Assembled code:",
        "    {0}o <file>            Rename output file (default: <source>.bin)",
        "    {0}n                   Disable optimizations. Not yet implemented (GitHub issue #118)",
        "",
        "  Output formats (mutually exclusive):",
        "    <default>            Write a full 64 KB image, padded with the fill byte (see {0}z below)",
        "    {1}raw                Write the assembled bytes, unpadded",
        "    {1}dos-bin            Write the assembled bytes behind a 4-byte DOS 3.3 header (load address + length), ready to BLOAD",
        "    {0}s                   Write the assembled bytes as Motorola S-records, each with its address (<source>.s19)",
        "    {0}s2                  Write the assembled bytes as Intel HEX records, each with its address (<source>.hex)",
        "",
        "    {0}z                   Fill unused space in the padded image with $00 (default: $FF)",
        "",
        "  Listing:",
        "    {0}l[<file>]           Generate listing ({0}l alone goes to stdout)",
        "    {0}p                   Generate pass 1 listing",
        "    {0}c                   Show cycle counts in listing",
        "    {0}m                   Show macro expansions in listing",
        "    {0}w[<width>]          Wrap listing at <width> columns, 60 to 200 (default: 79, {0}w alone = 133)",
        "    {0}h<lines>            Page height: a header and a form feed every <lines>, {0}h0 for no paging. Not yet implemented (GitHub issue #118)",
        "",
        "  Debug:",
        "    {0}t                   Print the symbol table to stdout, each symbol with its address in hex and decimal",
        "    {0}g <file>            Write symbol addresses to <file> as NAME=$ADDR, by address and again by name",
        "",
        "  General:",
        "    {0}d <name>[=<value>]  Define symbol (defaults to 1 if <value> is omitted)",
        "    {0}v                   Verbose: pass timings and an assembly summary, on stderr",
        "    {0}q                   Quiet mode (suppress progress)",
        "    {0}i                   Ignore case of opcodes. Always enabled in Casso, accepted for command-line compatibility with AS65",
    };

    for (const char * fmt : lines)
    {
        // {0} is the short prefix and {1} the long one, so a row naming either
        // writes it the way this invocation does.
        std::string  lp = (sp[0] == '/') ? "/" : "--";

        PrintUsageLine (std::vformat (fmt, std::make_format_args (sp, lp)));
    }

    std::println ("");
    PrintUsageLine ("  CPU:");
    PrintUsageLine ("    <default>            Assemble 6502 instructions");
    PrintUsageLine (std::format ("    {0}x                   Assemble 65C02 instructions", sp));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLine::PrintUsageRun
//
////////////////////////////////////////////////////////////////////////////////

void CommandLine::PrintUsageRun (const char * lp, const char * sp, const char * pad)
{
    PrintSectionHeading ("Run mode");
    PrintUsageLine ("  <binary>               A binary file to load and execute");
    PrintUsageLine ("  <source>               An assembly source file to assemble and run (tries .a65, .asm, .s if no extension is given)");
    std::println ("");

    // The two dialects get a row each rather than sharing one, because what
    // differs between them here is which assembler options come along -- and
    // that belongs beside the name that admits them, not in a paragraph
    // underneath that the reader has to re-split by dialect.
    PrintUsageLine (std::format ("  {:<22} Assemble the source as AS65 (the default). Allows AS65 {}x and {}d; see AS65 mode above.",
                      CommandLineParser::FormatLongOption ("--as65", sp[0]), sp, sp));
    PrintUsageLine (std::format ("  {:<22} Assemble the source as Merlin. Allows {}d; see Merlin mode above.",
                      CommandLineParser::FormatLongOption ("--merlin", sp[0]), sp));
    std::println ("");

    const char * lines[] =
    {
        "  {0}load <addr>{1}          Load address (default: $8000)",
        "  {0}entry <addr>{1}         Entry point address",
        "  {0}reset-vector{1}         Use reset vector at $FFFC/$FFFD",
        "  {0}stop <addr>{1}          Stop when PC reaches address",
        "  {0}max-cycles <n>{1}       Maximum cycles before stopping",
    };

    for (const char * fmt : lines)
    {
        PrintUsageLine (std::vformat (fmt, std::make_format_args (lp, pad)));
    }

    PrintUsageLine (std::format ("  {0}v                     Verbose output", sp));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLine::PrintUsage
//
////////////////////////////////////////////////////////////////////////////////

void CommandLine::PrintUsage (char prefix)
{
    const char * sp  = (prefix == '/') ? "/"  : "-";
    const char * lp  = (prefix == '/') ? "/"  : "--";
    const char * pad = (prefix == '/') ? " "  : "";



    PrintUsageHeader    (sp, lp);
    PrintUsageGeneral   (lp, sp, pad);
    PrintUsageAssembler (sp);
    PrintUsageMerlin    (sp, prefix);
    PrintUsageRun       (lp, sp, pad);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLine::PrintUsageMerlin
//
//  The merlin section's flag lines are composed in core from the same tables
//  the parser walks, so they cannot describe a tool that no longer exists. The
//  heading and the notes are here because they are prose about one dialect
//  rather than data any dialect supplies.
//
//  A dialect added later gets its flags printed by the same call and needs no
//  edit here; what it would not get is a section of its own, which is a note
//  for whoever adds one rather than a claim that this scales.
//
////////////////////////////////////////////////////////////////////////////////

void CommandLine::PrintUsageMerlin (const char * sp, char prefix)
{
    PrintSectionHeading ("Merlin mode");
    PrintUsageLine ("  <source>               Merlin assembly source file (tries .a65, .asm, .s if no extension is given)");
    std::println ("");
    PrintUsageLine ("  Merlin uses assembler directives in the source file in lieu of switches. Some examples are:");
    PrintUsageLine ("    XC       Select the 65C02.");
    PrintUsageLine (std::format ("    DSK      Name the output file. {0}o overrides it.", sp));
    PrintUsageLine ("    ORG      Set the origin.");
    PrintUsageBlock (DialectHelp::GetDialectFlags (DialectRegistry::Get (DialectId::Merlin), prefix));
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



    PrintUsage (prefix);
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

    PrintUsage (prefix);
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
