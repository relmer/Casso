#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  CliMain
//
//  Everything CassoCli.exe does.
//
//  The executable is a shim over this and holds nothing else: one function that
//  hands argv straight through and returns what comes back. What is here is
//  parsing, dispatch, and the exit code each arm earns, none of which touches a
//  window, a device or anything else a test cannot drive.
//
//  THE SPLIT IS TESTABILITY, NOT PLATFORM. Win32 file calls live in this
//  library too, because a test can drive them; what stays in an executable is
//  only what cannot exist without one, which is the entry point itself.
//
////////////////////////////////////////////////////////////////////////////////

int CliMain (int argc, char * argv[]);
