#pragma once

#include "CommandLineOptions.h"
#include "CommandLineParser.h"
#include "Assembler.h"
#include "Cpu.h"
#include "DialectReporting.h"
#include "Microcode.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLine
//
//  The executable's half of the command line: the subcommand bodies and the
//  usage text. Everything that DECIDES what an argv means lives in
//  CassoCore/CommandLineParser, where the UnitTest project can reach it; what
//  remains here is the platform edge -- reading source, writing artifacts, and
//  printing.
//
//  The helpers are private statics rather than file-scope functions, so the
//  eight entry points below are the whole of what the executable can call and
//  the other twenty-seven are visibly the inside of one thing.
//
////////////////////////////////////////////////////////////////////////////////

class CommandLine
{
public:
    static CommandLineOptions  Parse                     (int argc, char * argv[]);

    static int                 DoRun                     (const CommandLineOptions & options);
    static int                 DoAs65                    (const CommandLineOptions & options);
    static int                 DoMerlin                  (const CommandLineOptions & options);

    static void                PrintUsage                (char prefix);
    static void                PrintVersion              ();

    //  A first word that named no subcommand. Deliberately NOT the usage
    //  block: see the note at the call site.
    static void                PrintUnrecognizedArgument (const std::string & word);

    //  A CPU flag the active dialect does not take. The sentence is composed
    //  in core, where the dialect's own data is; this prints it and says what
    //  the process is about to return.
    static int                 PrintCpuFlagRefusal       (const std::string & refusal);

private:
    ////////////////////////////////////////////////////////////////////////////
    //
    //  AssembleResult
    //
    //  What one assembly produced, and whether it produced it.
    //
    ////////////////////////////////////////////////////////////////////////////

    struct AssembleResult
    {
        AssemblyResult result;
        bool           ok = false;      // default: treat an unfilled result as failure
        std::string    inputFile;
    };

    //  Files
    static HRESULT                           ReadFileContents         (const std::string & path, std::string & contents);
    static HRESULT                           WriteBinaryFormatFile    (const std::string & path,
                                                                       const AssemblyResult & result,
                                                                       CommandLineOptions::OutputFormat format,
                                                                       Byte fillByte);
    static HRESULT                           WriteSymbolFile          (const std::string & path, const std::unordered_map<std::string, Word> & symbols);
    static bool                              FileExists               (const std::string & path);

    //  Assembling
    static const Microcode *                 SelectInstructionSet     (const CommandLineOptions & options, const Cpu & cpu);
    static AssemblerOptions                  BuildAssemblerOptions    (const CommandLineOptions & options);
    static AssembleResult                    AssembleFile             (const std::string & inputFile,
                                                                       const Microcode instructionSet[256],
                                                                       const Microcode extendedSet[256],
                                                                       const AssemblerOptions & asmOptions);
    static void                              ReportAssemblyDiagnostics (const AssembleResult & ar);
    static CpuReport                         BuildCpuReport           (const CommandLineOptions & options, const AssemblyResult & result);
    static void                              ReportToStandardError    (const std::vector<DialectReportLine> & reports);

    //  Output
    static CommandLineOptions::OutputFormat  ResolveOutputFormat      (const CommandLineOptions & options);
    static HRESULT                           WriteBinaryOutput        (const AssemblyResult & result,
                                                                       const CommandLineOptions & options);
    static HRESULT                           WriteListingOutput       (const AssemblyResult & result,
                                                                       const CommandLineOptions & options,
                                                                       const std::vector<DialectReportLine> & reports);
    static void                              WriteSymbolTableOutput   (const AssemblyResult & result);
    static HRESULT                           WriteDebugInfoOutput     (const AssemblyResult & result,
                                                                       const std::string & debugFile);
    static std::string                       ResolveMerlinOutputName  (const CommandLineOptions & options, const AssemblyResult & result);

    //  Running
    static void                              LoadAssembledIntoMemory  (Cpu & cpu, const AssemblyResult & result);
    static HRESULT                           LoadBinaryFileIntoMemory (Cpu & cpu,
                                                                       const std::string & inputFile,
                                                                       Word loadAddr,
                                                                       Word & entryPoint);
    static int                               RunCpu                   (Cpu & cpu,
                                                                       const CommandLineOptions & options,
                                                                       Word entryPoint,
                                                                       std::vector<std::string> & status);

    //  Usage
    static size_t                            UsageWidth               ();
    static void                              Say                      (const std::string & line);
    static void                              SayBlock                 (const std::string & block);
    static void                              PrintSectionHeading      (const std::string & name);
    static void                              PrintUsageHeader         (const char * sp, const char * lp);
    static void                              PrintUsageGeneral        (const char * lp, const char * sp, const char * pad);
    static void                              PrintUsageAssembler      (const char * sp);
    static void                              PrintUsageRun            (const char * lp, const char * sp, const char * pad);
};
