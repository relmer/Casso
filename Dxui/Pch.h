#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <d3d11.h>
#include <d3dcompiler.h>
#include <d2d1_3.h>
#include <d2d1helper.h>
#include <dwrite_3.h>
#include <dxgi1_3.h>
#include <dcomp.h>
#include <wincodec.h>
#include <dwmapi.h>
#include <ole2.h>
#include <oleidl.h>
#include <shellapi.h>

#include <wrl/client.h>

#include <atomic>
#include <cmath>
#include <cstdint>
#include <deque>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "../CassoCore/Ehm.h"

template <typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;
