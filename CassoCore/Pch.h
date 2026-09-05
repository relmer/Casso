#pragma once

//
//  WINDOWS COMES FIRST, because Ehm chooses its platform from `_WINDOWS_` at
//  the point Ehm.h is parsed, and Ehm.cpp -- the single translation unit that
//  DEFINES DEBUGMSG, RELEASEMSG and EhmNotifyUser -- lives in this project.
//  Without it those three compiled their portable bodies, which write to
//  stderr; a GUI process has none, so every diagnostic message in Casso and
//  Dxui was discarded while the header promised OutputDebugString. Every other
//  Windows project already pulls <windows.h> in ahead of this header for the
//  same reason (see CassoCli/Pch.h); this project was the one that did not.
//
//  <strsafe.h> follows because that is where the StringCch* functions Ehm.cpp
//  uses live -- <windows.h> does not provide them.
//
//  THE UNDEFS ARE NOT OPTIONAL. <winnt.h> defines BitTest and its siblings as
//  aliases for the _bittest intrinsics, and CpuOperations::BitTest is the 6502
//  BIT instruction, so leaving the macro live renames that function in this
//  project but not in callers compiled without Windows -- an unresolved
//  external at LINK time rather than an error here. UnitTest/Pch.h has carried
//  the same four undefs for exactly this reason; they belong here too, at the
//  point the collision is introduced.
//

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <strsafe.h>

#undef BitTest
#undef BitTestAndSet
#undef BitTestAndReset
#undef BitTestAndComplement

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <format>
#include <functional>
#include <fstream>
#include <iostream>
#include <print>
#include <memory>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "Ehm.h"





typedef unsigned char   Byte;
typedef signed   char   SByte;
typedef unsigned short  Word;
