#pragma once

#include "Pch.h"

#include "DiskImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DamagedMountReport
//
//  The text of the report shown when a damaged disk image is inserted: a
//  checksum that does not match, tracks that could not be read from the file,
//  or both. Built here rather than in the shell so the wording can be tested
//  without a window.
//
////////////////////////////////////////////////////////////////////////////////

class DamagedMountReport
{
public:
    static constexpr size_t  kMaxListedTracks = 8;

    // The body of the report. Empty when the image is not damaged.
    static wstring       FormatBody              (const DiskImage & image, const wstring & path);

    // One quarter track per damaged track, at the middle of the run of
    // quarter tracks the map points at it, in ascending order.
    static vector<int>   GetDamagedQuarterTracks (const DiskImage & image);

    // "track 3", "tracks 3 and 7.5", "tracks 3, 7.5, and 12", and past eight,
    // the first eight and how many more.
    static wstring       FormatTrackList         (const vector<int> & quarterTracks);

    static wstring       FormatTrackNumber       (int quarterTrack);
};
