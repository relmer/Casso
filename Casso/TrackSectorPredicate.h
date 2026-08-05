#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TrackSectorPredicate
//
//  Parsed representation of an FR-014a (track) / FR-014b (sector)
//  filter expression entered into the Disk II Debug dialog's
//  RichEdit input. Grammar:
//
//     expr     := token (',' token)*
//     token    := value | range
//     range    := value '-' value
//     value    := decimal_int | decimal_qt          // qt only valid for tracks
//     decimal_qt := decimal_int '.' ('0'|'25'|'5'|'75')
//
//  Whitespace is allowed around tokens / separators. A malformed
//  token (e.g. "abc") is silently dropped from the predicate but its
//  half-open UTF-16 code-unit offset within the source expression is
//  recorded as a RejectedSpan so the dialog can red-squiggle just
//  that substring (FR-014e).
//
//  Semantics:
//    * An empty / whitespace-only source expression yields a
//      "match-all" predicate (Matches returns true for every input).
//      This is the natural default when the user hasn't filtered.
//    * A non-empty expression in which every token was rejected
//      yields a "match-nothing" predicate (zero ranges, m_matchAll
//      stays false). Per T027's explicit clarification: the
//      "empty = match all" rule applies only to literally empty
//      input, not to all-junk input.
//    * `rawQt = true` reinterprets bare integers as quarter-track
//      values (so `68` matches qt 68); decimal_qt tokens are
//      unaffected. `rawQt = false` treats bare integers as
//      whole-track values.
//    * NO max-value validation. `999` parses as `{999, 999}` and
//      simply fails to match in practice (FR-014a).
//
////////////////////////////////////////////////////////////////////////////////

class TrackSectorPredicate
{
public:
    static constexpr int  kQuarterTracksPerTrack = 4;

    // Disk II geometry caps for the FR-014a / FR-014b validators.
    // Standard DOS 3.3: 35 tracks. With quarter-track expansion the
    // physical head can reach track 39.75 (160 quarter-tracks total
    // numbered 0..159). The parser rejects any whole-track value >=
    // kMaxWholeTrackExclusive and any quarter-track value >=
    // kMaxQuarterTrackExclusive; sectors are capped at
    // kMaxSectorExclusive (DOS 3.3 = 0..15).
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

    // Spec-006 bug fix. `mode` selects the per-value validator (track
    // / sector). The third `rawQt` arg is meaningful only when
    // mode == Track; when mode == Sector the parser ignores it.
    static TrackSectorPredicate    Parse (std::wstring_view expr, Mode mode, bool rawQt = false);

    bool                            Matches (int value) const noexcept;
    bool                            MatchesQuarterTrack (int qt) const noexcept;

    const std::vector<RejectedSpan> & RejectedSpans() const noexcept   { return m_rejected; }
    const std::vector<Range> &        Ranges() const noexcept   { return m_ranges; }
    bool                              IsMatchAll() const noexcept   { return m_matchAll; }

private:
    // Expression-parsing helpers. Every reader is Parse (directly or via
    // another of these), so they belong to the class rather than to the
    // translation unit.

    // Whitespace test that does NOT depend on the C locale. Mirrors
    // the FR-014a "tolerate surrounding whitespace" rule.
    static bool  IsAsciiSpace (wchar_t ch) noexcept;

    // Parses `whole '.' frac` into an integer quarter-track count
    // (whole * 4 + quarterIndex). Only the four exact fractional
    // forms "0", "25", "5", and "75" are accepted; anything else
    // rejects the whole token. Returns true on success.
    static bool  TryParseDecimalQt  (std::wstring_view tok, int & outQt)  noexcept;
    static bool  TryParseDecimalInt (std::wstring_view tok, int & outVal) noexcept;

    static bool  TryParseValue (std::wstring_view tok,
                                bool               rawQt,
                                int &              outVal,
                                bool &             outIsQt) noexcept;

    // Trims leading / trailing whitespace from [begin, end) and
    // returns the trimmed substring view into `expr`. Updates the
    // in/out offsets to reflect the trimmed half-open range so the
    // caller can record a RejectedSpan covering exactly the token
    // text (FR-014e: the span MUST cover only the token, not the
    // surrounding whitespace or comma separators).
    static std::wstring_view  TrimSpan (std::wstring_view expr, int & ioBegin, int & ioEnd);

    static int   ValueCap (Mode mode, bool isQt) noexcept;

    std::vector<Range>          m_ranges;
    std::vector<RejectedSpan>   m_rejected;
    bool                        m_matchAll = true;
};
