#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorFormat
//
//  How the disk inspector writes each kind of value, in one place (FR-006).
//  Track numbers, blocks, volumes, counts, cell positions, times and angles
//  are decimal, angles in degrees clockwise from the index. Sector numbers,
//  byte and nibble values and offsets are hex with a "$", except in byte
//  sequences such as marks, which are bare hex. Counts above 999 take a
//  thousands separator. Go to parses each value in the base it is shown in.
//
////////////////////////////////////////////////////////////////////////////////

class InspectorFormat
{
public:
    static std::wstring  FormatQuarterTrack     (int quarterTrack);
    static std::wstring  FormatQuarterTrackList (const vector<int> & quarterTracks);
    static std::wstring  FormatSector           (int sector);
    static std::wstring  FormatHexByte          (int value);
    static std::wstring  FormatHexOffset        (int offset);
    static std::wstring  FormatByteSequence     (std::span<const Byte> bytes);
    static std::wstring  FormatCount            (uint64_t count);
    static std::wstring  FormatDegrees          (double fractionOfTurn);
    static std::wstring  FormatMicroseconds     (double ticks);
    static std::wstring  FormatPercent          (double fraction);

    static bool          TryParseQuarterTrack   (std::wstring_view text, int & outQuarterTrack);
    static bool          TryParseHex            (std::wstring_view text, int & outValue);
    static bool          TryParseDecimal        (std::wstring_view text, int & outValue);
};
