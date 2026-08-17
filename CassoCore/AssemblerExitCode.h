#pragma once

#include "AssemblerTypes.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblyExitCode
//
//  What an assembly run tells the process that started it. Three outcomes, and
//  the numbers are the vocabulary the tool already speaks rather than a new one:
//  a script driving any assembler subcommand needs no per-dialect knowledge of
//  what a number means.
//
//  NoOutput covers every way an assembly can fail to produce a file, including
//  a construct the dialect deliberately refuses. Such a refusal does NOT get a
//  code of its own: the exit code answers "did I get an output file", which is
//  the only question a script can act on, and the refusal is distinguished by
//  its message, which is where a developer reads the difference. A distinct code
//  would oblige every caller to learn a number to discover something the message
//  already says.
//
//  The enumerators carry their numeric values because those values ARE the
//  contract. They are not an implementation detail free to be reordered.
//
////////////////////////////////////////////////////////////////////////////////

enum class AssemblyExitCode
{
    Clean                 = 0,   // assembled, nothing to say
    AssembledWithWarnings = 1,   // assembled, but warnings were emitted
    NoOutput              = 2,   // nothing was produced
};





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblerExitCode
//
//  The mapping from an assembly's result to that vocabulary.
//
//  In core rather than in the executable so it can be exercised by a test
//  instead of only by running the tool. An exit code reachable solely by
//  launching a process is checked by nobody, and it is precisely the kind of
//  thing that drifts: it has no compiler to answer to and no caller inside the
//  build to notice.
//
////////////////////////////////////////////////////////////////////////////////

class AssemblerExitCode
{
public:
    // What the assembly itself earned. Writing the output files afterward can
    // still fail, and that is the caller's own outcome to report -- this looks
    // only at what the assembler produced.
    static AssemblyExitCode  FromResult    (const AssemblyResult & result);

    // The same value as the number a process exits with, so the executable
    // hands back a named result rather than casting an enum at its edge.
    static int               ToProcessCode (AssemblyExitCode code);
};
