#include "Pch.h"

#include "Ui/Debugger/DebuggerKeySchemes.h"
#include "Ui/Debugger/DebuggerWindow.h"
#include "Debugger/Source/SourcePathList.h"
#include "Video/MachineFrameRenderer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ConfigureTimeline
//
//  The history timeline is a toolbar with one entry, the strip of
//  thumbnails, and a grab handle; it docks and floats through a Dxui toolbar
//  host of its own, as the command bar does, and keeps its place in the
//  debugger's preferences. The pictures come from the host, which draws them
//  from its recorded history.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ConfigureTimeline()
{
    constexpr int  kTimelineId = 1;



    HistoryThumbnails   * thumbnails = (m_host != nullptr) ? m_host->GetHistoryThumbnails() : nullptr;
    DxuiToolbar::Entry    entry;



    m_timelineCommand        = std::make_shared<DxuiCommand>();
    m_timelineCommand->id    = kTimelineId;
    m_timelineCommand->label = L"History timeline";
    m_timelineCommand->tip   = L"History timeline";

    entry.command       = m_timelineCommand;
    entry.custom        = &m_timelineStrip;
    entry.neverOverflow = true;

    m_timelineStrip.SetSource    (thumbnails);
    m_timelineStrip.SetAspect    ((float) MachineFrameRenderer::kWidth / (float) MachineFrameRenderer::kHeight);
    m_timelineStrip.SetPopupHost (GetPopupHost());

    m_timelineBar->SetTextRenderer (GetTextRenderer());
    m_timelineBar->SetPopupHost    (GetPopupHost());
    m_timelineBar->SetGrabHandle   (true);
    m_timelineBar->SetEntries      ({ entry });

    if (thumbnails != nullptr)
    {
        thumbnails->SetOnSeek ([this] (const HistoryThumbnailCell & cell) { OnTimelineSeek (cell); });
    }

    m_timelineHost.Attach (this, m_timelineBar, nullptr, m_hInstance);

    m_timelineHost.SetOnLayout        ([this] { LayoutWidgets(); });
    m_timelineHost.SetOnFloatMouse    ([this] (const DxuiMouseEvent & ev) { return RouteTimelineMouse (ev); });
    m_timelineHost.SetOnMoveLoopFrame ([this] { RunModalLoopTick(); });
    m_timelineHost.SetOnDragStart     ([this] { m_timelineStrip.HidePreview(); });

    m_timelineHost.SetOnSave ([this] (const std::wstring & text)
    {
        if (m_host != nullptr)
        {
            m_host->SetDebuggerTimelineDock (SourcePathList::WideToUtf8 (text));
        }
    });

    m_timelineHost.SetOnFloatCreated ([this] (DxuiToolbarWindow & window)
    {
        window.SetTheme  (m_theme);
        window.SetKeyMap (&DebuggerKeySchemes::GetMap (m_keyScheme));
    });

    //  The preview opens from whichever window holds the strip.
    m_timelineHost.SetOnFloatChanged ([this]
    {
        DxuiToolbarWindow  * window = m_timelineHost.GetFloatWindow();
        DxuiHwndSource     * popups = (window != nullptr) ? window->GetPopupHost() : GetPopupHost();



        m_timelineStrip.SetPopupHost (popups);
        m_timelineBar->SetPopupHost  (popups);
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::PlaceTimeline
//
//  Docked, the timeline runs the length of its edge of `area` and takes its
//  band out of it; floating or being carried, it asks for its default length
//  instead, so a window torn off is not the width of the debugger.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::PlaceTimeline (RECT & area)
{
    const DxuiToolbarDock  & dock     = m_timelineHost.GetDock();
    bool                     floating = m_timelineHost.IsFloating();
    bool                     carried  = m_timelineHost.IsDragging();
    int                      band     = m_scaler.ToPx (DxuiToolbar::GetBandDip());
    int                      edge     = dock.IsVertical() ? area.bottom - area.top : area.right - area.left;



    if (m_timelineBar == nullptr)
    {
        return;
    }

    m_timelineStrip.SetPreferredLengthPx ((floating || carried) ? 0 : edge);
    m_timelineHost.Layout (area, RECT { 0, 0, m_widthDip, m_heightDip }, m_scaler);

    if (floating)
    {
        return;
    }

    switch (dock.edge)
    {
    case DxuiToolbarDock::Edge::Bottom: area.bottom -= band; break;
    case DxuiToolbarDock::Edge::Left:   area.left   += band; break;
    case DxuiToolbarDock::Edge::Right:  area.right  -= band; break;
    default:                            area.top    += band; break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RouteTimelineMouse
//
//  The grab handle's drags first, then the strip under the pointer; leaving
//  it hides the preview.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::RouteTimelineMouse (const DxuiMouseEvent & ev)
{
    int   x    = ev.positionDip.x;
    int   y    = ev.positionDip.y;
    RECT  bar  = {};
    bool  over = false;



    if (m_timelineBar == nullptr)
    {
        return false;
    }

    if (m_timelineHost.RouteDrag (ev))
    {
        return true;
    }

    bar  = m_timelineBar->GetBounds();
    over = x >= bar.left && x < bar.right && y >= bar.top && y < bar.bottom;

    if (!over)
    {
        m_timelineBar->OnToolbarMouseLeave();
        return false;
    }

    switch (ev.kind)
    {
    case DxuiMouseEventKind::Move:
        return m_timelineBar->OnToolbarMouseMove (x, y);

    case DxuiMouseEventKind::Down:
        m_timelineStrip.HidePreview();
        return m_timelineBar->OnToolbarLButtonDown (x, y);

    case DxuiMouseEventKind::Up:
        return m_timelineBar->OnToolbarLButtonUp (x, y);

    default:
        return true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::SyncTimeline
//
//  Once a frame. The host draws pictures only while the strip can be seen,
//  and a seek asked for while the machine ran is made once it has stopped.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::SyncTimeline()
{
    HistoryThumbnails     * thumbnails = (m_host != nullptr) ? m_host->GetHistoryThumbnails() : nullptr;
    HWND                    hwnd       = GetHwnd();
    bool                    shown      = hwnd != nullptr && IsWindowVisible (hwnd) && !IsIconic (hwnd);
    HistoryThumbnailCell    cell;



    m_timelineHost.Sync();

    if (thumbnails == nullptr)
    {
        return;
    }

    thumbnails->SetVisible (shown && m_timelineStrip.GetCellCount() > 0);
    m_timelineStrip.Sync();

    if (m_pendingSeek.has_value() && m_snapshot != nullptr && m_snapshot->isPaused)
    {
        cell = *m_pendingSeek;
        m_pendingSeek.reset();

        OnTimelineSeek (cell);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::OnTimelineSeek
//
//  History moves only under a stopped machine, so a click while it runs
//  stops it first and seeks once it has. The live end goes live, which a
//  machine already live has no need of.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::OnTimelineSeek (const HistoryThumbnailCell & cell)
{
    bool  paused     = m_snapshot != nullptr && m_snapshot->isPaused;
    bool  behindLive = m_snapshot != nullptr && m_snapshot->history.isBehindLive;



    if (m_host == nullptr)
    {
        return;
    }

    if (!paused)
    {
        m_pendingSeek = cell;
        m_host->PauseDebugger();
        return;
    }

    if (cell.isLive)
    {
        if (behindLive)
        {
            RunCommandBarEntry (DebuggerCommands::kGoLive);
        }

        return;
    }

    m_host->SeekHistory (cell.position);
}
