#include "Pch.h"

#include "Ui/DiskInspector/PlatterView.h"
#include "Ui/DiskInspector/PlatterOverlays.h"
#include "Ui/DiskInspector/InspectorText.h"
#include "Ui/DiskInspector/InspectorMarquee.h"
#include "Ui/DiskInspector/InspectorTip.h"
#include "Devices/Disk/Inspector/TrackAnalyzer.h"
#include "Ui/DiskInspector/FluxTiming.h"
#include "Ui/DiskInspector/PlatterCells.h"
#include "Ui/DiskInspector/StripGeometry.h"





static constexpr float  s_kIndexMarkDip     = 10.0f;
static constexpr float  s_kHubFraction      = 0.6f;
static constexpr float  s_kSpindleFraction  = 0.28f;
static constexpr float  s_kHubLightDip     = 2.0f;
static constexpr float  s_kHubLightShare   = 0.35f;
static constexpr float  s_kHoverOutlineDip  = 1.0f;
static constexpr float  s_kSelectOutlineDip = 2.0f;
static constexpr float  s_kValueMinDip      = 22.0f;
static constexpr float  s_kTickMinDip       = 3.0f;
static constexpr float  s_kTickShare        = 0.3f;
static constexpr double s_kTwoPi            = 6.283185307179586;
static constexpr uint32_t s_kSelectionAlpha = 0x60000000u;
static constexpr float    s_kHeadReachDip    = 14.0f;
static constexpr float    s_kHeadEyeDip      = 14.0f;
static constexpr float    s_kHeadFarShare    = 1.5f;
static constexpr float    s_kHeadMarginDip   = 9.0f;
static constexpr uint32_t s_kHeadBodyAlpha   = 0x90000000u;
static constexpr float    s_kHeadSightAlpha  = 0.75f;
static constexpr uint32_t s_kHeadHaloAlpha   = 0xA0000000u;
static constexpr float    s_kHeadShadowDip   = 6.0f;
static constexpr uint32_t s_kHeadShadowArgb  = 0x40000000u;





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterView::PlatterView
//
////////////////////////////////////////////////////////////////////////////////

PlatterView::PlatterView (InspectorViewContext & context) :
    InspectorView (context),
    m_overlays    (std::make_unique<PlatterOverlays> (*this))
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterView::~PlatterView
//
////////////////////////////////////////////////////////////////////////////////

PlatterView::~PlatterView() = default;





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
        PaintHead    (painter, theme, view);
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
    float                             hub  = view.outerRadiusPx * PlatterRenderer::kInnerFraction;
    float                             mark = m_scaler.ToPxf (s_kIndexMarkDip);
    float                             x0   = 0.0f;
    float                             y0   = 0.0f;
    float                             x1   = 0.0f;
    float                             y1   = 0.0f;
    vector<PlatterOverlays::FileArc>  arcs;



    painter.DrawCustom (m_boundsDip, [this] (const DxuiCustomDrawArgs & args) { Draw (args); });

    if (m_context.isFileHighlighted)
    {
        m_overlays->GetFileArcs    (arcs);
        m_overlays->DimOutsideFile (painter, view, arcs);
    }

    //  The hub: a raised ring inside the tracks and the spindle hole.
    FillDiskCircle (painter, view, hub * s_kHubFraction,     theme.BackgroundElevated());
    FillDiskCircle (painter, view, hub * s_kSpindleFraction, theme.Background());

    //  A lit edge around the spindle hole and around the raised ring (FR-149).
    StrokeDiskCircle (painter, view, hub * s_kSpindleFraction + m_scaler.ToPxf (s_kHubLightDip) / 2.0f, m_scaler.ToPxf (s_kHubLightDip),
                   DxuiColor::Lerp (theme.BackgroundElevated(), 0xFFFFFFFF, s_kHubLightShare));
    StrokeDiskCircle (painter, view, hub * s_kHubFraction - m_scaler.ToPxf (s_kHubLightDip) / 2.0f, m_scaler.ToPxf (s_kHubLightDip) / 2.0f,
                   DxuiColor::Lerp (theme.BackgroundElevated(), 0xFFFFFFFF, s_kHubLightShare / 2.0f));

    //  The index mark just outside the rim, where each track starts.
    PlatterGeometry::ProjectAt (view, 0.0, view.outerRadiusPx + 2.0f,        x0, y0);
    PlatterGeometry::ProjectAt (view, 0.0, view.outerRadiusPx + 2.0f + mark, x1, y1);
    painter.DrawLine (x0, y0, x1, y1, m_scaler.ToPxf (s_kSelectOutlineDip), theme.Foreground());

    //  The line at the head's limit (FR-004).
    if (view.headLimitRing < PlatterRenderer::kRingCount)
    {
        StrokeDiskCircle (painter, view, static_cast<float> (PlatterGeometry::GetRingOuter (view.headLimitRing)) * view.outerRadiusPx,
                       m_scaler.ToPxf (s_kHoverOutlineDip), theme.ForegroundMuted());
    }

    if (m_context.isFilesOverlay && m_context.fileMap != nullptr && m_context.fileMap->notMapped == NotMappedReason::None)
    {
        m_overlays->PaintFiles (painter, theme, view);
    }

    //  Over the Files overlay, which colors the data fields the reads mark.
    if ((m_context.isReadsOverlay || m_context.isWritesOverlay || m_context.isScansOverlay) && m_context.drive != nullptr)
    {
        m_overlays->DimForAccess (painter, view);
    }

    if (m_context.isReadsOverlay && m_context.drive != nullptr)
    {
        m_overlays->PaintAccess (painter, view, AccessKind::Reads);
    }

    if (m_context.isWritesOverlay && m_context.drive != nullptr)
    {
        m_overlays->PaintAccess (painter, view, AccessKind::Writes);
    }

    if (m_context.isScansOverlay && m_context.drive != nullptr)
    {
        m_overlays->PaintAccess (painter, view, AccessKind::Scans);
    }

    //  The ring under the pointer, unless the pointer is on the selection,
    //  which the ring's outline would obscure.
    if (m_hoverRing >= 0 && m_hoverRing != m_context.model->GetQuarterTrack() && !m_overlays->IsOverSelection (view, m_context.hoverPx))
    {
        OutlineRing (painter, view, m_hoverRing, m_scaler.ToPxf (s_kSelectOutlineDip), theme.Foreground());
    }

    OutlineRing (painter, view, m_context.model->GetQuarterTrack(), m_scaler.ToPxf (s_kSelectOutlineDip), theme.Accent());

    if (m_isAlignmentShown)
    {
        m_overlays->PaintAlignment (painter, theme, view);
    }

    if (m_context.isDiffsOverlay && m_context.comparison != nullptr)
    {
        m_overlays->PaintDiffs (painter, view);
    }

    if (m_context.findMarks != nullptr)
    {
        m_overlays->PaintHits (painter, view);
    }

    if (m_context.isFileHighlighted)
    {
        m_overlays->PaintMarquee (painter, view, arcs);
    }

    m_overlays->PaintSelectedArc (painter, theme, view);
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
    double               local      = 1.0;
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

            //  Tilted, a value is left out where the ring is drawn too thin for
            //  it (FR-151).
            local = PlatterGeometry::GetLocalScale (view, (a + b) / 2, rMid);

            if (showValues && arc * local >= m_scaler.ToPxf (s_kValueMinDip) && ringW * local >= m_scaler.ToPxf (kSmallDip))
            {
                DrawRadial (painter, view, a, rInner, rOuter, 1.0f, theme.Background());
                DrawUpright (text, view, (a + b) / 2, rText, std::format (L"{:02X}", framed.nibbles[i].value), static_cast<float> (arc * local), textPx, ink);
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
    float  x0 = 0.0f;
    float  y0 = 0.0f;
    float  x1 = 0.0f;
    float  y1 = 0.0f;



    PlatterGeometry::ProjectAt (view, turn, r0, x0, y0);
    PlatterGeometry::ProjectAt (view, turn, r1, x1, y1);
    painter.DrawLine (x0, y0, x1, y1, thicknessPx, argb);
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
    std::array<float, 8>  q = {};



    PlatterGeometry::ProjectAt (view, a, r0, q[0], q[1]);
    PlatterGeometry::ProjectAt (view, a, r1, q[2], q[3]);
    PlatterGeometry::ProjectAt (view, b, r1, q[4], q[5]);
    PlatterGeometry::ProjectAt (view, b, r0, q[6], q[7]);
    painter.FillConvexQuad (q[0], q[1], q[2], q[3], q[4], q[5], q[6], q[7], argb);
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
    float  x = 0.0f;
    float  y = 0.0f;



    PlatterGeometry::ProjectAt (view, turn, radius, x, y);

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
    double                 most  = PlatterGeometry::GetScaleAt (view, view.outerRadiusPx);
    double                 ringW = PlatterGeometry::GetRingWidth() * view.outerRadiusPx * most;



    //  Tilted, the near side is drawn larger, so values can fit there.
    return track != nullptr && !track->framed.nibbles.empty() && ringW >= m_scaler.ToPxf (kSmallDip) &&
           s_kTwoPi * rMid * most / track->framed.nibbles.size() >= m_scaler.ToPxf (s_kValueMinDip);
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



    //  The tilt handle and a right drag tilt and turn the platter (FR-151).
    if (m_context.hasDisk && OnTiltMouse (ev))
    {
        return true;
    }

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
                    else
                    {
                        m_context.model->ClearSelection();
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
    InspectorTip  tip;
    bool          hasTip = false;



    hasTip = BuildTip (pointPx, tip);

    if (hasTip)
    {
        outText     = tip.GetText();
        outAnchorPx = { pointPx.x, pointPx.y, pointPx.x + 1, pointPx.y + 1 };
    }

    return hasTip;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterView::GetTooltipSwatches
//
////////////////////////////////////////////////////////////////////////////////

bool PlatterView::GetTooltipSwatches (POINT pointPx, vector<DxuiTooltipSwatch> & outSwatches) const
{
    InspectorTip  tip;
    bool          hasTip = BuildTip (pointPx, tip);



    if (hasTip)
    {
        outSwatches = tip.GetSwatches();
    }

    return hasTip;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterView::BuildTip
//
//  The tip for the cells one pixel covers at the pointer: a pixel's width
//  along the ring there, as a fraction of a turn. None during a drag
//  (FR-027).
//
////////////////////////////////////////////////////////////////////////////////

bool PlatterView::BuildTip (POINT pointPx, InspectorTip & outTip) const
{
    PlatterPlacement  view   = MakeView();
    PlatterHit        hit    = HitTest (pointPx);
    bool              hasTip = m_context.hasDisk && !m_isPanning && hit.isOnDisk && m_context.analysis != nullptr;
    double            radius = std::hypot (pointPx.x + 0.5 - view.centerXPx, pointPx.y + 0.5 - view.centerYPx);



    if (hasTip)
    {
        outTip = InspectorTip::BuildPlatterTip (m_context, hit.quarterTrack, hit.turn, 1.0 / (s_kTwoPi * std::max (radius, 1.0)),
                                                IsShowingValues (view, hit.quarterTrack));
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



    StrokeDiskCircle (painter, view, outer + thicknessPx / 2, thicknessPx, argb);
    StrokeDiskCircle (painter, view, inner - thicknessPx / 2, thicknessPx, argb);
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
    return m_context.model->GetPlacement (m_boundsDip);
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





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterView::GetHeadPoint
//
////////////////////////////////////////////////////////////////////////////////

bool PlatterView::GetHeadPoint (POINT & outPointPx) const
{
    PlatterPlacement  view    = MakeView();
    bool              isShown = m_context.head.isShown && m_context.head.quarterTrack >= 0 && m_context.head.quarterTrack < PlatterRenderer::kRingCount;
    double            radius  = isShown ? PlatterGeometry::GetRingMiddle (m_context.head.quarterTrack) * view.outerRadiusPx : 0.0;
    float             x       = 0.0f;
    float             y       = 0.0f;



    PlatterGeometry::ProjectAt (view, m_context.head.turn, radius, x, y);
    outPointPx.x = static_cast<LONG> (std::lround (x));
    outPointPx.y = static_cast<LONG> (std::lround (y));

    return isShown;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterView::PaintHead
//
//  The head marker, drawn as a hard drive's head arm (FR-064, FR-152): a
//  translucent arm with a rounded tip centered on the head, its sides
//  tangent to the tip and widening out past the rim. In the tip, a ring with
//  a margin of arm around it has a clear window over the platter under the
//  head, and a crosshair in the window has its center exactly on the head's
//  quarter track and angle, its lines along the radius and across it. The
//  ring and crosshair are a little translucent, and the arm is one tapered
//  capsule, so its translucent fill has no seams. Tilted,
//  the arm has a soft shadow under it. The arm's edge, the ring and the
//  crosshair indicate idle, reading or writing by color, and a blocked write
//  adds a cross. The marker is drawn over everything and is not hit-tested,
//  so the pointer acts on the platter under it.
//
////////////////////////////////////////////////////////////////////////////////

void PlatterView::PaintHead (IDxuiPainter & painter, const IDxuiTheme & theme, const PlatterPlacement & view) const
{
    const DiskInspectorColors &  colors = m_context.palette.colors;
    POINT                        eyeAt  = {};
    float                        outer  = view.outerRadiusPx + m_scaler.ToPxf (s_kHeadReachDip);
    float                        ring   = m_scaler.ToPxf (s_kHeadEyeDip);
    float                        line   = m_scaler.ToPxf (s_kSelectOutlineDip);
    float                        rimW   = line * 2.0f;
    float                        hole   = ring - rimW;
    float                        tip    = ring + m_scaler.ToPxf (s_kHeadMarginDip);
    float                        farW   = tip * s_kHeadFarShare;
    float                        drop   = m_scaler.ToPxf (s_kHeadShadowDip);
    float                        cross  = hole * 0.7f;
    float                        gap    = std::max (hole * 0.18f, line);
    float                        ex     = 0.0f;
    float                        ey     = 0.0f;
    float                        ox     = 0.0f;
    float                        oy     = 0.0f;
    float                        ux     = 0.0f;
    float                        uy     = 0.0f;
    float                        len    = 0.0f;
    float                        span   = 0.0f;
    uint32_t                     argb   = colors.headIdle;
    uint32_t                     body   = (theme.BackgroundElevated() & 0x00FFFFFFu) | s_kHeadBodyAlpha;
    uint32_t                     halo   = (theme.Background() & 0x00FFFFFFu) | s_kHeadHaloAlpha;
    float                        alpha  = painter.GetGlobalAlpha();



    if (!GetHeadPoint (eyeAt))
    {
        return;
    }

    //  The exact point, not the rounded pixel, so the crosshair's center is
    //  on the head's track at any zoom.
    PlatterGeometry::ProjectAt (view, m_context.head.turn, PlatterGeometry::GetRingMiddle (m_context.head.quarterTrack) * view.outerRadiusPx, ex, ey);
    argb = (m_context.head.activity == HeadActivity::Reading) ? colors.headReading
         : (m_context.head.activity == HeadActivity::Idle)    ? colors.headIdle
                                                              : colors.headWriting;

    //  Outward along the radius on screen; the arm ends past the rim and is
    //  always long enough to taper.
    PlatterGeometry::ProjectAt (view, m_context.head.turn, outer, ox, oy);
    span = std::max (std::hypot (ox - ex, oy - ey), 1.0f);
    len  = std::max (span, tip * 3.0f);
    ux   = (ox - ex) / span;
    uy   = (oy - ey) / span;
    ox   = ex + ux * len;
    oy   = ey + uy * len;

    if (view.tilt != 0.0f)
    {
        painter.FillTaperedCapsule (ex, ey + drop, tip, ox, oy + drop, farW, hole, 0.0f, s_kHeadShadowArgb);
    }

    painter.FillTaperedCapsule (ex, ey, tip, ox, oy, farW, hole, 0.0f, body);
    painter.FillTaperedCapsule (ex, ey, tip, ox, oy, farW, 0.0f, line / 2.0f, argb);

    //  The window's ring and crosshair are a little translucent too.
    painter.SetGlobalAlpha (alpha * s_kHeadSightAlpha);

    //  The window's ring: a dark halo so it reads over any ring color, then
    //  its rim.
    OutlineCircle (painter, ex, ey, ring + line, rimW + line * 2.0f, halo);
    OutlineCircle (painter, ex, ey, ring, rimW, argb);

    //  The crosshair, along the radius and across it, each line haloed and
    //  broken around the center, with a dot on the head's position.
    for (const std::array<float, 2> & d : { std::array<float, 2> { ux, uy }, std::array<float, 2> { -uy, ux } })
    {
        for (float sign : { 1.0f, -1.0f })
        {
            float  x0 = ex + sign * d[0] * gap;
            float  y0 = ey + sign * d[1] * gap;
            float  x1 = ex + sign * d[0] * hole;
            float  y1 = ey + sign * d[1] * hole;

            painter.DrawLine (x0, y0, x1, y1, line * 1.5f, halo);
            painter.DrawLine (x0, y0, x1, y1, line / 2.0f, argb);
        }
    }

    painter.FillCircle (ex, ey, line * 0.75f, argb);

    if (m_context.head.activity == HeadActivity::WriteBlocked)
    {
        painter.DrawLine (ex - cross, ey - cross, ex + cross, ey + cross, line, argb);
        painter.DrawLine (ex - cross, ey + cross, ex + cross, ey - cross, line, argb);
    }

    painter.SetGlobalAlpha (alpha);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterView::StrokeDiskCircle
//
//  Tilted, the circle is a closed line through its projected points, about
//  six pixels apart.
//
////////////////////////////////////////////////////////////////////////////////

void PlatterView::StrokeDiskCircle (IDxuiPainter & painter, const PlatterPlacement & view, float radiusPx, float thicknessPx, uint32_t argb)
{
    int    count = std::clamp (static_cast<int> (s_kTwoPi * radiusPx / 6.0), 48, 720);
    float  x0    = 0.0f;
    float  y0    = 0.0f;
    float  x1    = 0.0f;
    float  y1    = 0.0f;
    int    k     = 0;



    if (view.tilt == 0.0f)
    {
        OutlineCircle (painter, view.centerXPx, view.centerYPx, radiusPx, thicknessPx, argb);
    }
    else if (radiusPx > 0.0f)
    {
        PlatterGeometry::Project (view, 0.0, -radiusPx, x0, y0);

        for (k = 1; k <= count; k++)
        {
            PlatterGeometry::Project (view, radiusPx * std::sin (s_kTwoPi * k / count), -radiusPx * std::cos (s_kTwoPi * k / count), x1, y1);
            painter.DrawLine (x0, y0, x1, y1, thicknessPx, argb);
            x0 = x1;
            y0 = y1;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterView::FillDiskCircle
//
//  Tilted, the disk is a fan of triangles from its center. The projection
//  maps the center to a point inside the projected disk.
//
////////////////////////////////////////////////////////////////////////////////

void PlatterView::FillDiskCircle (IDxuiPainter & painter, const PlatterPlacement & view, float radiusPx, uint32_t argb)
{
    int    count = std::clamp (static_cast<int> (s_kTwoPi * radiusPx / 6.0), 48, 720);
    float  x0    = 0.0f;
    float  y0    = 0.0f;
    float  x1    = 0.0f;
    float  y1    = 0.0f;
    int    k     = 0;



    if (view.tilt == 0.0f)
    {
        painter.FillCircle (view.centerXPx, view.centerYPx, radiusPx, argb);
    }
    else if (radiusPx > 0.0f)
    {
        PlatterGeometry::Project (view, 0.0, -radiusPx, x0, y0);

        for (k = 1; k <= count; k++)
        {
            PlatterGeometry::Project (view, radiusPx * std::sin (s_kTwoPi * k / count), -radiusPx * std::cos (s_kTwoPi * k / count), x1, y1);
            painter.FillConvexQuad (view.centerXPx, view.centerYPx, x0, y0, x1, y1, x1, y1, argb);
            x0 = x1;
            y0 = y1;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterView::OnTiltMouse
//
//  A right drag tilts the platter by its height and turns it by its width
//  (FR-151).
//
////////////////////////////////////////////////////////////////////////////////

bool PlatterView::OnTiltMouse (const DxuiMouseEvent & ev)
{
    POINT    p         = ev.positionDip;
    double   half      = std::max (1.0f, std::min (GetWidth(), GetHeight()) / 2.0f);
    double   radius    = std::max (MakeView().outerRadiusPx, 1.0f);
    bool     isHandled = false;



    switch (ev.kind)
    {
        case DxuiMouseEventKind::Down:
            if (ev.button == DxuiMouseButton::Right && PtInRect (&m_boundsDip, p))
            {
                m_isTilting = true;
                m_tiltAt    = p;
                isHandled   = true;
            }

            break;

        case DxuiMouseEventKind::Move:
            if (m_isTilting)
            {
                m_context.model->SetTilt     (m_context.model->GetTilt() + (p.y - m_tiltAt.y) / half);
                m_context.model->SetRotation (m_context.model->GetRotation() + (p.x - m_tiltAt.x) / (s_kTwoPi * radius));
                m_tiltAt  = p;
                isHandled = true;
            }

            break;

        case DxuiMouseEventKind::Up:
            isHandled   = m_isTilting && ev.button == DxuiMouseButton::Right;
            m_isTilting = m_isTilting && ev.button != DxuiMouseButton::Right;
            break;

        default:
            break;
    }

    return isHandled;
}
