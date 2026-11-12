#include "Pch.h"

#include "Core/TextEncoding.h"
#include "Ui/Debugger/DebuggerStatusText.h"
#include "Ui/Debugger/DebuggerMenuCommand.h"
#include "Ui/Debugger/DebuggerWindow.h"
#include "resource.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::CreateStatusBar
//
//  The status bar along the bottom of the window: a replay note at the left,
//  which takes whatever width the fixed parts leave and so leaves room for
//  more, then where the beam is, which a press turns its mark on the screen
//  on and off, then the history section, whose bar under its text fills as
//  the history budget does, and the text zoom at the right. Pressing the zoom opens a slider over
//  it, from the smallest text size to the largest in the steps the keys take.
//  The slider has marks at the smallest size, 100% and the largest only. The
//  wheel over it steps the size, and a double-click on it or a click on its
//  percentage sets 100%.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::CreateStatusBar()
{
    std::vector<DxuiStatusBar::Field>  fields (kStatusZoom + 1);



    fields[kStatusReplay].stretch   = true;
    fields[kStatusBeam].widthDip    = kStatusBeamDip;
    fields[kStatusBeam].onClick     = [this] (const RECT &) { ToggleBeamMark(); };
    fields[kStatusHistory].widthDip = kStatusHistoryDip;
    fields[kStatusHistory].onClick  = [this] (const RECT &) { m_isHistoryPressed = true; };
    fields[kStatusSpeed].widthDip   = kStatusSpeedDip;
    fields[kStatusSpeed].onClick    = [this] (const RECT &) { m_isSpeedPressed = true; };
    fields[kStatusZoom].widthDip    = kStatusZoomDip;
    fields[kStatusZoom].text        = DebuggerStatusText::GetZoomText (m_textZoom);
    fields[kStatusZoom].onClick     = [this] (const RECT &) { OpenZoomPopup(); };

    m_statusBar = CreateChild<DxuiStatusBar>();
    m_statusBar->SetFields (std::move (fields));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::PlaceStatusBar
//
//  The band along the bottom edge, and the zoom panel, opened from the zoom
//  field, above it. Returns the band's top, which is where the panes above
//  it end.
//
////////////////////////////////////////////////////////////////////////////////

int DebuggerWindow::PlaceStatusBar (
    int  width,
    int  height)
{
    int                   bandPx  = m_scaler.ToPx (DxuiStatusBar::GetBandDp());
    int                   top     = (std::max) (0, height - bandPx);
    IDxuiTextRenderer   * text    = GetTextRenderer();



    if (m_statusBar == nullptr)
    {
        return height;
    }

    if (!m_isHistoryFitted && text != nullptr)
    {
        FitHistoryField (*text);
    }

    m_statusBar->Layout (RECT { 0, top, width, height }, m_scaler);

    m_windowZoom.PlaceAtButton (m_statusBar->GetFieldRect (kStatusZoom), RECT { 0, 0, width, height }, m_scaler,
                                (float) DxuiStatusBar::kFieldPadDip, DxuiStatusBar::kFontDip);
    return top;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::FitHistoryField
//
//  The history item at the width of the longest text it can show in the
//  user's locale, measured once the window has a renderer and before it
//  first draws, so the item never changes width as its text does. Without
//  a measurement it keeps its default width.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::FitHistoryField (IDxuiTextRenderer & text)
{
    int  widthDip = DxuiStatusBar::MeasureFieldWidthDip (text, DebuggerStatusText::GetHistoryFitTexts (LOCALE_NAME_USER_DEFAULT));



    m_isHistoryFitted = true;

    if (m_statusBar == nullptr || widthDip <= 0)
    {
        return;
    }

    m_statusBar->SetWidth (kStatusHistory, widthDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::UpdateStatusBar
//
//  From the newest snapshot's history, which the CPU thread built, and the
//  host's replay progress, which it reports while a replay runs, since no
//  snapshot is built until the replay ends.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::UpdateStatusBar()
{
    using Beam = std::optional<DebuggerViewSnapshot::BeamState>;

    HistoryStatus  status      = (m_snapshot != nullptr) ? m_snapshot->history  : HistoryStatus();
    Beam           beam        = (m_snapshot != nullptr) ? m_snapshot->beam     : std::nullopt;
    bool           isPaused    = (m_snapshot != nullptr) && m_snapshot->isPaused;
    ReplayProgress progress    = (m_host != nullptr) ? m_host->GetReplayProgress() : ReplayProgress();
    float          fill        = DebuggerStatusText::GetHistoryFill (status);



    SyncReplayDialog (progress);

    if (m_snapshot != nullptr && m_actionReplySerial != m_snapshot->actionReplySerial)
    {
        if (m_actionReplySerial.has_value() && !m_snapshot->actionReply.empty())
        {
            ShowActionReply (*m_snapshot);
        }

        m_actionReplySerial = m_snapshot->actionReplySerial;
    }

    if (m_statusBar == nullptr)
    {
        return;
    }

    m_statusBar->SetText  (kStatusReplay,  DebuggerStatusText::GetReplayText (progress, (m_snapshot != nullptr) ? m_snapshot->runNotice : std::string()));
    m_statusBar->SetText  (kStatusBeam,    DebuggerStatusText::GetBeamText (beam, isPaused));
    m_statusBar->SetText  (kStatusHistory, DebuggerStatusText::GetHistoryText (status));
    m_statusBar->SetBar   (kStatusHistory, fill, DebuggerStatusText::kEmptyArgb, DebuggerStatusText::GetBudgetColor (fill));
    m_statusBar->SetText  (kStatusSpeed,   DebuggerStatusText::GetSpeedText ((m_host != nullptr) ? m_host->GetEmulatorSpeed() : SpeedMode::Authentic));
    m_statusBar->SetText  (kStatusZoom,    DebuggerStatusText::GetZoomText (m_textZoom));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::SyncReplayDialog
//
//  A search that runs long opens the progress dialog, and the window is
//  disabled until the search ends and the dialog closes.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::SyncReplayDialog (const ReplayProgress & progress)
{
    if (m_replayDialog == nullptr && ReplayProgressDialog::IsDue (progress) && GetHwnd() != nullptr)
    {
        m_replayDialog = ReplayProgressDialog::Open (GetHwnd(), m_theme,
                                                     [this] { return (m_host != nullptr) ? m_host->GetReplayProgress() : ReplayProgress(); },
                                                     [this] { (void) TryStopReplay(); });

        if (m_replayDialog != nullptr)
        {
            EnableWindow (GetHwnd(), FALSE);
        }
    }
    else if (m_replayDialog != nullptr && !progress.isReplaying)
    {
        EnableWindow (GetHwnd(), TRUE);
        m_replayDialog.reset();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::OpenSpeedMenu
//
//  The emulator's speeds, the one in force checked, each posting the command
//  the emulator's own speed menu posts.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::OpenSpeedMenu (const RECT & field)
{
    std::vector<DxuiPopupMenuItem>  menu;
    SpeedMode                       speed  = (m_host != nullptr) ? m_host->GetEmulatorSpeed() : SpeedMode::Authentic;
    const std::pair<SpeedMode, int> speeds[] =
    {
        { SpeedMode::Authentic, IDM_MACHINE_SPEED_1X  },
        { SpeedMode::Double,    IDM_MACHINE_SPEED_2X  },
        { SpeedMode::Maximum,   IDM_MACHINE_SPEED_MAX },
    };



    if (m_host == nullptr || GetMenuHost() == nullptr)
    {
        return;
    }

    for (const auto & [mode, id] : speeds)
    {
        m_menuCommands.push_back (DebuggerMenuCommand::Make (DebuggerStatusText::GetSpeedText (mode), mode == speed, [this, id] { m_host->RunEmulatorCommand (id); }));
        menu.push_back (DxuiPopupMenuItem::ForCommand (m_menuCommands.back()));
    }

    DxuiContextMenu::ShowUnder (*GetMenuHost(), field, std::move (menu));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ToggleBeamMark
//
//  The same switch as the View menu's beam item, which reads the host and
//  so shows the change.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ToggleBeamMark()
{
    if (m_host == nullptr)
    {
        return;
    }

    m_host->SetBeamOverlayOn (!m_host->IsBeamOverlayOn());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::OpenZoomPopup
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::OpenZoomPopup()
{
    m_windowZoom.SetOpen (true);
    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::CloseZoomPopup
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::CloseZoomPopup()
{
    m_windowZoom.SetOpen (false);
    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RouteStatusBarMouse
//
//  The zoom panel works as the heat map's does: it opens with the pointer
//  over the zoom field, and closes once the pointer moves kNearDip away from
//  the field and the panel, except during a drag of its thumb. A press or
//  drag on its track sets the size there, Reset sets 100%, and a press
//  anywhere else closes it; a press on the zoom field toggles it. The wheel
//  over the zoom field or the panel steps the size. Then the status bar takes the pointer
//  over its band.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::RouteStatusBarMouse (const DxuiMouseEvent & ev)
{
    using Hit = HeatMapZoomWidget::Hit;

    POINT  point = ev.positionDip;
    Hit    hit   = Hit::None;



    if (m_statusBar == nullptr)
    {
        return false;
    }

    hit = m_windowZoom.HitTest (point);

    if (m_windowZoom.IsDragging())
    {
        if (ev.kind == DxuiMouseEventKind::Move)
        {
            ApplyTextZoom (GetTextZoomAt (m_windowZoom.GetPositionAt (point)));
        }
        else if (ev.kind == DxuiMouseEventKind::Up)
        {
            m_windowZoom.SetDragging (false);
        }

        Invalidate();
        return true;
    }

    if (ev.kind == DxuiMouseEventKind::Move)
    {
        bool  wasOpen = m_windowZoom.IsOpen();

        m_windowZoom.OnHover (point);

        if (m_windowZoom.IsOpen() != wasOpen)
        {
            Invalidate();
        }
    }
    else if (ev.kind == DxuiMouseEventKind::Leave && m_windowZoom.IsOpen())
    {
        CloseZoomPopup();
    }

    if (ev.kind == DxuiMouseEventKind::Wheel && !ev.wheelHorizontal && ev.wheelDelta != 0.0f && hit != Hit::None)
    {
        StepTextZoom (ev.wheelDelta > 0.0f ? 1 : -1);
        Invalidate();
        return true;
    }

    if (ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Left && hit == Hit::Button)
    {
        m_windowZoom.Toggle();
        Invalidate();
        return true;
    }

    if (m_windowZoom.IsOpen() && hit != Hit::None && hit != Hit::Button)
    {
        if (ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Left && hit == Hit::Reset)
        {
            ResetTextZoom();
        }
        else if (ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Left && hit == Hit::Track)
        {
            m_windowZoom.SetDragging (true);
            ApplyTextZoom (GetTextZoomAt (m_windowZoom.GetPositionAt (point)));
        }

        Invalidate();
        return true;
    }

    if (m_windowZoom.IsOpen() && ev.kind == DxuiMouseEventKind::Down && hit != Hit::Button)
    {
        CloseZoomPopup();
    }

    if (ev.kind == DxuiMouseEventKind::Wheel && !ev.wheelHorizontal && ev.wheelDelta != 0.0f &&
        m_statusBar->FindFieldAt (point) == (int) kStatusZoom)
    {
        StepTextZoom (ev.wheelDelta > 0.0f ? 1 : -1);
        return true;
    }

    //  Options opens at its History page as a click on the history field
    //  ends, so the release does not land in the dialog.
    if (ev.kind == DxuiMouseEventKind::Up && m_isHistoryPressed)
    {
        m_isHistoryPressed = false;

        if (m_statusBar->FindFieldAt (point) == (int) kStatusHistory)
        {
            OpenOptions();
            return true;
        }
    }

    //  The speed menu opens as the click on its field ends, so the release
    //  does not land outside the menu and close it.
    if (ev.kind == DxuiMouseEventKind::Up && m_isSpeedPressed)
    {
        m_isSpeedPressed = false;

        if (m_statusBar->FindFieldAt (point) == (int) kStatusSpeed)
        {
            OpenSpeedMenu (m_statusBar->GetFieldRect (kStatusSpeed));
            return true;
        }
    }

    if (!DxuiDockSite::Contains (m_statusBar->GetBounds(), point))
    {
        return false;
    }

    m_statusBar->OnMouse (ev);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::PaintZoomPopup
//
//  The heat map's zoom panel, above the zoom field, with the window's size.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::PaintZoomPopup (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    UNREFERENCED_PARAMETER (painter);

    m_windowZoom.PaintPanel (text, theme, DebuggerStatusText::GetZoomText (m_textZoom), GetTextZoomPosition (m_textZoom));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetTextZoomPosition
//
//  The size's logarithm across the slider, from kMinTextZoom at the left to
//  kMaxTextZoom at the right, as the heat map's cell size is.
//
////////////////////////////////////////////////////////////////////////////////

float DebuggerWindow::GetTextZoomPosition (float zoom)
{
    double  span = std::log ((double) kMaxTextZoom / (double) kMinTextZoom);



    return (float) std::clamp (std::log ((double) zoom / (double) kMinTextZoom) / span, 0.0, 1.0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetTextZoomAt
//
//  The size at a slider position, to the nearest kTextZoomStep.
//
////////////////////////////////////////////////////////////////////////////////

float DebuggerWindow::GetTextZoomAt (float position)
{
    double  span = std::log ((double) kMaxTextZoom / (double) kMinTextZoom);
    double  zoom = (double) kMinTextZoom * std::exp (std::clamp ((double) position, 0.0, 1.0) * span);



    zoom = std::round (zoom / (double) kTextZoomStep) * (double) kTextZoomStep;
    return (float) std::clamp (zoom, (double) kMinTextZoom, (double) kMaxTextZoom);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::TryStopReplay
//
//  Asks the host to stop a replay that is running, which reaches the CPU
//  thread without waiting behind the replay in its command queue. False
//  when no replay is running, so the key that asked can do its usual work.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::TryStopReplay()
{
    bool  isReplaying = m_host != nullptr && m_host->IsReplayingHistory();



    if (isReplaying)
    {
        m_host->StopReplay();
    }

    return isReplaying;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ShowActionReply
//
//  Over the pane the action came from, in an infobar that times out; in a
//  message box for an action a dialog ran, or when that pane isn't on
//  screen. The box waits for the next frame, so it doesn't open in the
//  middle of applying a snapshot.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ShowActionReply (const DebuggerViewSnapshot & snapshot)
{
    IDebuggerPane  * pane    = snapshot.isActionReplyBoxed ? nullptr : FindPane (snapshot.actionReplyPane);
    IDxuiControl   * content = (pane != nullptr) ? pane->GetContent() : nullptr;
    std::wstring     text;



    for (const std::string & line : snapshot.actionReply)
    {
        text += (text.empty() ? L"" : L"  ") + TextEncoding::NarrowToWide (line);
    }

    if (content == nullptr || !content->IsVisible())
    {
        m_pendingReplyBox = text;
        return;
    }

    if (m_replyBanner == nullptr)
    {
        m_replyBanner = CreateChild<DxuiTimedInfoBanner>();
    }

    m_replyBannerPane = snapshot.actionReplyPane;
    m_replyBanner->SetSeverity (snapshot.isActionReplyError ? DxuiInfoBanner::Severity::Warning : DxuiInfoBanner::Severity::Info);
    m_replyBanner->Show        (text, (int64_t) GetTickCount64());
    m_replyBanner->SetVisible  (true);
    PlaceReplyBanner ((int64_t) GetTickCount64());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::PlaceReplyBanner
//
//  Along the top of its pane, as tall as its text, each frame, since the
//  pane can move; hidden once it times out or the pane goes. Then the
//  message box opens for any reply waiting for one.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::PlaceReplyBanner (int64_t nowMs)
{
    IDebuggerPane  * pane    = (m_replyBanner != nullptr && m_replyBanner->IsVisible()) ? FindPane (m_replyBannerPane) : nullptr;
    IDxuiControl   * content = (pane != nullptr) ? pane->GetContent() : nullptr;
    RECT             bounds  = {};
    float            height  = 0.0f;



    if (m_replyBanner != nullptr && m_replyBanner->IsVisible() && (content == nullptr || !content->IsVisible() || !m_replyBanner->IsShowing (nowMs)))
    {
        m_replyBanner->SetVisible (false);
        Invalidate();
    }
    else if (content != nullptr)
    {
        bounds = content->GetBounds();
        height = m_replyBanner->GetPreferredHeightPx ((float) (bounds.right - bounds.left), m_scaler);

        m_replyBanner->SetDpi (m_scaler.GetDpi());
        m_replyBanner->Layout (RECT { bounds.left, bounds.top, bounds.right, bounds.top + (LONG) std::ceil (height) }, m_scaler);
    }

    ShowPendingReplyBox();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ShowPendingReplyBox
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ShowPendingReplyBox()
{
    std::wstring  text;



    if (!m_pendingReplyBox.has_value() || m_isReplyBoxUp || GetHwnd() == nullptr)
    {
        return;
    }

    text = std::exchange (m_pendingReplyBox, std::nullopt).value();

    m_isReplyBoxUp = true;
    (void) ShowMessageBox (GetHwnd(), text, L"Debugger", MB_ICONINFORMATION | MB_OK);
    m_isReplyBoxUp = false;
}
