#pragma once

//
//  WINDOWS COMES FIRST, because Ehm chooses its platform from `_WINDOWS_` at
//  the point Ehm.h is parsed. Every Windows project pulls <windows.h> in ahead
//  of that header for the same reason (see CassoCli/Pch.h and Ehm/Pch.h).
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
#include <charconv>
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

#include "../Ehm/Ehm.h"





typedef unsigned char   Byte;
typedef signed   char   SByte;
typedef unsigned short  Word;
