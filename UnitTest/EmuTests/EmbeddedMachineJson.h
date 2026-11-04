#pragma once

#include "../../CassoEmuCore/Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  EmbeddedMachineJson
//
//  The machine configuration JSON that Casso.exe embeds, read from the test
//  assembly, which MachineConfigs.rc builds from the same files.
//
//  A test that builds a real machine uses the real configuration. The
//  resource compiler reads the files Casso.rc embeds when the test assembly
//  is built, so the bytes under test are the bytes the app ships. There is
//  no copy in the test tree to update, and no file is read at run time.
//
////////////////////////////////////////////////////////////////////////////////

class EmbeddedMachineJson
{
public:

    //  RCDATA `resourceId` from the test assembly, as text. Every failure
    //  asserts rather than returning empty, so a caller doesn't check the
    //  result.
    static std::string  Load (int resourceId);

    //  Casso.exe beside the test assembly, or an empty path when it is not
    //  there, for a scenario test of the built executables. vstest drops
    //  both binaries in one output folder.
    static std::filesystem::path  LocateCassoExe ();
};
