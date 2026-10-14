#include "Pch.h"

#include "Ui/DiskInspector/InspectorMarquee.h"





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorMarquee::InspectorMarquee
//
//  The dashes march a pixel every kStepMs, a dash pair every two dashes'
//  worth of steps, which reads as motion without flicker.
//
////////////////////////////////////////////////////////////////////////////////

InspectorMarquee::InspectorMarquee (const DiskInspectorPalette & palette, float dashPx, float thicknessPx, int64_t nowMs) :
    m_dashPx      (std::max (dashPx, 1.0f)),
    m_thicknessPx (thicknessPx),
    m_colorA      (palette.colors.selection),
    m_colorB      (DiskInspectorPalette::GetTextColorOn (palette.colors.selection))
{
    m_phasePx = static_cast<float> ((nowMs / kStepMs) % static_cast<int64_t> (std::ceil (m_dashPx * 2.0f)));
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorMarquee::DrawLine
//
////////////////////////////////////////////////////////////////////////////////

float InspectorMarquee::DrawLine (IDxuiPainter & painter, float x0, float y0, float x1, float y1, float startPx) const
{
    float  length = std::hypot (x1 - x0, y1 - y0);
    float  s      = startPx;
    float  next   = 0.0f;
    float  k      = 0.0f;
    float  pad    = m_thicknessPx + 1.0f;
    bool   isOut  = m_isCulled && (std::max (x0, x1) + pad < m_cull.left || std::min (x0, x1) - pad > m_cull.right ||
                                   std::max (y0, y1) + pad < m_cull.top  || std::min (y0, y1) - pad > m_cull.bottom);



    while (!isOut && length > 0.0f && s < startPx + length)
    {
        k    = std::floor ((s + m_phasePx) / m_dashPx);
        next = std::min ((k + 1.0f) * m_dashPx - m_phasePx, startPx + length);

        painter.DrawLine (x0 + (x1 - x0) * (s - startPx) / length,    y0 + (y1 - y0) * (s - startPx) / length,
                          x0 + (x1 - x0) * (next - startPx) / length, y0 + (y1 - y0) * (next - startPx) / length,
                          m_thicknessPx, (std::abs (static_cast<int64_t> (k)) % 2 == 0) ? m_colorA : m_colorB);
        s = next;
    }

    return startPx + length;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorMarquee::Dim
//
////////////////////////////////////////////////////////////////////////////////

uint32_t InspectorMarquee::Dim (const DiskInspectorPalette & palette, uint32_t argb)
{
    return DxuiColor::Lerp (argb, palette.background, 0.5f);
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorMarquee::GetShimmer
//
//  A narrow band of light on a faint glow. A point's place along the
//  window's diagonal is (x + y) from its top left; the band's center sweeps
//  from before the top left corner to past the bottom right one, so the
//  band moves fully across the window, at 45 degrees.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t InspectorMarquee::GetShimmer (double nowMs, float xPx, float yPx, const RECT & scenePx)
{
    double  place = (xPx - scenePx.left) + (yPx - scenePx.top);
    double  alpha = kShimmerPeakAlpha * (kShimmerGlowShare + (1.0 - kShimmerGlowShare) * GetBand (place - GetBandCenter (nowMs, scenePx)));



    return (static_cast<uint32_t> (std::clamp (alpha, 0.0, 255.0)) << 24) | 0x00FFFFFFu;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorMarquee::GetBandCenter
//
//  Where the band's center is along the window's diagonal, in pixels of
//  (x + y) from its top left.
//
////////////////////////////////////////////////////////////////////////////////

double InspectorMarquee::GetBandCenter (double nowMs, const RECT & scenePx)
{
    double  span    = std::max (1.0, static_cast<double> ((scenePx.right - scenePx.left) + (scenePx.bottom - scenePx.top)));
    double  overrun = kShimmerReach * kShimmerBandPx;



    return std::fmod (nowMs / kSweepMs, 1.0) * (span + 2 * overrun) - overrun;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorMarquee::GetBand
//
//  The band's strength at a distance from its center along the diagonal:
//  1 at the center, falling off as a bell curve.
//
////////////////////////////////////////////////////////////////////////////////

double InspectorMarquee::GetBand (double distance)
{
    return std::exp (-std::pow (distance / kShimmerBandPx, 2.0));
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorMarquee::PaintShimmer
//
//  The shimmer over a rectangle: the glow across it, then the band as
//  diagonal stripes two pixels apart along (x + y), each its part of the
//  bell curve, clipped to the rectangle.
//
////////////////////////////////////////////////////////////////////////////////

void InspectorMarquee::PaintShimmer (IDxuiPainter & painter, float x, float y, float w, float h, double nowMs, const RECT & scenePx)
{
    static constexpr float  kStripePx = 2.0f;



    double    center   = GetBandCenter (nowMs, scenePx) + scenePx.left + scenePx.top;
    double    reach    = kShimmerReach * kShimmerBandPx;
    double    alpha    = 0;
    RECT      prior    = {};
    RECT      box      = { static_cast<LONG> (std::floor (x)), static_cast<LONG> (std::floor (y)), static_cast<LONG> (std::ceil (x + w)), static_cast<LONG> (std::ceil (y + h)) };
    RECT      clip     = box;
    bool      hasPrior = painter.GetClipRect (prior);
    bool      isShown  = !hasPrior || IntersectRect (&clip, &box, &prior) != FALSE;
    uint32_t  white    = 0x00FFFFFFu;



    painter.FillRect (x, y, w, h, (static_cast<uint32_t> (kShimmerPeakAlpha * kShimmerGlowShare) << 24) | white);

    //  Only the stripes that cross the rectangle: x + y runs from x + y at
    //  its top left to (x + w) + (y + h) at its bottom right.
    if (isShown && center + reach > x + y && center - reach < x + w + y + h)
    {
        painter.SetClipRect (&clip);

        for (double k = center - reach; k < center + reach; k += kStripePx)
        {
            alpha = kShimmerPeakAlpha * (1.0 - kShimmerGlowShare) * GetBand (k + kStripePx / 2 - center);

            painter.FillConvexQuad (static_cast<float> (k - y),                  y,
                                    static_cast<float> (k + kStripePx - y),      y,
                                    static_cast<float> (k + kStripePx - y - h),  y + h,
                                    static_cast<float> (k - y - h),              y + h,
                                    (static_cast<uint32_t> (std::clamp (alpha, 0.0, 255.0)) << 24) | white);
        }

        painter.SetClipRect (hasPrior ? &prior : nullptr);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorMarquee::GetFileSpans
//
//  The track's sectors in the order they pass the head, each from its first
//  field's first nibble to its last field's end; a sector of the file that
//  directly follows another of the file's sectors extends that one's stretch
//  rather than starting its own, around the index too.
//
////////////////////////////////////////////////////////////////////////////////

vector<InspectorMarquee::Span> InspectorMarquee::GetFileSpans (const TrackAnalysis & track, const vector<double> & nibbleTurns, const FileMap & map, int wholeTrack,
                                                                int file)
{
    struct Placed
    {
        int   first  = 0;
        int   end    = 0;
        bool  isMine = false;
    };

    int             count  = static_cast<int> (track.framed.nibbles.size());
    int             cell   = -1;
    int             half   = 0;
    double          shift  = 0;
    vector<Placed>  placed;
    vector<Span>    spans;

    auto  turnAt = [&] (int nibble) { return nibbleTurns[nibble % count] + (nibble / count); };



    for (const AnalyzedSector & sector : (count > 0 && nibbleTurns.size() > static_cast<size_t> (count)) ? track.sectors : vector<AnalyzedSector>())
    {
        int  head = (sector.addressField >= 0) ? sector.addressField : sector.dataField;
        int  tail = (sector.dataField >= 0) ? sector.dataField : sector.addressField;

        if (head < 0)
        {
            continue;
        }

        cell = (sector.dataField >= 0) ? map.GetCellOf (wholeTrack, sector.sector, half) : -1;

        Placed  p { track.fields[head].firstNibble, track.fields[tail].firstNibble + track.fields[tail].nibbleCount,
                    cell >= 0 && std::find (map.cells[cell].owners.begin(), map.cells[cell].owners.end(), file) != map.cells[cell].owners.end() };

        p.end = (p.end < p.first) ? p.end + count : p.end;
        placed.push_back (p);
    }

    std::sort (placed.begin(), placed.end(), [] (const Placed & x, const Placed & y) { return x.first < y.first; });

    for (size_t i = 0; i < placed.size(); i++)
    {
        if (!placed[i].isMine)
        {
            continue;
        }

        if (i > 0 && placed[i - 1].isMine && !spans.empty())
        {
            spans.back().b = turnAt (placed[i].end);
        }
        else
        {
            spans.push_back ({ turnAt (placed[i].first), turnAt (placed[i].end) });
        }
    }

    //  The last sector around the track joins the first when both are the
    //  file's.
    if (spans.size() > 1 && placed.front().isMine && placed.back().isMine)
    {
        spans.back().b = spans.front().b + 1.0;
        spans.erase (spans.begin());
    }

    for (Span & span : spans)
    {
        shift   = std::floor (span.a);
        span.a -= shift;
        span.b -= shift;
    }

    return spans;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorMarquee::GetClockMs
//
////////////////////////////////////////////////////////////////////////////////

double InspectorMarquee::GetClockMs()
{
    return std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now().time_since_epoch()).count();
}