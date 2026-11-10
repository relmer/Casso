#include "Pch.h"

#include "BasicCommand.h"
#include "BasicCommandRunner.h"
#include "CommandLine.h"
#include "Core/TextEncoding.h"
#include "Seams/Win32DiskFileIo.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BasicCommand::Run
//
//  The output file is written only when the command succeeded, so a failed
//  command does not replace an existing file.
//
////////////////////////////////////////////////////////////////////////////////

int BasicCommand::Run (const CommandLineOptions & options)
{
    Win32DiskFileIo     hostIo;
    BasicCommandResult  result;
    std::vector<Byte>   input;
    HRESULT             hr = S_OK;



    hr = hostIo.ReadAllBytes (options.basic.inputFile, input);

    if (FAILED (hr))
    {
        result.diagnostics = "Error: cannot read " + options.basic.inputFile + "\n";
        result.exitStatus  = BasicCommandResult::kNoOutput;
    }
    else
    {
        result = BasicCommandRunner::Run (options.basic, input);
    }

    if (result.hasFile)
    {
        hr = hostIo.WriteAllBytes (options.basic.outputFile, result.fileBytes);

        if (FAILED (hr))
        {
            result.diagnostics += "Error: cannot write " + options.basic.outputFile + "\n";
            result.exitStatus   = BasicCommandResult::kNoOutput;
        }
    }

    if (!result.output.empty())
    {
        std::cout << TextEncoding::NarrowToConsole (result.output);
        CommandLine::FlushOutput();
    }

    if (!result.diagnostics.empty())
    {
        std::cerr << TextEncoding::NarrowToConsole (result.diagnostics);
        std::cerr.flush();
    }

    return result.exitStatus;
}
