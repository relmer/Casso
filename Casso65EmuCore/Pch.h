#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "../Casso65Core/Ehm.h"

using namespace std;
namespace fs = std::filesystem;

typedef unsigned char   Byte;
typedef signed   char   SByte;
typedef unsigned short  Word;
