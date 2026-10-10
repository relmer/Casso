#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/DiskAnalysis.h"
#include "Ui/DiskInspector/PlatterRenderer.h"





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

    struct Point
    {
        double  x = 0.0;
        double  y = 0.0;
    };

    //  Disk and analysis.
    void  SetDisk     (uint64_t mediaId);
    void  SetAnalysis (const DiskAnalysis * analysis);

    //  Selection.
    void  SelectQuarterTrack (int quarterTrack);
    void  SelectSector       (int quarterTrack, int sectorIndex);
    void  SelectNibbles      (int quarterTrack, int firstNibble, int nibbleCount);

    //  Ranges (FR-043): from the nibble or byte last clicked to another, all
    //  of the track's nibbles, or the sector's bytes in the hex or the text
    //  column.
    void  ExtendNibbles      (int toNibble);
    void  SelectAllNibbles   ();
    void  SelectBytes        (int firstByte, int byteCount, bool isTextColumn);
    void  ExtendBytes        (int toByte);
    void  SelectAllBytes     ();

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
    PlatterPlacement  GetPlacement (const RECT & boundsPx, double rotation) const;

    //  Strip.
    double  GetStripStart   () const { return m_stripStart; }
    double  GetStripSpan    () const { return m_stripSpan; }
    void    SetStrip        (double start, double span);

    static double  GetRingRadius (int quarterTrack);

private:
    void    SelectFirstSector      ();
    void    ClearBytes             ();
    void    BringSelectionIntoView ();
    void    ClampPan               ();
    double  GetSelectionTurn       (double & outEnd) const;

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
    Point                 m_pan;
    double                m_stripStart    = 0.0;
    double                m_stripSpan     = kDefaultStripSpan;
};
