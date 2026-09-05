#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>

// <winnt.h> aliases these to the _bittest intrinsics, and CpuOperations::BitTest
// is the 6502 BIT instruction. CassoCore/Pch.h is where that actually bites --
// it compiles the definition, and its Byte typedef means every consumer of a
// CPU header goes through it anyway -- so this is belt and braces: a Pch that
// pulls Windows in should not leave the macro live behind it.
#undef BitTest
#undef BitTestAndSet
#undef BitTestAndReset
#undef BitTestAndComplement

#include <d3d11.h>
#include <d3dcompiler.h>
#include <d2d1_3.h>
#include <d2d1helper.h>
#include <dwrite_3.h>
#include <dxgi1_3.h>
#include <dxgidebug.h>
#include <dcomp.h>
#include <wincodec.h>
#include <dwmapi.h>
#include <ole2.h>
#include <oleidl.h>
#include <shellapi.h>

#include <wrl/client.h>

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <deque>
#include <functional>
#include <future>
#include <iterator>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../CassoCore/Ehm.h"

#include "Core/DxuiThread.h"

template <typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

#include "Core/DxuiAnimation.h"
#include "Core/DxuiDpiScaler.h"
#include "Core/DxuiHitTester.h"
#include "Core/DxuiInput.h"
#include "Render/DxuiPainter.h"
#include "Render/DxuiTextRenderer.h"
#include "Theme/DxuiDwm.h"
#include "Theme/DxuiWindowsThemeColors.h"
#include "Theme/IDxuiTheme.h"
#include "Window/DxuiDragDropTarget.h"
