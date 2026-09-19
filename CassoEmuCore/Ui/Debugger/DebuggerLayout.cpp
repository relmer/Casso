#include "Pch.h"

#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerViewState.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerLayout::GetMemoryPaneId
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DebuggerLayout::GetMemoryPaneId (int window)
{
    return std::format (L"memory{}", window);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerLayout::MakeDefault
//
//  Built by the same operations a user would perform, so the default is a
//  layout like any other and saves and restores the same way.
//
////////////////////////////////////////////////////////////////////////////////

DxuiPaneLayout DebuggerLayout::MakeDefault()
{
    DxuiPaneLayout  layout  = DxuiPaneLayout::MakeSingle (kCode);
    std::wstring    memory1 = GetMemoryPaneId (1);



    layout.Add        (kRegisters,   L"");
    layout.Add        (kConsole,     L"");
    layout.DockToSide (kConsole,     kCode,        DxuiDockSide::Bottom);
    layout.Add        (kSource,      L"");
    layout.DockToSide (kSource,      kCode,        DxuiDockSide::Left);
    layout.Add        (kBreakpoints, L"");
    layout.DockToSide (kBreakpoints, kRegisters,   DxuiDockSide::Bottom);
    layout.Add        (kWatches,     L"");
    layout.DockToSide (kWatches,     kBreakpoints, DxuiDockSide::Bottom);
    layout.Add        (memory1,      L"");
    layout.DockToEdge (memory1,      DxuiDockSide::Bottom);

    for (int window = 2; window <= DebuggerViewState::kMaxMemoryWindows; window++)
    {
        layout.Add (GetMemoryPaneId (window), memory1);
    }

    layout.Add        (kStack,       L"");
    layout.DockToSide (kStack,       memory1,      DxuiDockSide::Right);
    layout.Activate   (memory1);

    //  Proportions of the fixed layout this replaces: a right column of about
    //  300 of 1100, memory eight rows high, and code over a shorter console.
    //  A path is the split's place in the tree, one digit a level.
    layout.SetRatio (L"",    0.70f);
    layout.SetRatio (L"0",   0.73f);
    layout.SetRatio (L"00",  0.65f);
    layout.SetRatio (L"000", 0.60f);
    layout.SetRatio (L"01",  0.34f);
    layout.SetRatio (L"1",   0.73f);

    return layout;
}