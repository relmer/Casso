#include "Pch.h"

#include "Ui/Debugger/DebuggerWindow.h"

#include "Core/TextEncoding.h"
#include "Core/UnicodeSymbols.h"
#include "Debugger/SymbolTable.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/Panes/DebuggerPaneFrame.h"
#include "Ui/Debugger/Panes/FlameGraphView.h"
#include "Ui/Debugger/Panes/ProfileTimeline.h"
#include "Ui/Debugger/Panes/ToolPanes.h"
#include "Ui/Debugger/StepFiltersDialog.h"





//  The profiler bar's entries.
static constexpr int  s_kProfileOn    = 1;
static constexpr int  s_kProfileReset = 2;
static constexpr int  s_kProfileSave  = 3;
static constexpr int  s_kProfileView  = 4;





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::CreateToolPanes
//
//  The Symbols, Bookmarks, Profiler and data ranges panes (FR-182, FR-183,
//  FR-189, FR-193), each a list. The profiler's timeline and flame graph,
//  then its bar, which paints over them.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::CreateToolPanes()
{
    auto  show = [this] (Word address) { ShowCode (address); };



    m_symbolList    = CreateChild<DxuiListView>();
    m_bookmarkList  = CreateChild<DxuiListView>();
    m_profilerList  = CreateChild<DxuiListView>();
    m_dataBlockList = CreateChild<DxuiListView>();
    m_profileTimeline = CreateChild<ProfileTimeline>();
    m_flameGraph    = CreateChild<FlameGraphView>();
    m_profilerBar   = CreateChild<DxuiToolbar>();

    m_flameGraph->SetOnActivate (show);

    m_symbolsPane    = std::make_unique<SymbolsPane>    (m_symbolList, show, [this] (const std::string & table, bool enable)
    {
        RunAction (DebuggerActions::GetEnableSymbols (SymbolTable::ToUpper (table), enable, GetMode()));
    });
    m_bookmarksPane  = std::make_unique<BookmarksPane>  (m_bookmarkList,  show);
    m_profilerPane   = std::make_unique<ProfilerPane>   (m_profilerList,  show);
    m_dataBlocksPane = std::make_unique<DataBlocksPane> (m_dataBlockList, show);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ConfigureToolPanes
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ConfigureToolPanes()
{
    m_symbolsPane->Configure();
    m_bookmarksPane->Configure();
    m_profilerPane->Configure();
    m_dataBlocksPane->Configure();

    m_profilerBar->SetTextRenderer  (GetTextRenderer());
    m_profilerBar->SetPopupHost     (GetPopupHost());
    m_profilerBar->SetIconFace      (DxuiToolbar::kMdl2IconFace);
    m_profilerBar->SetCompact       (true);
    m_profilerBar->SetPaneTextInset (true);
    m_profilerBar->EnableSeeMore    (s_kpszMdl2More, L"See more");
    m_profilerBar->SetVisible       (false);

    SetProfilerBarEntries();
    SetProfilerBarMenu();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::AddToolPanes
//
//  Each is its list, with its commands on its rows' menus. The profiler is
//  its bar, in a place PlaceProfilerBar fills, over its timeline, over its
//  list or, in the flame graph view, its flame graph.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::AddToolPanes()
{
    auto  isFlame = [this] { return m_profilerPane->GetView() == ProfilerPane::View::Flame; };



    m_profilerSlot = std::make_unique<DebuggerPaneFrame> (L"Profiler commands");

    for (const std::wstring & pane : DebuggerLayout::GetToolPaneIds())
    {
        m_toolFrames[pane] = std::make_unique<DebuggerPaneFrame> (GetPaneTitle (pane));

        if (pane != DebuggerLayout::kProfiler)
        {
            m_toolFrames[pane]->AddPart (GetToolList (pane));
        }
        else
        {
            m_toolFrames[pane]->AddPart (m_profilerSlot.get(),    [] (int, const DxuiDpiScaler & scaler) { return scaler.ToPx (DxuiToolbar::kCompactBandDp); });
            m_toolFrames[pane]->AddPart (m_profileTimeline,       [] (int, const DxuiDpiScaler & scaler) { return scaler.ToPx (ProfileTimeline::kHeightDip); });
            m_toolFrames[pane]->AddPart (m_profilerList,          nullptr, [isFlame] { return !isFlame(); });
            m_toolFrames[pane]->AddPart (m_flameGraph,            nullptr, isFlame);
            m_toolFrames[pane]->SetPartInsetDip (m_profileTimeline, 4);
        }

        m_dockSite->AddPane (pane, GetPaneTitle (pane), m_toolFrames[pane].get());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetToolFrame
//
////////////////////////////////////////////////////////////////////////////////

DebuggerPaneFrame * DebuggerWindow::GetToolFrame (const std::wstring & pane) const
{
    auto  found = m_toolFrames.find (pane);



    return (found != m_toolFrames.end()) ? found->second.get() : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetToolList
//
////////////////////////////////////////////////////////////////////////////////

DxuiListView * DebuggerWindow::GetToolList (const std::wstring & pane) const
{
    if (pane == DebuggerLayout::kSymbols)    { return m_symbolList;    }
    if (pane == DebuggerLayout::kBookmarks)  { return m_bookmarkList;  }
    if (pane == DebuggerLayout::kProfiler)   { return m_profilerList;  }
    if (pane == DebuggerLayout::kDataBlocks) { return m_dataBlockList; }

    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ApplyToolPanes
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ApplyToolPanes()
{
    m_symbolsPane->Apply    (m_snapshot->symbols);
    m_bookmarksPane->Apply  (m_snapshot->bookmarks, m_snapshot->symbols.get());
    m_profilerPane->Apply   (m_snapshot->profile, m_snapshot->profileByAddress, m_snapshot->isProfiling);
    m_dataBlocksPane->Apply (m_snapshot->dataBlocks);

    m_profileTimeline->SetData (m_profilerPane->GetProfile());
    m_flameGraph->SetData      (m_profilerPane->GetProfile());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::AddToolMenuItems
//
//  A tool pane's row menu; false for a list that is not one of theirs.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::AddToolMenuItems (DxuiListView * list, int row, std::vector<std::pair<std::wstring, std::function<void()>>> & items)
{
    if (list == m_symbolList)
    {
        const SymbolsPane::Row  * each = m_symbolsPane->GetRow (row);



        if (each != nullptr && !each->isHeading)
        {
            items.push_back ({ L"Go to",  [this, at = each->address] { ShowCode (at); } });
            items.push_back ({ L"Remove", [this, table = each->table, name = each->name]
            {
                RunAction (DebuggerActions::GetRemoveSymbol (SymbolTable::ToUpper (table), name, GetMode()));
            } });
        }

        if (each != nullptr)
        {
            items.push_back ({ L"Clear table",    [this, table = each->table] { RunAction (DebuggerActions::GetClearSymbols (SymbolTable::ToUpper (table), GetMode())); } });
            items.push_back ({ L"Save table...",  [this, table = each->table] { SaveSymbolTable (table); } });
        }

        return true;
    }

    if (list == m_bookmarkList)
    {
        std::optional<DebuggerViewSnapshot::WatchLine>  bookmark = m_bookmarksPane->GetBookmark (row);



        if (bookmark.has_value())
        {
            items.push_back ({ L"Go to",  [this, at = bookmark->address] { ShowCode (at); } });
            items.push_back ({ L"Delete", [this, id = bookmark->id]      { RunAction (DebuggerActions::GetClearBookmark (id, GetMode())); } });
        }

        return true;
    }

    if (list == m_profilerList)
    {
        items.push_back ({ m_profilerPane->IsOn() ? L"Stop profiling" : L"Start profiling",
                           [this, on = m_profilerPane->IsOn()] { RunAction (DebuggerActions::GetProfile (on ? "OFF" : "ON", GetMode())); } });
        items.push_back ({ L"Reset",   [this] { RunAction (DebuggerActions::GetProfile ("RESET", GetMode())); } });
        items.push_back ({ L"Save...", [this] { SaveProfile(); } });
        return true;
    }

    if (list == m_dataBlockList)
    {
        std::optional<DataBlockEntry>  block = m_dataBlocksPane->GetBlock (row);



        if (block.has_value())
        {
            items.push_back ({ L"Go to",  [this, at = block->first] { ShowCode (at); } });
            items.push_back ({ L"Delete", [this, from = block->first, to = block->last] { RunAction (DebuggerActions::GetClearData (from, to, GetMode())); } });
        }

        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::SaveSymbolTable
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::SaveSymbolTable (const std::string & table)
{
    HRESULT                hr     = S_OK;
    FileDialogSpec         spec;
    std::filesystem::path  chosen;
    bool                   picked = false;



    BAIL_OUT_IF (m_host == nullptr, S_OK);

    spec.filters          = { { L"Symbol files", L"*.sym" }, { L"All files", L"*.*" } };
    spec.defaultExtension = L"sym";
    spec.defaultFileName  = TextEncoding::NarrowToWide (table) + L".sym";

    hr = m_host->GetHostDialogs().PickFileToSave (GetHwnd(), spec, chosen, picked);
    CHR (hr);

    BAIL_OUT_IF (!picked, S_OK);

    RunAction (DebuggerActions::GetSaveSymbols (SymbolTable::ToUpper (table), TextEncoding::WideToNarrow (chosen.wstring()), GetMode()));

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::SaveProfile
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::SaveProfile()
{
    HRESULT                hr     = S_OK;
    FileDialogSpec         spec;
    std::filesystem::path  chosen;
    bool                   picked = false;



    BAIL_OUT_IF (m_host == nullptr, S_OK);

    spec.filters          = { { L"Text files", L"*.txt" }, { L"All files", L"*.*" } };
    spec.defaultExtension = L"txt";
    spec.defaultFileName  = L"Profile.txt";

    hr = m_host->GetHostDialogs().PickFileToSave (GetHwnd(), spec, chosen, picked);
    CHR (hr);

    BAIL_OUT_IF (!picked, S_OK);

    RunAction (DebuggerActions::GetProfile (std::format ("SAVE \"{}\"", TextEncoding::WideToNarrow (chosen.wstring())), GetMode()));

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::SetProfilerBarEntries
//
//  Profile turns profiling on and off, Reset starts the counts over, Save...
//  writes them to a text file, and the view drop-down chooses opcodes,
//  addresses, routines, the call tree or the flame graph.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::SetProfilerBarEntries()
{
    std::vector<DxuiToolbar::Entry>  entries;
    auto                             add = [this, &entries] (int id, const wchar_t * label, const wchar_t * tip, DxuiToolbar::Kind kind, int group, std::function<void()> dispatch)
    {
        std::shared_ptr<DxuiCommand>  command = std::make_shared<DxuiCommand>();
        DxuiToolbar::Entry            entry;

        command->id       = id;
        command->label    = label;
        command->tip      = tip;
        command->dispatch = std::move (dispatch);

        entry.command = command;
        entry.kind    = kind;
        entry.group   = group;

        m_profilerCommands.push_back (command);
        entries.push_back (std::move (entry));

        return command;
    };
    std::shared_ptr<DxuiCommand>     command;



    m_profilerCommands.clear();

    command            = add (s_kProfileOn, L"Profile", L"Count each instruction and its cycles as the machine runs", DxuiToolbar::Kind::Toggle, 0, [this]
    {
        RunAction (DebuggerActions::GetProfile (m_profilerPane->IsOn() ? "OFF" : "ON", GetMode()));
    });
    command->isChecked = [this] { return m_profilerPane->IsOn(); };

    add (s_kProfileReset, L"Reset",   L"Start the counts over",            DxuiToolbar::Kind::Command, 0, [this] { RunAction (DebuggerActions::GetProfile ("RESET", GetMode())); });
    add (s_kProfileSave,  L"Save...", L"Save the profile to a text file", DxuiToolbar::Kind::Command, 0, [this] { SaveProfile(); });

    command            = add (s_kProfileView, L"Opcodes", L"Choose the profile's view: opcodes, addresses, routines, the call tree or a flame graph", DxuiToolbar::Kind::DropDown, 1, [] {});
    command->labelText = [this] { return std::wstring (ProfilerPane::GetViewLabel (m_profilerPane->GetView())); };

    m_profilerBar->SetEntries (std::move (entries));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::SetProfilerBarMenu
//
//  Built again before the drop-down opens, since the rows have the checks
//  they were built with.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::SetProfilerBarMenu()
{
    std::vector<DxuiPopupMenuItem>  views;



    for (int view = 0; view < ProfilerPane::kViewCount; view++)
    {
        views.push_back (DxuiPopupMenuItem::ForCommand (MakeMenuCommand (ProfilerPane::GetViewLabel ((ProfilerPane::View) view),
                                                                         (int) m_profilerPane->GetView() == view, [this, view] { SetProfilerView (view); })));
    }

    m_profilerBar->SetDropDownItems (s_kProfileView, std::move (views));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::PlaceProfilerBar
//
//  The bar sits in the place at the top of its pane and goes with the pane
//  into a floating window, as the breakpoints pane's does.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::PlaceProfilerBar()
{
    bool          shown = m_profilerBar != nullptr && m_profilerSlot != nullptr && m_profilerSlot->IsVisible();
    DxuiWindow  * host  = nullptr;
    RECT          slot  = {};



    if (m_profilerBar == nullptr)
    {
        return;
    }

    m_profilerBar->SetVisible (shown);

    if (!shown)
    {
        return;
    }

    host = GetPaneHost (DebuggerLayout::kProfiler);
    slot = m_profilerSlot->GetBounds();

    m_profilerBar->SetTextRenderer   (host->GetTextRenderer());
    m_profilerBar->SetPopupHost      (host->GetPopupHost());
    m_profilerBar->SetHostClientRect (host->GetBounds());
    m_profilerBar->Layout            (GetBarStrip (DebuggerLayout::kProfiler, slot), m_scaler);

    host->SetChildClip (m_profilerBar, slot);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RouteProfilerBarMouse
//
//  As the breakpoints pane's bar: the strip and whatever menu it has open take
//  the left button.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::RouteProfilerBarMouse (const DxuiMouseEvent & ev)
{
    int   x    = ev.positionDip.x;
    int   y    = ev.positionDip.y;
    bool  open = m_profilerBar->IsMenuOpen();
    bool  over = m_profilerBar->IsVisible() && DxuiDockSite::Contains (m_profilerBar->GetBounds(), POINT { x, y });



    if (!over && !open)
    {
        m_profilerBar->OnToolbarMouseLeave();
        return false;
    }

    if ((ev.kind == DxuiMouseEventKind::Down || ev.kind == DxuiMouseEventKind::Up) && ev.button != DxuiMouseButton::Left && !open)
    {
        return false;
    }

    switch (ev.kind)
    {
    case DxuiMouseEventKind::Move:
        UpdateTooltip (ev.positionDip);
        return m_profilerBar->OnToolbarMouseMove (x, y);

    case DxuiMouseEventKind::Down:
        if (!open)
        {
            SetProfilerBarMenu();
        }

        return m_profilerBar->OnToolbarLButtonDown (x, y);

    case DxuiMouseEventKind::Up:
        return m_profilerBar->OnToolbarLButtonUp (x, y);

    default:
        return open;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RunStepFiltersDialog
//
////////////////////////////////////////////////////////////////////////////////

std::optional<std::vector<std::wstring>> DebuggerWindow::RunStepFiltersDialog (const std::vector<std::wstring> & filters,
                                                                               std::function<bool (const std::wstring &, std::wstring &)> check)
{
    return StepFiltersDialog::RunModal (GetHwnd(), m_theme, filters, std::move (check));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::EditStepFilters
//
//  Debug > Step filters... (FR-184): the dialog's list against the one it
//  opened with, each filter taken out run as SKIP - and each new one as
//  SKIP. A name must be a symbol, and the filter is added by that name.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::EditStepFilters()
{
    std::vector<std::wstring>                  before;
    std::optional<std::vector<std::wstring>>   after;
    std::vector<DebuggerAction>                actions;
    auto                                       findSymbol = [this] (const std::wstring & name) -> std::optional<Word>
    {
        std::string  narrow = TextEncoding::WideToNarrow (name);



        if (m_snapshot == nullptr || m_snapshot->symbols == nullptr)
        {
            return std::nullopt;
        }

        for (const SymbolInfo & symbol : m_snapshot->symbols->symbols)
        {
            if (_stricmp (symbol.name.c_str(), narrow.c_str()) == 0)
            {
                return symbol.address;
            }
        }

        return std::nullopt;
    };



    if (m_snapshot == nullptr)
    {
        return;
    }

    for (const StepFilterEntry & entry : m_snapshot->stepFilters)
    {
        before.push_back (TextEncoding::NarrowToWide (DebuggerActions::GetRemoveSkip (entry, GetMode()).command.text));
    }

    after = RunStepFiltersDialog (before, [&findSymbol] (const std::wstring & text, std::wstring & error)
    {
        bool  isSymbol = findSymbol (text).has_value();



        error = isSymbol ? std::wstring() : std::format (L"{} is not a symbol, an address or a range.", text);
        return isSymbol;
    });

    if (!after.has_value())
    {
        return;
    }

    for (size_t i = 0; i < m_snapshot->stepFilters.size(); i++)
    {
        if (std::ranges::find (*after, before[i]) == after->end())
        {
            actions.push_back (DebuggerActions::GetRemoveSkip (m_snapshot->stepFilters[i], GetMode()));
        }
    }

    for (const std::wstring & text : *after)
    {
        Word                 first   = 0;
        Word                 last    = 0;
        std::optional<Word>  address;



        if (std::ranges::find (before, text) != before.end())
        {
            continue;
        }

        if (StepFiltersDialog::TryParseRange (text, first, last))
        {
            actions.push_back (DebuggerActions::GetSkipRange (first, last, GetMode()));
        }
        else if (address = findSymbol (text); address.has_value())
        {
            actions.push_back (DebuggerActions::GetSkipRoutine (TextEncoding::WideToNarrow (text), *address, GetMode()));
        }
    }

    for (const DebuggerAction & action : actions)
    {
        RunAction (action);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetCursorAddress
//
//  The selected disassembly line's address, or the PC's.
//
////////////////////////////////////////////////////////////////////////////////

Word DebuggerWindow::GetCursorAddress() const
{
    int  line = GetCodeLineOfRow (m_activeCode, m_codeLists[(size_t) m_activeCode]->GetSelectedRow());



    if (line >= 0)
    {
        return GetCodeLines (m_activeCode)[(size_t) line].address;
    }

    return (m_snapshot != nullptr) ? m_snapshot->pc : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ToggleBookmarkAtCursor
//
//  Ctrl+K, K (FR-189): a bookmark on the selected line is removed, and one
//  is added where there is none.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ToggleBookmarkAtCursor()
{
    Word  at       = GetCursorAddress();
    auto  bookmark = std::ranges::find_if (m_snapshot->bookmarks, [at] (const DebuggerViewSnapshot::WatchLine & each) { return each.address == at; });



    RunAction (bookmark != m_snapshot->bookmarks.end() ? DebuggerActions::GetClearBookmark (bookmark->id, GetMode())
                                                       : DebuggerActions::GetAddBookmark (at, GetMode()));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GoToBookmark
//
//  Ctrl+K, N and Ctrl+K, P: the next bookmark after the selected line, or
//  the one before it, round from the last to the first.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::GoToBookmark (bool isBackward)
{
    std::optional<Word>  next = BookmarksPane::GetNext (m_snapshot->bookmarks, GetCursorAddress(), isBackward);



    if (next.has_value())
    {
        ShowCode (*next);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::SetProfilerView
//
//  The flame graph and the list take turns in the pane, so it is laid out
//  again.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::SetProfilerView (int view)
{
    m_profilerPane->SetView ((ProfilerPane::View) view);

    if (GetToolFrame (DebuggerLayout::kProfiler) != nullptr)
    {
        GetToolFrame (DebuggerLayout::kProfiler)->Relayout();
    }

    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RouteFlameGraphMouse
//
//  The flame graph takes a click on a bar, to zoom or to go to its routine.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::RouteFlameGraphMouse (const DxuiMouseEvent & ev)
{
    bool  isUsed = false;



    if (!IsRoutable (m_flameGraph) || !m_flameGraph->IsVisible() || !DxuiDockSite::Contains (m_flameGraph->GetBounds(), ev.positionDip))
    {
        return false;
    }

    isUsed = m_flameGraph->OnMouse (ev);

    if (isUsed)
    {
        Invalidate();
    }

    return isUsed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::TryGetProfilerTip
//
//  Over the timeline, the frame there; over the flame graph, the routine.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::TryGetProfilerTip (POINT clientPx, std::wstring & text) const
{
    if (IsRoutable (m_profileTimeline) && m_profileTimeline->TryGetTipAt (clientPx, text))
    {
        return true;
    }

    return IsRoutable (m_flameGraph) && m_flameGraph->TryGetTipAt (clientPx, text);
}
