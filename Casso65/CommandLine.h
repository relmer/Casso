#pragma once

#include "AssemblerTypes.h"


////////////////////////////////////////////////////////////////////////////////
//
//  CommandLineOptions
//
////////////////////////////////////////////////////////////////////////////////

struct CommandLineOptions
{
    enum class Subcommand { None, Assemble, Run, Help, Version };

    Subcommand  subcommand      = Subcommand::None;
    std::string inputFile;
    std::string outputFile;
    std::string symbolFile;
    Byte        fillByte        = 0xFF;
    Word        loadAddress     = 0x8000;
    Word        stopAddress     = 0;
    Word        entryAddress    = 0;
    uint32_t    maxCycles       = 0;
    bool        useResetVector  = false;
    bool        verbose         = false;
    bool        generateListing = false;
    WarningMode warningMode     = WarningMode::Warn;
    bool        showVersion     = false;
    bool        showHelp        = false;
    bool        hasLoadAddress  = false;
    bool        hasStopAddress  = false;
    bool        hasEntryAddress = false;
};



CommandLineOptions ParseCommandLine (int argc, char * argv[]);
int  DoAssemble  (const CommandLineOptions & options);
int  DoRun       (const CommandLineOptions & options);
void PrintUsage  ();
void PrintVersion ();
