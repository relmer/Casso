#pragma once

#include "CommandLineOptions.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebugMode
//
//  The console entry point for the `debug` subcommand, and deliberately
//  almost nothing: read the script, construct the platform pieces, hand the
//  options to DebugBatchRunner, print what comes back, return the status.
//  Every decision is the runner's, which the test assembly can reach.
//
////////////////////////////////////////////////////////////////////////////////

class DebugMode
{
public:
    static HRESULT  Run (const CommandLineOptions & options, int & exitCode);

private:
    static HRESULT  ReadScript (const std::string & path, std::string & text);
    static void     ListInstances  (int & exitCode);
    static HRESULT  AttachAndRun   (const CommandLineOptions & options, const std::string & script, int & exitCode);
};
