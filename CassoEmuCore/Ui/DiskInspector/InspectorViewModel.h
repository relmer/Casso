#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/DiskAnalysis.h"
#include "Ui/DiskInspector/PlatterRenderer.h"
#include "Ui/DiskInspector/SelectionHistory.h"





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorViewModel
//
//  What the inspector window is looking at, kept apart from how it draws:
//  the selected quarter track, sector and nibbles; the platter's zoom and
//  pan; and the part of the turn the strip shows. It lasts only while the
//  window shows one disk (FR-080), so a new disk starts it over.
//
//  The platter's zoom and pan are in units of the fit radius: at zoom 1 the
//  disk's rim touches the view's edge, and a pan of (0, 0) centers it.
//  Positions along a track are fractions of a turn, clockwise from the index.
//
////////////////////////////////////////////////////////////////////////////////

class InspectorViewModel
{
public:
    static constexpr double  kMaxZoom          = 640.0;
    static constexpr double  kViewMargin       = 0.1;
    static constexpr double  kDefaultStripSpan = 0.125;

    //  A flight to a sector ends with its chord this share of the line
    //  through the view's center along it, from edge to edge; it lasts
    //  kFlightLegMs for each leg it has.
    static constexpr double  kFlightFill       = 0.75;
    static constexpr double  kFlightLegMs      = 400.0;

    //  The disk's edge sits this far inside the view, so the view's bounds
    //  clip neither the disk nor the marks drawn across its outermost ring:
    //  the selected sector's outline and the selected file's marquee.
    static constexpr double  kRimInsetPx       = 10.0;

    struct Point
    {
        double  x = 0.0;
        double  y = 0.0;
    };

    //  The platter's zoom and pan.
    struct View
    {
        double  zoom = 1.0;
        Point   pan;
    };

    //  Disk and analysis.
    void  SetDisk     (uint64_t mediaId);
    void  SetAnalysis (const DiskAnalysis * analysis);

    //  Selection.
    void  SelectQuarterTrack (int quarterTrack);
    void  SelectSector       (int quarterTrack, int sectorIndex);
    void  SelectNibbles      (int quarterTrack, int firstNibble, int nibbleCount);

    //  Nothing selected on the quarter track: no sector, nibbles or bytes.
    void  ClearSelection     ();

    //  Ranges (FR-043): from the nibble or byte last clicked to another, all
    //  of the track's nibbles, or the sector's bytes in the hex or the text
    //  column.
    void  ExtendNibbles      (int toNibble);
    void  SelectAllNibbles   ();
    void  SelectBytes        (int firstByte, int byteCount, bool isTextColumn);
    void  ExtendBytes        (int toByte);
    void  SelectAllBytes     ();

    //  The selection as a whole, and the history of the user's selections
    //  for Back and Forward (FR-164).
    InspectorSelection  GetSelection     () const;
    void                RestoreSelection (const InspectorSelection & selection);
    SelectionHistory &  GetHistory       () { return m_history; }

    int                    GetQuarterTrack     () const { return m_quarterTrack; }
    int                    GetSectorIndex      () const { return m_sectorIndex; }
    int                    GetFirstNibble      () const { return m_firstNibble; }
    int                    GetNibbleCount      () const { return m_nibbleCount; }
    int                    GetFirstByte        () const { return m_firstByte; }
    int                    GetByteCount        () const { return m_byteCount; }
    bool                   IsTextColumn        () const { return m_isTextColumn; }
    int                    GetNibbleAnchor     () const { return m_nibbleAnchor; }
    int                    GetByteAnchor       () const { return m_byteAnchor; }
    const TrackAnalysis *  GetTrack            () const;
    const AnalyzedSector * GetSector           () const;

    //  Platter zoom and pan.
    void    ZoomAbout       (double zoom, Point anchor);
    void    Fit             ();
    void    PanBy           (Point delta);
    bool    IsAtFit         () const { return m_zoom <= 1.0; }
    double  GetZoom         () const { return m_zoom; }
    Point   GetPan          () const { return m_pan; }
    Point   GetDiskPoint    (int quarterTrack, double turn) const;
    View    GetView         () const { return { m_zoom, m_pan }; }

    //  How far the drive has turned the platter, as a fraction of a turn
    //  clockwise. Every turn on screen is shifted by it (FR-064).
    double  GetRotation     () const { return m_rotation; }
    void    SetRotation     (double rotation) { m_rotation = rotation; }

    //  How far the platter is tilted back from flat, in radians, up to
    //  kMaxTilt (FR-151).
    static constexpr double  kMaxTilt = 1.0471975511965976;

    double  GetTilt         () const { return m_tilt; }
    void    SetTilt         (double tilt) { m_tilt = std::clamp (tilt, 0.0, kMaxTilt); }
    bool    IsTilted        () const { return m_tilt > 0.0; }

    //  Where the head's angle sits, as a fraction of a turn from 12 o'clock:
    //  0 flat, 0.5 at the near edge tilted (FR-152). When the platter tilts
    //  or lies flat, StepTiltTurn eases it to the other over kTiltTurnMs and
    //  turns the platter at fit with it, so the head sweeps from one place to
    //  the other.
    static constexpr double  kTiltTurnMs = 300.0;

    double  GetHeadSide     () const { return m_headSide; }
    void    StepTiltTurn    (double nowMs);

    //  A point on a ring at fit, zoom 1 and no pan, as the tilt draws it.
    Point   GetFitPoint     (int quarterTrack, double turn) const;

    //  Tilted, the disk at fit is drawn smaller by a scale and moved down
    //  by an offset in fit units, so all of its projection is in view.
    void    GetTiltFit      (double & outScale, double & outOffsetY) const;

    //  The view that centers the chord from `from` to `to`, given at fit, at
    //  the zoom that makes it kFlightFill of the line through the view's
    //  center along it, from edge to edge.
    static View   GetChordView (Point from, Point to);

    //  A view `t` of the way from `from` to `to`, 0 to 1: a zoom about the
    //  one point both views put in the same place, or a pan between two
    //  views at one zoom.
    static View   GetBetweenView (const View & from, const View & to, double t);

    //  A flight zooms the platter out until both the view's center and the
    //  selection are in view, then in to the selection (FR-038). ZoomAbout, PanBy, Fit and StopFlight end the flight.
    void    StartFlight     (double nowMs);
    bool    StepFlight      (double nowMs);
    void    StopFlight      () { m_isFlying = false; }
    bool    IsFlying        () const { return m_isFlying; }
    PlatterPlacement  GetPlacement (const RECT & boundsPx) const;

    //  Strip.
    double  GetStripStart   () const { return m_stripStart; }
    double  GetStripSpan    () const { return m_stripSpan; }
    void    SetStrip        (double start, double span);

    static double  GetRingRadius (int quarterTrack);

    //  Where the selection starts and ends as fractions of the turn, or -1
    //  with nothing selected.
    double  GetSelectionTurn (double & outEnd) const;

private:
    void    SelectFirstSector      ();
    void    FitStripToTrack        ();
    void    ClearBytes             ();
    void    BringSelectionIntoView ();
    void    ClampPan               ();
    static double  Ease            (double t);

    const DiskAnalysis *  m_analysis      = nullptr;
    uint64_t              m_mediaId       = 0;
    int                   m_quarterTrack  = 0;
    int                   m_sectorIndex   = -1;
    int                   m_firstNibble   = -1;
    int                   m_nibbleCount   = 0;
    int                   m_nibbleAnchor  = -1;
    int                   m_firstByte     = -1;
    int                   m_byteCount     = 0;
    int                   m_byteAnchor    = -1;
    bool                  m_isTextColumn  = false;
    uint32_t              m_anchorCell    = 0;
    bool                  m_hasAnchor     = false;
    double                m_zoom          = 1.0;
    double                m_rotation      = 0.0;
    double                m_tilt          = 0.0;
    Point                 m_pan;
    double                m_stripStart    = 0.0;
    double                m_stripSpan     = kDefaultStripSpan;
    bool                  m_isStripWhole  = false;
    bool                  m_isStripFitted = false;
    View                  m_flightFrom;
    View                  m_flightTo;
    View                  m_flightMid;
    bool                  m_isTwoLegs     = false;
    double                m_flightStartMs = 0.0;
    bool                  m_isFlying      = false;
    double                m_headSide      = 0.0;
    double                m_headSideFrom  = 0.0;
    double                m_headSideTo    = 0.0;
    double                m_tiltTurnMs    = -1.0;
    SelectionHistory      m_history;
};
