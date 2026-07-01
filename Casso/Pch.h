#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#include <dxgidebug.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <commdlg.h>
#include <commctrl.h>
#include <richedit.h>
#include <winhttp.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <wincodec.h>
#include <dwrite_3.h>
#include <shellscalingapi.h>
#include <d2d1_3.h>
#include <d2d1helper.h>
#include <dxgi1_2.h>
#include <dxgi1_3.h>
#include <dcomp.h>

#include <crtdbg.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <shlobj.h>
#include <ole2.h>
#include <oleidl.h>
#include <wrl/client.h>
#include <bcrypt.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <deque>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <numbers>
#include <set>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../CassoCore/Ehm.h"

// Opt in to the Dxui umbrella. Dxui.h hard-errors unless this is defined,
// which enforces that its heavy header surface is only ever parsed here
// (during PCH creation) and never cold in a per-TU compile.
#define DXUI_UMBRELLA_VIA_PCH
#include "Dxui.h"

using namespace std;
namespace fs = std::filesystem;

typedef unsigned char   Byte;
typedef signed   char   SByte;
typedef unsigned short  Word;





