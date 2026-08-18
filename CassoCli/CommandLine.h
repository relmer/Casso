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

//  The help page the request asked for, spelled with the prefix it was typed
//  with. Both live on the options, so the caller hands over the whole parse
//  rather than picking two fields out of it and deciding between them again.
void PrintUsage   (const CommandLineOptions & options);

//  What the tool is called, which build this is, and who holds the copyright.
//  It heads every help page. The disk page is assembled in the library, which
//  does not know the build's version, so that page is handed this at print time.
std::string BuildBanner ();
void PrintVersion ();
