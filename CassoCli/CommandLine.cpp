#include "Pch.h"

#include "CommandLine.h"
#include "Assembler.h"
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
//  WriteBinaryShapeFile
//
//  Opens the output file in binary mode and hands the stream to the writer for
//  the chosen shape.
//
//  The three binary shapes differ only in what goes INTO the stream, so the
//  file handling -- open it, check it, verify the write landed -- is written
//  once here, and each shape lives in OutputFormats where tests can reach it
//  without a file at all.
//
////////////////////////////////////////////////////////////////////////////////

static HRESULT WriteBinaryShapeFile (const std::string & path,
                                     const AssemblyResult & result,
                                     CommandLineOptions::OutputFormat shape,
                                     Byte fillByte)
{
    HRESULT  hr         = S_OK;
    bool     isOpen     = false;
    bool     wasWritten = false;
    std::ofstream  file (path, std::ios::binary);
    isOpen = file.is_open();



    CBR (isOpen);

    if (shape == CommandLineOptions::OutputFormat::Raw)
    {
        OutputFormats::WriteRaw (result.bytes, file);
    }
    else if (shape == CommandLineOptions::OutputFormat::DosBinary)
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
////////////////////////////////////////////////////////////////////////////////

static AssemblerOptions BuildAssemblerOptions (const CommandLineOptions & options)
{
    AssemblerOptions asmOptions   = {};
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
////////////////////////////////////////////////////////////////////////////////

static AssembleResult AssembleFile (const std::string & inputFile,
                                   const Microcode instructionSet[256],
                                   const AssemblerOptions & asmOptions)
{
    HRESULT         hr = S_OK;
    AssembleResult  ar = {};
    std::string     source;

    ar.inputFile = inputFile;



    hr = ReadFileContents (inputFile, source);

    if (FAILED (hr))
    {
        std::cerr << "Error: Cannot read input file: " << inputFile << "\n";
        ar.ok = false;
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
    for (const auto & w : ar.result.warnings)
    {
        std::println (stderr, "{}:{}: warning: {}", ar.inputFile, w.lineNumber, w.message);
    }

    for (const auto & e : ar.result.errors)
    {
        std::println (stderr, "{}:{}: error: {}", ar.inputFile, e.lineNumber, e.message);
    }

    if (!ar.ok)
    {
        std::println (stderr, "Assembly failed with {} error(s)", ar.result.errors.size());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ResolveOutputFormat
//
//  Decides which shape to write.
//
//  An explicit format flag WINS. Extension matching remains, but only as the
//  fallback when no flag was given, which is what keeps as65-era build scripts
//  -- which name a .s19 or .hex output and pass no flag -- working unchanged.
//
//  Deriving purely from the extension, as this used to, meant `-s -o out.dat`
//  silently wrote a flat binary: the flag said S-record and the extension won
//  anyway. It also leaves the two new shapes unreachable, since neither raw
//  nor DOS-binary output has an extension of its own to be recognized by.
//
////////////////////////////////////////////////////////////////////////////////

static CommandLineOptions::OutputFormat ResolveOutputFormat (const CommandLineOptions & options)
{
    CommandLineOptions::OutputFormat  shape      = options.outputFormat;
    bool                              isDefault  = shape == CommandLineOptions::OutputFormat::Binary;
    bool                              isSRec     = CommandLineParser::EndsWith (options.outputFile, ".s19");
    bool                              isHex      = CommandLineParser::EndsWith (options.outputFile, ".hex");



    if (isDefault && isSRec)
    {
        shape = CommandLineOptions::OutputFormat::SRecord;
    }
    else if (isDefault && isHex)
    {
        shape = CommandLineOptions::OutputFormat::IntelHex;
    }

    return shape;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteBinaryOutput
//
//  Writes the assembled image in the resolved shape.
//
//  "nul" is the explicit bit bucket and is matched case-insensitively, since
//  it is a Windows device name that scripts spell every way. Writing nothing
//  is SUCCESS on that path: it is how a caller asks for diagnostics only, and
//  reporting failure would break a build that deliberately discards output.
//
//  The text formats open the stream in text mode and the binary shapes in
//  binary mode, which is the only reason this splits in two rather than
//  handing every shape to one writer.
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
    CommandLineOptions::OutputFormat  shape     = ResolveOutputFormat (options);
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
    isText = shape == CommandLineOptions::OutputFormat::SRecord ||
             shape == CommandLineOptions::OutputFormat::IntelHex;

    if (shape == CommandLineOptions::OutputFormat::DosBinary)
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

        if (shape == CommandLineOptions::OutputFormat::SRecord)
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
        hr = WriteBinaryShapeFile (options.outputFile, result, shape, options.fillByte);
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
//  The three spellings tested are the ones the assembler itself accepts.
//
////////////////////////////////////////////////////////////////////////////////

static HRESULT WriteListingOutput (const AssemblyResult & result,
                                   const CommandLineOptions & options)
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

        *listOut << Assembler::FormatListingLine (line, options.cycleCounts) << "\n";
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
//  PrintUsageHeader
//
////////////////////////////////////////////////////////////////////////////////

static void PrintUsageHeader (const char * sp, const char * lp)
{
    std::cout << "CassoCli - 6502 Assembler and Emulator  v" VERSION_STRING
              << " (" << arch << ")  " VERSION_BUILD_TIMESTAMP "\n"
              << "Copyright (c) 2025-" VERSION_YEAR_STRING " by Robert Elmer\n"
              << "\n"
              << "Usage: CassoCli <source> [flags] | run <binary | source> [options] | "
              << sp << "? | " << lp << "version\n"
              << "       CassoCli disk list <image> [" << lp << "long]\n"
              << "                disk get  <image> <path> [" << lp << "out <file>]"
                 " [" << lp << "text | " << lp << "basic | " << lp << "verbatim]\n"
              << "                disk put  <image> <file> [" << lp << "as <path>]"
                 " [" << lp << "type <t>] [" << lp << "addr $XXXX]\n"
              << "                disk delete <image> <path>   |   disk boot <image> <path>\n"
              << "         aliases: ls = list, rm = delete."
                 "  put/get are named from the DISK's point of view.\n"
              << "         exit: 0 clean, 1 succeeded with complaints, 2 produced no output\n";
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
    std::println ("\nGeneral:");
    std::println ("  {0}help, {1}?{2}             Show this help", lp, sp, pad);
    std::println ("  {0}version{1}              Show version information", lp, pad);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrintUsageAssembler
//
//  Prints the AS65-mode flag reference, spelled with whichever prefix the user
//  typed.
//
//  The prefix is substituted rather than hard-coded because both `/` and `-`
//  are accepted, and usage text showing the form the reader did NOT type reads
//  as though their invocation was wrong. The flag table is a format-string
//  array for exactly that reason: one placeholder per flag, filled at print
//  time, so neither spelling can be forgotten when a flag is added.
//
//  The `--cpu` and source lines sit outside the table because they take a
//  long-form or positional argument and carry no prefix to substitute.
//
////////////////////////////////////////////////////////////////////////////////

static void PrintUsageAssembler (const char * sp)
{
    std::println ("");
    std::println ("Assembler flags:");
    std::println ("  <source>               Assembly source file");
    std::println ("                         (will try .a65, .asm, .s if no extension is present)");
    std::println ("");
    std::println ("  --cpu <6502|65c02>     Target CPU (default: 6502). 65c02 enables the");
    std::println ("                         CMOS opcodes (STZ, BRA, RMBn/SMBn, BBRn/BBSn, ...);");
    std::println ("                         under 6502 those are rejected as invalid.");
    std::println ("");
    std::println ("  --raw                  Write only the assembled bytes, unpadded");
    std::println ("  --dos-bin              Write the assembled bytes behind a 4-byte DOS 3.3");
    std::println ("                         header (load address + length), ready to BLOAD");
    std::println ("                         (default: a full 64 KB image, padded with the fill byte)");

    const char * lines[] =
    {
        "  {0}c                     Show cycle counts in listing",
        "  {0}d <name>[=<value>]    Pre-define symbol",
        "  {0}g                     Generate debug information file",
        "  {0}h <lines>             Page height for listing (0 = no pagination)",
        "  {0}i                     Case-insensitive (default, no-op)",
        "  {0}l [<file>]            Generate listing ({0}l = stdout, {0}l file = to file)",
        "  {0}m                     Show macro expansions in listing",
        "  {0}n                     Disable optimizations (no-op)",
        "  {0}o <file>              Output file (default: input with .bin extension)",
        "  {0}p                     Generate pass 1 listing",
        "  {0}q                     Quiet mode (suppress progress)",
        "  {0}s                     Output S-record format (.s19)",
        "  {0}s2                    Output Intel HEX format (.hex)",
        "  {0}t                     Generate symbol table",
        "  {0}v                     Verbose mode",
        "  {0}w [<width>]           Column width (default: 79, {0}w alone = 133)",
        "  {0}z                     Fill unused space with $00 (default: $FF)",
    };

    for (const char * fmt : lines)
    {
        std::println ("{}", std::vformat (fmt, std::make_format_args (sp)));
    }

    std::println ("");
    std::println ("  Flags can be concatenated: {0}tlfile = {0}t {0}lfile", sp);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrintUsageRun
//
////////////////////////////////////////////////////////////////////////////////

static void PrintUsageRun (const char * lp, const char * sp, const char * pad)
{
    std::println ("");
    std::println ("Run options:");
    std::println ("  <binary>               A binary file to load and execute");
    std::println ("  <source>               An assembly source file to assemble and run");
    std::println ("                         (will try .a65, .asm, .s if no extension is present)");
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
        std::println ("{}", std::vformat (fmt, std::make_format_args (lp, pad)));
    }

    std::println ("  {0}v                     Verbose output", sp);
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

        asmOptions.warningMode = options.warningMode;

        ar = AssembleFile (options.inputFile, SelectInstructionSet (options, cpu), asmOptions);
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



    HRESULT                   hr          = S_OK;
    std::vector<std::string>  status;
    AssemblerOptions          asmOptions;
    DefaultFileReader         fileReader;
    Cpu                       cpu;
    AssembleResult            ar;
    Clock::time_point         startTime;
    Clock::time_point         endTime;
    size_t                    lastSep     = 0;
    int                       exitCode    = 0;
    bool                      hasInput    = !options.inputFile.empty();
    bool                      hasWarnings = false;
    bool                      wasWritten  = false;



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
    ar        = AssembleFile (options.inputFile, SelectInstructionSet (options, cpu), asmOptions);
    endTime   = Clock::now();

    if (options.verbose)
    {
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds> (endTime - startTime);
        std::cerr << "Pass 2...\n";
        std::println (stderr, "Assembly time: {} us", elapsed.count());
    }

    hasWarnings = !ar.result.warnings.empty();
    ReportAssemblyDiagnostics (ar);

    exitCode = ar.ok ? 0 : 2;

    BAIL_OUT_IF (!ar.ok, S_OK);

    if (!options.quiet)
    {
        std::cerr << ar.result.listing.size() << " lines assembled\n";
    }

    // Each output artifact is optional but fails the run the same way, so the
    // exit code is set once and each step just reports whether it wrote.
    hr         = options.generateListing ? WriteListingOutput (ar.result, options) : S_OK;
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
