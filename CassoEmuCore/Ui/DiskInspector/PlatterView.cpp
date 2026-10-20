#include "Pch.h"

#include "Ui/DiskInspector/PlatterView.h"
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
static constexpr float  s_kAlignmentDotDip  = 2.5f;
static constexpr float  s_kValueMinDip      = 22.0f;
static constexpr float  s_kTickMinDip       = 3.0f;
static constexpr float  s_kTickShare        = 0.3f;
static constexpr double s_kTwoPi            = 6.283185307179586;
static constexpr double s_kPanStep          = 0.1;
static constexpr uint32_t s_kSelectionAlpha = 0x60000000u;
static constexpr uint32_t s_kFilesAlpha     = 0xD0000000u;
static constexpr uint32_t s_kDimAlpha       = 0x80000000u;
static constexpr float    s_kMarqueeStepDip = 8.0f;
static constexpr float    s_kShadeStepDip   = 24.0f;
static constexpr int      s_kMaxPieces      = 4096;
static constexpr int      s_kMaxRings       = 256;
static constexpr int      s_kMaxQuads       = 32768;
static constexpr double   s_kShimmerSteps   = 16.0;
static constexpr uint32_t s_kSweepLineArgb  = 0xFF808080u;
static constexpr float    s_kMinMarkDip     = 8.0f;
static constexpr float    s_kShimmerStepDip = 3.0f;
static constexpr double   s_kPiecesPerTurn   = 96.0;
static constexpr double   s_kMinDiffTurn     = 0.004;
static constexpr float    s_kMinHitDip       = 3.0f;
static constexpr float    s_kHitEdgeDip      = 1.0f;
static constexpr float    s_kHitSolidDip     = 24.0f;
static constexpr uint32_t s_kHitAlpha        = 0x70000000u;





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
    float            hub  = view.outerRadiusPx * PlatterRenderer::kInnerFraction;
    float            a    = static_cast<float> (m_rotation * 6.283185307179586);
    float            mark = m_scaler.ToPxf (s_kIndexMarkDip);
    vector<FileArc>  arcs;



    painter.DrawCustom (m_boundsDip, [this] (const DxuiCustomDrawArgs & args) { Draw (args); });

    if (m_context.isFileHighlighted)
    {
        GetFileArcs    (arcs);
        DimOutsideFile (painter, view, arcs);
    }

    //  The hub: a raised ring inside the tracks and the spindle hole.
    painter.FillCircle (view.centerXPx, view.centerYPx, hub * s_kHubFraction,     theme.BackgroundElevated());
    painter.FillCircle (view.centerXPx, view.centerYPx, hub * s_kSpindleFraction, theme.Background());

    //  A lit edge around the spindle hole and around the raised ring (FR-149).
    OutlineCircle (painter, view.centerXPx, view.centerYPx, hub * s_kSpindleFraction + m_scaler.ToPxf (s_kHubLightDip) / 2.0f, m_scaler.ToPxf (s_kHubLightDip),
                   DxuiColor::Lerp (theme.BackgroundElevated(), 0xFFFFFFFF, s_kHubLightShare));
    OutlineCircle (painter, view.centerXPx, view.centerYPx, hub * s_kHubFraction - m_scaler.ToPxf (s_kHubLightDip) / 2.0f, m_scaler.ToPxf (s_kHubLightDip) / 2.0f,
                   DxuiColor::Lerp (theme.BackgroundElevated(), 0xFFFFFFFF, s_kHubLightShare / 2.0f));

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

    //  The ring under the pointer, unless the pointer is on the selection,
    //  which the ring's outline would obscure.
    if (m_hoverRing >= 0 && m_hoverRing != m_context.model->GetQuarterTrack() && !IsOverSelection (view, m_context.hoverPx))
    {
        OutlineRing (painter, view, m_hoverRing, m_scaler.ToPxf (s_kSelectOutlineDip), theme.Foreground());
    }

    OutlineRing (painter, view, m_context.model->GetQuarterTrack(), m_scaler.ToPxf (s_kSelectOutlineDip), theme.Accent());

    if (m_isAlignmentShown)
    {
        PaintAlignment (painter, theme, view);
    }

    if (m_context.isFilesOverlay && m_context.fileMap != nullptr && m_context.fileMap->notMapped == NotMappedReason::None)
    {
        PaintFiles (painter, theme, view);
    }

    if (m_context.isDiffsOverlay && m_context.comparison != nullptr)
    {
        PaintDiffs (painter, view);
    }

    if (m_context.findMarks != nullptr)
    {
        PaintHits (painter, view);
    }

    if (m_context.isFileHighlighted)
    {
        PaintMarquee (painter, view, arcs);
    }

    PaintSelectedArc (painter, theme, view);
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
//  PlatterView::PaintFiles
//
//  The "Files" overlay (FR-091): each standard sector's data field across
//  its whole track in its role's color, and the selected file's sectors
//  marked with a band along the track's inner edge (FR-090).
//
////////////////////////////////////////////////////////////////////////////////

void PlatterView::PaintFiles (IDxuiPainter & painter, const IDxuiTheme & theme, const PlatterPlacement & view)
{
    const FileMap  & map    = *m_context.fileMap;
    int              half   = 0;
    int              cell   = -1;
    int              slot   = -1;
    double           outer  = 0;
    double           inner  = 0;
    double           band   = 0;
    uint32_t         color  = 0;
    bool             isMine = false;



    for (int t = 0; t < map.tracks; t++)
    {
        int                    qt    = t * DiskImage::kQuarterTracksPerWholeTrack;
        const TrackAnalysis *  track = GetRingTrack (qt);

        if (track == nullptr || track->framed.nibbles.empty())
        {
            continue;
        }

        slot  = m_context.analysis->entries[qt].slot;
        outer = PlatterGeometry::GetRingOuter (qt) * view.outerRadiusPx;
        inner = PlatterGeometry::GetRingOuter (qt + DiskImage::kQuarterTracksPerWholeTrack) * view.outerRadiusPx;
        band  = inner + (outer - inner) * s_kTickShare;

        const RingTurns &  turns = GetRingTurns (slot, *track);

        for (const AnalyzedSector & sector : track->sectors)
        {
            cell = (sector.dataField >= 0) ? map.GetCellOf (t, sector.sector, half) : -1;

            if (cell < 0)
            {
                continue;
            }

            const LocatedField &  field = track->fields[sector.dataField];
            double                a     = turns.nibbles[field.firstNibble];
            double                b     = turns.nibbles[std::min<size_t> (static_cast<size_t> (field.firstNibble + field.nibbleCount), turns.nibbles.size() - 1)];
            const vector<int> &   own   = map.cells[cell].owners;

            isMine = std::find (own.begin(), own.end(), m_context.selectedFile) != own.end();
            color  = m_context.palette.GetRoleColor (static_cast<MapRole> (map.cells[cell].role));
            color  = (m_context.isFileHighlighted && !isMine) ? InspectorMarquee::Dim (m_context.palette, color) : color;

            ShadeRingArc (painter, view, a, b, inner, outer, (color & 0x00FFFFFFu) | s_kFilesAlpha);

            //  The marquee marks the file's sectors while the File map tab is
            //  up; a band along the inner edge marks them otherwise.
            if (isMine && !m_context.isFileHighlighted)
            {
                ShadeRingArc (painter, view, a, b, inner, band, theme.Accent());
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterView::GetFileArcs
//
//  The stretches the selected file's sectors cover on each whole track's
//  rings, a run of them as one stretch.
//
////////////////////////////////////////////////////////////////////////////////

void PlatterView::GetFileArcs (vector<FileArc> & outArcs) const
{
    const FileMap *  map  = m_context.fileMap;
    int              file = m_context.selectedFile;
    bool             isOn = map != nullptr && map->notMapped == NotMappedReason::None && file >= 0 && file < static_cast<int> (map->files.size());



    for (int t = 0; isOn && t < map->tracks; t++)
    {
        int                    qt    = t * DiskImage::kQuarterTracksPerWholeTrack;
        const TrackAnalysis *  track = GetRingTrack (qt);

        if (track == nullptr || track->framed.nibbles.empty())
        {
            continue;
        }

        for (const InspectorMarquee::Span & span : InspectorMarquee::GetFileSpans (*track, GetRingTurns (m_context.analysis->entries[qt].slot, *track).nibbles, *map, t, file))
        {
            outArcs.push_back ({ t, span.a, span.b });
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterView::DimOutsideFile
//
//  Every whole track's rings shaded by half toward the background, except
//  where the selected file's sectors lie, so they are the brightest part of
//  the platter (FR-090).
//
////////////////////////////////////////////////////////////////////////////////

void PlatterView::DimOutsideFile (IDxuiPainter & painter, const PlatterPlacement & view, const vector<FileArc> & arcs) const
{
    uint32_t                          dim   = (m_context.palette.background & 0x00FFFFFFu) | s_kDimAlpha;
    vector<std::pair<double, double>> spans;
    double                            from  = 0;
    double                            outer = 0;
    double                            inner = 0;



    for (int t = 0; t * DiskImage::kQuarterTracksPerWholeTrack < PlatterRenderer::kRingCount; t++)
    {
        outer = PlatterGeometry::GetRingOuter (t * DiskImage::kQuarterTracksPerWholeTrack) * view.outerRadiusPx;
        inner = PlatterGeometry::GetRingOuter ((t + 1) * DiskImage::kQuarterTracksPerWholeTrack) * view.outerRadiusPx;
        from  = 0;
        spans.clear();

        if (!IsBandVisible (view, inner, outer))
        {
            continue;
        }

        for (const FileArc & arc : arcs)
        {
            if (arc.wholeTrack == t)
            {
                spans.push_back ({ arc.a, std::min (arc.b, 1.0) });
            }

            if (arc.wholeTrack == t && arc.b > 1.0)
            {
                spans.push_back ({ 0.0, arc.b - 1.0 });
            }
        }

        std::sort (spans.begin(), spans.end());

        for (const std::pair<double, double> & span : spans)
        {
            if (span.first > from)
            {
                ShadeRingArc (painter, view, from, span.first, inner, outer, dim);
            }

            from = std::max (from, span.second);
        }

        if (from < 1.0)
        {
            ShadeRingArc (painter, view, from, 1.0, inner, outer, dim);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterView::PaintMarquee
//
//  A marching marquee around the outside of the selected file's sectors:
//  on each whole track, along its outer edge where the track outside it has
//  none of the file's sectors, along its inner edge where the track inside
//  has none, and across the track at each end of a stretch. A stretch that
//  crosses the index is split there with no edge between the halves.
//
////////////////////////////////////////////////////////////////////////////////

void PlatterView::PaintMarquee (IDxuiPainter & painter, const PlatterPlacement & view, const vector<FileArc> & arcs) const
{
    using Spans = vector<std::pair<double, double>>;

    static constexpr double  kJoin = 1e-9;



    InspectorMarquee  marquee (m_context.palette, m_scaler.ToPxf (InspectorMarquee::kDashDip), InspectorMarquee::kThicknessPx,
                               static_cast<int64_t> (GetTickCount64()));
    int               tracks = PlatterRenderer::kRingCount / DiskImage::kQuarterTracksPerWholeTrack;
    vector<Spans>     sets (static_cast<size_t> (tracks) + 2);
    double            outer  = 0;
    double            inner  = 0;
    float             s      = 0;

    auto  getX = [&] (double turn, double r) { return static_cast<float> (view.centerXPx + r * std::sin ((turn + view.rotation) * s_kTwoPi)); };
    auto  getY = [&] (double turn, double r) { return static_cast<float> (view.centerYPx - r * std::cos ((turn + view.rotation) * s_kTwoPi)); };

    //  The parts of `of` outside every span of `minus`.
    auto  subtract = [] (const Spans & of, const Spans & minus)
    {
        Spans  out;



        for (const std::pair<double, double> & span : of)
        {
            double  from = span.first;

            for (const std::pair<double, double> & cut : minus)
            {
                if (cut.second > from && cut.first < span.second)
                {
                    if (cut.first > from)
                    {
                        out.push_back ({ from, cut.first });
                    }

                    from = std::max (from, cut.second);
                }
            }

            if (from < span.second)
            {
                out.push_back ({ from, span.second });
            }
        }

        return out;
    };



    marquee.SetCullRect (m_boundsDip);

    //  Each whole track's stretches within one turn, sorted and joined; the
    //  sets are offset by one so the tracks beyond either end are empty.
    for (const FileArc & arc : arcs)
    {
        Spans &  set = sets[static_cast<size_t> (arc.wholeTrack) + 1];

        set.push_back ({ arc.a, std::min (arc.b, 1.0) });

        if (arc.b > 1.0)
        {
            set.push_back ({ 0.0, arc.b - 1.0 });
        }
    }

    for (Spans & set : sets)
    {
        Spans  joined;



        std::sort (set.begin(), set.end());

        for (const std::pair<double, double> & span : set)
        {
            if (!joined.empty() && span.first <= joined.back().second + kJoin)
            {
                joined.back().second = std::max (joined.back().second, span.second);
            }
            else
            {
                joined.push_back (span);
            }
        }

        set = joined;
    }

    for (int t = 0; t < tracks; t++)
    {
        const Spans &  set = sets[static_cast<size_t> (t) + 1];

        outer = PlatterGeometry::GetRingOuter (t * DiskImage::kQuarterTracksPerWholeTrack) * view.outerRadiusPx;
        inner = PlatterGeometry::GetRingOuter ((t + 1) * DiskImage::kQuarterTracksPerWholeTrack) * view.outerRadiusPx;

        if (set.empty() || !IsBandVisible (view, inner, outer))
        {
            continue;
        }

        for (const std::pair<double, double> & span : subtract (set, sets[static_cast<size_t> (t)]))
        {
            DrawMarqueeClear (marquee, painter, view, span.first, span.second, outer);
        }

        for (const std::pair<double, double> & span : subtract (set, sets[static_cast<size_t> (t) + 2]))
        {
            DrawMarqueeClear (marquee, painter, view, span.first, span.second, inner);
        }

        //  Across the track at each end, except at the index where a stretch
        //  continues past it.
        for (const std::pair<double, double> & span : set)
        {
            double  first = GetClearTurn (view, span.first,  inner, outer);
            double  last  = GetClearTurn (view, span.second, inner, outer);

            if (span.first > kJoin || set.back().second < 1.0 - kJoin)
            {
                s = marquee.DrawLine (painter, getX (first, inner), getY (first, inner), getX (first, outer), getY (first, outer), 0);
            }

            if (span.second < 1.0 - kJoin || set.front().first > kJoin)
            {
                s = marquee.DrawLine (painter, getX (last, outer), getY (last, outer), getX (last, inner), getY (last, inner), s);
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterView::PaintSelectedArc
//
//  The selected sector or nibbles, and the sector under the pointer so the
//  tooltip's subject is plain (FR-148), each marked on its ring.
//
////////////////////////////////////////////////////////////////////////////////

void PlatterView::PaintSelectedArc (IDxuiPainter & painter, const IDxuiTheme & theme, const PlatterPlacement & view) const
{
    PlatterHit             hit    = HitTest (m_context.hoverPx);
    const TrackAnalysis *  track  = (!m_isPanning && hit.isOnDisk && PtInRect (&m_boundsDip, m_context.hoverPx)) ? GetRingTrack (hit.quarterTrack) : nullptr;
    int                    sector = -1;
    double                 end    = 0;
    double                 start  = m_context.model->GetSelectionTurn (end);



    (void) theme;

    if (start >= 0)
    {
        PaintArcMark (painter, view, m_context.model->GetQuarterTrack(), start, end, true);
    }

    if (track != nullptr && track->framed.cellCount > 0)
    {
        sector = PlatterGeometry::GetSectorAt (*track, PlatterGeometry::GetCellAtTurn (*track, hit.turn));
    }

    if (sector >= 0 && !IsOverSelection (view, m_context.hoverPx))
    {
        const AnalyzedSector &  s = track->sectors[sector];

        PaintArcMark (painter, view, hit.quarterTrack, TrackAnalyzer::GetAngle (*track, track->fields[s.addressField].startCell),
                      TrackAnalyzer::GetAngle (*track, track->fields[s.dataField >= 0 ? s.dataField : s.addressField].endCell), false);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterView::PaintArcMark
//
//  A stretch of a ring marked across a band at least s_kMinMarkDip wide, so
//  the mark is plain at fit. The selection is outlined in the selection
//  color over a contrasting edge, the selection color dimmed to mid gray
//  while the shimmer's band sweeps so the band is plain to see against it
//  (FR-038).
//  The hovered sector is lit faintly and outlined thinly.
//
////////////////////////////////////////////////////////////////////////////////

void PlatterView::PaintArcMark (IDxuiPainter & painter, const PlatterPlacement & view, int quarterTrack, double start, double end, bool isSelected) const
{
    double    now    = InspectorMarquee::GetClockMs();
    double    r0     = 0;
    double    r1     = 0;
    int       rings  = 1;
    RECT      box    = {};
    double    step   = 0;
    double    fill0  = 0;
    double    fill1  = 0;
    float     thick  = isSelected ? m_scaler.ToPxf (s_kSelectOutlineDip) : 1.0f;
    uint32_t  ink    = isSelected ? m_context.palette.colors.selection : InspectorMarquee::GetHoverLine();
    uint32_t  edge   = DiskInspectorPalette::GetTextColorOn (m_context.palette.colors.selection);
    int       pieces = 0;
    double    a      = 0;
    double    b      = 0;
    double    m      = 0;
    auto      getX   = [&] (double turn, double r) { return static_cast<float> (view.centerXPx + r * std::sin ((turn + view.rotation) * s_kTwoPi)); };
    auto      getY   = [&] (double turn, double r) { return static_cast<float> (view.centerYPx - r * std::cos ((turn + view.rotation) * s_kTwoPi)); };
    auto      drawOutline = [&] (float w, const std::function<uint32_t (float, float)> & colorAt)
    {
        for (const std::pair<double, double> & part : GetVisibleParts (view, start, end))
        {
            pieces = std::clamp (static_cast<int> (std::ceil ((part.second - part.first) * s_kTwoPi * r1 / m_scaler.ToPxf (s_kMarqueeStepDip))), 1, s_kMaxPieces);

            for (int k = 0; k < pieces; k++)
            {
                a = part.first + (part.second - part.first) * k / pieces;
                b = part.first + (part.second - part.first) * (k + 1) / pieces;
                m = (a + b) / 2;

                painter.DrawLine (getX (a, r1), getY (a, r1), getX (b, r1), getY (b, r1), w, colorAt (getX (m, r1), getY (m, r1)));
                painter.DrawLine (getX (a, r0), getY (a, r0), getX (b, r0), getY (b, r0), w, colorAt (getX (m, r0), getY (m, r0)));
            }
        }

        painter.DrawLine (getX (start, r0), getY (start, r0), getX (start, r1), getY (start, r1), w, colorAt (getX (start, (r0 + r1) / 2), getY (start, (r0 + r1) / 2)));
        painter.DrawLine (getX (end, r0),   getY (end, r0),   getX (end, r1),   getY (end, r1),   w, colorAt (getX (end, (r0 + r1) / 2),   getY (end, (r0 + r1) / 2)));
    };



    GetMarkBand (view, quarterTrack, r0, r1);

    if (!IsBandVisible (view, r0, r1))
    {
        return;
    }

    end    = (end <= start) ? end + 1.0 : end;
    box    = GetArcBox (view, start, end, r0, r1);
    step   = isSelected ? std::max (1.0, InspectorMarquee::GetBandPx (box) / s_kShimmerSteps) : m_scaler.ToPxf (s_kShimmerStepDip);
    GetVisibleRadii (view, r0, r1, fill0, fill1);
    rings  = isSelected ? std::clamp (static_cast<int> (std::ceil ((fill1 - fill0) / step)), 1, s_kMaxRings) : 1;
    fill0  = isSelected ? fill0 : r0;
    fill1  = isSelected ? fill1 : r1;

    //  The fill in pieces a share of the band wide, along the ring and
    //  across it, so the band crosses the whole mark at 45 degrees with no
    //  steps to see.
    for (const std::pair<double, double> & part : GetVisibleParts (view, start, end))
    {
        pieces = std::clamp (static_cast<int> (std::ceil ((part.second - part.first) * s_kTwoPi * fill1 / step)), 1, std::max (1, s_kMaxQuads / rings));

        for (int k = 0; k < pieces; k++)
        {
            a = part.first + (part.second - part.first) * k / pieces;
            b = part.first + (part.second - part.first) * (k + 1) / pieces;
            m = (a + b) / 2;

            for (int j = 0; j < rings; j++)
            {
                double  ra = fill0 + (fill1 - fill0) * j / rings;
                double  rb = fill0 + (fill1 - fill0) * (j + 1) / rings;

                ShadeArc (painter, view, a, b, ra, rb, isSelected ? InspectorMarquee::GetShimmer (now, getX (m, (ra + rb) / 2), getY (m, (ra + rb) / 2), box)
                                                                  : InspectorMarquee::GetHoverFill());
            }
        }
    }

    //  The outline: for the selection, the edge wider than the line, then
    //  the selection color, dimmed to mid gray while the band sweeps so the
    //  band is plain to see against it.
    if (isSelected)
    {
        ink = DxuiColor::Lerp (ink, s_kSweepLineArgb, static_cast<float> (InspectorMarquee::GetSweepWeight (now)));
        drawOutline (thick + 2.0f, [&] (float, float) { return edge; });
    }

    drawOutline (thick, [&] (float, float) { return ink; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterView::GetVisibleRadii
//
//  The part of the radii from r0 to r1 that falls within the view: no nearer
//  the platter's center than the view's nearest point, and no farther than
//  its farthest corner.
//
////////////////////////////////////////////////////////////////////////////////

void PlatterView::GetVisibleRadii (const PlatterPlacement & view, double r0, double r1, double & outNear, double & outFar) const
{
    double  dx       = std::max ({ m_boundsDip.left - static_cast<double> (view.centerXPx), 0.0, view.centerXPx - static_cast<double> (m_boundsDip.right) });
    double  dy       = std::max ({ m_boundsDip.top  - static_cast<double> (view.centerYPx), 0.0, view.centerYPx - static_cast<double> (m_boundsDip.bottom) });
    double  farthest = 0;



    for (LONG x : { m_boundsDip.left, m_boundsDip.right })
    {
        for (LONG y : { m_boundsDip.top, m_boundsDip.bottom })
        {
            farthest = std::max (farthest, std::hypot (x - static_cast<double> (view.centerXPx), y - static_cast<double> (view.centerYPx)));
        }
    }

    outNear = std::max (r0, std::hypot (dx, dy));
    outFar  = std::max (outNear, std::min (r1, farthest));
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterView::GetArcBox
//
//  The box around a mark's part in view: every point of its edges at the
//  marquee's steps, clipped to the view, for the shimmer's sweep.
//
////////////////////////////////////////////////////////////////////////////////

RECT PlatterView::GetArcBox (const PlatterPlacement & view, double start, double end, double r0, double r1) const
{
    RECT    box    = { LONG_MAX, LONG_MAX, LONG_MIN, LONG_MIN };
    int     pieces = 0;
    double  t      = 0;
    double  x      = 0;
    double  y      = 0;



    for (const std::pair<double, double> & part : GetVisibleParts (view, start, end))
    {
        pieces = std::clamp (static_cast<int> (std::ceil ((part.second - part.first) * s_kTwoPi * r1 / m_scaler.ToPxf (s_kMarqueeStepDip))), 1, s_kMaxPieces);

        for (int k = 0; k <= pieces; k++)
        {
            t = (part.first + (part.second - part.first) * k / pieces + view.rotation) * s_kTwoPi;

            for (double r : { r0, r1 })
            {
                x          = view.centerXPx + r * std::sin (t);
                y          = view.centerYPx - r * std::cos (t);
                box.left   = std::min (box.left,   static_cast<LONG> (std::floor (x)));
                box.top    = std::min (box.top,    static_cast<LONG> (std::floor (y)));
                box.right  = std::max (box.right,  static_cast<LONG> (std::ceil (x)));
                box.bottom = std::max (box.bottom, static_cast<LONG> (std::ceil (y)));
            }
        }
    }

    box.left   = std::max (box.left,   m_boundsDip.left);
    box.top    = std::max (box.top,    m_boundsDip.top);
    box.right  = std::min (box.right,  m_boundsDip.right);
    box.bottom = std::min (box.bottom, m_boundsDip.bottom);

    return box;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterView::GetMarkBand
//
//  The radii between which a ring's mark is drawn: the ring itself, or a
//  band s_kMinMarkDip wide centered on it where the ring is narrower.
//
////////////////////////////////////////////////////////////////////////////////

void PlatterView::GetMarkBand (const PlatterPlacement & view, int quarterTrack, double & outR0, double & outR1) const
{
    double  outer = PlatterGeometry::GetRingOuter (quarterTrack) * view.outerRadiusPx;
    double  inner = PlatterGeometry::GetRingOuter (quarterTrack + 1) * view.outerRadiusPx;
    double  half  = std::max ((outer - inner) / 2, static_cast<double> (m_scaler.ToPxf (s_kMinMarkDip)) / 2);



    outR0 = (outer + inner) / 2 - half;
    outR1 = (outer + inner) / 2 + half;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterView::IsOverSelection
//
//  Whether the point is on the selection's mark, its outline included: the
//  point's radius within the mark's band and its angle within the
//  selection's turns.
//
////////////////////////////////////////////////////////////////////////////////

bool PlatterView::IsOverSelection (const PlatterPlacement & view, POINT pointPx) const
{
    double  end    = 0;
    double  start  = m_context.model->GetSelectionTurn (end);
    double  dx     = pointPx.x - view.centerXPx;
    double  dy     = pointPx.y - view.centerYPx;
    double  r      = std::hypot (dx, dy);
    double  slack  = m_scaler.ToPxf (s_kSelectOutlineDip) + 2.0;
    double  r0     = 0;
    double  r1     = 0;
    double  turn   = 0;
    double  margin = 0;
    bool    isOver = false;



    if (start >= 0 && r > 0 && PtInRect (&m_boundsDip, pointPx))
    {
        GetMarkBand (view, m_context.model->GetQuarterTrack(), r0, r1);

        end    = (end <= start) ? end + 1.0 : end;
        margin = slack / (s_kTwoPi * r);
        turn   = std::atan2 (dx, -dy) / s_kTwoPi - view.rotation;
        turn  -= std::floor (turn - (start - margin));
        isOver = r >= r0 - slack && r <= r1 + slack && turn <= end + margin;
    }

    return isOver;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterView::GetSelectionBorder
//
//  The selection's mark on the platter: its turns, the end past the start,
//  and the radii of its border's two arcs; false with nothing selected.
//
////////////////////////////////////////////////////////////////////////////////

bool PlatterView::GetSelectionBorder (const PlatterPlacement & view, double & outStart, double & outEnd, double & outR0, double & outR1) const
{
    outStart = m_context.model->GetSelectionTurn (outEnd);

    if (outStart >= 0)
    {
        outEnd = (outEnd <= outStart) ? outEnd + 1.0 : outEnd;
        GetMarkBand (view, m_context.model->GetQuarterTrack(), outR0, outR1);
    }

    return outStart >= 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterView::DrawMarqueeClear
//
//  A marquee arc as DrawMarqueeArc draws it, except where it lies on the
//  selection's border: there the arc moves just outside the border, outward
//  from its outer arc and inward from its inner one, with a short step
//  across the ring at each end of the moved part.
//
////////////////////////////////////////////////////////////////////////////////

void PlatterView::DrawMarqueeClear (const InspectorMarquee & marquee, IDxuiPainter & painter, const PlatterPlacement & view, double a, double b, double r) const
{
    double                              start  = 0;
    double                              end    = 0;
    double                              r0     = 0;
    double                              r1     = 0;
    double                              apart  = (m_scaler.ToPxf (s_kSelectOutlineDip) + 2.0) / 2.0 + 1.0;
    double                              clear  = r;
    double                              pad    = apart / (s_kTwoPi * std::max (r, 1.0));
    double                              cursor = a;
    bool                                isOn   = GetSelectionBorder (view, start, end, r0, r1) && (std::abs (r - r1) <= apart || std::abs (r - r0) <= apart);
    vector<std::pair<double, double>>   moved;
    auto                                step   = [&] (double turn, double from, double to)
    {
        marquee.DrawLine (painter, static_cast<float> (view.centerXPx + from * std::sin ((turn + view.rotation) * s_kTwoPi)),
                          static_cast<float> (view.centerYPx - from * std::cos ((turn + view.rotation) * s_kTwoPi)),
                          static_cast<float> (view.centerXPx + to * std::sin ((turn + view.rotation) * s_kTwoPi)),
                          static_cast<float> (view.centerYPx - to * std::cos ((turn + view.rotation) * s_kTwoPi)), 0);
    };



    //  The parts of the arc within the selection's turns, a turn either way
    //  included, as the arc and the selection can each pass the index.
    for (double k = -1.0; isOn && k <= 1.0; k += 1.0)
    {
        if (std::max (a, start - pad + k) < std::min (b, end + pad + k))
        {
            moved.push_back ({ std::max (a, start - pad + k), std::min (b, end + pad + k) });
        }
    }

    clear = (std::abs (r - r1) <= apart) ? r1 + apart : r0 - apart;

    for (const std::pair<double, double> & part : moved)
    {
        if (part.first > cursor)
        {
            DrawMarqueeArc (marquee, painter, view, cursor, part.first, r);
        }

        step (part.first, r, clear);
        DrawMarqueeArc (marquee, painter, view, part.first, part.second, clear);
        step (part.second, clear, r);
        cursor = part.second;
    }

    if (cursor < b)
    {
        DrawMarqueeArc (marquee, painter, view, cursor, b, r);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterView::GetClearTurn
//
//  Where a marquee line across the ring from `inner` to `outer` is drawn: at
//  its turn, or just outside the selection's border where it lies on one of
//  the border's ends.
//
////////////////////////////////////////////////////////////////////////////////

double PlatterView::GetClearTurn (const PlatterPlacement & view, double turn, double inner, double outer) const
{
    double  start  = 0;
    double  end    = 0;
    double  r0     = 0;
    double  r1     = 0;
    double  apart  = (m_scaler.ToPxf (s_kSelectOutlineDip) + 2.0) / 2.0 + 1.0;
    double  pad    = 0;
    double  toEnd  = 0;
    double  toHead = 0;
    bool    isOn   = GetSelectionBorder (view, start, end, r0, r1) && std::max (inner, outer) >= r0 - apart && std::min (inner, outer) <= r1 + apart;



    if (isOn)
    {
        pad    = apart / (s_kTwoPi * std::max ((r0 + r1) / 2, 1.0));
        toHead = turn - start - std::round (turn - start);
        toEnd  = turn - end   - std::round (turn - end);
        turn   = (std::abs (toHead) <= pad) ? start - pad : (std::abs (toEnd) <= pad) ? end + pad : turn;
    }

    return turn;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterView::DrawMarqueeArc
//
//  The marquee along a ring's edge between two turns, in steps short enough
//  to follow the curve, over only the parts in view.
//
////////////////////////////////////////////////////////////////////////////////

void PlatterView::DrawMarqueeArc (const InspectorMarquee & marquee, IDxuiPainter & painter, const PlatterPlacement & view, double a, double b, double r) const
{
    float  s      = 0;
    int    pieces = 0;
    auto   getX   = [&] (double turn) { return static_cast<float> (view.centerXPx + r * std::sin ((turn + view.rotation) * s_kTwoPi)); };
    auto   getY   = [&] (double turn) { return static_cast<float> (view.centerYPx - r * std::cos ((turn + view.rotation) * s_kTwoPi)); };



    for (const std::pair<double, double> & part : GetVisibleParts (view, a, b))
    {
        pieces = std::clamp (static_cast<int> (std::ceil ((part.second - part.first) * s_kTwoPi * r / m_scaler.ToPxf (s_kMarqueeStepDip))), 1, s_kMaxPieces);
        s      = 0;

        for (int k = 0; k < pieces; k++)
        {
            double  t0 = part.first + (part.second - part.first) * k / pieces;
            double  t1 = part.first + (part.second - part.first) * (k + 1) / pieces;

            s = marquee.DrawLine (painter, getX (t0), getY (t0), getX (t1), getY (t1), s);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterView::GetVisibleParts
//
//  The parts of the stretch from turn `a` to turn `b` inside the view: all
//  of it when the platter's center is inside, otherwise its overlap with the
//  turns between the view's corners, a turn either side included.
//
////////////////////////////////////////////////////////////////////////////////

vector<std::pair<double, double>> PlatterView::GetVisibleParts (const PlatterPlacement & view, double a, double b) const
{
    vector<std::pair<double, double>>  parts;
    double                             start = 0;
    double                             end   = 1;
    double                             low   = 0;
    double                             high  = 0;



    if (!PlatterGeometry::GetVisibleTurns (view, m_boundsDip, start, end))
    {
        parts.push_back ({ a, b });
    }

    for (int k = -2; parts.empty() && k <= 2; k++)
    {
        low  = std::max (a, start + k);
        high = std::min (b, end + k);

        if (high > low)
        {
            parts.push_back ({ low, high });
        }
    }

    return parts;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterView::IsBandVisible
//
//  Whether any of the view lies between two radii of the platter.
//
////////////////////////////////////////////////////////////////////////////////

bool PlatterView::IsBandVisible (const PlatterPlacement & view, double r0, double r1) const
{
    double  dx      = std::max ({ m_boundsDip.left - view.centerXPx, 0.0f, view.centerXPx - m_boundsDip.right });
    double  dy      = std::max ({ m_boundsDip.top - view.centerYPx, 0.0f, view.centerYPx - m_boundsDip.bottom });
    double  nearest = std::hypot (dx, dy);
    double  farX    = std::max (std::abs (m_boundsDip.left - view.centerXPx), std::abs (m_boundsDip.right - view.centerXPx));
    double  farY    = std::max (std::abs (m_boundsDip.top - view.centerYPx), std::abs (m_boundsDip.bottom - view.centerYPx));



    return r1 >= nearest && r0 <= std::hypot (farX, farY);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterView::PaintDiffs
//
//  The "Differences" overlay (FR-121): where each difference lies on A's
//  ring, across the whole track for a whole track as the Files overlay
//  marks it; a track only one disk has, or a volume number that differs,
//  marks its whole ring.
//
////////////////////////////////////////////////////////////////////////////////

void PlatterView::PaintDiffs (IDxuiPainter & painter, const PlatterPlacement & view)
{
    uint32_t  color = (m_context.palette.colors.difference & 0x00FFFFFFu) | s_kFilesAlpha;



    for (const Difference & d : m_context.comparison->differences)
    {
        int                    qt    = d.quarterTrack;
        int                    span  = (qt % DiskImage::kQuarterTracksPerWholeTrack == 0) ? DiskImage::kQuarterTracksPerWholeTrack : 1;
        const TrackAnalysis *  track = GetRingTrack (qt);
        double                 outer = 0;
        double                 inner = 0;
        double                 a     = 0;
        double                 b     = 1;

        if (qt < 0)
        {
            continue;
        }

        outer = PlatterGeometry::GetRingOuter (qt) * view.outerRadiusPx;
        inner = PlatterGeometry::GetRingOuter (std::min (qt + span, static_cast<int> (PlatterRenderer::kRingCount))) * view.outerRadiusPx;

        if (track != nullptr && !track->framed.nibbles.empty())
        {
            GetDiffTurns (d, *track, GetRingTurns (m_context.analysis->entries[qt].slot, *track), a, b);
        }

        ShadeRingArc (painter, view, a, std::max (b, a + s_kMinDiffTurn), inner, outer, color);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterView::PaintHits
//
//  Each find hit (FR-056) as a stretch of its ring, across the whole track
//  for a whole track as the Differences overlay marks it, with a dark edge
//  so it contrasts with the ring's own colors. A stretch is at least a
//  few pixels long, so a hit is visible at fit, and once it is long on
//  screen it is translucent, so the nibbles under it remain visible. Hits closer
//  together than that are joined, and every edge is drawn before any fill.
//
////////////////////////////////////////////////////////////////////////////////

void PlatterView::PaintHits (IDxuiPainter & painter, const PlatterPlacement & view)
{
    struct Stretch
    {
        double  a     = 0;
        double  b     = 0;
        double  inner = 0;
        double  outer = 0;
    };

    uint32_t         fill      = m_context.palette.colors.findHit;
    uint32_t         edge      = DiskInspectorPalette::GetTextColorOn (fill);
    double           edgePx    = m_scaler.ToPxf (s_kHitEdgeDip);
    double           minPx     = m_scaler.ToPxf (s_kMinHitDip);
    vector<Stretch>  stretches;
    int              qt        = 0;
    int              slot      = 0;
    int              span      = 0;
    int              size      = 0;
    double           outer     = 0;
    double           inner     = 0;
    double           minTurn   = 0;
    double           a         = 0;
    double           b         = 0;



    for (const auto & [track, marks] : m_context.findMarks->GetTracks())
    {
        qt   = marks.quarterTrack;
        slot = (qt >= 0) ? m_context.analysis->entries[qt].slot : -1;

        //  Hits on a record analyzed again since the search are listed in the
        //  Find results tab, marked out of date, but not drawn on the platter.
        if (slot < 0 || m_context.analysis->tracks[slot].get() != track)
        {
            continue;
        }

        size = static_cast<int> (track->framed.nibbles.size());

        if (size == 0)
        {
            continue;
        }

        const RingTurns &  turns = GetRingTurns (slot, *track);

        span    = (qt % DiskImage::kQuarterTracksPerWholeTrack == 0) ? DiskImage::kQuarterTracksPerWholeTrack : 1;
        outer   = PlatterGeometry::GetRingOuter (qt) * view.outerRadiusPx;
        inner   = PlatterGeometry::GetRingOuter (std::min (qt + span, static_cast<int> (PlatterRenderer::kRingCount))) * view.outerRadiusPx;
        minTurn = minPx / (s_kTwoPi * std::max (outer, 1.0));

        for (const FindMarks::Run & run : marks.nibbles)
        {
            a = turns.nibbles[static_cast<size_t> (run.first % size)];
            b = turns.nibbles[static_cast<size_t> ((run.first + run.count) % size)];
            b = std::max ((b > a) ? b : b + 1.0, a + minTurn);

            if (!stretches.empty() && stretches.back().outer == outer && a <= stretches.back().b + minTurn)
            {
                stretches.back().b = std::max (stretches.back().b, b);
            }
            else
            {
                stretches.push_back ({ a, b, inner, outer });
            }
        }
    }

    auto  isLong = [&] (const Stretch & s) { return (s.b - s.a) * s_kTwoPi * s.outer > m_scaler.ToPxf (s_kHitSolidDip); };

    //  Under a translucent stretch the edge is only a frame, which the fill
    //  does not cover.
    for (const Stretch & s : stretches)
    {
        double  t = edgePx / (s_kTwoPi * s.outer);

        if (isLong (s))
        {
            ShadeRingArc (painter, view, s.a - t, s.b + t, s.outer,          s.outer + edgePx, edge);
            ShadeRingArc (painter, view, s.a - t, s.b + t, s.inner - edgePx, s.inner,          edge);
            ShadeRingArc (painter, view, s.a - t, s.a,     s.inner,          s.outer,          edge);
            ShadeRingArc (painter, view, s.b,     s.b + t, s.inner,          s.outer,          edge);
        }
        else
        {
            ShadeRingArc (painter, view, s.a - t, s.b + t, s.inner - edgePx, s.outer + edgePx, edge);
        }
    }

    for (const Stretch & s : stretches)
    {
        ShadeRingArc (painter, view, s.a, s.b, s.inner, s.outer, isLong (s) ? (fill & 0x00FFFFFFu) | s_kHitAlpha : fill);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterView::GetDiffTurns
//
//  Where a difference starts and ends on A's ring, as turns: its nibbles,
//  its sector from the address field to the end of the data field, or its
//  cells; the whole ring for a difference about the whole track.
//
////////////////////////////////////////////////////////////////////////////////

void PlatterView::GetDiffTurns (const Difference & difference, const TrackAnalysis & track, const RingTurns & turns, double & outA, double & outB)
{
    const Difference &  d      = difference;
    size_t              last   = turns.nibbles.size() - 1;
    size_t              lastC  = turns.cells.empty() ? 0 : turns.cells.size() - 1;
    int                 first  = -1;
    int                 end    = -1;



    outA = 0;
    outB = 1;

    if (d.kind == DifferenceKind::Nibbles && d.firstNibbleA >= 0)
    {
        outA = turns.nibbles[std::min<size_t> (static_cast<size_t> (d.firstNibbleA), last)];
        outB = turns.nibbles[std::min<size_t> (static_cast<size_t> (d.firstNibbleA + d.nibbleCountA), last)];
    }
    else if (d.kind == DifferenceKind::Timing && !turns.cells.empty())
    {
        outA = turns.cells[std::min<size_t> (d.cell, lastC)];
        outB = turns.cells[std::min<size_t> (static_cast<size_t> (d.cell) + static_cast<size_t> (d.count), lastC)];
    }
    else if (d.sector >= 0)
    {
        for (const AnalyzedSector & s : track.sectors)
        {
            if (first < 0 && s.sector == d.sector)
            {
                first = (s.addressField >= 0) ? track.fields[s.addressField].firstNibble : track.fields[s.dataField].firstNibble;
                end   = (s.dataField >= 0) ? track.fields[s.dataField].firstNibble + track.fields[s.dataField].nibbleCount
                                           : track.fields[s.addressField].firstNibble + track.fields[s.addressField].nibbleCount;
            }
        }

        if (first >= 0)
        {
            outA = turns.nibbles[std::min<size_t> (static_cast<size_t> (first), last)];
            outB = turns.nibbles[std::min<size_t> (static_cast<size_t> (end), last)];
        }
        else if (!turns.cells.empty())
        {
            outA = turns.cells[std::min<size_t> (d.cell, lastC)];
            outB = outA;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterView::ShadeRingArc
//
//  A stretch of a ring filled between two turns, over only the parts in
//  view, in quadrilaterals short enough that each chord lies close to its
//  arc.
//
////////////////////////////////////////////////////////////////////////////////

void PlatterView::ShadeRingArc (IDxuiPainter & painter, const PlatterPlacement & view, double a, double b, double r0, double r1, uint32_t argb) const
{
    int  pieces = 0;



    if (!IsBandVisible (view, r0, r1))
    {
        return;
    }

    for (const std::pair<double, double> & part : GetVisibleParts (view, a, b))
    {
        pieces = std::clamp (std::max (static_cast<int> (std::ceil ((part.second - part.first) * s_kPiecesPerTurn)),
                                       static_cast<int> (std::ceil ((part.second - part.first) * s_kTwoPi * r1 / m_scaler.ToPxf (s_kShadeStepDip)))), 1, s_kMaxPieces);

        for (int k = 0; k < pieces; k++)
        {
            ShadeArc (painter, view, part.first + (part.second - part.first) * k / pieces, part.first + (part.second - part.first) * (k + 1) / pieces, r0, r1, argb);
        }
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
    bool          hasTip = BuildTip (pointPx, tip);



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
