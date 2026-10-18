#include "Pch.h"

#include "Ui/Debugger/Panes/HeatMapPane.h"

#include "Core/TextEncoding.h"
#include "Core/UnicodeSymbols.h"
#include "Debugger/HeatAccessJump.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerMenuCommand.h"
#include "Ui/Debugger/HeatMapBarCommands.h"
#include "Ui/Debugger/Panes/DebuggerPaneFrame.h"





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
//  HeatMapPane::HeatMapPane
//
////////////////////////////////////////////////////////////////////////////////

HeatMapPane::HeatMapPane (IDebuggerPaneHost & host, Ranges ranges) :
    IDebuggerPane (host),
    m_ranges      (std::move (ranges)),
    m_bar         (host, DebuggerLayout::kHeatMap, L"Heat map commands")
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapPane::~HeatMapPane
//
////////////////////////////////////////////////////////////////////////////////

HeatMapPane::~HeatMapPane() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapPane::GetId
//
////////////////////////////////////////////////////////////////////////////////

std::wstring HeatMapPane::GetId() const
{
    return DebuggerLayout::kHeatMap;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapPane::CreateControls
//
//  The map, then its bar and the strip beside its tabs, which paint over it.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapPane::CreateControls()
{
    m_view    = m_host.GetControlParent().CreateChild<HeatMapView>();
    m_bar.Create();
    m_viewBar = m_host.GetControlParent().CreateChild<DxuiToolbar>();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapPane::Configure
//
//  The options the user last left; each change is saved and sent on to the
//  machine as it is made.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapPane::Configure()
{
    IDebuggerWindowHost         * windowHost = m_host.GetWindowHost();
    HeatMapOptions                options    = HeatMapOptions::FromText ((windowHost != nullptr) ? windowHost->GetDebuggerHeatMapOptions() : std::string());
    HeatMapBarCommands::Handlers  handlers;



    m_view->SetOptions (options);

    m_view->SetOnOptionsChanged ([this]
    {
        SaveOptions (m_view->GetOptions());
        m_host.Repaint();
    });

    handlers.dispatch  = [this] (int id) { RunBarEntry (id); };
    handlers.isEnabled = [this] (int id) { return IsBarEnabled (id); };
    handlers.isChecked = [this] (int id) { return IsBarChecked (id); };
    handlers.getLabel  = [this] (int id) { return GetBarLabel (id); };

    m_commands = std::make_unique<HeatMapBarCommands> (std::move (handlers));

    m_bar.Configure();
    m_bar.SetOnPress ([this] { SetBarMenus(); });
    m_bar.GetBar()->SetEntries (m_commands->BuildEntries (options.cumulative));

    //  The set of ranges and the bank, beside the map's tabs, as tall as
    //  their row.
    m_viewBar->SetTextRenderer (m_host.GetPaneTextRenderer());
    m_viewBar->SetPopupHost    (m_host.GetPanePopupHost());
    m_viewBar->SetIconFace     (DxuiToolbar::kMdl2IconFace);
    m_viewBar->SetCompact      (true);
    m_viewBar->SetBandDp       (HeatMapView::kBarDip);
    m_viewBar->EnableSeeMore   (s_kpszMdl2More, L"See more");
    m_viewBar->SetEntries      (m_commands->BuildViewRowEntries());
    m_viewBar->SetVisible      (false);

    SetBarMenus();

    m_view->SetOnPickAddress ([this] (Word address) { m_host.ShowMemoryAddress (address); });

    m_view->SetOnPickAccess ([this] (Word address, HeatMapView::PickAction action)
    {
        RequestAccess (address, action, m_view->GetShownBank());
    });

    m_view->SetOnHoverChanged ([this] (std::optional<Word> address) { SendHover (address); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapPane::OnDocked
//
//  The options are sent on to the machine at once, since it starts from the
//  defaults. Not before the dock is built: a saved layout with no heat map
//  options predates the heat map, which the dock then opens closed.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapPane::OnDocked()
{
    SaveOptions (m_view->GetOptions());
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapPane::AddToDock
//
//  The bar, a place PlaceBar fills, over the map. The machine records
//  accesses only while the map is visible.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapPane::AddToDock (DxuiDockSite & dock)
{
    m_frame = std::make_unique<DebuggerPaneFrame> (GetTitle());
    m_frame->AddPart (m_bar.GetSlot(), PaneBarSlot::GetHeightPx);
    m_frame->AddPart (m_view);
    m_view->SetVisible (false);

    dock.AddPane (GetId(), GetTitle(), m_frame.get());
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapPane::GetContent
//
////////////////////////////////////////////////////////////////////////////////

IDxuiControl * HeatMapPane::GetContent() const
{
    return m_frame.get();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapPane::GetControls
//
////////////////////////////////////////////////////////////////////////////////

std::vector<IDxuiControl *> HeatMapPane::GetControls() const
{
    return { m_bar.GetSlot(), m_view, m_bar.GetBar(), m_viewBar };
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapPane::GetColorKey
//
////////////////////////////////////////////////////////////////////////////////

std::optional<PaneColorKeySpec> HeatMapPane::GetColorKey() const
{
    return PaneColorKeySpec { m_bar.GetSlot(), ColorLegend::Pane::HeatMap, nullptr };
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapPane::SaveOptions
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapPane::SaveOptions (const HeatMapOptions & options)
{
    IDebuggerWindowHost  * windowHost = m_host.GetWindowHost();



    if (windowHost != nullptr)
    {
        windowHost->SetDebuggerHeatMapOptions (options.ToText());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapPane::SendHover
//
//  The machine is told which cell the mouse is over, for its last writer and
//  reader.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapPane::SendHover (std::optional<Word> address)
{
    IDebuggerWindowHost  * windowHost = m_host.GetWindowHost();



    if (windowHost != nullptr)
    {
        windowHost->SendDebuggerHeatMapRequest (address.has_value() ? std::format ("hover {:04X}", *address) : std::string ("hover none"));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapPane::OnProjectSwitched
//
//  The other project's options, shown and sent on to the machine.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapPane::OnProjectSwitched()
{
    IDebuggerWindowHost  * windowHost = m_host.GetWindowHost();



    if (windowHost != nullptr)
    {
        m_view->SetOptions (HeatMapOptions::FromText (windowHost->GetDebuggerHeatMapOptions()));
        windowHost->SetDebuggerHeatMapOptions (windowHost->GetDebuggerHeatMapOptions());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapPane::SetOptions
//
//  The drop-downs' rows are built again, since each has the check it was
//  built with, and the bar's entries when the map starts or stops counting
//  totals, which Reset counts is shown only for.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapPane::SetOptions (const HeatMapOptions & options)
{
    bool  wasCumulative = m_view->GetOptions().cumulative;



    m_view->SetOptions (options);

    if (options.cumulative != wasCumulative)
    {
        m_bar.GetBar()->SetEntries (m_commands->BuildEntries (options.cumulative));
        PlaceBar();
    }

    SaveOptions (options);
    SendIgnore();
    SetBarMenus();
    m_host.Repaint();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapPane::SetBarMenus
//
//  How the map counts: fading, at each of the fade times, then cumulative,
//  the one in force checked. Which accesses it shows, and the banks the
//  machine has, each with the one shown checked.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapPane::SetBarMenus()
{
    std::vector<DxuiPopupMenuItem>  items;
    HeatMapOptions                  shown   = m_view->GetOptions();
    std::shared_ptr<DxuiCommand>    command;



    m_fadeCommands.clear();

    for (int seconds : HeatMapOptions::kFadeChoices)
    {
        command = DebuggerMenuCommand::Make (HeatMapBarCommands::GetFadeLabel (seconds), !shown.cumulative && seconds == shown.fadeSeconds, [this, seconds]
        {
            HeatMapOptions  options = m_view->GetOptions();



            options.cumulative  = false;
            options.fadeSeconds = seconds;
            SetOptions (options);
        });

        m_fadeCommands.push_back (command);
        items.push_back (DxuiPopupMenuItem::ForCommand (command));
    }

    command = DebuggerMenuCommand::Make (L"Cumulative", shown.cumulative, [this]
    {
        HeatMapOptions  options = m_view->GetOptions();



        options.cumulative = true;
        SetOptions (options);
    });

    m_fadeCommands.push_back (command);
    items.push_back (DxuiPopupMenuItem::ForSeparator());
    items.push_back (DxuiPopupMenuItem::ForCommand (command));

    m_bar.GetBar()->SetDropDownItems (HeatMapBarCommands::kMode, std::move (items));

    items.clear();
    m_viewCommands.clear();

    for (int index = 0; index < HeatMapView::kModeCount; index++)
    {
        HeatMapView::Mode  mode = (HeatMapView::Mode) index;

        command = DebuggerMenuCommand::Make (HeatMapView::GetModeLabel (mode), mode == shown.view, [this, mode]
        {
            HeatMapOptions  options = m_view->GetOptions();



            options.view = mode;
            SetOptions (options);
        });

        m_viewCommands.push_back (command);
        items.push_back (DxuiPopupMenuItem::ForCommand (command));
    }

    m_viewBar->SetDropDownItems (HeatMapBarCommands::kView, std::move (items));

    items.clear();
    m_bankCommands.clear();

    for (HeatMapOptions::Bank bank : GetBanks())
    {
        command = DebuggerMenuCommand::Make (HeatMapOptions::GetBankLabel (bank), bank == GetShownBank(), [this, bank]
        {
            HeatMapOptions  options = m_view->GetOptions();



            options.bank = bank;
            SetOptions (options);
        });

        m_bankCommands.push_back (command);
        items.push_back (DxuiPopupMenuItem::ForCommand (command));
    }

    m_viewBar->SetDropDownItems (HeatMapBarCommands::kBank, std::move (items));

    SetIgnoreMenu();

    if (m_ranges.buildMenu)
    {
        m_ranges.buildMenu();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapPane::SetIgnoreMenu
//
//  None and then every set of ranges, the one left out checked.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapPane::SetIgnoreMenu()
{
    std::vector<DxuiPopupMenuItem>  items;
    std::string                     current = m_view->GetOptions().ignoreSet;
    std::vector<std::string>        names   = { std::string() };



    m_ignoreCommands.clear();

    for (const HeatMapRangeSet & set : m_ranges.sets().sets)
    {
        names.push_back (set.name);
    }

    for (const std::string & name : names)
    {
        std::wstring                  label   = name.empty() ? std::wstring (L"None") : TextEncoding::Utf8ToWide (name);
        std::shared_ptr<DxuiCommand>  command = DebuggerMenuCommand::Make (label, name == current, [this, name]
        {
            HeatMapOptions  options = m_view->GetOptions();



            options.ignoreSet = name;
            SetOptions (options);
        });

        m_ignoreCommands.push_back (command);
        items.push_back (DxuiPopupMenuItem::ForCommand (command));
    }

    m_bar.GetBar()->SetDropDownItems (HeatMapBarCommands::kIgnoreSet, std::move (items));
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapPane::SendIgnore
//
//  The spans of the set whose reads before written are left out, as they
//  read against the symbols now, sent on to the machine when they change.
//  A set no longer there leaves out nothing.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapPane::SendIgnore()
{
    IDebuggerWindowHost                 * windowHost = m_host.GetWindowHost();
    std::vector<std::pair<Word, Word>>    spans;
    std::string                           words;



    for (const HeatMapRangeSets::Span & span : m_ranges.sets().GetSpans (m_view->GetOptions().ignoreSet, m_ranges.symbols()))
    {
        spans.emplace_back (span.first, span.last);
    }

    words = HeatMapRangeSets::FormatSpanWords (spans);

    if (windowHost == nullptr || words == m_ignoreWords)
    {
        return;
    }

    m_ignoreWords = words;
    windowHost->SendDebuggerHeatMapRequest ("ignore " + words);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapPane::GetBanks
//
//  The banks the machine has, as the last snapshot gave them; the CPU's
//  alone before one has.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<HeatMapOptions::Bank> HeatMapPane::GetBanks() const
{
    if (m_banks.empty())
    {
        return { HeatMapOptions::Bank::Cpu };
    }

    return m_banks;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapPane::GetShownBank
//
//  The bank chosen while the machine has it, and the CPU's otherwise.
//
////////////////////////////////////////////////////////////////////////////////

HeatMapOptions::Bank HeatMapPane::GetShownBank() const
{
    std::vector<HeatMapOptions::Bank>  banks  = GetBanks();
    HeatMapOptions::Bank               chosen = m_view->GetOptions().bank;



    return (std::ranges::find (banks, chosen) != banks.end()) ? chosen : HeatMapOptions::Bank::Cpu;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapPane::PlaceBar
//
//  The bar sits in the place at the top of the pane, and the strip in the
//  map's row of views, beside the tabs. Both are the pane's controls, so
//  they go with the pane into a floating window and draw, measure and open
//  their menus there.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapPane::PlaceBar()
{
    DxuiWindow  * host  = nullptr;
    RECT          row   = {};
    bool          shown = false;



    m_bar.Place();

    row   = m_view->GetViewRowFreeRect();
    shown = m_bar.GetBar()->IsVisible() && m_view->IsVisible() && row.right > row.left;

    m_viewBar->SetVisible (shown);

    if (!shown)
    {
        return;
    }

    host = m_host.GetPaneWindow (GetId());

    m_viewBar->SetTextRenderer   (host->GetTextRenderer());
    m_viewBar->SetPopupHost      (host->GetPopupHost());
    m_viewBar->SetHostClientRect (host->GetBounds());
    m_viewBar->Layout            (row, m_host.GetPaneScaler());

    host->SetChildClip (m_viewBar, row);

    //  The note that the heat is being rebuilt goes in the room the strip
    //  leaves between its two ends.
    m_view->SetNoteRect (m_viewBar->GetFreeRect());
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapPane::RouteBarMouse
//
//  The strip and the bar, each with whatever menu it has open.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapPane::RouteBarMouse (const DxuiMouseEvent & ev)
{
    return PaneBarSlot::RouteToolbarMouse (m_host, GetId(), m_viewBar, ev, [this] { SetBarMenus(); }) || m_bar.RouteMouse (ev);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapPane::GetBarTip
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * HeatMapPane::GetBarTip (POINT clientPx, RECT & anchor) const
{
    const wchar_t  * tip = m_bar.GetTip (clientPx, anchor);



    if (tip == nullptr || *tip == L'\0')
    {
        tip = PaneBarSlot::GetToolbarTip (m_host, GetId(), m_viewBar, clientPx, anchor);
    }

    return tip;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapPane::IsBarEnabled
//
//  There is a bank to choose only on a machine with aux RAM or a language
//  card.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapPane::IsBarEnabled (int id) const
{
    constexpr size_t  kBanksWithoutBanking = 3;        // the CPU's, main RAM and ROM



    switch (id)
    {
    case HeatMapBarCommands::kBank:        return GetBanks().size() > kBanksWithoutBanking;
    default:                               return true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapPane::IsBarChecked
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapPane::IsBarChecked (int id) const
{
    switch (id)
    {
    case HeatMapBarCommands::kBlend:       return m_view->GetOptions().blend;
    case HeatMapBarCommands::kIgnoreSame:  return m_view->GetOptions().ignoreSameWrites;
    default:                               return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapPane::GetBarLabel
//
//  The mode drop-down reads how the map counts, with the fade time while
//  fading; the bank drop-down the bank shown and the ranges' the set shown;
//  the rest have their own labels.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring HeatMapPane::GetBarLabel (int id) const
{
    if (id == HeatMapBarCommands::kMode)
    {
        return m_view->GetOptions().cumulative ? std::wstring (L"Cumulative")
                                               : HeatMapBarCommands::GetFadeLabel (m_view->GetOptions().fadeSeconds);
    }

    if (id == HeatMapBarCommands::kBank)
    {
        return HeatMapBarCommands::GetBankEntryLabel (GetShownBank());
    }

    if (id == HeatMapBarCommands::kView)
    {
        return HeatMapView::GetModeLabel (m_view->GetOptions().view);
    }

    if (id == HeatMapBarCommands::kIgnoreSet)
    {
        return HeatMapBarCommands::GetIgnoreSetLabel (TextEncoding::Utf8ToWide (m_view->GetOptions().ignoreSet));
    }

    if (id == HeatMapBarCommands::kRangeSet)
    {
        return TextEncoding::Utf8ToWide (m_ranges.sets().shown.empty() ? std::string (HeatMapRangeSets::kpszAllMemory) : m_ranges.sets().shown);
    }

    return {};
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapPane::RunBarEntry
//
//  How the map counts is chosen from its drop-down's rows, not here.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapPane::RunBarEntry (int id)
{
    HeatMapOptions         options    = m_view->GetOptions();
    IDebuggerWindowHost  * windowHost = m_host.GetWindowHost();



    switch (id)
    {
    case HeatMapBarCommands::kBlend:
        options.blend = !options.blend;
        SetOptions (options);
        break;

    case HeatMapBarCommands::kIgnoreSame:
        options.ignoreSameWrites = !options.ignoreSameWrites;
        SetOptions (options);
        break;

    case HeatMapBarCommands::kResetCounts:
        if (windowHost != nullptr)
        {
            windowHost->ResetDebuggerHeatMap();
        }

        break;

    default:
        break;
    }

    m_host.Repaint();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapPane::RouteEarlyMouse
//
//  Ctrl+wheel over the map zooms the map rather than the panes' text.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapPane::RouteEarlyMouse (const DxuiMouseEvent & ev)
{
    return ev.kind == DxuiMouseEventKind::Wheel && ev.ctrl && m_host.IsRoutable (m_view) && m_view->IsVisible() &&
           m_view->IsOverMap (ev.positionDip) && RouteContentMouse (ev);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapPane::RouteContentMouse
//
//  The map takes a press on its modes and scrollbars, a click or a drag on
//  its map and the wheel over it, and follows the mouse over its map for the
//  cell it frames; a move anywhere else clears the frame. A drag begun on
//  the map has the mouse until the button comes up, wherever it goes. Only
//  what it acts on is used up.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapPane::RouteContentMouse (const DxuiMouseEvent & ev)
{
    DxuiMouseEvent  leave  = ev;
    bool            isOver = m_host.IsRoutable (m_view) && m_view->IsVisible() &&
                             DxuiDockSite::Contains (m_view->GetBounds(), ev.positionDip);
    bool            isUsed = false;



    if (m_view->IsPressed())
    {
        isOver = true;
    }

    if (!isOver)
    {
        leave.kind = DxuiMouseEventKind::Leave;
        m_view->OnMouse (leave);
        return false;
    }

    isUsed = m_view->OnMouse (ev);

    //  A press on the map focuses it, as a press in any pane focuses that
    //  pane, so the pane takes the accent.
    if (ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Left)
    {
        m_host.FocusControl (m_view);
    }

    //  Painted once the messages waiting are handled, so a wheel spun hard
    //  moves the map by every notch before it is drawn, rather than drawing
    //  each notch while the rest wait.
    if (ev.kind == DxuiMouseEventKind::Move || isUsed)
    {
        m_host.RepaintLater();
    }

    return isUsed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapPane::TryGetTip
//
//  Over the zoom, how to zoom, as a control's tip. Over the map, the cell the
//  mouse picks and what touched it, at once and following the pointer. Until
//  the machine has looked up the cell's last writer and reader, a tip already
//  up stays as it is and none is put up, so a tip never shows one line and
//  then grows; Apply shows it whole once they arrive.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapPane::TryGetTip (POINT clientPx, PaneTip & tip)
{
    bool  isShown = m_host.IsRoutable (m_view) && m_view->IsVisible();



    if (isShown && m_view->TryGetZoomTipAt (clientPx, tip.anchor, tip.text))
    {
        tip.kind = PaneTip::Kind::Control;
        return true;
    }

    m_tipAt.reset();
    m_isTipPending = false;

    if (!isShown || !DxuiDockSite::Contains (m_view->GetBounds(), clientPx) || !m_view->TryGetTipAt (clientPx, tip.anchor, tip.text))
    {
        return false;
    }

    m_tipAt        = clientPx;
    m_isTipPending = !m_view->IsTipComplete (clientPx);
    tip.kind       = m_isTipPending ? PaneTip::Kind::Pending : PaneTip::Kind::Pointer;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapPane::AddContentMenuItems
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapPane::AddContentMenuItems (POINT clientPx, PaneMenuItems & items)
{
    std::optional<Word>  byte;



    if (!m_host.IsRoutable (m_view) || !m_view->IsVisible())
    {
        return;
    }

    byte = m_view->GetPickAt (clientPx);

    if (byte.has_value())
    {
        AddAccessItems (*byte, m_view->GetShownBank(), items);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapPane::RequestAccess
//
//  A Ctrl+click on a cell, or a row of a menu on one: the machine looks up
//  the access in the bank given and shows its instruction or goes back to
//  it, and writes to the console when it cannot.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapPane::RequestAccess (Word address, HeatMapView::PickAction action, HeatMapOptions::Bank bank)
{
    IDebuggerWindowHost  * windowHost = m_host.GetWindowHost();
    HeatAccessRequest      request;



    if (windowHost == nullptr || action == HeatMapView::PickAction::ShowMemory)
    {
        return;
    }

    request.isRewind = action == HeatMapView::PickAction::RewindToWrite || action == HeatMapView::PickAction::RewindToRead;
    request.isWrite  = action == HeatMapView::PickAction::RewindToWrite || action == HeatMapView::PickAction::ShowWriter;
    request.bank     = bank;
    request.address  = address;

    windowHost->SendDebuggerHeatMapRequest (HeatAccessJump::FormatWords (request));
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapPane::AddAccessItems
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapPane::AddAccessItems (Word address, HeatMapOptions::Bank bank, PaneMenuItems & items)
{
    for (const auto & [label, action] : s_kAccessRows)
    {
        items.push_back ({ label, [this, address, bank, action = action] { RequestAccess (address, action, bank); } });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapPane::Tick
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapPane::Tick (int64_t nowMs)
{
    return m_view->TickScrollbars (nowMs);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapPane::Apply
//
//  The map's levels are set only for a reading the machine has not given
//  before; the marks, the symbols and the cell under the mouse each time.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapPane::Apply (const DebuggerViewSnapshot & snapshot)
{
    const DebuggerViewSnapshot::HeatMapState  & state      = snapshot.heatMap;
    std::vector<HeatMapView::Breakpoint>        breakpoints;
    bool                                        isNewHover = state.hover != nullptr;



    for (const DebuggerViewSnapshot::BreakpointLine & line : snapshot.breakpoints)
    {
        std::optional<HeatMapView::BreakKind>  kind = HeatMapView::GetBreakKind (line.info);



        if (kind.has_value())
        {
            breakpoints.push_back ({ line.id, line.info.address, std::max (line.info.address, line.info.last), *kind });
        }
    }

    m_banks = state.banks;

    m_view->SetPalette      (m_host.GetHeatPalette());
    m_view->SetTop          (state.top);
    m_view->SetShownBank    (state.bank, state.hasAux);
    m_view->SetSymbols      (snapshot.heatMapSymbols);
    m_view->SetOpcodeForms  (state.opcodeForms);
    m_view->SetCpuMarks     (state.pc, state.stack);
    m_view->SetBreakpoints  (std::move (breakpoints));
    m_view->SetRebuilding   (state.isRebuilding);

    //  The machine reads its map only now and then while it runs, and the
    //  snapshots between have the same reading, whose cells are already
    //  colored. A state no reading numbered is always new.
    if (state.serial == 0 || state.serial != m_appliedSerial)
    {
        m_appliedSerial = state.serial;

        m_view->SetOpcodes       (state.opcodes);
        m_view->SetValues        (state.values);
        m_view->SetChannelLevels (state.unwritten, state.changed, state.edited);
        m_view->SetLevels        (state.execute, state.read, state.write);
    }

    m_view->SetHoverAccess (state.hover);

    //  The tip over the map lists the last writer and reader as soon as they
    //  come in, without waiting for the mouse to move, and a tip held back
    //  for them shows now.
    if (isNewHover && m_tipAt.has_value() && (m_host.IsTooltipVisible() || m_isTipPending))
    {
        m_host.UpdateTooltip (*m_tipAt);
    }
}
