#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapSections
//
//  How memory is cut into the ranges on the heat map, and the number of
//  addresses in a row of a range. Each range on the map is a band; sections
//  are the bands memory is cut into at the I/O, the slots and the ROM's
//  parts, each drawn in a box under its label.
//
////////////////////////////////////////////////////////////////////////////////

class HeatMapSections
{
public:
    //  A range drawn by itself on the map, under a header with its title
    //  and span.
    struct Band
    {
        std::wstring  title;
        Word          first     = 0;
        int           count     = 0;
        bool          isSection = false;   // drawn in a box, labeled by its region unless isTitled
        bool          isTitled  = false;   // a whole range of a set, labeled by its title and span

        bool operator== (const Band & other) const = default;
    };

    static constexpr int     kAddressCount = 0x10000;
    static constexpr int     kMinColumns   = 1;
    static constexpr int     kMaxColumns   = 1024;
    static constexpr int     kStreetPx     = 1;

    //  The most a row's cells are widened to fill the pane before the row
    //  takes twice the addresses instead, each a little narrower than the
    //  zoom's: a column stays between three quarters and one and a half times
    //  the zoom's width, except where kMaxColumns or a one-pixel cell stop it,
    //  or where a single cell at the zoom's width is wider than the pane.
    static constexpr double  kMaxStretch   = 1.5;

    //  Where the sections start: the I/O, each slot's I/O, each slot's ROM
    //  page, the expansion ROM, and the ROM or language card above it. The
    //  addresses are fixed, so banking changes a section's label and color
    //  and never the layout.
    static constexpr std::array<Word, 18>  kSectionStarts =
    {
        0xC000, 0xC080, 0xC090, 0xC0A0, 0xC0B0, 0xC0C0, 0xC0D0, 0xC0E0, 0xC0F0,
        0xC100, 0xC200, 0xC300, 0xC400, 0xC500, 0xC600, 0xC700, 0xC800, 0xD000,
    };

    static std::vector<Word>  GetSectionStarts (const std::string & machineId = {});
    static std::wstring       GetRamLabel      (Word address);
    static std::wstring       GetBandTitle     (const Band & band);
    static std::vector<Band>  SplitAtSections  (const std::vector<Band> & ranges, bool isAllMemory, const std::string & machineId = {});
    static int                GetColumnsFor    (long widthPx, long pitchPx);
    static int                GetColumnsOf     (int columns, int count);
};
