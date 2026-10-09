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

    SelectFirstSector();
    BringSelectionIntoView();
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorViewModel::SelectSector
//
////////////////////////////////////////////////////////////////////////////////

void InspectorViewModel::SelectSector (int quarterTrack, int sectorIndex)
{
    const TrackAnalysis *  track = nullptr;



    m_quarterTrack = std::clamp (quarterTrack, 0, DiskImage::kQuarterTrackCount - 1);
    m_firstNibble  = -1;
    m_nibbleCount  = 0;
    track          = GetTrack();

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
//  The sector whose field holds the first nibble is selected with them.
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
        m_firstNibble = firstNibble;
        m_nibbleCount = std::max (nibbleCount, 1);
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
    m_zoom = 1.0;
    m_pan  = Point();
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorViewModel::PanBy
//
////////////////////////////////////////////////////////////////////////////////

void InspectorViewModel::PanBy (Point delta)
{
    m_pan.x += delta.x;
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
    static constexpr double  kTwoPi = 6.283185307179586;



    double  r     = GetRingRadius (quarterTrack);
    Point   point;



    point.x = m_pan.x + m_zoom * r * std::sin (kTwoPi * turn);
    point.y = m_pan.y - m_zoom * r * std::cos (kTwoPi * turn);

    return point;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorViewModel::GetPlatterView
//
////////////////////////////////////////////////////////////////////////////////

PlatterView InspectorViewModel::GetPlatterView (const RECT & boundsPx, double rotation) const
{
    PlatterView  view;
    double       fit = std::min (boundsPx.right - boundsPx.left, boundsPx.bottom - boundsPx.top) / 2.0;



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
//  The strip and, when zoomed, the platter pan the least that puts the
//  selection's start in view with a margin, keeping their zoom (FR-038). A
//  strip whose part of the turn already holds the selection stays put, so on
//  a new track it keeps the same part of the turn.
//
////////////////////////////////////////////////////////////////////////////////

void InspectorViewModel::BringSelectionIntoView()
{
    double  end    = 0;
    double  start  = GetSelectionTurn (end);
    double  offset = 0;
    double  limit  = 1.0 - kViewMargin;
    Point   point;



    offset = start - m_stripStart;
    offset = offset - std::floor (offset);

    if (start >= 0 && offset > m_stripSpan)
    {
        SetStrip (start - kViewMargin * m_stripSpan, m_stripSpan);
    }

    if (start >= 0 && !IsAtFit())
    {
        point = GetDiskPoint (m_quarterTrack, start);

        m_pan.x -= std::max (0.0, point.x - limit) + std::min (0.0, point.x + limit);
        m_pan.y -= std::max (0.0, point.y - limit) + std::min (0.0, point.y + limit);

        ClampPan();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorViewModel::ClampPan
//
//  Panning stops before the disk leaves the view: its center can move no
//  farther from the view's than the disk's radius past fit.
//
////////////////////////////////////////////////////////////////////////////////

void InspectorViewModel::ClampPan()
{
    double  reach = m_zoom - 1.0;



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
        last   = std::min (m_firstNibble + m_nibbleCount, static_cast<int> (track->framed.nibbles.size())) - 1;
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
