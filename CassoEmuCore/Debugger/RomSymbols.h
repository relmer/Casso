#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  RomSymbols
//
//  The shipped symbol tables, as Casso debug file text: the Monitor, zero
//  page and I/O names of each machine, Applesoft's entry points, and the
//  DOS 3.3 and ProDOS entry points. Names and addresses come from Apple's
//  published reference manuals, listed in RomSymbols.cpp.
//
////////////////////////////////////////////////////////////////////////////////

class RomSymbols
{
public:
    // The Main table for a machine, chosen from its name: the //e and //c
    // tables add the memory-management and video switches of those machines.
    static const char *  GetMain    (const std::string & machineName);
    static const char *  GetBasic   ();
    static const char *  GetDos33   ();
    static const char *  GetProDos  ();

private:
    static bool  IsIie (const std::string & machineName);
};
