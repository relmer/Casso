#pragma once

#include "Ui/Debugger/Panes/HeatMapSections.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapBands
//
//  The heat map's address ranges down the map: the ranges the map is focused
//  on, or all of memory; the bands they are cut into at the sections in a
//  bank that has regions; and where each band's header and rows lie, in
//  pixels from the map's top. A cell is a place: its band and how far into
//  the band it is.
//
////////////////////////////////////////////////////////////////////////////////

class HeatMapBands
{
public:
    using Band = HeatMapSections::Band;

    //  Where a band lies in the map, in pixels from the map's top: its
    //  header, then its rows, each with `columns` addresses.
    struct Placed
    {
        int   columns   = 1;
        long  headerTop = 0;
        long  rowsTop   = 0;
        long  rows      = 0;
    };

    //  A cell: the band it is in, and how far into the band.
    struct Place
    {
        size_t  band   = 0;
        long    offset = 0;
    };

    //  The ranges the map is focused on; false when they are the ranges
    //  already set. Clear returns to all of memory, and is false when no
    //  ranges are set.
    bool   SetRanges (std::vector<Band> ranges);
    bool   Clear     ();

    //  The bands joined back into the ranges, for a new machine, whose ROM
    //  starts its sections elsewhere.
    void   Reset     ();

    //  The ranges cut into sections when the bank has regions, and whole when
    //  it does not; false when that did not change.
    bool   Sync      (bool canSplit, const std::string & machineId);

    //  Each band's header and rows, one after another down the map.
    void   Arrange   (int columns, long pitch, long headerPx, long sectionHeaderPx, long afterPx, long labelLinePx);

    bool                         IsSplit          () const { return m_isSplit; }
    bool                         HasRanges        () const { return m_hasRanges; }
    bool                         HasHeader        (size_t band) const { return m_hasRanges || (band < m_bands.size() && m_bands[band].isSection); }
    const std::vector<Band>    & GetAll           () const { return m_bands; }
    const std::vector<Placed>  & GetPlaced        () const { return m_placed; }
    long                         GetContentHeight () const { return m_contentH; }

    int                          GetRows          (int columns) const;
    long                         GetBandAt        (long y, long pitch) const;
    std::optional<Place>         GetPlaceOf       (Word address) const;
    Word                         GetAddressOf     (const Place & place) const;

private:
    std::vector<Band>     m_ranges    = { Band { {}, 0, HeatMapSections::kAddressCount } };   // the set's ranges, or all of memory, before sections cut them
    std::vector<Band>     m_bands     = { Band { {}, 0, HeatMapSections::kAddressCount } };
    std::vector<Placed>   m_placed;
    bool                  m_isSplit   = false;
    bool                  m_hasRanges = false;
    long                  m_contentH  = 0;
};
