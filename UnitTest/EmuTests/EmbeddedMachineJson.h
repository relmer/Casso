#pragma once

#include "../../CassoEmuCore/Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  EmbeddedMachineJson
//
//  The machine configuration JSON a shipped Casso.exe carries, read back out
//  of the executable next to the test assembly.
//
//  A test that wants to build a real machine wants the real configuration,
//  and the real configuration is the RCDATA the exe was built with -- not a
//  copy in the test tree, which is a second thing to keep in step and the
//  reason for the copy would be that reading the first was inconvenient.
//
//  Nothing under UnitTest/ is read, and neither is Resources/. The exe is
//  opened as a resource-only module, which maps its resource section without
//  running a byte of it.
//
////////////////////////////////////////////////////////////////////////////////

class EmbeddedMachineJson
{
public:

    //  RCDATA `resourceId` from Casso.exe, as text. Every failure asserts
    //  rather than returning empty, so callers can keep their bodies tight.
    static std::string  Load (int resourceId);

    //  Casso.exe beside the test assembly, or an empty path when it is not
    //  there. vstest drops both binaries in one output folder.
    static std::filesystem::path  LocateCassoExe ();
};
