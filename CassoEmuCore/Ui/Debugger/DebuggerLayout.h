#pragma once

#include "Pch.h"
#include "Core/DxuiPaneLayout.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerLayout
//
//  The debugger's pane ids and the arrangement it opens with before the user
//  has moved anything. The ids are what a saved layout stores, so they never
//  change once shipped.
//
//  The default keeps the window as it was before panes could be docked: the
//  source beside the disassembly over the console on the left, registers,
//  breakpoints and watches down the right, and memory beside the stack across
//  the bottom, with memory windows 2 to 4 as tabs of the first and the call
//  stack as a tab of the stack. The trace is a tab of the console.
//
////////////////////////////////////////////////////////////////////////////////

class DebuggerLayout
{
public:
    static constexpr const wchar_t * kCode        = L"code";
    static constexpr const wchar_t * kSource      = L"source";
    static constexpr const wchar_t * kConsole     = L"console";
    static constexpr const wchar_t * kRegisters   = L"registers";
    static constexpr const wchar_t * kBreakpoints = L"breakpoints";
    static constexpr const wchar_t * kWatches     = L"watches";
    static constexpr const wchar_t * kStack       = L"stack";
    static constexpr const wchar_t * kCallStack   = L"callstack";
    static constexpr const wchar_t * kTrace       = L"trace";

    //  Memory windows are "memory1" to "memory4".
    static std::wstring    GetMemoryPaneId (int window);

    static DxuiPaneLayout  MakeDefault ();

    //  A saved arrangement: panes this version does not have are dropped and
    //  panes it has that the text lacks are added, so a layout saved by an
    //  older or newer build still holds every pane. Text that does not parse
    //  gives the default.
    static DxuiPaneLayout  Restore     (const std::wstring & text);

    static std::vector<std::wstring>  GetPaneIds ();

private:
    //  The pane a restored layout tabs a missing pane with, or none.
    static std::wstring  GetDefaultTabHost (const DxuiPaneLayout & layout, const std::wstring & pane);
};