#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  MeshBake's precompiled header.
//
//  The baker compiles ObjMeshParser.cpp and MeshBlob.cpp straight out of the
//  emulator core's tree rather than linking the library that holds them.
//  Not linking it is what lets this tool stay x64 in an ARM64 build: it runs
//  on the BUILD machine, so it has to match the host rather than the target,
//  and a project reference would drag the target's architecture in with it.
//
//  Those two files open with `#include "Pch.h"`. Their own directory holds
//  no such header, so the search falls through to MeshBake's directory and
//  reaches this one, which forwards to the header they were written against.
//  Ehm.cpp is compiled in too and resolves the same line to CassoCore's own
//  Pch.h, which sits beside it and therefore wins before the include path is
//  consulted at all. That is the right answer for it, and this file exists to
//  make sure the other two get theirs.
//
////////////////////////////////////////////////////////////////////////////////

#include "../CassoEmuCore/Pch.h"

// Not in the core's header, and the baker talks to a console.
#include <iostream>
