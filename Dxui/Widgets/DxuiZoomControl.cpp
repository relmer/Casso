#include "Pch.h"

#include "Widgets/DxuiZoomControl.h"





//  The stops a step moves between, as percentages.
static constexpr int     s_kStops[]     = { 50, 60, 70, 80, 90, 100, 110, 120, 130, 140, 150, 160, 170, 180, 190, 200 };





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiZoomControl::DxuiZoomControl
//
//  The standard range, linear, stepped through the standard stops, with
//  Reset at 100%.
//
////////////////////////////////////////////////////////////////////////////////

DxuiZoomControl::DxuiZoomControl()
{
    m_popup.SetRange     (kMinZoom, kMaxZoom, false);
    m_popup.SetResetZoom (1.0);
    m_popup.SetOnStep    ([] (double zoom, int steps) { return StepZoom (zoom, steps); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiZoomControl::SetRange
//
////////////////////////////////////////////////////////////////////////////////

void DxuiZoomControl::SetRange (double minZoom, double maxZoom, bool isLogarithmic)
{
    m_popup.SetRange (minZoom, maxZoom, isLogarithmic);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiZoomControl::UpdateHover
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiZoomControl::UpdateHover (POINT cursorPx, int64_t nowMs)
{
    bool  wasOpen = m_popup.IsOpen();



    if (!m_popup.IsDragging())
    {
        m_popup.OnHover (cursorPx);
    }

    return Advance (nowMs, wasOpen);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiZoomControl::Follow
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiZoomControl::Follow (HWND owner, double zoom, int64_t nowMs)
{
    POINT  cursor   = {};
    bool   isCursor = false;
    bool   wasOpen  = m_popup.IsOpen();



    if (!m_popup.IsDragging())
    {
        m_popup.SetZoom (zoom);
    }

    isCursor = owner != nullptr && IsVisible() && GetCursorPos (&cursor) && ScreenToClient (owner, &cursor);

    if (isCursor && !m_popup.IsDragging())
    {
        m_popup.OnHover (cursor);
    }

    return Advance (nowMs, wasOpen);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiZoomControl::Advance
//
//  Moves the opening along, and fits the bounds to what is drawn. True while
//  the panel moves or when it opened or closed.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiZoomControl::Advance (int64_t nowMs, bool wasOpen)
{
    bool  isMoving = m_popup.Tick (nowMs);



    FitBounds();

    return isMoving || wasOpen != m_popup.IsOpen();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiZoomControl::FitBounds
//
//  The button at rest; open, the button and the panel, so a host that
//  routes presses by bounds sends those on the panel here.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiZoomControl::FitBounds()
{
    RECT  button = m_popup.GetButtonRect();
    RECT  panel  = m_popup.GetPanelRect();
    RECT  fitted = button;



    if (m_popup.IsOpen())
    {
        UnionRect (&fitted, &button, &panel);
    }

    SetBounds (fitted);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiZoomControl::PlaceInRow
//
//  The popup puts the button a margin in from its corner's bottom right, so
//  the corner extends a margin past the row's right end and the button's
//  bottom.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiZoomControl::PlaceInRow (const RECT & rowPx, const RECT & panelBoundsPx, const DxuiDpiScaler & scaler)
{
    long  margin = scaler.ToPx (DxuiZoomPopup::kMarginDip);
    long  button = scaler.ToPx (DxuiZoomPopup::kButtonHeightDip);
    RECT  corner = { rowPx.left, rowPx.top, rowPx.right + margin, (rowPx.top + rowPx.bottom + button) / 2 + margin };



    m_panelBounds = panelBoundsPx;

    m_popup.Place (corner, m_panelBounds, scaler);
    FitBounds();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiZoomControl::StepZoom
//
//  From between two stops, a step down lands on the lower one and a step up
//  on the higher one.
//
////////////////////////////////////////////////////////////////////////////////

double DxuiZoomControl::StepZoom (double zoom, int steps)
{
    static constexpr double  kNear = 0.5;



    double  percent = zoom * 100;
    int     index   = 0;
    int     count   = static_cast<int> (std::size (s_kStops));



    //  The stop at or just below the zoom, then that many stops on; a step
    //  down from a zoom between stops counts the lower stop as the first.
    while (index + 1 < count && s_kStops[index + 1] <= percent + kNear)
    {
        index++;
    }

    if (steps < 0 && percent > s_kStops[index] + kNear)
    {
        steps++;
    }

    index = std::clamp (index + steps, 0, count - 1);

    return s_kStops[index] / 100.0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiZoomControl::GetStandardSteps
//
////////////////////////////////////////////////////////////////////////////////

std::vector<double> DxuiZoomControl::GetStandardSteps()
{
    std::vector<double>  steps;



    for (int percent : s_kStops)
    {
        steps.push_back (percent / 100.0);
    }

    return steps;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiZoomControl::Layout
//
//  A containing panel's layout pass passes in the bounds FitBounds set,
//  and then the button doesn't move; other bounds are a new corner, the
//  button a margin in from its bottom right.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiZoomControl::Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler)
{
    if (EqualRect (&boundsPx, &m_boundsDip) && m_popup.GetButtonRect().right > m_popup.GetButtonRect().left)
    {
        return;
    }

    m_popup.Place (boundsPx, m_panelBounds, scaler);
    FitBounds();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiZoomControl::Paint
//
////////////////////////////////////////////////////////////////////////////////

void DxuiZoomControl::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    (void) painter;

    m_popup.Paint (text, theme);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiZoomControl::OnMouse
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiZoomControl::OnMouse (const DxuiMouseEvent & ev)
{
    bool  isConsumed = m_popup.OnMouse (ev);



    FitBounds();

    return isConsumed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiZoomControl::Tick
//
////////////////////////////////////////////////////////////////////////////////

void DxuiZoomControl::Tick (int64_t nowMs)
{
    bool  isMoving = m_popup.Tick (nowMs);



    (void) isMoving;
    FitBounds();
}
