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
//  DebuggerLayout::GetPaneIds
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> DebuggerLayout::GetPaneIds()
{
    std::vector<std::wstring>  ids = { kCode, kSource, kConsole, kRegisters, kBreakpoints, kWatches, kStack, kTrace };



    for (int window = 1; window <= DebuggerViewState::kMaxMemoryWindows; window++)
    {
        ids.push_back (GetMemoryPaneId (window));
    }

    return ids;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerLayout::Restore
//
////////////////////////////////////////////////////////////////////////////////

DxuiPaneLayout DebuggerLayout::Restore (const std::wstring & text)
{
    DxuiPaneLayout             layout;
    std::vector<std::wstring>  ids    = GetPaneIds();



    if (text.empty() || !DxuiPaneLayout::TryParse (text, layout))
    {
        return MakeDefault();
    }

    layout.DropUnknown ([&ids] (const std::wstring & pane)
    {
        return std::find (ids.begin(), ids.end(), pane) != ids.end();
    });

    if (layout.GetRoot() == nullptr)
    {
        return MakeDefault();
    }

    //  A memory window the text lacks joins the first and the trace joins the
    //  console, as they open by default; anything else lacks a better place
    //  than the right edge.
    for (const std::wstring & pane : ids)
    {
        if (!layout.Contains (pane))
        {
            layout.Add (pane, GetDefaultTabHost (layout, pane));
        }
    }

    return layout;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerLayout::GetDefaultTabHost
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DebuggerLayout::GetDefaultTabHost (const DxuiPaneLayout & layout, const std::wstring & pane)
{
    if (pane.starts_with (L"memory"))
    {
        return GetMemoryPaneId (1);
    }

    if (pane == kTrace && layout.Contains (kConsole))
    {
        return kConsole;
    }

    return L"";
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
    layout.Add        (kTrace,       kConsole);
    layout.Activate   (kConsole);

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