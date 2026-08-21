#pragma once

//
//  CassoCli's precompiled header: Windows first, then the core one.
//
//  It exists to add what the platform edge needs -- atomic replacement, file
//  stamps, the standard-output mode, and the console's width -- on top of the
//  core library's header. Those belong to this project alone: putting
//  <windows.h> into CassoCore's Pch would drag the platform into the assembler,
//  which is deliberately free of it.
//
//  WHERE <windows.h> IS INCLUDED IS NOT A DETAIL. Ehm chooses its platform from
//  `_WINDOWS_` at its own include point, so pulling Windows in AFTER
//  CassoCore/Pch.h leaves Ehm having already defined `S_OK` and friends
//  portably, and every one of them is then redefined by <winerror.h>. Including
//  it here, ahead of the core header, is what the other Windows projects in the
//  solution do, and it settles the question for EVERY translation unit in this
//  one rather than for whichever file happened to need a console API.
//
//  Everything else comes from the core header below, so a translation unit here
//  sees exactly what one in CassoCore does, plus the platform.
//

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <fcntl.h>
#include <io.h>

#include <filesystem>

#include "../CassoCore/Pch.h"
