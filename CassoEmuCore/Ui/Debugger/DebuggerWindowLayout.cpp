#include "Pch.h"

#include "Ui/Debugger/DebuggerWindow.h"
#include "Ui/Debugger/DebuggerMenuCommand.h"
#include "Ui/Debugger/DroppedFiles.h"
#include "Ui/Debugger/BranchArrow.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerStatusText.h"
#include "Ui/Debugger/GutterGlyph.h"
#include "Debugger/CommandModeNames.h"
#include "Debugger/Source/SourcePathList.h"
#include "Config/WindowPlacementProfile.h"

#include "Core/TextEncoding.h"
#include "Core/UnicodeSymbols.h"
#include "Ui/Debugger/FlagsDialog.h"
#include "Ui/Debugger/BreakpointDialog.h"
#include "Debugger/SymbolDescriptions.h"
#include "Cassque/CassquePromptDialog.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/Panes/TracePaneController.h"
#include "Ui/Debugger/MemoryBarCommands.h"
#include "Ui/Debugger/Panes/MeterBar.h"
#include "Ui/Debugger/MemoryAddressEntry.h"
#include "Ui/Debugger/Panes/BasicProgramController.h"
#include "Ui/Debugger/Panes/BreakpointsPane.h"
#include "Ui/Debugger/Panes/CodeEditCommands.h"
#include "Ui/Debugger/Panes/ConsolePanes.h"
#include "Ui/Debugger/ViewMenuRows.h"
#include "Ui/Debugger/Panes/DisassemblyGutter.h"
#include "Ui/Debugger/Panes/DisassemblyViews.h"
#include "Ui/Debugger/Panes/FindWidgets.h"
#include "Ui/Debugger/Panes/SourceDocumentPanes.h"
#include "Ui/Debugger/Panes/ToolPanes.h"
#include "Ui/Debugger/Panes/HeatMapPane.h"
#include "Ui/Debugger/Panes/MemoryPane.h"
#include "Ui/Debugger/Panes/MemoryWindows.h"
#include "Ui/Debugger/Panes/OperandTipController.h"
#include "Ui/Debugger/Panes/UndoListPanes.h"
#include "Ui/Debugger/Panes/WatchesPane.h"
#include "Ui/Debugger/DebuggerCommands.h"
#include "Ui/Debugger/HistoryBand.h"
#include "Ui/Debugger/HeatStrip.h"
#include "Ui/Debugger/Panes/FlameGraphView.h"
#include "Ui/Debugger/Panes/MemoryMapBar.h"
#include "Ui/Debugger/Panes/ProfileTimeline.h"
#include "Ui/Debugger/Panes/DiagnosticsPane.h"
#include "Ui/Debugger/ColorKeyButton.h"
#include "Ui/Debugger/HeatMapBarCommands.h"
#include "Ui/Debugger/HeatMapRangeBarCommands.h"
#include "Ui/Debugger/ToolbarLabelEntry.h"
#include "Ui/Debugger/ToolbarCheckEntry.h"
#include "Ui/Debugger/UndoBarCommands.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::PlaceOpeningFocus
//
//  When the window opens, the keys go back to the pane that had them when it
//  closed: its tab comes forward and its main control takes the focus. With
//  none saved, or one that no longer shows, the console comes forward and its
//  command line takes the focus. If the console is closed or slid away at an
//  edge, they go to the first pane's main control in the tab order: a list, a
//  memory view or a text view, never a text box in a pane's toolbar.
//
//  A saved pane that opens only after the machine's first snapshot -- a
//  second disassembly view, a device panel -- takes the focus when it shows,
//  if the user has not clicked or typed in the window by then.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::PlaceOpeningFocus()
{
    std::wstring  saved   = GetSavedFocusPane();
    bool          isReady = IsFocusPaneReady (saved);



    if (!saved.empty() && !isReady && !m_floating->Contains (saved))
    {
        m_openingFocus.Defer();
    }

    FocusPane (isReady ? saved : std::wstring (DebuggerLayout::kConsole));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::PlaceLateFocus
//
//  After each snapshot while the opening focus waits on a saved pane that had
//  not opened yet.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::PlaceLateFocus()
{
    std::wstring                  saved;
    OpeningFocusDeferral::Action  action = OpeningFocusDeferral::Action::None;



    if (m_openingFocus.IsPending())
    {
        saved  = GetSavedFocusPane();
        action = m_openingFocus.OnSnapshot (IsFocusPaneReady (saved), IsRestoringViews());
    }

    if (action == OpeningFocusDeferral::Action::Place)
    {
        FocusPane (saved);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetSavedFocusPane
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DebuggerWindow::GetSavedFocusPane() const
{
    return (m_host != nullptr) ? SourcePathList::Utf8ToWide (m_host->GetDebuggerFocusedPane()) : std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::IsFocusPaneReady
//
//  A docked pane on screen with a control to take the keys.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::IsFocusPaneReady (const std::wstring & pane) const
{
    return !pane.empty() && !m_floating->Contains (pane) && IsPaneShown (pane) && GetMainControl (pane) != nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::FocusPane
//
//  The pane's tab comes forward and its main control takes the focus, or the
//  first main control in the tab order when that one cannot.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::FocusPane (const std::wstring & pane)
{
    IDxuiControl  * target = nullptr;
    IDxuiControl  * each   = nullptr;
    bool            isMain = false;
    size_t          count  = 0;



    if (IsPaneShown (pane) && !m_dockSite->GetPaneLayout().IsAutoHidden (pane))
    {
        m_dockSite->ActivatePane (pane);
        LayoutWidgets();
    }

    m_focusMgr.Rebuild();
    count = m_focusMgr.GetTabOrderCount();

    target = GetMainControl (pane);
    target = (target != nullptr && target->IsVisible() && IsRoutable (target)) ? target : nullptr;

    for (size_t i = 0; i < count && target == nullptr; i++)
    {
        each   = m_focusMgr.GetTabOrderAt (i);
        isMain = dynamic_cast<DxuiListView *> (each) != nullptr || dynamic_cast<DxuiHexView *> (each) != nullptr || dynamic_cast<DxuiTextView *> (each) != nullptr;
        target = (isMain && each->IsVisible()) ? each : nullptr;
    }

    if (target != nullptr)
    {
        m_focusMgr.SetFocused (target);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetMainControl
//
//  The control in a pane that takes the keys when the pane is given the
//  focus: the console's command line, or else the pane's list, memory view or
//  text view. A pane with none of those gives its last control; an unknown
//  pane gives none.
//
////////////////////////////////////////////////////////////////////////////////

IDxuiControl * DebuggerWindow::GetMainControl (const std::wstring & pane) const
{
    std::vector<IDxuiControl *>  controls = GetPaneControls (pane);



    if (pane == DebuggerLayout::kConsole)
    {
        return m_consolePane->GetCommandLine().GetBox();
    }

    for (IDxuiControl * each : controls)
    {
        if (dynamic_cast<DxuiListView *> (each) != nullptr || dynamic_cast<DxuiHexView *> (each) != nullptr || dynamic_cast<DxuiTextView *> (each) != nullptr)
        {
            return each;
        }
    }

    return controls.empty() ? nullptr : controls.back();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::SaveFocusedPane
//
//  The pane holding the focused control, kept so the window gives it the
//  keys when it opens again. A control in no pane, the toolbar's, saves none.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::SaveFocusedPane()
{
    std::wstring  pane = GetPaneOfControl (m_focusMgr.GetFocusedControl());



    if (m_host != nullptr)
    {
        m_host->SetDebuggerFocusedPane (SourcePathList::WideToUtf8 (pane));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetPaneHost
//
//  The window a pane's controls are in: its floating window, or this one.
//
////////////////////////////////////////////////////////////////////////////////

DxuiWindow * DebuggerWindow::GetPaneHost (const std::wstring & pane)
{
    DxuiDockedWindow  * window = m_floating->Find (pane);



    return (window != nullptr) ? (DxuiWindow *) window : (DxuiWindow *) this;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetMenuHost
//
//  Where a menu opened by the event being routed shows: the floating window
//  the event came from, or this one.
//
////////////////////////////////////////////////////////////////////////////////

DxuiHwndSource * DebuggerWindow::GetMenuHost() const
{
    DxuiHwndSource  * host = m_floating->GetRoutedPopupHost();



    return (host != nullptr) ? host : GetPopupHost();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetBarRoutingPane
//
//  The routing pane under which a pane's bar takes the mouse: the pane's own
//  while it floats, and none while it is in this window.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DebuggerWindow::GetBarRoutingPane (const std::wstring & pane) const
{
    return m_floating->GetBarRoutingPane (pane);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ClipPaneControls
//
//  Each pane's controls paint inside the pane, so nothing a control draws
//  past its edge lands on a neighbor. The pane's info button is left out:
//  it is in the pane's title bar, outside the body, and PaneColorKeys::Place
//  clips it to its own place.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ClipPaneControls()
{
    for (const std::wstring & pane : DebuggerLayout::GetPaneIds())
    {
        IDxuiControl  * content = GetPaneContent (pane);
        RECT            clip    = {};



        if (content == nullptr)
        {
            continue;
        }

        clip = content->GetBounds();

        SetChildClip (content, clip);

        for (IDxuiControl * control : GetOwnControls (pane))
        {
            SetChildClip (control, clip);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetPaneMinimum
//
//  A framed pane's least height: a tool window's title bar, its tab band
//  and the lines round them, over the least height the frame's parts fit in
//  at its width. A pane of one control has no minimum.
//
////////////////////////////////////////////////////////////////////////////////

SIZE DebuggerWindow::GetPaneMinimum (const std::wstring & pane) const
{
    const DebuggerPaneFrame  * frame  = dynamic_cast<const DebuggerPaneFrame *> (m_dockSite->GetPaneContent (pane));
    RECT                       bounds = {};
    long                       chrome = 0;



    if (frame == nullptr)
    {
        return SIZE {};
    }

    bounds = frame->GetBounds();
    chrome = m_scaler.ToPx (DxuiTabGroup::kTitleDip + DxuiTabGroup::kStripDip) + 2 * DxuiPaneMetrics::GetLinePx (m_scaler);

    return SIZE { 0, chrome + frame->GetMinimumHeightPx ((int) (bounds.right - bounds.left), m_scaler) };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ReadSavedLayout
//
//  The saved layout text, with the closed panes read into m_closedPanes from
//  their own setting. A layout an older build saved carries them on a line
//  ahead of the text instead; that line is moved to the setting once.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DebuggerWindow::ReadSavedLayout()
{
    std::wstring            savedText;
    std::wstring            layoutText;
    std::set<std::wstring>  legacy;
    bool                    hasHeatMapOptions = false;



    m_closedPanes.clear();

    if (m_host == nullptr)
    {
        return {};
    }

    savedText  = SourcePathList::Utf8ToWide (m_host->GetDebuggerLayout());
    layoutText = DebuggerLayout::TakeClosedPanes (savedText, legacy);

    DebuggerLayout::ReadClosedPanes (SourcePathList::Utf8ToWide (m_host->GetDebuggerClosedPanes()), m_closedPanes);

    //  The heat map opens by default only in the default layout. A layout
    //  saved before it did so had it closed, and no build that opened it by
    //  default has yet saved its options.
    hasHeatMapOptions = !m_host->GetDebuggerHeatMapOptions().empty();

    if (!layoutText.empty() && !hasHeatMapOptions)
    {
        m_closedPanes.insert (DebuggerLayout::kHeatMap);
        m_host->SetDebuggerClosedPanes (SourcePathList::WideToUtf8 (DebuggerLayout::ClosedPanesToText (m_closedPanes)));
    }


    m_barHost.SetDock      (DxuiToolbarDock::FromText (SourcePathList::Utf8ToWide (m_host->GetDebuggerCommandBarDock())));
    m_timelineHost.SetDock (DxuiToolbarDock::FromText (SourcePathList::Utf8ToWide (m_host->GetDebuggerTimelineDock())));

    if (layoutText != savedText)
    {
        m_closedPanes.insert (legacy.begin(), legacy.end());

        m_host->SetDebuggerClosedPanes (SourcePathList::WideToUtf8 (DebuggerLayout::ClosedPanesToText (m_closedPanes)));
        m_host->SetDebuggerLayout      (SourcePathList::WideToUtf8 (layoutText));
    }

    return layoutText;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ConfigureDockSite
//
//  Every pane goes to the site under its layout id. The source pane and the
//  console are frames over several controls; the rest are one control each.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ConfigureDockSite()
{
    //  A pane's toolbar is a tool window's compact strip, as Visual Studio's.
    constexpr int   kMemoryBarDip = DxuiToolbar::kCompactBandDp;
    auto            boxHeight     = [] (int, const DxuiDpiScaler & scaler) { return scaler.ToPx (30); };
    auto            barHeight     = [] (int, const DxuiDpiScaler & scaler) { return scaler.ToPx (kMemoryBarDip); };
    auto            bandHeight    = [] (int, const DxuiDpiScaler & scaler) { return scaler.ToPx (HistoryBand::GetHeightDip (kPaneRowDip)); };
    std::wstring    savedText;
    DxuiPaneLayout  restored;
    InputFilter     dragFilter;



    for (const std::unique_ptr<IDebuggerPane> & pane : m_panes)
    {
        pane->AddToDock (*m_dockSite);
    }

    m_dockSite->SetShownFn    ([this] (const std::wstring & pane) { return IsPaneShown (pane); });

    //  The site lays a pane out no shorter than its bar and bands, so a sash
    //  dragged over it stops before its controls run past its edge.
    m_dockSite->SetMinSizeFn  ([this] (const std::wstring & pane) { return GetPaneMinimum (pane); });

    //  Source and Disassembly are documents, the rest tool windows, each with
    //  a title bar whose menu is the pane's Dock To menu (FR-084).
    m_dockSite->SetDocumentFn  ([this] (const std::wstring & pane) { return IsDocumentPane (pane); });
    m_dockSite->SetOnPaneMenu  ([this] (const std::wstring & pane, POINT clientPx) { ShowDockToMenu (pane, clientPx); });
    m_dockSite->SetOnClosePane ([this] (const std::wstring & pane) { ClosePane (pane); },
                                [this] (const std::wstring & pane) { return CanClosePane (pane); });

    //  A pane slid out from an edge lies over the others, so its controls are
    //  painted above the page.
    m_dockSite->SetOnSlid ([this] (const std::wstring & pane)
    {
        m_slidPane = pane;
        UpdateTopLayer();
    });
    savedText = ReadSavedLayout();
    std::erase_if (m_closedPanes, [this] (const std::wstring & pane) { return !IsFixedPane (pane); });

    restored = DebuggerLayout::Restore (savedText);
    restored.PlaceOnMonitors (GetMonitors());
    m_dockSite->SetPaneLayout (restored);
    m_floating->MarkSyncNeeded();

    m_dockSite->SetOnFloatRequested ([this] (const std::wstring & pane, POINT clientPx) { m_floating->RequestFloat (pane, clientPx); });
    m_dockSite->SetOnTearOff        ([this] (const std::wstring & pane, POINT clientPx) { m_floating->TearOff      (pane, clientPx); });

    //  Every change the user makes is saved as it happens, so a crash or a
    //  closed emulator loses nothing.
    m_dockSite->SetOnChanged ([this]
    {
        m_floating->MarkSyncNeeded();
        SaveLayout();
    });

    //  A pane drag is canceled when the window loses the mouse with the
    //  button still down. The window releases the capture itself as the
    //  button comes up, with the button already up, which is no loss.
    dragFilter.onMouseLost = [this] (UINT)
    {
        m_dockSite->OnDragMouseLost ((GetKeyState (VK_LBUTTON) & 0x8000) != 0);
    };

    AddInputFilter (m_dockSite, std::move (dragFilter));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::IsPaneShown
//
//  The source pane shows while a debug file is loaded, a memory window while
//  it is open, a device panel while its device is present and its panel open
//  (FR-044), and the heat map's ranges once they have been opened; the rest
//  show until their close buttons close them.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::IsPaneShown (const std::wstring & pane) const
{
    bool  isOpened = false;



    if (m_closedPanes.contains (pane))
    {
        return false;
    }

    if (FindPane (pane) != nullptr && FindPane (pane)->TryGetOpened (isOpened))
    {
        return isOpened;
    }

    if (DebuggerLayout::IsOpenedLater (pane))
    {
        return m_basicOpened.contains (pane);
    }


    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::PaintTopLayer
//
//  The slid-out pane over the page: its background, its controls, then its
//  title bar and outline; then the status bar's zoom popup over all of it.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::PaintTopLayer (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    //  Under a slid-out pane, which lies over the disassembly and the source.
    for (int view = 0; view < DebuggerViewState::kMaxCodeViews + SourceDocuments::kMaxDocuments; view++)
    {
        PaintBranchArrow (painter, view);
    }

    m_dockSite->PaintSlidUnder (painter, theme);
    DxuiWindow::PaintTopLayer  (painter, text, theme);
    m_dockSite->PaintSlidOver  (painter, text, theme);
    PaintZoomPopup             (painter, text, theme);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::HasTopLayer
//
//  A slid-out pane, the status bar's zoom popup, or the PC on a branch with
//  its arrow to draw. The top
//  layer is a second flush of the frame, so it is asked for only then.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::HasTopLayer() const
{
    if (DxuiWindow::HasTopLayer() || m_zoomOpen)
    {
        return true;
    }

    for (int view = 0; view < DebuggerViewState::kMaxCodeViews; view++)
    {
        for (const DebuggerViewSnapshot::CodeLine & line : GetCodeLines (view))
        {
            //  The PC's branch, or the one a step just took to the PC, which
            //  can be out of the lines built for the view.
            if (m_code->IsOpen (view) && line.isCurrent && (line.target.has_value() || m_snapshot->branchedFrom.has_value()))
            {
                return true;
            }
        }
    }

    for (int slot = 0; slot < SourceDocuments::kMaxDocuments; slot++)
    {
        const SourceDocumentPane  & document = m_sources->GetDocument (slot);

        if (document.IsShown() && document.GetModel().IsActive() && m_snapshot != nullptr &&
            m_snapshot->source.has_value() && m_snapshot->source->pcTarget.has_value())
        {
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::PaintDragLayer
//
//  A pane drag's drop preview and guides, over every pane and its pictures,
//  the heat map's included, and over the top layer.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::PaintDragLayer (
    IDxuiPainter       & painter,
    IDxuiTextRenderer  & text,
    const IDxuiTheme   & theme)
{
    if (m_dockSite != nullptr)
    {
        m_dockSite->PaintDragLayer (painter, text, theme);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::HasDragLayer
//
//  While a pane is dragged in this window and its marks are not drawn in the
//  overlay above the floating windows.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::HasDragLayer() const
{
    return m_dockSite != nullptr && m_dockSite->HasDragLayer();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::IsDocumentPane
//
//  The Disassembly views and the source documents are documents; every
//  other pane is a tool window (FR-084).
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::IsDocumentPane (const std::wstring & pane) const
{
    for (int view = 0; view < DebuggerViewState::kMaxCodeViews; view++)
    {
        if (pane == DebuggerLayout::GetCodePaneId (view))
        {
            return true;
        }
    }

    return GetSourceSlotOf (pane) >= 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::CanClosePane
//
//  Every pane can close, since the View menu opens any of them again.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::CanClosePane (const std::wstring &) const
{
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::IsFixedPane
//
//  A pane the window always holds: the first Disassembly view, the first
//  memory window and the tool windows. Closing one only hides it; the
//  others close for real and open again from the snapshot.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::IsFixedPane (const std::wstring & pane) const
{
    std::string  diagnosticsId;



    for (int view = 1; view < DebuggerViewState::kMaxCodeViews; view++)
    {
        if (pane == DebuggerLayout::GetCodePaneId (view))
        {
            return false;
        }
    }

    if (FindPane (pane) != nullptr && !FindPane (pane)->IsFixed())
    {
        return false;
    }

    return GetSourceSlotOf (pane) < 0 && !DebuggerLayout::TryGetDiagnosticsId (pane, diagnosticsId);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetViewMenuPanes
//
//  What the View menu lists, in its order.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> DebuggerWindow::GetViewMenuPanes() const
{
    return ViewMenuRows::GetPanes (m_panes, m_snapshot.get());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ShowPane
//
//  A View menu choice: a closed pane opens where the layout still keeps it,
//  docked or floating, and the pane comes forward. A pane the snapshot opens
//  comes forward once the snapshot shows it.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ShowPane (const std::wstring & pane)
{
    //  A pane turned on opens docked and shown, not slid in at an edge.
    if (!IsPaneShown (pane) && m_dockSite->GetPaneLayout().IsAutoHidden (pane))
    {
        (void) m_dockSite->EditPaneLayout().DockBack (pane);
    }

    m_closedPanes.erase (pane);

    //  A pane closed until first shown, such as the heat map's ranges, is
    //  opened from then on.
    if (FindPane (pane) != nullptr)
    {
        FindPane (pane)->Open();
    }

    //  A BASIC or tool pane is added to the open views the same way.
    if (DebuggerLayout::IsOpenedLater (pane))
    {
        m_basicOpened.insert (pane);
    }

    if (!IsPaneShown (pane))
    {
        if (FindPane (pane) != nullptr)
        {
            (void) FindPane (pane)->RequestOpen();
        }
    }

    m_floating->MarkSyncNeeded();
    m_dockSite->Relayout();
    m_pendingShowPane = pane;
    ShowPendingPane();
    SetWindowMenus();
    SaveLayout();
    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ShowPendingPane
//
//  The pane the View menu chose comes forward once it shows: its tab is
//  selected, an auto-hidden one slides out, and a floating one's window is
//  raised.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ShowPendingPane()
{
    std::wstring  pane = m_pendingShowPane;



    if (pane.empty() || !IsPaneShown (pane))
    {
        return;
    }

    m_pendingShowPane.clear();
    m_dockSite->ActivatePane (pane);

    m_floating->Show (pane);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ClosePane
//
//  A close button on a document tab or a tool window's title bar, carried
//  out as the pane's own Close menu item would. A fixed pane hides, keeping
//  its place in the layout, until the View menu shows it again.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ClosePane (const std::wstring & pane)
{
    int  slot = GetSourceSlotOf (pane);



    if (IsFixedPane (pane))
    {
        m_closedPanes.insert (pane);
        m_floating->MarkSyncNeeded();
        m_dockSite->Relayout();
        SetWindowMenus();
        SaveLayout();
        Invalidate();
        return;
    }

    if (slot >= 0)
    {
        m_sources->Close (slot);
        return;
    }

    if (m_host == nullptr)
    {
        return;
    }

    if (FindPane (pane) != nullptr && FindPane (pane)->RequestClose())
    {
        return;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ClosePaneOfFocus
//
//  Closes the pane holding the focus, when there is one and it can close.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::ClosePaneOfFocus()
{
    std::wstring  pane = GetPaneOfFocus();



    if (pane.empty() || !CanClosePane (pane))
    {
        return false;
    }

    ClosePane (pane);
    Invalidate();
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetPaneOfFocus
//
//  The pane holding the focused control, including the parts of a frame.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DebuggerWindow::GetPaneOfFocus() const
{
    IDxuiControl  * focused = GetFocused();
    std::wstring    owner;



    if (!m_floating->GetRoutingPane().empty())
    {
        return m_floating->GetRoutingPane();
    }

    if (focused == nullptr)
    {
        return L"";
    }

    if (focused == m_consolePane->GetOutput().GetView() || focused == m_consolePane->GetCommandLine().GetBox() || GetPaneOfControl (focused) == DebuggerLayout::kConsole)
    {
        return DebuggerLayout::kConsole;
    }

    //  A pane docked as a frame -- a disassembly view, the call stack, a
    //  memory window -- holds its focusable controls inside the frame, so
    //  the dock site, which knows only the frame, cannot place them.
    owner = GetPaneOfControl (focused);

    if (!owner.empty())
    {
        return owner;
    }

    return m_dockSite->GetPaneOf (focused);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetPaneControls
//
//  The window's children that make up a pane, which move together when it
//  floats.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<IDxuiControl *> DebuggerWindow::GetPaneControls (const std::wstring & pane) const
{
    std::vector<IDxuiControl *>  controls = GetOwnControls (pane);
    ColorKeyButton             * key      = GetColorKey (pane);



    if (key != nullptr && !controls.empty())
    {
        controls.push_back (key);
    }

    return controls;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetOwnControls
//
//  A pane's controls less its info button.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<IDxuiControl *> DebuggerWindow::GetOwnControls (const std::wstring & pane) const
{

    if (FindPane (pane) != nullptr)
    {
        return FindPane (pane)->GetControls();
    }

    return {};
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetPaneContent
//
//  The one control a dock site places for a pane: its frame, or the pane's
//  only control.
//
////////////////////////////////////////////////////////////////////////////////

IDxuiControl * DebuggerWindow::GetPaneContent (const std::wstring & pane) const
{
    std::vector<IDxuiControl *>  controls;



    if (FindPane (pane) != nullptr)
    {
        return FindPane (pane)->GetContent();
    }

    controls = GetPaneControls (pane);
    return controls.empty() ? nullptr : controls.front();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetPaneTitle
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DebuggerWindow::GetPaneTitle (const std::wstring & pane) const
{
    if (pane == DebuggerLayout::kCode)
    {
        return L"Disassembly";
    }

    return (FindPane (pane) != nullptr) ? FindPane (pane)->GetTitle() : pane;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetMonitors
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiPaneLayout::Monitor> DebuggerWindow::GetMonitors()
{
    std::vector<DxuiPaneLayout::Monitor>  monitors;



    EnumDisplayMonitors (nullptr, nullptr,
        [] (HMONITOR monitor, HDC, LPRECT, LPARAM data) -> BOOL
        {
            MONITORINFOEXW            info = {};
            DxuiPaneLayout::Monitor   entry;

            info.cbSize = sizeof (info);

            if (GetMonitorInfoW (monitor, &info))
            {
                entry.key       = info.szDevice;
                entry.workDip   = info.rcWork;
                entry.isPrimary = (info.dwFlags & MONITORINFOF_PRIMARY) != 0;
                reinterpret_cast<std::vector<DxuiPaneLayout::Monitor> *> (data)->push_back (entry);
            }

            return TRUE;
        },
        reinterpret_cast<LPARAM> (&monitors));

    return monitors;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::OnFloatsSynced
//
//  The floating windows match the layout again: the focus moves to a
//  control that an event can be routed to.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::OnFloatsSynced()
{
    IDxuiControl  * focused = nullptr;



    m_dockSite->Relayout();
    m_focusMgr.Rebuild();

    focused = m_focusMgr.GetFocusedControl();

    if (focused == nullptr || !IsRoutable (focused))
    {
        m_focusMgr.SetFocused (IsRoutable (m_consolePane->GetCommandLine().GetBox()) ? (IDxuiControl *) m_consolePane->GetCommandLine().GetBox() : m_code->GetCodeList (0));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::OnFloatFocusChanged
//
//  The accent is drawn on the window that got the focus last, so it remains
//  on a float when another application gets the focus.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::OnFloatFocusChanged (const std::wstring & pane, bool focused)
{
    m_accentOwner.OnFloatFocusChanged (pane, focused);
    ShowFocusAccent();
    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::OnWindowFocusChanged
//
//  The window, main or floating, that took the keyboard focus last shows the
//  accent (see FocusAccentOwner): this one taking it takes the accent back
//  from a floating pane. Losing it to another application changes nothing.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::OnWindowFocusChanged (bool focused)
{
    //  Input in another window comes next, so the console did not receive
    //  the last of it; a click or key routed back from a floating console
    //  marks it again.
    if (!focused)
    {
        m_stopFocus.OnInput (false);

        //  The pointer can move off the timeline while another window is
        //  active, and the strip then gets no OnMouseLeave, so it is called
        //  here.
        m_timelineStrip.OnMouseLeave();
    }

    m_accentOwner.OnMainFocusChanged (focused);
    ShowFocusAccent();
    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ShowFocusAccent
//
//  Of this window and the floating ones, the one that took the focus last
//  shows the accent. The focus messages run this as well as each frame, so
//  the window losing the accent repaints without it in the same message
//  rather than at its next frame paint, up to kIdlePaintMs later.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ShowFocusAccent()
{
    if (m_dockSite == nullptr || m_theme == nullptr)
    {
        return;
    }

    m_dockSite->SetFocusedPane (m_accentOwner.GetFocusedFloat().empty() ? GetPaneOfFocus() : std::wstring());

    m_floating->SetFocusedLooks (m_accentOwner.GetFocusedFloat(), *m_theme);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::CloseFloatingPane
//
//  The pane closes, keeping its floating place, so the View menu opens it
//  there again.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::CloseFloatingPane (const std::wstring & pane)
{
    ClosePane (pane);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetPaneLayout
//
////////////////////////////////////////////////////////////////////////////////

const DxuiPaneLayout & DebuggerWindow::GetPaneLayout() const
{
    return m_dockSite->GetPaneLayout();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::EditPaneLayout
//
////////////////////////////////////////////////////////////////////////////////

DxuiPaneLayout & DebuggerWindow::EditPaneLayout()
{
    return m_dockSite->EditPaneLayout();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::OnFloatDocked
//
//  A pane's floating window is about to go, its controls back in this one.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::OnFloatDocked (const std::wstring & pane, DxuiDockedWindow * window)
{
    m_accentOwner.OnFloatDocked (pane);

    //  And an operand tip up in it.
    m_operandTips->OnPopupHostClosed (window->GetPopupHost());

    //  A control several panes share, as the memory bar is, may be sitting
    //  in this pane, and would go with the window.
    for (const std::unique_ptr<IDebuggerPane> & each : m_panes)
    {
        each->OnFloatClosed (window);
    }

    //  And each find widget, in the pane it searches.
    m_finds->OnFloatClosed (window);

    //  The site's strips and drop zones are drawn over the panes, so the site
    //  is the last child.
    {
        std::unique_ptr<IDxuiControl>  site = DetachChild (m_dockSite);

        (void) AttachChild (std::move (site));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::SaveLayout
//
//  Each floating window's place is read back first, since moving or sizing
//  one is not a layout operation.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::SaveLayout()
{
    DxuiPaneLayout &  layout = m_dockSite->EditPaneLayout();



    m_floating->SaveRects (layout);

    if (m_host != nullptr)
    {
        m_host->SetDebuggerLayout      (SourcePathList::WideToUtf8 (layout.ToText()));
        m_host->SetDebuggerClosedPanes (SourcePathList::WideToUtf8 (DebuggerLayout::ClosedPanesToText (m_closedPanes)));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ApplySavedPlacement
//
//  The window opens where the user last left it on this monitor arrangement.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ApplySavedPlacement()
{
    RECT  visible = {};
    RECT  placed  = {};



    if (m_host == nullptr || !m_host->TryGetDebuggerPlacement (visible))
    {
        WindowTrace::Log ("restore.miss", "debugger", "nothing saved for this monitor arrangement");
        return;
    }

    //  What was saved is the frame the user sees. The window rect around it
    //  is this window's own invisible border wider, and only this window can
    //  say how wide that is.
    placed = DxuiWindowFrame::ToWindowRect (GetHwnd(), visible);

    WindowTrace::LogRect ("restore.hit", "debugger", visible, "the visible rect that was saved");

    //  TWICE, ON PURPOSE. Landing on a monitor of a different scale makes
    //  Windows send WM_DPICHANGED and resize the window by the ratio of the
    //  two: a rect saved at 120dpi came back on a 144dpi screen exactly 1.2x
    //  too big. The first call takes that rescale, and the border is measured
    //  again afterwards because it scales with the window.

    SetWindowPos (GetHwnd(), nullptr, placed.left, placed.top,
                  placed.right - placed.left, placed.bottom - placed.top,
                  SWP_NOZORDER | SWP_NOACTIVATE);

    placed = DxuiWindowFrame::ToWindowRect (GetHwnd(), visible);

    SetWindowPos (GetHwnd(), nullptr, placed.left, placed.top,
                  placed.right - placed.left, placed.bottom - placed.top,
                  SWP_NOZORDER | SWP_NOACTIVATE);

    WindowTrace::LogWindow ("restore.applied", "debugger", GetHwnd(), "window rect after placing");
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::OnWindowPlaced
//
//  The user finished dragging or sizing the window, so where it is now is
//  where it should open next time. A close catches the moves that never
//  reach this hook.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::OnWindowPlaced()
{
    SavePlacementIfMoved();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::SavePlacementIfMoved
//
//  A placement is written only when the window is somewhere other than where
//  it opened. That keeps the rule the user asked for -- two instances fight
//  over the file only if both were moved -- and it holds for the ways a
//  window moves WITHOUT the OS drag loop, which is the only thing
//  OnWindowPlaced can see: a snap from the keyboard, or an arrangement made
//  and then closed.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::SavePlacementIfMoved()
{
    RECT  rect = {};



    //  Until the window has been shown and its opening rect taken, the size
    //  and move events of its own creation have nothing to compare against.
    //  A minimized window is parked far off the desktop -- x=-32000 -- and
    //  that is not a placement anyone chose. The log caught one being
    //  written as the arrangement for the whole monitor set.
    if (!m_placed || m_host == nullptr || !IsCreated() || IsIconic (GetHwnd()))
    {
        WindowTrace::Log ("save.skipped", "debugger", "not placed yet, or no host");
        return;
    }

    rect = DxuiWindowFrame::GetVisibleRect (GetHwnd());

    if (!WindowPlacementProfile::IsPlaceableRect (rect))
    {
        WindowTrace::LogRect ("save.refused", "debugger", rect, "not a rect anyone put a window at");
        return;
    }

    if (EqualRect (&rect, &m_openedRect))
    {
        WindowTrace::LogRect ("save.unmoved", "debugger", rect, "where it opened, so the file is left alone");
        return;
    }

    WindowTrace::LogRect ("save", "debugger", rect, "opened at x=" + std::to_string (m_openedRect.left) +
                          " y=" + std::to_string (m_openedRect.top));

    m_openedRect = rect;
    m_host->SetDebuggerPlacement (rect);
}
