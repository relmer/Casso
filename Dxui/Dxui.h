#pragma once

////////////////////////////////////////////////////////////////////////////////
//
//  Dxui — public umbrella header.
//
//  This is the single public chokepoint for Dxui's system-header surface.
//  Consumers include this header (typically from their own Pch.h) instead of
//  pulling in DirectX / Direct2D / DirectWrite / DCOMP / WIC headers directly.
//
//  THREADING: Dxui is UI-thread-only. All public Dxui APIs must be called on
//  the host window message-pump thread. The DXUI_ASSERT_UI_THREAD() macro
//  below is intended for use at the top of public Dxui entry points to catch
//  cross-thread misuse in debug builds.
//
////////////////////////////////////////////////////////////////////////////////

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <d3d11.h>
#include <d3dcompiler.h>
#include <d2d1.h>
#include <dwrite.h>
#include <dxgi1_3.h>
#include <dcomp.h>
#include <wincodec.h>

#include <wrl/client.h>

#include <future>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <cstdint>
#include <cmath>

#if defined(_DEBUG)
    #define DXUI_ASSERT_UI_THREAD() ((void) 0)
#else
    #define DXUI_ASSERT_UI_THREAD() ((void) 0)
#endif
