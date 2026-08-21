#pragma once





//
//  CassoCli's precompiled header: Windows first, then the core one.
//
//  The CLI asks the console how wide it is, which is the only Win32 it wants --
//  but WHERE <windows.h> is included is not a detail. Ehm chooses its platform
//  from `_WINDOWS_` at its own include point, so pulling Windows in after
//  CassoCore/Pch.h leaves Ehm having already defined `S_OK` and friends
//  portably, and every one of them is then redefined by <winerror.h>. Including
//  it here, ahead of the core header, is what the other Windows projects in the
//  solution do, and it settles the question for EVERY translation unit in this
//  one rather than for whichever file happened to need a console API.
//

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "../CassoCore/Pch.h"
