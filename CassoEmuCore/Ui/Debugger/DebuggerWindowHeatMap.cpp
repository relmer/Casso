#include "Pch.h"

#include "Core/TextEncoding.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerWindow.h"





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
//  left out. The fade drop-down's rows are built again, since each holds the
//  check it was built with.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::SetHeatMapOptions (const HeatMapOptions & options)
{
    m_heatMapView->SetOptions (options);

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
//  The fade times, the one in force checked; and the banks the machine has,
//  the one shown checked.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::SetHeatMapBarMenus()
{
    std::vector<DxuiPopupMenuItem>  items;
    int                             current = m_heatMapView->GetOptions().fadeSeconds;



    m_heatMapFadeCommands.clear();

    for (int seconds : HeatMapOptions::kFadeChoices)
    {
        std::shared_ptr<DxuiCommand>  command = MakeMenuCommand (HeatMapBarCommands::GetFadeChoiceLabel (seconds), seconds == current, [this, seconds]
        {
            HeatMapOptions  options = m_heatMapView->GetOptions();



            options.fadeSeconds = seconds;
            SetHeatMapOptions (options);
        });

        m_heatMapFadeCommands.push_back (command);
        items.push_back (DxuiPopupMenuItem::ForCommand (command));
    }

    m_heatMapBar->SetDropDownItems (HeatMapBarCommands::kFade, std::move (items));

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

    m_heatMapBar->SetDropDownItems (HeatMapBarCommands::kBank, std::move (items));

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
//  The bar sits in the place held at the top of the heat map pane and is one
//  of the pane's controls, so it goes with the pane into a floating window
//  and draws, measures and opens its menu there.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::PlaceHeatMapBar()
{
    bool          shown = m_heatMapBarSlot != nullptr && m_heatMapBarSlot->IsVisible();
    DxuiWindow  * host  = GetPaneHost (DebuggerLayout::kHeatMap);
    RECT          slot  = {};



    m_heatMapBar->SetVisible (shown);

    if (!shown)
    {
        return;
    }

    slot = m_heatMapBarSlot->GetBounds();

    m_heatMapBar->SetTextRenderer   (host->GetTextRenderer());
    m_heatMapBar->SetPopupHost      (host->GetPopupHost());
    m_heatMapBar->SetHostClientRect (host->GetBounds());
    m_heatMapBar->Layout            (ColorKeyButton::GetStripBeside (slot, m_scaler), m_scaler);

    host->SetChildClip (m_heatMapBar, slot);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RouteHeatMapBarMouse
//
//  As the breakpoints pane's: the strip and whatever menu it has open take
//  the left button.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::RouteHeatMapBarMouse (const DxuiMouseEvent & ev)
{
    int   x     = ev.positionDip.x;
    int   y     = ev.positionDip.y;
    RECT  strip = m_heatMapBar->GetBounds();
    bool  open  = m_heatMapBar->IsMenuOpen();
    bool  over  = m_heatMapBar->IsVisible() && DxuiDockSite::Contains (strip, POINT { x, y });



    if (!over && !open)
    {
        m_heatMapBar->OnToolbarMouseLeave();
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

        return m_heatMapBar->OnToolbarMouseMove (x, y);

    case DxuiMouseEventKind::Down:
        if (!open)
        {
            SetHeatMapBarMenus();
        }

        return m_heatMapBar->OnToolbarLButtonDown (x, y);

    case DxuiMouseEventKind::Up:
        return m_heatMapBar->OnToolbarLButtonUp (x, y);

    default:
        return open;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::IsHeatMapBarEnabled
//
//  The fade time counts only while fading, and Reset counts only while
//  cumulative. There is a bank to choose only on a machine with aux RAM or
//  a language card.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::IsHeatMapBarEnabled (int id) const
{
    constexpr size_t  kBanksWithoutBanking = 3;        // the CPU's, main RAM and ROM
    bool              isCumulative         = m_heatMapView->GetOptions().cumulative;



    switch (id)
    {
    case HeatMapBarCommands::kFade:        return !isCumulative;
    case HeatMapBarCommands::kResetCounts: return isCumulative;
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
    bool  isCumulative = m_heatMapView->GetOptions().cumulative;



    switch (id)
    {
    case HeatMapBarCommands::kFading:      return !isCumulative;
    case HeatMapBarCommands::kCumulative:  return isCumulative;
    case HeatMapBarCommands::kBlend:       return m_heatMapView->GetOptions().blend;
    case HeatMapBarCommands::kIgnoreSame:  return m_heatMapView->GetOptions().ignoreSameWrites;
    default:                               return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetHeatMapBarLabel
//
//  The fade drop-down reads the time in force, the bank drop-down the bank
//  shown and the ranges' the set shown; the rest keep their labels.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DebuggerWindow::GetHeatMapBarLabel (int id) const
{
    if (id == HeatMapBarCommands::kFade)
    {
        return HeatMapBarCommands::GetFadeLabel (m_heatMapView->GetOptions().fadeSeconds);
    }

    if (id == HeatMapBarCommands::kBank)
    {
        return HeatMapBarCommands::GetBankEntryLabel (GetShownHeatMapBank());
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
//  The fade time is chosen from its drop-down's rows, not here.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::RunHeatMapBarEntry (int id)
{
    HeatMapOptions  options = m_heatMapView->GetOptions();



    switch (id)
    {
    case HeatMapBarCommands::kFading:
    case HeatMapBarCommands::kCumulative:
        options.cumulative = (id == HeatMapBarCommands::kCumulative);
        SetHeatMapOptions (options);
        break;

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

    case HeatMapBarCommands::kEditRanges: OpenHeatRanges(); break;

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





