#pragma once

#include "AssemblerTypes.h"
#include "CommandLineOptions.h"
#include "Cpu.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RunMode
//
//  The `run` subcommand: load a program into a CPU and execute it.
//
//  It takes either a binary or a source file, and a source is assembled on the
//  way in rather than written out -- which is the whole reason `run` is not an
//  AssemblerMode. The assembler subcommands exist to produce files; this one
//  exists to produce a result, and the assembly is a step it passes through.
//
////////////////////////////////////////////////////////////////////////////////

class RunMode
{
public:
    //  Load, execute, and report. The HRESULT says what went wrong; `exitCode`
    //  is what the process hands back, set alongside rather than derived from
    //  it, since the three-value exit vocabulary cannot carry what went wrong.
    static HRESULT  Run                      (const CommandLineOptions & options, int & exitCode);

private:
    //  The assembled bytes, at the addresses the assembly gave them.
    static void     LoadAssembledIntoMemory  (Cpu & cpu, const AssemblyResult & result);

    //  A pre-assembled image, at the load address the caller named.
    static HRESULT  LoadBinaryFileIntoMemory (Cpu & cpu,
                                              const std::string & inputFile,
                                              Word loadAddr,
                                              Word & entryPoint);

    //  Execute from the entry point until a stop condition. Collects status
    //  lines rather than printing them, so `run` stays quiet enough to pipe.
    //  An illegal opcode is a failed run on bad input data: exit 3.
    static HRESULT  RunCpu                   (Cpu & cpu,
                                              const CommandLineOptions & options,
                                              Word entryPoint,
                                              std::vector<std::string> & status,
                                              int & exitCode);
};
