#include "Pch.h"

#include "Cli/CliMain.h"





////////////////////////////////////////////////////////////////////////////////
//
//  main
//
//  The console tool's entry point, and the whole of what its executable used
//  to hold.
//
//  It lives here rather than in CassoCli.exe for the reason every other line
//  does: an executable is not a precondition for having an entry point, only
//  for having a process. The linker recovers this function because the project
//  names mainCRTStartup as its entry symbol, which gives it an undefined
//  symbol to resolve, and startup references main.
//
////////////////////////////////////////////////////////////////////////////////

int main (int argc, char * argv[])
{
    return (CliMain (argc, argv));
}
