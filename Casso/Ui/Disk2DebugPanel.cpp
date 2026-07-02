#include "Pch.h"

#include "Disk2DebugPanel.h"

#include "Chrome/CassoTheme.h"

#include "../DebugDialogProjection.h"


namespace
{
    constexpr LPCWSTR  s_kpszClassName  = L"Casso.Disk2Debug.Panel";
    constexpr LPCWSTR  s_kpszWindowTitle = L"Casso - Disk ][ debug";

    constexpr int      s_kPreferredWidthDip  = 960;
    constexpr int      s_kPreferredHeightDip = 600;

    constexpr LPCWSTR  s_kpszTrackFilterLabel  = L"Track:";
    constexpr LPCWSTR  s_kpszSectorFilterLabel = L"Sector:";
    constexpr LPCWSTR  s_kpszTrackQtFilterLabel = L"Quarter-track:";

    constexpr float    s_kLabelFontDip = 13.0f;

    constexpr LPCWSTR  s_kpszEventCheckLabels[kEventTypeCheckCount] =
    {
        L"Motor", L"HeadStep", L"HeadBump", L"AddrMark",
        L"Read",  L"Write",    L"Door",     L"DriveSel",
    };

    constexpr LPCWSTR  s_kpszAudioSubLabels[kAudioSubCheckCount] =
    {
        L"Started", L"Restarted", L"Continued", L"Silent",
    };

    constexpr LPCWSTR  s_kpszDriveOptionLabels[kDriveRadioCount] =
    {
        L"All", L"Drive 1", L"Drive 2",
    };

    constexpr LPCWSTR  s_kpszRawQtLabel    = L"Quarter-track steps";
    constexpr LPCWSTR  s_kpszPauseLabel    = L"Pause";
    constexpr LPCWSTR  s_kpszResumeLabel   = L"Resume";
    constexpr LPCWSTR  s_kpszClearLabel    = L"Clear";
    constexpr LPCWSTR  s_kpszAudioLabel    = L"All";
    constexpr LPCWSTR  s_kpszInvalidLabel  = L"Invalid";
    constexpr LPCWSTR  s_kpszTrackInvalidPrefix  = L"Invalid track: ";
    constexpr LPCWSTR  s_kpszSectorInvalidPrefix = L"Invalid sector: ";
    constexpr LPCWSTR  s_kpszDriveFilterLabel    = L"Drive:";
    constexpr LPCWSTR  s_kpszDiskEventsLabel     = L"Disk events:";
    constexpr LPCWSTR  s_kpszAudioEventsLabel    = L"Audio events:";

    constexpr LPCWSTR  s_kpszEventCheckTips[kEventTypeCheckCount] =
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

    constexpr LPCWSTR  s_kpszAudioSubTips[kAudioSubCheckCount] =
    {
        L"Audio loop started",
        L"Audio loop restarted with new parameters",
        L"Audio loop continued without retrigger",
        L"Audio loop silenced (with reason)",
    };

    constexpr LPCWSTR  s_kpszDriveRadioTips[kDriveRadioCount] =
    {
        L"Show events from all drives",
        L"Show only events targeting Drive 1",
        L"Show only events targeting Drive 2",
    };

    constexpr LPCWSTR  s_kpszAudioMasterTip = L"Master toggle for all audio-event categories below";
    constexpr LPCWSTR  s_kpszRawQtTip       = L"Show every quarter-track head step (verbose)";
    constexpr LPCWSTR  s_kpszTrackEditTip   = L"Filter rows to a single track (blank = all)";
    constexpr LPCWSTR  s_kpszSectorEditTip  = L"Filter rows to a single sector (blank = all)";


    void ArgbToFloat4 (uint32_t argb, float (& outRgba)[4]) noexcept
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
    std::wstring BuildInvalidLabel (
        LPCWSTR                                                  prefix,
        const std::wstring                                     & expr,
        const std::vector<TrackSectorPredicate::RejectedSpan> & spans)
    {
        std::wstring  result;

        if (spans.empty()) { return result; }

        result = prefix;
        for (size_t i = 0; i < spans.size(); ++i)
        {
            int  beginIdx = spans[i].beginUtf16;
            int  endIdx   = spans[i].endUtf16;
            if (beginIdx < 0)                       { beginIdx = 0; }
            if (endIdx   > (int) expr.size())       { endIdx   = (int) expr.size(); }
            if (endIdx  <= beginIdx)                { continue; }
            if (i > 0)                              { result += L", "; }
            result.append (expr, (size_t) beginIdx, (size_t) (endIdx - beginIdx));
        }
        return result;
    }
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
    m_trackFilterLabel   = CreateChild<DxuiLabel> ();
    m_sectorFilterLabel  = CreateChild<DxuiLabel> ();
    m_driveFilterLabel   = CreateChild<DxuiLabel> ();
    m_diskEventsLabel    = CreateChild<DxuiLabel> ();
    m_audioEventsLabel   = CreateChild<DxuiLabel> ();
    m_trackInvalidLabel  = CreateChild<DxuiLabel> ();
    m_sectorInvalidLabel = CreateChild<DxuiLabel> ();

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

    m_columnMenu.Show (anchorX, anchorY, std::move (items), *textRenderer, host);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyListSelection
//
//  Resolves m_listSelectedEventIndex (an absolute index into m_events)
//  against the current m_filteredIndices and pushes the corresponding
//  visible-row index into the DxuiListView. If the previously-selected
//  event is no longer visible under the current filter, snap to the
//  previous still-visible row (or the next one if there is no
//  previous). If neither exists, clear the selection.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::ApplyListSelection()
{
    if (m_listSelectedEventIndex < 0 || m_filteredIndices.empty())
    {
        m_listSelectedEventIndex = -1;
        m_eventList->SetSelectedRow (-1);
        return;
    }

    size_t  target = (size_t) m_listSelectedEventIndex;
    auto    it     = std::lower_bound (m_filteredIndices.begin(),
                                       m_filteredIndices.end(),
                                       target);

    if (it != m_filteredIndices.end() && *it == target)
    {
        m_eventList->SetSelectedRow ((int) (it - m_filteredIndices.begin()));
        return;
    }

    if (it != m_filteredIndices.begin())
    {
        auto prev = it - 1;
        m_listSelectedEventIndex = (int) *prev;
        m_eventList->SetSelectedRow ((int) (prev - m_filteredIndices.begin()));
        return;
    }

    if (it != m_filteredIndices.end())
    {
        m_listSelectedEventIndex = (int) *it;
        m_eventList->SetSelectedRow ((int) (it - m_filteredIndices.begin()));
        return;
    }

    m_listSelectedEventIndex = -1;
    m_eventList->SetSelectedRow (-1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnListSelectionMoved
//
//  Mirrors the DxuiListView's new selected-row index back into our
//  persistent event-index identity so it survives filter/sort
//  rebuilds.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::OnListSelectionMoved()
{
    int  row = m_eventList->GetSelectedRow();


    if (row < 0 || (size_t) row >= m_filteredIndices.size())
    {
        m_listSelectedEventIndex = -1;
        return;
    }
    m_listSelectedEventIndex = (int) m_filteredIndices[(size_t) row];
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
    PushListViewRows();
    ApplyListSelection();
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnMouse
//
////////////////////////////////////////////////////////////////////////////////

bool Disk2DebugPanel::OnMouse (const DxuiMouseEvent & ev)
{
    int  x = ev.positionDip.x;
    int  y = ev.positionDip.y;



    switch (ev.kind)
    {
        case DxuiMouseEventKind::Move:
            // While the list owns a drag (scrollbar thumb / column resize),
            // route moves to it. DxuiListView::OnMouse treats a non-Left
            // move while interacting as a release (its missed-button-up
            // safety net), so pass Left explicitly.
            if (m_eventList->IsInteracting())
            {
                (void) ForwardMouseToList (DxuiMouseEventKind::Move, DxuiMouseButton::Left, x, y, 0.0f);
                return true;
            }

            if (m_columnMenu.IsVisible())
            {
                m_columnMenu.OnMouse (ev);
                m_tooltip.RequestHide (NowMs());
                return true;
            }

            for (auto & cb : m_eventChecks)        { cb->SetMouseHover (x, y); }
            m_audioMasterCheck->SetMouseHover (x, y);
            for (auto & cb : m_audioSubChecks)     { cb->SetMouseHover (x, y); }
            m_rawQtCheck->SetMouseHover (x, y);
            m_driveRadio->SetMouseHover (x, y);
            m_trackEdit->SetMouseHover  (x, y);
            m_sectorEdit->SetMouseHover (x, y);

            m_pauseButton->SetMouse (x, y, m_pauseButton->HitTest (x, y) && (GetKeyState (VK_LBUTTON) & 0x8000));
            m_clearButton->SetMouse (x, y, m_clearButton->HitTest (x, y) && (GetKeyState (VK_LBUTTON) & 0x8000));

            // Row-hover highlight: the list owns the hit-test + hovered state.
            (void) ForwardMouseToList (DxuiMouseEventKind::Move, DxuiMouseButton::None, x, y, 0.0f);

            UpdateTooltip (x, y);
            return true;

        case DxuiMouseEventKind::Down:
            if (ev.button == DxuiMouseButton::Left)
            {
                bool  handled = false;


                if (m_columnMenu.IsVisible())
                {
                    if (m_columnMenu.OnMouse (ev)) { return true; }
                }

                // The client-px widgets share the panel's coordinate space
                // (ev.positionDip == client px), so route the press straight
                // to each widget's OnMouse; the widget hit-tests itself and
                // reports whether it consumed the press. The focus manager
                // records the consuming widget so keyboard traversal resumes
                // from the last-clicked control.
                for (size_t i = 0; i < m_eventChecks.size(); ++i)
                {
                    if (m_eventChecks[i]->OnMouse (ev))
                    {
                        m_focusMgr.SetFocused (m_eventChecks[i]);
                        handled = true;
                        break;
                    }
                }
                if (!handled && m_audioMasterCheck->OnMouse (ev))
                {
                    m_focusMgr.SetFocused (m_audioMasterCheck);
                    handled = true;
                }
                if (!handled)
                {
                    for (size_t i = 0; i < m_audioSubChecks.size(); ++i)
                    {
                        if (m_audioSubChecks[i]->OnMouse (ev))
                        {
                            m_focusMgr.SetFocused (m_audioSubChecks[i]);
                            handled = true;
                            break;
                        }
                    }
                }
                if (!handled && m_rawQtCheck->OnMouse (ev))
                {
                    m_focusMgr.SetFocused (m_rawQtCheck);
                    handled = true;
                }
                if (!handled && m_driveRadio->OnMouse (ev))
                {
                    m_focusMgr.SetFocused (m_driveRadio);
                    handled = true;
                }
                if (!handled && m_trackEdit->OnMouse (ev))
                {
                    m_focusMgr.SetFocused (m_trackEdit);
                    handled = true;
                }
                if (!handled && m_sectorEdit->OnMouse (ev))
                {
                    m_focusMgr.SetFocused (m_sectorEdit);
                    handled = true;
                }

                if (m_pauseButton->OnMouse (ev))
                {
                    m_focusMgr.SetFocused (m_pauseButton);
                    handled = true;
                }
                if (!handled && m_clearButton->OnMouse (ev))
                {
                    m_focusMgr.SetFocused (m_clearButton);
                    handled = true;
                }

                if (!handled)
                {
                    // The list owns all in-list routing (scrollbar arrows /
                    // thumb / track, column resize, header-click sort, row
                    // select) via OnMouse and reports outcomes through the
                    // callbacks wired at setup. DxuiWindow holds the
                    // Win32 capture for the full press, so any drag the list
                    // starts keeps receiving moves after the cursor leaves the
                    // client. OnMouse consumes only in-bounds presses; when it
                    // does, focus moves to the list.
                    handled = ForwardMouseToList (DxuiMouseEventKind::Down, DxuiMouseButton::Left, x, y, 0.0f);
                    if (handled) { m_focusMgr.SetFocused (m_eventList); }
                }

                return true;
            }

            if (ev.button == DxuiMouseButton::Right)
            {
                // Right-click inside the list-view header strip surfaces a
                // themed popup menu of column-visibility toggles. Anywhere
                // else, right-click is currently a no-op.
                int  relX        = x - m_layout.listView.left;
                int  relY        = y - m_layout.listView.top;
                int  headerH     = m_eventList->GetHeaderHeightPx();
                int  listWidthPx = m_layout.listView.right - m_layout.listView.left;


                if (!m_eventList->IsHeaderShown())          { return true; }
                if (relX < 0 || relX >= listWidthPx)       { return true; }
                if (relY < 0 || relY >= headerH)           { return true; }

                ShowColumnMenu (x, y);
                return true;
            }

            return false;

        case DxuiMouseEventKind::Up:
            if (ev.button == DxuiMouseButton::Left)
            {
                // Finish any list drag (scrollbar thumb / column resize) the
                // list started on button-down. The pointer may have left the
                // list bounds mid-drag, so forward the release
                // unconditionally. DxuiWindow releases the Win32 capture
                // before routing this release.
                if (m_eventList->IsInteracting())
                {
                    (void) ForwardMouseToList (DxuiMouseEventKind::Up, DxuiMouseButton::Left, x, y, 0.0f);
                    return true;
                }

                if (m_columnMenu.IsVisible())
                {
                    if (m_columnMenu.OnMouse (ev)) { return true; }
                }

                // Route the release to each widget: it clears its own press
                // visual and, on a click-release over itself, fires the
                // callback wired at setup (checkbox change / button click),
                // which folds the outcome back into the panel model.
                for (auto & cb : m_eventChecks)        { cb->OnMouse (ev); }
                m_audioMasterCheck->OnMouse (ev);
                for (auto & cb : m_audioSubChecks)     { cb->OnMouse (ev); }
                m_rawQtCheck->OnMouse   (ev);
                m_driveRadio->OnMouse   (ev);
                m_trackEdit->OnMouse    (ev);
                m_sectorEdit->OnMouse   (ev);

                m_pauseButton->OnMouse (ev);
                m_clearButton->OnMouse (ev);

                return true;
            }

            return false;

        case DxuiMouseEventKind::Wheel:
            // Wheel up scrolls back in history (older events); wheel down
            // scrolls toward the tail. Forwarded to the list, which scrolls
            // only when the pointer is over it (standard control behavior).
            (void) ForwardMouseToList (DxuiMouseEventKind::Wheel, DxuiMouseButton::None, x, y, ev.wheelDelta);
            return true;

        default:
            return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnKey
//
////////////////////////////////////////////////////////////////////////////////

bool Disk2DebugPanel::OnKey (const DxuiKeyEvent & ev)
{
    IDxuiControl *  focused = nullptr;


    // Char events route to the text inputs only; each edit inserts the
    // character when it owns focus and reports whether it consumed it.
    if (ev.kind == DxuiKeyEventKind::Char)
    {
        if (m_trackEdit->OnKey  (ev)) { return true; }
        if (m_sectorEdit->OnKey (ev)) { return true; }
        return false;
    }

    if (ev.kind != DxuiKeyEventKind::Down) { return false; }

    // The column popup, when visible, captures every key-down.
    if (m_columnMenu.IsVisible()) { return m_columnMenu.OnKey (ev); }

    // Focused-first: the focused control sees the key before the panel's
    // Tab traversal. A focused list owns Tab, cycling its header /
    // divider / body sub-stops (column sort / resize / row navigation)
    // and returning false only when Tab steps past either end; focused
    // checkboxes / buttons self-activate on Space / Enter.
    focused = m_focusMgr.Focused();
    if (focused != nullptr && focused->OnKey (ev))
    {
        return true;
    }

    // Tab advances the panel's control focus once the focused control
    // (e.g. the list at a sub-stop boundary) declines the key.
    if ((WPARAM) ev.vk == VK_TAB)
    {
        m_focusMgr.HandleKey ((GetKeyState (VK_SHIFT) & 0x8000) ? DxuiFocusKey::ShiftTab : DxuiFocusKey::Tab);
        return true;
    }

    return false;
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
//  LayoutWidgets
//
//  Reapplies rect / DPI / theme color to every widget owned by the
//  panel. Called whenever the layout slots or theme change.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::LayoutWidgets()
{
    uint32_t  textArgb     = 0xFFE8EEF4;
    uint32_t  invalidArgb  = 0xFFFF6666;



    if (m_theme != nullptr)
    {
        textArgb    = m_theme->Foreground();
        invalidArgb = m_theme->ErrorForeground();
    }

    m_trackFilterLabel->SetText        (m_filter.trackFilterRawQt ? s_kpszTrackQtFilterLabel : s_kpszTrackFilterLabel);
    m_trackFilterLabel->Layout         (m_layout.trackFilterLabel, m_scaler);
    m_trackFilterLabel->SetFontSizeDip (s_kLabelFontDip);
    m_trackFilterLabel->SetColorArgb   (textArgb);
    m_trackFilterLabel->SetHAlign      (DxuiTextRenderer::HAlign::Right);
    m_trackFilterLabel->SetVAlign      (DxuiTextRenderer::VAlign::Center);

    m_sectorFilterLabel->SetText        (s_kpszSectorFilterLabel);
    m_sectorFilterLabel->Layout         (m_layout.sectorFilterLabel, m_scaler);
    m_sectorFilterLabel->SetFontSizeDip (s_kLabelFontDip);
    m_sectorFilterLabel->SetColorArgb   (textArgb);
    m_sectorFilterLabel->SetHAlign      (DxuiTextRenderer::HAlign::Right);
    m_sectorFilterLabel->SetVAlign      (DxuiTextRenderer::VAlign::Center);

    m_trackInvalidLabel->SetText        (BuildInvalidLabel (s_kpszTrackInvalidPrefix, m_trackEdit->Text(), m_filter.trackFilter.RejectedSpans()).c_str());
    m_trackInvalidLabel->Layout         (m_layout.trackInvalidLabel, m_scaler);
    m_trackInvalidLabel->SetFontSizeDip (s_kLabelFontDip);
    m_trackInvalidLabel->SetColorArgb   (invalidArgb);
    m_trackInvalidLabel->SetHAlign      (DxuiTextRenderer::HAlign::Left);
    m_trackInvalidLabel->SetVAlign      (DxuiTextRenderer::VAlign::Center);

    m_sectorInvalidLabel->SetText        (BuildInvalidLabel (s_kpszSectorInvalidPrefix, m_sectorEdit->Text(), m_filter.sectorFilter.RejectedSpans()).c_str());
    m_sectorInvalidLabel->Layout         (m_layout.sectorInvalidLabel, m_scaler);
    m_sectorInvalidLabel->SetFontSizeDip (s_kLabelFontDip);
    m_sectorInvalidLabel->SetColorArgb   (invalidArgb);
    m_sectorInvalidLabel->SetHAlign      (DxuiTextRenderer::HAlign::Left);
    m_sectorInvalidLabel->SetVAlign      (DxuiTextRenderer::VAlign::Center);

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

    m_driveFilterLabel->SetText        (s_kpszDriveFilterLabel);
    m_driveFilterLabel->Layout         (m_layout.driveFilterLabel, m_scaler);
    m_driveFilterLabel->SetFontSizeDip (s_kLabelFontDip);
    m_driveFilterLabel->SetColorArgb   (textArgb);
    m_driveFilterLabel->SetHAlign      (DxuiTextRenderer::HAlign::Left);
    m_driveFilterLabel->SetVAlign      (DxuiTextRenderer::VAlign::Center);

    m_diskEventsLabel->SetText        (s_kpszDiskEventsLabel);
    m_diskEventsLabel->Layout         (m_layout.diskEventsLabel, m_scaler);
    m_diskEventsLabel->SetFontSizeDip (s_kLabelFontDip);
    m_diskEventsLabel->SetColorArgb   (textArgb);
    m_diskEventsLabel->SetHAlign      (DxuiTextRenderer::HAlign::Left);
    m_diskEventsLabel->SetVAlign      (DxuiTextRenderer::VAlign::Center);

    m_audioEventsLabel->SetText        (s_kpszAudioEventsLabel);
    m_audioEventsLabel->Layout         (m_layout.audioEventsLabel, m_scaler);
    m_audioEventsLabel->SetFontSizeDip (s_kLabelFontDip);
    m_audioEventsLabel->SetColorArgb   (textArgb);
    m_audioEventsLabel->SetHAlign      (DxuiTextRenderer::HAlign::Left);
    m_audioEventsLabel->SetVAlign      (DxuiTextRenderer::VAlign::Center);

    // DxuiRadioGroup expects rects in its option records.
    std::vector<DxuiRadioOption>  driveOpts;
    for (int i = 0; i < kDriveRadioCount; i++)
    {
        DxuiRadioOption  opt;
        opt.rect  = m_layout.driveRadios[i];
        opt.label = s_kpszDriveOptionLabels[i];
        driveOpts.push_back (std::move (opt));
    }
    m_driveRadio->SetOptions  (std::move (driveOpts));
    m_driveRadio->SetDpi      (m_dpi);
    // Re-apply selection after SetOptions: ConfigureWidgets calls
    // SetSelected before LayoutWidgets has supplied any options, which
    // makes the initial SetSelected a no-op (out-of-range clamps to -1).
    m_driveRadio->SetSelected (m_filter.driveFilter);

    m_trackEdit->Layout   (m_layout.trackEdit, m_scaler);
    m_trackEdit->SetTheme (m_theme);
    m_trackEdit->SetHwnd  (Hwnd());

    m_sectorEdit->Layout   (m_layout.sectorEdit, m_scaler);
    m_sectorEdit->SetTheme (m_theme);
    m_sectorEdit->SetHwnd  (Hwnd());

    m_pauseButton->Layout (m_layout.pauseButton, m_scaler);
    m_clearButton->Layout (m_layout.clearButton, m_scaler);

    m_eventList->Layout   (m_layout.listView, m_scaler);
    m_eventList->SetTheme (m_theme);

    m_columnMenu.SetDpi   (m_dpi);
    m_columnMenu.SetTheme (m_theme);

    m_tooltip.SetDpi      (m_dpi);
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
        LayoutWidgets();
    });

    m_driveRadio->SetSelected (m_filter.driveFilter);
    m_driveRadio->SetOnChange ([this] (int newIndex)
    {
        m_filter.driveFilter = newIndex;
        OnFilterChanged();
    });

    m_trackEdit->SetMaxLength  (32);
    m_trackEdit->SetOnChange   ([this] (const std::wstring &) { OnTrackEditChanged(); OnFilterChanged(); });

    m_sectorEdit->SetMaxLength (32);
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
    cols.push_back ({ L"Detail", 0, true,  DxuiTextRenderer::HAlign::Left  });
    m_eventList->SetColumns    (std::move (cols));
    m_eventList->SetShowHeader (true);

    // The list owns keyboard column navigation: when it holds focus, its
    // own OnKey cycles the header / divider / body sub-stops on Tab and
    // fires the sort / resize / selection callbacks below.
    m_eventList->SetKeyboardColumnNav (true);

    // The list owns its own scroll / thumb / column-resize / row-select
    // routing via OnMouse; these callbacks fold the semantic outcomes
    // back into the panel (selected event, sort).
    m_eventList->SetOnSelectionChanged ([this] (int row)
    {
        if (row >= 0 && row < (int) m_filteredIndices.size())
        {
            m_listSelectedEventIndex = (int) m_filteredIndices[(size_t) row];
        }
        ApplyListSelection();
    });
    m_eventList->SetOnSortColumn ([this] (int col)
    {
        SortByColumn (col);
    });
    m_eventList->SetOnColumnResized ([] (int, int) {});

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
    DebugDialogProjection::DrainAndProject (m_ring, m_events, dropped, m_uptimeAnchor);

    RebuildFilteredIndices();
    PushListViewRows();
    ApplyListSelection();
}





////////////////////////////////////////////////////////////////////////////////
//
//  RebuildFilteredIndices
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
//  PushListViewRows
//
//  Manual virtualization: only push the rows that fit visibly within
//  the DxuiListView slot. Walking from the tail keeps the most recent
//  events visible, matching the legacy auto-tail behavior.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::PushListViewRows()
{
    size_t  total = m_filteredIndices.size();
    size_t  cap   = m_events.size();
    std::vector<std::vector<DxuiListView::Cell>>  rows;


    rows.reserve (total);

    for (size_t k = 0; k < total; k++)
    {
        size_t  idx = m_filteredIndices[k];
        if (idx >= cap) { continue; }
        const Disk2EventDisplay & e = m_events[idx];

        std::vector<DxuiListView::Cell>  row;
        row.push_back ({ std::wstring (e.wallStr.data()),   false });
        row.push_back ({ std::wstring (e.uptimeStr.data()), false });
        row.push_back ({ std::wstring (e.cycleStr.data()),  false });

        wchar_t  driveBuf[8] = {};
        if (e.drive == Disk2EventDisplay::kFieldNotApplicable)
        {
            row.push_back ({ L"", false });
        }
        else
        {
            swprintf_s (driveBuf, L"%d", e.drive + 1);
            row.push_back ({ std::wstring (driveBuf), false });
        }

        std::wstring_view  label = DebugDialogProjection::EventLabel (e.category, e.type);
        row.push_back ({ std::wstring (label), false });
        row.push_back ({ e.detail, false });

        rows.push_back (std::move (row));
    }

    m_eventList->SetRows (std::move (rows));
    m_eventList->UpdateAutoFitFromRows();
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
    PushListViewRows();
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
    m_trackInvalidLabel->SetText (BuildInvalidLabel (s_kpszTrackInvalidPrefix, m_trackEdit->Text(), m_filter.trackFilter.RejectedSpans()).c_str());
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
    m_sectorInvalidLabel->SetText (BuildInvalidLabel (s_kpszSectorInvalidPrefix, m_sectorEdit->Text(), m_filter.sectorFilter.RejectedSpans()).c_str());
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
    m_listSelectedEventIndex = -1;
    m_eventList->ResetAutoFit();
    PushListViewRows();
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

void Disk2DebugPanel::OnMotorCommandOn ()
{
    Disk2Event  e = MakeStampedEvent (EventCategory::Controller, Disk2EventType::MotorCommandOn);
    PublishToRing (e);
}
void Disk2DebugPanel::OnMotorEngaged ()
{
    Disk2Event  e = MakeStampedEvent (EventCategory::Controller, Disk2EventType::MotorEngaged);
    PublishToRing (e);
}
void Disk2DebugPanel::OnMotorCommandOff ()
{
    Disk2Event  e = MakeStampedEvent (EventCategory::Controller, Disk2EventType::MotorCommandOff);
    PublishToRing (e);
}
void Disk2DebugPanel::OnMotorDisengaged ()
{
    Disk2Event  e = MakeStampedEvent (EventCategory::Controller, Disk2EventType::MotorDisengaged);
    PublishToRing (e);
}

void Disk2DebugPanel::OnHeadStep (int prevQt, int newQt)
{
    Disk2Event  e = MakeStampedEvent (EventCategory::Controller, Disk2EventType::HeadStep);
    e.payload.step.prevQt = prevQt;
    e.payload.step.newQt  = newQt;
    PublishToRing (e);
}

void Disk2DebugPanel::OnHeadBump (int atQt)
{
    Disk2Event  e = MakeStampedEvent (EventCategory::Controller, Disk2EventType::HeadBump);
    e.payload.bump.atQt = atQt;
    PublishToRing (e);
}

void Disk2DebugPanel::OnAddressMark (int track, int sector, int volume)
{
    Disk2Event  e = MakeStampedEvent (EventCategory::Controller, Disk2EventType::AddrMark);
    e.payload.addrMark.track  = track;
    e.payload.addrMark.sector = sector;
    e.payload.addrMark.volume = volume;
    PublishToRing (e);
}

void Disk2DebugPanel::OnDataMarkRead (int track, int sector, int volume, int byteCount)
{
    Disk2Event  e = MakeStampedEvent (EventCategory::Controller, Disk2EventType::DataRead);
    e.payload.dataMark.track     = track;
    e.payload.dataMark.sector    = sector;
    e.payload.dataMark.volume    = volume;
    e.payload.dataMark.byteCount = byteCount;
    PublishToRing (e);
}

void Disk2DebugPanel::OnDataMarkWrite (int track, int sector, int volume, int byteCount)
{
    Disk2Event  e = MakeStampedEvent (EventCategory::Controller, Disk2EventType::DataWrite);
    e.payload.dataMark.track     = track;
    e.payload.dataMark.sector    = sector;
    e.payload.dataMark.volume    = volume;
    e.payload.dataMark.byteCount = byteCount;
    PublishToRing (e);
}

void Disk2DebugPanel::OnDriveSelect (int drive)
{
    m_currentDrive = drive;
    Disk2Event  e = MakeStampedEvent (EventCategory::Controller, Disk2EventType::DriveSelect);
    e.drive               = (int8_t) drive;
    e.payload.drive.drive = drive;
    PublishToRing (e);
}

void Disk2DebugPanel::OnDiskInserted (int drive)
{
    Disk2Event  e = MakeStampedEvent (EventCategory::Controller, Disk2EventType::DiskInserted);
    e.drive               = (int8_t) drive;
    e.payload.drive.drive = drive;
    PublishToRing (e);
}

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

void Disk2DebugPanel::OnAudioRestarted (SoundKind kind, int drive)
{
    Disk2Event  e = MakeStampedEvent (EventCategory::Audio, Disk2EventType::AudioRestarted);
    e.drive                = (int8_t) drive;
    e.payload.audio.kind   = kind;
    e.payload.audio.drive  = drive;
    e.payload.audio.reason = SilentReason::DriveAudioDisabled;
    PublishToRing (e);
}

void Disk2DebugPanel::OnAudioContinued (SoundKind kind, int drive)
{
    Disk2Event  e = MakeStampedEvent (EventCategory::Audio, Disk2EventType::AudioContinued);
    e.drive                = (int8_t) drive;
    e.payload.audio.kind   = kind;
    e.payload.audio.drive  = drive;
    e.payload.audio.reason = SilentReason::DriveAudioDisabled;
    PublishToRing (e);
}

void Disk2DebugPanel::OnAudioSilent (SoundKind kind, int drive, SilentReason reason)
{
    Disk2Event  e = MakeStampedEvent (EventCategory::Audio, Disk2EventType::AudioSilent);
    e.drive                = (int8_t) drive;
    e.payload.audio.kind   = kind;
    e.payload.audio.drive  = drive;
    e.payload.audio.reason = reason;
    PublishToRing (e);
}

void Disk2DebugPanel::OnAudioLoopStarted (SoundKind kind, int drive)
{
    Disk2Event  e = MakeStampedEvent (EventCategory::Audio, Disk2EventType::AudioLoopStarted);
    e.drive                = (int8_t) drive;
    e.payload.audio.kind   = kind;
    e.payload.audio.drive  = drive;
    e.payload.audio.reason = SilentReason::DriveAudioDisabled;
    PublishToRing (e);
}

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
    int64_t  now = NowMs();

    for (size_t i = 0; i < m_eventChecks.size(); ++i)
    {
        if (m_eventChecks[i]->HitTest (x, y))
        {
            m_tooltip.RequestShow (m_eventChecks[i]->Rect(), s_kpszEventCheckTips[i], now);
            return;
        }
    }

    if (m_audioMasterCheck->HitTest (x, y))
    {
        m_tooltip.RequestShow (m_audioMasterCheck->Rect(), s_kpszAudioMasterTip, now);
        return;
    }

    for (size_t i = 0; i < m_audioSubChecks.size(); ++i)
    {
        if (m_audioSubChecks[i]->HitTest (x, y))
        {
            m_tooltip.RequestShow (m_audioSubChecks[i]->Rect(), s_kpszAudioSubTips[i], now);
            return;
        }
    }

    if (m_rawQtCheck->HitTest (x, y))
    {
        m_tooltip.RequestShow (m_rawQtCheck->Rect(), s_kpszRawQtTip, now);
        return;
    }

    int  driveHit = m_driveRadio->HitTest (x, y);
    if (driveHit >= 0 && driveHit < (int) m_driveRadio->Options().size())
    {
        m_tooltip.RequestShow (m_driveRadio->Options()[driveHit].rect,
                               s_kpszDriveRadioTips[driveHit],
                               now);
        return;
    }

    if (m_trackEdit->HitTest (x, y))
    {
        m_tooltip.RequestShow (m_trackEdit->Rect(), s_kpszTrackEditTip, now);
        return;
    }

    if (m_sectorEdit->HitTest (x, y))
    {
        m_tooltip.RequestShow (m_sectorEdit->Rect(), s_kpszSectorEditTip, now);
        return;
    }

    m_tooltip.RequestHide (now);
}
