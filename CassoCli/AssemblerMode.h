#pragma once

#include "CommandLineOptions.h"
#include "Cpu.h"
#include "InstructionSetProvider.h"
#include "SourceAssembler.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblerMode
//
//  What an assembler subcommand does, once: read the source, assemble it,
//  report what the assembly had to say, and write the files the flags asked
//  for.
//
//  `as65` and `merlin` differ in four places and agree everywhere else -- which
//  instruction tables the source is assembled against, what the object is
//  called, what progress is printed along the way, and which extra artifacts
//  exist at all. Those are the hooks below; the order of the steps, the exit
//  codes and the bail-on-first-failure are the same for both and live here.
//
//  THE EXIT CODES ARE NOT A PER-DIALECT DECISION. AssemblerExitCode maps a
//  finished assembly onto the three the tool speaks -- 0 clean, 1 assembled
//  with complaints, 2 no output -- and a subcommand that computed its own would
//  be teaching a script a second vocabulary for the same three outcomes. A
//  failed WRITE is the one thing this adds: it earns the same 2, because the
//  question a script asks is whether it got a file.
//
//  A dialect added later gets all of this by deriving; what it must supply is
//  exactly the four answers above.
//
////////////////////////////////////////////////////////////////////////////////

class AssemblerMode
{
public:
    virtual ~AssemblerMode () = default;

    //  Assemble the named source and write its artifacts.
    //
    //  THE TWO RESULTS ANSWER DIFFERENT QUESTIONS and neither derives from the
    //  other. The HRESULT says whether this went wrong and what went wrong,
    //  for a caller that has to decide something; `exitCode` says what the
    //  process should hand back, which is a three-value vocabulary a script
    //  branches on. An assembly that merely warned succeeded -- S_OK -- and
    //  still exits 1.
    HRESULT Run (const CommandLineOptions & options, int & exitCode) const;

protected:
    //  The instruction sets this assembly may choose between, in core's own
    //  terms: the base set it starts on, and the extended set an in-source
    //  directive may switch to, if the dialect has such a directive. Which
    //  CPUs those are is the dialect's answer -- AS65 takes the base from `-x`
    //  and has no directive; Merlin starts on the 6502 and lets `XC` reach the
    //  65C02.
    virtual InstructionSetProvider  CreateInstructionSetProvider (const CommandLineOptions & options, const Cpu & cpu) const = 0;

    //  What the object file is called. The default is the name the flags
    //  resolved; a dialect whose source can name its own object overrides it.
    virtual std::string             ResolveOutputName            (const CommandLineOptions & options, const AssemblyResult & result) const;

    //  Progress either side of the assembly, and once it has been judged
    //  successful. All three do nothing by default: a subcommand written today
    //  can simply not print, and AS65's lines are a historical courtesy.
    virtual void                    ReportAssemblyStarting       (const CommandLineOptions & options) const;
    virtual void                    ReportAssemblyFinished       (const CommandLineOptions & options, long long elapsedMicroseconds) const;
    virtual void                    ReportAssemblySucceeded      (const CommandLineOptions & options, const AssemblyResult & result) const;

    //  Anything beyond the listing and the object. Nothing, by default.
    virtual HRESULT                 WriteExtraArtifacts          (const CommandLineOptions & options, const AssemblyResult & result) const;
};
