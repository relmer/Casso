#pragma once

#include "Pch.h"

#include "CommandLineOptions.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommand
//
//  The console entry point for the `disk` subcommand, and deliberately almost
//  nothing: construct the platform implementation, hand the parsed options to
//  the runner, deliver what comes back, return the status.
//
//  In its own file rather than appended to CommandLine.cpp for two reasons.
//  That file is over a thousand lines and is the specific one GH #85 names, so
//  adding to it would worsen the condition this separation exists to improve.
//  And another feature is editing it concurrently, so a new file reduces the
//  overlap to a single registration line in the usage text.
//
////////////////////////////////////////////////////////////////////////////////

class DiskCommand
{
public:
    //  Returns the process exit status: 0 clean, 1 succeeded with complaints,
    //  2 produced no output.
    static int  Run (const CommandLineOptions & options);
};
