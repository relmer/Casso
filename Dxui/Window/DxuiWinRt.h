#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiWinRt
//
//  The C++/WinRT headers for the popups' Windows.UI.Composition tree.
//
//  They are not in Pch.h because they take about four seconds to
//  parse and only DxuiPopupHost.cpp uses them. Dxui headers include Pch.h,
//  and Dxui.h is in the precompiled headers of CassoEmuCore, UnitTest and
//  ScenarioTests, so in Pch.h that cost was paid once per precompiled
//  header, each on the build's critical path.
//
//  Include this only from a .cpp, after Pch.h. Never include it from a
//  header that Dxui.h includes.
//
////////////////////////////////////////////////////////////////////////////////

//  winbase.h's GetCurrentTime macro collides with a WinRT method of that name.
#undef GetCurrentTime

#include <DispatcherQueue.h>
#include <windows.ui.composition.interop.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Numerics.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.UI.h>
#include <winrt/Windows.UI.Composition.h>
#include <winrt/Windows.UI.Composition.Desktop.h>
#include <winrt/Windows.UI.ViewManagement.h>
