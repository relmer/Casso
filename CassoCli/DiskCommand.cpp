#include "Pch.h"

#include "DiskCommand.h"
#include "Win32DiskFileIo.h"
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
        std::cout << result.output;
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
        std::cerr << result.diagnostics;
        std::cerr.flush();
    }

    return exitCode;
}
