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
//  ParseBoundedHex
//
//  Shared by ParseAddress and ParseFillByte, which differ only in their upper
//  bound. Accepts an optional `$` prefix and requires the whole string to be
//  consumed, so "12zz" is rejected rather than silently read as $12.
//
////////////////////////////////////////////////////////////////////////////////

static HRESULT ParseBoundedHex (const char * text, long maxValue, long & outValue)
{
    HRESULT       hr      = S_OK;
    const char *  hex     = text;
    char       *  end     = nullptr;
    long          value   = 0;
    bool          isValid = false;



    CBREx (text != nullptr, E_INVALIDARG);
    CBREx (text[0] != '\0', E_INVALIDARG);

    if (hex[0] == '$')
    {
        hex++;
    }

    value   = strtol (hex, &end, 16);
    isValid = end != hex && *end == '\0' && value >= 0 && value <= maxValue;
    CBREx (isValid, E_INVALIDARG);

    outValue = value;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ParseAddress
//
////////////////////////////////////////////////////////////////////////////////

static HRESULT ParseAddress (const char * text, Word & address)
{
    HRESULT  hr    = S_OK;
    long     value = 0;



    hr = ParseBoundedHex (text, 0xFFFF, value);
    CHR (hr);

    address = (Word) value;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ParseDecimal
//
////////////////////////////////////////////////////////////////////////////////

static HRESULT ParseDecimal (const char * text, uint32_t & value)
{
    HRESULT  hr      = S_OK;
    char *   end     = nullptr;
    long     val     = 0;
    bool     isValid = false;



    CBREx (text != nullptr, E_INVALIDARG);
    CBREx (text[0] != '\0', E_INVALIDARG);

    val     = strtol (text, &end, 10);
    isValid = end != text && *end == '\0' && val >= 0;
    CBREx (isValid, E_INVALIDARG);

    value = (uint32_t) val;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ParseFillByte
//
////////////////////////////////////////////////////////////////////////////////

static HRESULT ParseFillByte (const char * text, Byte & fillByte)
{
    HRESULT  hr    = S_OK;
    long     value = 0;



    hr = ParseBoundedHex (text, 0xFF, value);
    CHR (hr);

    fillByte = (Byte) value;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadFileContents
//
////////////////////////////////////////////////////////////////////////////////

static HRESULT ReadFileContents (const std::string & path, std::string & contents)
{
    HRESULT             hr = S_OK;
    std::ifstream       file (path, std::ios::binary);
    std::ostringstream  ss;
    bool                isOpen = file.is_open();



    CBR (isOpen);

    ss << file.rdbuf();
    contents = ss.str();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteFlatBinaryFile
//
//  Writes the assembled bytes as a FULL 64 KB memory image: fill from $0000
//  to the start address, the code, then fill out to $FFFF.
//
//  This is the as65-compatible output shape, not a convenience. Tools that
//  consume a flat image -- ROM burners, emulator memory loaders, byte-for-byte
//  comparison against a reference build -- index it by absolute address, so
//  the file offset must equal the address. Writing only the assembled span
//  would shift every byte by the start address.
//
//  The fill byte is caller-supplied because it is visible in the output and
//  therefore part of the comparison: matching a reference image requires
//  matching what its gaps were padded with.
//
////////////////////////////////////////////////////////////////////////////////

static HRESULT WriteFlatBinaryFile (const std::string & path,
                                    const std::vector<Byte> & data,
                                    Word startAddress,
                                    Byte fillByte)
{
    HRESULT        hr = S_OK;
    std::ofstream  file (path, std::ios::binary);
    uint32_t       endAddress = 0;
    bool           isOpen     = file.is_open();
    bool           wasWritten = false;



    CBR (isOpen);

    // Pad from address 0 to startAddress
    for (Word i = 0; i < startAddress; i++)
    {
        file.put ((char) fillByte);
    }

    // Write assembled bytes
    file.write (reinterpret_cast<const char *> (data.data()), data.size());

    // Pad from end to fill full 64KB address space
    endAddress = (uint32_t) startAddress + (uint32_t) data.size();

    for (uint32_t i = endAddress; i < 0x10000u; i++)
    {
        file.put ((char) fillByte);
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
    HRESULT                                    hr = S_OK;
    std::ofstream                              file (path);
    std::vector<std::pair<std::string, Word>>  sorted;
    bool                                       isOpen     = file.is_open();
    bool                                       wasWritten = false;



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
//  EndsWith
//
//  Case-insensitive suffix test, used for file-extension matching.
//
//  Case-insensitive because these are Windows paths: a user typing FOO.ASM and
//  a makefile emitting foo.asm name the same file, and a case-sensitive test
//  would silently classify one of them as having no recognized extension.
//
//  Both sides are lowered rather than assuming the caller passes a lowercase
//  suffix, so the function is correct regardless of how the call site spells
//  its literal.
//
////////////////////////////////////////////////////////////////////////////////

static bool EndsWith (const std::string & str, const std::string & suffix)
{
    std::string  strEnd;
    std::string  suffLower = suffix;
    bool         fits      = suffix.size() <= str.size();
    bool         matches   = false;



    if (fits)
    {
        strEnd = str.substr (str.size() - suffix.size());

        for (auto & c : strEnd)
        {
            c = (char) tolower ((unsigned char) c);
        }

        for (auto & c : suffLower)
        {
            c = (char) tolower ((unsigned char) c);
        }

        matches = strEnd == suffLower;
    }

    return matches;
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
//  TryAutoExtend
//
//  Resolves an extensionless source path by trying the common assembler
//  extensions in order, so `casso build` finds build.a65 the way as65 does.
//
//  "Has an extension" is decided against the last path SEPARATOR, not just the
//  last dot. A dot before the final separator belongs to a directory name --
//  `src/v1.2/build` has no extension despite containing a dot -- and testing
//  for a bare dot would leave that path unresolved.
//
//  Existence decides the match, so a path that already resolves is returned
//  untouched and a name with no candidate on disk is returned unchanged for
//  the caller to report against the name the user actually typed.
//
////////////////////////////////////////////////////////////////////////////////

static std::string TryAutoExtend (const std::string & path)
{
    // Common source extensions, tried in order.
    static const char * extensions[] = { ".a65", ".asm", ".s", nullptr };

    std::string  result   = path;
    std::string  candidate;
    size_t       dot      = path.rfind ('.');
    size_t       sep      = path.find_last_of ("/\\");
    // A dot after the last separator is an extension; a dot before it belongs
    // to a directory name and does not count.
    bool         hasExt   = dot != std::string::npos && (sep == std::string::npos || dot > sep);
    bool         found    = false;
    int          i        = 0;



    for (i = 0; !hasExt && !found && extensions[i] != nullptr; i++)
    {
        candidate = path + extensions[i];

        if (FileExists (candidate))
        {
            result = candidate;
            found  = true;
        }
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  StripExtension
//
////////////////////////////////////////////////////////////////////////////////

static std::string StripExtension (const std::string & path)
{
    std::string  result = path;
    size_t       dot    = path.rfind ('.');
    size_t       sep    = path.find_last_of ("/\\");
    // See TryAutoExtend: only a dot after the last separator is an extension.
    bool         hasExt = dot != std::string::npos && (sep == std::string::npos || dot > sep);



    if (hasExt)
    {
        result = path.substr (0, dot);
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsAssemblySource
//
////////////////////////////////////////////////////////////////////////////////

static bool IsAssemblySource (const std::string & path)
{
    return EndsWith (path, ".asm") || EndsWith (path, ".s") ||
           EndsWith (path, ".a65") || EndsWith (path, ".a65c");
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
//  WriteBinaryOutput
//
//  Writes the assembled image in whichever format the output filename implies:
//  Motorola S-record for .s19, Intel HEX for .hex, and a flat 64 KB binary
//  otherwise.
//
//  Format-by-extension is the as65 convention, so build scripts written for it
//  keep working unchanged -- there is no format flag to add.
//
//  "nul" is the explicit bit bucket and is matched case-insensitively, since
//  it is a Windows device name that scripts spell every way. Writing nothing
//  is SUCCESS on that path: it is how a caller asks for diagnostics only, and
//  reporting failure would break a build that deliberately discards output.
//
//  The failure diagnostic is emitted once for all three formats. It used to be
//  written out at four separate sites, which is three opportunities for the
//  wording to drift apart.
//
////////////////////////////////////////////////////////////////////////////////

static HRESULT WriteBinaryOutput (const AssemblyResult & result,
                                  const CommandLineOptions & options)
{
    HRESULT      hr       = S_OK;
    std::string  outLower = options.outputFile;
    bool         isNul    = false;
    bool         isSRec   = false;
    bool         isHex    = false;
    bool         isOpen   = false;



    for (auto & c : outLower)
    {
        c = (char) tolower ((unsigned char) c);
    }

    // "nul" is the explicit bit bucket: nothing written, and that is success.
    isNul  = outLower == "nul";
    isSRec = EndsWith (options.outputFile, ".s19");
    isHex  = EndsWith (options.outputFile, ".hex");

    if (!isNul && (isSRec || isHex))
    {
        std::ofstream  outFile (options.outputFile);

        isOpen = outFile.is_open();
        CBR (isOpen);

        if (isSRec)
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
        hr = WriteFlatBinaryFile (options.outputFile, result.bytes, result.startAddress, options.fillByte);
        CHR (hr);
    }

Error:
    // One diagnostic for all three paths -- it was written out identically at
    // four sites before, which is three chances for the wording to drift.
    if (FAILED (hr))
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
    HRESULT        hr = S_OK;
    std::ofstream  dbgFile (debugFile);
    bool           isOpen = dbgFile.is_open();



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
    cpu.SetPC (entryPoint);
    status.push_back (std::format ("Executing from ${:04X}", entryPoint));

    uint32_t cycles   = 0;
    int      exitCode = 0;



    for (;;)
    {
        if (options.maxCycles > 0 && cycles >= options.maxCycles)
        {
            status.push_back (std::format ("Stopped: cycle limit reached ({})", options.maxCycles));
            break;
        }

        Byte opcode = cpu.PeekByte (cpu.GetPC());

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
//  ParseAs65Flags
//
//  Parses the AS65-compatible command line, which is not a modern one and
//  cannot be handled by a modern parser.
//
//  Three period conventions have to be honored together:
//
//    concatenation   flags pack into one argument, so `-lsc` is three flags,
//                    not an unknown flag named "lsc"
//    prefix parity   `/` and `-` both introduce a flag. The prefix the user
//                    chose is REMEMBERED in flagPrefix so the usage text comes
//                    back spelled the way they type
//    attached values a flag's argument may be glued to it or separated
//
//  Which is why this is a hand-rolled walk rather than a table: a table-driven
//  parser would have to encode all three exceptions anyway, and every one of
//  them is about matching a specific historical tool.
//
//  The stop flag ends parsing outright for a help request or a bad --cpu
//  target. Both leave showHelp set, so the caller prints usage and no later
//  argument can quietly undo that decision.
//
////////////////////////////////////////////////////////////////////////////////

static void ParseAs65Flags (int argc, char * argv[], CommandLineOptions & options)
{
    int   argIndex = 1;
    // Set when an argument ends parsing outright -- a help request, or a bad
    // --cpu target. Both leave showHelp set, so the caller prints usage.
    bool  stop     = false;

    options.subcommand = CommandLineOptions::Subcommand::As65;



    while (argIndex < argc && !stop)
    {
        std::string arg (argv[argIndex]);

        // Check for help requests
        if (arg == "--help" || arg == "-help" || arg == "-?" || arg == "/?" || arg == "/help")
        {
            if (arg[0] == '/')
            {
                options.flagPrefix = '/';
            }

            options.showHelp = true;
            stop             = true;
            continue;
        }

        // Long option: --cpu <target> / --cpu=<target> selects the target
        // instruction set. Default stays 6502 so 65C02-only opcodes never assemble
        // by accident; only an explicit --cpu 65c02 unlocks the CMOS tier.
        if (arg == "--cpu" || arg.rfind ("--cpu=", 0) == 0)
        {
            std::string val;

            if (arg == "--cpu")
            {
                if (argIndex + 1 < argc)
                {
                    val = argv[++argIndex];
                }
            }
            else
            {
                val = arg.substr (6);   // after "--cpu="
            }

            for (char & c : val)
            {
                c = (char) tolower ((unsigned char) c);
            }

            if (val == "6502")
            {
                options.cpuTarget = CommandLineOptions::CpuTarget::M6502;
            }
            else if (val == "65c02")
            {
                options.cpuTarget = CommandLineOptions::CpuTarget::M65C02;
            }
            else
            {
                std::cerr << "Error: unknown --cpu target '" << val
                          << "' (expected 6502 or 65c02)\n";
                options.showHelp = true;
                stop             = true;
            }

            if (!stop)
            {
                argIndex++;
            }

            continue;
        }

        // Normalize / prefix to - for flag parsing
        if (arg[0] == '/')
        {
            arg[0] = '-';
        }

        // Non-flag argument is the input file
        if (arg[0] != '-' && arg[0] != '/')
        {
            if (options.inputFile.empty())
            {
                options.inputFile = arg;
            }

            argIndex++;
            continue;
        }

        // Parse concatenated flags: -tlfile means -t -lfile
        size_t pos = 1;

        while (pos < arg.size())
        {
            char         flag = arg[pos];
            std::string  rest = arg.substr (pos + 1);

            switch (flag)
            {
            case 'c':
                options.cycleCounts = true;
                pos++;
                break;

            case 't':
                options.symbolTable = true;
                pos++;
                break;

            case 'l':
                options.generateListing = true;

                if (!rest.empty())
                {
                    options.listingFile = rest;
                    pos = arg.size();
                }
                else if (argIndex + 1 < argc && argv[argIndex + 1][0] != '-' && argv[argIndex + 1][0] != '/')
                {
                    options.listingFile = argv[++argIndex];
                    pos = arg.size();
                }
                else
                {
                    options.listingToStdout = true;
                    pos++;
                }

                break;

            case 'o':
                if (!rest.empty())
                {
                    options.outputFile = rest;
                    pos = arg.size();
                }
                else if (argIndex + 1 < argc)
                {
                    options.outputFile = argv[++argIndex];
                    pos = arg.size();
                }

                break;

            case 'm':
                options.macroExpansion = true;
                pos++;
                break;

            case 'h':
            {
                if (!rest.empty())
                {
                    uint32_t val = 0;
                    HRESULT  hr  = ParseDecimal (rest.c_str(), val);

                    if (SUCCEEDED (hr))
                    {
                        options.pageHeight = (int) val;
                    }

                    pos = arg.size();
                }
                else
                {
                    pos++;
                }

                break;
            }

            case 'w':
            {
                if (!rest.empty())
                {
                    uint32_t val = 0;
                    HRESULT  hr  = ParseDecimal (rest.c_str(), val);

                    if (SUCCEEDED (hr))
                    {
                        options.pageWidth = (int) val;
                    }

                    pos = arg.size();
                }
                else
                {
                    pos++;
                }

                break;
            }

            case 'v':
                options.verbose = true;
                pos++;
                break;

            case 'q':
                options.quiet = true;
                pos++;
                break;

            case 'n':
                options.disableOpt = true;
                pos++;
                break;

            case 'i':
                options.caseSensitive = true;
                pos++;
                break;

            case 'p':
                options.pass1Listing = true;
                pos++;
                break;

            case 'z':
                options.fillZero = true;
                options.fillByte = 0x00;
                pos++;
                break;

            case 'g':
                options.debugInfo = true;

                if (!rest.empty())
                {
                    options.debugFile = rest;
                    pos = arg.size();
                }
                else
                {
                    pos++;
                }

                break;

            case 'd':
            {
                std::string def = rest;

                if (def.empty() && argIndex + 1 < argc)
                {
                    def = argv[++argIndex];
                }

                if (!def.empty())
                {
                    size_t       eqPos = def.find ('=');
                    std::string  name;
                    int32_t      value = 1;

                    if (eqPos != std::string::npos)
                    {
                        name = def.substr (0, eqPos);
                        std::string    valStr = def.substr (eqPos + 1);
                        char         * end    = nullptr;
                        long           v      = strtol (valStr.c_str(), &end, 0);

                        if (end != valStr.c_str())
                        {
                            value = (int32_t) v;
                        }
                    }
                    else
                    {
                        name = def;
                    }

                    if (!name.empty())
                    {
                        options.predefinedSymbols[name] = value;
                    }
                }

                pos = arg.size();
                break;
            }

            case 's':
                // -s = S-record output (.s19), -s2 = Intel HEX output (.hex)
                if (!rest.empty() && rest[0] == '2')
                {
                    options.outputFormat = CommandLineOptions::OutputFormat::IntelHex;

                    if (rest.size() > 1)
                    {
                        options.outputFile = rest.substr (1);
                    }
                }
                else
                {
                    options.outputFormat = CommandLineOptions::OutputFormat::SRecord;

                    if (!rest.empty())
                    {
                        options.outputFile = rest;
                    }
                }

                pos = arg.size();
                break;

            default:
                std::cerr << "Warning: Unknown flag: -" << flag << "\n";
                pos++;
                break;
            }
        }

        argIndex++;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ParseCommandLine
//
//  The top-level dispatcher: decide which command line this IS, then parse it
//  accordingly.
//
//  The CLI has two grammars, and they are not compatible. `run` takes modern
//  separated options; anything else is AS65 mode, whose grammar is the
//  historical one handled by ParseAs65Flags. An unrecognized first argument is
//  therefore NOT an error -- it is a source filename, which is exactly how
//  as65 was invoked -- so AS65 is the fallback rather than a mode flag.
//
//  Help and version are matched before either grammar, so they work regardless
//  of which one would have applied.
//
//  A leading `/` is normalized to `-` throughout, after recording the user's
//  chosen prefix in flagPrefix so usage text is spelled back the way they type.
//
//  Filename inference happens only in AS65 mode, and only when the user did
//  not supply the name. The output extension follows the selected FORMAT (.s19
//  / .hex / .bin) so `-s file.a65` writes an S-record without a second flag,
//  and `-g` alone yields a .dbg beside it. The input name is auto-extended
//  first, so both derive from the resolved source path rather than the
//  possibly extensionless one the user typed.
//
////////////////////////////////////////////////////////////////////////////////

CommandLineOptions ParseCommandLine (int argc, char * argv[])
{
    HRESULT             hr       = S_OK;
    CommandLineOptions  options  = {};
    std::string         first;
    int                 argIndex = 1;
    bool                isHelp   = false;
    bool                isVer    = false;
    bool                isAs65   = false;



    if (argc < 2)
    {
        options.showHelp = true;
    }

    BAIL_OUT_IF (argc < 2, S_OK);

    // Check for --help / --version first (accept / prefix on Windows)
    first = argv[1];

    if (first[0] == '/')
    {
        options.flagPrefix = '/';
        first[0] = '-';
    }

    isHelp = first == "--help" || first == "-help" || first == "-h" || first == "-?";
    isVer  = first == "--version" || first == "-version";

    if (isHelp)
    {
        options.subcommand = CommandLineOptions::Subcommand::Help;
        options.showHelp   = true;
    }
    else if (isVer)
    {
        options.subcommand  = CommandLineOptions::Subcommand::Version;
        options.showVersion = true;
    }

    BAIL_OUT_IF (isHelp || isVer, S_OK);

    // Parse subcommand
    isAs65 = first != "run";

    if (!isAs65)
    {
        options.subcommand = CommandLineOptions::Subcommand::Run;
        argIndex = 2;
    }
    else
    {
        // No recognized subcommand - treat as AS65 mode
        ParseAs65Flags (argc, argv, options);

        // Auto-extend input file
        if (!options.inputFile.empty())
        {
            options.inputFile = TryAutoExtend (options.inputFile);
        }

        // Auto-generate output file if not specified
        if (options.outputFile.empty() && !options.inputFile.empty())
        {
            std::string ext = ".bin";

            if (options.outputFormat == CommandLineOptions::OutputFormat::SRecord)
            {
                ext = ".s19";
            }
            else if (options.outputFormat == CommandLineOptions::OutputFormat::IntelHex)
            {
                ext = ".hex";
            }

            options.outputFile = StripExtension (options.inputFile) + ext;
        }

        // Auto-generate debug file if -g but no file specified
        if (options.debugInfo && options.debugFile.empty() && !options.inputFile.empty())
        {
            options.debugFile = StripExtension (options.inputFile) + ".dbg";
        }
    }

    // AS65 mode consumed the whole command line above; only `run` continues
    // into the option loop below.
    BAIL_OUT_IF (isAs65, S_OK);

    // Parse remaining arguments
    while (argIndex < argc)
    {
        std::string arg (argv[argIndex]);

        // Normalize / prefix to - on Windows
        if (arg.size() > 1 && arg[0] == '/')
        {
            arg[0] = '-';
        }

        if (arg == "-o" && argIndex + 1 < argc)
        {
            options.outputFile = argv[++argIndex];
        }
        else if (arg == "-l" && argIndex + 1 < argc)
        {
            options.symbolFile = argv[++argIndex];
        }
        else if (arg == "-a")
        {
            options.generateListing = true;
        }
        else if (arg == "-v")
        {
            options.verbose = true;
        }
        else if (arg == "--fill" && argIndex + 1 < argc)
        {
            hr = ParseFillByte (argv[++argIndex], options.fillByte);

            if (FAILED (hr))
            {
                std::cerr << "Error: Invalid fill byte value\n";
            }
        }
        else if (arg == "--load" && argIndex + 1 < argc)
        {
            hr = ParseAddress (argv[++argIndex], options.loadAddress);

            if (SUCCEEDED (hr))
            {
                options.hasLoadAddress = true;
            }
            else
            {
                std::cerr << "Error: Invalid load address\n";
            }
        }
        else if (arg == "--entry" && argIndex + 1 < argc)
        {
            hr = ParseAddress (argv[++argIndex], options.entryAddress);

            if (SUCCEEDED (hr))
            {
                options.hasEntryAddress = true;
            }
            else
            {
                std::cerr << "Error: Invalid entry address\n";
            }
        }
        else if (arg == "--stop" && argIndex + 1 < argc)
        {
            hr = ParseAddress (argv[++argIndex], options.stopAddress);

            if (SUCCEEDED (hr))
            {
                options.hasStopAddress = true;
            }
            else
            {
                std::cerr << "Error: Invalid stop address\n";
            }
        }
        else if (arg == "--max-cycles" && argIndex + 1 < argc)
        {
            hr = ParseDecimal (argv[++argIndex], options.maxCycles);

            if (FAILED (hr))
            {
                std::cerr << "Error: Invalid max-cycles value\n";
            }
        }
        else if (arg == "--reset-vector")
        {
            options.useResetVector = true;
        }
        else if (arg == "--warn")
        {
            options.warningMode = WarningMode::Warn;
        }
        else if (arg == "--no-warn")
        {
            options.warningMode = WarningMode::NoWarn;
        }
        else if (arg == "--fatal-warnings")
        {
            options.warningMode = WarningMode::FatalWarnings;
        }
        else if (arg[0] != '-' && options.inputFile.empty())
        {
            options.inputFile = arg;
        }
        else
        {
            std::cerr << "Error: Unknown option: " << arg << "\n";
        }

        argIndex++;
    }

Error:
    return options;
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
              << sp << "? | " << lp << "version\n";
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

    if (IsAssemblySource (options.inputFile))
    {
        AssemblerOptions asmOptions = {};
        asmOptions.warningMode     = options.warningMode;

        auto ar = AssembleFile (options.inputFile, SelectInstructionSet (options, cpu), asmOptions);
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
