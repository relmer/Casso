#pragma once

#include "Pch.h"

#include "CommandLineOptions.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BasicCommand
//
//  The console entry point of the `basic` subcommand: read the input file,
//  call BasicCommandRunner, write the output file and print the text.
//
////////////////////////////////////////////////////////////////////////////////

class BasicCommand
{
public:
    //  Returns the process exit status: 0 done, 2 nothing was written.
    static int  Run (const CommandLineOptions & options);
};
