#include "Pch.h"

#include "Ui/Chrome/RewindBar.h"
#include "Config/HistorySettings.h"
#include "Debugger/Reverse/HistoryThumbnails.h"
#include "Debugger/Reverse/HistoryTimelineClick.h"
#include "Debugger/Reverse/ReverseCommand.h"
#include "Shell/Components/ShellHistory.h"
#include "Ui/Debugger/DebuggerStatusText.h"
#include "Video/MachineFrameRenderer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RewindBar::RewindBar
//
////////////////////////////////////////////////////////////////////////////////

RewindBar::RewindBar()
{
    constexpr int  kStripId    = 1;
    constexpr int  kStripGroup = 1;



    DxuiToolbar::Entry  entry;



    m_command        = std::make_shared<DxuiCommand>();
    m_command->id    = kStripId;
    m_command->label = L"History";
    m_command->tip   = L"History";

    entry.command       = m_command;
    entry.custom        = &m_strip;
    entry.group         = kStripGroup;
    entry.neverOverflow = true;
    entry.fill          = true;

    m_strip.SetAspect      ((float) MachineFrameRenderer::kWidth / (float) MachineFrameRenderer::kHeight);
    m_strip.SetLabelRoomDp (kLabelRoomDp);

    m_bar.SetBandDp  (DxuiToolbar::GetBandDip() + (int) kLabelRoomDp * kLabelRooms);
    m_bar.SetEntries ({ entry });
    m_bar.SetVisible (false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  RewindBar::IsInOpeningBand
//
////////////////////////////////////////////////////////////////////////////////

bool RewindBar::IsInOpeningBand (
    POINT         pointer,
    const RECT  & screen,
    UINT          dpi,
    bool          isCaptured,
    bool          isHistoryOn)
{
    int  bandPx = (int) std::lround (kOpeningBandDip * (float) dpi / (float) USER_DEFAULT_SCREEN_DPI);



    if (isCaptured || !isHistoryOn)
    {
        return false;
    }

    return pointer.x >= screen.left && pointer.x < screen.right && pointer.y >= screen.bottom - bandPx && pointer.y < screen.bottom;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RewindBar::IsInOpeningBand
//
//  The point on the screen, and the point kOpeningBandDip below it off it.
//
////////////////////////////////////////////////////////////////////////////////

bool RewindBar::IsInOpeningBand (
    POINT                 pointer,
    const ScreenTestFn  & isOnScreen,
    UINT                  dpi,
    bool                  isCaptured,
    bool                  isHistoryOn)
{
    int  bandPx = (int) std::lround (kOpeningBandDip * (float) dpi / (float) USER_DEFAULT_SCREEN_DPI);



    if (isCaptured || !isHistoryOn || !isOnScreen)
    {
        return false;
    }

    return isOnScreen (pointer.x, pointer.y) && !isOnScreen (pointer.x, pointer.y + bandPx);
}





////////////////////////////////////////////////////////////////////////////////
//
//  RewindBar::FindScreen
//
//  The screen's rectangle around a point on it: its top and bottom straight
//  above and below the point, and its left and right edges across its middle.
//
////////////////////////////////////////////////////////////////////////////////

RECT RewindBar::FindScreen (
    POINT                 inside,
    const ScreenTestFn  & isOnScreen)
{
    constexpr int  kMostSteps = 16384;



    RECT  screen = { inside.x, inside.y, inside.x + 1, inside.y + 1 };
    int   middle = 0;
    int   step   = 0;



    for (step = 0; step < kMostSteps && isOnScreen (inside.x, screen.top - 1); step++)
    {
        screen.top--;
    }

    for (step = 0; step < kMostSteps && isOnScreen (inside.x, screen.bottom); step++)
    {
        screen.bottom++;
    }

    middle = (screen.top + screen.bottom) / 2;

    for (step = 0; step < kMostSteps && isOnScreen (screen.left - 1, middle); step++)
    {
        screen.left--;
    }

    for (step = 0; step < kMostSteps && isOnScreen (screen.right, middle); step++)
    {
        screen.right++;
    }

    return screen;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RewindBar::Configure
//
//  The strip's pictures are the bar's thumbnails of history, labeled as the
//  debugger's timeline is: the host's time of day above, or the time since
//  power-on alone when the host's time is unknown, and the time since
//  power-on below.
//
////////////////////////////////////////////////////////////////////////////////

void RewindBar::Configure (
    ShellHistory       & history,
    IDxuiTextRenderer  * text)
{
    HistoryThumbnails  & thumbnails = history.GetBarThumbnails();



    m_history = &history;

    m_strip.SetSource       (&thumbnails);
    m_strip.SetTextRenderer (text);
    m_bar.SetTextRenderer   (text);

    thumbnails.SetOnSeek  ([this] (const HistoryThumbnailCell & cell) { OnSeek (cell); });
    thumbnails.SetOnScrub ([this] (uint64_t cycle, bool isFinal) { OnScrub (cycle, isFinal); });

    thumbnails.SetLabeler ([] (uint64_t cycle, uint64_t wallTime, std::wstring & outTop, std::wstring & outBottom)
    {
        outTop    = DebuggerStatusText::FormatWallClock (wallTime, LOCALE_NAME_USER_DEFAULT);
        outBottom = DebuggerStatusText::GetPowerText (cycle, LOCALE_NAME_USER_DEFAULT);

        if (outTop.empty())
        {
            outTop.swap (outBottom);
        }
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  RewindBar::SetPopupHost
//
//  The strip's preview opens as a popup of the window. The chrome sets the
//  tooltip's host along with the chrome tooltips'.
//
////////////////////////////////////////////////////////////////////////////////

void RewindBar::SetPopupHost (DxuiHwndSource * host)
{
    m_strip.SetPopupHost (host);
    m_bar.SetPopupHost   (host);
}





////////////////////////////////////////////////////////////////////////////////
//
//  RewindBar::Adopt
//
////////////////////////////////////////////////////////////////////////////////

void RewindBar::Adopt (DxuiPanel & root)
{
    root.Adopt (m_bar);
}





////////////////////////////////////////////////////////////////////////////////
//
//  RewindBar::Layout
//
//  With a screen test, the screen is found when the bar opens, and found
//  again around the middle of the last one while it is open; a bar whose
//  screen has moved off that point closes.
//
////////////////////////////////////////////////////////////////////////////////

void RewindBar::Layout (
    const RECT           & screen,
    const DxuiDpiScaler  & scaler)
{
    POINT  middle = { (m_screen.left + m_screen.right) / 2, (m_screen.top + m_screen.bottom) / 2 };



    m_scaler = scaler;

    if (!m_screenTest)
    {
        PlaceBar (screen);
    }
    else if (m_isOpen && m_screenTest (middle.x, middle.y))
    {
        PlaceBar (FindScreen (middle, m_screenTest));
    }
    else
    {
        Close();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RewindBar::PlaceBar
//
//  The bar spans the screen's width at the bottom of it, as tall as its band.
//
////////////////////////////////////////////////////////////////////////////////

void RewindBar::PlaceBar (const RECT & screen)
{
    RECT  bounds = screen;



    m_screen   = screen;
    bounds.top = std::max (screen.top, screen.bottom - m_scaler.ToPx (m_bar.GetBandDp()));

    m_bar.Layout (bounds, m_scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  RewindBar::OfferMouse
//
//  Closed, a move into the opening band opens the bar. Open, a move over it
//  is its hover, its tip and its drag; a move off it closes it unless a
//  press on it is down. A press on it and its release go to the bar.
//
////////////////////////////////////////////////////////////////////////////////

bool RewindBar::OfferMouse (
    DxuiMouseEventKind  kind,
    int                 x,
    int                 y,
    bool                isCaptured,
    int64_t             nowMs)
{
    RECT             bounds  = {};
    RECT             anchor  = {};
    POINT            pointer = { x, y };
    bool             isOver  = false;
    bool             isTaken = false;
    const wchar_t  * tip     = nullptr;



    if (m_history == nullptr || m_isHidden)
    {
        return false;
    }

    if (kind == DxuiMouseEventKind::Move && !m_isOpen && m_screenTest && IsInOpeningBand (pointer, m_screenTest, m_scaler.GetDpi(), isCaptured, m_history->GetSettings().isRecording))
    {
        PlaceBar (FindScreen (pointer, m_screenTest));
        Open();
    }
    else if (kind == DxuiMouseEventKind::Move && !m_isOpen && !m_screenTest && IsInOpeningBand (pointer, m_screen, m_scaler.GetDpi(), isCaptured, m_history->GetSettings().isRecording))
    {
        Open();
    }

    bounds = m_bar.GetBounds();
    isOver = m_isOpen && PtInRect (&bounds, pointer) != FALSE;


    switch (kind)
    {
        case DxuiMouseEventKind::Move:
            if (!isOver && !m_isPressed)
            {
                Close();
                break;
            }

            m_isLeftForFlyout = false;

            (void) m_bar.OnToolbarMouseMove (x, y);

            tip = m_isPressed ? nullptr : m_bar.GetTooltipAt (x, y, anchor);

            if (tip != nullptr)
            {
                m_tooltip.RequestShow (anchor, tip, nowMs);
            }
            else
            {
                m_tooltip.RequestHide (nowMs);
            }

            isTaken = true;
            break;

        case DxuiMouseEventKind::Down:
            if (isOver)
            {
                m_strip.HidePreview();
                m_tooltip.HideImmediate();

                (void) m_bar.OnToolbarLButtonDown (x, y);

                m_isPressed = true;
                isTaken     = true;
            }

            break;

        case DxuiMouseEventKind::Up:
            if (m_isPressed)
            {
                m_isPressed = false;

                (void) m_bar.OnToolbarLButtonUp (x, y);

                if (!isOver)
                {
                    Close();
                }

                isTaken = true;
            }

            break;

        case DxuiMouseEventKind::Leave:
            //  The pointer went into the strip's flyout, a separate window:
            //  the bar closes once the flyout does.
            if (m_strip.GetFlyout().IsOpen() && m_strip.GetFlyout().IsPointerOn())
            {
                m_isLeftForFlyout = true;
            }
            else if (!m_isPressed)
            {
                Close();
            }

            break;

        default:
            break;
    }

    return isTaken;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RewindBar::Open
//
////////////////////////////////////////////////////////////////////////////////

void RewindBar::Open()
{
    m_isOpen = true;

    ShowState();
}





////////////////////////////////////////////////////////////////////////////////
//
//  RewindBar::Close
//
////////////////////////////////////////////////////////////////////////////////

void RewindBar::Close()
{
    if (!m_isOpen)
    {
        return;
    }

    m_isOpen          = false;
    m_isPressed       = false;
    m_isLeftForFlyout = false;

    m_bar.OnToolbarMouseLeave();
    m_strip.HidePreview();
    m_tooltip.HideImmediate();

    ShowState();
}





////////////////////////////////////////////////////////////////////////////////
//
//  RewindBar::SetHidden
//
////////////////////////////////////////////////////////////////////////////////

void RewindBar::SetHidden (bool isHidden)
{
    m_isHidden = isHidden;

    if (isHidden)
    {
        m_strip.HidePreview();
        m_tooltip.HideImmediate();
    }

    ShowState();
}





////////////////////////////////////////////////////////////////////////////////
//
//  RewindBar::ShowState
//
//  The pictures are drawn only while the bar is open and not hidden.
//
////////////////////////////////////////////////////////////////////////////////

void RewindBar::ShowState()
{
    bool  isShown = m_isOpen && !m_isHidden;



    m_bar.SetVisible (isShown);

    if (m_history != nullptr)
    {
        m_history->GetBarThumbnails().SetVisible (isShown && m_strip.GetCellCount() > 0);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RewindBar::Sync
//
//  Once a frame. History's start time appears once there is history, and
//  the strip is laid out again with room beside its pictures for it.
//
////////////////////////////////////////////////////////////////////////////////

void RewindBar::Sync()
{
    if (m_history == nullptr)
    {
        return;
    }

    ShowState();

    if (!m_isOpen)
    {
        return;
    }

    if (m_isLeftForFlyout && !m_strip.GetFlyout().IsOpen())
    {
        Close();
        return;
    }

    m_strip.Sync();

    if (m_strip.HasOutgrownLabelRoom())
    {
        PlaceBar (m_screen);
    }

    if (m_scrub.IsScrubbing())
    {
        ApplyScrub (m_scrub.OnFrame (true, m_history->IsSeekBusy()));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RewindBar::OnSeek
//
//  A click on the strip. A running machine is paused first, then moved to
//  the cycle clicked, or to the live end, and left paused there.
//
////////////////////////////////////////////////////////////////////////////////

void RewindBar::OnSeek (const HistoryThumbnailCell & cell)
{
    HistoryTimelineClickPlan  plan;



    m_history->PauseMachine();

    plan = HistoryTimelineClick::Plan (cell, true, m_history->GetBarThumbnails().IsBehindLive());

    if (plan.seek)
    {
        m_history->SeekCycle (plan.cycle, false);
    }

    if (plan.goLive)
    {
        m_history->PostCommand (ReverseCommand::GoLive);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RewindBar::OnScrub
//
//  The playhead line dragged to a cycle. A running machine is paused first;
//  while the line moves, each frame seeks once, to where it is then, and
//  once it is let go the machine lands exactly there and stays paused.
//
////////////////////////////////////////////////////////////////////////////////

void RewindBar::OnScrub (
    uint64_t  cycle,
    bool      isFinal)
{
    m_history->PauseMachine();

    ApplyScrub (m_scrub.OnDragged (cycle, isFinal, true, m_history->IsSeekBusy()));
}





////////////////////////////////////////////////////////////////////////////////
//
//  RewindBar::ApplyScrub
//
////////////////////////////////////////////////////////////////////////////////

void RewindBar::ApplyScrub (const HistoryTimelineScrubStep & step)
{
    if (step.seek || step.isSettled)
    {
        m_history->SeekCycle (step.cycle, step.isInterim);
    }
}
