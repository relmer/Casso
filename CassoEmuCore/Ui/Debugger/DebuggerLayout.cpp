#include "Pch.h"

#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerViewState.h"
#include "Ui/Debugger/Panes/SourceDocuments.h"





static constexpr DebuggerLayout::DiagnosticsPanel  s_kDiagnosticsPanels[] =
{
    { "clock",        L"Clock"        },
    { "keyboard",     L"Keyboard"     },
    { "video",        L"Video"        },
    { "mmu",          L"MMU"          },
    { "disk",         L"Disk II"      },
    { "mockingboard", L"Mockingboard" },
    { "printer",      L"Printer"      },
};

static constexpr const wchar_t * s_kpszDiagnosticsPrefix = L"diag-";





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
//  DebuggerLayout::GetCodePaneId
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DebuggerLayout::GetCodePaneId (int view)
{
    return (view <= 0) ? std::wstring (kCode) : std::format (L"code{}", view + 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerLayout::GetSourcePaneId
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DebuggerLayout::GetSourcePaneId (int slot)
{
    return (slot <= 0) ? std::wstring (kSource) : std::format (L"source{}", slot + 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerLayout::GetDiagnosticsPanels
//
////////////////////////////////////////////////////////////////////////////////

std::span<const DebuggerLayout::DiagnosticsPanel> DebuggerLayout::GetDiagnosticsPanels()
{
    return s_kDiagnosticsPanels;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerLayout::GetDiagnosticsPaneId
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DebuggerLayout::GetDiagnosticsPaneId (const std::string & id)
{
    return s_kpszDiagnosticsPrefix + std::wstring (id.begin(), id.end());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerLayout::TryGetDiagnosticsId
//
//  Ids are ASCII, so a pane id narrows character by character.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerLayout::TryGetDiagnosticsId (const std::wstring & pane, std::string & id)
{
    std::wstring_view  prefix = s_kpszDiagnosticsPrefix;



    if (!pane.starts_with (prefix))
    {
        return false;
    }

    id.clear();

    for (wchar_t ch : pane.substr (prefix.size()))
    {
        id += (char) ch;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerLayout::GetPaneIds
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> DebuggerLayout::GetPaneIds()
{
    std::vector<std::wstring>  ids = { kCode, kSource, kConsole, kRegisters, kBreakpoints, kWatches, kStack, kCallStack, kTrace, kHeatMap };



    for (int view = 1; view < DebuggerViewState::kMaxCodeViews; view++)
    {
        ids.push_back (GetCodePaneId (view));
    }

    for (int slot = 1; slot < SourceDocuments::kMaxDocuments; slot++)
    {
        ids.push_back (GetSourcePaneId (slot));
    }

    for (int window = 1; window <= DebuggerViewState::kMaxMemoryWindows; window++)
    {
        ids.push_back (GetMemoryPaneId (window));
    }

    for (const DiagnosticsPanel & panel : s_kDiagnosticsPanels)
    {
        ids.push_back (GetDiagnosticsPaneId (panel.id));
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

    //  A memory window or the heat map the text lacks joins the first memory
    //  window, and the trace and a
    //  device panel the console, as they open by default; the call stack
    //  joins the stack, and anything else lacks a better place than the
    //  right edge.
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
//  DebuggerLayout::ClosedPanesToText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DebuggerLayout::ClosedPanesToText (const std::set<std::wstring> & closed)
{
    std::wstring  text;



    for (const std::wstring & pane : closed)
    {
        if (!text.empty())
        {
            text += L' ';
        }

        text += pane;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerLayout::ReadClosedPanes
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerLayout::ReadClosedPanes (const std::wstring & text, std::set<std::wstring> & closed)
{
    std::wistringstream  panes (text);
    std::wstring         pane;



    closed.clear();

    while (panes >> pane)
    {
        closed.insert (pane);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerLayout::TakeClosedPanes
//
//  Text without the closed line, as every build before it saved, has no
//  pane closed.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DebuggerLayout::TakeClosedPanes (const std::wstring & text, std::set<std::wstring> & closed)
{
    size_t  end = text.find (L'\n');



    closed.clear();

    if (!text.starts_with (kClosedPrefix) || end == std::wstring::npos)
    {
        return text;
    }

    ReadClosedPanes (text.substr (wcslen (kClosedPrefix), end - wcslen (kClosedPrefix)), closed);

    return text.substr (end + 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerLayout::GetDefaultTabHost
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DebuggerLayout::GetDefaultTabHost (const DxuiPaneLayout & layout, const std::wstring & pane)
{
    if (pane.starts_with (L"memory") || pane == kHeatMap)
    {
        return GetMemoryPaneId (1);
    }

    if (pane.starts_with (kCode) && pane != kCode)
    {
        return kCode;
    }

    //  A source document the text lacks joins the first, as it opens by
    //  default.
    if (pane.starts_with (kSource) && pane != kSource && layout.Contains (kSource))
    {
        return kSource;
    }

    if (pane.starts_with (s_kpszDiagnosticsPrefix) && layout.Contains (kConsole))
    {
        return kConsole;
    }

    if (pane == kCallStack && layout.Contains (kStack))
    {
        return kStack;
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
//  On the left the disassembly over the console, which carries the trace
//  and the device panels as tabs; on the right the registers beside the
//  stack, then watches, call stack, breakpoints and memory, top to bottom,
//  with the heat map a tab beside the memory windows.
//
////////////////////////////////////////////////////////////////////////////////

DxuiPaneLayout DebuggerLayout::MakeDefault()
{
    DxuiPaneLayout  layout  = DxuiPaneLayout::MakeSingle (kCode);
    std::wstring    memory1 = GetMemoryPaneId (1);



    layout.Add        (kConsole,     L"");
    layout.DockToSide (kConsole,     kCode,        DxuiDockSide::Bottom);
    layout.Add        (kRegisters,   L"");
    layout.Add        (kWatches,     L"");
    layout.DockToSide (kWatches,     kRegisters,   DxuiDockSide::Bottom);
    layout.Add        (kCallStack,   L"");
    layout.DockToSide (kCallStack,   kWatches,     DxuiDockSide::Bottom);
    layout.Add        (kBreakpoints, L"");
    layout.DockToSide (kBreakpoints, kCallStack,   DxuiDockSide::Bottom);
    layout.Add        (memory1,      L"");
    layout.DockToSide (memory1,      kBreakpoints, DxuiDockSide::Bottom);
    layout.Add        (kStack,       L"");
    layout.DockToSide (kStack,       kRegisters,   DxuiDockSide::Right);

    for (int window = 2; window <= DebuggerViewState::kMaxMemoryWindows; window++)
    {
        layout.Add (GetMemoryPaneId (window), memory1);
    }

    //  The heat map is a tab beside the memory windows, since a cell clicked
    //  on it is shown in one.
    layout.Add (kHeatMap, memory1);

    for (int view = 1; view < DebuggerViewState::kMaxCodeViews; view++)
    {
        layout.Add (GetCodePaneId (view), kCode);
    }

    layout.Add (kTrace,   kConsole);

    for (const DiagnosticsPanel & panel : s_kDiagnosticsPanels)
    {
        layout.Add (GetDiagnosticsPaneId (panel.id), kConsole);
    }

    //  The source documents sit left of the disassembly, and show only
    //  while a debug file is loaded.
    layout.Add        (kSource,      L"");
    layout.DockToSide (kSource,      kCode,        DxuiDockSide::Left);

    for (int slot = 1; slot < SourceDocuments::kMaxDocuments; slot++)
    {
        layout.Add (GetSourcePaneId (slot), kSource);
    }

    layout.Activate (kSource);
    layout.Activate (kCode);
    layout.Activate (kConsole);
    layout.Activate (memory1);

    //  The left column about three fifths of the width, the disassembly two
    //  fifths of its height, and the right column's rows about equal, the
    //  registers and the stack sharing the first. A path is the split's
    //  place in the tree, one digit a level.
    layout.SetRatio (L"",     0.615f);
    layout.SetRatio (L"0",    0.41f);
    layout.SetRatio (L"00",   0.35f);
    layout.SetRatio (L"1",    0.20f);
    layout.SetRatio (L"10",   0.50f);
    layout.SetRatio (L"11",   0.25f);
    layout.SetRatio (L"111",  0.37f);
    layout.SetRatio (L"1111", 0.50f);

    return layout;
}
