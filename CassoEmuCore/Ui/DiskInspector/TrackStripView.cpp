#include "Pch.h"

#include "Ui/DiskInspector/TrackStripView.h"
#include "Devices/Disk/Inspector/InspectorFormat.h"
#include "Devices/Disk/Inspector/TrackAnalyzer.h"
#include "Core/UnicodeSymbols.h"
#include "Ui/DiskInspector/FluxTiming.h"
#include "Ui/DiskInspector/InspectorMarquee.h"
#include "Ui/DiskInspector/InspectorText.h"
#include "Ui/DiskInspector/InspectorTip.h"
#include "Ui/DiskInspector/PlatterCells.h"
#include "Ui/DiskInspector/PlatterGeometry.h"





static constexpr int    s_kLabelRowDip     = 18;
static constexpr float  s_kOutlineDip      = 2.0f;
static constexpr float  s_kDividerMinDip   = 4.0f;
static constexpr float  s_kValuePadDip     = 3.0f;
static constexpr float  s_kLabelDip        = 11.0f;
static constexpr float  s_kCellsPerNibble   = 8.0f;
static constexpr float  s_kPadDip          = 6.0f;
static constexpr float  s_kCellDigitDip    = 9.0f;
static constexpr float  s_kCellTimingDip   = 30.0f;
static constexpr float  s_kValueShare      = 0.4f;
static constexpr float  s_kLineInsetDip    = 3.0f;
static constexpr float  s_kLineStepDip     = 2.0f;
static constexpr float  s_kLineDip         = 1.5f;
static constexpr float  s_kSeamRuleDip     = 3.0f;
static constexpr uint32_t  s_kBandAlpha    = 0xB0000000u;
static constexpr LPCWSTR   s_kpszSeamLabel = L"Write seam";
static constexpr float     s_kFilesShare     = 0.3f;
static constexpr float     s_kDiffBandDip    = 4.0f;
static constexpr float     s_kMinHitDip      = 4.0f;
static constexpr float     s_kHitLineDip     = 2.0f;
static constexpr float     s_kHeadLineDip    = 2.0f;
static constexpr uint32_t  s_kWaveShadeAlpha = 0x48000000u;
static constexpr float     s_kTrailDip       = 48.0f;
static constexpr int       s_kTrailSteps     = 12;
static constexpr uint32_t  s_kTrailAlpha     = 0x90u;





////////////////////////////////////////////////////////////////////////////////
//
//  TrackStripView::Paint
//
////////////////////////////////////////////////////////////////////////////////

void TrackStripView::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    const TrackAnalysis *  track = m_context.GetTrack();
    RECT                   bar   = GetBarRect();



    painter.FillRect (static_cast<float> (bar.left), static_cast<float> (bar.top), static_cast<float> (bar.right - bar.left), static_cast<float> (bar.bottom - bar.top),
                      m_context.palette.colors.nothingRecorded);

    if (track != nullptr && track->framed.cellCount > 0 && !track->framed.nibbles.empty())
    {
        PaintTrack (painter, text, theme, *track);
        PaintHead  (painter, MakeGeometry());
    }
    else if (m_context.hasDisk)
    {
        text.DrawString (L"Nothing recorded on this quarter track", static_cast<float> (bar.left), static_cast<float> (bar.top),
                         static_cast<float> (bar.right - bar.left), static_cast<float> (bar.bottom - bar.top), theme.ForegroundMuted(),
                         m_scaler.ToPxf (kTextDip), DxuiTheme::kBodyFace, DxuiTextHAlign::Center, DxuiTextVAlign::Center);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackStripView::PaintTrack
//
//  Each nibble in its kind's color, or in Timing mode on a flux track its
//  cells' mean timing, with its value and then its kind once they fit and a
//  divider before it once its value fits (FR-033, FR-035). Zoomed in far
//  enough, the cells, the timing line on a flux track, then the write seam
//  and the sector labels.
//
////////////////////////////////////////////////////////////////////////////////

void TrackStripView::PaintTrack (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const TrackAnalysis & track)
{
    const FramedTrack &          framed    = track.framed;
    StripGeometry                g         = MakeGeometry();
    RECT                         bar       = GetBarRect();
    std::array<StripSegment, 2>  parts     = {};
    float                        top       = static_cast<float> (bar.top);
    float                        height    = static_cast<float> (bar.bottom - bar.top);
    float                        textPx    = m_scaler.ToPxf (kSmallDip);
    bool                         isTimed   = IsTimed (track);
    bool                         showCells = GetCellPx (track) >= m_scaler.ToPxf (s_kCellDigitDip);
    float                        valueH    = showCells ? height * s_kValueShare : height;
    size_t                       i         = 0;
    int                          count     = 0;
    int                          k         = 0;
    uint32_t                     color     = 0;
    float                        w         = 0;
    float                        textW     = 0;
    float                        textH     = 0;
    float                        digitsW   = 0;
    std::wstring                 label;
    vector<StripSegment>         fileParts;



    UpdateTurns (track);

    //  A value shows wherever its two digits fit, and a divider wherever a
    //  nibble is wide enough to tell from its neighbors, so a run of
    //  nibbles of one kind does not read as one block.
    text.MeasureString (L"FF", textPx, DxuiTheme::kMonoFace, digitsW, textH);

    for (i = 0; i + 1 < m_turns.size(); i++)
    {
        count = g.GetSegments (m_turns[i], m_turns[i + 1] - m_turns[i], parts);
        color = m_context.palette.GetNibbleColor (track, static_cast<int> (i), m_context.isTimingMode, m_context.timingRange);

        for (k = 0; k < count; k++)
        {
            w = parts[k].x1 - parts[k].x0;
            painter.FillRect (parts[k].x0, top, std::max (w, 1.0f), height, color);

            if (w >= digitsW + m_scaler.ToPxf (s_kValuePadDip))
            {

                label = std::format (L"{:02X} ", framed.nibbles[i].value)
                      + InspectorText::FormatNibbleKind (track.nibbleKinds[i], i < track.isFailedChecksum.size() && track.isFailedChecksum[i] != 0);
                text.MeasureString (label.c_str(), textPx, DxuiTheme::kMonoFace, textW, textH);

                if (textW > w - m_scaler.ToPxf (s_kPadDip))
                {
                    label = std::format (L"{:02X}", framed.nibbles[i].value);
                }

                text.DrawString (label.c_str(), parts[k].x0, top, w, valueH, DiskInspectorPalette::GetTextColorOn (color), textPx, DxuiTheme::kMonoFace,
                                 DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
            }
        }
    }

    for (const RandomRegion & region : framed.randomRegions)
    {
        double  ra = TrackAnalyzer::GetAngle (track, region.startCell);
        double  rb = TrackAnalyzer::GetAngle (track, region.startCell + region.cellCount);

        count = g.GetSegments (ra, (rb > ra) ? rb - ra : rb + 1.0 - ra, parts);

        for (k = 0; k < count; k++)
        {
            painter.FillRect (parts[k].x0, top, std::max (parts[k].x1 - parts[k].x0, 1.0f), height, m_context.palette.GetKindColor (PlatterKind::RandomBits));
        }
    }

    if (showCells)
    {
        PaintCells (painter, text, theme, track, g);
    }

    //  Zoomed in to cells, the line takes the cell band's empty upper row,
    //  until each cell's own timing fills that row in its place.
    if (isTimed && !showCells)
    {
        PaintTimingLine (painter, theme, track, g, top, top + height);
    }
    else if (isTimed && GetCellPx (track) < m_scaler.ToPxf (s_kCellTimingDip))
    {
        PaintTimingLine (painter, theme, track, g, top + valueH, top + valueH + (height - valueH) / 2);
    }

    if (m_context.isFileHighlighted)
    {
        GetFileParts   (track, g, fileParts);
        DimOutsideFile (painter, fileParts);
    }

    PaintFiles     (painter, theme, track, g);
    PaintDividers  (painter, theme, g, top, height);

    if (m_context.isFileHighlighted)
    {
        PaintMarquee (painter, fileParts);
    }

    PaintDiffs     (painter, track, g);
    PaintHits      (painter, track, g);
    PaintSelection (painter, theme, g);
    PaintShimmer   (painter, g);
    PaintSeam      (painter, text, theme, track, g);
    PaintLabels    (painter, text, theme, track, g);

    //  Where each nibble's value fills the bar, the stretch labels move to
    //  the label row.
    PaintStretchLabels (text, theme, track, g, GetCellPx (track) * s_kCellsPerNibble >= digitsW + m_scaler.ToPxf (s_kValuePadDip));
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackStripView::PaintStretchLabels
//
//  Each stretch of one kind in view, labeled where the label fits: "Sync
//  20", "Address $0", "Data $0" (FR-035). Centered inside the stretch while
//  the bar has room; once nibble values fill the bar, in the label row at
//  the left of the stretch's visible part, clear of the sector number and
//  the write seam's label.
//
////////////////////////////////////////////////////////////////////////////////

void TrackStripView::PaintStretchLabels (IDxuiTextRenderer & text, const IDxuiTheme & theme, const TrackAnalysis & track, const StripGeometry & g, bool isInLabelRow)
{
    RECT                          bar           = GetBarRect();
    static constexpr PlatterKind  kKindColors[] = { PlatterKind::Sync, PlatterKind::AddressField, PlatterKind::DataField, PlatterKind::Other };



    std::array<StripSegment, 2>  parts  = {};
    float                        textPx = m_scaler.ToPxf (s_kLabelDip);
    float                        pad    = m_scaler.ToPxf (s_kPadDip);
    float                        rowTop = static_cast<float> (m_boundsDip.top);
    float                        rowH   = static_cast<float> (bar.top - m_boundsDip.top) - m_scaler.ToPxf (s_kSeamRuleDip);
    float                        textW  = 0;
    float                        textH  = 0;
    float                        clearW = 0;
    std::wstring                 label;



    if (m_sectionsOf != &track && m_context.analysis != nullptr)
    {
        m_sections   = NibbleSections::Build (*m_context.analysis, m_context.model->GetQuarterTrack());
        m_sectionsOf = &track;
    }

    for (const NibbleSection & section : m_sections)
    {
        int     end   = section.firstNibble + section.count;
        double  a     = m_turns[static_cast<size_t> (section.firstNibble)];
        double  b     = (end < static_cast<int> (m_turns.size())) ? m_turns[static_cast<size_t> (end)] : m_turns.back();
        int     count = g.GetSegments (a, b - a, parts);

        switch (section.kind)
        {
            case NibbleSectionKind::Sync:    label = std::format (L"Sync {}", section.count);                                          break;
            case NibbleSectionKind::Address: label = L"Address " + (section.sector >= 0 ? InspectorFormat::FormatSector (static_cast<Byte> (section.sector)) : std::wstring (L"?")); break;
            case NibbleSectionKind::Data:    label = L"Data " + (section.sector >= 0 ? InspectorFormat::FormatSector (static_cast<Byte> (section.sector)) : std::wstring (L"?"));    break;
            default:                         label = std::format (L"Other {}", section.count);                                         break;
        }

        text.MeasureString (label.c_str(), textPx, DxuiTheme::kBodyFace, textW, textH);

        for (int k = 0; k < count; k++)
        {
            float  x0 = std::max (parts[k].x0, static_cast<float> (bar.left));
            float  x1 = std::min (parts[k].x1, static_cast<float> (bar.right));

            if (isInLabelRow)
            {
                clearW = GetLabelRowClearance (text, track, g, section, parts[k], x0, x1);

                if (x1 - x0 >= clearW + textW + 2 * pad)
                {
                    text.DrawString (label.c_str(), x0 + clearW + pad, rowTop, x1 - x0 - clearW - pad, rowH, theme.ForegroundMuted(), textPx,
                                     DxuiTheme::kBodyFace, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
                }
            }
            else if (x1 - x0 >= textW + 2 * pad)
            {
                text.DrawString (label.c_str(), x0, static_cast<float> (bar.top), x1 - x0, static_cast<float> (bar.bottom - bar.top),
                                 DiskInspectorPalette::GetTextColorOn (m_context.palette.GetKindColor (kKindColors[static_cast<size_t> (section.kind)])), textPx,
                                 DxuiTheme::kBodyFace, DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::SemiBold, false);
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackStripView::GetLabelRowClearance
//
//  How far from the left of a stretch's visible part its label in the
//  label row starts: past the sector number over an address field whose
//  start is in view, and past the write seam's label over the seam.
//
////////////////////////////////////////////////////////////////////////////////

float TrackStripView::GetLabelRowClearance (IDxuiTextRenderer & text, const TrackAnalysis & track, const StripGeometry & g, const NibbleSection & section,
                                            const StripSegment & part, float x0, float x1) const
{
    std::array<StripSegment, 2>  seams  = {};
    int                          count  = GetSeamSegments (track, g, seams);
    float                        textPx = m_scaler.ToPxf (s_kLabelDip);
    float                        pad    = m_scaler.ToPxf (s_kPadDip);
    float                        clearW = 0;
    float                        textW  = 0;
    float                        textH  = 0;
    std::wstring                 mark;



    if (section.kind == NibbleSectionKind::Address && section.sectorIndex >= 0 && part.x0 >= x0)
    {
        const AnalyzedSector &  sector = track.sectors[static_cast<size_t> (section.sectorIndex)];

        mark = std::wstring (DiskInspectorPalette::GetStateSymbol (sector.state)) + L" " + InspectorFormat::FormatSector (sector.sector).substr (1);
        text.MeasureString (mark.c_str(), textPx, DxuiTheme::kBodyFace, textW, textH);
        clearW = std::max (clearW, part.x0 + textW - x0);
    }

    text.MeasureString (s_kpszSeamLabel, textPx, DxuiTheme::kBodyFace, textW, textH);

    for (int k = 0; k < count; k++)
    {
        if (seams[k].x1 > x0 && seams[k].x0 < x1 && seams[k].x1 - seams[k].x0 >= textW + pad)
        {
            clearW = std::max (clearW, (seams[k].x0 + seams[k].x1 + textW) / 2 - x0);
        }
    }

    return clearW;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackStripView::PaintCells
//
//  Under the values, a band with each cell in view: its 1 or 0 with a
//  pulse over each 1, and on a flux track its own timing once that fits
//  (FR-035).
//
////////////////////////////////////////////////////////////////////////////////

void TrackStripView::PaintCells (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const TrackAnalysis & track, const StripGeometry & g)
{
    const FramedTrack &          framed     = track.framed;
    RECT                         bar        = GetBarRect();
    std::array<StripSegment, 2>  parts      = {};
    float                        height     = static_cast<float> (bar.bottom - bar.top);
    float                        bandTop    = static_cast<float> (bar.top) + height * s_kValueShare;
    float                        bandH      = static_cast<float> (bar.bottom) - bandTop;
    float                        rowH       = bandH / 2;
    float                        textPx     = m_scaler.ToPxf (kSmallDip);
    bool                         showTiming = IsTimed (track) && GetCellPx (track) >= m_scaler.ToPxf (s_kCellTimingDip);
    uint32_t                     first      = FluxTiming::GetCellAt (m_cellTurns, g.GetTurn (static_cast<float> (bar.left)));
    uint32_t                     inView     = static_cast<uint32_t> (GetWidth() / std::max (GetCellPx (track), 1.0f)) + 2;
    uint32_t                     n          = 0;
    uint32_t                     cell       = 0;
    int                          count      = 0;
    int                          k          = 0;
    float                        cx         = 0;
    uint32_t                     band       = (theme.Background() & 0x00FFFFFFu) | s_kBandAlpha;



    painter.FillRect (static_cast<float> (bar.left), bandTop, GetWidth(), bandH, band);

    for (n = 0; n < inView && n < framed.cellCount; n++)
    {
        cell  = (first + n) % framed.cellCount;
        count = g.GetSegments (m_cellTurns[cell], m_cellTurns[cell + 1] - m_cellTurns[cell], parts);

        for (k = 0; k < count; k++)
        {
            cx = (parts[k].x0 + parts[k].x1) / 2;

            if (framed.cells[cell] != 0)
            {
                painter.DrawLine (cx, bandTop + m_scaler.ToPxf (2.0f), cx, bandTop + rowH, m_scaler.ToPxf (1.5f), theme.Foreground());
            }

            text.DrawString (framed.cells[cell] != 0 ? L"1" : L"0", parts[k].x0, bandTop + rowH, parts[k].x1 - parts[k].x0, rowH, theme.Foreground(), textPx,
                             DxuiTheme::kMonoFace, DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);

            if (showTiming)
            {
                text.DrawString (InspectorFormat::FormatPercent (FluxTiming::GetDeviation (framed.cellTicks[cell])).c_str(), parts[k].x0, bandTop,
                                 parts[k].x1 - parts[k].x0, rowH, theme.ForegroundMuted(), textPx, DxuiTheme::kBodyFace, DxuiTextHAlign::Center,
                                 DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackStripView::PaintTimingLine
//
//  On a flux track, the mean cell timing under each few pixels, nominal in
//  the middle of the band from top to bottom, slow above and fast below,
//  with the timing range filling it. In Timing mode it is a waveform over
//  the colored bands, the area below the line shaded (FR-033).
//
////////////////////////////////////////////////////////////////////////////////

void TrackStripView::PaintTimingLine (IDxuiPainter & painter, const IDxuiTheme & theme, const TrackAnalysis & track, const StripGeometry & g, float top, float bottom)
{
    RECT      bar    = GetBarRect();
    float     inset  = m_scaler.ToPxf (s_kLineInsetDip);
    float     mid    = (top + bottom) / 2.0f;
    float     half   = std::max ((bottom - top) / 2.0f - inset, 1.0f);
    float     step   = m_scaler.ToPxf (s_kLineStepDip);
    float     thick  = m_scaler.ToPxf (s_kLineDip);
    float     x      = static_cast<float> (bar.left);
    float     prevX  = 0;
    float     prevY  = 0;
    float     y      = 0;
    uint32_t  a      = 0;
    uint32_t  b      = 0;
    double    t      = 0;
    bool      isPrev = false;
    bool      isWave = m_context.isTimingMode;
    uint32_t  shade  = (theme.Foreground() & 0x00FFFFFFu) | s_kWaveShadeAlpha;



    painter.DrawLine (static_cast<float> (bar.left), mid, static_cast<float> (bar.right), mid, 1.0f, theme.Border());

    for (x = static_cast<float> (bar.left); x < bar.right; x += step)
    {
        a = FluxTiming::GetCellAt (m_cellTurns, g.GetTurn (x));
        b = FluxTiming::GetCellAt (m_cellTurns, g.GetTurn (std::min (x + step, static_cast<float> (bar.right))));
        t = std::clamp (FluxTiming::GetMeanDeviation (track, a, (b == a) ? a + 1 : b) / std::max (m_context.timingRange, 1e-6), -1.0, 1.0);
        y = mid - static_cast<float> (t) * half;

        if (isPrev && isWave)
        {
            painter.FillConvexQuad (prevX, prevY, x + step / 2, y, x + step / 2, bottom, prevX, bottom, shade);
        }

        if (isPrev)
        {
            painter.DrawLine (prevX, prevY, x + step / 2, y, isWave ? thick * 1.5f : thick, theme.Foreground());
        }

        prevX  = x + step / 2;
        prevY  = y;
        isPrev = true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackStripView::PaintSeam
//
//  The longest sync run, the likely write seam (FR-033): a rule along the
//  bottom of the label row, with its label over it where that fits.
//
////////////////////////////////////////////////////////////////////////////////

void TrackStripView::PaintSeam (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const TrackAnalysis & track, const StripGeometry & g)
{
    RECT                         bar    = GetBarRect();
    std::array<StripSegment, 2>  parts  = {};
    int                          count  = GetSeamSegments (track, g, parts);
    float                        rule   = m_scaler.ToPxf (s_kSeamRuleDip);
    float                        textPx = m_scaler.ToPxf (s_kLabelDip);
    float                        textW  = 0;
    float                        textH  = 0;
    int                          k      = 0;



    text.MeasureString (s_kpszSeamLabel, textPx, DxuiTheme::kBodyFace, textW, textH);

    for (k = 0; k < count; k++)
    {
        painter.FillRect (parts[k].x0, static_cast<float> (bar.top) - rule - 1.0f, std::max (parts[k].x1 - parts[k].x0, 1.0f), rule, theme.ForegroundMuted());

        if (parts[k].x1 - parts[k].x0 >= textW + m_scaler.ToPxf (s_kPadDip))
        {
            text.DrawString (s_kpszSeamLabel, parts[k].x0, static_cast<float> (m_boundsDip.top), parts[k].x1 - parts[k].x0,
                             static_cast<float> (bar.top - m_boundsDip.top) - rule, theme.ForegroundMuted(), textPx, DxuiTheme::kBodyFace,
                             DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackStripView::GetSeamSegments
//
////////////////////////////////////////////////////////////////////////////////

int TrackStripView::GetSeamSegments (const TrackAnalysis & track, const StripGeometry & g, std::array<StripSegment, 2> & outParts) const
{
    const SyncRun &  seam  = track.measurements.longestSync;
    size_t           n     = track.framed.nibbles.size();
    double           a     = 0;
    double           b     = 0;
    int              count = 0;



    //  A run's width in cells is each sync nibble's, so its extent comes from
    //  where its first nibble starts and where the nibble after it starts.
    if (seam.count > 0 && n > 0 && m_turns.size() == n + 1)
    {
        a     = m_turns[static_cast<size_t> (seam.firstNibble) % n];
        b     = m_turns[static_cast<size_t> (seam.firstNibble + seam.count) % n];
        count = g.GetSegments (a, b - a - std::floor (b - a), outParts);
    }

    return count;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackStripView::PaintLabels
//
//  Each sector's number over its address prologue, and the selected sector
//  outlined.
//
////////////////////////////////////////////////////////////////////////////////

void TrackStripView::PaintLabels (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const TrackAnalysis & track, const StripGeometry & g)
{
    RECT                         bar      = GetBarRect();
    std::array<StripSegment, 2>  parts    = {};
    int                          count    = 0;
    int                          k        = 0;
    size_t                       s        = 0;
    float                        rowTop   = static_cast<float> (m_boundsDip.top);
    float                        rowH     = static_cast<float> (bar.top - m_boundsDip.top);
    float                        thick    = m_scaler.ToPxf (s_kOutlineDip);
    double                       a        = 0;
    double                       b        = 0;
    std::wstring                 label;



    for (s = 0; s < track.sectors.size(); s++)
    {
        const AnalyzedSector &  sector = track.sectors[s];
        const LocatedField &    first  = track.fields[sector.addressField];
        const LocatedField &    last   = track.fields[sector.dataField >= 0 ? sector.dataField : sector.addressField];

        a     = m_turns[first.firstNibble];
        b     = (last.firstNibble + last.nibbleCount < static_cast<int> (m_turns.size())) ? m_turns[last.firstNibble + last.nibbleCount] : m_turns.back();
        label = std::wstring (DiskInspectorPalette::GetStateSymbol (sector.state)) + L" " + InspectorFormat::FormatSector (sector.sector).substr (1);

        count = g.GetSegments (a, 0.0001, parts);

        for (k = 0; k < count; k++)
        {
            text.DrawString (label.c_str(), parts[k].x0, rowTop, m_scaler.ToPxf (60), rowH, m_context.palette.GetStateColor (sector.state),
                             m_scaler.ToPxf (s_kLabelDip), DxuiTheme::kBodyFace, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::SemiBold, false);
        }

        if (static_cast<int> (s) == m_context.model->GetSectorIndex())
        {
            count = g.GetSegments (a, b - a - std::floor (b - a), parts);

            for (k = 0; k < count; k++)
            {
                painter.OutlineRect (parts[k].x0, static_cast<float> (bar.top), parts[k].x1 - parts[k].x0, static_cast<float> (bar.bottom - bar.top), thick, theme.Accent());
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackStripView::OnMouse
//
////////////////////////////////////////////////////////////////////////////////

bool TrackStripView::OnMouse (const DxuiMouseEvent & ev)
{
    const TrackAnalysis *  track     = m_context.GetTrack();
    POINT                  p         = ev.positionDip;
    bool                   isInside  = PtInRect (&m_boundsDip, p) != FALSE;
    bool                   isHandled = false;
    double                 span      = m_context.model->GetStripSpan();
    double                 fraction  = (p.x - m_boundsDip.left) / std::max (GetWidth(), 1.0f);
    double                 newSpan   = 0;
    int                    nibble    = -1;
    int64_t                now       = static_cast<int64_t> (GetTickCount64());
    bool                   isMoved   = false;



    switch (track != nullptr ? ev.kind : DxuiMouseEventKind::Leave)
    {
        case DxuiMouseEventKind::Wheel:
            if (isInside && !ev.wheelHorizontal && !ev.shift)
            {
                newSpan = std::clamp (span / std::pow (kWheelZoomStep, ev.wheelDelta), StripGeometry::kMinSpan, 1.0);
                m_context.model->SetStrip (StripGeometry::GetStartForZoom (m_context.model->GetStripStart(), span, newSpan, fraction), newSpan);
                isHandled = true;
            }
            else if (isInside)
            {
                m_context.model->SetStrip (m_context.model->GetStripStart() - ev.wheelDelta * span / 8.0, span);
                isHandled = true;
            }

            break;

        case DxuiMouseEventKind::Down:
            if (isInside && ev.button == DxuiMouseButton::Left)
            {
                m_isPressed    = true;
                m_isPanning    = false;
                m_isShiftPress = ev.shift;
                m_pressAt      = p;
                m_lastAt       = p;
                isHandled      = true;

                //  A plain press starts a selection at the nibble under it.
                if (!ev.shift)
                {
                    UpdateTurns (*track);
                    nibble        = GetNibbleAtX (*track, static_cast<float> (p.x));
                    m_isSelecting = nibble >= 0;

                    if (nibble >= 0)
                    {
                        m_context.model->SelectNibbles (m_context.model->GetQuarterTrack(), nibble, 1);
                    }
                }
            }

            break;

        case DxuiMouseEventKind::Move:
            isMoved = m_isPressed && std::abs (p.x - m_pressAt.x) > GetSystemMetrics (SM_CXDRAG) / 2;

            if (m_isSelecting && isMoved)
            {
                UpdateTurns (*track);
                nibble = GetNibbleAtX (*track, std::clamp (static_cast<float> (p.x), static_cast<float> (m_boundsDip.left), static_cast<float> (m_boundsDip.right - 1)));

                if (nibble >= 0)
                {
                    m_context.model->ExtendNibbles (nibble);
                }
            }

            m_isPanning = m_isPanning || (m_isShiftPress && isMoved);

            if (m_isPanning)
            {
                m_context.model->SetStrip (m_context.model->GetStripStart() - (p.x - m_lastAt.x) / std::max (GetWidth(), 1.0f) * span, span);
                m_lastAt = p;
            }

            isHandled = isInside || m_isPressed;
            break;

        case DxuiMouseEventKind::Up:
            isHandled = m_isPressed;
            isMoved   = std::abs (p.x - m_pressAt.x) > GetSystemMetrics (SM_CXDRAG) / 2;

            //  Shift and a click without a drag extends the selection to the
            //  nibble clicked.
            if (m_isPressed && m_isShiftPress && !m_isPanning && m_context.model->GetNibbleCount() > 0)
            {
                UpdateTurns (*track);
                nibble = GetNibbleAtX (*track, static_cast<float> (p.x));

                if (nibble >= 0)
                {
                    m_context.model->ExtendNibbles (nibble);
                }
            }

            if (m_isPressed && !m_isShiftPress && !isMoved && now - m_lastClickMs <= static_cast<int64_t> (GetDoubleClickTime()))
            {
                ShowWholeTrack();
                m_lastClickMs = 0;
            }
            else if (m_isPressed && !m_isShiftPress && !isMoved)
            {
                m_lastClickMs = now;
            }

            if (m_isPressed && !m_isPanning)
            {
                NotifySelection();
            }

            m_isPressed    = false;
            m_isPanning    = false;
            m_isSelecting  = false;
            m_isShiftPress = false;
            break;
        default:
            break;
    }

    return isHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackStripView::GetTooltip
//
////////////////////////////////////////////////////////////////////////////////

bool TrackStripView::GetTooltip (POINT pointPx, std::wstring & outText, RECT & outAnchorPx) const
{
    const TrackAnalysis *  track  = m_context.GetTrack();
    RECT                   bar    = GetBarRect();
    InspectorTip           tip;
    bool                   hasTip = BuildTip (pointPx, tip);



    if (hasTip)
    {
        outText     = tip.GetText();
        outAnchorPx = { pointPx.x, bar.top, pointPx.x + 1, bar.bottom };
    }
    else if (track != nullptr && !m_isPanning && pointPx.y >= m_boundsDip.top && pointPx.y < bar.top && IsOverSeam (*track, static_cast<float> (pointPx.x)))
    {
        outText     = std::format (L"Longest sync run, {} nibbles at cell {}\nThe likely write seam", track->measurements.longestSync.count,
                                   InspectorFormat::FormatCount (track->measurements.longestSync.startCell));
        outAnchorPx = { pointPx.x, m_boundsDip.top, pointPx.x + 1, bar.top };
        hasTip      = true;
    }

    return hasTip;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackStripView::GetTooltipSwatches
//
////////////////////////////////////////////////////////////////////////////////

bool TrackStripView::GetTooltipSwatches (POINT pointPx, vector<DxuiTooltipSwatch> & outSwatches) const
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
//  TrackStripView::BuildTip
//
//  The tip for the nibble under the pointer, with the colors of the cells
//  its pixel covers.
//
////////////////////////////////////////////////////////////////////////////////

bool TrackStripView::BuildTip (POINT pointPx, InspectorTip & outTip) const
{
    const TrackAnalysis *  track  = m_context.GetTrack();
    RECT                   bar    = GetBarRect();
    int                    nibble = -1;
    uint32_t               first  = 0;
    uint32_t               next   = 0;



    if (track != nullptr && !m_isPanning && PtInRect (&bar, pointPx))
    {
        UpdateTurns (*track);
        nibble = GetNibbleAtX (*track, static_cast<float> (pointPx.x));
    }

    if (nibble >= 0)
    {
        first  = static_cast<uint32_t> (GetCellAtX (*track, static_cast<float> (pointPx.x)));
        next   = static_cast<uint32_t> (GetCellAtX (*track, static_cast<float> (pointPx.x + 1)));
        outTip = InspectorTip::BuildNibbleTip (m_context, *track, nibble, first, (next + track->framed.cellCount - first) % track->framed.cellCount,
                                               m_context.isTimingMode);
    }

    return nibble >= 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackStripView::UpdateTurns
//
////////////////////////////////////////////////////////////////////////////////

void TrackStripView::UpdateTurns (const TrackAnalysis & track) const
{
    if (m_turnsOf != &track)
    {
        StripGeometry::BuildNibbleTurns (track, m_turns);
        FluxTiming::BuildCellTurns (track, m_cellTurns);
        m_turnsOf = &track;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackStripView::MakeGeometry
//
////////////////////////////////////////////////////////////////////////////////

StripGeometry TrackStripView::MakeGeometry() const
{
    return StripGeometry (m_context.model->GetStripStart(), m_context.model->GetStripSpan(), static_cast<float> (m_boundsDip.left), GetWidth());
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackStripView::GetBarRect
//
//  The colored bar under the row of sector labels.
//
////////////////////////////////////////////////////////////////////////////////

RECT TrackStripView::GetBarRect() const
{
    RECT  bar = m_boundsDip;



    bar.top = std::min (bar.bottom, bar.top + m_scaler.ToPx (s_kLabelRowDip));

    return bar;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackStripView::GetNibbleAtX
//
////////////////////////////////////////////////////////////////////////////////

int TrackStripView::GetNibbleAtX (const TrackAnalysis & track, float xPx) const
{
    double  turn   = MakeGeometry().GetTurn (xPx);
    int     nibble = -1;



    if (m_turns.size() > 1)
    {
        //  The turns rise from the first nibble's, which can be just before
        //  the index, so the turn is brought into the same lap.
        turn -= std::floor (turn);
        turn += (turn < m_turns.front()) ? 1.0 : 0.0;

        auto  after = std::upper_bound (m_turns.begin(), m_turns.end() - 1, turn);

        nibble = (after == m_turns.begin()) ? static_cast<int> (m_turns.size()) - 2 : static_cast<int> (after - m_turns.begin()) - 1;
    }

    (void) track;

    return nibble;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackStripView::GetCellAtX
//
////////////////////////////////////////////////////////////////////////////////

int TrackStripView::GetCellAtX (const TrackAnalysis & track, float xPx) const
{
    double  turn = MakeGeometry().GetTurn (xPx);



    turn -= std::floor (turn);

    return static_cast<int> (PlatterGeometry::GetCellAtTurn (track, turn));
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackStripView::IsTimed
//
////////////////////////////////////////////////////////////////////////////////

bool TrackStripView::IsTimed (const TrackAnalysis & track)
{
    return track.framed.isFlux && track.framed.turnTicks > 0 && track.framed.cellTicks.size() >= track.framed.cellCount;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackStripView::GetCellPx
//
//  How wide a cell of the mean length is at the strip's zoom.
//
////////////////////////////////////////////////////////////////////////////////

float TrackStripView::GetCellPx (const TrackAnalysis & track) const
{
    return GetWidth() / static_cast<float> (m_context.model->GetStripSpan() * std::max<uint32_t> (track.framed.cellCount, 1));
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackStripView::IsOverSeam
//
////////////////////////////////////////////////////////////////////////////////

bool TrackStripView::IsOverSeam (const TrackAnalysis & track, float xPx) const
{
    std::array<StripSegment, 2>  parts  = {};
    int                          count  = GetSeamSegments (track, MakeGeometry(), parts);
    int                          k      = 0;
    bool                         isOver = false;



    for (k = 0; k < count; k++)
    {
        isOver = isOver || (xPx >= parts[k].x0 && xPx < parts[k].x1);
    }

    return isOver;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackStripView::ZoomAboutCenter
//
//  For the strip's buttons and keys; the zoom keeps the middle of the view
//  where it is.
//
////////////////////////////////////////////////////////////////////////////////

void TrackStripView::ZoomAboutCenter (double factor)
{
    double  span    = m_context.model->GetStripSpan();
    double  newSpan = std::clamp (span / factor, StripGeometry::kMinSpan, 1.0);



    m_context.model->SetStrip (StripGeometry::GetStartForZoom (m_context.model->GetStripStart(), span, newSpan, 0.5), newSpan);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackStripView::PanBy
//
////////////////////////////////////////////////////////////////////////////////

void TrackStripView::PanBy (double fractionOfView)
{
    double  span = m_context.model->GetStripSpan();



    m_context.model->SetStrip (m_context.model->GetStripStart() + fractionOfView * span, span);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackStripView::ShowWholeTrack
//
////////////////////////////////////////////////////////////////////////////////

void TrackStripView::ShowWholeTrack()
{
    m_context.model->SetStrip (0.0, 1.0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackStripView::GetReadout
//
////////////////////////////////////////////////////////////////////////////////

std::wstring TrackStripView::GetReadout() const
{
    const TrackAnalysis *  track = m_context.GetTrack();
    double                 start = m_context.model->GetStripStart();
    double                 span  = m_context.model->GetStripSpan();
    uint32_t               first = 0;
    uint32_t               last  = 0;



    if (track != nullptr && track->framed.cellCount > 0)
    {
        UpdateTurns (*track);
        first = (span >= 1.0) ? 0 : FluxTiming::GetCellAt (m_cellTurns, start);
        last  = (span >= 1.0) ? track->framed.cellCount - 1 : FluxTiming::GetCellAt (m_cellTurns, start + span);
    }

    return (track != nullptr && track->framed.cellCount > 0)
         ? InspectorText::FormatStripReadout (span, first, last, IsHeadOnTrack() ? GetHeadLabel (m_context.head.activity) : L"")
         : std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackStripView::PaintDiffs
//
//  While comparing, a band along the bottom of the bar under each run of
//  this side's nibbles that differs from the other side's (FR-121).
//
////////////////////////////////////////////////////////////////////////////////

void TrackStripView::PaintDiffs (IDxuiPainter & painter, const TrackAnalysis & track, const StripGeometry & g)
{
    RECT                         bar    = GetBarRect();
    float                        h      = m_scaler.ToPxf (s_kDiffBandDip);
    int                          size   = static_cast<int> (track.framed.nibbles.size());
    std::array<StripSegment, 2>  parts  = {};
    int                          count  = 0;
    int                          first  = 0;
    int                          length = 0;
    double                       a      = 0;
    double                       b      = 0;



    for (const Difference & d : (m_context.nibbleDiffs != nullptr && size > 0) ? *m_context.nibbleDiffs : vector<Difference>())
    {
        first  = (m_side == 0) ? d.firstNibbleA : d.firstNibbleB;
        length = std::max ((m_side == 0) ? d.nibbleCountA : d.nibbleCountB, 1);

        if (first < 0)
        {
            continue;
        }

        a     = m_turns[static_cast<size_t> (first % size)];
        b     = m_turns[static_cast<size_t> ((first + length) % size)];
        count = g.GetSegments (a, (b > a) ? b - a : b + 1.0 - a, parts);

        for (int k = 0; k < count; k++)
        {
            painter.FillRect (parts[k].x0, static_cast<float> (bar.bottom) - h, std::max (parts[k].x1 - parts[k].x0, 1.0f), h, m_context.palette.colors.difference);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackStripView::PaintHits
//
//  Each run of nibbles with find hits (FR-056) outlined across the bar in
//  the hit color, framed in a dark line so it reads apart from the gold of
//  the address marks it often contains, and at least a few pixels wide so
//  a hit is visible at any zoom.
//
////////////////////////////////////////////////////////////////////////////////

void TrackStripView::PaintHits (IDxuiPainter & painter, const TrackAnalysis & track, const StripGeometry & g)
{
    const FindMarks::TrackMarks *  marks  = (m_context.findMarks != nullptr) ? m_context.findMarks->GetTrack (&track) : nullptr;
    RECT                           bar    = GetBarRect();
    float                          top    = static_cast<float> (bar.top);
    float                          h      = static_cast<float> (bar.bottom - bar.top);
    float                          line   = m_scaler.ToPxf (s_kHitLineDip);
    float                          edge   = m_scaler.ToPxf (1.0f);
    float                          minW   = m_scaler.ToPxf (s_kMinHitDip);
    uint32_t                       fill   = m_context.palette.colors.findHit;
    int                            size   = static_cast<int> (track.framed.nibbles.size());
    std::array<StripSegment, 2>    parts  = {};
    int                            count  = 0;
    double                         a      = 0;
    double                         b      = 0;
    float                          w      = 0;



    for (const FindMarks::Run & run : (marks != nullptr && size > 0) ? marks->nibbles : vector<FindMarks::Run>())
    {
        a     = m_turns[static_cast<size_t> (run.first % size)];
        b     = m_turns[static_cast<size_t> ((run.first + run.count) % size)];
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
//  TrackStripView::PaintSelection
//
//  A run of selected nibbles outlined across the bar (FR-043). An outline,
//  not a shade, so each nibble keeps the color its kind gives it.
//
////////////////////////////////////////////////////////////////////////////////

void TrackStripView::PaintSelection (IDxuiPainter & painter, const IDxuiTheme & theme, const StripGeometry & g)
{
    RECT                         bar   = GetBarRect();
    std::array<StripSegment, 2>  parts = {};
    int                          first = m_context.model->GetFirstNibble();
    int                          end   = first + m_context.model->GetNibbleCount();
    int                          count = 0;
    int                          k     = 0;



    if (first >= 0 && end > first && end < static_cast<int> (m_turns.size()))
    {
        count = g.GetSegments (m_turns[first], m_turns[end] - m_turns[first], parts);
    }

    for (k = 0; k < count; k++)
    {
        painter.OutlineRect (parts[k].x0, static_cast<float> (bar.top), std::max (parts[k].x1 - parts[k].x0, 1.0f), static_cast<float> (bar.bottom - bar.top),
                             m_scaler.ToPxf (s_kOutlineDip), theme.Accent());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackStripView::PaintShimmer
//
//  The selection lit by the window's shimmer (FR-038), and the nibble under
//  the pointer lit faintly and outlined, so the tooltip's subject is plain
//  (FR-148).
//
////////////////////////////////////////////////////////////////////////////////

void TrackStripView::PaintShimmer (IDxuiPainter & painter, const StripGeometry & g) const
{
    double                       now    = InspectorMarquee::GetClockMs();
    RECT                         bar    = GetBarRect();
    std::array<StripSegment, 2>  parts  = {};
    double                       end    = 0;
    double                       start  = m_context.model->GetSelectionTurn (end);
    float                        height = static_cast<float> (bar.bottom - bar.top);
    int                          count  = 0;
    int                          nibble = -1;
    RECT                         box    = { bar.right, bar.top, bar.left, bar.bottom };



    if (start >= 0)
    {
        end   = (end <= start) ? end + 1.0 : end;
        count = g.GetSegments (start, end - start, parts);
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

    if (PtInRect (&bar, m_context.hoverPx) && !m_isPanning && m_context.GetTrack() != nullptr)
    {
        nibble = GetNibbleAtX (*m_context.GetTrack(), static_cast<float> (m_context.hoverPx.x));
    }

    count = (nibble >= 0 && nibble + 1 < static_cast<int> (m_turns.size())) ? g.GetSegments (m_turns[nibble], m_turns[nibble + 1] - m_turns[nibble], parts) : 0;

    for (int k = 0; k < count; k++)
    {
        painter.FillRect    (parts[k].x0, static_cast<float> (bar.top), std::max (parts[k].x1 - parts[k].x0, 2.0f), height, InspectorMarquee::GetHoverFill());
        painter.OutlineRect (parts[k].x0, static_cast<float> (bar.top), std::max (parts[k].x1 - parts[k].x0, 2.0f), height, 1.0f, InspectorMarquee::GetHoverLine());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackStripView::PaintDividers
//
//  A divider before each nibble wide enough to tell from its neighbors,
//  drawn after the Files band so it runs the bar's full height.
//
////////////////////////////////////////////////////////////////////////////////

void TrackStripView::PaintDividers (IDxuiPainter & painter, const IDxuiTheme & theme, const StripGeometry & g, float top, float height)
{
    std::array<StripSegment, 2>  parts = {};
    int                          count = 0;



    for (size_t i = 0; i + 1 < m_turns.size(); i++)
    {
        count = g.GetSegments (m_turns[i], m_turns[i + 1] - m_turns[i], parts);

        for (int k = 0; k < count; k++)
        {
            if (parts[k].x1 - parts[k].x0 >= m_scaler.ToPxf (s_kDividerMinDip))
            {
                painter.FillRect (parts[k].x0, top, 1.0f, height, theme.Background());
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackStripView::PaintFiles
//
//  With the "Files" overlay on a whole track of a mapped volume, each data
//  field's role as a band along the bottom of the bar, and the selected
//  file's fields outlined (FR-090).
//
////////////////////////////////////////////////////////////////////////////////

void TrackStripView::PaintFiles (IDxuiPainter & painter, const IDxuiTheme & theme, const TrackAnalysis & track, const StripGeometry & g)
{
    const FileMap *              map    = m_context.fileMap;
    int                          qt     = m_context.model->GetQuarterTrack();
    RECT                         bar    = GetBarRect();
    float                        height = static_cast<float> (bar.bottom - bar.top);
    std::array<StripSegment, 2>  parts  = {};
    int                          count  = 0;
    int                          cell   = -1;
    int                          half   = 0;
    bool                         isOn   = m_context.isFilesOverlay && map != nullptr && map->notMapped == NotMappedReason::None &&
                                          qt % DiskImage::kQuarterTracksPerWholeTrack == 0;



    for (const AnalyzedSector & sector : isOn ? track.sectors : vector<AnalyzedSector>())
    {
        cell = (sector.dataField >= 0 && map != nullptr) ? map->GetCellOf (qt / DiskImage::kQuarterTracksPerWholeTrack, sector.sector, half) : -1;

        if (cell < 0)
        {
            continue;
        }

        const LocatedField  & field     = track.fields[sector.dataField];
        double                a         = m_turns[field.firstNibble];
        double                b         = m_turns[std::min<size_t> (static_cast<size_t> (field.firstNibble + field.nibbleCount), m_turns.size() - 1)];
        const vector<int>   & own       = map->cells[cell].owners;
        bool                  isMine    = std::find (own.begin(), own.end(), m_context.selectedFile) != own.end();
        uint32_t              roleColor = m_context.palette.GetRoleColor (static_cast<MapRole> (map->cells[cell].role));

        count = g.GetSegments (a, b - a, parts);

        for (int k = 0; k < count; k++)
        {
            painter.FillRect (parts[k].x0, static_cast<float> (bar.bottom) - height * s_kFilesShare, parts[k].x1 - parts[k].x0, height * s_kFilesShare,
                              (m_context.isFileHighlighted && !isMine) ? InspectorMarquee::Dim (m_context.palette, roleColor) : roleColor);

            //  A dark line sets the band off from the nibbles above it.
            painter.FillRect (parts[k].x0, static_cast<float> (bar.bottom) - height * s_kFilesShare, parts[k].x1 - parts[k].x0, 1.0f, theme.Background());

            if (isMine && !m_context.isFileHighlighted)
            {
                painter.OutlineRect (parts[k].x0, static_cast<float> (bar.top), parts[k].x1 - parts[k].x0, height, m_scaler.ToPxf (s_kOutlineDip), theme.Accent());
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackStripView::GetFileParts
//
//  Where the selected file's sectors lie across the bar, a run of them as
//  one part, on a whole track, in order from left to right.
//
////////////////////////////////////////////////////////////////////////////////

void TrackStripView::GetFileParts (const TrackAnalysis & track, const StripGeometry & g, vector<StripSegment> & outParts) const
{
    const FileMap *              map   = m_context.fileMap;
    int                          qt    = m_context.model->GetQuarterTrack();
    int                          file  = m_context.selectedFile;
    std::array<StripSegment, 2>  parts = {};
    int                          count = 0;
    bool                         isOn  = map != nullptr && map->notMapped == NotMappedReason::None && qt % DiskImage::kQuarterTracksPerWholeTrack == 0 && file >= 0;



    for (const InspectorMarquee::Span & span : isOn ? InspectorMarquee::GetFileSpans (track, m_turns, *map, qt / DiskImage::kQuarterTracksPerWholeTrack, file)
                                                    : vector<InspectorMarquee::Span>())
    {
        count = g.GetSegments (span.a, span.b - span.a, parts);
        outParts.insert (outParts.end(), parts.begin(), parts.begin() + count);
    }

    std::sort (outParts.begin(), outParts.end(), [] (const StripSegment & x, const StripSegment & y) { return x.x0 < y.x0; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackStripView::DimOutsideFile
//
//  The bar shaded by half toward the background except under the selected
//  file's data fields (FR-090).
//
////////////////////////////////////////////////////////////////////////////////

void TrackStripView::DimOutsideFile (IDxuiPainter & painter, const vector<StripSegment> & parts) const
{
    RECT      bar  = GetBarRect();
    uint32_t  dim  = (m_context.palette.background & 0x00FFFFFFu) | 0x80000000u;
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
//  TrackStripView::PaintMarquee
//
//  A marching marquee around each of the selected file's data fields, the
//  height of the bar, inset so the selected sector's outline over it does
//  not cover it.
//
////////////////////////////////////////////////////////////////////////////////

void TrackStripView::PaintMarquee (IDxuiPainter & painter, const vector<StripSegment> & parts) const
{
    InspectorMarquee  marquee (m_context.palette, m_scaler.ToPxf (InspectorMarquee::kDashDip), InspectorMarquee::kThicknessPx,
                               static_cast<int64_t> (GetTickCount64()));
    RECT              bar    = GetBarRect();
    float             inset  = std::ceil (m_scaler.ToPxf (s_kOutlineDip)) + 0.5f;
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
//  TrackStripView::IsHeadOnTrack
//
//  True when the head is over the record of the selected quarter track, on
//  side A.
//
////////////////////////////////////////////////////////////////////////////////

bool TrackStripView::IsHeadOnTrack() const
{
    const InspectorHead &  head     = m_context.head;
    int                    selected = m_context.model->GetQuarterTrack();
    bool                   isOn     = m_side == 0 && head.isShown && m_context.analysis != nullptr;



    isOn = isOn && head.quarterTrack >= 0 && head.quarterTrack < DiskImage::kQuarterTrackCount && selected >= 0 && selected < DiskImage::kQuarterTrackCount;

    return isOn && m_context.analysis->entries[head.quarterTrack].slot >= 0 &&
           m_context.analysis->entries[head.quarterTrack].slot == m_context.analysis->entries[selected].slot;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackStripView::GetHeadLabel
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR TrackStripView::GetHeadLabel (HeadActivity activity)
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
//  TrackStripView::PaintHead
//
//  While the motor turns the selected track, the head's position as a line
//  across the bar, in the head marker's colors, with a trail behind it that
//  fades out over a fixed width (FR-066).
//
////////////////////////////////////////////////////////////////////////////////

void TrackStripView::PaintHead (IDxuiPainter & painter, const StripGeometry & g) const
{
    const InspectorHead &        head   = m_context.head;
    const DiskInspectorColors &  colors = m_context.palette.colors;
    RECT                         bar    = GetBarRect();
    float                        top    = static_cast<float> (bar.top);
    float                        height = static_cast<float> (bar.bottom - bar.top);
    float                        line   = m_scaler.ToPxf (s_kHeadLineDip);
    float                        step   = m_scaler.ToPxf (s_kTrailDip) / s_kTrailSteps;
    float                        x      = g.GetX (head.turn);
    float                        x0     = 0.0f;
    uint32_t                     argb   = (head.activity == HeadActivity::Reading) ? colors.headReading : colors.headWriting;
    uint32_t                     alpha  = 0;
    int                          k      = 0;



    if (!IsHeadOnTrack() || head.activity == HeadActivity::Idle || x < static_cast<float> (bar.left) || x > static_cast<float> (bar.right))
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
