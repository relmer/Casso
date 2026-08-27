#include "Pch.h"

#include "CommandLine.h"
#include "DiskCommand.h"
#include "Win32DiskFileIo.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/DiskCommandRunner.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommand::Run
//
//  Deliberately decision-free. Every branch that could be taken wrongly -- which
//  command, which filesystem, whether a result is safe, what the status should be,
//  how to word a failure -- was made by the runner, which the test assembly can
//  reach. If a condition ever appears in this function, it belongs there
//  instead.
//
//  The two streams are kept apart on purpose. The payload goes through the
//  seam, which is where the platform's opinion about binary output is settled;
//  diagnostics go to the error stream so they never contaminate a pipe, and
//  listings to the output stream as text.
//
//  TEXT IS RE-ENCODED FOR THE CONSOLE ON ITS WAY OUT, and that is a correctness
//  fix rather than a cosmetic one. The runner's strings are in the process's own
//  narrow code page -- image paths arrive that way from argv -- and a console
//  set to UTF-8 reads a CP-1252 byte as a broken sequence and draws a question
//  mark. A disk named `Space Quarks (1981)(Br<o-slash>derbund).woz` came back
//  from its own error message as `Br?derbund`, which is a filename nobody can
//  paste back into a command line.
//
//  It belongs HERE rather than in the runner because a code page is a property
//  of the destination and nothing else: the same string written to a file wants
//  different bytes from the same string written to a window, and only the layer
//  holding the stream knows which one it has. The payload deliberately does not
//  pass through it -- those are a file's bytes, not text, and re-encoding them
//  would corrupt every extraction.
//
////////////////////////////////////////////////////////////////////////////////

int DiskCommand::Run (const CommandLineOptions & options)
{
    Win32DiskFileIo    fileIo;
    DiskCommandRunner  runner (fileIo);
    DiskCommandResult  result;
    HRESULT            hr       = S_OK;
    int                exitCode = 0;



    //  The banner is the executable's to know: the disk help is assembled in
    //  the core library, which has no VERSION_STRING of its own.
    runner.SetBanner (CommandLine::BuildBanner());

    result   = runner.Run (options);
    exitCode = result.exitStatus;



    //  A PAGE FOLDS TO THE TERMINAL. A LISTING MUST NOT.
    //
    //  The runner hands back both through `output`, and printing all of it
    //  verbatim was right for one and wrong for the other: a catalog row is a
    //  table and reflowing one destroys it, while the help is prose composed as
    //  one logical line per paragraph. Measured, `disk` alone emitted a single
    //  623-character line and left the terminal to break it at column zero,
    //  losing the indent on every continuation. `usageShown` is what tells the
    //  two apart.
    //
    //  A REFUSAL'S USAGE IS PART OF THE REFUSAL, so it goes to the error
    //  stream with the reason rather than to stdout.
    //
    //  Split across the two, each half is written correctly and a terminal
    //  still shows them interleaved: measured, `disk cat` put the blank line
    //  separating them in the middle of the usage instead of after it, because
    //  a terminal reads the two pipes on two threads and nothing a writer does
    //  can order them. Ordinary output -- a listing, an extracted file -- stays
    //  on stdout, which is what a caller pipes.
    if (!result.output.empty())
    {
        if (result.usageShown)
        {
            CommandLine::UsageOnErrorStream  toTheErrorStream;

            CommandLine::PrintUsageBlock (TextEncoding::NarrowToConsole (result.output));
            std::fflush (stderr);
        }
        else if (result.badCommandLine)
        {
            std::cerr << TextEncoding::NarrowToConsole (result.output);
            std::cerr.flush();
        }
        else
        {
            std::cout << TextEncoding::NarrowToConsole (result.output);
            CommandLine::FlushOutput();
        }
    }

    if (result.hasPayload)
    {
        hr = fileIo.WritePayloadToStandardOutput (result.payload);

        if (FAILED (hr))
        {
            std::cerr << "Error: could not write the extracted bytes to stdout\n";
            exitCode = DiskCommandResult::kNoOutput;
        }
    }

    //  A COMMAND LINE THIS GRAMMAR CANNOT READ IS ANSWERED WITH THE GRAMMAR.
    //  An unknown command used to earn one line naming the twelve that exist and
    //  nothing about what any of them takes, so a reader who had the command wrong
    //  learned its name and then had to ask a second question to learn its
    //  operands. The page goes first and the diagnostic last, the same order
    //  every other refusal uses: the reader sees the bottom of the screen.
    //
    //  Only for a bad command line. "PROG is not on this volume" is answered by
    //  a listing rather than by a page of syntax.
    //  Unless the runner already answered with the one command's usage, which
    //  is the better answer when the reader has said which command they want.
    if (result.badCommandLine && !result.usageShown)
    {
        CommandLine::UsageOnErrorStream  toTheErrorStream;

        CommandLine::PrintPageFor (CommandLineOptions::Subcommand::Disk, options.flagPrefix);
        std::fflush (stderr);
    }

    if (result.badCommandLine)
    {
        std::cerr << CommandLine::kGapBeforeTheReason;
    }

    if (!result.diagnostics.empty())
    {
        std::cerr << TextEncoding::NarrowToConsole (result.diagnostics);
        std::cerr.flush();
    }

    return exitCode;
}
