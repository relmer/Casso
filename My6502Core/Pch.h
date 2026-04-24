#pragma once



// C6262: "Function uses N bytes of stack. Consider moving some data to heap."
// The Cpu class embeds a 64 KB `memory[]` array by design (the 6502's full address space),
// and Cpu / TestCpu instances are intentionally stack-allocated throughout the codebase
// (main, CommandLine, and every unit test). Disable this SAL warning project-wide so the
// code-analysis CI gate does not flag the emulator's deliberate memory model.
#pragma warning(disable: 6262)

#include <algorithm>
#include <cassert>
#include <cctype>
#include <chrono>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>



typedef unsigned char   Byte;
typedef signed   char   SByte;
typedef unsigned short  Word;
