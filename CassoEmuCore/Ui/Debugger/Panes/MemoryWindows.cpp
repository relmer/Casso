#include "Pch.h"

#include "Ui/Debugger/Panes/MemoryWindows.h"

#include "Core/TextEncoding.h"
#include "Core/UnicodeSymbols.h"
#include "Debugger/Source/SourcePathList.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerMenuCommand.h"
#include "Ui/Debugger/MemoryAddressEntry.h"
#include "Ui/Debugger/MemoryBarCommands.h"
#include "Ui/Debugger/Panes/DebuggerPaneFrame.h"
#include "Ui/Debugger/Panes/MemoryPane.h"
#include "Ui/Debugger/UndoText.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryWindowPane::MemoryWindowPane
//
////////////////////////////////////////////////////////////////////////////////

MemoryWindowPane::MemoryWindowPane (IDebuggerPaneHost & host, MemoryWindowGroup & group, int id) :
    IDebuggerPane (host),
    m_group       (group),
    m_windowId    (id),
    m_slot        (std::make_unique<DebuggerPaneFrame> (L"Memory commands")),
    m_isOpen      (id == 1)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryWindowPane::~MemoryWindowPane
//
////////////////////////////////////////////////////////////////////////////////

MemoryWindowPane::~MemoryWindowPane() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryWindowPane::GetId
//
////////////////////////////////////////////////////////////////////////////////

std::wstring MemoryWindowPane::GetId() const
{
    return DebuggerLayout::GetMemoryPaneId (m_windowId);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryWindowPane::GetTitle
//
////////////////////////////////////////////////////////////////////////////////

std::wstring MemoryWindowPane::GetTitle() const
{
    return std::format (L"Memory {}", m_windowId);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryWindowPane::CreateControls
//
//  Every window exists from the start; the ones not open are hidden.
//
////////////////////////////////////////////////////////////////////////////////

void MemoryWindowPane::CreateControls()
{
    m_view  = m_host.GetControlParent().CreateChild<DxuiHexView>();
    m_model = std::make_unique<MemoryPane> (
        m_windowId, m_view,
        [this] (int window, Word first)
        {
            if (m_host.GetWindowHost() != nullptr)
            {
                m_host.GetWindowHost()->SetDebuggerMemoryWindow (window, first);
            }
        },
        [this] (const DebuggerActionBuilder & build) { m_host.RunAction (build (m_host.GetMode())); },
        [this] (const std::string & line)            { m_host.AppendConsole ({ line }); });

    m_view->SetVisible (m_isOpen);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryWindowPane::Configure
//
////////////////////////////////////////////////////////////////////////////////

void MemoryWindowPane::Configure()
{
    m_model->Configure (m_host.GetOwnerHwnd());
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryWindowPane::AddToDock
//
//  The window is its command bar over its bytes. The bar is a place at the
//  window's top; the controls, shared by every window, are put in the bar of
//  the one they act on.
//
////////////////////////////////////////////////////////////////////////////////

void MemoryWindowPane::AddToDock (DxuiDockSite & dock)
{
    m_frame = std::make_unique<DebuggerPaneFrame> (GetTitle());
    m_frame->AddPart (m_slot.get(), [] (int, const DxuiDpiScaler & scaler) { return scaler.ToPx (DxuiToolbar::kCompactBandDp); });
    m_frame->AddPart (m_view);
    m_view->SetPaneTextInset (true);

    dock.AddPane (GetId(), GetTitle(), m_frame.get());
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryWindowPane::GetContent
//
////////////////////////////////////////////////////////////////////////////////

IDxuiControl * MemoryWindowPane::GetContent() const
{
    return m_frame.get();
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryWindowPane::GetControls
//
////////////////////////////////////////////////////////////////////////////////

std::vector<IDxuiControl *> MemoryWindowPane::GetControls() const
{
    return { m_slot.get(), m_view };
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryWindowPane::GetColorKey
//
////////////////////////////////////////////////////////////////////////////////

std::optional<PaneColorKeySpec> MemoryWindowPane::GetColorKey() const
{
    return PaneColorKeySpec { m_slot.get(), ColorLegend::Pane::Memory, nullptr };
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryWindowPane::RequestOpen
//
//  A window after the first opens where the first one is.
//
////////////////////////////////////////////////////////////////////////////////

bool MemoryWindowPane::RequestOpen()
{
    MemoryWindowPane  * first = m_group.GetWindow (1);



    if (m_windowId == 1)
    {
        return false;
    }

    if (m_host.GetWindowHost() != nullptr && first != nullptr)
    {
        m_host.GetWindowHost()->SetDebuggerMemoryWindow (m_windowId, first->GetModel().GetTopAddress());
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryWindowPane::RequestClose
//
////////////////////////////////////////////////////////////////////////////////

bool MemoryWindowPane::RequestClose()
{
    if (m_windowId == 1)
    {
        return false;
    }

    if (m_host.GetWindowHost() != nullptr)
    {
        m_host.GetWindowHost()->SetDebuggerMemoryWindow (m_windowId, std::nullopt);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryWindowPane::AddContentMenuItems
//
////////////////////////////////////////////////////////////////////////////////

void MemoryWindowPane::AddContentMenuItems (POINT clientPx, PaneMenuItems & items)
{
    m_group.AddWindowMenuItems (*this, clientPx, items);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryWindowGroup::MemoryWindowGroup
//
////////////////////////////////////////////////////////////////////////////////

MemoryWindowGroup::MemoryWindowGroup (IDebuggerPaneHost & host) :
    IDebuggerPane (host)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryWindowGroup::~MemoryWindowGroup
//
////////////////////////////////////////////////////////////////////////////////

MemoryWindowGroup::~MemoryWindowGroup() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryWindowGroup::CreateWindows
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::unique_ptr<IDebuggerPane>> MemoryWindowGroup::CreateWindows()
{
    std::vector<std::unique_ptr<IDebuggerPane>>  windows;



    for (int id = 1; id <= DebuggerViewState::kMaxMemoryWindows; id++)
    {
        std::unique_ptr<MemoryWindowPane>  window = std::make_unique<MemoryWindowPane> (m_host, *this, id);

        m_windows[(size_t) (id - 1)] = window.get();
        windows.push_back (std::move (window));
    }

    return windows;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryWindowGroup::GetWindow
//
////////////////////////////////////////////////////////////////////////////////

MemoryWindowPane * MemoryWindowGroup::GetWindow (int id) const
{
    return (id >= 1 && id <= DebuggerViewState::kMaxMemoryWindows) ? m_windows[(size_t) (id - 1)] : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryWindowGroup::GetOpenWindows
//
//  In window order, which is the order they are laid out.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<MemoryWindowPane *> MemoryWindowGroup::GetOpenWindows() const
{
    std::vector<MemoryWindowPane *>  open;



    for (MemoryWindowPane * window : m_windows)
    {
        if (window != nullptr && window->IsOpen())
        {
            open.push_back (window);
        }
    }

    return open;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryWindowGroup::GetActive
//
//  The window last clicked in, which the Address box and the bar act on; the
//  first window until another is clicked.
//
////////////////////////////////////////////////////////////////////////////////

MemoryWindowPane * MemoryWindowGroup::GetActive() const
{
    return (m_active != nullptr) ? m_active : m_windows[0];
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryWindowGroup::GetFocused
//
////////////////////////////////////////////////////////////////////////////////

MemoryWindowPane * MemoryWindowGroup::GetFocused() const
{
    IDxuiControl  * focused = m_host.GetFocusedControl();



    for (MemoryWindowPane * window : GetOpenWindows())
    {
        if (focused != nullptr && focused == window->GetView())
        {
            return window;
        }
    }

    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryWindowGroup::IsAnyHeatOn
//
////////////////////////////////////////////////////////////////////////////////

bool MemoryWindowGroup::IsAnyHeatOn() const
{
    return std::ranges::any_of (m_windows, [] (const MemoryWindowPane * window) { return window != nullptr && window->GetModel().IsHeatOn(); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryWindowGroup::ClearHistories
//
////////////////////////////////////////////////////////////////////////////////

void MemoryWindowGroup::ClearHistories()
{
    for (MemoryWindowPane * window : m_windows)
    {
        window->GetModel().ClearHistory();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryWindowGroup::CreateControls
//
//  The bar before its Address box, so the box paints over the strip.
//
////////////////////////////////////////////////////////////////////////////////

void MemoryWindowGroup::CreateControls()
{
    m_bar = m_host.GetControlParent().CreateChild<DxuiToolbar>();
    m_box = m_host.GetControlParent().CreateChild<DxuiTextInput>();
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryWindowGroup::Configure
//
//  Visual Studio's memory window bar: the Address box, Refresh, Columns and
//  the grouping. View > Memory opens another window. Typing into the bytes or
//  POKE at the console writes memory, so the bar has no poke box; a window
//  closes from its tab.
//
////////////////////////////////////////////////////////////////////////////////

void MemoryWindowGroup::Configure()
{
    MemoryBarCommands::Handlers  handlers;



    handlers.dispatch  = [this] (int id) { RunBarEntry (id); };
    handlers.getLabel  = [this] (int id) { return GetBarLabel (id); };
    handlers.isEnabled = [this] (int id) { return IsBarEnabled (id); };
    handlers.isChecked = [this] (int id) { return id == MemoryBarCommands::kHeat && GetActive() != nullptr && GetActive()->GetModel().IsHeatOn(); };
    handlers.getTip    = [this] (int id) { return GetBarTip (id); };

    m_commands     = std::make_unique<MemoryBarCommands>  (std::move (handlers));
    m_addressEntry = std::make_unique<MemoryAddressEntry> (m_box);

    m_addressEntry->SetTooltips (L"Address: a hex address, a register, a symbol or an expression such as (3E),Y; Enter goes there",
                                 L"Addresses entered before");

    m_bar->SetTextRenderer  (m_host.GetPaneTextRenderer());
    m_bar->SetPopupHost     (m_host.GetPanePopupHost());
    m_bar->SetIconFace      (DxuiToolbar::kMdl2IconFace);
    m_bar->SetCompact       (true);
    m_bar->SetPaneTextInset (true);
    m_bar->SetBottomEdge    (false);
    m_bar->EnableSeeMore    (s_kpszMdl2More, L"See more");
    m_bar->SetEntries       (m_commands->BuildEntries (m_addressEntry.get()));
    m_bar->SetVisible       (false);

    m_box->SetHwnd        (m_host.GetOwnerHwnd());
    m_box->SetMaxLength   (256);
    m_box->SetPlaceholder (L"Address: 0300, PC, (3E),Y");

    SetBarMenus();
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryWindowGroup::SetBarMenus
//
//  The Address box's history, newest first, and the Columns and grouping
//  choices with the active window's checked. Built again whenever what they
//  list changes, since the rows have the state they were built with.
//
////////////////////////////////////////////////////////////////////////////////

void MemoryWindowGroup::SetBarMenus()
{
    std::vector<DxuiPopupMenuItem>  history;
    std::vector<DxuiPopupMenuItem>  columns;
    std::vector<DxuiPopupMenuItem>  grouping;
    MemoryWindowPane              * window   = GetActive();



    for (const std::wstring & entered : m_history)
    {
        history.push_back (DxuiPopupMenuItem::ForCommand (DebuggerMenuCommand::Make (entered, false, [this, entered]
        {
            m_box->SetText (entered);
            SubmitBox();
        })));
    }

    for (int choice : MemoryBarCommands::GetColumnChoices())
    {
        columns.push_back (DxuiPopupMenuItem::ForCommand (DebuggerMenuCommand::Make (MemoryBarCommands::GetColumnsLabel (choice),
                                                                                     window != nullptr && window->GetModel().GetColumns() == choice,
                                                                                     [this, choice] { ChooseColumns (choice); })));
    }

    for (int choice : MemoryBarCommands::GetGroupingChoices())
    {
        grouping.push_back (DxuiPopupMenuItem::ForCommand (DebuggerMenuCommand::Make (MemoryBarCommands::GetGroupingName (choice),
                                                                                      window != nullptr && window->GetModel().GetGrouping() == choice,
                                                                                      [this, choice] { ChooseGrouping (choice); })));
    }

    m_bar->SetDropDownItems (MemoryBarCommands::kAddress,  std::move (history));
    m_bar->SetDropDownItems (MemoryBarCommands::kColumns,  std::move (columns));
    m_bar->SetDropDownItems (MemoryBarCommands::kGrouping, std::move (grouping));
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryWindowGroup::GetBarLabel
//
//  What the drop-downs show: the choice in force in the active window.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring MemoryWindowGroup::GetBarLabel (int id) const
{
    MemoryWindowPane  * window = GetActive();



    if (window == nullptr)
    {
        return std::wstring();
    }

    if (id == MemoryBarCommands::kColumns)
    {
        return L"Columns: " + MemoryBarCommands::GetColumnsLabel (window->GetModel().GetColumns());
    }

    if (id == MemoryBarCommands::kGrouping)
    {
        return MemoryBarCommands::GetGroupingLabel (window->GetModel().GetGrouping());
    }

    return std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryWindowGroup::IsBarEnabled
//
//  Undo and Redo, while the active window has an edit to take back or make
//  again.
//
////////////////////////////////////////////////////////////////////////////////

bool MemoryWindowGroup::IsBarEnabled (int id) const
{
    MemoryWindowPane  * window = GetActive();



    if (id == MemoryBarCommands::kUndo)
    {
        return window != nullptr && window->GetModel().CanUndo();
    }

    if (id == MemoryBarCommands::kRedo)
    {
        return window != nullptr && window->GetModel().CanRedo();
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryWindowGroup::GetBarTip
//
//  Undo's and Redo's tips quote what they would act on.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring MemoryWindowGroup::GetBarTip (int id) const
{
    MemoryWindowPane  * window = GetActive();
    std::wstring        text;



    if (window != nullptr && id == MemoryBarCommands::kUndo && window->GetModel().CanUndo())
    {
        text = UndoText::GetLabel (false, window->GetModel().GetUndoText());
    }
    else if (window != nullptr && id == MemoryBarCommands::kRedo && window->GetModel().CanRedo())
    {
        text = UndoText::GetLabel (true, window->GetModel().GetRedoText());
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryWindowGroup::RunBarEntry
//
//  The drop-downs open from the strip; a choice from one comes back through
//  ChooseColumns or ChooseGrouping.
//
////////////////////////////////////////////////////////////////////////////////

void MemoryWindowGroup::RunBarEntry (int id)
{
    switch (id)
    {
    case MemoryBarCommands::kAddress:
        m_host.FocusControl (m_box);
        break;

    case MemoryBarCommands::kRefresh:
        GetActive()->GetModel().Refresh();
        break;

    case MemoryBarCommands::kUndo:
    case MemoryBarCommands::kRedo:
        Undo (GetActive(), id == MemoryBarCommands::kRedo);
        break;

    case MemoryBarCommands::kHeat:
        GetActive()->GetModel().SetHeatOn (!GetActive()->GetModel().IsHeatOn());
        m_host.SyncHeatRecording();
        break;

    default:
        break;
    }

    m_host.Repaint();
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryWindowGroup::ChooseColumns
//
////////////////////////////////////////////////////////////////////////////////

void MemoryWindowGroup::ChooseColumns (int columns)
{
    GetActive()->GetModel().SetColumns (columns);
    SetBarMenus();
    m_host.RelayoutWindow();
    m_host.Repaint();
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryWindowGroup::ChooseGrouping
//
////////////////////////////////////////////////////////////////////////////////

void MemoryWindowGroup::ChooseGrouping (int grouping)
{
    GetActive()->GetModel().SetGrouping (grouping);
    SetBarMenus();
    m_host.RelayoutWindow();
    m_host.Repaint();
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryWindowGroup::AddHistory
//
//  Newest first, each address once, as Visual Studio's Address box lists it.
//
////////////////////////////////////////////////////////////////////////////////

void MemoryWindowGroup::AddHistory (const std::wstring & text)
{
    constexpr size_t  kMaxHistory = 16;
    std::wstring      entered     = text;



    std::erase (m_history, entered);
    m_history.insert (m_history.begin(), entered);

    if (m_history.size() > kMaxHistory)
    {
        m_history.resize (kMaxHistory);
    }

    SetBarMenus();
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryWindowGroup::SubmitBox
//
//  A register's own letter is also a hex digit, so "A" alone goes to the
//  resolver, which reads it as the register, not as an address.
//
////////////////////////////////////////////////////////////////////////////////

void MemoryWindowGroup::SubmitBox()
{
    const std::wstring &  text     = m_box->GetText();
    Word                  address  = 0;
    bool                  isBlank  = std::all_of (text.begin(), text.end(), [] (wchar_t ch) { return iswspace (ch) != 0; });
    std::wstring          upper;



    for (wchar_t ch : text)
    {
        if (!iswspace (ch))
        {
            upper += (wchar_t) towupper (ch);
        }
    }

    if (isBlank)
    {
        return;
    }

    AddHistory (text);

    if (upper != L"A" && TextDialog::TryParseHexWord (text, address))
    {
        GetActive()->GetModel().GoTo (address);
    }
    else if (m_host.GetWindowHost() != nullptr)
    {
        m_host.GetWindowHost()->GoToDebuggerMemory (GetActive()->GetWindowId(), SourcePathList::WideToUtf8 (m_box->GetText()));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryWindowGroup::Undo
//
//  A window's last edit taken back, or with `redo` made again, while the
//  machine is stopped and the window takes edits.
//
////////////////////////////////////////////////////////////////////////////////

void MemoryWindowGroup::Undo (MemoryWindowPane * window, bool redo)
{
    if (window == nullptr)
    {
        return;
    }

    (void) (redo ? window->GetModel().Redo() : window->GetModel().Undo());
    m_host.Repaint();
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryWindowGroup::GetPaneOfSharedControl
//
//  The bar and its Address box are in the pane they sit in.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring MemoryWindowGroup::GetPaneOfSharedControl (const IDxuiControl * control) const
{
    return (control != nullptr && (control == m_bar || control == m_box)) ? m_barPane : std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryWindowGroup::TakesTyping
//
//  A window's bytes are typed into, as a box is.
//
////////////////////////////////////////////////////////////////////////////////

bool MemoryWindowGroup::TakesTyping (const IDxuiControl * control) const
{
    return std::ranges::any_of (GetOpenWindows(), [control] (const MemoryWindowPane * window) { return control != nullptr && control == window->GetView(); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryWindowGroup::Apply
//
//  A Go to the CPU thread resolved, once; a Find's or a comparison's
//  results; then which windows are open: each one the snapshot lists is
//  shown and given its bytes, and the rest are hidden. A window opened comes
//  to the front of its tab group.
//
////////////////////////////////////////////////////////////////////////////////

void MemoryWindowGroup::Apply (const DebuggerViewSnapshot & snapshot)
{
    std::array<bool, DebuggerViewState::kMaxMemoryWindows>  open    = {};
    bool                                                    changed = false;



    if (snapshot.goTo.has_value() && snapshot.goTo->serial != m_goToSerial)
    {
        const DebuggerViewSnapshot::GoTo & goTo = *snapshot.goTo;

        m_goToSerial = goTo.serial;

        if (goTo.address.has_value() && GetWindow (goTo.window) != nullptr)
        {
            GetWindow (goTo.window)->GetModel().GoTo (*goTo.address);
        }
        else
        {
            m_host.AppendConsole ({ std::format ("Go to: \"{}\" is not an address. {}", goTo.text, goTo.error) });
        }
    }

    m_isPaused    = snapshot.isPaused;
    m_hasSnapshot = true;
    m_searchHits  = snapshot.searchHits;

    ApplyResults (snapshot);

    for (const DebuggerViewSnapshot::MemoryWindow & window : snapshot.memoryWindows)
    {
        MemoryWindowPane  * each = GetWindow (window.id);



        if (each == nullptr)
        {
            continue;
        }

        open[(size_t) (window.id - 1)] = true;

        each->GetModel().SetTextColors (m_host.GetTextColors());
        each->GetModel().SetPaused     (snapshot.isPaused);
        each->GetModel().SetHeat       (snapshot.heatColumns, m_host.GetHeatPalette());

        //  Bytes are edited while the machine is stopped, as Visual Studio's
        //  memory window allows: a running machine would overwrite the edit,
        //  or be changed under the code using it.
        if (each->GetView()->IsEditable() != snapshot.isPaused)
        {
            each->GetView()->SetEditable (snapshot.isPaused);
        }

        each->GetModel().Apply (window);
    }

    for (size_t i = 0; i < m_windows.size(); i++)
    {
        if (m_windows[i]->IsOpen() != open[i])
        {
            m_windows[i]->SetOpen (open[i]);
            changed = true;

            if (open[i])
            {
                m_host.ActivatePaneTab (m_windows[i]->GetId());
            }
        }

        if (!open[i] && m_active == m_windows[i])
        {
            m_active = nullptr;
        }
    }

    if (changed)
    {
        m_host.RelayoutDock();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryWindowGroup::ApplyResults
//
//  A Find's hits or a comparison's differences, once the snapshot has the
//  ones its command made.
//
////////////////////////////////////////////////////////////////////////////////

void MemoryWindowGroup::ApplyResults (const DebuggerViewSnapshot & snapshot)
{
    m_searchSerial  = snapshot.searchSerial;
    m_compareSerial = snapshot.compareSerial;

    if (m_find.isAwaiting && snapshot.searchSerial != m_find.awaited)
    {
        m_find.isAwaiting = false;
        StepFind (true);
    }

    if (m_compare.isAwaiting && snapshot.compareSerial != m_compare.awaited && GetWindow (m_compare.window) != nullptr)
    {
        m_compare.isAwaiting = false;
        GetWindow (m_compare.window)->GetModel().SetComparison (snapshot.comparison);
        m_host.Repaint();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryWindowGroup::Tick
//
//  Each open window sends a read request around the rows it scrolled to,
//  and its scrollbars fade.
//
////////////////////////////////////////////////////////////////////////////////

bool MemoryWindowGroup::Tick (int64_t nowMs)
{
    bool  isAnimating = false;



    for (MemoryWindowPane * window : GetOpenWindows())
    {
        window->GetModel().FollowScroll();
        isAnimating = window->GetView()->TickScrollbars (nowMs) || isAnimating;
    }

    return isAnimating;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryWindowGroup::SetTextZoom
//
////////////////////////////////////////////////////////////////////////////////

void MemoryWindowGroup::SetTextZoom (float zoom)
{
    for (MemoryWindowPane * window : m_windows)
    {
        window->GetView()->SetZoom (zoom);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryWindowGroup::PlaceBar
//
//  The bar sits in the bar of the window it acts on: the active one when it
//  is shown in this window, or else the first one shown. With none shown in
//  this window, it hides.
//
////////////////////////////////////////////////////////////////////////////////

void MemoryWindowGroup::PlaceBar()
{
    DebuggerPaneFrame  * slot   = nullptr;
    MemoryWindowPane   * owner  = GetActive();
    DxuiWindow         * host   = nullptr;
    RECT                 bounds = {};



    if (owner != nullptr && owner->GetSlot()->IsVisible())
    {
        slot      = owner->GetSlot();
        m_barPane = owner->GetId();
    }

    for (size_t i = 0; slot == nullptr && i < m_windows.size(); i++)
    {
        if (m_windows[i]->GetSlot()->IsVisible())
        {
            slot      = m_windows[i]->GetSlot();
            m_barPane = m_windows[i]->GetId();
        }
    }

    //  The bar is the pane's, so it goes with the pane into a floating
    //  window; with no pane to sit in, it is put in the main window.
    host = (slot != nullptr) ? m_host.GetPaneWindow (m_barPane) : m_host.GetPaneWindow (std::wstring());
    MoveBar (host);

    if (slot == nullptr)
    {
        m_barPane.clear();
    }

    m_bar->SetVisible (slot != nullptr);

    //  A hidden box has no focus, or it would go on taking the keys; they go
    //  back to the command line, as they do when the find bar closes.
    m_box->SetVisible (slot != nullptr);

    if (slot == nullptr && m_host.GetFocusedControl() == m_box)
    {
        m_host.FocusDefault();
    }

    if (slot == nullptr)
    {
        return;
    }

    //  The strip lays out its own entries, the Address box among them, and
    //  moves what does not fit into its "..." menu.
    bounds = slot->GetBounds();

    m_bar->SetTextRenderer   (host->GetTextRenderer());
    m_bar->SetPopupHost      (host->GetPopupHost());
    m_bar->SetHostClientRect (host->GetBounds());
    m_bar->Layout            (m_host.GetBarStrip (m_barPane, bounds), m_host.GetPaneScaler());

    host->SetChildClip (m_bar, bounds);
    host->SetChildClip (m_box, bounds);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryWindowGroup::MoveBar
//
////////////////////////////////////////////////////////////////////////////////

void MemoryWindowGroup::MoveBar (DxuiWindow * to)
{
    DxuiWindow  * from = (m_barHost != nullptr) ? m_barHost : m_host.GetPaneWindow (std::wstring());



    if (to == from)
    {
        return;
    }

    m_host.MoveSharedControls ({ m_bar, m_box }, from, to);
    m_barHost = to;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryWindowGroup::OnFloatClosed
//
//  The bar may be sitting in a window of the floating one, and would go
//  with it.
//
////////////////////////////////////////////////////////////////////////////////

void MemoryWindowGroup::OnFloatClosed (DxuiWindow * window)
{
    if (m_barHost == window)
    {
        MoveBar (m_host.GetPaneWindow (std::wstring()));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryWindowGroup::RouteBarMouse
//
//  As the command bar's, less the Address box: the box is a control of its
//  own and takes its presses as every box does.
//
////////////////////////////////////////////////////////////////////////////////

bool MemoryWindowGroup::RouteBarMouse (const DxuiMouseEvent & ev)
{
    int   x     = ev.positionDip.x;
    int   y     = ev.positionDip.y;
    bool  open  = false;
    bool  over  = false;



    if (!m_host.IsBarRouted (m_barPane))
    {
        return false;
    }

    open = m_bar->IsMenuOpen();
    over = m_bar->IsVisible() && DxuiDockSite::Contains (m_bar->GetBounds(), POINT { x, y }) &&
           !(m_box->IsVisible() && DxuiDockSite::Contains (m_box->GetBounds(), POINT { x, y }));

    if (!over && !open)
    {
        m_bar->OnToolbarMouseLeave();
        return false;
    }

    //  The right button over the strip opens the pane's own menu.
    if ((ev.kind == DxuiMouseEventKind::Down || ev.kind == DxuiMouseEventKind::Up) && ev.button != DxuiMouseButton::Left && !open)
    {
        return false;
    }

    switch (ev.kind)
    {
    case DxuiMouseEventKind::Move:
        m_host.UpdateTooltip (ev.positionDip);
        return m_bar->OnToolbarMouseMove (x, y);

    case DxuiMouseEventKind::Down:
        //  The rows have the checks they were built with, and the active
        //  window may have changed since, so they are built again here.
        if (!open)
        {
            SetBarMenus();
        }

        return m_bar->OnToolbarLButtonDown (x, y);

    case DxuiMouseEventKind::Up:
        return m_bar->OnToolbarLButtonUp (x, y);

    default:
        return open;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryWindowGroup::GetBarTip
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * MemoryWindowGroup::GetBarTip (POINT clientPx, RECT & anchor) const
{
    if (!m_host.IsBarRouted (m_barPane) || !m_bar->IsVisible())
    {
        return nullptr;
    }

    return m_bar->GetTooltipAt (clientPx.x, clientPx.y, anchor);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryWindowGroup::RouteContentMouse
//
//  A drag in a window has its events until it ends; a press or the wheel
//  goes to the window under it, and a press makes it the active one.
//  Every window gets the release, so none is left mid-drag.
//
////////////////////////////////////////////////////////////////////////////////

bool MemoryWindowGroup::RouteContentMouse (const DxuiMouseEvent & ev)
{
    POINT  at = ev.positionDip;



    for (MemoryWindowPane * window : GetOpenWindows())
    {
        DxuiHexView  * view   = window->GetView();
        RECT           bounds = view->GetBounds();
        bool           inside = view->IsVisible() && at.x >= bounds.left && at.x < bounds.right && at.y >= bounds.top && at.y < bounds.bottom;

        if (!m_host.IsRoutable (view))
        {
            continue;
        }

        if (view->IsDragging() && ev.kind != DxuiMouseEventKind::Down)
        {
            (void) view->OnMouse (ev);
            return true;
        }

        if (inside && (ev.kind == DxuiMouseEventKind::Down || ev.kind == DxuiMouseEventKind::Wheel))
        {
            (void) view->OnMouse (ev);

            //  A press starts a selection or an edit, which the address tip
            //  would sit over; the next move brings it back.
            if (ev.kind == DxuiMouseEventKind::Down)
            {
                m_host.HideTooltip();
                m_host.FocusControl (view);
                m_active = window;
            }

            return true;
        }

        if (ev.kind == DxuiMouseEventKind::Up)
        {
            (void) view->OnMouse (ev);
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryWindowGroup::RouteEditKey
//
//  Typing into a window is an edit, which the address tip would sit over;
//  the next move of the pointer brings it back. Characters go to the
//  window's bytes, and Ctrl+Z and Ctrl+Y undo and redo its edits.
//
////////////////////////////////////////////////////////////////////////////////

bool MemoryWindowGroup::RouteEditKey (const DxuiKeyEvent & ev, IDxuiControl * focused)
{
    MemoryWindowPane  * window = GetFocused();



    if (window == nullptr || focused != window->GetView())
    {
        return false;
    }

    if (ev.kind == DxuiKeyEventKind::Down)
    {
        m_host.HideTooltip();
    }

    if (ev.kind == DxuiKeyEventKind::Char)
    {
        return focused->OnKey (ev);
    }

    if (ev.kind == DxuiKeyEventKind::Down && ev.ctrl && !ev.alt && (ev.vk == 'Z' || ev.vk == 'Y'))
    {
        Undo (window, ev.vk == 'Y');
        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryWindowGroup::OnMappedAction
//
//  In a window, Find searches memory and F3 and Shift+F3 walk the matches.
//
////////////////////////////////////////////////////////////////////////////////

bool MemoryWindowGroup::OnMappedAction (DebuggerKeySchemes::Action action, IDxuiControl *)
{
    MemoryWindowPane  * window = GetFocused();



    if (window == nullptr || (action != DebuggerKeySchemes::Action::Find && action != DebuggerKeySchemes::Action::FindNext &&
                              action != DebuggerKeySchemes::Action::FindPrevious))
    {
        return false;
    }

    if (action != DebuggerKeySchemes::Action::Find && m_find.window == window->GetWindowId())
    {
        StepFind (action == DebuggerKeySchemes::Action::FindNext);
    }
    else
    {
        FindIn (window);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryWindowGroup::SubmitTextBox
//
////////////////////////////////////////////////////////////////////////////////

bool MemoryWindowGroup::SubmitTextBox (DxuiTextInput * box)
{
    if (box != m_box)
    {
        return false;
    }

    SubmitBox();
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryWindowGroup::TryGetTip
//
//  Over a byte of an open window, in either column, its address, at once and
//  following the pointer itself rather than snapping from byte to byte.
//
////////////////////////////////////////////////////////////////////////////////

bool MemoryWindowGroup::TryGetTip (POINT clientPx, PaneTip & tip)
{
    for (MemoryWindowPane * window : GetOpenWindows())
    {
        DxuiHexView  * view = window->GetView();

        if (m_host.IsRoutable (view) && view->IsVisible() && DxuiDockSite::Contains (view->GetBounds(), clientPx) &&
            view->TryGetByteTipAt (clientPx, tip.anchor, tip.text))
        {
            tip.kind        = PaneTip::Kind::Pointer;
            tip.isMonospace = true;
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryWindowGroup::AddWindowMenuItems
//
//  On a selection: Fill and Copy write the machine, so only while it is
//  stopped; Compare reads it, so at any time. A byte in zero page can be
//  the low byte of a pointer.
//
////////////////////////////////////////////////////////////////////////////////

void MemoryWindowGroup::AddWindowMenuItems (MemoryWindowPane & window, POINT clientPx, PaneMenuItems & items)
{
    MemoryWindowPane     * each  = &window;
    MemoryPane           & model = window.GetModel();
    std::optional<Word>    byte;



    if (!window.IsOpen() || !m_host.IsRoutable (window.GetView()) || !window.GetView()->IsVisible() ||
        !DxuiDockSite::Contains (window.GetView()->GetBounds(), clientPx))
    {
        return;
    }

    m_active = each;
    items.push_back ({ L"Copy",                   [each] { each->GetView()->CopySelection(); } });
    items.push_back ({ L"Go to...",               [this] { m_host.FocusControl (m_box); } });
    items.push_back ({ L"Change bytes per value", [each] { (void) each->GetModel().CycleGrouping(); } });
    items.push_back ({ L"Find...",                [this, each] { FindIn (each); } });

    if (model.GetSelectedRange().has_value())
    {
        if (m_isPaused)
        {
            items.push_back ({ L"Fill selection...",    [this, each] { FillSelection (each); } });
            items.push_back ({ L"Copy selection to...", [this, each] { CopySelection (each); } });
        }

        items.push_back ({ L"Compare with...", [this, each] { CompareSelection (each); } });
    }

    if (model.HasComparison())
    {
        items.push_back ({ model.IsShowingOther() ? L"View this range's values" : L"View the other range's values", [each] { each->GetModel().ToggleShowOther(); } });
        items.push_back ({ L"Clear comparison", [each] { each->GetModel().ClearComparison(); } });
    }

    if (model.CanUndo())
    {
        items.push_back ({ UndoText::GetLabel (false, model.GetUndoText()), [this, each] { Undo (each, false); } });
    }

    if (model.CanRedo())
    {
        items.push_back ({ UndoText::GetLabel (true, model.GetRedoText()), [this, each] { Undo (each, true); } });
    }

    byte = model.GetAddressAt (clientPx);

    if (byte.has_value() && *byte < 0x100)
    {
        items.push_back ({ L"Add as pointer watch", [this, at = (Byte) *byte] { m_host.RunAction (DebuggerActions::GetAddZeroPagePointer (at, m_host.GetMode())); } });
    }

    if (byte.has_value())
    {
        m_host.AddHeatAccessItems (*byte, HeatMapOptions::Bank::Cpu, items);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryWindowGroup::GetSearchLength
//
//  How many bytes a match of S's items covers: each character of a quoted
//  text, a byte or a wildcard for each other item, and two for a value of
//  more than two hex digits, which S matches as a word.
//
////////////////////////////////////////////////////////////////////////////////

size_t MemoryWindowGroup::GetSearchLength (const std::wstring & items)
{
    size_t  length = 0;
    size_t  at     = 0;



    while (at < items.size())
    {
        size_t  end = 0;

        if (iswspace (items[at]) || items[at] == L',')
        {
            at++;
            continue;
        }

        if (items[at] == L'"' || items[at] == L'\'')
        {
            end     = items.find (items[at], at + 1);
            end     = (end == std::wstring::npos) ? items.size() : end;
            length += end - at - 1;
            at      = end + 1;
            continue;
        }

        end     = items.find_first_of (L" \t,", at);
        end     = (end == std::wstring::npos) ? items.size() : end;
        length += (end - at > 2 && items[at] != L'$') || (end - at > 3) ? 2 : 1;
        at      = end;
    }

    return std::max<size_t> (length, 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryWindowGroup::FindIn
//
//  S over all memory with the items typed: hex bytes, a word, text in
//  quotes, ?? for any byte and ?n or n? for half of one. The window goes to
//  the first match when the hits arrive.
//
////////////////////////////////////////////////////////////////////////////////

void MemoryWindowGroup::FindIn (MemoryWindowPane * window)
{
    std::optional<std::wstring>  items;



    if (window == nullptr || !m_hasSnapshot)
    {
        return;
    }

    items = m_host.RunTextDialog (L"Find in memory", L"Find", L"A9 00 ?? \"HELLO\"", [] (const std::wstring & text, std::wstring & error)
    {
        error = L"Type hex bytes, text in quotes, or ?? for any byte.";
        return text.find_first_not_of (L" \t") != std::wstring::npos;
    });

    //  F3 and Shift+F3 walk the matches from the window, which the dialog
    //  took the focus from.
    m_host.FocusControl (window->GetView());

    if (!items.has_value())
    {
        return;
    }

    m_find = Find { window->GetWindowId(), GetSearchLength (*items), -1, m_searchSerial, true };
    m_host.RunAction (DebuggerActions::GetSearchMemory (TextEncoding::WideToNarrow (*items), m_host.GetMode()));
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryWindowGroup::StepFind
//
//  The next match, or the one before, round to the other end.
//
////////////////////////////////////////////////////////////////////////////////

void MemoryWindowGroup::StepFind (bool forward)
{
    int  count = (int) m_searchHits.size();



    if (count == 0 || GetWindow (m_find.window) == nullptr)
    {
        m_host.AppendConsole ({ "No matches in memory." });
        return;
    }

    if (m_find.at < 0)
    {
        m_find.at = forward ? 0 : count - 1;
    }
    else
    {
        m_find.at = (m_find.at + (forward ? 1 : count - 1)) % count;
    }

    GetWindow (m_find.window)->GetModel().SelectRange (m_searchHits[(size_t) m_find.at], m_find.length);
    m_host.Repaint();
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryWindowGroup::FillSelection
//
//  F of the selection with the bytes typed, repeated over it.
//
////////////////////////////////////////////////////////////////////////////////

void MemoryWindowGroup::FillSelection (MemoryWindowPane * window)
{
    std::optional<std::pair<Word, Word>>  range = (window != nullptr) ? window->GetModel().GetSelectedRange() : std::nullopt;
    std::optional<std::wstring>           typed;
    std::vector<Byte>                     bytes;



    if (!range.has_value())
    {
        return;
    }

    typed = m_host.RunTextDialog (L"Fill selection", L"Bytes", L"00 or A9 20", [] (const std::wstring & text, std::wstring & error)
    {
        std::vector<Byte>  parsed;

        error = L"Type one or more hex bytes, as 00 or A9 20.";
        return TextDialog::TryParseBytes (text, parsed);
    });

    if (typed.has_value() && TextDialog::TryParseBytes (*typed, bytes))
    {
        m_host.RunAction (DebuggerActions::GetFillMemory (range->first, range->second, bytes, m_host.GetMode()));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryWindowGroup::CopySelection
//
////////////////////////////////////////////////////////////////////////////////

void MemoryWindowGroup::CopySelection (MemoryWindowPane * window)
{
    std::optional<std::pair<Word, Word>>  range = (window != nullptr) ? window->GetModel().GetSelectedRange() : std::nullopt;
    std::optional<std::wstring>           typed;
    Word                                  to    = 0;



    if (!range.has_value())
    {
        return;
    }

    typed = m_host.RunTextDialog (L"Copy selection to", L"Address", L"$6000", [] (const std::wstring & text, std::wstring & error)
    {
        Word  address = 0;

        error = L"Type an address, one to four hex digits, as $6000.";
        return TextDialog::TryParseAddress (text, address);
    });

    if (typed.has_value() && TextDialog::TryParseAddress (*typed, to))
    {
        m_host.RunAction (DebuggerActions::GetMoveMemory (to, range->first, range->second, m_host.GetMode()));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryWindowGroup::CompareSelection
//
//  MC of the selection with the range at the address typed; the window
//  marks the bytes that differ once the snapshot has them.
//
////////////////////////////////////////////////////////////////////////////////

void MemoryWindowGroup::CompareSelection (MemoryWindowPane * window)
{
    std::optional<std::pair<Word, Word>>  range = (window != nullptr) ? window->GetModel().GetSelectedRange() : std::nullopt;
    std::optional<std::wstring>           typed;
    Word                                  other = 0;



    if (window == nullptr || !range.has_value() || !m_hasSnapshot)
    {
        return;
    }

    typed = m_host.RunTextDialog (L"Compare with", L"Address", L"$6000", [] (const std::wstring & text, std::wstring & error)
    {
        Word  address = 0;

        error = L"Type an address, one to four hex digits, as $6000.";
        return TextDialog::TryParseAddress (text, address);
    });

    if (typed.has_value() && TextDialog::TryParseAddress (*typed, other))
    {
        m_compare = Compare { window->GetWindowId(), m_compareSerial, true };
        m_host.RunAction (DebuggerActions::GetCompareMemory (other, range->first, range->second, m_host.GetMode()));
    }
}
