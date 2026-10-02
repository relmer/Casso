#pragma once

//
//  WINDOWS COMES FIRST, because Ehm chooses its platform from `_WINDOWS_` at
//  the point Ehm.h is parsed. Without it Ehm.cpp compiles the portable bodies
//  of DEBUGMSG, RELEASEMSG and EhmNotifyUser, which write to stderr; a GUI
//  process has none, so every diagnostic message would be discarded.
//
//  <strsafe.h> follows because that is where the StringCch* functions Ehm.cpp
//  uses live -- <windows.h> does not provide them.
//

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <strsafe.h>

#include <cstdarg>
#include <cstdio>
#include <cwchar>
