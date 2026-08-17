#include "Pch.h"

#include "DiskCommand.h"
#include "Win32DiskFileIo.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/DiskCommandRunner.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommand::Run
//
//  Deliberately decision-free. Every branch that could be taken wrongly -- which
//  verb, which filesystem, whether a result is safe, what the status should be,
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
    DiskCommandResult  result   = runner.Run (options);
    HRESULT            hr       = S_OK;
    int                exitCode = result.exitStatus;



    if (!result.output.empty())
    {
        std::cout << TextEncoding::NarrowToConsole (result.output);
        std::cout.flush();
    }

    if (result.hasPayload)
    {
        hr = fileIo.WritePayloadToStandardOutput (result.payload);

        if (FAILED (hr))
        {
            std::cerr << "Error: could not write the extracted bytes to standard output\n";
            exitCode = DiskCommandRunner::kNoOutput;
        }
    }

    if (!result.diagnostics.empty())
    {
        std::cerr << TextEncoding::NarrowToConsole (result.diagnostics);
        std::cerr.flush();
    }

    return exitCode;
}
