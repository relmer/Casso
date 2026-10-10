#include "Pch.h"

#include "Ui/DiskInspector/PlatterView.h"
#include "Ui/DiskInspector/InspectorText.h"
#include "Devices/Disk/Inspector/TrackAnalyzer.h"





static constexpr int    s_kDragThresholdPx  = 4;
static constexpr float  s_kIndexMarkDip     = 10.0f;
static constexpr float  s_kHubFraction      = 0.6f;
static constexpr float  s_kSpindleFraction  = 0.28f;
static constexpr float  s_kHoverOutlineDip  = 1.0f;
static constexpr float  s_kSelectOutlineDip = 2.0f;
static constexpr float  s_kAlignmentDotDip = 2.5f;





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterView::Paint
//
//  The rings first, through the custom draw, then everything the painter
//  draws over them, clipped to the view.
//
////////////////////////////////////////////////////////////////////////////////

void PlatterView::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    PlatterPlacement  view      = MakeView();
    RECT              oldClip   = {};
    bool              hadClip   = painter.GetClipRect (oldClip);
    RECT              clip      = m_boundsDip;



    (void) text;

    if (!m_context.hasDisk || m_context.analysis == nullptr)
    {
        clip = {};
    }
    else if (hadClip)
    {
        IntersectRect (&clip, &clip, &oldClip);
    }

    if (!IsRectEmpty (&clip))
    {
        painter.SetClipRect (&clip);
        PaintDisk (painter, theme, view);
        painter.SetClipRect (hadClip ? &oldClip : nullptr);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterView::PaintDisk
//
////////////////////////////////////////////////////////////////////////////////

void PlatterView::PaintDisk (IDxuiPainter & painter, const IDxuiTheme & theme, const PlatterPlacement & view)
{
    float  hub  = view.outerRadiusPx * PlatterRenderer::kInnerFraction;
    float  a    = static_cast<float> (m_rotation * 6.283185307179586);
    float  mark = m_scaler.ToPxf (s_kIndexMarkDip);



    painter.DrawCustom (m_boundsDip, [this] (const DxuiCustomDrawArgs & args) { Draw (args); });

    //  The hub: a raised ring inside the tracks and the spindle hole.
    painter.FillCircle (view.centerXPx, view.centerYPx, hub * s_kHubFraction,     theme.BackgroundElevated());
    painter.FillCircle (view.centerXPx, view.centerYPx, hub * s_kSpindleFraction, theme.Background());

    //  The index mark just outside the rim, where each track starts.
    painter.DrawLine (view.centerXPx + (view.outerRadiusPx + 2.0f) * std::sin (a),        view.centerYPx - (view.outerRadiusPx + 2.0f) * std::cos (a),
                      view.centerXPx + (view.outerRadiusPx + 2.0f + mark) * std::sin (a), view.centerYPx - (view.outerRadiusPx + 2.0f + mark) * std::cos (a),
                      m_scaler.ToPxf (s_kSelectOutlineDip), theme.Foreground());

    //  The line at the head's limit (FR-004).
    if (view.headLimitRing < PlatterRenderer::kRingCount)
    {
        OutlineCircle (painter, view.centerXPx, view.centerYPx, static_cast<float> (PlatterGeometry::GetRingOuter (view.headLimitRing)) * view.outerRadiusPx,
                       m_scaler.ToPxf (s_kHoverOutlineDip), theme.ForegroundMuted());
    }

    if (m_hoverRing >= 0 && m_hoverRing != m_context.model->GetQuarterTrack())
    {
        OutlineRing (painter, view, m_hoverRing, m_scaler.ToPxf (s_kHoverOutlineDip), theme.ForegroundMuted());
    }

    OutlineRing (painter, view, m_context.model->GetQuarterTrack(), m_scaler.ToPxf (s_kSelectOutlineDip), theme.Accent());

    if (m_isAlignmentShown)
    {
        PaintAlignment (painter, theme, view);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterView::PaintAlignment
//
//  The "Alignment" overlay (FR-031): on each whole track, a dot where sector
//  0's address field starts and another where the longest sync run starts,
//  so their angles line up, or not, from track to track.
//
////////////////////////////////////////////////////////////////////////////////

void PlatterView::PaintAlignment (IDxuiPainter & painter, const IDxuiTheme & theme, const PlatterPlacement & view)
{
    static constexpr double  kTwoPi = 6.283185307179586;



    float                  dot   = m_scaler.ToPxf (s_kAlignmentDotDip);
    int                    qt    = 0;
    const TrackAnalysis *  track = nullptr;
    double                 r     = 0;
    double                 turn  = 0;



    for (qt = 0; qt < PlatterRenderer::kRingCount; qt += DiskImage::kQuarterTracksPerWholeTrack)
    {
        int  slot = m_context.analysis->entries[qt].slot;

        track = (slot >= 0 && slot < static_cast<int> (m_context.analysis->tracks.size())) ? m_context.analysis->tracks[slot].get() : nullptr;

        if (track == nullptr)
        {
            continue;
        }

        r = PlatterGeometry::GetRingMiddle (qt) * view.outerRadiusPx;

        if (track->measurements.longestSync.count > 0)
        {
            turn = TrackAnalyzer::GetAngle (*track, track->measurements.longestSync.startCell) + view.rotation;
            painter.FillCircle (view.centerXPx + static_cast<float> (r * std::sin (kTwoPi * turn)), view.centerYPx - static_cast<float> (r * std::cos (kTwoPi * turn)),
                                dot, theme.ForegroundMuted());
        }

        if (track->measurements.sector0Angle >= 0)
        {
            turn = track->measurements.sector0Angle + view.rotation;
            painter.FillCircle (view.centerXPx + static_cast<float> (r * std::sin (kTwoPi * turn)), view.centerYPx - static_cast<float> (r * std::cos (kTwoPi * turn)),
                                dot, theme.Accent());
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterView::OnMouse
//
//  A press becomes a pan once it moves past the drag threshold, and a click
//  otherwise; the click selects what is under it, and a second click within
//  the double-click time returns to fit.
//
////////////////////////////////////////////////////////////////////////////////

bool PlatterView::OnMouse (const DxuiMouseEvent & ev)
{
    bool        isHandled = false;
    POINT       p         = ev.positionDip;
    int64_t     now       = static_cast<int64_t> (GetTickCount64());
    double      half      = std::max (1.0f, std::min (GetWidth(), GetHeight()) / 2.0f);
    PlatterHit  hit;
    int         sector    = -1;
    bool        isInside  = PtInRect (&m_boundsDip, p) != FALSE;



    //  With no disk the view takes nothing, as if the pointer had left it.
    switch (m_context.hasDisk ? ev.kind : DxuiMouseEventKind::Leave)
    {
        case DxuiMouseEventKind::Wheel:
            if (!ev.wheelHorizontal && isInside)
            {
                ZoomBy (std::pow (kWheelZoomStep, ev.wheelDelta), p);
                isHandled = true;
            }

            break;

        case DxuiMouseEventKind::Down:
            if (ev.button == DxuiMouseButton::Left && isInside)
            {
                m_isPressed = true;
                m_isPanning = false;
                m_pressAt   = p;
                m_lastAt    = p;
                isHandled   = true;
            }

            break;

        case DxuiMouseEventKind::Move:
            hit         = HitTest (p);
            m_hoverRing = (isInside && hit.isOnDisk) ? hit.quarterTrack : -1;

            if (m_isPressed && !m_isPanning && std::abs (p.x - m_pressAt.x) + std::abs (p.y - m_pressAt.y) >= s_kDragThresholdPx)
            {
                m_isPanning = true;
            }

            if (m_isPanning)
            {
                m_context.model->PanBy ({ (p.x - m_lastAt.x) / half, (p.y - m_lastAt.y) / half });
                m_lastAt = p;
            }

            isHandled = isInside || m_isPressed;
            break;

        case DxuiMouseEventKind::Up:
            isHandled = m_isPressed;

            if (m_isPressed && !m_isPanning)
            {
                if (now - m_lastClickMs <= static_cast<int64_t> (GetDoubleClickTime()))
                {
                    m_context.model->Fit();
                    m_lastClickMs = 0;
                }
                else
                {
                    m_lastClickMs = now;
                    hit           = HitTest (p);

                    if (hit.isOnDisk)
                    {
                        const TrackAnalysis *  track = nullptr;

                        m_context.model->SelectQuarterTrack (hit.quarterTrack);
                        track = m_context.model->GetTrack();

                        if (track != nullptr)
                        {
                            sector = PlatterGeometry::GetSectorAt (*track, PlatterGeometry::GetCellAtTurn (*track, hit.turn));
                        }

                        if (sector >= 0)
                        {
                            m_context.model->SelectSector (hit.quarterTrack, sector);
                        }

                        NotifySelection();
                    }
                }
            }

            m_isPressed = false;
            m_isPanning = false;
            break;

        case DxuiMouseEventKind::Leave:
            m_hoverRing = -1;
            break;

        default:
            break;
    }

    return isHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterView::GetTooltip
//
//  None during a drag (FR-027).
//
////////////////////////////////////////////////////////////////////////////////

bool PlatterView::GetTooltip (POINT pointPx, std::wstring & outText, RECT & outAnchorPx) const
{
    PlatterHit  hit     = HitTest (pointPx);
    bool        hasTip  = m_context.hasDisk && !m_isPanning && hit.isOnDisk && m_context.analysis != nullptr;



    if (hasTip)
    {
        outText     = InspectorText::FormatPlatterTooltip (*m_context.analysis, hit.quarterTrack, hit.turn, false);
        outAnchorPx = { pointPx.x, pointPx.y, pointPx.x + 1, pointPx.y + 1 };
    }

    return hasTip;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterView::HitTest
//
////////////////////////////////////////////////////////////////////////////////

PlatterHit PlatterView::HitTest (POINT pointPx) const
{
    return PlatterGeometry::HitTest (MakeView(), pointPx);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterView::ZoomBy
//
////////////////////////////////////////////////////////////////////////////////

void PlatterView::ZoomBy (double factor, POINT anchorPx)
{
    m_context.model->ZoomAbout (m_context.model->GetZoom() * factor, ToViewPoint (anchorPx));
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterView::ZoomAboutCenter
//
////////////////////////////////////////////////////////////////////////////////

void PlatterView::ZoomAboutCenter (double factor)
{
    ZoomBy (factor, { (m_boundsDip.left + m_boundsDip.right) / 2, (m_boundsDip.top + m_boundsDip.bottom) / 2 });
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterView::Draw
//
//  The renderer is made on the device the window draws with, the first time
//  it draws, and takes the palette each time so a theme change shows at once.
//
////////////////////////////////////////////////////////////////////////////////

void PlatterView::Draw (const DxuiCustomDrawArgs & args)
{
    HRESULT  hr = S_OK;



    if (!m_isRendererReady)
    {
        hr = m_renderer.Initialize (args.device);
        CHR (hr);

        m_isRendererReady = true;
    }

    m_renderer.SetPalette (m_context.palette);

    hr = m_renderer.Render (args, MakeView());
    CHR (hr);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterView::OutlineRing
//
////////////////////////////////////////////////////////////////////////////////

void PlatterView::OutlineRing (IDxuiPainter & painter, const PlatterPlacement & view, int quarterTrack, float thicknessPx, uint32_t argb) const
{
    float  outer = static_cast<float> (PlatterGeometry::GetRingOuter (quarterTrack)) * view.outerRadiusPx;
    float  inner = static_cast<float> (PlatterGeometry::GetRingOuter (quarterTrack + 1)) * view.outerRadiusPx;



    OutlineCircle (painter, view.centerXPx, view.centerYPx, outer + thicknessPx / 2, thicknessPx, argb);
    OutlineCircle (painter, view.centerXPx, view.centerYPx, inner - thicknessPx / 2, thicknessPx, argb);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterView::OutlineCircle
//
//  A rounded rectangle whose corner radius is half its side is a circle.
//
////////////////////////////////////////////////////////////////////////////////

void PlatterView::OutlineCircle (IDxuiPainter & painter, float cx, float cy, float radius, float thicknessPx, uint32_t argb)
{
    if (radius > 0)
    {
        painter.OutlineRoundedRect (cx - radius, cy - radius, 2 * radius, 2 * radius, radius, thicknessPx, argb);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterView::MakeView
//
////////////////////////////////////////////////////////////////////////////////

PlatterPlacement PlatterView::MakeView() const
{
    return m_context.model->GetPlacement (m_boundsDip, m_rotation);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterView::ToViewPoint
//
//  A pixel as the view model's fit units: the view's center is (0, 0) and
//  its edges are at -1 and 1.
//
////////////////////////////////////////////////////////////////////////////////

InspectorViewModel::Point PlatterView::ToViewPoint (POINT pointPx) const
{
    double                     half  = std::max (1.0f, std::min (GetWidth(), GetHeight()) / 2.0f);
    InspectorViewModel::Point  point;



    point.x = (pointPx.x - (m_boundsDip.left + m_boundsDip.right) / 2.0) / half;
    point.y = (pointPx.y - (m_boundsDip.top + m_boundsDip.bottom) / 2.0) / half;

    return point;
}
