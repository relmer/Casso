#include "Pch.h"

#include "Devices/Disk/Inspector/InspectorFormat.h"
#include "Devices/Disk/Inspector/TrackAnalyzer.h"
#include "Core/UnicodeSymbols.h"
#include "Ui/DiskInspector/FluxTiming.h"
#include "Ui/DiskInspector/InspectorText.h"
#include "Ui/DiskInspector/StripTrackPainter.h"
#include "Ui/DiskInspector/OrphanFields.h"





static constexpr float     s_kLabelDip          = 11.0f;
static constexpr float     s_kPadDip            = 6.0f;
static constexpr float     s_kValueShare        = 0.4f;
static constexpr float     s_kLineInsetDip      = 3.0f;
static constexpr float     s_kLineStepDip       = 2.0f;
static constexpr float     s_kLineDip           = 1.5f;
static constexpr float     s_kSeamRuleDip       = 3.0f;
static constexpr float     s_kBracketDip        = 1.0f;
static constexpr float     s_kBracketGapDip     = 3.0f;
static constexpr float     s_kTickShare         = 0.45f;
static constexpr double    s_kTickTurn          = 0.0001;
static constexpr uint32_t  s_kBandAlpha         = 0xB0000000u;
static constexpr uint32_t  s_kPercentPatchAlpha = 0xD0000000u;
static constexpr LPCWSTR   s_kpszSeamLabel      = L"Write seam";
static constexpr uint32_t  s_kWaveShadeAlpha    = 0x48000000u;





////////////////////////////////////////////////////////////////////////////////
//
//  StripTrackPainter::PaintNibbles
//
//  Each nibble in its kind's color, or in Timing mode on a flux track its
//  cells' mean timing, with its value and then its kind once they fit
//  (FR-033, FR-035). Zoomed in far enough, the cells, and the timing line
//  on a flux track. Returns the width of a value's two digits.
//
////////////////////////////////////////////////////////////////////////////////

float StripTrackPainter::PaintNibbles (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const TrackAnalysis & track, const StripGeometry & g) const
{
    const FramedTrack &          framed    = track.framed;
    RECT                         bar       = m_scene.bar;
    std::array<StripSegment, 2>  parts     = {};
    float                        top       = static_cast<float> (bar.top);
    float                        height    = static_cast<float> (bar.bottom - bar.top);
    float                        textPx    = m_scene.scaler.ToPxf (InspectorView::kSmallDip);
    bool                         isTimed   = StripHitTest::IsTimed (track);
    bool                         showCells = StripHitTest::GetCellPx (track, m_scene.context.model->GetStripSpan(), m_scene.GetWidth()) >= m_scene.scaler.ToPxf (StripHitTest::kCellDigitDip);
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



    struct StripValue
    {
        std::wstring  label;
        float         x     = 0.0f;
        float         width = 0.0f;
        uint32_t      color = 0;
    };

    vector<StripValue>  values;

    //  A value shows wherever its two digits fit, and a divider wherever a
    //  nibble is wide enough to tell from its neighbors, so a run of
    //  nibbles of one kind does not read as one block.
    text.MeasureString (L"FF", textPx, DxuiTheme::kMonoFace, digitsW, textH);

    for (i = 0; i + 1 < m_scene.hits.GetTurns().size(); i++)
    {
        count = g.GetSegments (m_scene.hits.GetTurns()[i], m_scene.hits.GetTurns()[i + 1] - m_scene.hits.GetTurns()[i], parts);
        color = m_scene.context.palette.GetNibbleColor (track, static_cast<int> (i), m_scene.context.isTimingMode, m_scene.context.timingRange);

        for (k = 0; k < count; k++)
        {
            w = parts[k].x1 - parts[k].x0;
            painter.FillRect (parts[k].x0, top, std::max (w, 1.0f), height, color);

            //  A nibble's width varies by up to a pixel with where it falls, and
            //  the hex zoom stop is exactly this width, so a pixel of slack
            //  puts every nibble's value on screen at that stop.
            if (w + 1.0f >= digitsW + m_scene.scaler.ToPxf (StripHitTest::kValuePadDip))
            {
                label = GetNibbleKindLabel (track.nibbleKinds[i]) + std::format (L": {:02X}", framed.nibbles[i].value)
                      + (i < track.isFailedChecksum.size() && track.isFailedChecksum[i] != 0 ? L", checksum failed" : L"");
                text.MeasureString (label.c_str(), textPx, DxuiTheme::kMonoFace, textW, textH);

                if (textW > w - m_scene.scaler.ToPxf (s_kPadDip))
                {
                    label = std::format (L"{:02X}", framed.nibbles[i].value);
                }

                values.push_back ({ label, parts[k].x0, w, color });
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
            painter.FillRect (parts[k].x0, top, std::max (parts[k].x1 - parts[k].x0, 1.0f), height, m_scene.context.palette.GetKindColor (PlatterKind::RandomBits));
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
    else if (isTimed && StripHitTest::GetCellPx (track, m_scene.context.model->GetStripSpan(), m_scene.GetWidth()) < m_scene.scaler.ToPxf (StripHitTest::kCellTimingDip))
    {
        PaintTimingLine (painter, theme, track, g, top + valueH, top + valueH + (height - valueH) / 2);
    }

    //  Each value on a patch of its nibble's color, so the timing line can't
    //  run through its digits.
    for (const StripValue & value : values)
    {
        text.MeasureString (value.label.c_str(), textPx, DxuiTheme::kMonoFace, textW, textH);
        painter.FillRect (value.x + (value.width - textW) / 2, top + (valueH - textH) / 2, textW, textH, value.color);
        text.DrawString (value.label.c_str(), value.x, top, value.width, valueH, DiskInspectorPalette::GetTextColorOn (value.color), textPx, DxuiTheme::kMonoFace,
                         DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    }

    return digitsW;
}





////////////////////////////////////////////////////////////////////////////////
//
//  StripTrackPainter::PaintStretchLabels
//
//  Each stretch of one kind in view, labeled where the label fits: "Sync
//  20", "Address", "Data" (FR-035). Centered inside the stretch while the
//  bar has room; once nibble values fill the bar, in the label row at the
//  left of the stretch's visible part, clear of the sector label and the
//  write seam's label.
//
////////////////////////////////////////////////////////////////////////////////

void StripTrackPainter::PaintStretchLabels (IDxuiTextRenderer & text, const IDxuiTheme & theme, const TrackAnalysis & track, const StripGeometry & g, bool isInLabelRow) const
{
    RECT                          bar           = m_scene.bar;
    static constexpr PlatterKind  kKindColors[] = { PlatterKind::Sync, PlatterKind::AddressField, PlatterKind::DataField, PlatterKind::Other };



    std::array<StripSegment, 2>  parts  = {};
    float                        textPx = m_scene.scaler.ToPxf (s_kLabelDip);
    float                        pad    = m_scene.scaler.ToPxf (s_kPadDip);
    float                        rowTop = static_cast<float> (m_scene.bounds.top);
    float                        rowH   = static_cast<float> (bar.top - m_scene.bounds.top) - m_scene.scaler.ToPxf (s_kSeamRuleDip);
    float                        textW  = 0;
    float                        textH  = 0;
    float                        clearW = 0;
    std::wstring                 label;



    for (const NibbleSection & section : m_scene.hits.GetSections())
    {
        int     end   = section.firstNibble + section.count;
        double  a     = m_scene.hits.GetTurns()[static_cast<size_t> (section.firstNibble)];
        double  b     = (end < static_cast<int> (m_scene.hits.GetTurns().size())) ? m_scene.hits.GetTurns()[static_cast<size_t> (end)] : m_scene.hits.GetTurns().back();
        int     count = g.GetSegments (a, b - a, parts);

        switch (section.kind)
        {
            case NibbleSectionKind::Sync:    label = std::format (L"Sync {}", section.count);  break;
            case NibbleSectionKind::Address: label = L"Address";                               break;
            case NibbleSectionKind::Data:    label = L"Data";                                  break;
            default:                         label = std::format (L"Other {}", section.count); break;
        }

        text.MeasureString (label.c_str(), textPx, DxuiTheme::kBodyFace, textW, textH);

        for (int k = 0; k < count; k++)
        {
            float  x0 = std::max (parts[k].x0, static_cast<float> (bar.left));
            float  x1 = std::min (parts[k].x1, static_cast<float> (bar.right));

            if (isInLabelRow)
            {
                clearW = GetLabelRowClearance (text, track, g, section, x0, x1);

                if (x1 - x0 >= clearW + textW + 2 * pad)
                {
                    text.DrawString (label.c_str(), x0 + clearW + pad, rowTop, x1 - x0 - clearW - pad, rowH, theme.ForegroundMuted(), textPx,
                                     DxuiTheme::kBodyFace, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
                }
            }
            else if (x1 - x0 >= textW + 2 * pad)
            {
                text.DrawString (label.c_str(), x0, static_cast<float> (bar.top), x1 - x0, static_cast<float> (bar.bottom - bar.top),
                                 DiskInspectorPalette::GetTextColorOn (m_scene.context.palette.GetKindColor (kKindColors[static_cast<size_t> (section.kind)])), textPx,
                                 DxuiTheme::kBodyFace, DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::SemiBold, false);
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  StripTrackPainter::GetLabelRowClearance
//
//  How far from the left of a stretch's visible part its label in the
//  label row starts: past the sector label where the stretch's visible part
//  starts where its sector's bracket does, and past the write seam's label
//  over the seam.
//
////////////////////////////////////////////////////////////////////////////////

float StripTrackPainter::GetLabelRowClearance (IDxuiTextRenderer & text, const TrackAnalysis & track, const StripGeometry & g, const NibbleSection & section,
                                            float x0, float x1) const
{
    std::array<StripSegment, 2>  seams    = {};
    std::array<StripSegment, 2>  brackets = {};
    int                          count    = m_scene.hits.GetSeamSegments (track, g, seams);
    int                          inView   = 0;
    float                        textPx   = m_scene.scaler.ToPxf (s_kLabelDip);
    float                        pad      = m_scene.scaler.ToPxf (s_kPadDip);
    float                        gap      = m_scene.scaler.ToPxf (s_kBracketGapDip);
    float                        clearW   = 0;
    float                        labelW   = 0;
    float                        textW    = 0;
    float                        textH    = 0;
    double                       a        = 0;
    double                       b        = 0;
    std::wstring                 label;



    if (section.sectorIndex >= 0)
    {
        inView = m_scene.hits.GetSectorParts (track, g, section.sectorIndex, a, b, brackets);

        for (int j = 0; j < inView; j++)
        {
            labelW = GetSectorLabel (text, track.sectors[static_cast<size_t> (section.sectorIndex)], brackets[j].x1 - brackets[j].x0 - 2 * gap, label);

            if (labelW > 0 && std::abs (brackets[j].x0 - x0) < 1.0f)
            {
                clearW = std::max (clearW, gap + labelW);
            }
        }
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
//  StripTrackPainter::PaintCells
//
//  Under the values, a band with each cell in view: its 1 or 0 with a
//  pulse over each 1, and on a flux track its own timing once that fits
//  (FR-035).
//
////////////////////////////////////////////////////////////////////////////////

void StripTrackPainter::PaintCells (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const TrackAnalysis & track, const StripGeometry & g) const
{
    const FramedTrack &          framed     = track.framed;
    RECT                         bar        = m_scene.bar;
    std::array<StripSegment, 2>  parts      = {};
    float                        height     = static_cast<float> (bar.bottom - bar.top);
    float                        bandTop    = static_cast<float> (bar.top) + height * s_kValueShare;
    float                        bandH      = static_cast<float> (bar.bottom) - bandTop;
    float                        rowH       = bandH / 2;
    float                        textPx     = m_scene.scaler.ToPxf (InspectorView::kSmallDip);
    bool                         showTiming = StripHitTest::IsTimed (track) && StripHitTest::GetCellPx (track, m_scene.context.model->GetStripSpan(), m_scene.GetWidth()) >= m_scene.scaler.ToPxf (StripHitTest::kCellTimingDip);
    uint32_t                     first      = FluxTiming::GetCellAt (m_scene.hits.GetCellTurns(), g.GetTurn (static_cast<float> (bar.left)));
    uint32_t                     inView     = static_cast<uint32_t> (m_scene.GetWidth() / std::max (StripHitTest::GetCellPx (track, m_scene.context.model->GetStripSpan(), m_scene.GetWidth()), 1.0f)) + 2;
    uint32_t                     n          = 0;
    uint32_t                     cell       = 0;
    int                          count      = 0;
    int                          k          = 0;
    float                        cx         = 0;
    uint32_t                     band       = (theme.Background() & 0x00FFFFFFu) | s_kBandAlpha;



    painter.FillRect (static_cast<float> (bar.left), bandTop, m_scene.GetWidth(), bandH, band);

    for (n = 0; n < inView && n < framed.cellCount; n++)
    {
        cell  = (first + n) % framed.cellCount;
        count = g.GetSegments (m_scene.hits.GetCellTurns()[cell], m_scene.hits.GetCellTurns()[cell + 1] - m_scene.hits.GetCellTurns()[cell], parts);

        for (k = 0; k < count; k++)
        {
            cx = (parts[k].x0 + parts[k].x1) / 2;

            if (framed.cells[cell] != 0)
            {
                painter.DrawLine (cx, bandTop + m_scene.scaler.ToPxf (2.0f), cx, bandTop + rowH, m_scene.scaler.ToPxf (1.5f), theme.Foreground());
            }

            text.DrawString (framed.cells[cell] != 0 ? L"1" : L"0", parts[k].x0, bandTop + rowH, parts[k].x1 - parts[k].x0, rowH, theme.Foreground(), textPx,
                             DxuiTheme::kMonoFace, DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);

            if (showTiming)
            {
                std::wstring  percent = InspectorFormat::FormatPercent (FluxTiming::GetDeviation (framed.cellTicks[cell]));
                float         textW   = 0.0f;
                float         textH   = 0.0f;

                //  On a patch, so the cell's tick can't run through its digits.
                text.MeasureString (percent.c_str(), textPx, DxuiTheme::kBodyFace, textW, textH);
                painter.FillRect (cx - textW / 2, bandTop + (rowH - textH) / 2, textW, textH, (theme.Background() & 0x00FFFFFFu) | s_kPercentPatchAlpha);
                text.DrawString (percent.c_str(), parts[k].x0, bandTop,
                                 parts[k].x1 - parts[k].x0, rowH, theme.Foreground(), textPx, DxuiTheme::kBodyFace, DxuiTextHAlign::Center,
                                 DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  StripTrackPainter::PaintTimingLine
//
//  On a flux track, the mean cell timing under each few pixels, nominal in
//  the middle of the band from top to bottom, slow above and fast below,
//  with the timing range filling it. In Timing mode it is a waveform over
//  the colored bands, the area below the line shaded (FR-033).
//
////////////////////////////////////////////////////////////////////////////////

void StripTrackPainter::PaintTimingLine (IDxuiPainter & painter, const IDxuiTheme & theme, const TrackAnalysis & track, const StripGeometry & g, float top, float bottom) const
{
    RECT      bar    = m_scene.bar;
    float     inset  = m_scene.scaler.ToPxf (s_kLineInsetDip);
    float     mid    = (top + bottom) / 2.0f;
    float     half   = std::max ((bottom - top) / 2.0f - inset, 1.0f);
    float     step   = m_scene.scaler.ToPxf (s_kLineStepDip);
    float     thick  = m_scene.scaler.ToPxf (s_kLineDip);
    float     x      = static_cast<float> (bar.left);
    float     prevX  = 0;
    float     prevY  = 0;
    float     y      = 0;
    uint32_t  a      = 0;
    uint32_t  b      = 0;
    double    t      = 0;
    bool      isPrev = false;
    bool      isWave = m_scene.context.isTimingMode;
    uint32_t  shade  = (theme.Foreground() & 0x00FFFFFFu) | s_kWaveShadeAlpha;



    painter.DrawLine (static_cast<float> (bar.left), mid, static_cast<float> (bar.right), mid, 1.0f, theme.Border());

    for (x = static_cast<float> (bar.left); x < bar.right; x += step)
    {
        a = FluxTiming::GetCellAt (m_scene.hits.GetCellTurns(), g.GetTurn (x));
        b = FluxTiming::GetCellAt (m_scene.hits.GetCellTurns(), g.GetTurn (std::min (x + step, static_cast<float> (bar.right))));
        t = std::clamp (FluxTiming::GetMeanDeviation (track, a, (b == a) ? a + 1 : b) / std::max (m_scene.context.timingRange, 1e-6), -1.0, 1.0);
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
//  StripTrackPainter::PaintSeam
//
//  The longest sync run, the likely write seam (FR-033): a rule along the
//  bottom of the label row, with its label over it where that fits.
//
////////////////////////////////////////////////////////////////////////////////

void StripTrackPainter::PaintSeam (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const TrackAnalysis & track, const StripGeometry & g) const
{
    RECT                         bar    = m_scene.bar;
    std::array<StripSegment, 2>  parts  = {};
    int                          count  = m_scene.hits.GetSeamSegments (track, g, parts);
    float                        rule   = m_scene.scaler.ToPxf (s_kSeamRuleDip);
    float                        textPx = m_scene.scaler.ToPxf (s_kLabelDip);
    float                        textW  = 0;
    float                        textH  = 0;
    int                          k      = 0;



    text.MeasureString (s_kpszSeamLabel, textPx, DxuiTheme::kBodyFace, textW, textH);

    for (k = 0; k < count; k++)
    {
        painter.FillRect (parts[k].x0, static_cast<float> (bar.top) - rule - 1.0f, std::max (parts[k].x1 - parts[k].x0, 1.0f), rule, theme.ForegroundMuted());

        if (parts[k].x1 - parts[k].x0 >= textW + m_scene.scaler.ToPxf (s_kPadDip))
        {
            text.DrawString (s_kpszSeamLabel, parts[k].x0, static_cast<float> (m_scene.bounds.top), parts[k].x1 - parts[k].x0,
                             static_cast<float> (bar.top - m_scene.bounds.top) - rule, theme.ForegroundMuted(), textPx, DxuiTheme::kBodyFace,
                             DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  StripTrackPainter::PaintLabels
//
//  Each sector bracketed in the label row from its address prologue to the
//  end of its data epilogue, with its state's symbol and "Sector $N" at the
//  left of the bracket's visible part, and the selected sector outlined.
//
////////////////////////////////////////////////////////////////////////////////

void StripTrackPainter::PaintLabels (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const TrackAnalysis & track, const StripGeometry & g) const
{
    RECT                         bar      = m_scene.bar;
    std::array<StripSegment, 2>  parts    = {};
    std::array<StripSegment, 2>  ticks    = {};
    int                          count    = 0;
    int                          k        = 0;
    size_t                       s        = 0;
    float                        rowTop   = static_cast<float> (m_scene.bounds.top);
    float                        line     = m_scene.scaler.ToPxf (s_kBracketDip);
    float                        ruleY    = static_cast<float> (bar.top) - line - 1.0f;
    float                        tickTop  = rowTop + (ruleY - rowTop) * s_kTickShare;
    float                        gap      = m_scene.scaler.ToPxf (s_kBracketGapDip);
    float                        thick    = m_scene.scaler.ToPxf (StripPaintScene::kOutlineDip);
    float                        labelW   = 0;
    double                       a        = 0;
    double                       b        = 0;
    std::wstring                 label;



    for (s = 0; s < track.sectors.size(); s++)
    {
        const AnalyzedSector &  sector = track.sectors[s];

        count = m_scene.hits.GetSectorParts (track, g, static_cast<int> (s), a, b, parts);

        for (k = 0; k < count; k++)
        {
            painter.DrawLine (parts[k].x0, ruleY, parts[k].x1, ruleY, line, theme.ForegroundMuted());
            labelW = GetSectorLabel (text, sector, parts[k].x1 - parts[k].x0 - 2 * gap, label);

            if (labelW > 0)
            {
                text.DrawString (label.c_str(), parts[k].x0 + gap, rowTop, parts[k].x1 - parts[k].x0 - gap, ruleY - rowTop, m_scene.context.palette.GetStateColor (sector.state),
                                 m_scene.scaler.ToPxf (s_kLabelDip), DxuiTheme::kBodyFace, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::SemiBold, false);
            }
        }

        //  A tick at each end of the bracket that is in view.
        for (double at : { a, b - s_kTickTurn })
        {
            for (k = 0; k < g.GetSegments (at, s_kTickTurn, ticks); k++)
            {
                float  x = (at == a) ? ticks[k].x0 : ticks[k].x1;

                painter.DrawLine (x, tickTop, x, ruleY + line / 2, line, theme.ForegroundMuted());
            }
        }

        if (static_cast<int> (s) == m_scene.context.model->GetSectorIndex())
        {
            for (k = 0; k < count; k++)
            {
                painter.OutlineRect (parts[k].x0, static_cast<float> (bar.top), parts[k].x1 - parts[k].x0, static_cast<float> (bar.bottom - bar.top), thick, theme.Accent());
            }
        }
    }

    PaintOrphanLabels (painter, text, theme, track, g);
}





////////////////////////////////////////////////////////////////////////////////
//
//  StripTrackPainter::PaintOrphanLabels
//
//  Each data field with no address field before it, bracketed in the label
//  row as a sector is, in the bad address's color.
//
////////////////////////////////////////////////////////////////////////////////

void StripTrackPainter::PaintOrphanLabels (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const TrackAnalysis & track,
                                           const StripGeometry & g) const
{
    const vector<double> &       turns   = m_scene.hits.GetTurns();
    std::array<StripSegment, 2>  parts   = {};
    std::array<StripSegment, 2>  ticks   = {};
    int                          count   = 0;
    int                          k       = 0;
    float                        rowTop  = static_cast<float> (m_scene.bounds.top);
    float                        line    = m_scene.scaler.ToPxf (s_kBracketDip);
    float                        ruleY   = static_cast<float> (m_scene.bar.top) - line - 1.0f;
    float                        tickTop = rowTop + (ruleY - rowTop) * s_kTickShare;
    float                        gap     = m_scene.scaler.ToPxf (s_kBracketGapDip);
    std::wstring                 symbol  = DiskInspectorPalette::GetStateSymbol (SectorState::BadAddress);
    uint32_t                     color   = m_scene.context.palette.GetStateColor (SectorState::BadAddress);
    std::wstring                 label;



    for (int f : OrphanFields::Find (track))
    {
        const LocatedField &  field = track.fields[static_cast<size_t> (f)];
        int                   end   = field.firstNibble + field.nibbleCount;
        double                a     = 0;
        double                b     = 0;

        if (turns.empty() || field.firstNibble >= static_cast<int> (turns.size()))
        {
            continue;
        }

        a     = turns[static_cast<size_t> (field.firstNibble)];
        b     = (end < static_cast<int> (turns.size())) ? turns[static_cast<size_t> (end)] : turns.back();
        count = g.GetSegments (a, b - a - std::floor (b - a), parts);

        for (k = 0; k < count; k++)
        {
            painter.DrawLine (parts[k].x0, ruleY, parts[k].x1, ruleY, line, theme.ForegroundMuted());

            if (GetFittingLabel (text, { symbol + L" No address field", symbol + L" No address", symbol }, parts[k].x1 - parts[k].x0 - 2 * gap, label) > 0)
            {
                text.DrawString (label.c_str(), parts[k].x0 + gap, rowTop, parts[k].x1 - parts[k].x0 - gap, ruleY - rowTop, color, m_scene.scaler.ToPxf (s_kLabelDip),
                                 DxuiTheme::kBodyFace, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::SemiBold, false);
            }
        }

        for (double at : { a, b - s_kTickTurn })
        {
            for (k = 0; k < g.GetSegments (at, s_kTickTurn, ticks); k++)
            {
                float  x = (at == a) ? ticks[k].x0 : ticks[k].x1;

                painter.DrawLine (x, tickTop, x, ruleY + line / 2, line, theme.ForegroundMuted());
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  StripTrackPainter::GetNibbleKindLabel
//
//  A nibble's kind as its value's label in the bar: the kind as the legend
//  lists it, with an address or data field's body as just "Address" or
//  "Data".
//
////////////////////////////////////////////////////////////////////////////////

std::wstring StripTrackPainter::GetNibbleKindLabel (NibbleKind kind)
{
    std::wstring  label;



    switch (kind)
    {
        case NibbleKind::AddressField: label = L"Address";                                    break;
        case NibbleKind::DataField:    label = L"Data";                                       break;
        default:                       label = InspectorText::FormatNibbleKind (kind, false); break;
    }

    return label;
}





////////////////////////////////////////////////////////////////////////////////
//
//  StripTrackPainter::GetSectorLabel
//
//  A sector's label in its bracket: its state's symbol and "Sector $N", or
//  the symbol and "$N" where that is all that fits in the width given.
//  Returns the label's width, or 0 when neither fits.
//
////////////////////////////////////////////////////////////////////////////////

float StripTrackPainter::GetSectorLabel (IDxuiTextRenderer & text, const AnalyzedSector & sector, float widthPx, std::wstring & outLabel) const
{
    std::wstring  symbol = DiskInspectorPalette::GetStateSymbol (sector.state);
    std::wstring  number = InspectorFormat::FormatSector (sector.sector);



    return GetFittingLabel (text, { symbol + L" Sector " + number, symbol + L" " + number }, widthPx, outLabel);
}





////////////////////////////////////////////////////////////////////////////////
//
//  StripTrackPainter::GetFittingLabel
//
//  The first of the labels, longest first, that fits in the width given.
//  Returns its width, or 0 when none fits.
//
////////////////////////////////////////////////////////////////////////////////

float StripTrackPainter::GetFittingLabel (IDxuiTextRenderer & text, std::initializer_list<std::wstring> labels, float widthPx, std::wstring & outLabel) const
{
    float  textPx = m_scene.scaler.ToPxf (s_kLabelDip);
    float  textW  = 0;
    float  textH  = 0;
    float  fitW   = 0;



    outLabel.clear();

    for (const std::wstring & label : labels)
    {
        text.MeasureString (label.c_str(), textPx, DxuiTheme::kBodyFace, textW, textH);

        if (outLabel.empty() && textW <= widthPx)
        {
            outLabel = label;
            fitW     = textW;
        }
    }

    return fitW;
}
