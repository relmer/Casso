#pragma once

#include "AssemblerTypes.h"
#include "CommandLineOptions.h"
#include "Cpu.h"
#include "DialectReporting.h"
#include "InstructionSetProvider.h"
#include "Microcode.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SourceAssembler
//
//  One source file in, one assembly out, with its diagnostics reported.
//
//  This is the half of the work that every subcommand needs and none of them
//  owns: `as65` and `merlin` assemble in order to write artifacts, `run`
//  assembles in order to execute, and all three want the same request built
//  from the same flags and the same errors printed the same way.
//
//  It writes NOTHING but diagnostics. What becomes of the bytes is
//  ArtifactWriter's question under the assembler subcommands and RunMode's
//  under `run`, and keeping that out of here is what lets `run` assemble
//  without acquiring an opinion about output files.
//
////////////////////////////////////////////////////////////////////////////////

class SourceAssembler
{
public:
    ////////////////////////////////////////////////////////////////////////////
    //
    //  Result
    //
    //  What one assembly produced, and whether it produced it.
    //
    ////////////////////////////////////////////////////////////////////////////

    struct Result
    {
        AssemblyResult result;
        bool           ok = false;      // default: treat an unfilled result as failure

        //  Whether the source was READ, which is not the same as whether it
        //  assembled. A file that could not be opened produces no diagnostics
        //  of its own, and the failure summary used to total those anyway and
        //  report "failed with 0 error(s)" under a line that had already said
        //  the file could not be read.
        bool           sourceRead = false;

        std::string    inputFile;
    };

    //  The assembler request the flags describe. The dialect travels WITH its
    //  provenance, since a dialect the invocation named needs no report and one
    //  the caller merely inherited does.
    static AssemblerOptions   BuildOptions          (const CommandLineOptions & options);

    //  Assemble one file against the instruction sets the caller chose.
    //
    //  `sourceReader` SUPPLIES THE TOP-LEVEL SOURCE, and null (every
    //  production caller) reads it off the disk exactly as before.
    //
    //  It is a parameter of its own rather than `asmOptions.fileReader`
    //  because those two answer different questions. The one in the options
    //  resolves an `include` RELATIVE TO baseDir; this one is handed the input
    //  path as the caller wrote it, and baseDir was derived from that path, so
    //  routing the first read through the other reader would apply the
    //  directory twice.
    static Result             Assemble              (const std::string & inputFile,
                                                     const InstructionSetProvider & instructionSets,
                                                     const AssemblerOptions & asmOptions,
                                                     FileReader * sourceReader = nullptr);

    //  The assembly's own warnings and errors, on stderr.
    static void               ReportDiagnostics     (const Result & ar);

    //  What is worth saying about the CPU that stood, for core to decide
    //  whether and where to say it.
    static CpuReport          BuildCpuReport        (const CommandLineOptions & options, const AssemblyResult & result);

    //  Print what core decided to report. Chooses nothing itself.
    static void               ReportToStderr        (const std::vector<DialectReportLine> & reports);
};
