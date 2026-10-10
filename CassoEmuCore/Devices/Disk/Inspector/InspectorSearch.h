#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/DiskAnalysis.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SearchKind / SearchItem / SearchQuery / SearchHit
//
//  What Find looks for (FR-056): a hex nibble pattern, hex bytes in decoded
//  sector data, or text in sector data. A pattern is a run of items, each a
//  value under a mask, so ? leaves a hex digit out of the comparison, and a
//  nibble item can require extra zero cells after it. A hit is a run of
//  nibbles, a stretch of cells not aligned to the framed nibbles, or a run
//  of bytes in a sector's data.
//
////////////////////////////////////////////////////////////////////////////////

enum class SearchKind
{
    Nibbles,
    SectorHex,
    SectorText,
};


struct SearchItem
{
    Byte  value           = 0;
    Byte  mask            = 0xFF;
    bool  needsExtraZeros = false;
};


struct SearchQuery
{
    SearchKind    kind           = SearchKind::Nibbles;
    std::wstring  text;
    bool          isWholeDisk    = false;
    bool          isAnyBitOffset = false;
};


struct SearchHit
{
    int                    quarterTrack = -1;
    const TrackAnalysis *  track        = nullptr;
    int                    firstNibble  = -1;
    int                    nibbleCount  = 0;
    uint32_t               cell         = 0;
    bool                   isAligned    = true;
    int                    sectorIndex  = -1;
    int                    offset       = -1;
    int                    length       = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorSearch
//
//  Find over the selected track or the whole disk, each record once, at the
//  first quarter track that plays it. "Any bit offset" frames the cells again
//  from every 1 cell by the latch's rule, so it also finds a pattern that
//  starts inside a framed nibble. Pure.
//
////////////////////////////////////////////////////////////////////////////////

class InspectorSearch
{
public:
    static bool               ParsePattern (const std::wstring & text, SearchKind kind, vector<SearchItem> & outItems);
    static vector<SearchHit>  Find         (const DiskAnalysis & analysis, int quarterTrack, const SearchQuery & query);
    static void               FindInTrack  (const TrackAnalysis & track, int quarterTrack, const vector<SearchItem> & items, const SearchQuery & query,
                                            vector<SearchHit> & inOutHits);

    //  A hit's place, as "Track 17.25, nibble $1A3" or "Track 3, sector $5
    //  at $2C".
    static std::wstring       FormatHit    (const SearchHit & hit);
    static std::wstring       FormatHitCount (size_t count);

    //  Whether the record a hit was found in has been analyzed again since.
    static bool               IsOutOfDate  (const SearchHit & hit, const DiskAnalysis & analysis);

private:
    static void  FindAligned   (const TrackAnalysis & track, int quarterTrack, const vector<SearchItem> & items, vector<SearchHit> & inOutHits);
    static void  FindAnyBit    (const TrackAnalysis & track, int quarterTrack, const vector<SearchItem> & items, vector<SearchHit> & inOutHits);
    static void  FindInSectors (const TrackAnalysis & track, int quarterTrack, const vector<SearchItem> & items, bool isText, vector<SearchHit> & inOutHits);
    static bool  IsMatch       (Byte value, const SearchItem & item, bool isText);
};





////////////////////////////////////////////////////////////////////////////////
//
//  GoToKind / GoToTarget
//
//  What "Go to" selects (FR-055), each read in the base the inspector shows
//  it in: a track as 17.25, a physical or DOS 3.3 sector in hex, a ProDOS
//  block and a cell in decimal, a nibble offset in hex.
//
////////////////////////////////////////////////////////////////////////////////

enum class GoToKind
{
    Track,
    PhysicalSector,
    Dos33Sector,
    ProDosBlock,
    NibbleOffset,
    Cell,
};


struct GoToTarget
{
    int  quarterTrack = -1;
    int  sectorIndex  = -1;
    int  firstNibble  = -1;
};





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorGoTo
//
//  Reads a "Go to" entry and finds what it selects, from the selected track
//  for a sector, nibble or cell, and from the block's own track for a ProDOS
//  block. Says why when there is nothing there. Pure.
//
////////////////////////////////////////////////////////////////////////////////

class InspectorGoTo
{
public:
    static bool  Resolve (const DiskAnalysis & analysis, int quarterTrack, GoToKind kind, const std::wstring & text, GoToTarget & outTarget,
                          std::wstring & outError);

    static LPCWSTR  GetLabel (GoToKind kind);

private:
    static const TrackAnalysis *  GetTrack (const DiskAnalysis & analysis, int quarterTrack);
    static int  FindSector (const TrackAnalysis & track, GoToKind kind, int value);
};
