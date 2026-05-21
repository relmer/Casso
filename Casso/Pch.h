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
#include <commctrl.h>
#include <richedit.h>
#include <winhttp.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <wincodec.h>
#include <dwrite_3.h>

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

#include <RmlUi/Core.h>
#include <RmlUi/Debugger.h>

using namespace std;
namespace fs = std::filesystem;

typedef unsigned char   Byte;
typedef signed   char   SByte;
typedef unsigned short  Word;

template <typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;
#define CASSO_COMPTR_ALIAS_DECLARED 1





