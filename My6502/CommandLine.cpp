#include "Pch.h"

#include "CommandLine.h"
#include "Assembler.h"
#include "Cpu.h"
#include "Microcode.h"



static const char * const s_version = "1.0.0";


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

    if (first == "--help" || first == "-h")
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
    if (first == "assemble")
    {
        options.subcommand = CommandLineOptions::Subcommand::Assemble;
        argIndex = 2;
    }
    else if (first == "run")
    {
        options.subcommand = CommandLineOptions::Subcommand::Run;
        argIndex = 2;
    }
    else
    {
        // Unknown subcommand — show help
        options.showHelp = true;
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
    std::cout << "My6502 — 6502 Assembler and Emulator\n"
              << "\n"
              << "Usage:\n"
              << "  My6502 assemble <input.asm> -o <output.bin> [options]\n"
              << "  My6502 run <input> [options]\n"
              << "  My6502 --help\n"
              << "  My6502 --version\n"
              << "\n"
              << "Assemble options:\n"
              << "  -o <file>           Output binary file (required)\n"
              << "  -l <file>           Write symbol table to file\n"
              << "  -a                  Print listing to stdout\n"
              << "  --fill <byte>       Fill byte for unused space (default: $FF)\n"
              << "  -v                  Verbose output\n"
              << "  --warn              Show warnings (default)\n"
              << "  --no-warn           Suppress warnings\n"
              << "  --fatal-warnings    Treat warnings as errors\n"
              << "\n"
              << "Run options:\n"
              << "  --load <addr>       Load address for binary files (default: $8000)\n"
              << "  --entry <addr>      Entry point address\n"
              << "  --reset-vector      Use reset vector at $FFFC/$FFFD\n"
              << "  --stop <addr>       Stop when PC reaches address\n"
              << "  --max-cycles <n>    Maximum cycles before stopping\n"
              << "  -v                  Verbose output\n";
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrintVersion
//
////////////////////////////////////////////////////////////////////////////////

void PrintVersion ()
{
    std::cout << "My6502 version " << s_version << "\n";
}





////////////////////////////////////////////////////////////////////////////////
//
//  DoAssemble
//
////////////////////////////////////////////////////////////////////////////////

int DoAssemble (const CommandLineOptions & options)
{
    // Validate required arguments
    if (options.inputFile.empty ())
    {
        std::cerr << "Error: No input file specified\n";
        return 2;
    }

    if (options.outputFile.empty ())
    {
        std::cerr << "Error: No output file specified (use -o)\n";
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
        std::cerr << "Assembling: " << options.inputFile << "\n";
    }

    // Set up assembler options
    AssemblerOptions asmOptions = {};
    asmOptions.fillByte        = options.fillByte;
    asmOptions.generateListing = options.generateListing;
    asmOptions.warningMode     = options.warningMode;

    // Create assembler and assemble
    Cpu cpu;
    cpu.Reset ();

    auto startTime = std::chrono::high_resolution_clock::now ();

    Assembler  asm6502 (cpu.GetInstructionSet (), asmOptions);
    auto       result = asm6502.Assemble (source);

    auto endTime = std::chrono::high_resolution_clock::now ();

    // Print warnings
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
        return 1;
    }

    // Print listing if requested
    if (options.generateListing)
    {
        for (const auto & line : result.listing)
        {
            std::cout << Assembler::FormatListingLine (line) << "\n";
        }
    }

    // Write output binary
    if (!WriteBinaryFile (options.outputFile, result.bytes))
    {
        std::cerr << "Error: Cannot write output file: " << options.outputFile << "\n";
        return 2;
    }

    // Write symbol file if requested
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
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds> (endTime - startTime);

        std::cerr << "Assembly successful\n";
        std::cerr << "  Output:  " << options.outputFile << "\n";
        std::cerr << "  Bytes:   " << result.bytes.size () << "\n";

        char addrBuf[16];
        snprintf (addrBuf, sizeof (addrBuf), "$%04X", result.startAddress);
        std::cerr << "  Start:   " << addrBuf << "\n";

        snprintf (addrBuf, sizeof (addrBuf), "$%04X", result.endAddress);
        std::cerr << "  End:     " << addrBuf << "\n";

        std::cerr << "  Symbols: " << result.symbols.size () << "\n";
        std::cerr << "  Time:    " << elapsed.count () << " us\n";
    }

    return 0;
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
