#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <commdlg.h>

#include <shellapi.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "../Casso65Core/Ehm.h"

using namespace std;
namespace fs = std::filesystem;

typedef unsigned char   Byte;
typedef signed   char   SByte;
typedef unsigned short  Word;

template <typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;





