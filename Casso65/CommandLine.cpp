#include "Pch.h"

#include "CommandLine.h"
#include "Assembler.h"
#include "Cpu.h"
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
//  ParseAddress
//
////////////////////////////////////////////////////////////////////////////////

static bool ParseAddress (const char * text, Word & address)
{
    if (text == nullptr || text[0] == '\0')
    {
        return false;
    }

    const char * hex = text;

    if (hex[0] == '$')
    {
        hex++;
    }

    char * end   = nullptr;
    long   value = strtol (hex, &end, 16);

    if (end == hex || *end != '\0' || value < 0 || value > 0xFFFF)
    {
        return false;
    }

    address = (Word) value;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ParseDecimal
//
////////////////////////////////////////////////////////////////////////////////

static bool ParseDecimal (const char * text, uint32_t & value)
{
    if (text == nullptr || text[0] == '\0')
    {
        return false;
    }

    char * end  = nullptr;
    long   val  = strtol (text, &end, 10);

    if (end == text || *end != '\0' || val < 0)
    {
        return false;
    }

    value = (uint32_t) val;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ParseFillByte
//
////////////////////////////////////////////////////////////////////////////////

static bool ParseFillByte (const char * text, Byte & fillByte)
{
    if (text == nullptr || text[0] == '\0')
    {
        return false;
    }

    const char * hex = text;

    if (hex[0] == '$')
    {
        hex++;
    }

    char * end   = nullptr;
    long   value = strtol (hex, &end, 16);

    if (end == hex || *end != '\0' || value < 0 || value > 0xFF)
    {
        return false;
    }

    fillByte = (Byte) value;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadFileContents
//
////////////////////////////////////////////////////////////////////////////////

static bool ReadFileContents (const std::string & path, std::string & contents)
{
    std::ifstream file (path);

    if (!file.is_open ())
    {
        return false;
    }

    std::ostringstream ss;
    ss << file.rdbuf ();
    contents = ss.str ();
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteBinaryFile
//
////////////////////////////////////////////////////////////////////////////////

static bool WriteBinaryFile (const std::string & path, const std::vector<Byte> & data)
{
    std::ofstream file (path, std::ios::binary);

    if (!file.is_open ())
    {
        return false;
    }

    file.write (reinterpret_cast<const char *> (data.data ()), data.size ());
    return file.good ();
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
    std::ofstream file (path, std::ios::binary);

    if (!file.is_open ())
    {
        return false;
    }

    // Pad from address 0 to startAddress
    for (Word i = 0; i < startAddress; i++)
    {
        file.put ((char) fillByte);
    }

    // Write assembled bytes
    file.write (reinterpret_cast<const char *> (data.data ()), data.size ());

    // Pad from end to fill full 64KB address space
    uint32_t endAddress = (uint32_t) startAddress + (uint32_t) data.size ();

    for (uint32_t i = endAddress; i < 0x10000u; i++)
    {
        file.put ((char) fillByte);
    }

    return file.good ();
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteSymbolFile
//
////////////////////////////////////////////////////////////////////////////////

static bool WriteSymbolFile (const std::string & path, const std::unordered_map<std::string, Word> & symbols)
{
    std::ofstream file (path);

    if (!file.is_open ())
    {
        return false;
    }

    // Sort symbols by address for deterministic output
    std::vector<std::pair<std::string, Word>> sorted (symbols.begin (), symbols.end ());

    std::sort (sorted.begin (), sorted.end (),
        [] (const auto & a, const auto & b) { return a.second < b.second; });

    for (const auto & pair : sorted)
    {
        char buf[32];
        snprintf (buf, sizeof (buf), "$%04X  %s\n", pair.second, pair.first.c_str ());
        file << buf;
    }

    return file.good ();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EndsWith
//
////////////////////////////////////////////////////////////////////////////////

static bool EndsWith (const std::string & str, const std::string & suffix)
{
    if (suffix.size () > str.size ())
    {
        return false;
    }

    std::string strEnd = str.substr (str.size () - suffix.size ());

    for (auto & c : strEnd)
    {
        c = (char) tolower ((unsigned char) c);
    }

    std::string suffLower = suffix;

    for (auto & c : suffLower)
    {
        c = (char) tolower ((unsigned char) c);
    }

    return strEnd == suffLower;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileExists
//
////////////////////////////////////////////////////////////////////////////////

static bool FileExists (const std::string & path)
{
    std::ifstream f (path);
    return f.good ();
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryAutoExtend - if file has no extension, try common source extensions
//
////////////////////////////////////////////////////////////////////////////////

static std::string TryAutoExtend (const std::string & path)
{
    // If the file already has an extension, return as-is
    size_t dot  = path.rfind ('.');
    size_t sep  = path.find_last_of ("/\\");

    if (dot != std::string::npos && (sep == std::string::npos || dot > sep))
    {
        return path;
    }

    // Try common source extensions
    static const char * extensions[] = { ".a65", ".asm", ".s", nullptr };

    for (int i = 0; extensions[i] != nullptr; i++)
    {
        std::string candidate = path + extensions[i];

        if (FileExists (candidate))
        {
            return candidate;
        }
    }

    return path;
}





////////////////////////////////////////////////////////////////////////////////
//
//  StripExtension
//
////////////////////////////////////////////////////////////////////////////////

static std::string StripExtension (const std::string & path)
{
    size_t dot = path.rfind ('.');
    size_t sep = path.find_last_of ("/\\");

    if (dot != std::string::npos && (sep == std::string::npos || dot > sep))
    {
        return path.substr (0, dot);
    }

    return path;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ParseAs65Flags - AS65-compatible flag parsing with concatenation
//
////////////////////////////////////////////////////////////////////////////////

static void ParseAs65Flags (int argc, char * argv[], CommandLineOptions & options)
{
    options.subcommand = CommandLineOptions::Subcommand::As65;
    int argIndex = 1;



    while (argIndex < argc)
    {
        std::string arg (argv[argIndex]);

        // Check for help requests
        if (arg == "--help" || arg == "-?" || arg == "/?")
        {
            options.showHelp = true;
            return;
        }

        // Non-flag argument is the input file
        if (arg[0] != '-' && arg[0] != '/')
        {
            if (options.inputFile.empty ())
            {
                options.inputFile = arg;
            }

            argIndex++;
            continue;
        }

        // Parse concatenated flags: -tlfile means -t -lfile
        size_t pos = 1;

        while (pos < arg.size ())
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

                if (!rest.empty ())
                {
                    options.listingFile = rest;
                    pos = arg.size ();
                }
                else
                {
                    options.listingToStdout = true;
                    pos++;
                }

                break;

            case 'o':
                if (!rest.empty ())
                {
                    options.outputFile = rest;
                    pos = arg.size ();
                }
                else if (argIndex + 1 < argc)
                {
                    options.outputFile = argv[++argIndex];
                    pos = arg.size ();
                }

                break;

            case 'm':
                options.macroExpansion = true;
                pos++;
                break;

            case 'h':
            {
                if (!rest.empty ())
                {
                    uint32_t val = 0;

                    if (ParseDecimal (rest.c_str (), val))
                    {
                        options.pageHeight = (int) val;
                    }

                    pos = arg.size ();
                }
                else
                {
                    pos++;
                }

                break;
            }

            case 'w':
            {
                if (!rest.empty ())
                {
                    uint32_t val = 0;

                    if (ParseDecimal (rest.c_str (), val))
                    {
                        options.pageWidth = (int) val;
                    }

                    pos = arg.size ();
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

                if (!rest.empty ())
                {
                    options.debugFile = rest;
                    pos = arg.size ();
                }
                else
                {
                    pos++;
                }

                break;

            case 'd':
            {
                std::string def = rest;

                if (def.empty () && argIndex + 1 < argc)
                {
                    def = argv[++argIndex];
                }

                if (!def.empty ())
                {
                    size_t eqPos = def.find ('=');
                    std::string name;
                    int32_t     value = 1;

                    if (eqPos != std::string::npos)
                    {
                        name = def.substr (0, eqPos);
                        std::string valStr = def.substr (eqPos + 1);
                        char * end = nullptr;
                        long   v   = strtol (valStr.c_str (), &end, 0);

                        if (end != valStr.c_str ())
                        {
                            value = (int32_t) v;
                        }
                    }
                    else
                    {
                        name = def;
                    }

                    if (!name.empty ())
                    {
                        options.predefinedSymbols[name] = value;
                    }
                }

                pos = arg.size ();
                break;
            }

            case 's':
                // -s = S-record output (.s19), -s2 = Intel HEX output (.hex)
                if (!rest.empty () && rest[0] == '2')
                {
                    options.outputFormat = CommandLineOptions::OutputFormat::IntelHex;

                    if (rest.size () > 1)
                    {
                        options.outputFile = rest.substr (1);
                    }
                }
                else
                {
                    options.outputFormat = CommandLineOptions::OutputFormat::SRecord;

                    if (!rest.empty ())
                    {
                        options.outputFile = rest;
                    }
                }

                pos = arg.size ();
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
    CommandLineOptions options = {};

    if (argc < 2)
    {
        options.showHelp = true;
        return options;
    }

    int argIndex = 1;

    // Check for --help / --version first
    std::string first (argv[1]);

    if (first == "--help" || first == "-h" || first == "-?" || first == "/?")
    {
        options.subcommand = CommandLineOptions::Subcommand::Help;
        options.showHelp   = true;
        return options;
    }

    if (first == "--version")
    {
        options.subcommand  = CommandLineOptions::Subcommand::Version;
        options.showVersion = true;
        return options;
    }

    // Parse subcommand
    if (first == "run")
    {
        options.subcommand = CommandLineOptions::Subcommand::Run;
        argIndex = 2;
    }
    else
    {
        // No recognized subcommand - treat as AS65 mode
        ParseAs65Flags (argc, argv, options);

        // Auto-extend input file
        if (!options.inputFile.empty ())
        {
            options.inputFile = TryAutoExtend (options.inputFile);
        }

        // Auto-generate output file if not specified
        if (options.outputFile.empty () && !options.inputFile.empty ())
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
        if (options.debugInfo && options.debugFile.empty () && !options.inputFile.empty ())
        {
            options.debugFile = StripExtension (options.inputFile) + ".dbg";
        }

        return options;
    }

    // Parse remaining arguments
    while (argIndex < argc)
    {
        std::string arg (argv[argIndex]);

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
        else if (arg[0] != '-' && options.inputFile.empty ())
        {
            options.inputFile = arg;
        }
        else
        {
            std::cerr << "Error: Unknown option: " << arg << "\n";
        }

        argIndex++;
    }

    return options;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrintUsage
//
////////////////////////////////////////////////////////////////////////////////

void PrintUsage ()
{
    std::cout << "Casso65 - 6502 Assembler and Emulator  v" VERSION_STRING " (" << arch << ")\n"
              << "Copyright (c) 2025-" VERSION_YEAR_STRING " by Robert Elmer\n"
              << "Built: " VERSION_BUILD_TIMESTAMP "\n"
              << "\n"
              << "Usage:\n"
              << "  Casso65 <input> [flags]               (assemble)\n"
              << "  Casso65 run <input> [options]          (run binary)\n"
              << "  Casso65 --help | -? | /?\n"
              << "  Casso65 --version\n"
              << "\n"
              << "Run options:\n"
              << "  --load <addr>       Load address for binary files (default: $8000)\n"
              << "  --entry <addr>      Entry point address\n"
              << "  --reset-vector      Use reset vector at $FFFC/$FFFD\n"
              << "  --stop <addr>       Stop when PC reaches address\n"
              << "  --max-cycles <n>    Maximum cycles before stopping\n"
              << "  -v                  Verbose output\n"
              << "\n"
              << "Assembly options (no subcommand):\n"
              << "  -c                  Show cycle counts in listing\n"
              << "  -d <name>[=<value>] Pre-define symbol\n"
              << "  -g                  Generate debug information file\n"
              << "  -h <lines>          Page height for listing (0 = no pagination)\n"
              << "  -i                  Case-insensitive (default, accepted as no-op)\n"
              << "  -l [<file>]         Generate listing (-l alone = stdout, -lfile = to file)\n"
              << "  -m                  Show macro expansions in listing\n"
              << "  -n                  Disable optimizations (accepted as no-op)\n"
              << "  -o <file>           Output file (default: input with .bin extension)\n"
              << "  -p                  Generate pass 1 listing\n"
              << "  -q                  Quiet mode (suppress progress)\n"
              << "  -s                  Output S-record format (.s19)\n"
              << "  -s2                 Output Intel HEX format (.hex)\n"
              << "  -t                  Generate symbol table\n"
              << "  -v                  Verbose mode\n"
              << "  -w [<width>]        Column width (default: 79, -w alone = 133)\n"
              << "  -z                  Fill unused space with $00 (default: $FF)\n"
              << "\n"
              << "  Flags can be concatenated: -tlfile = -t -lfile\n"
              << "  Input file auto-extends: .a65, .asm, .s\n";
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrintVersion
//
////////////////////////////////////////////////////////////////////////////////

void PrintVersion ()
{
    std::cout << "Casso65 v" VERSION_STRING " (" << arch << ")\n";
}





////////////////////////////////////////////////////////////////////////////////
//
//  DoRun
//
////////////////////////////////////////////////////////////////////////////////

int DoRun (const CommandLineOptions & options)
{
    if (options.inputFile.empty ())
    {
        std::cerr << "Error: No input file specified\n";
        return 2;
    }

    Cpu cpu;
    cpu.Reset ();

    Word entryPoint  = 0x8000;
    bool isAssembly  = EndsWith (options.inputFile, ".asm") || EndsWith (options.inputFile, ".s");

    if (isAssembly)
    {
        // Assemble the source file first
        std::string source;

        if (!ReadFileContents (options.inputFile, source))
        {
            std::cerr << "Error: Cannot read input file: " << options.inputFile << "\n";
            return 2;
        }

        if (options.verbose)
        {
            std::cerr << "Assembling: " << options.inputFile << "\n";
        }

        AssemblerOptions asmOptions = {};
        asmOptions.warningMode = options.warningMode;

        Assembler  asm6502 (cpu.GetInstructionSet (), asmOptions);
        auto       result = asm6502.Assemble (source);

        // Print warnings and errors
        for (const auto & warning : result.warnings)
        {
            std::cerr << options.inputFile << ":" << warning.lineNumber << ": warning: " << warning.message << "\n";
        }

        for (const auto & error : result.errors)
        {
            std::cerr << options.inputFile << ":" << error.lineNumber << ": error: " << error.message << "\n";
        }

        if (!result.success)
        {
            std::cerr << "Assembly failed with " << result.errors.size () << " error(s)\n";
            return 1;
        }

        // Load assembled bytes into CPU memory
        Word loadAddr = result.startAddress;

        for (size_t i = 0; i < result.bytes.size (); i++)
        {
            cpu.PokeByte (loadAddr + (Word) i, result.bytes[i]);
        }

        entryPoint = result.startAddress;

        if (options.verbose)
        {
            std::cerr << "Assembled " << result.bytes.size () << " bytes\n";

            char buf[16];
            snprintf (buf, sizeof (buf), "$%04X", result.startAddress);
            std::cerr << "  Start: " << buf << "\n";
        }
    }
    else
    {
        // Load binary file
        std::string contents;

        if (!ReadFileContents (options.inputFile, contents))
        {
            std::cerr << "Error: Cannot read input file: " << options.inputFile << "\n";
            return 2;
        }

        Word loadAddr = options.hasLoadAddress ? options.loadAddress : (Word) 0x8000;

        for (size_t i = 0; i < contents.size (); i++)
        {
            cpu.PokeByte (loadAddr + (Word) i, (Byte) contents[i]);
        }

        entryPoint = loadAddr;

        if (options.verbose)
        {
            std::cerr << "Loaded " << contents.size () << " bytes at $"
                      << std::hex << loadAddr << std::dec << "\n";
        }
    }

    // Determine entry point
    if (options.hasEntryAddress)
    {
        entryPoint = options.entryAddress;
    }
    else if (options.useResetVector)
    {
        entryPoint = cpu.PeekWord (0xFFFC);
    }

    // Set PC and run
    cpu.SetPC (entryPoint);

    if (options.verbose)
    {
        char buf[16];
        snprintf (buf, sizeof (buf), "$%04X", entryPoint);
        std::cerr << "Executing from " << buf << "\n";
    }

    uint32_t cycles    = 0;
    bool     stopped   = false;
    int      exitCode  = 0;

    while (!stopped)
    {
        if (options.maxCycles > 0 && cycles >= options.maxCycles)
        {
            if (options.verbose)
            {
                std::cerr << "Stopped: cycle limit reached (" << options.maxCycles << ")\n";
            }

            stopped = true;
            break;
        }

        Byte opcode = cpu.PeekByte (cpu.GetPC ());

        if (!cpu.GetMicrocode (opcode).isLegal)
        {
            char buf[16];
            snprintf (buf, sizeof (buf), "$%04X", cpu.GetPC ());
            std::cerr << "Illegal opcode $" << std::hex << (int) opcode << " at " << buf << "\n";
            exitCode = 3;
            stopped  = true;
            break;
        }

        if (options.hasStopAddress && cpu.GetPC () == options.stopAddress)
        {
            if (options.verbose)
            {
                char buf[16];
                snprintf (buf, sizeof (buf), "$%04X", options.stopAddress);
                std::cerr << "Stopped at address " << buf << "\n";
            }

            stopped = true;
            break;
        }

        // BRK instruction (opcode 0x00) also stops
        if (opcode == 0x00)
        {
            if (options.verbose)
            {
                char buf[16];
                snprintf (buf, sizeof (buf), "$%04X", cpu.GetPC ());
                std::cerr << "BRK encountered at " << buf << "\n";
            }

            stopped = true;
            break;
        }

        cpu.StepOne ();
        cycles++;
    }

    if (options.verbose)
    {
        std::cerr << "Execution complete: " << cycles << " cycle(s)\n";
        std::cerr << "  A=$" << std::hex << (int) cpu.GetA ()
                  << " X=$" << (int) cpu.GetX ()
                  << " Y=$" << (int) cpu.GetY ()
                  << " SP=$" << (int) cpu.GetSP ()
                  << " PC=$" << cpu.GetPC ()
                  << std::dec << "\n";
    }

    return exitCode;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DoAs65 - AS65-compatible assembly mode
//
////////////////////////////////////////////////////////////////////////////////

int DoAs65 (const CommandLineOptions & options)
{
    // Validate input
    if (options.inputFile.empty ())
    {
        std::cerr << "Error: No input file specified\n";
        return 2;
    }

    // Read input file
    std::string source;

    if (!ReadFileContents (options.inputFile, source))
    {
        std::cerr << "Error: Cannot read input file: " << options.inputFile << "\n";
        return 2;
    }

    if (options.verbose)
    {
        std::cerr << "Pass 1...\n";
    }

    // Set up assembler options
    AssemblerOptions asmOptions = {};
    asmOptions.fillByte        = options.fillByte;
    asmOptions.generateListing = options.generateListing;
    asmOptions.warningMode     = options.warningMode;
    asmOptions.cycleCounts     = options.cycleCounts;
    asmOptions.macroExpansion  = options.macroExpansion;
    asmOptions.pageHeight      = options.pageHeight;
    asmOptions.pageWidth       = options.pageWidth;
    asmOptions.pass1Listing    = options.pass1Listing;
    asmOptions.symbolTable     = options.symbolTable;
    asmOptions.debugInfo       = options.debugInfo;
    asmOptions.verbose         = options.verbose;
    asmOptions.quiet           = options.quiet;
    asmOptions.disableOpt      = options.disableOpt;
    asmOptions.predefinedSymbols = options.predefinedSymbols;

    // Set up file reader for includes
    DefaultFileReader fileReader;
    asmOptions.fileReader = &fileReader;

    // Extract base directory from input file
    size_t lastSep = options.inputFile.find_last_of ("/\\");

    if (lastSep != std::string::npos)
    {
        asmOptions.baseDir = options.inputFile.substr (0, lastSep);
    }

    // Create assembler and assemble
    Cpu cpu;
    cpu.Reset ();

    auto startTime = std::chrono::high_resolution_clock::now ();

    Assembler  asm6502 (cpu.GetInstructionSet (), asmOptions);
    auto       result = asm6502.Assemble (source);

    auto endTime = std::chrono::high_resolution_clock::now ();

    if (options.verbose)
    {
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds> (endTime - startTime);
        std::cerr << "Pass 2...\n";
        std::cerr << "Assembly time: " << elapsed.count () << " us\n";
    }

    // Print warnings
    bool hasWarnings = !result.warnings.empty ();

    for (const auto & warning : result.warnings)
    {
        std::cerr << options.inputFile << ":" << warning.lineNumber << ": warning: " << warning.message << "\n";
    }

    // Print errors
    for (const auto & error : result.errors)
    {
        std::cerr << options.inputFile << ":" << error.lineNumber << ": error: " << error.message << "\n";
    }

    if (!result.success)
    {
        std::cerr << "Assembly failed with " << result.errors.size () << " error(s)\n";
        return 2;
    }

    // Line counter on stderr unless -q
    if (!options.quiet)
    {
        std::cerr << result.listing.size () << " lines assembled\n";
    }

    // Print listing to stdout or file
    if (options.generateListing)
    {
        std::ostream * listOut = &std::cout;
        std::ofstream  listFile;

        if (!options.listingFile.empty ())
        {
            listFile.open (options.listingFile);

            if (!listFile.is_open ())
            {
                std::cerr << "Error: Cannot write listing file: " << options.listingFile << "\n";
                return 2;
            }

            listOut = &listFile;
        }

        // Print title if set
        if (!result.listingTitle.empty ())
        {
            *listOut << result.listingTitle << "\n\n";
        }

        for (const auto & line : result.listing)
        {
            // Handle .page directive (form feed)
            if (line.sourceText.find (".page") != std::string::npos ||
                line.sourceText.find (".PAGE") != std::string::npos ||
                line.sourceText.find ("page") == 0)
            {
                *listOut << "\f";

                if (!result.listingTitle.empty ())
                {
                    *listOut << result.listingTitle << "\n\n";
                }
            }

            *listOut << Assembler::FormatListingLine (line, options.cycleCounts) << "\n";
        }
    }

    // Write output binary (skip if "nul")
    std::string outLower = options.outputFile;

    for (auto & c : outLower)
    {
        c = (char) tolower ((unsigned char) c);
    }

    if (outLower != "nul")
    {
        // Determine output format from extension
        if (EndsWith (options.outputFile, ".s19"))
        {
            std::ofstream outFile (options.outputFile);

            if (!outFile.is_open ())
            {
                std::cerr << "Error: Cannot write output file: " << options.outputFile << "\n";
                return 2;
            }

            OutputFormats::WriteSRecord (result.bytes, result.startAddress, result.endAddress, result.startAddress, outFile);
        }
        else if (EndsWith (options.outputFile, ".hex"))
        {
            std::ofstream outFile (options.outputFile);

            if (!outFile.is_open ())
            {
                std::cerr << "Error: Cannot write output file: " << options.outputFile << "\n";
                return 2;
            }

            OutputFormats::WriteIntelHex (result.bytes, result.startAddress, result.endAddress, result.startAddress, outFile);
        }
        else
        {
            if (!WriteFlatBinaryFile (options.outputFile, result.bytes, result.startAddress, options.fillByte))
            {
                std::cerr << "Error: Cannot write output file: " << options.outputFile << "\n";
                return 2;
            }
        }
    }

    // Write symbol table if -t
    if (options.symbolTable)
    {
        std::cout << "\nSymbol Table:\n";
        std::cout << Assembler::FormatSymbolTable (result.symbols, result.symbolKinds);
    }

    // Write debug info file if -g
    if (options.debugInfo && !options.debugFile.empty ())
    {
        std::ofstream dbgFile (options.debugFile);

        if (!dbgFile.is_open ())
        {
            std::cerr << "Error: Cannot write debug file: " << options.debugFile << "\n";
            return 2;
        }

        dbgFile << Assembler::FormatDebugInfo (result.symbols);
    }

    // Write symbol file for Casso65-mode compatibility
    if (!options.symbolFile.empty ())
    {
        if (!WriteSymbolFile (options.symbolFile, result.symbols))
        {
            std::cerr << "Error: Cannot write symbol file: " << options.symbolFile << "\n";
            return 2;
        }
    }

    if (options.verbose)
    {
        std::cerr << "Assembly successful\n";
        std::cerr << "  Output:  " << options.outputFile << "\n";
        std::cerr << "  Bytes:   " << result.bytes.size () << "\n";

        char addrBuf[16];
        snprintf (addrBuf, sizeof (addrBuf), "$%04X", result.startAddress);
        std::cerr << "  Start:   " << addrBuf << "\n";

        snprintf (addrBuf, sizeof (addrBuf), "$%04X", result.endAddress);
        std::cerr << "  End:     " << addrBuf << "\n";

        std::cerr << "  Symbols: " << result.symbols.size () << "\n";
    }

    // AS65 exit codes: 0=success, 1=warnings, 2=errors
    return hasWarnings ? 1 : 0;
}
