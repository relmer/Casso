#pragma once

//
//  The console executable's precompiled header.
//
//  It exists only to add what the platform edge needs -- atomic replacement,
//  file stamps, and the standard-output mode -- on top of the core library's
//  header. Those belong to this project alone: putting <windows.h> into
//  CassoCore's Pch would drag the platform into the assembler, which is
//  deliberately free of it.
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
