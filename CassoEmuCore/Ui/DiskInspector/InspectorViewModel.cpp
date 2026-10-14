#include "Pch.h"

#include "Ui/DiskInspector/InspectorViewModel.h"
#include "Devices/Disk/Inspector/TrackAnalyzer.h"
#include "Ui/DiskInspector/PlatterGeometry.h"





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorViewModel::SetDisk
//
//  Another disk starts everything over; the same disk keeps it all.
//
////////////////////////////////////////////////////////////////////////////////

void InspectorViewModel::SetDisk (uint64_t mediaId)
{
    if (mediaId != m_mediaId)
    {
        *this     = InspectorViewModel();
        m_mediaId = mediaId;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorViewModel::SetAnalysis
//
//  After a track is analyzed again, the sector nearest the cell the
//  selection started at is selected (FR-038), which is the same sector when
//  it is still there. A nibble range past the track's new end is dropped.
//
////////////////////////////////////////////////////////////////////////////////

void InspectorViewModel::SetAnalysis (const DiskAnalysis * analysis)
{
    const TrackAnalysis *  track    = nullptr;
    uint32_t               cells    = 0;
    uint32_t               best     = UINT32_MAX;
    uint32_t               distance = 0;
    uint32_t               start    = 0;
    size_t                 i        = 0;



    m_analysis = analysis;
    track      = GetTrack();

    //  The current track's analysis arrives after the disk; the strip is
    //  fitted to it then.
    if (!m_isStripFitted)
    {
        FitStripToTrack();
    }

    if (track == nullptr || track->sectors.empty())
    {
        m_sectorIndex = -1;
    }
    else if (!m_hasAnchor)
    {
        SelectFirstSector();
    }
    else
    {
        cells = std::max<uint32_t> (track->framed.cellCount, 1);

        for (i = 0; i < track->sectors.size(); i++)
        {
            start    = track->fields[track->sectors[i].addressField].startCell;
            distance = (start + cells - m_anchorCell % cells) % cells;
            distance = std::min (distance, cells - distance);

            if (distance < best)
            {
                best          = distance;
                m_sectorIndex = static_cast<int> (i);
            }
        }
    }

    if (track == nullptr || m_firstNibble + m_nibbleCount > static_cast<int> (track->framed.nibbles.size()))
    {
        m_firstNibble = -1;
        m_nibbleCount = 0;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorViewModel::SelectQuarterTrack
//
//  A track chosen on its own selects its first sector in passing order
//  (FR-039).
//
////////////////////////////////////////////////////////////////////////////////

void InspectorViewModel::SelectQuarterTrack (int quarterTrack)
{
    m_quarterTrack = std::clamp (quarterTrack, 0, DiskImage::kQuarterTrackCount - 1);
    m_firstNibble  = -1;
    m_nibbleCount  = 0;
    m_nibbleAnchor = -1;
    ClearBytes();

    SelectFirstSector();
    FitStripToTrack();
    BringSelectionIntoView();
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorViewModel::FitStripToTrack
//
//  The strip spans the whole of a track with no standard sectors, and is set
//  back to the usual span on the next track with sectors.
//
////////////////////////////////////////////////////////////////////////////////

void InspectorViewModel::FitStripToTrack()
{
    const TrackAnalysis *  track = GetTrack();



    m_isStripFitted = (track != nullptr);

    if (track != nullptr && track->sectorsFound == 0)
    {
        SetStrip (0.0, 1.0);
        m_isStripWhole = true;
    }
    else if (m_isStripWhole)
    {
        SetStrip (m_stripStart, kDefaultStripSpan);
        m_isStripWhole = false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorViewModel::ClearSelection
//
////////////////////////////////////////////////////////////////////////////////

void InspectorViewModel::ClearSelection()
{
    m_sectorIndex  = -1;
    m_firstNibble  = -1;
    m_nibbleCount  = 0;
    m_nibbleAnchor = -1;
    m_hasAnchor    = false;
    ClearBytes();
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorViewModel::SelectSector
//
////////////////////////////////////////////////////////////////////////////////

void InspectorViewModel::SelectSector (int quarterTrack, int sectorIndex)
{
    const TrackAnalysis *  track     = nullptr;
    int                    previous  = m_quarterTrack;



    m_quarterTrack = std::clamp (quarterTrack, 0, DiskImage::kQuarterTrackCount - 1);
    m_firstNibble  = -1;
    m_nibbleCount  = 0;
    m_nibbleAnchor = -1;
    ClearBytes();
    track          = GetTrack();

    if (m_quarterTrack != previous)
    {
        FitStripToTrack();
    }

    if (track != nullptr && sectorIndex >= 0 && sectorIndex < static_cast<int> (track->sectors.size()))
    {
        m_sectorIndex = sectorIndex;
        m_anchorCell  = track->fields[track->sectors[sectorIndex].addressField].startCell;
        m_hasAnchor   = true;
    }
    else
    {
        SelectFirstSector();
    }

    BringSelectionIntoView();
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorViewModel::SelectNibbles
//
//  The sector whose field contains the first nibble is selected with them.
//
////////////////////////////////////////////////////////////////////////////////

void InspectorViewModel::SelectNibbles (int quarterTrack, int firstNibble, int nibbleCount)
{
    const TrackAnalysis *  track = nullptr;
    int                    field = -1;
    size_t                 i     = 0;



    m_quarterTrack = std::clamp (quarterTrack, 0, DiskImage::kQuarterTrackCount - 1);
    track          = GetTrack();

    if (track == nullptr || firstNibble < 0 || firstNibble >= static_cast<int> (track->framed.nibbles.size()))
    {
        SelectQuarterTrack (m_quarterTrack);
    }
    else
    {
        m_firstNibble  = firstNibble;
        m_nibbleCount  = std::max (nibbleCount, 1);
        m_nibbleAnchor = firstNibble;
        ClearBytes();
        m_anchorCell  = track->framed.nibbles[firstNibble].startCell;
        m_hasAnchor   = true;
        field         = (firstNibble < static_cast<int> (track->fieldOfNibble.size())) ? track->fieldOfNibble[firstNibble] : -1;

        for (i = 0; i < track->sectors.size() && field >= 0; i++)
        {
            if (track->sectors[i].addressField == field || track->sectors[i].dataField == field)
            {
                m_sectorIndex = static_cast<int> (i);
            }
        }

        BringSelectionIntoView();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorViewModel::ExtendNibbles
//
//  From the nibble the range started on to the one given, in either
//  direction, keeping where it started for the next extension.
//
////////////////////////////////////////////////////////////////////////////////

void InspectorViewModel::ExtendNibbles (int toNibble)
{
    int  anchor = (m_nibbleAnchor >= 0) ? m_nibbleAnchor : toNibble;



    SelectNibbles (m_quarterTrack, std::min (anchor, toNibble), std::abs (toNibble - anchor) + 1);
    m_nibbleAnchor = anchor;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorViewModel::SelectAllNibbles
//
////////////////////////////////////////////////////////////////////////////////

void InspectorViewModel::SelectAllNibbles()
{
    const TrackAnalysis *  track = GetTrack();



    if (track != nullptr && !track->framed.nibbles.empty())
    {
        SelectNibbles (m_quarterTrack, 0, static_cast<int> (track->framed.nibbles.size()));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorViewModel::SelectBytes
//
//  Bytes of the selected sector's data, clamped to it.
//
////////////////////////////////////////////////////////////////////////////////

void InspectorViewModel::SelectBytes (int firstByte, int byteCount, bool isTextColumn)
{
    int  first = std::clamp (firstByte, 0, DiskFieldFormat::kSectorBytes - 1);



    if (GetSector() != nullptr && GetSector()->dataField >= 0)
    {
        m_firstByte    = first;
        m_byteCount    = std::clamp (byteCount, 1, DiskFieldFormat::kSectorBytes - first);
        m_byteAnchor   = first;
        m_isTextColumn = isTextColumn;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorViewModel::ExtendBytes
//
////////////////////////////////////////////////////////////////////////////////

void InspectorViewModel::ExtendBytes (int toByte)
{
    int  to     = std::clamp (toByte, 0, DiskFieldFormat::kSectorBytes - 1);
    int  anchor = (m_byteAnchor >= 0) ? m_byteAnchor : to;



    SelectBytes (std::min (anchor, to), std::abs (to - anchor) + 1, m_isTextColumn);
    m_byteAnchor = anchor;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorViewModel::SelectAllBytes
//
////////////////////////////////////////////////////////////////////////////////

void InspectorViewModel::SelectAllBytes()
{
    SelectBytes (0, DiskFieldFormat::kSectorBytes, m_isTextColumn);
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorViewModel::ClearBytes
//
////////////////////////////////////////////////////////////////////////////////

void InspectorViewModel::ClearBytes()
{
    m_firstByte  = -1;
    m_byteCount  = 0;
    m_byteAnchor = -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorViewModel::GetTrack
//
////////////////////////////////////////////////////////////////////////////////

const TrackAnalysis * InspectorViewModel::GetTrack() const
{
    const TrackAnalysis *  track = nullptr;
    int                    slot  = -1;



    if (m_analysis != nullptr)
    {
        slot = m_analysis->entries[m_quarterTrack].slot;

        if (slot >= 0 && slot < static_cast<int> (m_analysis->tracks.size()))
        {
            track = m_analysis->tracks[slot].get();
        }
    }

    return track;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorViewModel::GetSector
//
////////////////////////////////////////////////////////////////////////////////

const AnalyzedSector * InspectorViewModel::GetSector() const
{
    const TrackAnalysis *  track = GetTrack();



    return (track != nullptr && m_sectorIndex >= 0 && m_sectorIndex < static_cast<int> (track->sectors.size()))
           ? &track->sectors[m_sectorIndex] : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorViewModel::ZoomAbout
//
//  The point of the disk under the anchor stays under it. Zooming out to fit
//  recenters the disk (FR-025).
//
////////////////////////////////////////////////////////////////////////////////

void InspectorViewModel::ZoomAbout (double zoom, Point anchor)
{
    double  next  = std::clamp (zoom, 1.0, kMaxZoom);
    double  ratio = next / m_zoom;



    m_isFlying = false;
    m_pan.x = anchor.x - (anchor.x - m_pan.x) * ratio;
    m_pan.y = anchor.y - (anchor.y - m_pan.y) * ratio;
    m_zoom  = next;

    ClampPan();
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorViewModel::Fit
//
////////////////////////////////////////////////////////////////////////////////

void InspectorViewModel::Fit()
{
    m_isFlying = false;
    m_zoom     = 1.0;
    m_pan      = Point();
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorViewModel::PanBy
//
////////////////////////////////////////////////////////////////////////////////

void InspectorViewModel::PanBy (Point delta)
{
    m_isFlying = false;
    m_pan.x   += delta.x;
    m_pan.y += delta.y;

    ClampPan();
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorViewModel::GetDiskPoint
//
//  Where a point on a ring sits in the view, in fit units from the view's
//  center: the turn runs clockwise from 12 o'clock, and y grows downward.
//
////////////////////////////////////////////////////////////////////////////////

InspectorViewModel::Point InspectorViewModel::GetDiskPoint (int quarterTrack, double turn) const
{
    Point  fit = GetFitPoint (quarterTrack, turn);



    return { m_pan.x + m_zoom * fit.x, m_pan.y + m_zoom * fit.y };
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorViewModel::GetFitPoint
//
////////////////////////////////////////////////////////////////////////////////

InspectorViewModel::Point InspectorViewModel::GetFitPoint (int quarterTrack, double turn)
{
    static constexpr double  kTwoPi = 6.283185307179586;



    double  r = GetRingRadius (quarterTrack);



    return { r * std::sin (kTwoPi * turn), -r * std::cos (kTwoPi * turn) };
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorViewModel::GetChordView
//
//  The view runs from -1 to 1 on each axis, so the line through its center
//  along a chord is 2 / max (|dx|, |dy|) times the chord's length. A chord
//  too long to fit at kFlightFill gets the view at fit.
//
////////////////////////////////////////////////////////////////////////////////

InspectorViewModel::View InspectorViewModel::GetChordView (Point from, Point to)
{
    double  span = std::max (std::abs (to.x - from.x), std::abs (to.y - from.y));
    View    view;



    view.zoom = (span > 0) ? std::clamp (2.0 * kFlightFill / span, 1.0, kMaxZoom) : 1.0;

    if (view.zoom > 1.0)
    {
        view.pan.x = -view.zoom * (from.x + to.x) / 2;
        view.pan.y = -view.zoom * (from.y + to.y) / 2;
    }

    return view;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorViewModel::GetBetweenView
//
//  A point d of the disk is at pan + zoom * d in the view. The point both
//  views put in the same place is d = (to.pan - from.pan) / (from.zoom -
//  to.zoom); the zoom moves from one to the other at an even rate in scale,
//  and the pan places d where both views put it.
//
////////////////////////////////////////////////////////////////////////////////

InspectorViewModel::View InspectorViewModel::GetBetweenView (const View & from, const View & to, double t)
{
    View   view;
    Point  fixed;
    Point  at;



    if (std::abs (from.zoom - to.zoom) < 1e-9)
    {
        view.zoom  = to.zoom;
        view.pan.x = from.pan.x + (to.pan.x - from.pan.x) * t;
        view.pan.y = from.pan.y + (to.pan.y - from.pan.y) * t;
    }
    else
    {
        fixed.x    = (to.pan.x - from.pan.x) / (from.zoom - to.zoom);
        fixed.y    = (to.pan.y - from.pan.y) / (from.zoom - to.zoom);
        at.x       = from.pan.x + from.zoom * fixed.x;
        at.y       = from.pan.y + from.zoom * fixed.y;
        view.zoom  = from.zoom * std::pow (to.zoom / from.zoom, t);
        view.pan.x = at.x - view.zoom * fixed.x;
        view.pan.y = at.y - view.zoom * fixed.y;
    }

    return view;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorViewModel::StartFlight
//
//  From the view as it is, out to fit, then in to the view centered on the
//  selection's chord. From a view already at fit, the first leg is skipped.
//
////////////////////////////////////////////////////////////////////////////////

void InspectorViewModel::StartFlight (double nowMs)
{
    double  end   = 0;
    double  start = GetSelectionTurn (end);



    if (start >= 0)
    {
        end             = (end < start) ? end + 1.0 : end;
        m_flightFrom    = GetView();
        m_flightTo      = GetChordView (GetFitPoint (m_quarterTrack, start), GetFitPoint (m_quarterTrack, end));
        m_flightStartMs = nowMs;
        m_isFlying      = true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorViewModel::StepFlight
//
//  The view for the moment. The flight as a whole is eased, slow at its
//  start and end and fastest between, and the two legs split it in the
//  proportion of their changes in zoom by scale, so the zoom's speed runs on
//  through the whole disk rather than stopping there. Returns whether the
//  flight goes on.
//
////////////////////////////////////////////////////////////////////////////////

bool InspectorViewModel::StepFlight (double nowMs)
{
    double  out      = std::log (std::max (m_flightFrom.zoom, 1.0));
    double  in       = std::log (std::max (m_flightTo.zoom, 1.0));
    double  share    = (out + in > 0) ? out / (out + in) : 0.0;
    double  duration = (out > 0) ? 2 * kFlightLegMs : kFlightLegMs;
    double  t        = std::clamp ((nowMs - m_flightStartMs) / duration, 0.0, 1.0);
    double  e        = Ease (t);
    View    view;



    if (out + in <= 0)
    {
        view = GetBetweenView (m_flightFrom, m_flightTo, e);
    }
    else if (e < share)
    {
        view = GetBetweenView (m_flightFrom, View(), e / share);
    }
    else
    {
        view = GetBetweenView (View(), m_flightTo, (e - share) / (1.0 - share));
    }

    if (m_isFlying)
    {
        m_zoom     = view.zoom;
        m_pan      = view.pan;
        m_isFlying = t < 1.0;
    }

    return m_isFlying;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorViewModel::Ease
//
//  Progress whose speed follows half a sine wave: slow at the start, fastest
//  halfway, and slow at the end.
//
////////////////////////////////////////////////////////////////////////////////

double InspectorViewModel::Ease (double t)
{
    static constexpr double  kPi = 3.141592653589793;



    return (1.0 - std::cos (kPi * t)) / 2.0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorViewModel::GetPlacement
//
////////////////////////////////////////////////////////////////////////////////

PlatterPlacement InspectorViewModel::GetPlacement (const RECT & boundsPx, double rotation) const
{
    PlatterPlacement  view;
    double            fit  = std::max (0.0, std::min (boundsPx.right - boundsPx.left, boundsPx.bottom - boundsPx.top) / 2.0 - kRimInsetPx);



    view.centerXPx     = static_cast<float> ((boundsPx.left + boundsPx.right) / 2.0 + m_pan.x * fit);
    view.centerYPx     = static_cast<float> ((boundsPx.top + boundsPx.bottom) / 2.0 + m_pan.y * fit);
    view.outerRadiusPx = static_cast<float> (fit * m_zoom);
    view.rotation      = static_cast<float> (rotation);
    view.headLimitRing = (m_analysis != nullptr) ? m_analysis->headLimit + 1 : DiskImage::kQuarterTrackCount;

    return view;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorViewModel::SetStrip
//
////////////////////////////////////////////////////////////////////////////////

void InspectorViewModel::SetStrip (double start, double span)
{
    m_stripSpan  = std::clamp (span, 1e-6, 1.0);
    m_stripStart = start - std::floor (start);
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorViewModel::GetRingRadius
//
//  The middle of a quarter track's ring, as a fraction of the rim's radius.
//
////////////////////////////////////////////////////////////////////////////////

double InspectorViewModel::GetRingRadius (int quarterTrack)
{
    return PlatterGeometry::GetRingMiddle (quarterTrack);
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorViewModel::SelectFirstSector
//
////////////////////////////////////////////////////////////////////////////////

void InspectorViewModel::SelectFirstSector()
{
    const TrackAnalysis *  track = GetTrack();
    size_t                 i     = 0;



    m_sectorIndex = -1;
    m_hasAnchor   = false;

    for (i = 0; track != nullptr && i < track->sectors.size(); i++)
    {
        if (m_sectorIndex < 0 || track->sectors[i].passingIndex < track->sectors[m_sectorIndex].passingIndex)
        {
            m_sectorIndex = static_cast<int> (i);
        }
    }

    if (m_sectorIndex >= 0)
    {
        m_anchorCell = track->fields[track->sectors[m_sectorIndex].addressField].startCell;
        m_hasAnchor  = true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorViewModel::BringSelectionIntoView
//
//  The strip pans the least that puts the selection's start in view with a
//  margin (FR-038). A strip whose part of the turn already contains the
//  selection stays put, so on a new track the same part of the turn is in
//  view. The platter is brought to the selection by a flight, StartFlight.
//
////////////////////////////////////////////////////////////////////////////////

void InspectorViewModel::BringSelectionIntoView()
{
    double  end    = 0;
    double  start  = GetSelectionTurn (end);
    double  offset = start - m_stripStart;



    offset = offset - std::floor (offset);

    if (start >= 0 && offset > m_stripSpan)
    {
        SetStrip (start - kViewMargin * m_stripSpan, m_stripSpan);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorViewModel::ClampPan
//
//  Panning stops with the disk's edge at the view's center: the disk's
//  center can move no farther from the view's than the disk's radius, so
//  a sector on the outermost ring can be centered.
//
////////////////////////////////////////////////////////////////////////////////

void InspectorViewModel::ClampPan()
{
    double  reach = m_zoom;



    if (IsAtFit())
    {
        m_pan = Point();
    }

    m_pan.x = std::clamp (m_pan.x, -reach, reach);
    m_pan.y = std::clamp (m_pan.y, -reach, reach);
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorViewModel::GetSelectionTurn
//
//  Where the selection starts and ends as fractions of the turn, or -1 with
//  nothing selected.
//
////////////////////////////////////////////////////////////////////////////////

double InspectorViewModel::GetSelectionTurn (double & outEnd) const
{
    const TrackAnalysis *   track  = GetTrack();
    const AnalyzedSector *  sector = GetSector();
    double                  start  = -1;
    int                     last   = 0;



    outEnd = -1;

    if (track != nullptr && m_firstNibble >= 0)
    {
        //  The selection ends where the nibble after its last one starts, so a
        //  single nibble spans its own cells rather than none.
        last   = (m_firstNibble + std::max (m_nibbleCount, 1)) % static_cast<int> (track->framed.nibbles.size());
        start  = TrackAnalyzer::GetAngle (*track, track->framed.nibbles[m_firstNibble].startCell);
        outEnd = TrackAnalyzer::GetAngle (*track, track->framed.nibbles[last].startCell);
    }
    else if (track != nullptr && sector != nullptr)
    {
        start  = TrackAnalyzer::GetAngle (*track, track->fields[sector->addressField].startCell);
        outEnd = TrackAnalyzer::GetAngle (*track, track->fields[sector->dataField >= 0 ? sector->dataField : sector->addressField].endCell);
    }

    return start;
}
