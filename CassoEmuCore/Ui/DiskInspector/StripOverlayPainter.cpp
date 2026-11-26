#include "Pch.h"

#include "Ui/DiskInspector/InspectorMarquee.h"
#include "Ui/DiskInspector/StripOverlayPainter.h"





static constexpr float     s_kDividerMinDip = 4.0f;
static constexpr float     s_kFilesShare    = 0.3f;
static constexpr float     s_kDiffBandDip   = 4.0f;
static constexpr float     s_kDiffLiftDip   = 2.0f;
static constexpr float     s_kMinHitDip     = 4.0f;
static constexpr float     s_kHitLineDip    = 2.0f;
static constexpr uint32_t  s_kDimMatch      = 0xCC000000u;
static constexpr float     s_kHeadLineDip   = 2.0f;
static constexpr float     s_kTrailDip      = 48.0f;
static constexpr int       s_kTrailSteps    = 12;
static constexpr uint32_t  s_kTrailAlpha    = 0x90u;





////////////////////////////////////////////////////////////////////////////////
//
//  StripOverlayPainter::PaintDiffs
//
//  While comparing, the bar is dimmed except where this side's nibbles
//  differ from the other side's, and a band along the bottom of the bar,
//  above the focused strip's outline, marks each differing run (FR-121).
//
////////////////////////////////////////////////////////////////////////////////

void StripOverlayPainter::PaintDiffs (IDxuiPainter & painter, const TrackAnalysis & track, const StripGeometry & g) const
{
    RECT                             bar    = m_scene.bar;
    float                            h      = m_scene.scaler.ToPxf (s_kDiffBandDip);
    float                            lift   = m_scene.scaler.ToPxf (s_kDiffLiftDip);
    int                              size   = static_cast<int> (track.framed.nibbles.size());
    std::array<StripSegment, 2>      parts  = {};
    int                              count  = 0;
    int                              first  = 0;
    int                              length = 0;
    double                           a      = 0;
    double                           b      = 0;
    vector<std::pair<float, float>>  spans;
    float                            at     = static_cast<float> (bar.left);



    for (const Difference & d : (m_scene.context.nibbleDiffs != nullptr && size > 0) ? *m_scene.context.nibbleDiffs : vector<Difference>())
    {
        first  = (m_scene.side == 0) ? d.firstNibbleA : d.firstNibbleB;
        length = std::max ((m_scene.side == 0) ? d.nibbleCountA : d.nibbleCountB, 1);

        if (first < 0)
        {
            continue;
        }

        a     = m_scene.hits.GetTurns()[static_cast<size_t> (first % size)];
        b     = m_scene.hits.GetTurns()[static_cast<size_t> ((first + length) % size)];
        count = g.GetSegments (a, (b > a) ? b - a : b + 1.0 - a, parts);

        for (int k = 0; k < count; k++)
        {
            painter.FillRect (parts[k].x0, static_cast<float> (bar.bottom) - h - lift, std::max (parts[k].x1 - parts[k].x0, 1.0f), h, m_scene.context.palette.colors.difference);
            spans.push_back ({ parts[k].x0, std::max (parts[k].x1, parts[k].x0 + 1.0f) });
        }
    }

    if (m_scene.context.nibbleDiffs == nullptr || size == 0)
    {
        return;
    }

    std::sort (spans.begin(), spans.end());

    for (const auto & [x0, x1] : spans)
    {
        if (x0 > at)
        {
            painter.FillRect (at, static_cast<float> (bar.top), x0 - at, static_cast<float> (bar.bottom - bar.top), s_kDimMatch);
        }

        at = std::max (at, x1);
    }

    if (at < static_cast<float> (bar.right))
    {
        painter.FillRect (at, static_cast<float> (bar.top), static_cast<float> (bar.right) - at, static_cast<float> (bar.bottom - bar.top), s_kDimMatch);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  StripOverlayPainter::PaintHits
//
//  Each run of nibbles with find hits (FR-056) outlined across the bar in
//  the hit color, framed in a dark line so it reads apart from the gold of
//  the address marks it often contains, and at least a few pixels wide so
//  a hit is visible at any zoom.
//
////////////////////////////////////////////////////////////////////////////////

void StripOverlayPainter::PaintHits (IDxuiPainter & painter, const TrackAnalysis & track, const StripGeometry & g) const
{
    const FindMarks::TrackMarks *  marks  = (m_scene.context.findMarks != nullptr) ? m_scene.context.findMarks->GetTrack (&track) : nullptr;
    RECT                           bar    = m_scene.bar;
    float                          top    = static_cast<float> (bar.top);
    float                          h      = static_cast<float> (bar.bottom - bar.top);
    float                          line   = m_scene.scaler.ToPxf (s_kHitLineDip);
    float                          edge   = m_scene.scaler.ToPxf (1.0f);
    float                          minW   = m_scene.scaler.ToPxf (s_kMinHitDip);
    uint32_t                       fill   = m_scene.context.palette.colors.findHit;
    int                            size   = static_cast<int> (track.framed.nibbles.size());
    std::array<StripSegment, 2>    parts  = {};
    int                            count  = 0;
    double                         a      = 0;
    double                         b      = 0;
    float                          w      = 0;



    for (const FindMarks::Run & run : (marks != nullptr && size > 0) ? marks->nibbles : vector<FindMarks::Run>())
    {
        a     = m_scene.hits.GetTurns()[static_cast<size_t> (run.first % size)];
        b     = m_scene.hits.GetTurns()[static_cast<size_t> ((run.first + run.count) % size)];
        count = g.GetSegments (a, (b > a) ? b - a : b + 1.0 - a, parts);

        for (int k = 0; k < count; k++)
        {
            w = std::max (parts[k].x1 - parts[k].x0, minW);

            painter.OutlineRect (parts[k].x0 - edge, top - edge, w + 2 * edge, h + 2 * edge, edge, DiskInspectorPalette::GetTextColorOn (fill));
            painter.OutlineRect (parts[k].x0, top, w, h, line, fill);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  StripOverlayPainter::PaintSelection
//
//  A run of selected nibbles outlined across the bar (FR-043). An outline,
//  not a shade, so each nibble keeps the color its kind gives it.
//
////////////////////////////////////////////////////////////////////////////////

void StripOverlayPainter::PaintSelection (IDxuiPainter & painter, const IDxuiTheme & theme, const StripGeometry & g) const
{
    RECT                         bar   = m_scene.bar;
    std::array<StripSegment, 2>  parts = {};
    int                          first = m_scene.context.model->GetFirstNibble();
    int                          end   = first + m_scene.context.model->GetNibbleCount();
    int                          count = 0;
    int                          k     = 0;



    if (first >= 0 && end > first && end < static_cast<int> (m_scene.hits.GetTurns().size()))
    {
        count = g.GetSegments (m_scene.hits.GetTurns()[first], m_scene.hits.GetTurns()[end] - m_scene.hits.GetTurns()[first], parts);
    }

    for (k = 0; k < count; k++)
    {
        painter.OutlineRect (parts[k].x0, static_cast<float> (bar.top), std::max (parts[k].x1 - parts[k].x0, 1.0f), static_cast<float> (bar.bottom - bar.top),
                             m_scene.scaler.ToPxf (StripPaintScene::kOutlineDip), theme.Accent());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  StripOverlayPainter::PaintShimmer
//
//  The selection lit by the window's shimmer (FR-038), and the nibble under
//  the pointer lit faintly and outlined, so the tooltip's subject is plain
//  (FR-148).
//
////////////////////////////////////////////////////////////////////////////////

void StripOverlayPainter::PaintShimmer (IDxuiPainter & painter, const StripGeometry & g) const
{
    double                       now    = InspectorMarquee::GetClockMs();
    RECT                         bar    = m_scene.bar;
    std::array<StripSegment, 2>  parts  = {};
    double                       end    = 0;
    double                       start  = m_scene.context.model->GetSelectionTurn (end);
    float                        height = static_cast<float> (bar.bottom - bar.top);
    int                          count  = 0;
    int                          nibble = -1;
    LONG                         top    = (m_scene.lightEnd > m_scene.lightTop) ? m_scene.lightTop : bar.top;
    LONG                         bottom = (m_scene.lightEnd > m_scene.lightTop) ? m_scene.lightEnd : bar.bottom;
    RECT                         box    = { bar.right, top, bar.left, bottom };



    if (start >= 0)
    {
        end   = (end <= start) ? end + 1.0 : end;
        count = g.GetSegments (start, end - start, parts);
    }

    //  A selection of bytes also lights the rest of the nibbles they occupy.
    for (const auto & run : m_scene.context.model->GetByteRuns())
    {
        std::array<StripSegment, 2>  runParts = {};
        int                          runCount = g.GetSegments (run.first, ((run.second <= run.first) ? run.second + 1.0 : run.second) - run.first, runParts);

        for (int k = 0; k < runCount; k++)
        {
            InspectorMarquee::PaintShimmer (painter, runParts[k].x0, static_cast<float> (bar.top), runParts[k].x1 - runParts[k].x0, height, now,
                                            { static_cast<LONG> (runParts[k].x0), top, static_cast<LONG> (runParts[k].x1), bottom });
        }
    }

    //  The band crosses the selection's part in view, however wide the zoom
    //  makes it, in the same time as any other object.
    for (int k = 0; k < count; k++)
    {
        box.left  = std::min (box.left,  static_cast<LONG> (std::max (parts[k].x0, static_cast<float> (bar.left))));
        box.right = std::max (box.right, static_cast<LONG> (std::min (parts[k].x1, static_cast<float> (bar.right))));
    }

    for (int k = 0; k < count; k++)
    {
        InspectorMarquee::PaintShimmer (painter, parts[k].x0, static_cast<float> (bar.top), parts[k].x1 - parts[k].x0, height, now, box);
    }

    if (PtInRect (&bar, m_scene.context.hoverPx) && !m_scene.isPanning && m_scene.context.GetTrack() != nullptr)
    {
        nibble = m_scene.hits.GetNibbleAt (g, static_cast<float> (m_scene.context.hoverPx.x));
    }

    count = (nibble >= 0 && nibble + 1 < static_cast<int> (m_scene.hits.GetTurns().size())) ? g.GetSegments (m_scene.hits.GetTurns()[nibble], m_scene.hits.GetTurns()[nibble + 1] - m_scene.hits.GetTurns()[nibble], parts) : 0;

    for (int k = 0; k < count; k++)
    {
        painter.FillRect    (parts[k].x0, static_cast<float> (bar.top), std::max (parts[k].x1 - parts[k].x0, 2.0f), height, InspectorMarquee::GetHoverFill());
        painter.OutlineRect (parts[k].x0, static_cast<float> (bar.top), std::max (parts[k].x1 - parts[k].x0, 2.0f), height, 1.0f, InspectorMarquee::GetHoverLine());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  StripOverlayPainter::PaintDividers
//
//  A divider before each nibble wide enough to tell from its neighbors,
//  drawn after the Files band so it runs the bar's full height.
//
////////////////////////////////////////////////////////////////////////////////

void StripOverlayPainter::PaintDividers (IDxuiPainter & painter, const IDxuiTheme & theme, const StripGeometry & g, float top, float height) const
{
    std::array<StripSegment, 2>  parts = {};
    int                          count = 0;



    for (size_t i = 0; i + 1 < m_scene.hits.GetTurns().size(); i++)
    {
        count = g.GetSegments (m_scene.hits.GetTurns()[i], m_scene.hits.GetTurns()[i + 1] - m_scene.hits.GetTurns()[i], parts);

        for (int k = 0; k < count; k++)
        {
            if (parts[k].x1 - parts[k].x0 >= m_scene.scaler.ToPxf (s_kDividerMinDip))
            {
                painter.FillRect (parts[k].x0, top, 1.0f, height, theme.Background());
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  StripOverlayPainter::PaintFiles
//
//  With the "Files" overlay on a whole track of a mapped volume, each data
//  field's role as a band along the bottom of the bar, and the selected
//  file's fields outlined (FR-090).
//
////////////////////////////////////////////////////////////////////////////////

void StripOverlayPainter::PaintFiles (IDxuiPainter & painter, const IDxuiTheme & theme, const TrackAnalysis & track, const StripGeometry & g) const
{
    const FileMap *              map    = m_scene.context.fileMap;
    int                          qt     = m_scene.context.model->GetQuarterTrack();
    RECT                         bar    = m_scene.bar;
    float                        height = static_cast<float> (bar.bottom - bar.top);
    std::array<StripSegment, 2>  parts  = {};
    int                          count  = 0;
    int                          cell   = -1;
    int                          half   = 0;
    bool                         isOn   = m_scene.context.isFilesOverlay && map != nullptr && map->notMapped == NotMappedReason::None &&
                                          qt % DiskImage::kQuarterTracksPerWholeTrack == 0;



    for (const AnalyzedSector & sector : isOn ? track.sectors : vector<AnalyzedSector>())
    {
        cell = (sector.dataField >= 0 && map != nullptr) ? map->GetCellOf (qt / DiskImage::kQuarterTracksPerWholeTrack, sector.sector, half) : -1;

        if (cell < 0)
        {
            continue;
        }

        const LocatedField  & field     = track.fields[sector.dataField];
        double                a         = m_scene.hits.GetTurns()[field.firstNibble];
        double                b         = m_scene.hits.GetTurns()[std::min<size_t> (static_cast<size_t> (field.firstNibble + field.nibbleCount), m_scene.hits.GetTurns().size() - 1)];
        const vector<int>   & own       = map->cells[cell].owners;
        bool                  isMine    = std::find (own.begin(), own.end(), m_scene.context.selectedFile) != own.end();
        uint32_t              roleColor = m_scene.context.palette.GetRoleColor (static_cast<MapRole> (map->cells[cell].role));

        count = g.GetSegments (a, b - a, parts);

        for (int k = 0; k < count; k++)
        {
            painter.FillRect (parts[k].x0, static_cast<float> (bar.bottom) - height * s_kFilesShare, parts[k].x1 - parts[k].x0, height * s_kFilesShare,
                              (m_scene.context.isFileHighlighted && !isMine) ? InspectorMarquee::Dim (m_scene.context.palette, roleColor) : roleColor);

            //  A dark line sets the band off from the nibbles above it.
            painter.FillRect (parts[k].x0, static_cast<float> (bar.bottom) - height * s_kFilesShare, parts[k].x1 - parts[k].x0, 1.0f, theme.Background());

            if (isMine && !m_scene.context.isFileHighlighted)
            {
                painter.OutlineRect (parts[k].x0, static_cast<float> (bar.top), parts[k].x1 - parts[k].x0, height, m_scene.scaler.ToPxf (StripPaintScene::kOutlineDip), theme.Accent());
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  StripOverlayPainter::DimOutsideFile
//
//  The bar shaded by half toward the background except under the selected
//  file's data fields (FR-090).
//
////////////////////////////////////////////////////////////////////////////////

void StripOverlayPainter::DimOutsideFile (IDxuiPainter & painter, const vector<StripSegment> & parts) const
{
    RECT      bar  = m_scene.bar;
    uint32_t  dim  = (m_scene.context.palette.background & 0x00FFFFFFu) | 0x80000000u;
    float     from = static_cast<float> (bar.left);



    for (const StripSegment & part : parts)
    {
        if (part.x0 > from)
        {
            painter.FillRect (from, static_cast<float> (bar.top), part.x0 - from, static_cast<float> (bar.bottom - bar.top), dim);
        }

        from = std::max (from, part.x1);
    }

    if (from < static_cast<float> (bar.right))
    {
        painter.FillRect (from, static_cast<float> (bar.top), static_cast<float> (bar.right) - from, static_cast<float> (bar.bottom - bar.top), dim);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  StripOverlayPainter::PaintMarquee
//
//  A marching marquee around each of the selected file's data fields, the
//  height of the bar, inset so the selected sector's outline over it does
//  not cover it.
//
////////////////////////////////////////////////////////////////////////////////

void StripOverlayPainter::PaintMarquee (IDxuiPainter & painter, const vector<StripSegment> & parts) const
{
    InspectorMarquee  marquee (m_scene.context.palette, m_scene.scaler.ToPxf (InspectorMarquee::kDashDip), InspectorMarquee::kThicknessPx,
                               static_cast<int64_t> (GetTickCount64()));
    RECT              bar    = m_scene.bar;
    float             inset  = std::ceil (m_scene.scaler.ToPxf (StripPaintScene::kOutlineDip)) + 0.5f;
    float             top    = static_cast<float> (bar.top) + inset;
    float             bottom = static_cast<float> (bar.bottom) - inset;
    float             s      = 0;
    float             left   = 0;
    float             right  = 0;



    for (const StripSegment & part : parts)
    {
        left  = std::floor (part.x0) + inset;
        right = std::floor (part.x1) - inset;

        s = marquee.DrawLine (painter, left,  top,    right, top,    0);
        s = marquee.DrawLine (painter, right, top,    right, bottom, s);
        s = marquee.DrawLine (painter, right, bottom, left,  bottom, s);
        marquee.DrawLine (painter, left, bottom, left, top, s);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  StripOverlayPainter::IsHeadOnTrack
//
//  True when the head is over the record of the selected quarter track, on
//  side A.
//
////////////////////////////////////////////////////////////////////////////////

bool StripOverlayPainter::IsHeadOnTrack (const InspectorViewContext & context, int side)
{
    const InspectorHead &  head     = context.head;
    int                    selected = context.model->GetQuarterTrack();
    bool                   isOn     = side == 0 && head.isShown && context.analysis != nullptr;



    isOn = isOn && head.quarterTrack >= 0 && head.quarterTrack < DiskImage::kQuarterTrackCount && selected >= 0 && selected < DiskImage::kQuarterTrackCount;

    return isOn && context.analysis->entries[head.quarterTrack].slot >= 0 &&
           context.analysis->entries[head.quarterTrack].slot == context.analysis->entries[selected].slot;
}





////////////////////////////////////////////////////////////////////////////////
//
//  StripOverlayPainter::GetHeadLabel
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR StripOverlayPainter::GetHeadLabel (HeadActivity activity)
{
    LPCWSTR  label = L"Motor off";



    switch (activity)
    {
        case HeadActivity::Reading:      label = L"Reading";       break;
        case HeadActivity::Writing:      label = L"Writing";       break;
        case HeadActivity::WriteBlocked: label = L"Write blocked"; break;
        case HeadActivity::Idle:         label = L"Motor off";     break;
    }

    return label;
}





////////////////////////////////////////////////////////////////////////////////
//
//  StripOverlayPainter::PaintHead
//
//  While the motor turns the selected track, the head's position as a line
//  across the bar, in the head marker's colors, with a trail behind it that
//  fades out over a fixed width (FR-066).
//
////////////////////////////////////////////////////////////////////////////////

void StripOverlayPainter::PaintHead (IDxuiPainter & painter, const StripGeometry & g) const
{
    const InspectorHead &        head   = m_scene.context.head;
    const DiskInspectorColors &  colors = m_scene.context.palette.colors;
    RECT                         bar    = m_scene.bar;
    float                        top    = static_cast<float> (bar.top);
    float                        height = static_cast<float> (bar.bottom - bar.top);
    float                        line   = m_scene.scaler.ToPxf (s_kHeadLineDip);
    float                        step   = m_scene.scaler.ToPxf (s_kTrailDip) / s_kTrailSteps;
    float                        x      = g.GetX (head.turn);
    float                        x0     = 0.0f;
    uint32_t                     argb   = (head.activity == HeadActivity::Reading) ? colors.headReading : colors.headWriting;
    uint32_t                     alpha  = 0;
    int                          k      = 0;



    if (!IsHeadOnTrack (m_scene.context, m_scene.side) || head.activity == HeadActivity::Idle || x < static_cast<float> (bar.left) || x > static_cast<float> (bar.right))
    {
        return;
    }

    //  The trail, oldest and faintest first.
    for (k = s_kTrailSteps; k > 0; k--)
    {
        x0    = std::max (x - step * k, static_cast<float> (bar.left));
        alpha = s_kTrailAlpha * (s_kTrailSteps - k + 1) / s_kTrailSteps;

        if (x0 < x - step * (k - 1))
        {
            painter.FillRect (x0, top, x - step * (k - 1) - x0, height, (argb & 0x00FFFFFFu) | (alpha << 24));
        }
    }

    painter.FillRect (x - line / 2.0f, top, line, height, argb);
}
