#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

// <winnt.h> aliases these to the _bittest intrinsics, and CpuOperations::BitTest
// is the 6502 BIT instruction. CassoCore/Pch.h is where that actually bites --
// it compiles the definition, and its Byte typedef means every consumer of a
// CPU header goes through it anyway -- so this is belt and braces: a Pch that
// pulls Windows in should not leave the macro live behind it.
#undef BitTest
#undef BitTestAndSet
#undef BitTestAndReset
#undef BitTestAndComplement

#include <mmdeviceapi.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <propvarutil.h>
#include <shlobj.h>
#include <knownfolders.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <deque>
#include <fcntl.h>
#include <filesystem>
#include <io.h>
#include <format>
#include <fstream>
#include <functional>
#include <iterator>
#include <map>
#include <memory>
#include <mutex>
#include <numbers>
#include <optional>
#include <random>
#include <set>
#include <sstream>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../CassoCore/Ehm.h"

//
//  Dxui's public umbrella header, and the only way its headers are meant to be
//  reached: they assume its system-header surface is already present, so a
//  widget header included on its own fails on types it never declares. Casso's
//  own Pch does the same thing for the same reason.
//
#include "../Dxui/Dxui.h"

using namespace std;
namespace fs = std::filesystem;

typedef unsigned char   Byte;
typedef signed   char   SByte;
typedef unsigned short  Word;

template <typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;
