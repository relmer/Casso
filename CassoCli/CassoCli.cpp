#include "Pch.h"

#include "Cli/CliMain.h"





////////////////////////////////////////////////////////////////////////////////
//
//  main
//
//  The whole of the executable.
//
//  Everything the tool does is CliMain's, in CassoEmuCore, where the test
//  assembly can reach it. What is left here is the one thing that cannot live
//  in a library: the entry point the linker demands.
//
////////////////////////////////////////////////////////////////////////////////

int main (int argc, char * argv[])
{
    return CliMain (argc, argv);
}
