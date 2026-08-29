#include "Pch.h"

#include "RunMode.h"
#include "CountedNoun.h"
#include "AssemblerMode.h"
#include "HostFile.h"
#include "SourceAssembler.h"
#include "CommandLineParser.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RunMode::LoadAssembledIntoMemory
//
////////////////////////////////////////////////////////////////////////////////

void RunMode::LoadAssembledIntoMemory (Cpu & cpu, const AssemblyResult & result)
{
    Word loadAddr = result.startAddress;



    for (size_t i = 0; i < result.bytes.size(); i++)
    {
        cpu.PokeByte (loadAddr + (Word) i, result.bytes[i]);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunMode::LoadBinaryFileIntoMemory
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RunMode::LoadBinaryFileIntoMemory (Cpu & cpu,
                                               const std::string & inputFile,
                                               Word loadAddr,
                                               Word & entryPoint)
{
    HRESULT      hr = S_OK;
    std::string  contents;
    size_t       i  = 0;



    hr = HostFile::ReadAll (inputFile, contents);

    if (FAILED (hr))
    {
        std::cerr << "Error: cannot read input file: " << inputFile << "\n";
    }

    CHR (hr);

    for (i = 0; i < contents.size(); i++)
    {
        cpu.PokeByte (loadAddr + (Word) i, (Byte) contents[i]);
    }

    entryPoint = loadAddr;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunMode::RunCpu
//
//  Executes the loaded image from the entry point until something stops it,
//  then reports final register state.
//
//  Every exit is BOUNDED, which is what makes this safe to run unattended in a
//  build or a test script: a cycle limit, an explicit stop address, or an
//  illegal opcode. A 6502 program with no halt instruction would otherwise
//  loop forever, and the CLI has no user at the keyboard to interrupt it.
//
//  An illegal opcode exits with a distinct code rather than merely stopping,
//  so a script can tell "ran off into data" from "reached the stop address".
//
//  The cycle counter counts INSTRUCTIONS, not machine cycles -- it is a
//  runaway guard, not a timing model, and the CLI has no clock to be faithful
//  to.
//
//  Status lines are accumulated into the caller's vector instead of printed,
//  so the caller decides whether they belong on stdout, in quiet mode, or
//  interleaved with other output.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RunMode::RunCpu (Cpu & cpu,
                         const CommandLineOptions & options,
                         Word entryPoint,
                         std::vector<std::string> & status,
                         int & exitCode)
{
    HRESULT   hr     = S_OK;
    uint32_t  cycles = 0;



    exitCode = 0;
    cpu.SetPC (entryPoint);
    status.push_back (std::format ("Executing from ${:04X}", entryPoint));




    for (;;)
    {
        Byte  opcode = 0;

        if (options.maxCycles > 0 && cycles >= options.maxCycles)
        {
            status.push_back (std::format ("Stopped: cycle limit reached ({})", options.maxCycles));
            break;
        }

        opcode = cpu.PeekByte (cpu.GetPC());

        if (!cpu.GetMicrocode (opcode).isLegal)
        {
            std::println (stderr, "Illegal opcode ${:02X} at ${:04X}", opcode, cpu.GetPC());
            // Bad input, the same as a source that will not assemble: the
            // bytes describe something this CPU cannot execute.
            hr       = HRESULT_FROM_WIN32 (ERROR_INVALID_DATA);
            exitCode = 3;
            break;
        }

        if (options.hasStopAddress && cpu.GetPC() == options.stopAddress)
        {
            status.push_back (std::format ("Stopped at address ${:04X}", options.stopAddress));
            break;
        }

        cpu.StepOne();
        cycles++;
    }

    status.push_back (std::format ("Execution complete: {}",
                                   CountedNoun::Of ((long long) cycles, "cycle")));
    status.push_back (std::format ("  A=${:02X} X=${:02X} Y=${:02X} SP=${:02X} PC=${:04X}",
        cpu.GetA(), cpu.GetX(), cpu.GetY(), cpu.GetSP(), cpu.GetPC()));

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunMode::Run
//
//  The `run` subcommand: get an image into memory -- assembling it first if
//  the input is source -- pick an entry point, and execute.
//
//  Accepting either source or a binary is what makes this usable as a one-step
//  test harness: `casso run foo.a65` assembles and runs without an
//  intermediate file, while the same command on a .bin runs a prebuilt image.
//  The choice is made from the input's extension, not from a flag.
//
//  Exit codes are meaningful and distinct, because scripts branch on them:
//
//    0  ran to a normal stop
//    1  the tools ran and said no (assembly errors)
//    2  could not even start (no input, unreadable file)
//    3  from RunCpu -- an illegal opcode
//
//  Entry point resolution has three tiers, most-explicit first: an explicit
//  --entry, then the RESET vector at $FFFC when asked for, then the assembled
//  start address (or the load address for a binary). Reading the reset vector
//  is what lets a ROM image boot the way the hardware would rather than from
//  wherever its bytes happen to begin.
//
//  Status lines are collected throughout and printed only under --verbose, so
//  the default run stays quiet enough to pipe.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RunMode::Run (const CommandLineOptions & options, int & exitCode)
{
    HRESULT                   hr         = S_OK;
    Cpu                       cpu;
    Word                      entryPoint = 0x8000;
    Word                      loadAddr   = 0;
    std::vector<std::string>  status;
    bool                      hasInput   = !options.inputFile.empty();
    bool                      wasLoaded  = false;



    // 2 = "cannot even start" (no input, unreadable file); 1 = "ran the tools
    // and they said no" (assembly errors).
    exitCode = 2;

    if (!hasInput)
    {
        std::cerr << "Error: No input file specified\n";
    }

    CBREx (hasInput, E_INVALIDARG);

    cpu.Reset();

    if (CommandLineParser::IsAssemblySource (options.inputFile))
    {
        AssemblerOptions         asmOptions = {};
        SourceAssembler::Result  ar;

        asmOptions.warningMode      = options.warningMode;

        // Which assembler reads the source, from --as65 / --merlin. Carried with
        // its provenance for the same reason the subcommands carry it: a dialect
        // the caller named needs no report, one it inherited does.
        asmOptions.dialect           = options.dialect;
        asmOptions.dialectSelection  = options.dialectSelection;
        asmOptions.flagPrefix        = options.flagPrefix;
        asmOptions.predefinedSymbols = options.predefinedSymbols;

        //  The CPU answer is the dialect's own, asked of the same mode that
        //  answers it for the assembler subcommand -- a second copy here is how
        //  `run --merlin` came to refuse `XC`.
        ar = SourceAssembler::Assemble (options.inputFile,
                                        AssemblerMode::CreateFor (options.dialect)->CreateInstructionSetProvider (options, cpu),
                                        asmOptions);
        SourceAssembler::ReportDiagnostics (ar);

        wasLoaded = ar.ok;
        exitCode  = wasLoaded ? 0 : 1;

        CBREx (wasLoaded, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        LoadAssembledIntoMemory (cpu, ar.result);
        entryPoint = ar.result.startAddress;

        status.push_back (std::format ("Assembling: {}", options.inputFile));
        status.push_back (std::format ("Assembled {} bytes", ar.result.bytes.size()));
        status.push_back (std::format ("  Start: ${:04X}", ar.result.startAddress));
    }
    else
    {
        loadAddr  = options.hasLoadAddress ? options.loadAddress : (Word) 0x8000;
        hr        = LoadBinaryFileIntoMemory (cpu, options.inputFile, loadAddr, entryPoint);
        wasLoaded = SUCCEEDED (hr);
        exitCode  = wasLoaded ? 0 : 2;

        CHR (hr);

        status.push_back (std::format ("Loaded binary at ${:04X}", loadAddr));
    }

    if (options.hasEntryAddress)
    {
        entryPoint = options.entryAddress;
    }
    else if (options.useResetVector)
    {
        entryPoint = cpu.PeekWord (0xFFFC);
    }

    hr = RunCpu (cpu, options, entryPoint, status, exitCode);

    if (options.verbose)
    {
        for (const auto & msg : status)
        {
            std::cerr << msg << "\n";
        }
    }

Error:
    return hr;
}
