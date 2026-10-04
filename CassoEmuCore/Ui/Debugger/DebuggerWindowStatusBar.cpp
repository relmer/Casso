#include "Pch.h"

#include "Ui/Debugger/DebuggerStatusText.h"
#include "Ui/Debugger/DebuggerWindow.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::CreateStatusBar
//
//  The status bar along the bottom of the window: a replay note at the left,
//  which takes whatever width the fixed parts leave and so leaves room for
//  more, then where the beam is, which a press turns its mark on the screen
//  on and off, then where history begins, how full its budget is, and the text
//  zoom at the right. Pressing the zoom opens a slider over it, from the
//  smallest text size to the largest in the steps the keys take.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::CreateStatusBar()
{
    std::vector<DxuiStatusBar::Field>  fields (kStatusZoom + 1);



    fields[kStatusReplay].stretch       = true;
    fields[kStatusBeam].widthDip        = kStatusBeamDip;
    fields[kStatusBeam].onClick         = [this] (const RECT &) { ToggleBeamMark(); };
    fields[kStatusBegin].widthDip       = kStatusBeginDip;
    fields[kStatusBudget].widthDip      = kStatusBudgetDip;
    fields[kStatusBudget].meterWidthDip = kStatusMeterDip;
    fields[kStatusZoom].widthDip        = kStatusZoomDip;
    fields[kStatusZoom].text            = DebuggerStatusText::GetZoomText (m_textZoom);
    fields[kStatusZoom].onClick         = [this] (const RECT &) { OpenZoomPopup(); };

    m_statusBar = CreateChild<DxuiStatusBar>();
    m_statusBar->SetFields (std::move (fields));

    m_zoomSlider.SetRange         (kMinTextZoom * DebuggerStatusText::kPercent, kMaxTextZoom * DebuggerStatusText::kPercent);
    m_zoomSlider.SetStep          (kTextZoomStep * DebuggerStatusText::kPercent);
    m_zoomSlider.SetSuffix        (L"%");
    m_zoomSlider.SetDecimalPlaces (0);
    m_zoomSlider.SetValue         (m_textZoom * DebuggerStatusText::kPercent);
    m_zoomSlider.SetOnChange      ([this] (float percent) { ApplyTextZoom (percent / DebuggerStatusText::kPercent); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::PlaceStatusBar
//
//  The band along the bottom edge, and the zoom popup, while open, over the
//  zoom field's right end. Returns the band's top, which is where the panes
//  above it end.
//
////////////////////////////////////////////////////////////////////////////////

int DebuggerWindow::PlaceStatusBar (
    int  width,
    int  height)
{
    int   bandPx  = m_scaler.ToPx (DxuiStatusBar::GetBandDp());
    int   top     = (std::max) (0, height - bandPx);
    int   popupW  = m_scaler.ToPx (kZoomPopupWidthDip);
    int   popupH  = m_scaler.ToPx (kZoomPopupHeightDip);
    int   pad     = m_scaler.ToPx (DxuiStatusBar::kFieldPadDip);
    RECT  field   = {};



    if (m_statusBar == nullptr)
    {
        return height;
    }

    m_statusBar->Layout (RECT { 0, top, width, height }, m_scaler);

    field      = m_statusBar->GetFieldRect (kStatusZoom);
    m_zoomRect = RECT { (std::max) (0L, field.right - popupW), field.top - popupH, field.right, field.top };

    m_zoomSlider.Layout (RECT { m_zoomRect.left + pad, m_zoomRect.top, m_zoomRect.right - pad, m_zoomRect.bottom }, m_scaler);
    m_zoomSlider.SetDpi (m_scaler.GetDpi());
    return top;
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

    HistoryStatus  status      = (m_snapshot != nullptr) ? m_snapshot->history : HistoryStatus();
    Beam           beam        = (m_snapshot != nullptr) ? m_snapshot->beam    : std::nullopt;
    ReplayProgress progress    = (m_host != nullptr) ? m_host->GetReplayProgress() : ReplayProgress();
    float          fill        = DebuggerStatusText::GetBudgetFill (status);



    if (m_statusBar == nullptr)
    {
        return;
    }

    m_statusBar->SetText  (kStatusReplay, DebuggerStatusText::GetReplayText (progress));
    m_statusBar->SetText  (kStatusBeam,   DebuggerStatusText::GetBeamText (beam));
    m_statusBar->SetText  (kStatusBegin,  DebuggerStatusText::GetBeginText (status));
    m_statusBar->SetText  (kStatusBudget, DebuggerStatusText::GetBudgetText (status));
    m_statusBar->SetMeter (kStatusBudget, fill, DebuggerStatusText::GetBudgetColor (fill));
    m_statusBar->SetText  (kStatusZoom,   DebuggerStatusText::GetZoomText (m_textZoom));
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
    m_zoomSlider.SetValue (m_textZoom * DebuggerStatusText::kPercent);
    m_zoomOpen = true;
    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::CloseZoomPopup
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::CloseZoomPopup()
{
    m_zoomOpen = false;
    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RouteStatusBarMouse
//
//  While the zoom popup is open, the slider takes the pointer over the popup
//  and for the whole of a drag, and a press anywhere else closes it; a press
//  on the zoom field that opened it only closes it, so the field toggles.
//  Then the status bar takes the pointer over its band.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::RouteStatusBarMouse (const DxuiMouseEvent & ev)
{
    POINT  point = ev.positionDip;



    if (m_statusBar == nullptr)
    {
        return false;
    }

    if (m_zoomOpen)
    {
        if (DxuiDockSite::Contains (m_zoomRect, point) || m_zoomSlider.IsDragging())
        {
            m_zoomSlider.OnMouse (ev);
            Invalidate();
            return true;
        }

        if (ev.kind == DxuiMouseEventKind::Down)
        {
            CloseZoomPopup();

            if (m_statusBar->FindFieldAt (point) == (int) kStatusZoom)
            {
                return true;
            }
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
//  A raised card with an edge, and the slider on it.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::PaintZoomPopup (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    float  lineW = (float) (std::max) (1L, std::lround (m_scaler.ToPxf (1.0f)));
    float  left  = (float) m_zoomRect.left;
    float  top   = (float) m_zoomRect.top;
    float  w     = (float) (m_zoomRect.right - m_zoomRect.left);
    float  h     = (float) (m_zoomRect.bottom - m_zoomRect.top);



    if (!m_zoomOpen)
    {
        return;
    }

    painter.FillRect    (left, top, w, h, theme.BackgroundElevated());
    painter.OutlineRect (left, top, w, h, lineW, theme.Border());
    m_zoomSlider.Paint  (painter, text, theme);
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
