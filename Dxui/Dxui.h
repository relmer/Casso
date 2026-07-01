#pragma once

//
//  Dxui.h is the public umbrella and is expensive to parse: it re-exports
//  the entire framework surface. It is meant to be pulled in exactly once,
//  from a consuming project's precompiled header (Pch.h), so this cost is
//  paid during PCH creation rather than per translation unit. A consumer
//  opts in by defining DXUI_UMBRELLA_VIA_PCH before the include; a cold or
//  non-PCH inclusion trips the #error below instead of silently recompiling
//  the whole surface.
//
#if !defined(DXUI_UMBRELLA_VIA_PCH)
    #error "Dxui.h must be included via a precompiled header (Pch.h) that #defines DXUI_UMBRELLA_VIA_PCH first; direct / non-PCH inclusion is not supported."
#endif



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

#include <ole2.h>
#include <oleidl.h>

#include <wrl/client.h>

#include <atomic>
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
#include <vector>



////////////////////////////////////////////////////////////////////////////////
//
//  DirectX / COM forward declarations.
//
//  These interfaces appear in the public Dxui headers ONLY as pointers
//  (raw or `ComPtr<T>`, which stores a `T*`), never by value, as a base
//  class, or with inline method calls. Forward declarations therefore
//  satisfy the public surface, so consumers never parse the full
//  d3d11 / d2d1 / dwrite / dxgi / dcomp header tree. Dxui's own
//  translation units get the complete definitions via `Pch.h`.
//
////////////////////////////////////////////////////////////////////////////////

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11RenderTargetView;
struct ID3D11VertexShader;
struct ID3D11PixelShader;
struct ID3D11InputLayout;
struct ID3D11Buffer;
struct ID3D11BlendState;
struct ID3D11RasterizerState;
struct ID3D11DepthStencilState;
struct ID2D1Factory1;
struct ID2D1Device;
struct ID2D1DeviceContext;
struct ID2D1Bitmap;
struct ID2D1Bitmap1;
struct IDWriteFactory;
struct IDWriteTextFormat;
struct IDXGISurface;
struct IDXGISwapChain1;
struct IDCompositionDevice;
struct IDCompositionTarget;
struct IDCompositionVisual;



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
