#include "Pch.h"

#include "Ui/Debugger/DebuggerKeySchemes.h"
#include "Ui/Debugger/DebuggerWindow.h"
#include "Debugger/Reverse/HistoryTimelineClick.h"
#include "Ui/Debugger/DebuggerStatusText.h"
#include "Debugger/Source/SourcePathList.h"
#include "Video/MachineFrameRenderer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ConfigureTimeline
//
//  The history timeline is a toolbar holding the strip of thumbnails, with
//  where history begins before the pictures and Live after them, where
//  live time is, and a grab handle; it docks and floats through a Dxui
//  toolbar host of its own, as the command bar does, and keeps its place in
//  the debugger's preferences. Docked, the strip runs the whole length of
//  its band, less what the command bar takes when it shares the band;
//  floating, it keeps the length it was given. The pictures come from the host, which draws
//  them from its recorded history. The band is taller than a button by room
//  above and below the pictures, so a replay's playhead line can show the
//  host's time above it and the time since power-on below it.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ConfigureTimeline()
{
    constexpr int    kTimelineId  = 1;
    constexpr int    kStripGroup  = 1;
    constexpr float  kLabelRoomDp = 20.0f;
    constexpr int    kLabelRooms  = 2;      // above the pictures and below them



    HistoryThumbnails   * thumbnails = (m_host != nullptr) ? m_host->GetHistoryThumbnails() : nullptr;
    DxuiToolbar::Entry    entry;



    m_timelineCommand        = std::make_shared<DxuiCommand>();
    m_timelineCommand->id    = kTimelineId;
    m_timelineCommand->label = L"History timeline";
    m_timelineCommand->tip   = L"History timeline";

    entry.command       = m_timelineCommand;
    entry.custom        = &m_timelineStrip;
    entry.group         = kStripGroup;
    entry.neverOverflow = true;
    entry.fill          = true;

    m_timelineStrip.SetSource       (thumbnails);
    m_timelineStrip.SetAspect       ((float) MachineFrameRenderer::kWidth / (float) MachineFrameRenderer::kHeight);
    m_timelineStrip.SetPopupHost    (GetPopupHost());
    m_timelineStrip.SetLabelRoomDp  (kLabelRoomDp);
    m_timelineStrip.SetTextRenderer (GetTextRenderer());

    m_timelineBar->SetTextRenderer (GetTextRenderer());
    m_timelineBar->SetPopupHost    (GetPopupHost());
    m_timelineBar->SetGrabHandle   (true);
    m_timelineBar->SetBandDp       (DxuiToolbar::GetBandDip() + (int) kLabelRoomDp * kLabelRooms);
    m_timelineBar->SetEntries      ({ entry });

    if (thumbnails != nullptr)
    {
        thumbnails->SetOnSeek  ([this] (const HistoryThumbnailCell & cell) { OnTimelineSeek (cell); });
        thumbnails->SetOnScrub ([this] (uint64_t cycle, bool isFinal) { OnTimelineScrub (cycle, isFinal); });

        //  The host's time of day above, or the time since power-on alone
        //  when the host's time is unknown, and the time since power-on below.
        thumbnails->SetLabeler ([] (uint64_t cycle, uint64_t wallTime, std::wstring & outTop, std::wstring & outBottom)
        {
            outTop    = DebuggerStatusText::FormatWallClock (wallTime, LOCALE_NAME_USER_DEFAULT);
            outBottom = DebuggerStatusText::GetPowerText (cycle, LOCALE_NAME_USER_DEFAULT);

            if (outTop.empty())
            {
                outTop.swap (outBottom);
            }
        });
    }

    m_timelineHost.Attach       (this, m_timelineBar, nullptr, m_hInstance);
    m_timelineHost.SetFillsEdge (true);

    m_timelineHost.SetOnLayout     ([this] { LayoutWidgets(); });
    m_timelineHost.SetOnFloatMouse ([this] (const DxuiMouseEvent & ev) { return RouteTimelineMouse (ev); });
    m_timelineHost.SetOnDragStart  ([this] { m_timelineStrip.HidePreview(); });

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
//  DebuggerWindow::RouteTimelineMouse
//
//  The strip under the pointer; leaving it hides the preview. The grab
//  handle's drags are the toolbar host's, ahead of any of this.
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

    //  History's start time appears once there is history, and the strip
    //  makes room beside its pictures for it.
    if (m_timelineStrip.HasOutgrownLabelRoom())
    {
        LayoutWidgets();
    }

    if (m_pendingSeek.has_value() && m_snapshot != nullptr && m_snapshot->isPaused)
    {
        cell = *m_pendingSeek;
        m_pendingSeek.reset();

        OnTimelineSeek (cell);
    }

    SyncTimelineScrub();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::OnTimelineScrub
//
//  The playhead line dragged to the cycle under the pointer. The plan comes
//  from HistoryTimelineScrub: a drag of a running machine stops it first;
//  while the line moves, each frame seeks once, to where it is then, and
//  not while the last seek is still on its way; when the line is let go,
//  the machine lands exactly there at once, and one that was running runs on
//  from there, replaying the recorded future, while one that was stopped
//  stays stopped.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::OnTimelineScrub (
    uint64_t  cycle,
    bool      isFinal)
{
    bool  paused = m_snapshot != nullptr && m_snapshot->isPaused;



    if (m_host == nullptr)
    {
        return;
    }

    ApplyTimelineScrub (m_timelineScrub.OnDragged (cycle, isFinal, paused, m_host->IsHistorySeekBusy()));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::SyncTimelineScrub
//
//  Once a frame: the seek the line's drag is due, if any.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::SyncTimelineScrub()
{
    bool  paused = m_snapshot != nullptr && m_snapshot->isPaused;



    if (m_host == nullptr || !m_timelineScrub.IsScrubbing())
    {
        return;
    }

    ApplyTimelineScrub (m_timelineScrub.OnFrame (paused, m_host->IsHistorySeekBusy()));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ApplyTimelineScrub
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ApplyTimelineScrub (const HistoryTimelineScrubStep & step)
{
    if (step.pauseFirst)
    {
        m_host->PauseDebugger();
    }

    if (step.seek)
    {
        m_host->SeekHistoryCycle (step.cycle);
    }

    if (step.run)
    {
        RunCommandBarEntry (DebuggerCommands::kRun);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::OnTimelineSeek
//
//  The plan comes from HistoryTimelineClick: a click while the machine runs
//  stops it first and acts once it has; then the machine moves to the
//  cycle clicked and runs on from there. The seek, the go live and the run
//  all cross to the CPU thread through one queue, so they land in order.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::OnTimelineSeek (const HistoryThumbnailCell & cell)
{
    HistoryThumbnails         * thumbnails = (m_host != nullptr) ? m_host->GetHistoryThumbnails() : nullptr;
    bool                        paused     = m_snapshot != nullptr && m_snapshot->isPaused;
    bool                        behindLive = (thumbnails != nullptr) ? thumbnails->IsBehindLive() : m_snapshot != nullptr && m_snapshot->history.isBehindLive;
    HistoryTimelineClickPlan    plan       = HistoryTimelineClick::Plan (cell, paused, behindLive);



    if (m_host == nullptr)
    {
        return;
    }

    if (plan.pauseFirst)
    {
        m_pendingSeek = cell;
        m_host->PauseDebugger();
        return;
    }

    if (plan.seek)
    {
        m_host->SeekHistoryCycle (plan.cycle);
    }

    if (plan.goLive)
    {
        RunCommandBarEntry (DebuggerCommands::kGoLive);
    }

    if (plan.run)
    {
        RunCommandBarEntry (DebuggerCommands::kRun);
    }
}





