#include "Pch.h"

#include "Ui/DiskInspector/PlatterView.h"
#include "Ui/DiskInspector/InspectorText.h"
#include "Devices/Disk/Inspector/TrackAnalyzer.h"
#include "Ui/DiskInspector/FluxTiming.h"
#include "Ui/DiskInspector/PlatterCells.h"
#include "Ui/DiskInspector/StripGeometry.h"





static constexpr float  s_kIndexMarkDip     = 10.0f;
static constexpr float  s_kHubFraction      = 0.6f;
static constexpr float  s_kSpindleFraction  = 0.28f;
static constexpr float  s_kHoverOutlineDip  = 1.0f;
static constexpr float  s_kSelectOutlineDip = 2.0f;
static constexpr float  s_kAlignmentDotDip  = 2.5f;
static constexpr float  s_kValueMinDip      = 22.0f;
static constexpr float  s_kTickMinDip       = 3.0f;
static constexpr float  s_kTickShare        = 0.3f;
static constexpr double s_kTwoPi            = 6.283185307179586;
static constexpr double s_kPanStep          = 0.1;
static constexpr uint32_t s_kSelectionAlpha = 0x60000000u;





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
        PaintDisk    (painter, theme, view);
        PaintNibbles (painter, text, theme, view);
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
//  PlatterView::PaintNibbles
//
//  Zoomed in, along each ring in view whose nibbles are long enough: each
//  nibble's hex value, upright, with a divider at its start; and once cells
//  are far enough apart, a tick for each 1 cell (FR-026). Rings too short
//  for either are skipped before any nibble is looked at.
//
////////////////////////////////////////////////////////////////////////////////

void PlatterView::PaintNibbles (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const PlatterPlacement & view)
{
    int     first  = 0;
    int     last   = -1;
    int     qt     = 0;
    int     slot   = -1;
    double  start  = 0.0;
    double  end    = 1.0;



    if (m_context.analysis != nullptr)
    {
        PlatterGeometry::GetVisibleRings (view, m_boundsDip, first, last);
        (void) PlatterGeometry::GetVisibleTurns (view, m_boundsDip, start, end);
    }

    for (qt = first; qt <= last; qt++)
    {
        slot = m_context.analysis->entries[qt].slot;

        if (slot >= 0 && slot < static_cast<int> (m_context.analysis->tracks.size()) && m_context.analysis->tracks[slot] != nullptr &&
            (IsShowingValues (view, qt) || IsShowingTicks (view, qt)))
        {
            PaintRing (painter, text, theme, view, qt, *m_context.analysis->tracks[slot], GetRingTurns (slot, *m_context.analysis->tracks[slot]), start, end);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterView::PaintRing
//
//  The nibbles of one ring between two turns. A nibble's turns rise from the
//  first nibble's, so each is tried a turn either way of the range.
//
////////////////////////////////////////////////////////////////////////////////

void PlatterView::PaintRing (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const PlatterPlacement & view, int quarterTrack,
                             const TrackAnalysis & track, const RingTurns & turns, double start, double end)
{
    const FramedTrack &  framed     = track.framed;
    double               rOuter     = PlatterGeometry::GetRingOuter (quarterTrack) * view.outerRadiusPx;
    double               rInner     = PlatterGeometry::GetRingOuter (quarterTrack + 1) * view.outerRadiusPx;
    double               rMid       = (rOuter + rInner) / 2;
    double               ringW      = rOuter - rInner;
    bool                 showValues = IsShowingValues (view, quarterTrack);
    bool                 showTicks  = IsShowingTicks (view, quarterTrack);
    double               rText      = showTicks ? rMid + ringW * s_kTickShare / 2 : rMid;
    float                textPx     = m_scaler.ToPxf (kSmallDip);
    size_t               n          = framed.nibbles.size();
    size_t               i          = 0;
    size_t               c          = 0;
    uint32_t             cell       = 0;
    uint32_t             count      = 0;
    uint32_t             k          = 0;
    double               a          = 0;
    double               b          = 0;
    double               arc        = 0;
    double               turn       = 0;
    uint32_t             color      = 0;
    uint32_t             ink        = 0;



    for (int lap : { -1, 0, 1 })
    {
        i = static_cast<size_t> (std::max<ptrdiff_t> (std::upper_bound (turns.nibbles.begin(), turns.nibbles.end() - 1, start - lap) - turns.nibbles.begin() - 1, 0));

        for (; i < n && turns.nibbles[i] + lap <= end; i++)
        {
            a     = turns.nibbles[i] + lap;
            b     = turns.nibbles[i + 1] + lap;
            arc   = (b - a) * s_kTwoPi * rMid;
            color = m_context.palette.GetNibbleColor (track, static_cast<int> (i), m_context.isTimingMode, m_context.timingRange);
            ink   = DiskInspectorPalette::GetTextColorOn (color);

            if (b < start)
            {
                continue;
            }

            if (IsSelectedNibble (quarterTrack, static_cast<int> (i)))
            {
                ShadeArc (painter, view, a, b, rInner, rOuter, (theme.Accent() & 0x00FFFFFFu) | s_kSelectionAlpha);
            }

            if (showValues && arc >= m_scaler.ToPxf (s_kValueMinDip))
            {
                DrawRadial (painter, view, a, rInner, rOuter, 1.0f, theme.Background());
                DrawUpright (text, view, (a + b) / 2, rText, std::format (L"{:02X}", framed.nibbles[i].value), static_cast<float> (arc), textPx, ink);
            }

            if (showTicks)
            {
                cell  = framed.nibbles[i].startCell % framed.cellCount;
                count = (framed.nibbles[(i + 1) % n].startCell + framed.cellCount - cell) % framed.cellCount;

                for (c = 0; c < count; c++)
                {
                    k = (cell + static_cast<uint32_t> (c)) % framed.cellCount;

                    if (framed.cells[k] != 0)
                    {
                        turn  = turns.cells[k] - turns.cells[cell];
                        turn -= std::floor (turn);
                        turn += a + (turns.cells[k + 1] - turns.cells[k]) / 2;
                        DrawRadial (painter, view, turn, rInner + 1.0, rInner + ringW * s_kTickShare, 1.0f, ink);
                    }
                }
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterView::DrawRadial
//
//  A line across a ring at a turn, from one radius out to another.
//
////////////////////////////////////////////////////////////////////////////////

void PlatterView::DrawRadial (IDxuiPainter & painter, const PlatterPlacement & view, double turn, double r0, double r1, float thicknessPx, uint32_t argb)
{
    double  angle = (turn + view.rotation) * s_kTwoPi;
    double  s     = std::sin (angle);
    double  co    = std::cos (angle);



    painter.DrawLine (static_cast<float> (view.centerXPx + r0 * s), static_cast<float> (view.centerYPx - r0 * co),
                      static_cast<float> (view.centerXPx + r1 * s), static_cast<float> (view.centerYPx - r1 * co), thicknessPx, argb);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterView::ShadeArc
//
//  A stretch of a ring filled between two turns; zoomed in this far the
//  arc is near enough straight for a quadrilateral.
//
////////////////////////////////////////////////////////////////////////////////

void PlatterView::ShadeArc (IDxuiPainter & painter, const PlatterPlacement & view, double a, double b, double r0, double r1, uint32_t argb)
{
    double  angleA = (a + view.rotation) * s_kTwoPi;
    double  angleB = (b + view.rotation) * s_kTwoPi;
    float   cx     = view.centerXPx;
    float   cy     = view.centerYPx;



    painter.FillConvexQuad (static_cast<float> (cx + r0 * std::sin (angleA)), static_cast<float> (cy - r0 * std::cos (angleA)),
                            static_cast<float> (cx + r1 * std::sin (angleA)), static_cast<float> (cy - r1 * std::cos (angleA)),
                            static_cast<float> (cx + r1 * std::sin (angleB)), static_cast<float> (cy - r1 * std::cos (angleB)),
                            static_cast<float> (cx + r0 * std::sin (angleB)), static_cast<float> (cy - r0 * std::cos (angleB)), argb);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterView::IsSelectedNibble
//
////////////////////////////////////////////////////////////////////////////////

bool PlatterView::IsSelectedNibble (int quarterTrack, int nibble) const
{
    int  first = m_context.model->GetFirstNibble();



    return quarterTrack == m_context.model->GetQuarterTrack() && first >= 0 && nibble >= first && nibble < first + m_context.model->GetNibbleCount();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterView::DrawUpright
//
//  Text centered on a point of a ring, kept level rather than turned with
//  the ring, no wider than the nibble's arc. Text is not clipped as shapes
//  are, so a value that would cross the view's edge is left out.
//
////////////////////////////////////////////////////////////////////////////////

void PlatterView::DrawUpright (IDxuiTextRenderer & text, const PlatterPlacement & view, double turn, double radius, const std::wstring & value, float widthPx,
                               float textPx, uint32_t argb) const
{
    double  angle = (turn + view.rotation) * s_kTwoPi;
    float   x     = static_cast<float> (view.centerXPx + radius * std::sin (angle));
    float   y     = static_cast<float> (view.centerYPx - radius * std::cos (angle));



    if (x - textPx >= m_boundsDip.left && x + textPx <= m_boundsDip.right && y - textPx >= m_boundsDip.top && y + textPx <= m_boundsDip.bottom)
    {
        text.DrawString (value.c_str(), x - widthPx / 2, y - textPx, widthPx, 2 * textPx, argb, textPx, DxuiTheme::kMonoFace, DxuiTextHAlign::Center,
                         DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterView::IsShowingValues
//
//  Whether a ring's mean nibble is long enough for its value.
//
////////////////////////////////////////////////////////////////////////////////

bool PlatterView::IsShowingValues (const PlatterPlacement & view, int quarterTrack) const
{
    const TrackAnalysis *  track = GetRingTrack (quarterTrack);
    double                 rMid  = PlatterGeometry::GetRingMiddle (quarterTrack) * view.outerRadiusPx;
    double                 ringW = PlatterGeometry::GetRingWidth() * view.outerRadiusPx;



    return track != nullptr && !track->framed.nibbles.empty() && ringW >= m_scaler.ToPxf (kSmallDip) &&
           s_kTwoPi * rMid / track->framed.nibbles.size() >= m_scaler.ToPxf (s_kValueMinDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterView::IsShowingTicks
//
//  Whether a ring's mean cell is long enough that ticks for adjacent cells
//  do not touch.
//
////////////////////////////////////////////////////////////////////////////////

bool PlatterView::IsShowingTicks (const PlatterPlacement & view, int quarterTrack) const
{
    const TrackAnalysis *  track = GetRingTrack (quarterTrack);
    double                 rMid  = PlatterGeometry::GetRingMiddle (quarterTrack) * view.outerRadiusPx;



    return track != nullptr && track->framed.cellCount > 0 && IsShowingValues (view, quarterTrack) &&
           s_kTwoPi * rMid / track->framed.cellCount >= m_scaler.ToPxf (s_kTickMinDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterView::GetRingTrack
//
////////////////////////////////////////////////////////////////////////////////

const TrackAnalysis * PlatterView::GetRingTrack (int quarterTrack) const
{
    const TrackAnalysis *  track = nullptr;
    int                    slot  = -1;



    if (m_context.hasDisk && m_context.analysis != nullptr && quarterTrack >= 0 && quarterTrack < DiskImage::kQuarterTrackCount)
    {
        slot = m_context.analysis->entries[quarterTrack].slot;

        if (slot >= 0 && slot < static_cast<int> (m_context.analysis->tracks.size()))
        {
            track = m_context.analysis->tracks[slot].get();
        }
    }

    return track;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterView::GetRingTurns
//
//  Each record's nibble and cell turns, built the first time a ring of it is
//  drawn zoomed in and kept until the record is analyzed again.
//
////////////////////////////////////////////////////////////////////////////////

const PlatterView::RingTurns & PlatterView::GetRingTurns (int slot, const TrackAnalysis & track) const
{
    if (m_ringTurns.size() <= static_cast<size_t> (slot))
    {
        m_ringTurns.resize (static_cast<size_t> (slot) + 1);
    }

    if (m_ringTurns[slot].of != &track)
    {
        StripGeometry::BuildNibbleTurns (track, m_ringTurns[slot].nibbles);
        FluxTiming::BuildCellTurns      (track, m_ringTurns[slot].cells);
        m_ringTurns[slot].of = &track;
    }

    return m_ringTurns[slot];
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

            if (m_isPressed && !m_isPanning && (std::abs (p.x - m_pressAt.x) > GetSystemMetrics (SM_CXDRAG) / 2 || std::abs (p.y - m_pressAt.y) > GetSystemMetrics (SM_CYDRAG) / 2))
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
        outText     = InspectorText::FormatPlatterTooltip (*m_context.analysis, hit.quarterTrack, hit.turn, IsShowingValues (MakeView(), hit.quarterTrack));
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
    m_renderer.SetTiming  (m_context.isTimingMode, m_context.timingRange);

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
