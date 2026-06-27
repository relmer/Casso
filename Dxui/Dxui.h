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
#include <windowsx.h>

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

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <deque>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#if defined(_DEBUG)
    #ifndef DXUI_ASSERT_UI_THREAD
        #define DXUI_ASSERT_UI_THREAD() DxuiAssertUiThread()
    #endif
#else
    #ifndef DXUI_ASSERT_UI_THREAD
        #define DXUI_ASSERT_UI_THREAD() ((void) 0)
    #endif
#endif




////////////////////////////////////////////////////////////////////////////////
//
//  Public Dxui headers (umbrella re-exports).
//
//  Listed alphabetically by subfolder. Consumers get the entire public
//  Dxui surface just by including this umbrella (typically via their
//  own Pch.h).
//
////////////////////////////////////////////////////////////////////////////////

#include "Core/DxuiAnimation.h"
#include "Core/DxuiDpiScaler.h"
#include "Core/DxuiEvents.h"
#include "Core/DxuiHitTester.h"
#include "Core/DxuiInput.h"
#include "Core/DxuiThread.h"
#include "Core/DxuiTitleBarHitTest.h"
#include "Core/IDxuiControl.h"
#include "Core/IDxuiLayout.h"
#include "Core/IDxuiViewportInputSink.h"
#include "Core/DxuiAbsoluteLayout.h"
#include "Core/DxuiDockLayout.h"
#include "Core/DxuiFormLayout.h"
#include "Core/DxuiGridLayout.h"
#include "Core/DxuiStackLayout.h"
#include "Core/DxuiPanel.h"
#include "Core/DxuiViewport.h"
#include "Core/DxuiFocusManager.h"

// ComPtr alias needed by Render/* headers below. Defined in the
// umbrella so any consumer including Dxui.h (typically via their
// own Pch.h) gets the alias before the Render headers are parsed,
// regardless of whether the consumer's Pch also defines one later.
#ifndef DXUI_COMPTR_ALIAS_DEFINED
#define DXUI_COMPTR_ALIAS_DEFINED
template <typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;
#endif

#include "Render/IDxuiPainter.h"
#include "Render/IDxuiTextRenderer.h"
#include "Render/DxuiPainter.h"
#include "Render/DxuiTextRenderer.h"
#include "Theme/DxuiDwm.h"
#include "Theme/DxuiWindowsThemeColors.h"
#include "Theme/IDxuiTheme.h"
#include "Widgets/DxuiButton.h"
#include "Widgets/DxuiCheckbox.h"
#include "Widgets/DxuiDropdown.h"
#include "Widgets/DxuiLabel.h"
#include "Widgets/DxuiListView.h"
#include "Widgets/DxuiMenuBar.h"
#include "Widgets/DxuiModalScrim.h"
#include "Widgets/DxuiPopupMenu.h"
#include "Widgets/DxuiRadio.h"
#include "Widgets/DxuiSearchBox.h"
#include "Widgets/DxuiSlider.h"
#include "Widgets/DxuiTabStrip.h"
#include "Widgets/DxuiTextInput.h"
#include "Widgets/DxuiToggle.h"
#include "Widgets/DxuiTooltip.h"
#include "Widgets/DxuiTreeView.h"
#include "Window/DxuiDragDropTarget.h"
#include "Window/DxuiDragRegion.h"
#include "Window/DxuiCaptionBar.h"
#include "Window/DxuiSystemButton.h"
#include "Window/IDxuiHostClient.h"
#include "Window/DxuiHostWindow.h"
#include "Window/DxuiPopupHost.h"
#include "Dialog/DxuiDialog.h"
#include "Dialog/DxuiDialogManager.h"
