#include "Pch.h"

#include "Core/TextEncoding.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerWindow.h"
#include "Ui/Debugger/HeatMapBarCommands.h"





//  The rows a menu on a byte or a heat map cell gets for its last accesses.
static constexpr std::pair<const wchar_t *, HeatMapView::PickAction>  s_kAccessRows[] =
{
    { L"Show last writer", HeatMapView::PickAction::ShowWriter    },
    { L"Show last reader", HeatMapView::PickAction::ShowReader    },
    { L"Go to last write", HeatMapView::PickAction::RewindToWrite },
    { L"Go to last read",  HeatMapView::PickAction::RewindToRead  },
};





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::SetHeatMapOptions
//
//  A choice made on the heat map's bar: shown at once, kept, and sent on to
//  the machine, with the spans of the set whose reads before written are
//  left out. The drop-downs' rows are built again, since each holds the
//  check it was built with, and the bar's entries when the map starts or
//  stops counting totals, which Reset counts shows only for.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::SetHeatMapOptions (const HeatMapOptions & options)
{
    bool  wasCumulative = m_heatMapView->GetOptions().cumulative;



    m_heatMapView->SetOptions (options);

    if (options.cumulative != wasCumulative)
    {
        m_heatMapBar->SetEntries (m_heatMapCommands->BuildEntries (options.cumulative));
        PlaceHeatMapBar();
    }

    if (m_host != nullptr)
    {
        m_host->SetDebuggerHeatMapOptions (options.ToText());
    }

    SendHeatMapIgnore();
    SetHeatMapBarMenus();
    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::SetHeatMapBarMenus
//
//  How the map counts: fading, at each of the fade times, then cumulative,
//  the one in force checked. Which accesses it shows, and the banks the
//  machine has, each with the one shown checked.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::SetHeatMapBarMenus()
{
    std::vector<DxuiPopupMenuItem>  items;
    HeatMapOptions                  shown   = m_heatMapView->GetOptions();
    std::shared_ptr<DxuiCommand>    command;



    m_heatMapFadeCommands.clear();

    for (int seconds : HeatMapOptions::kFadeChoices)
    {
        command = MakeMenuCommand (HeatMapBarCommands::GetFadeLabel (seconds), !shown.cumulative && seconds == shown.fadeSeconds, [this, seconds]
        {
            HeatMapOptions  options = m_heatMapView->GetOptions();



            options.cumulative  = false;
            options.fadeSeconds = seconds;
            SetHeatMapOptions (options);
        });

        m_heatMapFadeCommands.push_back (command);
        items.push_back (DxuiPopupMenuItem::ForCommand (command));
    }

    command = MakeMenuCommand (L"Cumulative", shown.cumulative, [this]
    {
        HeatMapOptions  options = m_heatMapView->GetOptions();



        options.cumulative = true;
        SetHeatMapOptions (options);
    });

    m_heatMapFadeCommands.push_back (command);
    items.push_back (DxuiPopupMenuItem::ForSeparator());
    items.push_back (DxuiPopupMenuItem::ForCommand (command));

    m_heatMapBar->SetDropDownItems (HeatMapBarCommands::kMode, std::move (items));

    items.clear();
    m_heatViewCommands.clear();

    for (int index = 0; index < HeatMapView::kModeCount; index++)
    {
        HeatMapView::Mode  mode = (HeatMapView::Mode) index;

        command = MakeMenuCommand (HeatMapView::GetModeLabel (mode), mode == shown.view, [this, mode]
        {
            HeatMapOptions  options = m_heatMapView->GetOptions();



            options.view = mode;
            SetHeatMapOptions (options);
        });

        m_heatViewCommands.push_back (command);
        items.push_back (DxuiPopupMenuItem::ForCommand (command));
    }

    m_heatViewBar->SetDropDownItems (HeatMapBarCommands::kView, std::move (items));

    items.clear();
    m_heatMapBankCommands.clear();

    for (HeatMapOptions::Bank bank : GetHeatMapBanks())
    {
        std::shared_ptr<DxuiCommand>  command = MakeMenuCommand (HeatMapOptions::GetBankLabel (bank), bank == GetShownHeatMapBank(), [this, bank]
        {
            HeatMapOptions  options = m_heatMapView->GetOptions();



            options.bank = bank;
            SetHeatMapOptions (options);
        });

        m_heatMapBankCommands.push_back (command);
        items.push_back (DxuiPopupMenuItem::ForCommand (command));
    }

    m_heatViewBar->SetDropDownItems (HeatMapBarCommands::kBank, std::move (items));

    SetHeatIgnoreMenu();
    SetHeatRangeMenus();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::SetHeatIgnoreMenu
//
//  None and then every set of ranges, the one left out checked.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::SetHeatIgnoreMenu()
{
    std::vector<DxuiPopupMenuItem>  items;
    std::string                     current = m_heatMapView->GetOptions().ignoreSet;
    std::vector<std::string>        names   = { std::string() };



    m_heatIgnoreCommands.clear();

    for (const HeatMapRangeSet & set : m_heatRanges.sets)
    {
        names.push_back (set.name);
    }

    for (const std::string & name : names)
    {
        std::wstring                  label   = name.empty() ? std::wstring (L"None") : TextEncoding::Utf8ToWide (name);
        std::shared_ptr<DxuiCommand>  command = MakeMenuCommand (label, name == current, [this, name]
        {
            HeatMapOptions  options = m_heatMapView->GetOptions();



            options.ignoreSet = name;
            SetHeatMapOptions (options);
        });

        m_heatIgnoreCommands.push_back (command);
        items.push_back (DxuiPopupMenuItem::ForCommand (command));
    }

    m_heatMapBar->SetDropDownItems (HeatMapBarCommands::kIgnoreSet, std::move (items));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::SendHeatMapIgnore
//
//  The spans of the set whose reads before written are left out, as they
//  read against the symbols now, sent on to the machine when they change.
//  A set no longer there leaves out nothing.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::SendHeatMapIgnore()
{
    std::vector<std::pair<Word, Word>>  spans;
    std::string                         words;



    for (const HeatMapRangeSets::Span & span : m_heatRanges.GetSpans (m_heatMapView->GetOptions().ignoreSet, GetHeatRangeSymbols()))
    {
        spans.emplace_back (span.first, span.last);
    }

    words = HeatMapRangeSets::FormatSpanWords (spans);

    if (m_host == nullptr || words == m_heatIgnoreWords)
    {
        return;
    }

    m_heatIgnoreWords = words;
    m_host->SendDebuggerHeatMapRequest ("ignore " + words);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetHeatMapBanks
//
//  The banks the machine has, as the last snapshot gave them; the CPU's
//  alone before one has.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<HeatMapOptions::Bank> DebuggerWindow::GetHeatMapBanks() const
{
    if (m_snapshot == nullptr || m_snapshot->heatMap.banks.empty())
    {
        return { HeatMapOptions::Bank::Cpu };
    }

    return m_snapshot->heatMap.banks;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetShownHeatMapBank
//
//  The bank chosen while the machine has it, and the CPU's otherwise.
//
////////////////////////////////////////////////////////////////////////////////

HeatMapOptions::Bank DebuggerWindow::GetShownHeatMapBank() const
{
    std::vector<HeatMapOptions::Bank>  banks  = GetHeatMapBanks();
    HeatMapOptions::Bank               chosen = m_heatMapView->GetOptions().bank;



    return (std::ranges::find (banks, chosen) != banks.end()) ? chosen : HeatMapOptions::Bank::Cpu;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::PlaceHeatMapBar
//
//  The bar sits in the place held at the top of the heat map pane, and the
//  view row's strip in the map's row of views, beside the tabs. Both are the
//  pane's controls, so they go with the pane into a floating window and
//  draw, measure and open their menus there.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::PlaceHeatMapBar()
{
    bool          shown = m_heatMapBarSlot != nullptr && m_heatMapBarSlot->IsVisible();
    DxuiWindow  * host  = GetPaneHost (DebuggerLayout::kHeatMap);
    RECT          slot  = {};
    RECT          row   = {};



    m_heatMapBar->SetVisible (shown);

    if (shown)
    {
        slot = m_heatMapBarSlot->GetBounds();

        m_heatMapBar->SetTextRenderer   (host->GetTextRenderer());
        m_heatMapBar->SetPopupHost      (host->GetPopupHost());
        m_heatMapBar->SetHostClientRect (host->GetBounds());
        m_heatMapBar->Layout            (GetBarStrip (DebuggerLayout::kHeatMap, slot), m_scaler);

        host->SetChildClip (m_heatMapBar, slot);
    }

    row   = m_heatMapView->GetViewRowFreeRect();
    shown = shown && m_heatMapView->IsVisible() && row.right > row.left;

    m_heatViewBar->SetVisible (shown);

    if (!shown)
    {
        return;
    }

    m_heatViewBar->SetTextRenderer   (host->GetTextRenderer());
    m_heatViewBar->SetPopupHost      (host->GetPopupHost());
    m_heatViewBar->SetHostClientRect (host->GetBounds());
    m_heatViewBar->Layout            (row, m_scaler);

    host->SetChildClip (m_heatViewBar, row);

    //  The note that the heat is being rebuilt goes in the room the strip
    //  leaves between its two ends.
    m_heatMapView->SetNoteRect (m_heatViewBar->GetFreeRect());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RouteHeatMapBarMouse
//
//  As the breakpoints pane's: each strip and whatever menu it has open take
//  the left button.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::RouteHeatMapBarMouse (const DxuiMouseEvent & ev)
{
    return RouteHeatStripMouse (m_heatViewBar, ev) || RouteHeatStripMouse (m_heatMapBar, ev);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RouteHeatStripMouse
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::RouteHeatStripMouse (DxuiToolbar * bar, const DxuiMouseEvent & ev)
{
    int   x     = ev.positionDip.x;
    int   y     = ev.positionDip.y;
    RECT  strip = bar->GetBounds();
    bool  open  = bar->IsMenuOpen();
    bool  over  = bar->IsVisible() && DxuiDockSite::Contains (strip, POINT { x, y });



    if (!over && !open)
    {
        bar->OnToolbarMouseLeave();
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

        return bar->OnToolbarMouseMove (x, y);

    case DxuiMouseEventKind::Down:
        if (!open)
        {
            SetHeatMapBarMenus();
        }

        return bar->OnToolbarLButtonDown (x, y);

    case DxuiMouseEventKind::Up:
        return bar->OnToolbarLButtonUp (x, y);

    default:
        return open;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::IsHeatMapBarEnabled
//
//  There is a bank to choose only on a machine with aux RAM or a language
//  card.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::IsHeatMapBarEnabled (int id) const
{
    constexpr size_t  kBanksWithoutBanking = 3;        // the CPU's, main RAM and ROM



    switch (id)
    {
    case HeatMapBarCommands::kBank:        return GetHeatMapBanks().size() > kBanksWithoutBanking;
    default:                               return true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::IsHeatMapBarChecked
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::IsHeatMapBarChecked (int id) const
{
    switch (id)
    {
    case HeatMapBarCommands::kBlend:       return m_heatMapView->GetOptions().blend;
    case HeatMapBarCommands::kIgnoreSame:  return m_heatMapView->GetOptions().ignoreSameWrites;
    default:                               return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetHeatMapBarLabel
//
//  The mode drop-down reads how the map counts, with the fade time while
//  fading; the bank drop-down the bank shown and the ranges' the set shown;
//  the rest keep their labels.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DebuggerWindow::GetHeatMapBarLabel (int id) const
{
    if (id == HeatMapBarCommands::kMode)
    {
        return m_heatMapView->GetOptions().cumulative ? std::wstring (L"Cumulative")
                                                      : HeatMapBarCommands::GetFadeLabel (m_heatMapView->GetOptions().fadeSeconds);
    }

    if (id == HeatMapBarCommands::kBank)
    {
        return HeatMapBarCommands::GetBankEntryLabel (GetShownHeatMapBank());
    }

    if (id == HeatMapBarCommands::kView)
    {
        return HeatMapView::GetModeLabel (m_heatMapView->GetOptions().view);
    }

    if (id == HeatMapBarCommands::kIgnoreSet)
    {
        return HeatMapBarCommands::GetIgnoreSetLabel (TextEncoding::Utf8ToWide (m_heatMapView->GetOptions().ignoreSet));
    }

    if (id == HeatMapBarCommands::kRangeSet)
    {
        return TextEncoding::Utf8ToWide (m_heatRanges.shown.empty() ? std::string (HeatMapRangeSets::kpszAllMemory) : m_heatRanges.shown);
    }

    return {};
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RunHeatMapBarEntry
//
//  How the map counts is chosen from its drop-down's rows, not here.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::RunHeatMapBarEntry (int id)
{
    HeatMapOptions  options = m_heatMapView->GetOptions();



    switch (id)
    {
    case HeatMapBarCommands::kBlend:
        options.blend = !options.blend;
        SetHeatMapOptions (options);
        break;

    case HeatMapBarCommands::kIgnoreSame:
        options.ignoreSameWrites = !options.ignoreSameWrites;
        SetHeatMapOptions (options);
        break;

    case HeatMapBarCommands::kResetCounts:
        if (m_host != nullptr)
        {
            m_host->ResetDebuggerHeatMap();
        }

        break;

    default:
        break;
    }

    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::TryGetHeatMapTip
//
//  Over the heat map's map, the tip the view gives for the cell the mouse
//  picks.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::TryGetHeatMapTip (POINT clientPx, RECT & anchor, std::wstring & text) const
{
    return IsRoutable (m_heatMapView) && m_heatMapView->IsVisible() && DxuiDockSite::Contains (m_heatMapView->GetBounds(), clientPx) &&
           m_heatMapView->TryGetTipAt (clientPx, anchor, text);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RequestHeatMapAccess
//
//  A Ctrl+click on a cell, or a row of a menu on one: the machine looks up
//  the access in the bank given and shows its instruction or goes back to
//  it, and says so in the console when it cannot.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::RequestHeatMapAccess (
    Word                      address,
    HeatMapView::PickAction   action,
    HeatMapOptions::Bank      bank)
{
    HeatAccessRequest  request;



    if (m_host == nullptr || action == HeatMapView::PickAction::ShowMemory)
    {
        return;
    }

    request.isRewind = action == HeatMapView::PickAction::RewindToWrite || action == HeatMapView::PickAction::RewindToRead;
    request.isWrite  = action == HeatMapView::PickAction::RewindToWrite || action == HeatMapView::PickAction::ShowWriter;
    request.bank     = bank;
    request.address  = address;

    m_host->SendDebuggerHeatMapRequest (HeatAccessJump::FormatWords (request));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::AddHeatMapAccessItems
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::AddHeatMapAccessItems (
    Word                                                           address,
    HeatMapOptions::Bank                                           bank,
    std::vector<std::pair<std::wstring, std::function<void()>>>  & items)
{
    for (const auto & [label, action] : s_kAccessRows)
    {
        items.push_back ({ label, [this, address, bank, action = action] { RequestHeatMapAccess (address, action, bank); } });
    }
}





