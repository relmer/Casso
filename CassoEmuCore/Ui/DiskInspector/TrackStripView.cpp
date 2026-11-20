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
#include "Ui/DiskInspector/StripHitTest.h"
#include "Ui/DiskInspector/StripOverlayPainter.h"
#include "Ui/DiskInspector/StripTrackPainter.h"
#include "Ui/DiskInspector/ZoomStops.h"
#include "Core/NumberFormat.h"





static constexpr int  s_kLabelRowDip = 18;





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
//  The nibbles, then the overlays: the Files band, the dividers, the
//  differences, the find hits, the selection and the shimmer. Then the
//  write seam, the sector labels and the head.
//
////////////////////////////////////////////////////////////////////////////////

void TrackStripView::PaintTrack (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const TrackAnalysis & track)
{
    StripGeometry         g       = MakeGeometry();
    RECT                  bar     = GetBarRect();
    float                 top     = static_cast<float> (bar.top);
    float                 height  = static_cast<float> (bar.bottom - bar.top);
    float                 digitsW = 0;
    vector<StripSegment>  fileParts;



    m_hits.Update (track);

    if (m_context.analysis != nullptr)
    {
        m_hits.UpdateSections (track, *m_context.analysis, m_context.model->GetQuarterTrack());
    }

    StripPaintScene      scene   = { m_context, m_scaler, m_hits, m_boundsDip, bar, m_side, m_isPanning };
    StripTrackPainter    nibbles (scene);
    StripOverlayPainter  overlay (scene);

    digitsW   = nibbles.PaintNibbles (painter, text, theme, track, g);
    m_digitsW = digitsW;

    if (m_context.isFileHighlighted)
    {
        m_hits.GetFileParts (track, g, m_context.fileMap, m_context.model->GetQuarterTrack(), m_context.selectedFile, fileParts);
        overlay.DimOutsideFile (painter, fileParts);
    }

    overlay.PaintFiles    (painter, theme, track, g);
    overlay.PaintDividers (painter, theme, g, top, height);

    if (m_context.isFileHighlighted)
    {
        overlay.PaintMarquee (painter, fileParts);
    }

    overlay.PaintDiffs     (painter, track, g);
    overlay.PaintHits      (painter, track, g);
    overlay.PaintSelection (painter, theme, g);
    overlay.PaintShimmer   (painter, g);
    nibbles.PaintSeam      (painter, text, theme, track, g);
    nibbles.PaintLabels    (painter, text, theme, track, g);

    //  Where each nibble's value fills the bar, the stretch labels move to
    //  the label row.
    nibbles.PaintStretchLabels (text, theme, track, g, StripHitTest::GetCellPx (track, m_context.model->GetStripSpan(), GetWidth()) * StripHitTest::kCellsPerNibble >= digitsW + m_scaler.ToPxf (StripHitTest::kValuePadDip));

    overlay.PaintHead (painter, g);
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
    int                    nibble    = -1;
    int                    sector    = -1;
    int64_t                now       = static_cast<int64_t> (GetTickCount64());
    bool                   isMoved   = false;



    switch (track != nullptr ? ev.kind : DxuiMouseEventKind::Leave)
    {
        case DxuiMouseEventKind::Wheel:
            if (isInside && !ev.wheelHorizontal && !ev.shift)
            {
                StepZoomAt (ZoomStops::GetWheelSteps (ev.wheelDelta), fraction);
                isHandled = true;
            }
            else if (isInside)
            {
                m_context.model->SetStrip (m_context.model->GetStripStart() - ev.wheelDelta * span / 8.0, span);
                isHandled = true;
            }

            break;

        case DxuiMouseEventKind::Down:
            if (isInside && ev.button == DxuiMouseButton::Left && !ev.shift && p.y < GetBarRect().top)
            {
                //  A click on a sector's bracket selects the sector, and the
                //  platter flies to it.
                m_hits.Update (*track);
                sector    = m_hits.GetSectorAt (*track, MakeGeometry(), static_cast<float> (p.x));
                isHandled = sector >= 0;

                if (sector >= 0)
                {
                    m_context.model->SelectSector (m_context.model->GetQuarterTrack(), sector);
                    NotifySelection();
                    m_context.model->StartFlight (InspectorMarquee::GetClockMs());
                }
            }
            else if (isInside && ev.button == DxuiMouseButton::Left)
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
                    m_hits.Update (*track);
                    nibble        = m_hits.GetNibbleAt (MakeGeometry(), static_cast<float> (p.x));
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
                m_hits.Update (*track);
                nibble = m_hits.GetNibbleAt (MakeGeometry(), std::clamp (static_cast<float> (p.x), static_cast<float> (m_boundsDip.left), static_cast<float> (m_boundsDip.right - 1)));

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
                m_hits.Update (*track);
                nibble = m_hits.GetNibbleAt (MakeGeometry(), static_cast<float> (p.x));

                if (nibble >= 0)
                {
                    m_context.model->ExtendNibbles (nibble);
                }
            }

            if (m_isPressed && !m_isShiftPress && !isMoved && now - m_lastClickMs <= static_cast<int64_t> (GetDoubleClickTime()))
            {
                SelectFieldAt (*track, static_cast<float> (p.x));
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

            //  A range dragged out on the strip is flown to on the platter.
            if (m_isSelecting && isMoved)
            {
                m_context.model->StartFlight (InspectorMarquee::GetClockMs());
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
    else if (track != nullptr && !m_isPanning && pointPx.y >= m_boundsDip.top && pointPx.y < bar.top && m_hits.IsOverSeam (*track, MakeGeometry(), static_cast<float> (pointPx.x)))
    {
        outText     = std::format (L"Longest sync run, {} nibbles at cell {}\nThe likely write seam", track->measurements.longestSync.count,
                                   NumberFormat::FormatInteger (track->measurements.longestSync.startCell));
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
        m_hits.Update (*track);
        nibble = m_hits.GetNibbleAt (MakeGeometry(), static_cast<float> (pointPx.x));
    }

    if (nibble >= 0)
    {
        first  = static_cast<uint32_t> (StripHitTest::GetCellAt (*track, MakeGeometry(), static_cast<float> (pointPx.x)));
        next   = static_cast<uint32_t> (StripHitTest::GetCellAt (*track, MakeGeometry(), static_cast<float> (pointPx.x + 1)));
        outTip = InspectorTip::BuildNibbleTip (m_context, *track, nibble, first, (next + track->framed.cellCount - first) % track->framed.cellCount,
                                               m_context.isTimingMode);
    }

    return nibble >= 0;
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
//  TrackStripView::ZoomAboutCenter
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
//  TrackStripView::StepZoom
//
////////////////////////////////////////////////////////////////////////////////

void TrackStripView::StepZoom (int steps)
{
    StepZoomAt (steps, 0.5);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackStripView::StepZoomAt
//
//  The strip zoomed a number of stops about a point a fraction of the way
//  across it, which stays under the point.
//
////////////////////////////////////////////////////////////////////////////////

void TrackStripView::StepZoomAt (int steps, double fraction)
{
    const TrackAnalysis *  track   = m_context.GetTrack();
    double                 span    = m_context.model->GetStripSpan();
    double                 newSpan = span;



    if (track != nullptr && span > 0)
    {
        newSpan = std::clamp (1.0 / ZoomStops::Step (StripHitTest::GetZoomStops (*track, GetWidth(), m_digitsW, m_scaler), 1.0 / span, steps), StripGeometry::kMinSpan, 1.0);
        m_context.model->SetStrip (StripGeometry::GetStartForZoom (m_context.model->GetStripStart(), span, newSpan, fraction), newSpan);
    }
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
//  TrackStripView::SelectFieldAt
//
//  Selects the field around the nibble at x, as a double-click selects a word.
//
////////////////////////////////////////////////////////////////////////////////

void TrackStripView::SelectFieldAt (const TrackAnalysis & track, float x)
{
    int  nibble = m_hits.GetNibbleAt (MakeGeometry(), x);



    for (const auto & field : track.fields)
    {
        if (nibble >= field.firstNibble && nibble < field.firstNibble + field.nibbleCount)
        {
            m_context.model->SelectNibbles (m_context.model->GetQuarterTrack(), field.firstNibble, field.nibbleCount);
        }
    }
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
        m_hits.Update (*track);
        m_hits.GetCellsInView (*track, start, span, first, last);
    }

    return (track != nullptr && track->framed.cellCount > 0)
         ? InspectorText::FormatStripReadout (first, last, StripOverlayPainter::IsHeadOnTrack (m_context, m_side) ? StripOverlayPainter::GetHeadLabel (m_context.head.activity) : L"")
         : std::wstring();
}





