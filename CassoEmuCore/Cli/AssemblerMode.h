#pragma once

#include "CommandLineOptions.h"
#include "Cpu.h"
#include "InstructionSetProvider.h"
#include "SourceAssembler.h"




//  Where the object and the listing go. Declared rather than included:
//  a mode names the sink, and only the definitions need its shape.
class ArtifactSink;





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblerMode
//
//  What an assembler subcommand does, once: read the source, assemble it,
//  report what the assembly had to say, and write the files the flags asked
//  for.
//
//  `as65` and `merlin` differ in four places and agree everywhere else -- how
//  the CPU is chosen, what the object is called, what progress is printed
//  along the way, and which extra artifacts exist at all. Those are the hooks
//  below; the order of the steps, the exit codes and the bail-on-first-failure
//  are the same for both and live here.
//
//  THE EXIT CODES ARE NOT A PER-DIALECT DECISION. As65ExitStatus maps a
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

    //  The mode for a dialect, for a caller that has the dialect rather than
    //  the subcommand -- `run`, which assembles under whichever one `--as65` or
    //  `--merlin` named and needs that dialect's CPU answer.
    static std::unique_ptr<AssemblerMode>  CreateFor (DialectId dialect);

    //  Assemble the named source and write its artifacts.
    //
    //  THE TWO RESULTS ANSWER DIFFERENT QUESTIONS and neither derives from the
    //  other. The HRESULT says whether this went wrong and what went wrong,
    //  for a caller that has to decide something; `exitCode` says what the
    //  process should hand back, which is a three-value vocabulary a script
    //  branches on. An assembly that merely warned succeeded -- S_OK -- and
    //  still exits 1.
    //
    //  `sourceReader` IS THE DOOR THE OPTIONS ALREADY DESCRIBE. Null, which is
    //  every production caller, reads the source off the disk as before.
    //  AssemblerOptions has carried a `fileReader` for exactly this purpose all
    //  along, and this function was reaching past it for a local one, which put
    //  every documented exit status behind a real file: nothing could show that
    //  an assembly error exits 3 without writing a broken source to disk first.
    //  That is how 3 came to be documented, mapped, tested at the mapper, and
    //  never once returned by the tool.
    //  `artifacts` is the same door on the way out. Null writes files.
    HRESULT Run (const CommandLineOptions & options, int & exitCode,
                 FileReader * sourceReader = nullptr,
                 ArtifactSink * artifacts = nullptr) const;

    //  The instruction sets this assembly chooses between. Both dialects offer
    //  the same two CPUs, the 6502 and the 65C02; what differs is WHEN the
    //  choice is made. AS65 makes it once, on the command line with `-x`, for
    //  the whole file. Merlin makes it in the source with `XC`, from that line
    //  on. The provider is core's way of saying which: the set the assembly
    //  starts on, and the set a directive may switch to.
    //
    //  Public because `run` asks the same question and must get the same
    //  answer -- a second copy of it is how `run --merlin` came to refuse `XC`.
    virtual InstructionSetProvider  CreateInstructionSetProvider (const CommandLineOptions & options, const Cpu & cpu) const = 0;

protected:
    //  What the sink had to say about a write that failed. Nothing for the sink
    //  that writes host files, which says its own piece as it goes.
    static void          ReportSinkDiagnostics (const ArtifactSink & sink);

    //  What the invocation asked for against what the source turned out to
    //  produce. Neither question can be answered before the assembly: how many
    //  outputs there are, and whether one states a file type, are facts about
    //  the source.
    HRESULT              RefuseUnusableOutputRequest (const CommandLineOptions & options,
                                                      const AssemblyResult & result) const;

    //  What the object file is called. The default is the name the flags
    //  resolved; a dialect whose source can name its own object overrides it.
    virtual std::string  ResolveOutputName       (const CommandLineOptions & options, const AssemblyResult & result) const;

    //  Progress either side of the assembly, and once it has been judged
    //  successful. All three do nothing by default: a subcommand written today
    //  can simply not print, and AS65's lines are a historical courtesy.
    virtual void         ReportAssemblyStarting  (const CommandLineOptions & options) const;
    virtual void         ReportAssemblyFinished  (const CommandLineOptions & options, long long elapsedMicroseconds) const;
    virtual void         ReportAssemblySucceeded (const CommandLineOptions & options, const AssemblyResult & result) const;

    //  Anything beyond the listing and the object. Nothing, by default.
    virtual HRESULT      WriteExtraArtifacts     (const CommandLineOptions & options, const AssemblyResult & result) const;
};
