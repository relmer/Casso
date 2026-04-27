#pragma once

#include "AssemblerTypes.h"


////////////////////////////////////////////////////////////////////////////////
//
//  CommandLineOptions
//
////////////////////////////////////////////////////////////////////////////////

struct CommandLineOptions
{
    enum class Subcommand    { None, Run, Help, Version, As65 };
    enum class OutputFormat  { Binary, SRecord, IntelHex };

    Subcommand  subcommand      = Subcommand::None;
    OutputFormat outputFormat    = OutputFormat::Binary;
    std::string inputFile;
    std::string outputFile;
    std::string listingFile;     // -l<file> listing output file
    std::string symbolFile;
    std::string debugFile;       // -g debug info output file
    Byte        fillByte        = 0xFF;
    Word        loadAddress     = 0x8000;
    Word        stopAddress     = 0;
    Word        entryAddress    = 0;
    uint32_t    maxCycles       = 0;
    bool        useResetVector  = false;
    bool        verbose         = false;
    bool        generateListing = false;
    bool        listingToStdout = false;
    WarningMode warningMode     = WarningMode::Warn;
    bool        showVersion     = false;
    bool        showHelp        = false;
    bool        hasLoadAddress  = false;
    bool        hasStopAddress  = false;
    bool        hasEntryAddress = false;

    // AS65-compatible options
    bool        cycleCounts     = false;   // -c
    bool        macroExpansion  = false;   // -m
    int         pageHeight      = 0;       // -h<N>
    int         pageWidth       = 80;      // -w<N>
    bool        caseSensitive   = false;   // -i (no-op)
    bool        pass1Listing    = false;   // -p
    bool        symbolTable     = false;   // -t
    bool        debugInfo       = false;   // -g
    bool        quiet           = false;   // -q
    bool        disableOpt      = false;   // -n (no-op)
    bool        fillZero        = false;   // -z
    std::unordered_map<std::string, int32_t> predefinedSymbols; // -d
};



CommandLineOptions ParseCommandLine (int argc, char * argv[]);
int  DoRun       (const CommandLineOptions & options);
int  DoAs65      (const CommandLineOptions & options);
void PrintUsage  ();
void PrintVersion ();
