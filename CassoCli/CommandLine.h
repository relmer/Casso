#pragma once

#include "CommandLineOptions.h"
#include "CommandLineParser.h"





//
//  The executable's half of the command line: the subcommand bodies and the
//  usage text. Everything that DECIDES what an argv means lives in
//  CassoCore/CommandLineParser, where the UnitTest project can reach it; what
//  remains here is the platform edge -- reading source, writing artifacts, and
//  printing.
//

CommandLineOptions ParseCommandLine (int argc, char * argv[]);
int  DoRun        (const CommandLineOptions & options);
int  DoAs65       (const CommandLineOptions & options);
void PrintUsage   (char prefix);
void PrintVersion ();
