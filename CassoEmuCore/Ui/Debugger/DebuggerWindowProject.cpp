#include "Pch.h"

#include "Ui/Debugger/DebuggerWindow.h"

#include "Core/TextEncoding.h"
#include "Debugger/Source/SourcePathList.h"
#include "Ui/Debugger/Panes/MemoryPane.h"
#include "Ui/Debugger/UndoText.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/ProjectSettingsDialog.h"





////////////////////////////////////////////////////////////////////////////////
//
//  GetProjectFileSpec
//
//  The file picker's filter for debug projects.
//
////////////////////////////////////////////////////////////////////////////////

static FileDialogSpec GetProjectFileSpec()
{
    return { { { L"Casso debug projects", L"*.cassoproj" }, { L"All files", L"*.*" } }, L"cassoproj" };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::SaveProjectAs
//
//  A copy of the open project in a file the user picks, which becomes the
//  project's file (FR-170).
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::SaveProjectAs()
{
    HRESULT                hr     = S_OK;
    std::filesystem::path  chosen;
    bool                   picked = false;



    BAIL_OUT_IF (m_host == nullptr, S_OK);

    hr = m_host->GetHostDialogs().PickFileToSave (GetHwnd(), GetProjectFileSpec(), chosen, picked);
    CHR (hr);

    BAIL_OUT_IF (!picked, S_OK);

    m_host->RunDebuggerCommandInMode (std::format ("PROJECT SAVEAS \"{}\"", TextEncoding::WideToNarrow (chosen.wstring())), CommandMode::AppleWin);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::OpenProject
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::OpenProject()
{
    HRESULT                hr     = S_OK;
    std::filesystem::path  chosen;
    bool                   picked = false;



    BAIL_OUT_IF (m_host == nullptr, S_OK);

    hr = m_host->GetHostDialogs().PickFileToOpen (GetHwnd(), GetProjectFileSpec(), chosen, picked);
    CHR (hr);

    BAIL_OUT_IF (!picked, S_OK);

    m_host->RunDebuggerCommandInMode (std::format ("PROJECT OPEN \"{}\"", TextEncoding::WideToNarrow (chosen.wstring())), CommandMode::AppleWin);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::EditProjectSettings
//
//  The open project's startup commands (FR-171), on a dialog of rows until
//  the editable grid exists.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::EditProjectSettings()
{
    DebugProject                                           project;
    std::optional<ProjectSettingsDialog::StartupCommands>  commands;



    if (m_host == nullptr || !m_host->TryGetDebuggerProject (project))
    {
        return;
    }

    commands = ProjectSettingsDialog::RunModal (GetHwnd(), m_theme, project);

    if (commands.has_value())
    {
        m_host->SetDebuggerStartupCommands (*commands);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ReloadProjectState
//
//  Another project opened and its strings are the host's now: the panes
//  laid out as it has them, its disassembly options, its heat map ranges,
//  and its open views opened.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ReloadProjectState()
{
    std::wstring    savedText = ReadSavedLayout();
    DxuiPaneLayout  restored  = DebuggerLayout::Restore (savedText);



    restored.PlaceOnMonitors (GetMonitors());
    m_dockSite->SetPaneLayout (restored);
    m_syncFloats = true;

    m_codeOptions = DisassemblyOptions::FromText (m_host->GetDebuggerDisassemblyOptions());

    for (int view = 0; view < DebuggerViewState::kMaxCodeViews && m_snapshot != nullptr; view++)
    {
        ApplyCodeView (view);
    }

    m_isHeatRangesOpened = !m_host->GetDebuggerHeatMapRanges().empty();
    m_heatRanges         = HeatMapRangeSets::FromText (m_host->GetDebuggerHeatMapRanges());
    (void) m_heatRanges.TryAddDefaultSet();
    m_heatRangeSet       = (m_heatRanges.FindSet (m_heatRangeSet) != nullptr || m_heatRanges.sets.empty()) ? m_heatRangeSet : m_heatRanges.sets.front().name;

    ApplyHeatRanges();
    SetHeatRangeMenus();
    RestoreWindowState (true);

    m_openViewsRestored = false;
    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RestoreWindowState
//
//  The console history a line each; each pane's find state a line of its
//  pane, its options as letters (C for match case, W for whole word, R for
//  regular expression) and its history, apart at the unit separator; and
//  each sized list's columns as "pane=120,80,64;". A project's switch also
//  brings back the heat map's and the event logs' options and the pane that
//  had the focus.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::RestoreWindowState (bool isProjectSwitch)
{
    std::vector<std::wstring>  lines;
    std::wistringstream        console;
    std::wistringstream        finds;
    std::istringstream         columns;
    std::wstring               line;
    std::string                entry;



    if (m_host == nullptr)
    {
        return;
    }

    console.str (SourcePathList::Utf8ToWide (m_host->GetDebuggerWindowState ("console")));

    while (std::getline (console, line))
    {
        lines.push_back (line);
    }

    m_consoleHistory.SetLines (std::move (lines));

    m_savedFinds.clear();
    finds.str (SourcePathList::Utf8ToWide (m_host->GetDebuggerWindowState ("find")));

    while (std::getline (finds, line))
    {
        std::vector<std::wstring>  parts;
        size_t                     at = 0;
        SavedFind                  saved;

        for (size_t next = line.find (L'\x1F'); ; next = line.find (L'\x1F', at))
        {
            parts.push_back (line.substr (at, (next == std::wstring::npos) ? std::wstring::npos : next - at));

            if (next == std::wstring::npos)
            {
                break;
            }

            at = next + 1;
        }

        if (parts.size() < 2 || parts[0].empty())
        {
            continue;
        }

        saved.matchCase = parts[1].find (L'C') != std::wstring::npos;
        saved.wholeWord = parts[1].find (L'W') != std::wstring::npos;
        saved.isRegex   = parts[1].find (L'R') != std::wstring::npos;
        saved.history.assign (parts.begin() + 2, parts.end());
        std::erase (saved.history, std::wstring());

        m_savedFinds[parts[0]] = std::move (saved);
    }

    for (auto & [pane, state] : m_findStates)
    {
        ApplySavedFind (pane, state);
    }

    if (m_findBox != nullptr)
    {
        LoadFindState();
    }

    m_columnWidths.clear();
    columns.str (m_host->GetDebuggerWindowState ("columns"));

    while (std::getline (columns, entry, ';'))
    {
        size_t              equals = entry.find ('=');
        std::istringstream  widths;
        std::string         width;
        std::vector<int>    dips;

        if (equals == std::string::npos)
        {
            continue;
        }

        widths.str (entry.substr (equals + 1));

        while (std::getline (widths, width, ','))
        {
            dips.push_back (std::max (0, std::atoi (width.c_str())));
        }

        m_columnWidths[SourcePathList::Utf8ToWide (entry.substr (0, equals))] = std::move (dips);
    }

    ApplyColumnWidths();

    //  The registers', the stack's and each memory window's undo, a line
    //  each, its name before its history.
    for (const std::string & each : UndoText::Split (m_host->GetDebuggerWindowState ("undo"), '\n'))
    {
        size_t       equals = each.find ('=');
        std::string  name   = each.substr (0, equals);
        std::string  text   = (equals == std::string::npos) ? std::string() : each.substr (equals + 1);

        if (name == "registers")
        {
            m_registerHistory.FromText (text);
        }
        else if (name == "stack")
        {
            m_stackHistory.FromText (text);
        }
        else if (name.starts_with ("memory") && name.size() == 7 && name[6] >= '1' && name[6] < '1' + DebuggerViewState::kMaxMemoryWindows)
        {
            m_memoryPanes[(size_t) (name[6] - '1')]->SetHistoryText (text);
        }
    }

    if (!isProjectSwitch)
    {
        return;
    }

    m_heatMapView->SetOptions (HeatMapOptions::FromText (m_host->GetDebuggerHeatMapOptions()));
    m_host->SetDebuggerHeatMapOptions (m_host->GetDebuggerHeatMapOptions());

    m_diskLogPane->SetOptionsText  (m_host->GetDebuggerEventLogOptions (DebugLogKind::Disk));
    m_inputLogPane->SetOptionsText (m_host->GetDebuggerEventLogOptions (DebugLogKind::Input));
    m_diskLog.isOpened  = m_host->GetDebuggerEventLogOptions (DebugLogKind::Disk).find ("open=1")  != std::string::npos;
    m_inputLog.isOpened = m_host->GetDebuggerEventLogOptions (DebugLogKind::Input).find ("open=1") != std::string::npos;

    if (!GetSavedFocusPane().empty())
    {
        FocusPane (GetSavedFocusPane());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetUndoHistoriesText
//
////////////////////////////////////////////////////////////////////////////////

std::string DebuggerWindow::GetUndoHistoriesText() const
{
    std::string  text = "registers=" + m_registerHistory.ToText() + "\nstack=" + m_stackHistory.ToText() + "\n";



    for (size_t i = 0; i < m_memoryPanes.size(); i++)
    {
        text += std::format ("memory{}={}\n", i + 1, m_memoryPanes[i]->GetHistoryText());
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::SaveUndoHistories
//
//  Once a second at most, and to the host only when they changed.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::SaveUndoHistories()
{
    constexpr uint64_t  kIntervalMs = 1000;
    uint64_t            now         = GetTickCount64();



    if (m_host == nullptr || now - m_undoSavedMs < kIntervalMs)
    {
        return;
    }

    m_undoSavedMs = now;
    m_host->SetDebuggerWindowState ("undo", GetUndoHistoriesText());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ApplySavedFind
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ApplySavedFind (const std::wstring & pane, FindState & state) const
{
    auto  saved = m_savedFinds.find (pane);



    if (saved == m_savedFinds.end())
    {
        return;
    }

    state.matchCase = saved->second.matchCase;
    state.wholeWord = saved->second.wholeWord;
    state.isRegex   = saved->second.isRegex;
    state.history   = saved->second.history;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::SaveConsoleHistory
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::SaveConsoleHistory()
{
    std::wstring  text;



    for (const std::wstring & line : m_consoleHistory.GetLines())
    {
        text += line + L"\n";
    }

    if (m_host != nullptr)
    {
        m_host->SetDebuggerWindowState ("console", SourcePathList::WideToUtf8 (text));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::SaveFindStates
//
//  Every pane's, with those a project brought back for widgets not built
//  yet.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::SaveFindStates()
{
    std::map<std::wstring, SavedFind>  all = m_savedFinds;
    std::wstring                       text;



    if (m_findBox != nullptr)
    {
        SaveFindState();
    }

    for (const auto & [pane, state] : m_findStates)
    {
        all[pane] = SavedFind { state.matchCase, state.wholeWord, state.isRegex, state.history };
    }

    for (const auto & [pane, saved] : all)
    {
        text += pane + L"\x1F" + (saved.matchCase ? L"C" : L"") + (saved.wholeWord ? L"W" : L"") + (saved.isRegex ? L"R" : L"");

        for (const std::wstring & each : saved.history)
        {
            text += L"\x1F" + each;
        }

        text += L"\n";
    }

    if (m_host != nullptr)
    {
        m_host->SetDebuggerWindowState ("find", SourcePathList::WideToUtf8 (text));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::SaveColumnWidths
//
//  A list the user sized, or one a project brought widths back for, is saved
//  with each column's width as it is now, in dips.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::SaveColumnWidths()
{
    std::string  text;



    for (DxuiListView * list : GetLists())
    {
        std::wstring  pane = GetPaneOfControl (list);

        if (pane.empty() || !m_columnWidths.contains (pane))
        {
            continue;
        }

        m_columnWidths[pane].clear();
        text += SourcePathList::WideToUtf8 (pane) + "=";

        for (size_t c = 0; c < list->GetColumnCount(); c++)
        {
            int  dip = MulDiv (list->GetColumnEffectiveWidthPx (c), DxuiDpiScaler::kBaseDpi, (int) m_scaler.GetDpi());

            m_columnWidths[pane].push_back (dip);
            text += std::format ("{}{}", (c == 0) ? "" : ",", dip);
        }

        text += ";";
    }

    if (m_host != nullptr)
    {
        m_host->SetDebuggerWindowState ("columns", text);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ApplyColumnWidths
//
//  A width of zero, a hidden column's, is left to the list.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ApplyColumnWidths()
{
    for (DxuiListView * list : GetLists())
    {
        auto  widths = m_columnWidths.find (GetPaneOfControl (list));

        if (widths == m_columnWidths.end())
        {
            continue;
        }

        for (size_t c = 0; c < widths->second.size() && c < list->GetColumnCount(); c++)
        {
            if (widths->second[c] > 0)
            {
                list->SetColumnOverrideWidthPx (c, m_scaler.ToPx (widths->second[c]));
            }
        }
    }
}
