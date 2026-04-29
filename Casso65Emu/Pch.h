#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <d3d11.h>
#include <dxgi.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <commdlg.h>

#include <shellapi.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <format>
#include <fstream>
#include <functional>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "../Casso65Core/Ehm.h"

typedef unsigned char   Byte;
typedef signed   char   SByte;
typedef unsigned short  Word;
