#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TrackSectorPredicate
//
//  A track or sector filter as typed into the Disk II event log pane:
//
//     expr       := token (',' token)*
//     token      := value | range
//     range      := value '-' value
//     value      := decimal_int | decimal_qt          // qt for tracks only
//     decimal_qt := decimal_int '.' ('0'|'25'|'5'|'75')
//
//  Whitespace is allowed around tokens and separators. A token that does
//  not parse, or has a value past the disk's geometry, is left out of the
//  predicate, and its UTF-16 span goes in the list of tokens not taken.
//
//    * Empty or blank text matches everything.
//    * Text with tokens, none of them taken, matches nothing.
//    * A bare number is a whole track and a decimal_qt token a quarter-track.
//      A range with one of each runs over the whole track's quarter-tracks:
//      17-18.25 starts at 17.0, and 17.5-18 ends at 18.75.
//    * Tracks run 0 to 39 (quarter-tracks 0 to 159), sectors 0 to 15. A
//      range with one end past the cap is clamped to it, and its token goes
//      in the list of those not taken as well.
//
////////////////////////////////////////////////////////////////////////////////

class TrackSectorPredicate
{
public:
    static constexpr int  kQuarterTracksPerTrack = 4;

    // The Disk II's geometry: DOS 3.3 uses 35 tracks, but the head travels to
    // track 39.75 (quarter-tracks 0 to 159); sectors run 0 to 15.
    static constexpr int  kMaxWholeTrackExclusive   = 40;
    static constexpr int  kMaxQuarterTrackExclusive = 160;
    static constexpr int  kMaxSectorExclusive       = 16;

    struct Range
    {
        int   lo;
        int   hi;
        bool  isQt;
    };

    struct RejectedSpan
    {
        int   beginUtf16;
        int   endUtf16;
    };

    enum class Mode : uint8_t
    {
        Sector = 0,    // bare ints capped at kMaxSectorExclusive
        Track  = 1,    // bare ints capped at kMaxWholeTrackExclusive; qt at kMaxQuarterTrackExclusive
    };

    TrackSectorPredicate() = default;

    // `mode` selects the caps a value is checked against.
    static TrackSectorPredicate    Parse (std::wstring_view expr, Mode mode);

    // A whole value against the whole-value ranges; quarter-track ranges
    // match nothing here.
    bool                            Matches (int value) const noexcept;

    // A quarter-track against a quarter-track range, or against a whole-track
    // range when it lies on one of that range's tracks.
    bool                            MatchesQuarterTrack (int qt) const noexcept;

    const std::vector<RejectedSpan> & GetRejectedSpans() const noexcept   { return m_rejected; }
    const std::vector<Range> &        GetRanges() const noexcept   { return m_ranges; }
    bool                              IsMatchAll() const noexcept   { return m_matchAll; }

private:
    // A whitespace test that does not depend on the C locale.
    static bool  IsAsciiSpace (wchar_t ch) noexcept;

    // Parses `whole '.' frac` into an integer quarter-track count
    // (whole * 4 + quarterIndex). Only the four exact fractional
    // forms "0", "25", "5", and "75" are accepted; with anything else
    // the whole token is left out. Returns true on success.
    static bool  TryParseDecimalQt  (std::wstring_view tok, int & outQt)  noexcept;
    static bool  TryParseDecimalInt (std::wstring_view tok, int & outVal) noexcept;

    static bool  TryParseValue (std::wstring_view tok,
                                int &              outVal,
                                bool &             outIsQt) noexcept;

    // Trims leading / trailing whitespace from [begin, end) and
    // returns the trimmed substring view into `expr`. Updates the
    // in/out offsets to reflect the trimmed half-open range so the
    // caller can record a RejectedSpan covering exactly the token
    // text, without the whitespace or commas around it.
    static std::wstring_view  TrimSpan (std::wstring_view expr, int & ioBegin, int & ioEnd);

    static int   GetValueCap (Mode mode, bool isQt) noexcept;

    std::vector<Range>          m_ranges;
    std::vector<RejectedSpan>   m_rejected;
    bool                        m_matchAll = true;
};
