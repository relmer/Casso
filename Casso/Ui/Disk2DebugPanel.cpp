#include "Pch.h"

#include "Disk2DebugPanel.h"

#include "Chrome/CassoTheme.h"

#include "../DebugDialogProjection.h"


static constexpr LPCWSTR  s_kpszClassName  = L"Casso.Disk2Debug.Panel";
static constexpr LPCWSTR  s_kpszWindowTitle = L"Casso - Disk ][ debug";

static constexpr int      s_kPreferredWidthDip  = 960;
static constexpr int      s_kPreferredHeightDip = 600;

static constexpr LPCWSTR  s_kpszTrackFilterLabel  = L"Track:";
static constexpr LPCWSTR  s_kpszSectorFilterLabel = L"Sector:";
static constexpr LPCWSTR  s_kpszTrackQtFilterLabel = L"Quarter-track:";

static constexpr LPCWSTR  s_kpszEventCheckLabels[kEventTypeCheckCount] =
{
    L"Motor", L"HeadStep", L"HeadBump", L"AddrMark",
    L"Read",  L"Write",    L"Door",     L"DriveSel",
};

static constexpr LPCWSTR  s_kpszAudioSubLabels[kAudioSubCheckCount] =
{
    L"Started", L"Restarted", L"Continued", L"Silent",
};

static constexpr LPCWSTR  s_kpszDriveOptionLabels[kDriveRadioCount] =
{
    L"All", L"Drive 1", L"Drive 2",
};

static constexpr LPCWSTR  s_kpszRawQtLabel    = L"Quarter-track steps";
static constexpr LPCWSTR  s_kpszPauseLabel    = L"Pause";
static constexpr LPCWSTR  s_kpszResumeLabel   = L"Resume";
static constexpr LPCWSTR  s_kpszClearLabel    = L"Clear";
static constexpr LPCWSTR  s_kpszAudioLabel    = L"All";
static constexpr LPCWSTR  s_kpszInvalidLabel  = L"Invalid";
static constexpr LPCWSTR  s_kpszTrackInvalidPrefix  = L"Invalid track: ";
static constexpr LPCWSTR  s_kpszSectorInvalidPrefix = L"Invalid sector: ";
static constexpr LPCWSTR  s_kpszDriveFilterLabel    = L"Drive:";
static constexpr LPCWSTR  s_kpszDiskEventsLabel     = L"Disk events:";
static constexpr LPCWSTR  s_kpszAudioEventsLabel    = L"Audio events:";

static constexpr LPCWSTR  s_kpszEventCheckTips[kEventTypeCheckCount] =
{
    L"Motor spin-up / spin-down transitions",
    L"Stepper head moves between tracks",
    L"Head bumps against track 0 stop",
    L"Address-field reads (track / sector / volume)",
    L"Data-field sector reads",
    L"Data-field sector writes",
    L"Disk-inserted / disk-ejected events",
    L"Soft-switch drive selection (Drive 1 vs Drive 2)",
};

static constexpr LPCWSTR  s_kpszAudioSubTips[kAudioSubCheckCount] =
{
    L"Audio loop started",
    L"Audio loop restarted with new parameters",
    L"Audio loop continued without retrigger",
    L"Audio loop silenced (with reason)",
};

static constexpr LPCWSTR  s_kpszDriveRadioTips[kDriveRadioCount] =
{
    L"Show events from all drives",
    L"Show only events targeting Drive 1",
    L"Show only events targeting Drive 2",
};

static constexpr LPCWSTR  s_kpszAudioMasterTip = L"Master toggle for all audio-event categories below";
static constexpr LPCWSTR  s_kpszRawQtTip       = L"Show every quarter-track head step (verbose)";
static constexpr LPCWSTR  s_kpszTrackEditTip   = L"Filter rows to a single track (blank = all)";
static constexpr LPCWSTR  s_kpszSectorEditTip  = L"Filter rows to a single sector (blank = all)";





////////////////////////////////////////////////////////////////////////////////
//
//  ArgbToFloat4
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::ArgbToFloat4 (uint32_t argb, float (& outRgba)[4]) noexcept
{
    outRgba[0] = (float) ((argb >> 16) & 0xFFu) / 255.0f;
    outRgba[1] = (float) ((argb >>  8) & 0xFFu) / 255.0f;
    outRgba[2] = (float) ((argb      ) & 0xFFu) / 255.0f;
    outRgba[3] = (float) ((argb >> 24) & 0xFFu) / 255.0f;
}



// Builds the "Invalid track: tok1, tok2" detail label by slicing
// the rejected UTF-16 spans out of the original expression. If the
// edit parsed cleanly, returns an empty string. Defensive about
// bad spans so an out-of-range index can't crash the dialog.
std::wstring Disk2DebugPanel::BuildInvalidLabel (
    LPCWSTR                                                  prefix,
    const std::wstring                                     & expr,
    const std::vector<TrackSectorPredicate::RejectedSpan> & spans)
{
    std::wstring  result;
    size_t        i        = 0;
    int           beginIdx = 0;
    int           endIdx   = 0;

    // No rejected spans means the edit parsed cleanly, so there is no label
    // to build and `result` stays empty.
    if (!spans.empty())
    {
        result = prefix;

        for (i = 0; i < spans.size(); ++i)
        {
            beginIdx = spans[i].beginUtf16;
            endIdx   = spans[i].endUtf16;

            if (beginIdx < 0)                 { beginIdx = 0; }
            if (endIdx > (int) expr.size())   { endIdx   = (int) expr.size(); }
            if (endIdx <= beginIdx)           { continue; }
            if (i > 0)                        { result += L", "; }

            result.append (expr, (size_t) beginIdx, (size_t) (endIdx - beginIdx));
        }
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2DebugPanel
//
////////////////////////////////////////////////////////////////////////////////

Disk2DebugPanel::Disk2DebugPanel()
{
    // Content widgets are created as children of this panel in OnCreate
    // (which fires inside DxuiWindow::Create once the backend exists) so
    // the base paint pump walks and paints them. The constructor only
    // seeds the Uptime anchor; every other member default-initializes.
    m_uptimeAnchor = std::chrono::steady_clock::now();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ~Disk2DebugPanel
//
////////////////////////////////////////////////////////////////////////////////

Disk2DebugPanel::~Disk2DebugPanel()
{
    Destroy();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Create
//
//  Stands up the DxuiWindow backend (close-only caption, host-owned swap
//  chain / paint pump) sized to the panel's preferred client size. The
//  OnCreate hook fires inside DxuiWindow::Create to populate the child
//  widgets before the first layout. Idempotent -- a second call while
//  already open is a no-op.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Disk2DebugPanel::Create (
    HINSTANCE              hInstance,
    HWND                   hwndOwner,
    ID3D11Device         * device,
    ID3D11DeviceContext  * context,
    const CassoTheme    * theme)
{
    HRESULT                    hr     = S_OK;
    DxuiWindow::CreateParams   params;



    UNREFERENCED_PARAMETER (device);
    UNREFERENCED_PARAMETER (context);

    BAIL_OUT_IF (IsCreated(), S_OK);

    m_theme = theme;

    params.title             = s_kpszWindowTitle;
    params.hInstance         = hInstance;
    params.ownerHwnd         = hwndOwner;
    params.initialSizeDip    = { s_kPreferredWidthDip, s_kPreferredHeightDip };
    params.minSizeDip        = { s_kPreferredWidthDip, s_kPreferredHeightDip };
    params.resizable         = true;
    params.insetContentBelowCaption = false;
    params.captionStyle      = DxuiCaptionStyle::CloseOnly;
    params.classNameOverride = s_kpszClassName;

    hr = DxuiWindow::Create (params);
    CHR (hr);

    SetTheme (m_theme);
    Show();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnCreate
//
//  DxuiWindow hook fired from Create() once the backend + HWND exist.
//  Builds every content widget as a child of this panel via the
//  inherited Create<T> factory (so the base paint pump walks and paints
//  them), then wires initial state / callbacks and the popup + focus
//  helpers.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::OnCreate()
{
    m_trackFilterLabel   = CreateChild<DxuiLabel> (L"",                       DxuiTextRole::Body,  DxuiTextHAlign::Right);
    m_sectorFilterLabel  = CreateChild<DxuiLabel> (s_kpszSectorFilterLabel,   DxuiTextRole::Body,  DxuiTextHAlign::Right);
    m_driveFilterLabel   = CreateChild<DxuiLabel> (s_kpszDriveFilterLabel,    DxuiTextRole::Body,  DxuiTextHAlign::Left);
    m_diskEventsLabel    = CreateChild<DxuiLabel> (s_kpszDiskEventsLabel,     DxuiTextRole::Body,  DxuiTextHAlign::Left);
    m_audioEventsLabel   = CreateChild<DxuiLabel> (s_kpszAudioEventsLabel,    DxuiTextRole::Body,  DxuiTextHAlign::Left);
    m_trackInvalidLabel  = CreateChild<DxuiLabel> (L"",                       DxuiTextRole::Error, DxuiTextHAlign::Left);
    m_sectorInvalidLabel = CreateChild<DxuiLabel> (L"",                       DxuiTextRole::Error, DxuiTextHAlign::Left);

    for (int i = 0; i < kEventTypeCheckCount; i++)
    {
        m_eventChecks[i] = CreateChild<DxuiCheckbox> (s_kpszEventCheckLabels[i]);
    }

    m_audioMasterCheck = CreateChild<DxuiCheckbox> (s_kpszAudioLabel);
    for (int i = 0; i < kAudioSubCheckCount; i++)
    {
        m_audioSubChecks[i] = CreateChild<DxuiCheckbox> (s_kpszAudioSubLabels[i]);
    }

    m_rawQtCheck  = CreateChild<DxuiCheckbox>   (s_kpszRawQtLabel);
    m_driveRadio  = CreateChild<DxuiRadioGroup> ();
    m_trackEdit   = CreateChild<DxuiTextInput>  ();
    m_sectorEdit  = CreateChild<DxuiTextInput>  ();
    m_pauseButton = CreateChild<DxuiButton>     (s_kpszPauseLabel);
    m_clearButton = CreateChild<DxuiButton>     (s_kpszClearLabel);
    m_eventList   = CreateChild<DxuiListView>   ();

    ConfigureWidgets();
    UpdateDynamicLabels();

    m_columnMenu.SetPopupHost (PopupHost());
    m_tooltip.SetPopupHost    (PopupHost());
}





////////////////////////////////////////////////////////////////////////////////
//
//  Destroy
//
//  Tears down the DxuiWindow backend (HWND + swap chain). EmulatorShell
//  drops its unique_ptr right after, but keeping this explicit entry
//  point preserves the existing shutdown call site.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::Destroy()
{
    DestroyBackend();
}





////////////////////////////////////////////////////////////////////////////////
//
//  RenderFrame
//
//  Public per-frame entry point invoked by the EmulatorShell render
//  loop. Drains the event ring into the display rows, advances the
//  list / tooltip timers, then invalidates the window so its WM_PAINT
//  pump repaints the child widget tree.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Disk2DebugPanel::RenderFrame()
{
    HRESULT  hr  = S_OK;
    int64_t  now = NowMs();



    BAIL_OUT_IF (!IsCreated(), S_OK);

    DrainAndProject();

    // Drive scrollbar auto-repeat for any held arrow / track press and
    // the tooltip open / close dwell timers.
    m_eventList->Tick (now);
    m_tooltip.Tick    (now);

    Invalidate();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetTheme
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::SetTheme (const CassoTheme * theme)
{
    // Set the one window theme; the paint pump hands it to the child
    // widget tree (edits, list, labels) each frame, so they need no
    // per-control push. The focus manager keeps a copy only for its
    // row-height metric; the column-menu popup is themed at show time.
    m_theme = theme;
    DxuiWindow::SetTheme (theme);

    m_focusMgr.SetTheme (theme);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ForwardMouseToList
//
//  Translates a client-px mouse event into the event list's widget-local
//  space and dispatches it through DxuiListView::OnMouse, which owns all
//  scroll / thumb / column-resize / row-select routing and raises the
//  panel's selection / sort / column-resize callbacks. Returns true when
//  the list consumed the event.
//
////////////////////////////////////////////////////////////////////////////////

bool Disk2DebugPanel::ForwardMouseToList (DxuiMouseEventKind kind, DxuiMouseButton button, int x, int y, float wheelDelta)
{
    DxuiMouseEvent  ev;



    ev.kind        = kind;
    ev.button      = button;
    ev.positionDip = { x - m_layout.listView.left, y - m_layout.listView.top };
    ev.wheelDelta  = wheelDelta;

    return m_eventList->OnMouse (ev);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShowColumnMenu
//
//  Builds a popup menu item for each column with the current
//  visibility as the check state and anchors it at the click point.
//  Selection callback is wired in ConfigureWidgets and flips the
//  selected column's visibility, then re-runs layout so width / sort
//  reflect the change.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::ShowColumnMenu (int anchorX, int anchorY)
{
    std::vector<DxuiPopupMenu::Item>  items;
    IDxuiTextRenderer              *  textRenderer = TextRenderer();
    RECT                              host         = { 0, 0, m_widthPx, m_heightPx };



    // Bail rather than dereference a null renderer -- the shared text
    // renderer used to measure / lay the menu out is only available once
    // the backend exists.
    if (textRenderer == nullptr)
    {
        return;
    }

    items.reserve (m_eventList->GetColumnCount());

    for (size_t i = 0; i < m_eventList->GetColumnCount(); ++i)
    {
        DxuiPopupMenu::Item  item;
        item.label   = m_eventList->GetColumnAt (i).title;
        item.checked = m_eventList->IsColumnVisible (i);
        items.push_back (std::move (item));
    }

    // Hand the popup the current window theme at show time. The menu
    // renders deferred in a pooled popup host (not the widget tree), so
    // it can't pick the theme up from a paint-pump pass; the window theme
    // is stable (owned by the shell), so this pointer never dangles.
    m_columnMenu.SetTheme (m_theme);

    m_columnMenu.Show (anchorX, anchorY, std::move (items), *textRenderer, host);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyListSelection
//
//  Resolves the selected event (tracked by its stable seq) against the
//  current filtered/sorted order via the pure DebugDialogProjection helper,
//  then pushes the resulting visible-row index into the DxuiListView, which
//  scrolls it into view. Because identity is the event's seq -- not a row or
//  deque index -- a sort reorder keeps the same event selected AND visible,
//  and a filtered-out / evicted selection snaps to the nearest survivor. The
//  resolution logic is unit-tested headlessly in DebugDialogProjection.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::ApplyListSelection()
{
    DebugSelectionResult  res =
        DebugDialogProjection::ResolveSelection (m_selectedSeq, m_events, m_filteredIndices);

    m_selectedSeq = res.seq;
    m_eventList->SetSelectedRow (res.row);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnListSelectionMoved
//
//  Mirrors the DxuiListView's new selected-row index back into our
//  persistent seq identity so it survives filter/sort rebuilds.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::OnListSelectionMoved()
{
    int  row = m_eventList->GetSelectedRow();



    if (row < 0 || (size_t) row >= m_filteredIndices.size())
    {
        m_selectedSeq = 0;
        return;
    }

    size_t  idx = m_filteredIndices[(size_t) row];
    m_selectedSeq = (idx < m_events.size()) ? m_events[idx].seq : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SortByColumn
//
//  Toggles descending when the same column is re-sorted; otherwise
//  switches to ascending sort on the new column. Rebuilds rows and
//  preserves selection.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::SortByColumn (int absCol)
{
    if (absCol == m_sortColumn)
    {
        m_sortDescending = !m_sortDescending;
    }
    else
    {
        m_sortColumn     = absCol;
        m_sortDescending = false;
    }

    m_eventList->SetSortIndicator (m_sortColumn, m_sortDescending);
    RebuildFilteredIndices();
    SyncListRowCount();
    ApplyListSelection();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2DebugPanel::OfferPressTo
//
//  Offers a press to one control and, if it takes it, moves keyboard focus
//  there so traversal resumes from the last-clicked control. `handled`
//  short-circuits, so a chain of these stops at the first taker without
//  every site restating the test.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::OfferPressTo (IDxuiControl * control, const DxuiMouseEvent & ev, bool & handled)
{
    if (!handled && control != nullptr && control->OnMouse (ev))
    {
        m_focusMgr.SetFocused (control);
        handled = true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2DebugPanel::OnMouseMove
//
//  Hover tracking across every widget, plus the two states that swallow a
//  move outright: a list drag in progress and an open column menu.
//
////////////////////////////////////////////////////////////////////////////////

bool Disk2DebugPanel::OnMouseMove (const DxuiMouseEvent & ev)
{
    int   x       = ev.positionDip.x;
    int   y       = ev.positionDip.y;
    bool  lbDown  = (GetKeyState (VK_LBUTTON) & 0x8000) != 0;



    if (m_eventList->IsInteracting())
    {
        // While the list owns a drag (scrollbar thumb / column resize), route
        // moves to it. DxuiListView::OnMouse treats a non-Left move while
        // interacting as a release (its missed-button-up safety net), so pass
        // Left explicitly.
        (void) ForwardMouseToList (DxuiMouseEventKind::Move, DxuiMouseButton::Left, x, y, 0.0f);
    }
    else if (m_columnMenu.IsVisible())
    {
        m_columnMenu.OnMouse (ev);
        m_tooltip.RequestHide (NowMs());
    }
    else
    {
        for (auto & cb : m_eventChecks)        { cb->SetMouseHover (x, y); }
        m_audioMasterCheck->SetMouseHover (x, y);
        for (auto & cb : m_audioSubChecks)     { cb->SetMouseHover (x, y); }
        m_rawQtCheck->SetMouseHover (x, y);
        m_driveRadio->SetMouseHover (x, y);
        m_trackEdit->SetMouseHover  (x, y);
        m_sectorEdit->SetMouseHover (x, y);

        m_pauseButton->SetMouse (x, y, m_pauseButton->HitTest (x, y) && lbDown);
        m_clearButton->SetMouse (x, y, m_clearButton->HitTest (x, y) && lbDown);

        // Row-hover highlight: the list owns the hit-test + hovered state.
        (void) ForwardMouseToList (DxuiMouseEventKind::Move, DxuiMouseButton::None, x, y, 0.0f);

        UpdateTooltip (x, y);
    }

    // A move over the panel is always ours; nothing behind it wants one.
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2DebugPanel::OnMouseDownLeft
//
//  Offers the press to each widget in z-order, then to the list. The
//  client-px widgets share the panel's coordinate space (ev.positionDip ==
//  client px), so each hit-tests itself and reports whether it consumed the
//  press -- no hit-testing happens here.
//
////////////////////////////////////////////////////////////////////////////////

bool Disk2DebugPanel::OnMouseDownLeft (const DxuiMouseEvent & ev)
{
    int   x       = ev.positionDip.x;
    int   y       = ev.positionDip.y;
    bool  handled = false;



    if (m_columnMenu.IsVisible() && m_columnMenu.OnMouse (ev))
    {
        handled = true;
    }

    for (auto & eventCheck : m_eventChecks)
    {
        OfferPressTo (eventCheck, ev, handled);
    }

    OfferPressTo (m_audioMasterCheck, ev, handled);

    for (auto & audioSubCheck : m_audioSubChecks)
    {
        OfferPressTo (audioSubCheck, ev, handled);
    }

    OfferPressTo (m_rawQtCheck,  ev, handled);
    OfferPressTo (m_driveRadio,  ev, handled);
    OfferPressTo (m_trackEdit,   ev, handled);
    OfferPressTo (m_sectorEdit,  ev, handled);
    OfferPressTo (m_pauseButton, ev, handled);
    OfferPressTo (m_clearButton, ev, handled);

    if (!handled)
    {
        // The list owns all in-list routing (scrollbar arrows / thumb / track,
        // column resize, header-click sort, row select) via OnMouse and reports
        // outcomes through the callbacks wired at setup. DxuiWindow holds the
        // Win32 capture for the full press, so any drag the list starts keeps
        // receiving moves after the cursor leaves the client. OnMouse consumes
        // only in-bounds presses; when it does, focus moves to the list.
        handled = ForwardMouseToList (DxuiMouseEventKind::Down, DxuiMouseButton::Left, x, y, 0.0f);

        if (handled) { m_focusMgr.SetFocused (m_eventList); }
    }

    // Claimed either way: a left-press on the panel background is still ours.
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2DebugPanel::OnMouseDownRight
//
//  Right-click inside the list-view header strip surfaces a themed popup
//  menu of column-visibility toggles. Anywhere else it is a no-op -- but
//  still claimed, so no context menu leaks through from behind.
//
////////////////////////////////////////////////////////////////////////////////

bool Disk2DebugPanel::OnMouseDownRight (int x, int y)
{
    int  relX        = x - m_layout.listView.left;
    int  relY        = y - m_layout.listView.top;
    int  headerH     = m_eventList->GetHeaderHeightPx();
    int  listWidthPx = m_layout.listView.right - m_layout.listView.left;



    if (m_eventList->IsHeaderShown()
        && relX >= 0 && relX < listWidthPx
        && relY >= 0 && relY < headerH)
    {
        ShowColumnMenu (x, y);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2DebugPanel::OnMouseUpLeft
//
//  Ends a list drag, or fans the release out to every widget so each clears
//  its press visual and fires its click callback if the release landed on it.
//
////////////////////////////////////////////////////////////////////////////////

bool Disk2DebugPanel::OnMouseUpLeft (const DxuiMouseEvent & ev)
{
    int  x = ev.positionDip.x;
    int  y = ev.positionDip.y;



    if (m_eventList->IsInteracting())
    {
        // Finish any list drag (scrollbar thumb / column resize) the list
        // started on button-down. The pointer may have left the list bounds
        // mid-drag, so forward the release unconditionally. DxuiWindow releases
        // the Win32 capture before routing this release.
        (void) ForwardMouseToList (DxuiMouseEventKind::Up, DxuiMouseButton::Left, x, y, 0.0f);
    }
    else if (m_columnMenu.IsVisible() && m_columnMenu.OnMouse (ev))
    {
        // The menu took it.
    }
    else
    {
        // Route the release to each widget: it clears its own press visual
        // and, on a click-release over itself, fires the callback wired at
        // setup (checkbox change / button click), which folds the outcome back
        // into the panel model. Unlike the press, every widget sees this one --
        // they all need to drop a stale pressed state.
        for (auto & cb : m_eventChecks)        { cb->OnMouse (ev); }
        m_audioMasterCheck->OnMouse (ev);
        for (auto & cb : m_audioSubChecks)     { cb->OnMouse (ev); }
        m_rawQtCheck->OnMouse   (ev);
        m_driveRadio->OnMouse   (ev);
        m_trackEdit->OnMouse    (ev);
        m_sectorEdit->OnMouse   (ev);

        m_pauseButton->OnMouse (ev);
        m_clearButton->OnMouse (ev);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnMouse
//
//  Dispatch only -- one handler per event kind above.
//
////////////////////////////////////////////////////////////////////////////////

bool Disk2DebugPanel::OnMouse (const DxuiMouseEvent & ev)
{
    int   x       = ev.positionDip.x;
    int   y       = ev.positionDip.y;
    bool  isLeft  = (ev.button == DxuiMouseButton::Left);
    bool  isRight = (ev.button == DxuiMouseButton::Right);
    bool  handled = false;



    switch (ev.kind)
    {
        case DxuiMouseEventKind::Move:
            handled = OnMouseMove (ev);
            break;

        case DxuiMouseEventKind::Down:
            if      (isLeft)  { handled = OnMouseDownLeft (ev); }
            else if (isRight) { handled = OnMouseDownRight (x, y); }
            break;

        case DxuiMouseEventKind::Up:
            if (isLeft) { handled = OnMouseUpLeft (ev); }
            break;

        case DxuiMouseEventKind::Wheel:
            // Wheel up scrolls back in history (older events); wheel down
            // scrolls toward the tail. Forwarded to the list, which scrolls
            // only when the pointer is over it (standard control behavior).
            (void) ForwardMouseToList (DxuiMouseEventKind::Wheel, DxuiMouseButton::None, x, y, ev.wheelDelta);
            handled = true;
            break;

        default:
            break;
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnKey
//
//  Keyboard routing for the panel, in three tiers.
//
//  A VISIBLE column popup captures every key-down outright -- it is modal in
//  practice, so nothing behind it may act on a keystroke.
//
//  Otherwise the FOCUSED control sees the key before the panel's Tab
//  traversal, and that order is what makes the list's nested navigation work.
//  A focused list owns Tab itself, cycling its header, divider, and body
//  sub-stops -- column sort, column resize, row navigation -- and declines only
//  when Tab steps past either end. The panel then advances control focus with
//  the key the list handed back. Checking Tab first would make the list's
//  sub-stops unreachable.
//
//  Char events go only to the text inputs. Each edit inserts the character
//  when it owns focus and reports whether it consumed it, so no separate
//  focus test is needed here.
//
////////////////////////////////////////////////////////////////////////////////

bool Disk2DebugPanel::OnKey (const DxuiKeyEvent & ev)
{
    IDxuiControl *  focused = nullptr;
    bool            handled = false;



    if (ev.kind == DxuiKeyEventKind::Char)
    {
        // Char events route to the text inputs only; each edit inserts the
        // character when it owns focus and reports whether it consumed it.
        handled = m_trackEdit->OnKey (ev) || m_sectorEdit->OnKey (ev);
    }
    else if (ev.kind == DxuiKeyEventKind::Down)
    {
        if (m_columnMenu.IsVisible())
        {
            // The column popup, when visible, captures every key-down.
            handled = m_columnMenu.OnKey (ev);
        }
        else
        {
            // Focused-first: the focused control sees the key before the
            // panel's Tab traversal. A focused list owns Tab, cycling its
            // header / divider / body sub-stops (column sort / resize / row
            // navigation) and declining only when Tab steps past either end;
            // focused checkboxes / buttons self-activate on Space / Enter.
            focused = m_focusMgr.Focused();
            handled = (focused != nullptr) && focused->OnKey (ev);

            // Tab then advances the panel's control focus, once the focused
            // control (e.g. the list at a sub-stop boundary) has declined it.
            if (!handled && (WPARAM) ev.vk == VK_TAB)
            {
                m_focusMgr.HandleKey ((GetKeyState (VK_SHIFT) & 0x8000) ? DxuiFocusKey::ShiftTab
                                                                        : DxuiFocusKey::Tab);
                handled = true;
            }
        }
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Layout
//
//  DxuiWindow drives this after the OS window sizes / resizes: cache the
//  client size and DPI, then re-run the panel's absolute layout so the
//  child widgets track the new bounds.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::Layout (
    const RECT          & boundsDip,
    const DxuiDpiScaler & scaler)
{
    m_widthPx  = std::max (1, (int) (boundsDip.right  - boundsDip.left));
    m_heightPx = std::max (1, (int) (boundsDip.bottom - boundsDip.top));
    m_dpi      = scaler.Dpi();
    m_scaler   = scaler;

    RecomputeLayout();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CursorForPoint
//
//  DxuiWindow resolves the client cursor by fanning a client-px point
//  through the panel tree. DxuiListView::CursorForPoint expects list-
//  local coords, so translate by the list's bounds before delegating.
//  During an active column-resize drag the pointer may leave the header
//  strip (where the edge hit-test lives), so hold the resize cursor for
//  the duration of the drag.
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR Disk2DebugPanel::CursorForPoint (POINT clientPx) const
{
    LPCWSTR  cursor = nullptr;
    RECT     bounds = {};
    POINT    local  = {};



    // Before OnCreate there is no list to ask, and a null cursor means
    // "no opinion" -- DxuiWindow falls back to the default arrow.
    if (m_eventList != nullptr)
    {
        bounds  = m_eventList->Bounds();
        local.x = clientPx.x - bounds.left;
        local.y = clientPx.y - bounds.top;

        cursor = m_eventList->CursorForPoint (local);

        if (cursor == nullptr && m_eventList->IsResizingColumn())
        {
            cursor = IDC_SIZEWE;
        }
    }

    return cursor;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RecomputeLayout
//
//  Recomputes the cached PanelLayoutSlots whenever the panel's client
//  size or DPI changes. Slots are positioned below the chrome title bar
//  so they don't overlap it. Once slot rectangles are known, label
//  widgets are re-anchored to the appropriate slots.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::RecomputeLayout()
{
    int  titleHeight = CaptionHeightPx();



    m_layout = ComputeDisk2DebugPanelLayout (m_widthPx, m_heightPx, titleHeight, m_dpi);

    LayoutWidgets();
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDynamicLabels
//
//  Refreshes the label text that depends on live filter state: the
//  track-filter caption (Track vs Quarter-track) and the two invalid-
//  span detail labels. Called at creation and whenever the filter /
//  edit state changes -- NOT from the layout pass (text is content, not
//  geometry).
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::UpdateDynamicLabels()
{
    if (m_trackFilterLabel == nullptr)
    {
        return;
    }

    m_trackFilterLabel->SetText   (m_filter.trackFilterRawQt ? s_kpszTrackQtFilterLabel : s_kpszTrackFilterLabel);
    m_trackInvalidLabel->SetText  (BuildInvalidLabel (s_kpszTrackInvalidPrefix,  m_trackEdit->Text(),  m_filter.trackFilter.RejectedSpans()).c_str());
    m_sectorInvalidLabel->SetText (BuildInvalidLabel (s_kpszSectorInvalidPrefix, m_sectorEdit->Text(), m_filter.sectorFilter.RejectedSpans()).c_str());
}





////////////////////////////////////////////////////////////////////////////////
//
//  LayoutWidgets
//
//  Positions every child at its computed slot in the current DPI. Pure
//  geometry: each child's Layout(rect, scaler) call carries both bounds
//  and DPI, so no widget needs an explicit SetDpi. Text lives in the
//  label constructors / UpdateDynamicLabels; colors resolve at paint.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::LayoutWidgets()
{
    m_trackFilterLabel->Layout   (m_layout.trackFilterLabel,  m_scaler);
    m_sectorFilterLabel->Layout  (m_layout.sectorFilterLabel, m_scaler);
    m_trackInvalidLabel->Layout  (m_layout.trackInvalidLabel, m_scaler);
    m_sectorInvalidLabel->Layout (m_layout.sectorInvalidLabel, m_scaler);
    m_driveFilterLabel->Layout   (m_layout.driveFilterLabel,  m_scaler);
    m_diskEventsLabel->Layout    (m_layout.diskEventsLabel,   m_scaler);
    m_audioEventsLabel->Layout   (m_layout.audioEventsLabel,  m_scaler);

    for (int i = 0; i < kEventTypeCheckCount; i++)
    {
        m_eventChecks[i]->Layout (m_layout.eventTypeChecks[i], m_scaler);
    }

    m_audioMasterCheck->Layout (m_layout.audioMasterCheck, m_scaler);

    for (int i = 0; i < kAudioSubCheckCount; i++)
    {
        m_audioSubChecks[i]->Layout (m_layout.audioSubChecks[i], m_scaler);
    }

    m_rawQtCheck->Layout (m_layout.rawQtCheck, m_scaler);

    // DxuiRadioGroup positions its buttons from rects carried in its
    // option records, so the layout pass rebuilds them. The option
    // labels are static; only the rects change per resize. Laying the
    // group out (bounds = union of the option rects) folds in the DPI via
    // the scaler -- no separate SetDpi needed.
    std::vector<DxuiRadioOption>  driveOpts;
    RECT                          driveGroupBounds = m_layout.driveRadios[0];

    for (int i = 0; i < kDriveRadioCount; i++)
    {
        DxuiRadioOption  opt;
        opt.rect  = m_layout.driveRadios[i];
        opt.label = s_kpszDriveOptionLabels[i];
        driveOpts.push_back (std::move (opt));

        driveGroupBounds.left   = std::min (driveGroupBounds.left,   m_layout.driveRadios[i].left);
        driveGroupBounds.top    = std::min (driveGroupBounds.top,    m_layout.driveRadios[i].top);
        driveGroupBounds.right  = std::max (driveGroupBounds.right,  m_layout.driveRadios[i].right);
        driveGroupBounds.bottom = std::max (driveGroupBounds.bottom, m_layout.driveRadios[i].bottom);
    }

    m_driveRadio->SetOptions  (std::move (driveOpts));
    m_driveRadio->Layout      (driveGroupBounds, m_scaler);
    m_driveRadio->SetSelected (m_filter.driveFilter);

    m_trackEdit->Layout  (m_layout.trackEdit,  m_scaler);
    m_sectorEdit->Layout (m_layout.sectorEdit, m_scaler);

    m_pauseButton->Layout (m_layout.pauseButton, m_scaler);
    m_clearButton->Layout (m_layout.clearButton, m_scaler);

    m_eventList->Layout (m_layout.listView, m_scaler);

    // The column menu + tooltip are deferred popups that derive their DPI
    // from their popup host at show time (see DxuiPopupMenu/DxuiTooltip),
    // so no explicit SetDpi here.
    m_tooltip.SetViewportSize (m_widthPx, m_heightPx);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConfigureWidgets
//
//  Wires labels, initial state, and change callbacks onto every widget.
//  Called once after device init; layout (rect / DPI) is reapplied per
//  resize / theme change via LayoutWidgets.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::ConfigureWidgets()
{
    static const std::array<uint32_t, kEventTypeCheckCount> s_kCheckBits =
    {
        FilterState::kEventCatMotor,    FilterState::kEventCatHeadStep,
        FilterState::kEventCatHeadBump, FilterState::kEventCatAddrMark,
        FilterState::kEventCatRead,     FilterState::kEventCatWrite,
        FilterState::kEventCatDoor,     FilterState::kEventCatDriveSelect,
    };


    for (int i = 0; i < kEventTypeCheckCount; i++)
    {
        m_eventChecks[i]->SetChecked  ((m_filter.eventTypeMask & s_kCheckBits[i]) != 0);
        uint32_t  bit = s_kCheckBits[i];
        m_eventChecks[i]->SetOnChange ([this, bit] (bool checked)
        {
            if (checked) { m_filter.eventTypeMask |=  bit; }
            else         { m_filter.eventTypeMask &= ~bit; }
            OnFilterChanged();
        });
    }

    m_audioMasterCheck->SetChecked  (m_filter.audioMaster);
    m_audioMasterCheck->SetOnChange ([this] (bool checked)
    {
        m_filter.audioMaster = checked;
        for (auto & cb : m_audioSubChecks) { cb->SetEnabled (checked); }
        OnFilterChanged();
    });

    bool * const  s_kAudioSubBackers[kAudioSubCheckCount] =
    {
        &m_filter.audioStarted, &m_filter.audioRestarted,
        &m_filter.audioContinued, &m_filter.audioSilent,
    };

    for (int i = 0; i < kAudioSubCheckCount; i++)
    {
        m_audioSubChecks[i]->SetChecked  (*s_kAudioSubBackers[i]);
        m_audioSubChecks[i]->SetEnabled  (m_filter.audioMaster);
        bool * backer = s_kAudioSubBackers[i];
        m_audioSubChecks[i]->SetOnChange ([this, backer] (bool checked)
        {
            *backer = checked;
            OnFilterChanged();
        });
    }

    m_rawQtCheck->SetChecked  (m_filter.trackFilterRawQt);
    m_rawQtCheck->SetOnChange ([this] (bool checked)
    {
        m_filter.trackFilterRawQt = checked;
        OnTrackEditChanged();
        OnFilterChanged();
    });

    m_driveRadio->SetSelected (m_filter.driveFilter);
    m_driveRadio->SetOnChange ([this] (int newIndex)
    {
        m_filter.driveFilter = newIndex;
        OnFilterChanged();
    });

    m_trackEdit->SetMaxLength  (32);
    m_trackEdit->SetHwnd       (Hwnd());
    m_trackEdit->SetOnChange   ([this] (const std::wstring &) { OnTrackEditChanged(); OnFilterChanged(); });

    m_sectorEdit->SetMaxLength (32);
    m_sectorEdit->SetHwnd      (Hwnd());
    m_sectorEdit->SetOnChange  ([this] (const std::wstring &) { OnSectorEditChanged(); OnFilterChanged(); });

    m_pauseButton->SetOnClick ([this] ()
    {
        m_paused = !m_paused;
        UpdatePauseLabel();
    });

    m_clearButton->SetOnClick ([this] () { ClearEvents(); });

    std::vector<DxuiListView::Column>  cols;
    cols.push_back ({ L"Time",   0, false, DxuiTextRenderer::HAlign::Left  });
    cols.push_back ({ L"Uptime", 0, false, DxuiTextRenderer::HAlign::Left  });
    cols.push_back ({ L"Cycle",  0, false, DxuiTextRenderer::HAlign::Right });
    cols.push_back ({ L"Drive",  0, false, DxuiTextRenderer::HAlign::Right });
    cols.push_back ({ L"Event",  0, false, DxuiTextRenderer::HAlign::Left  });
    cols.push_back ({ L"Detail", 0, false, DxuiTextRenderer::HAlign::Left  });
    m_eventList->SetColumns                 (std::move (cols));
    m_eventList->SetShowHeader              (true);
    m_eventList->SetHorizontalScrollEnabled (true);

    // The list owns keyboard column navigation: when it holds focus, its
    // own OnKey cycles the header / divider / body sub-stops on Tab and
    // fires the sort / resize / selection callbacks below. Resize style: Tab
    // walks header/divider sub-stops and Left/Right nudge column widths.
    m_eventList->SetKeyboardColumnNav (true);
    m_eventList->SetKeyboardColumnResize (true);

    // The list owns its own scroll / thumb / column-resize / row-select
    // routing via OnMouse; these callbacks fold the semantic outcomes
    // back into the panel (selected event, sort).
    m_eventList->SetOnSelectionChanged ([this] (int row)
    {
        // The list already moved (and scrolled to) its own selected row;
        // just record that event's stable seq so the selection survives the
        // next filter/sort rebuild. Re-resolving here would be redundant.
        if (row >= 0 && row < (int) m_filteredIndices.size())
        {
            size_t  idx = m_filteredIndices[(size_t) row];
            m_selectedSeq = (idx < m_events.size()) ? m_events[idx].seq : 0;
        }
        else
        {
            m_selectedSeq = 0;
        }
    });
    m_eventList->SetOnSortColumn ([this] (int col)
    {
        SortByColumn (col);
    });
    m_eventList->SetOnColumnResized ([] (int, int) {});

    // Install the virtual-row provider once: the list pulls only its visible
    // window through FillRow, so a 100k-row live log costs O(visible) per
    // frame instead of re-materializing every row (GH #88). The row count is
    // republished each rebuild via SyncListRowCount.
    m_eventList->SetRowProvider (0, [this] (int row, std::vector<DxuiListView::Cell> & out)
    {
        FillRow (row, out);
    });

    m_columnMenu.SetOnSelect ([this] (int index)
    {
        if (index < 0 || index >= (int) m_eventList->GetColumnCount()) { return; }
        m_eventList->SetColumnVisible ((size_t) index, !m_eventList->IsColumnVisible ((size_t) index));
        LayoutWidgets();
        m_focusMgr.Rebuild();
    });

    m_focusMgr.Attach  (this);
    m_focusMgr.SetTheme (m_theme);
    m_focusMgr.Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DrainAndProject
//
//  Per-frame pull: drain the ring into the deque (with dropped-count
//  synthetic EventsLost), rebuild filtered index list, push visible
//  rows into the list view. Pause skips the drain so producer events
//  continue accumulating but the display freezes.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::DrainAndProject()
{
    uint32_t  dropped = 0;
    int64_t   ticks   = 0;



    if (m_resetAnchorPending.exchange (false, std::memory_order_acq_rel))
    {
        // A reset (Ctrl+R / power-cycle) was requested from the CPU
        // thread. Apply the staged Uptime anchor and clear the event
        // list HERE, on the render thread, so m_events, m_filteredIndices
        // and the DxuiListView rows are only ever touched by one thread.
        ticks = m_pendingAnchorTicks.load (std::memory_order_acquire);

        m_uptimeAnchor = std::chrono::steady_clock::time_point (std::chrono::steady_clock::duration (ticks));
        ClearEvents();
    }

    if (m_paused)
    {
        return;
    }

    dropped = m_droppedSinceLastDrain.exchange (0, std::memory_order_acq_rel);

    // Change-gate: DrainAndProject stamps each appended event from m_nextSeq,
    // so an unchanged counter means the ring was empty (and no dropped-count
    // synthetic was pushed) -- nothing was added and no front-eviction shifted
    // the deque, so the filtered set and rows are already current. Skip the
    // O(n) rebuild/re-sort on idle frames; the disk-heavy path (GH #88) still
    // rebuilds, but only when there is genuinely new data to show.
    uint64_t  seqBefore = m_nextSeq;

    DebugDialogProjection::DrainAndProject (m_ring, m_events, dropped, m_uptimeAnchor, &m_nextSeq);

    if (m_nextSeq != seqBefore)
    {
        RebuildFilteredIndices();
        SyncListRowCount();
        ApplyListSelection();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RebuildFilteredIndices
//
//  Recomputes which events are visible, then orders them by the active sort
//  column.
//
//  Indices are stored rather than copies, so filtering and sorting move
//  machine words instead of event records, and the events themselves stay put
//  in the deque.
//
//  The sort is STABLE, so events sharing a sort key keep their arrival order
//  -- which for a capture log is the order the reader most wants preserved.
//
//  Comparison is done on the DISPLAY strings, so the ordering always matches
//  what is on screen rather than an underlying value the reader cannot see.
//
//  Cycle counts get their own comparator for that reason: they are formatted
//  with thousands separators and no leading zeros, so a plain lexical compare
//  would order 9,999 after 10,000. Comparing LENGTH first and then
//  lexically is equivalent to numeric ordering for exactly that format -- and
//  it stays correct only while the format keeps those two properties.
//
//  An unsorted panel skips the sort entirely and keeps insertion order, which
//  is the common streaming case.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::RebuildFilteredIndices()
{
    m_filteredIndices.clear();
    m_filteredIndices.reserve (m_events.size());

    for (size_t i = 0; i < m_events.size(); i++)
    {
        if (MatchesFilter (m_events[i], m_filter))
        {
            m_filteredIndices.push_back (i);
        }
    }

    if (m_sortColumn < 0)
    {
        return;
    }

    const std::deque<Disk2EventDisplay> &  events = m_events;
    int                                     col    = m_sortColumn;
    bool                                    desc   = m_sortDescending;

    auto cmpStr = [] (const wchar_t * a, const wchar_t * b) -> int
    {
        return wcscmp (a, b);
    };

    auto cmpCycle = [] (const wchar_t * a, const wchar_t * b) -> int
    {
        // Cycle strings carry thousands separators and no leading zeros,
        // so length-then-lex is equivalent to numeric ordering.
        size_t  la = wcslen (a);
        size_t  lb = wcslen (b);
        if (la != lb) { return (la < lb) ? -1 : 1; }
        return wcscmp (a, b);
    };

    std::stable_sort (m_filteredIndices.begin(),
                      m_filteredIndices.end(),
                      [&] (size_t ia, size_t ib) -> bool
    {
        const Disk2EventDisplay &  ea = events[ia];
        const Disk2EventDisplay &  eb = events[ib];
        int                         c  = 0;

        switch (col)
        {
            case 0: c = cmpStr   (ea.wallStr.data(),   eb.wallStr.data());   break;
            case 1: c = cmpStr   (ea.uptimeStr.data(), eb.uptimeStr.data()); break;
            case 2: c = cmpCycle (ea.cycleStr.data(),  eb.cycleStr.data());  break;
            case 3:
                if (ea.drive != eb.drive) { c = (ea.drive < eb.drive) ? -1 : 1; }
                break;
            case 4:
            {
                std::wstring_view  la = DebugDialogProjection::EventLabel (ea.category, ea.type);
                std::wstring_view  lb = DebugDialogProjection::EventLabel (eb.category, eb.type);
                c = la.compare (lb);
                break;
            }

            case 5: c = ea.detail.compare (eb.detail); break;
            default: break;
        }

        if (c == 0)
        {
            // Fall back to chronological (insertion) order so equal
            // keys keep a stable, predictable arrangement.
            return ia < ib;
        }

        return desc ? (c > 0) : (c < 0);
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  FillRow
//
//  Virtual-row provider (GH #88). Called by the DxuiListView only for the
//  rows in its visible window, `row` being a visible-row index into
//  m_filteredIndices. Maps that to the backing Disk2EventDisplay and
//  materializes its six cells into `out` (already cleared by the widget).
//  Runs on the render thread during Paint, a pure read of m_events /
//  m_filteredIndices, so no per-frame allocation of the whole list.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::FillRow (int row, std::vector<DxuiListView::Cell> & out) const
{
    bool     inRange = (row >= 0 && (size_t) row < m_filteredIndices.size());
    size_t   idx     = inRange ? m_filteredIndices[(size_t) row] : m_events.size();
    wchar_t  driveBuf[8] = {};

    // The list can ask for a row that the filter has since dropped, or whose
    // event was evicted from the ring. Leaving `out` empty renders a blank
    // row, which is what the list expects for a vanished entry.
    if (idx < m_events.size())
    {
        const Disk2EventDisplay & e = m_events[idx];

        out.push_back ({ std::wstring (e.wallStr.data()),   false });
        out.push_back ({ std::wstring (e.uptimeStr.data()), false });
        out.push_back ({ std::wstring (e.cycleStr.data()),  false });

        if (e.drive == Disk2EventDisplay::kFieldNotApplicable)
        {
            out.push_back ({ L"", false });
        }
        else
        {
            swprintf_s (driveBuf, L"%d", e.drive + 1);
            out.push_back ({ std::wstring (driveBuf), false });
        }

        out.push_back ({ std::wstring (DebugDialogProjection::EventLabel (e.category, e.type)), false });
        out.push_back ({ e.detail, false });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SyncListRowCount
//
//  Republish the virtual row count after the filtered set changes. The list
//  keeps its stable FillRow provider and pulls only the visible window, so
//  this is O(1) -- no materialization of the (possibly 100k-row) list.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::SyncListRowCount()
{
    m_eventList->SetVirtualRowCount ((int) m_filteredIndices.size());
}





////////////////////////////////////////////////////////////////////////////////
//
//  PublishToRing
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::PublishToRing (const Disk2Event & e)
{
    Disk2Event  stamped = e;



    if (m_cycleCounter != nullptr)
    {
        stamped.cycle = *m_cycleCounter;
    }

    if (!m_ring.TryPush (stamped))
    {
        m_droppedSinceLastDrain.fetch_add (1, std::memory_order_relaxed);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MakeStampedEvent
//
////////////////////////////////////////////////////////////////////////////////

Disk2Event Disk2DebugPanel::MakeStampedEvent (EventCategory cat, Disk2EventType type) const noexcept
{
    Disk2Event  e = {};



    e.category = cat;
    e.type     = type;
    e.drive    = (int8_t) m_currentDrive;

    return e;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnFilterChanged
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::OnFilterChanged()
{
    RebuildFilteredIndices();
    SyncListRowCount();
    ApplyListSelection();
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnTrackEditChanged
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::OnTrackEditChanged()
{
    m_filter.trackFilter = TrackSectorPredicate::Parse (m_trackEdit->Text(),
                                                        TrackSectorPredicate::Mode::Track,
                                                        m_filter.trackFilterRawQt);
    m_trackEditValid = m_filter.trackFilter.RejectedSpans().empty();
    UpdateDynamicLabels();
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnSectorEditChanged
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::OnSectorEditChanged()
{
    m_filter.sectorFilter = TrackSectorPredicate::Parse (m_sectorEdit->Text(),
                                                         TrackSectorPredicate::Mode::Sector);
    m_sectorEditValid = m_filter.sectorFilter.RejectedSpans().empty();
    UpdateDynamicLabels();
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdatePauseLabel
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::UpdatePauseLabel()
{
    m_pauseButton->SetLabel (m_paused ? s_kpszResumeLabel : s_kpszPauseLabel);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ClearEvents
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::ClearEvents()
{
    constexpr uint32_t  kClearDrainBatchSize = 64;
    Disk2Event         scratch[kClearDrainBatchSize] = {};
    uint32_t            drained                       = 0;


    m_droppedSinceLastDrain.store (0, std::memory_order_release);
    do
    {
        drained = m_ring.Drain (scratch, kClearDrainBatchSize);
    }
    while (drained > 0);

    m_events.clear();
    m_filteredIndices.clear();
    m_currentDrive = 0;
    m_selectedSeq  = 0;
    m_eventList->ResetAutoFit();
    SyncListRowCount();
    ApplyListSelection();
}





////////////////////////////////////////////////////////////////////////////////
//
//  RequestResetAnchor
//
//  Thread-safe reset entry point for the CPU/reset thread. Stages the
//  new Uptime anchor and raises a pending-reset flag; DrainAndProject
//  applies the anchor and clears the event list on the render thread,
//  keeping the event deque and DxuiListView rows single-threaded.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::RequestResetAnchor (std::chrono::steady_clock::time_point anchor) noexcept
{
    m_pendingAnchorTicks.store (anchor.time_since_epoch().count(), std::memory_order_release);
    m_resetAnchorPending.store (true, std::memory_order_release);
}





////////////////////////////////////////////////////////////////////////////////
//
//  IDisk2EventSink implementations
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::OnMotorCommandOn()
{
    Disk2Event  e = MakeStampedEvent (EventCategory::Controller, Disk2EventType::MotorCommandOn);
    PublishToRing (e);
}

void Disk2DebugPanel::OnMotorEngaged()
{
    Disk2Event  e = MakeStampedEvent (EventCategory::Controller, Disk2EventType::MotorEngaged);
    PublishToRing (e);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnMotorCommandOff
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::OnMotorCommandOff()
{
    Disk2Event  e = MakeStampedEvent (EventCategory::Controller, Disk2EventType::MotorCommandOff);
    PublishToRing (e);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnMotorDisengaged
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::OnMotorDisengaged()
{
    Disk2Event  e = MakeStampedEvent (EventCategory::Controller, Disk2EventType::MotorDisengaged);
    PublishToRing (e);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnHeadStep
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::OnHeadStep (int prevQt, int newQt)
{
    Disk2Event  e = MakeStampedEvent (EventCategory::Controller, Disk2EventType::HeadStep);
    e.payload.step.prevQt = prevQt;
    e.payload.step.newQt  = newQt;
    PublishToRing (e);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnHeadBump
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::OnHeadBump (int atQt)
{
    Disk2Event  e = MakeStampedEvent (EventCategory::Controller, Disk2EventType::HeadBump);
    e.payload.bump.atQt = atQt;
    PublishToRing (e);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnAddressMark
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::OnAddressMark (int track, int sector, int volume)
{
    Disk2Event  e = MakeStampedEvent (EventCategory::Controller, Disk2EventType::AddrMark);
    e.payload.addrMark.track  = track;
    e.payload.addrMark.sector = sector;
    e.payload.addrMark.volume = volume;
    PublishToRing (e);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnDataMarkRead
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::OnDataMarkRead (int track, int sector, int volume, int byteCount)
{
    Disk2Event  e = MakeStampedEvent (EventCategory::Controller, Disk2EventType::DataRead);
    e.payload.dataMark.track     = track;
    e.payload.dataMark.sector    = sector;
    e.payload.dataMark.volume    = volume;
    e.payload.dataMark.byteCount = byteCount;
    PublishToRing (e);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnDataMarkWrite
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::OnDataMarkWrite (int track, int sector, int volume, int byteCount)
{
    Disk2Event  e = MakeStampedEvent (EventCategory::Controller, Disk2EventType::DataWrite);
    e.payload.dataMark.track     = track;
    e.payload.dataMark.sector    = sector;
    e.payload.dataMark.volume    = volume;
    e.payload.dataMark.byteCount = byteCount;
    PublishToRing (e);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnDriveSelect
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::OnDriveSelect (int drive)
{
    m_currentDrive = drive;
    Disk2Event  e = MakeStampedEvent (EventCategory::Controller, Disk2EventType::DriveSelect);
    e.drive               = (int8_t) drive;
    e.payload.drive.drive = drive;
    PublishToRing (e);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnDiskInserted
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::OnDiskInserted (int drive)
{
    Disk2Event  e = MakeStampedEvent (EventCategory::Controller, Disk2EventType::DiskInserted);
    e.drive               = (int8_t) drive;
    e.payload.drive.drive = drive;
    PublishToRing (e);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnDiskEjected
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::OnDiskEjected (int drive)
{
    Disk2Event  e = MakeStampedEvent (EventCategory::Controller, Disk2EventType::DiskEjected);
    e.drive               = (int8_t) drive;
    e.payload.drive.drive = drive;
    PublishToRing (e);
}





////////////////////////////////////////////////////////////////////////////////
//
//  IDriveAudioEventSink implementations
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::OnAudioStarted (SoundKind kind, int drive)
{
    Disk2Event  e = MakeStampedEvent (EventCategory::Audio, Disk2EventType::AudioStarted);
    e.drive                = (int8_t) drive;
    e.payload.audio.kind   = kind;
    e.payload.audio.drive  = drive;
    e.payload.audio.reason = SilentReason::DriveAudioDisabled;
    PublishToRing (e);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnAudioRestarted
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::OnAudioRestarted (SoundKind kind, int drive)
{
    Disk2Event  e = MakeStampedEvent (EventCategory::Audio, Disk2EventType::AudioRestarted);
    e.drive                = (int8_t) drive;
    e.payload.audio.kind   = kind;
    e.payload.audio.drive  = drive;
    e.payload.audio.reason = SilentReason::DriveAudioDisabled;
    PublishToRing (e);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnAudioContinued
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::OnAudioContinued (SoundKind kind, int drive)
{
    Disk2Event  e = MakeStampedEvent (EventCategory::Audio, Disk2EventType::AudioContinued);
    e.drive                = (int8_t) drive;
    e.payload.audio.kind   = kind;
    e.payload.audio.drive  = drive;
    e.payload.audio.reason = SilentReason::DriveAudioDisabled;
    PublishToRing (e);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnAudioSilent
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::OnAudioSilent (SoundKind kind, int drive, SilentReason reason)
{
    Disk2Event  e = MakeStampedEvent (EventCategory::Audio, Disk2EventType::AudioSilent);
    e.drive                = (int8_t) drive;
    e.payload.audio.kind   = kind;
    e.payload.audio.drive  = drive;
    e.payload.audio.reason = reason;
    PublishToRing (e);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnAudioLoopStarted
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::OnAudioLoopStarted (SoundKind kind, int drive)
{
    Disk2Event  e = MakeStampedEvent (EventCategory::Audio, Disk2EventType::AudioLoopStarted);
    e.drive                = (int8_t) drive;
    e.payload.audio.kind   = kind;
    e.payload.audio.drive  = drive;
    e.payload.audio.reason = SilentReason::DriveAudioDisabled;
    PublishToRing (e);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnAudioLoopStopped
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::OnAudioLoopStopped (SoundKind kind, int drive)
{
    Disk2Event  e = MakeStampedEvent (EventCategory::Audio, Disk2EventType::AudioLoopStopped);
    e.drive                = (int8_t) drive;
    e.payload.audio.kind   = kind;
    e.payload.audio.drive  = drive;
    e.payload.audio.reason = SilentReason::DriveAudioDisabled;
    PublishToRing (e);
}





////////////////////////////////////////////////////////////////////////////////
//
//  NowMs
//
//  Wall-clock-ish millisecond stamp for tooltip dwell timing. Uses
//  steady_clock so a system clock adjustment can't make a tooltip
//  hide millennia in the future.
//
////////////////////////////////////////////////////////////////////////////////

int64_t Disk2DebugPanel::NowMs() const
{
    auto  delta = std::chrono::steady_clock::now() - m_uptimeAnchor;
    return std::chrono::duration_cast<std::chrono::milliseconds> (delta).count();
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateTooltip
//
//  Walks the filter / drive / edit widgets and shows the appropriate
//  tooltip for whichever the cursor is over. Tooltips dwell-open after
//  ~500ms of stable hover (DxuiTooltip widget enforces it) and hide as soon
//  as the cursor leaves all known targets.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::UpdateTooltip (int x, int y)
{
    int64_t  now      = NowMs();
    size_t   i        = 0;
    int      driveHit = 0;
    bool     shown    = false;



    // Widgets do not overlap, so at most one of these hits -- but the scan
    // still short-circuits on `shown` because HitTest is not free and the
    // first match is the answer.
    for (i = 0; !shown && i < m_eventChecks.size(); ++i)
    {
        if (m_eventChecks[i]->HitTest (x, y))
        {
            m_tooltip.RequestShow (m_eventChecks[i]->Rect(), s_kpszEventCheckTips[i], now);
            shown = true;
        }
    }

    if (!shown && m_audioMasterCheck->HitTest (x, y))
    {
        m_tooltip.RequestShow (m_audioMasterCheck->Rect(), s_kpszAudioMasterTip, now);
        shown = true;
    }

    for (i = 0; !shown && i < m_audioSubChecks.size(); ++i)
    {
        if (m_audioSubChecks[i]->HitTest (x, y))
        {
            m_tooltip.RequestShow (m_audioSubChecks[i]->Rect(), s_kpszAudioSubTips[i], now);
            shown = true;
        }
    }

    if (!shown && m_rawQtCheck->HitTest (x, y))
    {
        m_tooltip.RequestShow (m_rawQtCheck->Rect(), s_kpszRawQtTip, now);
        shown = true;
    }

    if (!shown)
    {
        // The radio group hit-tests to an option index, not a bool, and each
        // option carries its own tip and rect.
        driveHit = m_driveRadio->HitTest (x, y);

        if (driveHit >= 0 && driveHit < (int) m_driveRadio->Options().size())
        {
            m_tooltip.RequestShow (m_driveRadio->Options()[driveHit].rect,
                                   s_kpszDriveRadioTips[driveHit],
                                   now);
            shown = true;
        }
    }

    if (!shown && m_trackEdit->HitTest (x, y))
    {
        m_tooltip.RequestShow (m_trackEdit->Rect(), s_kpszTrackEditTip, now);
        shown = true;
    }

    if (!shown && m_sectorEdit->HitTest (x, y))
    {
        m_tooltip.RequestShow (m_sectorEdit->Rect(), s_kpszSectorEditTip, now);
        shown = true;
    }

    // Cursor is over the panel but not over any tooltip target.
    if (!shown)
    {
        m_tooltip.RequestHide (now);
    }
}

