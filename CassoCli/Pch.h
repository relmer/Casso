#pragma once

//
//  CassoCli's precompiled header: Windows first, then the core one.
//
//  It exists to add what the platform edge needs -- atomic replacement, file
//  stamps, the standard-output mode, and the console's width -- on top of the
//  core library's header. Those belong to this project alone.
//
//  WHERE <windows.h> IS INCLUDED IS NOT A DETAIL. Ehm chooses its platform from
//  `_WINDOWS_` at its own include point, so pulling Windows in AFTER
//  CassoCore/Pch.h would leave Ehm having already defined `S_OK` and friends
//  portably, and every one of them then redefined by <winerror.h>. CassoCore's
//  own Pch now includes <windows.h> ahead of Ehm.h and settles that for every
//  project, this one included -- it had to, because Ehm.cpp is compiled THERE
//  and was picking the portable path for its definitions. Keeping the include
//  here as well costs nothing (the header is guarded) and keeps the order
//  obvious to a reader of this file.
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
