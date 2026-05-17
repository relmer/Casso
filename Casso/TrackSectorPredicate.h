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

    TrackSectorPredicate () = default;

    static TrackSectorPredicate    Parse           (std::wstring_view expr, bool rawQt);

    bool                            Matches             (int value) const noexcept;
    bool                            MatchesQuarterTrack (int qt) const noexcept;

    const std::vector<RejectedSpan> & RejectedSpans () const noexcept   { return m_rejected; }
    const std::vector<Range> &        Ranges        () const noexcept   { return m_ranges; }
    bool                              IsMatchAll    () const noexcept   { return m_matchAll; }

private:
    std::vector<Range>          m_ranges;
    std::vector<RejectedSpan>   m_rejected;
    bool                        m_matchAll = true;
};
