#include "Pch.h"

#include "CommandLine.h"
#include "UsageText.h"
#include "Assembler.h"
#include "AssemblerExitCode.h"
#include "DiagnosticFormatter.h"
#include "DialectHelp.h"
#include "DialectRegistry.h"
#include "DialectReporting.h"
#include "Cpu.h"
#include "Cpu65C02Table.h"
#include "Microcode.h"
#include "OutputFormats.h"
#include "Version.h"


#if defined(_M_X64) || defined(__x86_64__)
    static const char * arch = "x64";
#elif defined(_M_ARM64) || defined(__aarch64__)
    static const char * arch = "ARM64";
#else
    static const char * arch = "Unknown";
#endif





////////////////////////////////////////////////////////////////////////////////
//
//  ReadFileContents
//
////////////////////////////////////////////////////////////////////////////////

static HRESULT ReadFileContents (const std::string & path, std::string & contents)
{
    HRESULT             hr     = S_OK;
    std::ostringstream  ss;
    bool                isOpen = false;
    std::ifstream       file (path, std::ios::binary);
    isOpen = file.is_open();



    CBR (isOpen);

    ss << file.rdbuf();
    contents = ss.str();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteBinaryFormatFile
//
//  Opens the output file in binary mode and hands the stream to the writer for
//  the chosen format.
//
//  The three binary formats differ only in what goes INTO the stream, so the
//  file handling -- open it, check it, verify the write landed -- is written
//  once here, and each format lives in OutputFormats where tests can reach it
//  without a file at all.
//
////////////////////////////////////////////////////////////////////////////////

static HRESULT WriteBinaryFormatFile (const std::string & path,
                                     const AssemblyResult & result,
                                     CommandLineOptions::OutputFormat format,
                                     Byte fillByte)
{
    HRESULT  hr         = S_OK;
    bool     isOpen     = false;
    bool     wasWritten = false;
    std::ofstream  file (path, std::ios::binary);
    isOpen = file.is_open();



    CBR (isOpen);

    if (format == CommandLineOptions::OutputFormat::Raw)
    {
        OutputFormats::WriteRaw (result.bytes, file);
    }
    else if (format == CommandLineOptions::OutputFormat::DosBinary)
    {
        OutputFormats::WriteDosBinary (result.bytes, result.startAddress, file);
    }
    else
    {
        OutputFormats::WriteFlatImage (result.bytes, result.startAddress, fillByte, file);
    }

    wasWritten = file.good();
    CBR (wasWritten);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteSymbolFile
//
////////////////////////////////////////////////////////////////////////////////

static HRESULT WriteSymbolFile (const std::string & path, const std::unordered_map<std::string, Word> & symbols)
{
    HRESULT                                    hr         = S_OK;
    std::vector<std::pair<std::string, Word>>  sorted;
    bool                                       isOpen     = false;
    bool                                       wasWritten = false;
    std::ofstream                              file (path);
    isOpen = file.is_open();



    CBR (isOpen);

    // Sort symbols by address for deterministic output
    sorted.assign (symbols.begin(), symbols.end());

    std::sort (sorted.begin(), sorted.end(),
        [] (const auto & a, const auto & b) { return a.second < b.second; });

    for (const auto & pair : sorted)
    {
        file << std::format ("${:04X}  {}\n", pair.second, pair.first);
    }

    wasWritten = file.good();
    CBR (wasWritten);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileExists
//
////////////////////////////////////////////////////////////////////////////////

static bool FileExists (const std::string & path)
{
    std::ifstream f (path);
    return f.good();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SelectInstructionSet
//
//  Picks the assembler's target instruction table from --cpu. The default is the
//  strict 6502 table (`cpu`), so 65C02-only opcodes never assemble by accident;
//  --cpu 65c02 substitutes the CMOS 65C02 (Rockwell R65C02) table.
//
////////////////////////////////////////////////////////////////////////////////

static const Microcode * SelectInstructionSet (const CommandLineOptions & options, const Cpu & cpu)
{
    const Microcode *  set = nullptr;



    if (options.cpuTarget == CommandLineOptions::CpuTarget::M65C02)
    {
        set = GetCpu65C02InstructionSet();
    }
    else
    {
        set = cpu.GetInstructionSet();
    }

    return set;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssembleResult
//
////////////////////////////////////////////////////////////////////////////////

struct AssembleResult
{
    AssemblyResult result;
    bool           ok = false;      // default: treat an unfilled result as failure
    std::string    inputFile;
};





////////////////////////////////////////////////////////////////////////////////
//
//  BuildAssemblerOptions - build AssemblerOptions from CommandLineOptions
//
//  The dialect is carried across WITH its provenance. Both, because a dialect
//  the invocation named needs no report and one the caller merely inherited
//  does -- and the dialect alone cannot say which happened, since the default
//  is also a dialect a caller can ask for by name.
//
//  The output name goes across as the CALLER's answer, which beats any name the
//  source gives itself. Empty when no output flag was given, which is how a
//  dialect whose source names its own object gets to.
//
////////////////////////////////////////////////////////////////////////////////

static AssemblerOptions BuildAssemblerOptions (const CommandLineOptions & options)
{
    AssemblerOptions asmOptions   = {};
    asmOptions.dialect            = options.dialect;
    asmOptions.dialectSelection   = options.dialectSelection;
    asmOptions.outputFileName     = options.outputFile;
    asmOptions.fillByte           = options.fillByte;
    asmOptions.generateListing    = options.generateListing;
    asmOptions.warningMode        = options.warningMode;
    asmOptions.cycleCounts        = options.cycleCounts;
    asmOptions.macroExpansion     = options.macroExpansion;
    asmOptions.pageHeight         = options.pageHeight;
    asmOptions.pageWidth          = options.pageWidth;
    asmOptions.pass1Listing       = options.pass1Listing;
    asmOptions.symbolTable        = options.symbolTable;
    asmOptions.debugInfo          = options.debugInfo;
    asmOptions.verbose            = options.verbose;
    asmOptions.quiet              = options.quiet;
    asmOptions.disableOpt         = options.disableOpt;
    asmOptions.predefinedSymbols  = options.predefinedSymbols;

    // Carried so an assembler diagnostic naming a flag names it the way this
    // invocation used for its flags. The assembler never sees a command line.
    asmOptions.flagPrefix         = options.flagPrefix;

    return asmOptions;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssembleFile
//
//  Reads one source file and assembles it, bundling the result with the input
//  path so diagnostics can be attributed later.
//
//  Carrying the filename in the result is what lets ReportAssemblyDiagnostics
//  print `file:line: error:` without being handed the path separately -- the
//  format editors parse to jump to the offending line.
//
//  An unreadable file and a failed assembly both come back as ok == false, so
//  the caller has one failure test. They are distinguished for the USER by the
//  message, which is where the distinction actually matters.
//
//  A second instruction table is OPTIONAL and is what a dialect selecting its
//  CPU in the source switches to. Null means there is nothing to switch to, and
//  the assembler says so rather than pretending the wider set arrived -- so a
//  caller passing one table has not quietly promised two.
//
////////////////////////////////////////////////////////////////////////////////

static AssembleResult AssembleFile (const std::string & inputFile,
                                   const Microcode instructionSet[256],
                                   const Microcode extendedSet[256],
                                   const AssemblerOptions & asmOptions)
{
    HRESULT         hr          = S_OK;
    AssembleResult  ar          = {};
    std::string     source;
    bool            canSwitch   = extendedSet != nullptr;

    ar.inputFile = inputFile;



    hr = ReadFileContents (inputFile, source);

    if (FAILED (hr))
    {
        std::cerr << "Error: Cannot read input file: " << inputFile << "\n";
        ar.ok = false;
    }
    else if (canSwitch)
    {
        Assembler  asm6502 (instructionSet, extendedSet, asmOptions);

        ar.result = asm6502.Assemble (source);
        ar.ok     = ar.result.success;
    }
    else
    {
        Assembler  asm6502 (instructionSet, asmOptions);

        ar.result = asm6502.Assemble (source);
        ar.ok     = ar.result.success;
    }

    return ar;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReportAssemblyDiagnostics
//
////////////////////////////////////////////////////////////////////////////////

static void ReportAssemblyDiagnostics (const AssembleResult & ar)
{
    // The input path is the FALLBACK, not the answer. A diagnostic that carries
    // its own file names that file -- which is how an error inside an included
    // file stops being attributed to the top-level source.
    for (const auto & w : ar.result.warnings)
    {
        std::println (stderr, "{}", DiagnosticFormatter::Format (w, ar.inputFile, DiagnosticSeverity::Warning));
    }

    for (const auto & e : ar.result.errors)
    {
        std::println (stderr, "{}", DiagnosticFormatter::Format (e, ar.inputFile, DiagnosticSeverity::Error));
    }

    if (!ar.ok)
    {
        std::println (stderr, "Assembly failed with {} error(s)", ar.result.errors.size());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BuildCpuReport
//
//  What to call the instruction set the assembly ran on, and what chose it.
//
//  The NAME comes from here because the tables come from here. Core's assembler
//  receives instruction sets as unnamed tables, so only the caller that handed
//  them over knows which processors they are.
//
//  The source's own selection outranks the flag's absence, and both outrank the
//  default. That order matters: a run whose source selected the wider set and
//  whose command line said nothing would otherwise report the narrow default,
//  which is worse than saying nothing at all.
//
////////////////////////////////////////////////////////////////////////////////

static CpuReport BuildCpuReport (const CommandLineOptions & options, const AssemblyResult & result)
{
    CpuReport  report;
    bool       isCmos = options.cpuTarget == CommandLineOptions::CpuTarget::M65C02;



    report.name      = isCmos ? "65c02" : "6502";
    report.selection = options.hasCpuTarget ? CpuSelection::StatedOnCommandLine
                                            : CpuSelection::DialectDefault;

    if (result.extendedSetSelectedInSource)
    {
        report.name      = "65c02";
        report.selection = CpuSelection::SelectedInSource;
    }

    return report;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReportToStandardError
//
//  Prints the reports due on stderr, and only those.
//
//  Which sink a report belongs to is decided in core; this walks the list and
//  prints the ones addressed here. Nothing selects stdout, and nothing may:
//  stdout carries the listing when no listing file is named, and a line printed
//  there lands inside the artifact a build script is piping.
//
////////////////////////////////////////////////////////////////////////////////

static void ReportToStandardError (const std::vector<DialectReportLine> & reports)
{
    for (const DialectReportLine & report : reports)
    {
        if (report.sink == ReportSink::StandardError)
        {
            std::cerr << report.text << "\n";
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ResolveOutputFormat
//
//  Decides which format to write.
//
//  An explicit format flag WINS. Extension matching remains, but only as the
//  fallback when no flag was given, which is what keeps as65-era build scripts
//  -- which name a .s19 or .hex output and pass no flag -- working unchanged.
//
//  Deriving purely from the extension, as this used to, meant `-s -o out.dat`
//  silently wrote a flat binary: the flag said S-record and the extension won
//  anyway. It also leaves the two new formats unreachable, since neither raw
//  nor DOS-binary output has an extension of its own to be recognized by.
//
////////////////////////////////////////////////////////////////////////////////

static CommandLineOptions::OutputFormat ResolveOutputFormat (const CommandLineOptions & options)
{
    CommandLineOptions::OutputFormat  format     = options.outputFormat;
    bool                              isDefault  = format == CommandLineOptions::OutputFormat::Binary;
    bool                              isSRec     = CommandLineParser::EndsWith (options.outputFile, ".s19");
    bool                              isHex      = CommandLineParser::EndsWith (options.outputFile, ".hex");



    if (isDefault && isSRec)
    {
        format = CommandLineOptions::OutputFormat::SRecord;
    }
    else if (isDefault && isHex)
    {
        format = CommandLineOptions::OutputFormat::IntelHex;
    }

    return format;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteBinaryOutput
//
//  Writes the assembled image in the resolved format.
//
//  "nul" is the explicit bit bucket and is matched case-insensitively, since
//  it is a Windows device name that scripts write every way. Writing nothing
//  is SUCCESS on that path: it is how a caller asks for diagnostics only, and
//  reporting failure would break a build that deliberately discards output.
//
//  The text formats open the stream in text mode and the binary formats in
//  binary mode, which is the only reason this splits in two rather than
//  handing every format to one writer.
//
//  A DOS binary carries its length in 16 bits, so a span of exactly 64 KB is
//  refused here rather than written as a file claiming to be empty.
//
//  The failure diagnostic is emitted once for every format. It used to be
//  written out at four separate sites, which is three opportunities for the
//  wording to drift apart.
//
////////////////////////////////////////////////////////////////////////////////

static HRESULT WriteBinaryOutput (const AssemblyResult & result,
                                  const CommandLineOptions & options)
{
    HRESULT                           hr        = S_OK;
    CommandLineOptions::OutputFormat  format    = ResolveOutputFormat (options);
    std::string                       outLower  = options.outputFile;
    size_t                            spanBytes = result.bytes.size();
    bool                              isNul     = false;
    bool                              isText    = false;
    bool                              isOpen    = false;
    bool                              fitsDos   = true;
    bool                              reported  = false;



    for (auto & c : outLower)
    {
        c = (char) tolower ((unsigned char) c);
    }

    // "nul" is the explicit bit bucket: nothing written, and that is success.
    isNul  = outLower == "nul";
    isText = format == CommandLineOptions::OutputFormat::SRecord ||
             format == CommandLineOptions::OutputFormat::IntelHex;

    if (format == CommandLineOptions::OutputFormat::DosBinary)
    {
        fitsDos = spanBytes <= OutputFormats::kMaxDosBinaryLength;

        if (!fitsDos)
        {
            std::println (stderr,
                          "Error: {} bytes is too large for a DOS 3.3 binary (limit {})",
                          spanBytes,
                          OutputFormats::kMaxDosBinaryLength);
            reported = true;
        }
    }

    CBR (fitsDos);

    if (!isNul && isText)
    {
        std::ofstream  outFile (options.outputFile);

        isOpen = outFile.is_open();
        CBR (isOpen);

        if (format == CommandLineOptions::OutputFormat::SRecord)
        {
            OutputFormats::WriteSRecord (result.bytes, result.startAddress, result.endAddress, result.startAddress, outFile);
        }
        else
        {
            OutputFormats::WriteIntelHex (result.bytes, result.startAddress, result.endAddress, result.startAddress, outFile);
        }
    }
    else if (!isNul)
    {
        hr = WriteBinaryFormatFile (options.outputFile, result, format, options.fillByte);
        CHR (hr);
    }

Error:
    // One diagnostic for every path -- it was written out identically at four
    // sites before, which is three chances for the wording to drift. Suppressed
    // when the failure already explained itself, so one problem never reports
    // twice.
    if (FAILED (hr) && !reported)
    {
        std::cerr << "Error: Cannot write output file: " << options.outputFile << "\n";
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteListingOutput
//
//  Emits the assembly listing, to a file when one was named and to stdout
//  otherwise.
//
//  Defaulting to stdout is what makes `casso -l` pipeable, and it cannot fail
//  to open -- hence failure is only possible in the named-file case.
//
//  Page breaks are driven from the SOURCE TEXT rather than from a parsed
//  directive, because the listing is a faithful rendering of the input: a
//  `.page` line is reproduced where it appeared and emits a form feed plus a
//  repeated title, matching what a period assembler sent to a line printer.
//  The three forms tested are the ones the assembler itself accepts.
//
////////////////////////////////////////////////////////////////////////////////

static HRESULT WriteListingOutput (const AssemblyResult & result,
                                   const CommandLineOptions & options,
                                   const std::vector<DialectReportLine> & reports)
{
    HRESULT         hr      = S_OK;
    std::ostream *  listOut = &std::cout;
    std::ofstream   listFile;
    bool            isOpen  = false;



    // No listing file named means the listing goes to stdout, which cannot
    // fail to open.
    if (!options.listingFile.empty())
    {
        listFile.open (options.listingFile);
        isOpen = listFile.is_open();

        if (!isOpen)
        {
            std::cerr << "Error: Cannot write listing file: " << options.listingFile << "\n";
        }

        CBR (isOpen);

        listOut = &listFile;
    }

    // The dialect and CPU in effect belong INSIDE the listing rather than on a
    // line beside it, so a reader of the listing finds them where a header
    // belongs -- which is also what keeps them off stdout when the listing is
    // being piped from there.
    for (const DialectReportLine & report : reports)
    {
        if (report.sink == ReportSink::ListingHeader)
        {
            *listOut << report.text << "\n\n";
        }
    }

    if (!result.listingTitle.empty())
    {
        *listOut << result.listingTitle << "\n\n";
    }

    for (const auto & line : result.listing)
    {
        if (line.sourceText.find (".page") != std::string::npos ||
            line.sourceText.find (".PAGE") != std::string::npos ||
            line.sourceText.find ("page") == 0)
        {
            *listOut << "\f";

            if (!result.listingTitle.empty())
            {
                *listOut << result.listingTitle << "\n\n";
            }
        }

        for (const std::string & row : Assembler::FormatListingRows (line, options.cycleCounts, options.pageWidth))
        {
            *listOut << row << "\n";
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteSymbolTableOutput
//
////////////////////////////////////////////////////////////////////////////////

static void WriteSymbolTableOutput (const AssemblyResult & result)
{
    std::cout << "\nSymbol Table:\n";
    std::cout << Assembler::FormatSymbolTable (result.symbols, result.symbolKinds);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteDebugInfoOutput
//
////////////////////////////////////////////////////////////////////////////////

static HRESULT WriteDebugInfoOutput (const AssemblyResult & result,
                                     const std::string & debugFile)
{
    HRESULT  hr     = S_OK;
    bool     isOpen = false;
    std::ofstream  dbgFile (debugFile);
    isOpen = dbgFile.is_open();



    if (!isOpen)
    {
        std::cerr << "Error: Cannot write debug file: " << debugFile << "\n";
    }

    CBR (isOpen);

    dbgFile << Assembler::FormatDebugInfo (result.symbols);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadAssembledIntoMemory
//
////////////////////////////////////////////////////////////////////////////////

static void LoadAssembledIntoMemory (Cpu & cpu, const AssemblyResult & result)
{
    Word loadAddr = result.startAddress;



    for (size_t i = 0; i < result.bytes.size(); i++)
    {
        cpu.PokeByte (loadAddr + (Word) i, result.bytes[i]);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadBinaryFileIntoMemory
//
////////////////////////////////////////////////////////////////////////////////

static HRESULT LoadBinaryFileIntoMemory (Cpu & cpu,
                                         const std::string & inputFile,
                                         Word loadAddr,
                                         Word & entryPoint)
{
    HRESULT      hr = S_OK;
    std::string  contents;
    size_t       i  = 0;



    hr = ReadFileContents (inputFile, contents);

    if (FAILED (hr))
    {
        std::cerr << "Error: Cannot read input file: " << inputFile << "\n";
    }

    CHR (hr);

    for (i = 0; i < contents.size(); i++)
    {
        cpu.PokeByte (loadAddr + (Word) i, (Byte) contents[i]);
    }

    entryPoint = loadAddr;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunCpu
//
//  Executes the loaded image from the entry point until something stops it,
//  then reports final register state.
//
//  Every exit is BOUNDED, which is what makes this safe to run unattended in a
//  build or a test script: a cycle limit, an explicit stop address, or an
//  illegal opcode. A 6502 program with no halt instruction would otherwise
//  loop forever, and the CLI has no user at the keyboard to interrupt it.
//
//  An illegal opcode exits with a distinct code rather than merely stopping,
//  so a script can tell "ran off into data" from "reached the stop address".
//
//  The cycle counter counts INSTRUCTIONS, not machine cycles -- it is a
//  runaway guard, not a timing model, and the CLI has no clock to be faithful
//  to.
//
//  Status lines are accumulated into the caller's vector instead of printed,
//  so the caller decides whether they belong on stdout, in quiet mode, or
//  interleaved with other output.
//
////////////////////////////////////////////////////////////////////////////////

static int RunCpu (Cpu & cpu,
                   const CommandLineOptions & options,
                   Word entryPoint,
                   std::vector<std::string> & status)
{
    uint32_t cycles   = 0;
    int      exitCode = 0;



    cpu.SetPC (entryPoint);
    status.push_back (std::format ("Executing from ${:04X}", entryPoint));




    for (;;)
    {
        Byte  opcode = 0;

        if (options.maxCycles > 0 && cycles >= options.maxCycles)
        {
            status.push_back (std::format ("Stopped: cycle limit reached ({})", options.maxCycles));
            break;
        }

        opcode = cpu.PeekByte (cpu.GetPC());

        if (!cpu.GetMicrocode (opcode).isLegal)
        {
            std::println (stderr, "Illegal opcode ${:02X} at ${:04X}", opcode, cpu.GetPC());
            exitCode = 3;
            break;
        }

        if (options.hasStopAddress && cpu.GetPC() == options.stopAddress)
        {
            status.push_back (std::format ("Stopped at address ${:04X}", options.stopAddress));
            break;
        }

        cpu.StepOne();
        cycles++;
    }

    status.push_back (std::format ("Execution complete: {} cycle(s)", cycles));
    status.push_back (std::format ("  A=${:02X} X=${:02X} Y=${:02X} SP=${:02X} PC=${:04X}",
        cpu.GetA(), cpu.GetX(), cpu.GetY(), cpu.GetSP(), cpu.GetPC()));

    return exitCode;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ParseCommandLine
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

CommandLineOptions ParseCommandLine (int argc, char * argv[])
{
    return CommandLineParser::Parse (argc, argv, FileExists);
}





////////////////////////////////////////////////////////////////////////////////
//
//  UsageWidth
//
//  How wide the reader's terminal is, or 80 when there is no terminal to ask.
//
//  A redirected stream has no width, and guessing a wide one there would put
//  long lines into a file someone will read in an editor at 80. The last column
//  is left unused: writing INTO it makes a console wrap on its own, which
//  produces a blank line between every row on some terminals.
//
////////////////////////////////////////////////////////////////////////////////

static size_t UsageWidth()
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
//  Say
//
//  One logical line of usage, folded to the terminal. Every line of help goes
//  through here, so none of them is hand-wrapped to a width the reader may not
//  have.
//
////////////////////////////////////////////////////////////////////////////////

static void Say (const std::string & line)
{
    for (const std::string & row : UsageText::Wrap (line, UsageWidth()))
    {
        std::println ("{}", row);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SayBlock
//
//  A block of usage composed elsewhere -- core builds the dialect flag lines --
//  folded row by row. Split here rather than in core so the composing code stays
//  free of the terminal.
//
////////////////////////////////////////////////////////////////////////////////

static void SayBlock (const std::string & block)
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

        Say (block.substr (start, end - start));
        start = end + 1;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrintSectionHeading
//
//  A top-level heading, underlined to its own width. Written once so the four
//  sections cannot drift into three styles.
//
////////////////////////////////////////////////////////////////////////////////

static void PrintSectionHeading (const std::string & name)
{
    std::println ("");
    std::println ("{}", name);
    std::println ("{}", std::string (name.size(), '-'));
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrintUsageHeader
//
////////////////////////////////////////////////////////////////////////////////

static void PrintUsageHeader (const char * sp, const char * lp)
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

    std::println ("CassoCli - 6502 Assembler and Emulator  v" VERSION_STRING " ({})  " VERSION_BUILD_TIMESTAMP, arch);
    std::println ("Copyright (c) 2025-" VERSION_YEAR_STRING " by Robert Elmer");
    std::println ("");
    Say (std::format ("Usage:  CassoCli {} [options] | {}? | {}version", subcommands, sp, lp));
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrintUsageGeneral
//
////////////////////////////////////////////////////////////////////////////////

static void PrintUsageGeneral (const char * lp, const char * sp, const char * pad)
{
    // "--help, -?" = 10 chars, "--version" = 9 chars => +1 space for version
    // "/help, /?"  =  9 chars, "/version"  = 8 chars => +1 space for version
    // pad compensates: -- (2 chars) vs / (1 char) in long prefix
    PrintSectionHeading ("General");
    Say (std::format ("  Assembles AS65 or Merlin source for the 6502 and the 65C02. The subcommand names the dialect; the CPU is chosen with {0}x under AS65 and by the XC directive inside Merlin source.", sp));
    std::println ("");
    Say ("  See docs/Assembler.md for additional information.");
    std::println ("");
    Say (std::format ("  {0}help, {1}?{2}             Show this help", lp, sp, pad));
    Say (std::format ("  {0}version{1}              Show version information", lp, pad));
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrintUsageAssembler
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
//  The `--cpu` and source lines sit outside the table because they take a
//  long-form or positional argument and carry no prefix to substitute.
//
////////////////////////////////////////////////////////////////////////////////

static void PrintUsageAssembler (const char * sp)
{
    PrintSectionHeading ("AS65 mode");
    Say ("  <source>               Assembly source file (tries .a65, .asm, .s if no extension is given)");
    std::println ("");
    Say ("  AS65's command line has habits of its own, kept for compatibility:");
    Say (std::format ("    Single letters concatenate, with the value-taking flag last, so {0}tlfile is {0}t {0}lfile.", sp));
    Say (std::format ("    A value ATTACHES to its flag -- {0}ofile rather than {0}o file -- though {0}o and {0}l accept a separated one too.", sp));
    Say (std::format ("    {0}s2 is one flag, not {0}s followed by a 2.", sp));

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
        "    Output formats. Mutually exclusive: naming two is refused rather than resolved, and the output file's extension is consulted only when none is given.",
        "    <default>            Write a full 64 KB image, padded with the fill byte ({0}z sets it)",
        "    {0}s                   Write Motorola S-record (<source>.s19)",
        "    {0}s2                  Write Intel HEX (<source>.hex)",
        "    {1}dos-bin            Write the bytes behind a 4-byte DOS 3.3 header (load address + length), ready to BLOAD",
        "    {1}raw                Write only the assembled bytes, unpadded",
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

        Say (std::vformat (fmt, std::make_format_args (sp, lp)));
    }

    std::println ("");
    Say ("  CPU:");
    Say (std::format ("    {0}x                   Assemble 65C02 instructions. Omit it for the plain 6502, where they are an assembly error", sp));
    std::println ("");
    Say ("    This is wider than AS65, which assembles the 65SC02 subset: Casso also takes RMBn, SMBn, BBRn and BBSn. A source using those assembles here and would not under AS65.");
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrintUsageRun
//
////////////////////////////////////////////////////////////////////////////////

static void PrintUsageRun (const char * lp, const char * sp, const char * pad)
{
    PrintSectionHeading ("Run mode");
    Say ("  <binary>               A binary file to load and execute");
    Say ("  <source>               An assembly source file to assemble and run (tries .a65, .asm, .s if no extension is given)");
    std::println ("");

    // The two dialects get a row each rather than sharing one, because what
    // differs between them here is which assembler options come along -- and
    // that belongs beside the name that admits them, not in a paragraph
    // underneath that the reader has to re-split by dialect.
    Say (std::format ("  {:<22} Assemble the source as AS65 (the default). Takes {}x and {}d as well; see AS65 mode above.",
                      CommandLineParser::FormatLongOption ("--as65", sp[0]), sp, sp));
    Say (std::format ("  {:<22} Assemble the source as Merlin. Takes {}d as well; see Merlin mode above.",
                      CommandLineParser::FormatLongOption ("--merlin", sp[0]), sp));
    std::println ("");
    Say ("  Both are ignored for a binary, which needs no assembler. The assembler's remaining options describe output files that run does not write.");
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
        Say (std::vformat (fmt, std::make_format_args (lp, pad)));
    }

    Say (std::format ("  {0}v                     Verbose output", sp));
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrintUsage
//
////////////////////////////////////////////////////////////////////////////////

void PrintUsage (char prefix)
{
    const char * sp  = (prefix == '/') ? "/"  : "-";
    const char * lp  = (prefix == '/') ? "/"  : "--";
    const char * pad = (prefix == '/') ? " "  : "";



    PrintUsageHeader    (sp, lp);
    PrintUsageGeneral   (lp, sp, pad);
    PrintUsageAssembler (sp);

    // The merlin section's flag lines are composed in core from the same tables
    // the parser walks, so they cannot describe a tool that no longer exists.
    // The heading and the notes are here because they are prose about one
    // dialect rather than data any dialect supplies.
    //
    // A dialect added later gets its flags printed by the same call and needs no
    // edit here; what it would not get is a section of its own, which is a note
    // for whoever adds one rather than a claim that this scales.
    PrintSectionHeading ("Merlin mode");
    Say ("  <source>               Merlin assembly source file (tries .a65, .asm, .s if no extension is given)");
    std::println ("");
    Say ("  Merlin uses assembler directives in the source file in lieu of switches. Some examples are:");
    Say ("    XC       Select the 65C02.");
    Say (std::format ("    DSK      Name the output file. {0}o overrides it.", sp));
    Say ("    ORG      Set the origin.");
    SayBlock (DialectHelp::GetDialectFlags (DialectRegistry::Get (DialectId::Merlin), prefix));

    PrintUsageRun       (lp, sp, pad);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrintVersion
//
////////////////////////////////////////////////////////////////////////////////

void PrintVersion()
{
    std::cout << "CassoCli v" VERSION_STRING " (" << arch << ")  " VERSION_BUILD_TIMESTAMP "\n";
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrintUnrecognizedArgument
//
//  Deliberately NOT the usage block. `CassoCli input.a65` used to assemble, and
//  the people it stops are build scripts -- which nobody reads again until the
//  day they fail. A wall of usage text makes that day start with a bisect,
//  where one line naming the replacement ends it. So the replacement is written
//  out literally, ready to paste back into the script that broke.
//
////////////////////////////////////////////////////////////////////////////////

void PrintUnrecognizedArgument (const std::string & word)
{
    std::string  expected;



    std::cerr << "CassoCli: '" << word << "' is not a subcommand.\n";

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
//  DoRun
//
//  The `run` subcommand: get an image into memory -- assembling it first if
//  the input is source -- pick an entry point, and execute.
//
//  Accepting either source or a binary is what makes this usable as a one-step
//  test harness: `casso run foo.a65` assembles and runs without an
//  intermediate file, while the same command on a .bin runs a prebuilt image.
//  The choice is made from the input's extension, not from a flag.
//
//  Exit codes are meaningful and distinct, because scripts branch on them:
//
//    0  ran to a normal stop
//    1  the tools ran and said no (assembly errors)
//    2  could not even start (no input, unreadable file)
//    3  from RunCpu -- an illegal opcode
//
//  Entry point resolution has three tiers, most-explicit first: an explicit
//  --entry, then the RESET vector at $FFFC when asked for, then the assembled
//  start address (or the load address for a binary). Reading the reset vector
//  is what lets a ROM image boot the way the hardware would rather than from
//  wherever its bytes happen to begin.
//
//  Status lines are collected throughout and printed only under --verbose, so
//  the default run stays quiet enough to pipe.
//
////////////////////////////////////////////////////////////////////////////////

int DoRun (const CommandLineOptions & options)
{
    HRESULT                   hr         = S_OK;
    Cpu                       cpu;
    Word                      entryPoint = 0x8000;
    Word                      loadAddr   = 0;
    std::vector<std::string>  status;
    int                       exitCode   = 0;
    bool                      wasLoaded  = false;



    // 2 = "cannot even start" (no input, unreadable file); 1 = "ran the tools
    // and they said no" (assembly errors).
    exitCode = options.inputFile.empty() ? 2 : 0;

    if (options.inputFile.empty())
    {
        std::cerr << "Error: No input file specified\n";
    }

    BAIL_OUT_IF (options.inputFile.empty(), S_OK);

    cpu.Reset();

    if (CommandLineParser::IsAssemblySource (options.inputFile))
    {
        AssemblerOptions  asmOptions = {};
        AssembleResult    ar;

        asmOptions.warningMode      = options.warningMode;

        // Which assembler reads the source, from --as65 / --merlin. Carried with
        // its provenance for the same reason the subcommands carry it: a dialect
        // the caller named needs no report, one it inherited does.
        asmOptions.dialect           = options.dialect;
        asmOptions.dialectSelection  = options.dialectSelection;
        asmOptions.flagPrefix        = options.flagPrefix;
        asmOptions.predefinedSymbols = options.predefinedSymbols;

        ar = AssembleFile (options.inputFile, SelectInstructionSet (options, cpu), nullptr, asmOptions);
        ReportAssemblyDiagnostics (ar);

        wasLoaded = ar.ok;
        exitCode  = ar.ok ? 0 : 1;

        if (wasLoaded)
        {
            LoadAssembledIntoMemory (cpu, ar.result);
            entryPoint = ar.result.startAddress;

            status.push_back (std::format ("Assembling: {}", options.inputFile));
            status.push_back (std::format ("Assembled {} bytes", ar.result.bytes.size()));
            status.push_back (std::format ("  Start: ${:04X}", ar.result.startAddress));
        }
    }
    else
    {
        loadAddr  = options.hasLoadAddress ? options.loadAddress : (Word) 0x8000;
        hr        = LoadBinaryFileIntoMemory (cpu, options.inputFile, loadAddr, entryPoint);
        wasLoaded = SUCCEEDED (hr);
        exitCode  = wasLoaded ? 0 : 2;

        if (wasLoaded)
        {
            status.push_back (std::format ("Loaded binary at ${:04X}", loadAddr));
        }
    }

    BAIL_OUT_IF (!wasLoaded, S_OK);

    if (options.hasEntryAddress)
    {
        entryPoint = options.entryAddress;
    }
    else if (options.useResetVector)
    {
        entryPoint = cpu.PeekWord (0xFFFC);
    }

    exitCode = RunCpu (cpu, options, entryPoint, status);

    if (options.verbose)
    {
        for (const auto & msg : status)
        {
            std::cerr << msg << "\n";
        }
    }

Error:
    return exitCode;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DoAs65
//
//  AS65-compatible assembly: assemble once, then emit every artifact the flags
//  asked for -- listing, binary, symbol table, debug info, symbol file.
//
//  Exit codes follow as65, because build scripts written against it test them:
//
//    0  clean
//    1  assembled, but warned
//    2  produced no output (no input, assembly errors, or a failed write)
//
//  The warning code is applied LAST, after every write has succeeded, so a
//  warning never masks a real output failure.
//
//  The assembler's base directory is taken from the input file's own path, so
//  a `.include` resolves relative to the source that names it rather than to
//  the shell's working directory -- which is what makes a build work the same
//  from any directory.
//
//  Each artifact is optional but fails identically, so each step reduces to a
//  "not requested, or written successfully" test with a single shared exit
//  code. The alternative -- a distinct code per artifact -- would tell a script
//  which file failed while breaking every script that already knows 2 means
//  "no output".
//
//  The two verbose Pass 1 / Pass 2 lines are cosmetic. Assemble runs both
//  passes internally, so they bracket the single call rather than marking real
//  boundaries; the timing figure spans both.
//
////////////////////////////////////////////////////////////////////////////////

int DoAs65 (const CommandLineOptions & options)
{
    using Clock = std::chrono::high_resolution_clock;



    HRESULT                         hr          = S_OK;
    std::vector<std::string>        status;
    AssemblerOptions                asmOptions;
    DefaultFileReader               fileReader;
    Cpu                             cpu;
    AssembleResult                  ar;
    std::vector<DialectReportLine>  reports;
    Clock::time_point               startTime;
    Clock::time_point               endTime;
    size_t                          lastSep     = 0;
    int                             exitCode    = 0;
    bool                            hasInput    = !options.inputFile.empty();
    bool                            hasWarnings = false;
    bool                            wasWritten  = false;



    // 2 is the AS65 "could not produce output" code, used by every failure
    // below; 1 means it assembled but warned.
    if (!hasInput)
    {
        std::cerr << "Error: No input file specified\n";
        exitCode = 2;
    }

    BAIL_OUT_IF (!hasInput, S_OK);

    asmOptions            = BuildAssemblerOptions (options);
    asmOptions.fileReader = &fileReader;

    // Extract base directory from input file
    lastSep = options.inputFile.find_last_of ("/\\");

    if (lastSep != std::string::npos)
    {
        asmOptions.baseDir = options.inputFile.substr (0, lastSep);
    }

    if (options.verbose)
    {
        std::cerr << "Pass 1...\n";
    }

    cpu.Reset();

    startTime = Clock::now();
    ar        = AssembleFile (options.inputFile, SelectInstructionSet (options, cpu), nullptr, asmOptions);
    endTime   = Clock::now();

    if (options.verbose)
    {
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds> (endTime - startTime);
        std::cerr << "Pass 2...\n";
        std::println (stderr, "Assembly time: {} us", elapsed.count());
    }

    hasWarnings = !ar.result.warnings.empty();
    ReportAssemblyDiagnostics (ar);

    //  What is worth saying about the dialect and the CPU, and where it may be
    //  said, is decided in core. This prints what comes back and chooses
    //  nothing: several of those cases report nothing at all, and a caller
    //  reimplementing the rule is a caller that ends up printing always.
    reports = DialectReporting::BuildReport (asmOptions, BuildCpuReport (options, ar.result));
    ReportToStandardError (reports);

    exitCode = ar.ok ? 0 : 2;

    BAIL_OUT_IF (!ar.ok, S_OK);

    if (!options.quiet)
    {
        //  linesAssembled, NOT listing.size(): the listing is only built when
        //  one was requested, so this reported "0 lines assembled" for every
        //  ordinary invocation -- of a file it had just assembled correctly.
        std::cerr << ar.result.linesAssembled << " lines assembled\n";
    }

    // Each output artifact is optional but fails the run the same way, so the
    // exit code is set once and each step just reports whether it wrote.
    hr         = options.generateListing ? WriteListingOutput (ar.result, options, reports) : S_OK;
    wasWritten = SUCCEEDED (hr);
    exitCode   = wasWritten ? 0 : 2;

    BAIL_OUT_IF (!wasWritten, S_OK);

    hr         = WriteBinaryOutput (ar.result, options);
    wasWritten = SUCCEEDED (hr);
    exitCode   = wasWritten ? 0 : 2;

    BAIL_OUT_IF (!wasWritten, S_OK);

    if (options.symbolTable)
    {
        WriteSymbolTableOutput (ar.result);
    }

    hr         = (!options.debugInfo || options.debugFile.empty())
                     ? S_OK
                     : WriteDebugInfoOutput (ar.result, options.debugFile);
    wasWritten = SUCCEEDED (hr);
    exitCode   = wasWritten ? 0 : 2;

    BAIL_OUT_IF (!wasWritten, S_OK);

    hr         = options.symbolFile.empty()
                     ? S_OK
                     : WriteSymbolFile (options.symbolFile, ar.result.symbols);
    wasWritten = SUCCEEDED (hr);
    exitCode   = wasWritten ? 0 : 2;

    if (!wasWritten)
    {
        std::cerr << "Error: Cannot write symbol file: " << options.symbolFile << "\n";
    }

    BAIL_OUT_IF (!wasWritten, S_OK);

    if (options.verbose)
    {
        std::cerr << "Assembly successful\n";
        std::println (stderr, "  Output:  {}", options.outputFile);
        std::println (stderr, "  Bytes:   {}", ar.result.bytes.size());
        std::println (stderr, "  Start:   ${:04X}", ar.result.startAddress);
        std::println (stderr, "  End:     ${:04X}", ar.result.endAddress);
        std::println (stderr, "  Symbols: {}", ar.result.symbols.size());
    }

    exitCode = hasWarnings ? 1 : 0;

Error:
    return exitCode;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ResolveMerlinOutputName
//
//  What the object is called, once the flag and the source have both had their
//  say.
//
//  The precedence itself is NOT decided here: the assembler was handed the
//  caller's answer and reports the one in effect, so a name coming back is
//  already the winner. What is left is the case neither answered -- no flag and
//  no directive -- where the source's own name is the only thing to derive from.
//
////////////////////////////////////////////////////////////////////////////////

static std::string ResolveMerlinOutputName (const CommandLineOptions & options, const AssemblyResult & result)
{
    std::string  name   = result.outputFileName;
    bool         wasSet = !name.empty();



    if (!wasSet)
    {
        name = CommandLineParser::StripExtension (options.inputFile) + ".bin";
    }

    return name;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DoMerlin
//
//  The `merlin` subcommand: assemble one Merlin source and write its object.
//
//  Two instruction tables are handed over, because Merlin selects its CPU in the
//  source and a provider with nothing to switch to would leave such a source
//  told it had reached the wider processor while the assembler stayed on the
//  narrow one. It is also why there is no CPU flag: the source decides, and a
//  flag accepted here would assemble source the real assembler rejects.
//
//  The object is the assembled stream and nothing else. Merlin's origin
//  relocates rather than seeks -- one contiguous object may carry sections
//  destined for three different addresses -- so the as65 default of a full
//  address-indexed image would scatter it across memory.
//
//  Exit codes are the tool's existing vocabulary, computed in core: 0 clean,
//  1 assembled with complaints, 2 no output. A construct outside the supported
//  subset earns the same 2 as a syntax error, because the exit code answers
//  "did I get a file" and the refusal answers itself in its message.
//
//  A clean run says NOTHING, which is why this grammar has no quiet flag to
//  silence it. The progress line as65 prints and offers a flag against is a
//  historical courtesy, and a subcommand added today can simply not print it.
//
////////////////////////////////////////////////////////////////////////////////

int DoMerlin (const CommandLineOptions & options)
{
    HRESULT                         hr         = S_OK;
    AssemblerOptions                asmOptions;
    DefaultFileReader               fileReader;
    Cpu                             cpu;
    AssembleResult                  ar;
    std::vector<DialectReportLine>  reports;
    std::string                     outputName;
    //  The same request with the resolved object name in it, since the name is
    //  only known once the source has had its say.
    CommandLineOptions              writeOptions;
    size_t                          lastSep    = 0;
    int                             exitCode   = 0;
    bool                            hasInput   = !options.inputFile.empty();
    bool                            wasWritten = false;



    exitCode = AssemblerExitCode::ToProcessCode (AssemblyExitCode::NoOutput);

    if (!hasInput)
    {
        std::cerr << "Error: No input file specified\n";
    }

    BAIL_OUT_IF (!hasInput, S_OK);

    asmOptions            = BuildAssemblerOptions (options);
    asmOptions.fileReader = &fileReader;

    // An included file resolves against the source that names it rather than
    // against the shell's working directory, so a build works from anywhere.
    lastSep = options.inputFile.find_last_of ("/\\");

    if (lastSep != std::string::npos)
    {
        asmOptions.baseDir = options.inputFile.substr (0, lastSep);
    }

    cpu.Reset();

    ar = AssembleFile (options.inputFile, cpu.GetInstructionSet(), GetCpu65C02InstructionSet(), asmOptions);
    ReportAssemblyDiagnostics (ar);

    reports = DialectReporting::BuildReport (asmOptions, BuildCpuReport (options, ar.result));
    ReportToStandardError (reports);

    exitCode = AssemblerExitCode::ToProcessCode (AssemblerExitCode::FromResult (ar.result));

    BAIL_OUT_IF (!ar.ok, S_OK);

    hr         = options.generateListing ? WriteListingOutput (ar.result, options, reports) : S_OK;
    wasWritten = SUCCEEDED (hr);

    if (!wasWritten)
    {
        exitCode = AssemblerExitCode::ToProcessCode (AssemblyExitCode::NoOutput);
    }

    BAIL_OUT_IF (!wasWritten, S_OK);

    outputName              = ResolveMerlinOutputName (options, ar.result);
    writeOptions            = options;
    writeOptions.outputFile = outputName;

    hr         = WriteBinaryOutput (ar.result, writeOptions);
    wasWritten = SUCCEEDED (hr);

    if (!wasWritten)
    {
        exitCode = AssemblerExitCode::ToProcessCode (AssemblyExitCode::NoOutput);
    }

    BAIL_OUT_IF (!wasWritten, S_OK);

    if (options.verbose)
    {
        std::cerr << "Assembly successful\n";
        std::println (stderr, "  Output:  {}", outputName);
        std::println (stderr, "  Bytes:   {}", ar.result.bytes.size());
        std::println (stderr, "  Start:   ${:04X}", ar.result.startAddress);
        std::println (stderr, "  End:     ${:04X}", ar.result.endAddress);
        std::println (stderr, "  Symbols: {}", ar.result.symbols.size());
    }

Error:
    return exitCode;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrintCpuFlagRefusal
//
//  A CPU flag the active dialect does not take.
//
//  The sentence arrives composed, because naming the in-source directive that
//  replaces the flag is the dialect's own knowledge and this is the printing
//  edge. The exit code is the same "no output" every other way of producing
//  nothing earns: a script asks whether it got a file, and it did not.
//
////////////////////////////////////////////////////////////////////////////////

int PrintCpuFlagRefusal (const std::string & refusal)
{
    std::cerr << "CassoCli: " << refusal << "\n";

    return AssemblerExitCode::ToProcessCode (AssemblyExitCode::NoOutput);
}
