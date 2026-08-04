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

static bool ParseBoundedHex (const char * text, long maxValue, long & outValue)
{
    const char *  hex     = text;
    char       *  end     = nullptr;
    long          value   = 0;
    bool          isValid = false;



    if (text != nullptr && text[0] != '\0')
    {
        if (hex[0] == '$')
        {
            hex++;
        }

        value   = strtol (hex, &end, 16);
        isValid = end != hex && *end == '\0' && value >= 0 && value <= maxValue;
    }

    if (isValid)
    {
        outValue = value;
    }

    return isValid;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ParseAddress
//
////////////////////////////////////////////////////////////////////////////////

static bool ParseAddress (const char * text, Word & address)
{
    long  value   = 0;
    bool  isValid = ParseBoundedHex (text, 0xFFFF, value);



    if (isValid)
    {
        address = (Word) value;
    }

    return isValid;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ParseDecimal
//
////////////////////////////////////////////////////////////////////////////////

static bool ParseDecimal (const char * text, uint32_t & value)
{
    char *  end     = nullptr;
    long    val     = 0;
    bool    isValid = false;



    if (text != nullptr && text[0] != '\0')
    {
        val     = strtol (text, &end, 10);
        isValid = end != text && *end == '\0' && val >= 0;
    }

    if (isValid)
    {
        value = (uint32_t) val;
    }

    return isValid;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ParseFillByte
//
////////////////////////////////////////////////////////////////////////////////

static bool ParseFillByte (const char * text, Byte & fillByte)
{
    long  value   = 0;
    bool  isValid = ParseBoundedHex (text, 0xFF, value);



    if (isValid)
    {
        fillByte = (Byte) value;
    }

    return isValid;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadFileContents
//
////////////////////////////////////////////////////////////////////////////////

static bool ReadFileContents (const std::string & path, std::string & contents)
{
    std::ifstream       file (path, std::ios::binary);
    std::ostringstream  ss;
    bool                isOpen = file.is_open();



    if (isOpen)
    {
        ss << file.rdbuf();
        contents = ss.str();
    }

    return isOpen;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteBinaryFile
//
////////////////////////////////////////////////////////////////////////////////

static bool WriteBinaryFile (const std::string & path, const std::vector<Byte> & data)
{
    std::ofstream  file (path, std::ios::binary);
    bool           wasWritten = false;



    if (file.is_open())
    {
        file.write (reinterpret_cast<const char *> (data.data()), data.size());
        wasWritten = file.good();
    }

    return wasWritten;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteFlatBinaryFile
//
////////////////////////////////////////////////////////////////////////////////

static bool WriteFlatBinaryFile (const std::string & path,
                                 const std::vector<Byte> & data,
                                 Word startAddress,
                                 Byte fillByte)
{
    std::ofstream  file (path, std::ios::binary);
    uint32_t       endAddress = 0;
    bool           wasWritten = false;



    if (file.is_open())
    {
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
    }

    return wasWritten;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteSymbolFile
//
////////////////////////////////////////////////////////////////////////////////

static bool WriteSymbolFile (const std::string & path, const std::unordered_map<std::string, Word> & symbols)
{
    std::ofstream                              file (path);
    std::vector<std::pair<std::string, Word>>  sorted;
    bool                                       wasWritten = false;



    if (file.is_open())
    {
        // Sort symbols by address for deterministic output
        sorted.assign (symbols.begin(), symbols.end());

        std::sort (sorted.begin(), sorted.end(),
            [] (const auto & a, const auto & b) { return a.second < b.second; });

        for (const auto & pair : sorted)
        {
            file << std::format ("${:04X}  {}\n", pair.second, pair.first);
        }

        wasWritten = file.good();
    }

    return wasWritten;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EndsWith
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
//  TryAutoExtend - if file has no extension, try common source extensions
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
////////////////////////////////////////////////////////////////////////////////

static AssembleResult AssembleFile (const std::string & inputFile,
                                   const Microcode instructionSet[256],
                                   const AssemblerOptions & asmOptions)
{
    AssembleResult  ar     = {};
    std::string     source;
    bool            wasRead = false;

    ar.inputFile = inputFile;



    wasRead = ReadFileContents (inputFile, source);

    if (!wasRead)
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
////////////////////////////////////////////////////////////////////////////////

static bool WriteBinaryOutput (const AssemblyResult & result,
                               const CommandLineOptions & options)
{
    std::string  outLower = options.outputFile;
    bool         isNul    = false;
    bool         isSRec   = false;
    bool         isHex    = false;
    bool         ok       = true;



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

        ok = outFile.is_open();

        if (ok && isSRec)
        {
            OutputFormats::WriteSRecord (result.bytes, result.startAddress, result.endAddress, result.startAddress, outFile);
        }
        else if (ok)
        {
            OutputFormats::WriteIntelHex (result.bytes, result.startAddress, result.endAddress, result.startAddress, outFile);
        }
    }
    else if (!isNul)
    {
        ok = WriteFlatBinaryFile (options.outputFile, result.bytes, result.startAddress, options.fillByte);
    }

    // One diagnostic for all three paths -- it was written out identically at
    // four sites before, which is three chances for the wording to drift.
    if (!ok)
    {
        std::cerr << "Error: Cannot write output file: " << options.outputFile << "\n";
    }

    return ok;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteListingOutput
//
////////////////////////////////////////////////////////////////////////////////

static bool WriteListingOutput (const AssemblyResult & result,
                                const CommandLineOptions & options)
{
    std::ostream *  listOut = &std::cout;
    std::ofstream   listFile;
    bool            ok      = true;



    // No listing file named means the listing goes to stdout, which cannot
    // fail to open.
    if (!options.listingFile.empty())
    {
        listFile.open (options.listingFile);
        ok = listFile.is_open();

        if (ok)
        {
            listOut = &listFile;
        }
        else
        {
            std::cerr << "Error: Cannot write listing file: " << options.listingFile << "\n";
        }
    }

    if (ok && !result.listingTitle.empty())
    {
        *listOut << result.listingTitle << "\n\n";
    }

    for (const auto & line : result.listing)
    {
        if (!ok)
        {
            break;
        }

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

    return ok;
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

static bool WriteDebugInfoOutput (const AssemblyResult & result,
                                  const std::string & debugFile)
{
    std::ofstream  dbgFile (debugFile);
    bool           ok = dbgFile.is_open();



    if (ok)
    {
        dbgFile << Assembler::FormatDebugInfo (result.symbols);
    }
    else
    {
        std::cerr << "Error: Cannot write debug file: " << debugFile << "\n";
    }

    return ok;
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

static bool LoadBinaryFileIntoMemory (Cpu & cpu,
                                      const std::string & inputFile,
                                      Word loadAddr,
                                      Word & entryPoint)
{
    std::string  contents;
    size_t       i       = 0;
    bool         wasRead = ReadFileContents (inputFile, contents);



    if (wasRead)
    {
        for (i = 0; i < contents.size(); i++)
        {
            cpu.PokeByte (loadAddr + (Word) i, (Byte) contents[i]);
        }

        entryPoint = loadAddr;
    }
    else
    {
        std::cerr << "Error: Cannot read input file: " << inputFile << "\n";
    }

    return wasRead;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunCpu
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
//  ParseAs65Flags - AS65-compatible flag parsing with concatenation
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
            char flag = arg[pos];
            std::string rest = arg.substr (pos + 1);

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

                    if (ParseDecimal (rest.c_str(), val))
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

                    if (ParseDecimal (rest.c_str(), val))
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
                    size_t eqPos = def.find ('=');
                    std::string name;
                    int32_t     value = 1;

                    if (eqPos != std::string::npos)
                    {
                        name = def.substr (0, eqPos);
                        std::string valStr = def.substr (eqPos + 1);
                        char * end = nullptr;
                        long   v   = strtol (valStr.c_str(), &end, 0);

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
            if (!ParseFillByte (argv[++argIndex], options.fillByte))
            {
                std::cerr << "Error: Invalid fill byte value\n";
            }
        }
        else if (arg == "--load" && argIndex + 1 < argc)
        {
            if (ParseAddress (argv[++argIndex], options.loadAddress))
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
            if (ParseAddress (argv[++argIndex], options.entryAddress))
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
            if (ParseAddress (argv[++argIndex], options.stopAddress))
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
            if (!ParseDecimal (argv[++argIndex], options.maxCycles))
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
        wasLoaded = LoadBinaryFileIntoMemory (cpu, options.inputFile, loadAddr, entryPoint);
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
//  DoAs65 - AS65-compatible assembly mode
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
    wasWritten = !options.generateListing || WriteListingOutput (ar.result, options);
    exitCode   = wasWritten ? 0 : 2;

    BAIL_OUT_IF (!wasWritten, S_OK);

    wasWritten = WriteBinaryOutput (ar.result, options);
    exitCode   = wasWritten ? 0 : 2;

    BAIL_OUT_IF (!wasWritten, S_OK);

    if (options.symbolTable)
    {
        WriteSymbolTableOutput (ar.result);
    }

    wasWritten = !options.debugInfo || options.debugFile.empty() ||
                 WriteDebugInfoOutput (ar.result, options.debugFile);
    exitCode   = wasWritten ? 0 : 2;

    BAIL_OUT_IF (!wasWritten, S_OK);

    wasWritten = options.symbolFile.empty() ||
                 WriteSymbolFile (options.symbolFile, ar.result.symbols);
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
