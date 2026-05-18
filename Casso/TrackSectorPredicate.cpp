#include "Pch.h"

#include "TrackSectorPredicate.h"



namespace
{
    ////////////////////////////////////////////////////////////////////////////
    //
    //  IsAsciiSpace
    //
    //  Whitespace test that does NOT depend on the C locale. Mirrors
    //  the FR-014a "tolerate surrounding whitespace" rule.
    //
    ////////////////////////////////////////////////////////////////////////////

    bool IsAsciiSpace (wchar_t ch) noexcept
    {
        return ch == L' '  ||
               ch == L'\t' ||
               ch == L'\r' ||
               ch == L'\n';
    }



    ////////////////////////////////////////////////////////////////////////////
    //
    //  ParseDecimalQt
    //
    //  Parses `whole '.' frac` into an integer quarter-track count
    //  (whole * 4 + quarterIndex). Only the four exact fractional
    //  forms "0", "25", "5", and "75" are accepted; anything else
    //  rejects the whole token. Returns true on success.
    //
    ////////////////////////////////////////////////////////////////////////////

    bool ParseDecimalQt (std::wstring_view tok, int & outQt) noexcept
    {
        size_t  dot      = 0;
        int     whole    = 0;
        int     qtIndex  = -1;
        size_t  i        = 0;
        wchar_t c        = 0;

        dot = tok.find (L'.');

        if (dot == std::wstring_view::npos || dot == 0)
        {
            return false;
        }

        for (i = 0; i < dot; i++)
        {
            c = tok[i];

            if (c < L'0' || c > L'9')
            {
                return false;
            }

            whole = whole * 10 + (int) (c - L'0');
        }

        std::wstring_view frac = tok.substr (dot + 1);

        if      (frac == L"0")  { qtIndex = 0; }
        else if (frac == L"25") { qtIndex = 1; }
        else if (frac == L"5")  { qtIndex = 2; }
        else if (frac == L"75") { qtIndex = 3; }
        else                    { return false; }

        outQt = whole * TrackSectorPredicate::kQuarterTracksPerTrack + qtIndex;

        return true;
    }



    ////////////////////////////////////////////////////////////////////////////
    //
    //  ParseDecimalInt
    //
    //  Strict non-negative decimal integer. Returns true on success.
    //
    ////////////////////////////////////////////////////////////////////////////

    bool ParseDecimalInt (std::wstring_view tok, int & outVal) noexcept
    {
        int     v = 0;
        size_t  i = 0;
        wchar_t c = 0;

        if (tok.empty())
        {
            return false;
        }

        for (i = 0; i < tok.size(); i++)
        {
            c = tok[i];

            if (c < L'0' || c > L'9')
            {
                return false;
            }

            v = v * 10 + (int) (c - L'0');
        }

        outVal = v;
        return true;
    }



    ////////////////////////////////////////////////////////////////////////////
    //
    //  ParseValue
    //
    //  Parses either a bare integer or a decimal quarter-track value.
    //  Output is in quarter-track units when `isQt` comes back true,
    //  otherwise in the natural units of the surrounding token.
    //
    ////////////////////////////////////////////////////////////////////////////

    bool ParseValue (std::wstring_view tok,
                     bool               rawQt,
                     int &              outVal,
                     bool &             outIsQt) noexcept
    {
        int  qt    = 0;
        int  val   = 0;

        if (ParseDecimalQt (tok, qt))
        {
            outVal  = qt;
            outIsQt = true;
            return true;
        }

        if (ParseDecimalInt (tok, val))
        {
            if (rawQt)
            {
                outVal  = val;
                outIsQt = true;
            }
            else
            {
                outVal  = val;
                outIsQt = false;
            }

            return true;
        }

        return false;
    }



    ////////////////////////////////////////////////////////////////////////////
    //
    //  TrimSpan
    //
    //  Trims leading / trailing whitespace from [begin, end) and
    //  returns the trimmed substring view into `expr`. Updates the
    //  in/out offsets to reflect the trimmed half-open range so the
    //  caller can record a RejectedSpan covering exactly the token
    //  text (FR-014e: the span MUST cover only the token, not the
    //  surrounding whitespace or comma separators).
    //
    ////////////////////////////////////////////////////////////////////////////

    std::wstring_view TrimSpan (std::wstring_view expr, int & ioBegin, int & ioEnd)
    {
        while (ioBegin < ioEnd && IsAsciiSpace (expr[(size_t) ioBegin]))
        {
            ioBegin++;
        }

        while (ioEnd > ioBegin && IsAsciiSpace (expr[(size_t) (ioEnd - 1)]))
        {
            ioEnd--;
        }

        return expr.substr ((size_t) ioBegin, (size_t) (ioEnd - ioBegin));
    }



    ////////////////////////////////////////////////////////////////////////////
    //
    //  ValueCap
    //
    //  Returns the exclusive upper bound for a parsed value given the
    //  parser mode and whether the value is a quarter-track. Sector
    //  mode caps at 16 (DOS 3.3); track mode caps at 40 whole or 160
    //  quarter-tracks. Spec-006 bug fix: any value >= the cap is
    //  rejected (RejectedSpan recorded) so the dialog can squiggle it.
    //
    ////////////////////////////////////////////////////////////////////////////

    int ValueCap (TrackSectorPredicate::Mode mode, bool isQt) noexcept
    {
        if (mode == TrackSectorPredicate::Mode::Sector)
        {
            return TrackSectorPredicate::kMaxSectorExclusive;
        }

        return isQt
                   ? TrackSectorPredicate::kMaxQuarterTrackExclusive
                   : TrackSectorPredicate::kMaxWholeTrackExclusive;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Parse
//
//  Tokenizes the expression on commas, trims each token, classifies
//  it as a single value or a `lo-hi` range, and accumulates the
//  matching Range entry. Malformed tokens are silently dropped but
//  their original UTF-16 [begin, end) offsets are recorded so the
//  FR-014e squiggle layer can highlight them.
//
////////////////////////////////////////////////////////////////////////////////

TrackSectorPredicate TrackSectorPredicate::Parse (std::wstring_view expr, Mode mode, bool rawQt)
{
    TrackSectorPredicate   pred;
    int                    cursor       = 0;
    int                    exprEnd      = (int) expr.size();
    int                    tokStart     = 0;
    int                    trimmedBegin = 0;
    int                    trimmedEnd   = 0;
    bool                   sawAnyToken  = false;
    bool                   isQt         = false;
    bool                   hiIsQt       = false;
    int                    val          = 0;
    int                    hiVal        = 0;
    size_t                 dashPos      = 0;
    int                    cap          = 0;

    while (cursor <= exprEnd)
    {
        if (cursor == exprEnd || expr[(size_t) cursor] == L',')
        {
            trimmedBegin = tokStart;
            trimmedEnd   = cursor;

            std::wstring_view trimmed = TrimSpan (expr, trimmedBegin, trimmedEnd);

            if (!trimmed.empty())
            {
                sawAnyToken = true;
                dashPos     = trimmed.find (L'-');

                if (dashPos != std::wstring_view::npos && dashPos > 0)
                {
                    std::wstring_view loStr = trimmed.substr (0, dashPos);
                    std::wstring_view hiStr = trimmed.substr (dashPos + 1);

                    // Trim each half individually so "0 - 2" works.
                    while (!loStr.empty() && IsAsciiSpace (loStr.back()))  { loStr.remove_suffix (1); }
                    while (!hiStr.empty() && IsAsciiSpace (hiStr.front())) { hiStr.remove_prefix (1); }

                    if (ParseValue (loStr, rawQt, val,   isQt)   &&
                        ParseValue (hiStr, rawQt, hiVal, hiIsQt) &&
                        isQt == hiIsQt)
                    {
                        cap = ValueCap (mode, isQt);

                        // Both endpoints in range: accept the range.
                        // One endpoint out of range: clamp the in-
                        // range endpoint into [0, cap) and record a
                        // RejectedSpan so the dialog squiggles the
                        // whole token. Both out of range: drop.
                        if (val >= 0 && val < cap && hiVal >= 0 && hiVal < cap)
                        {
                            pred.m_ranges.push_back ({ val, hiVal, isQt });
                        }
                        else if (val >= 0 && val < cap)
                        {
                            pred.m_ranges.push_back ({ val, cap - 1, isQt });
                            pred.m_rejected.push_back ({ trimmedBegin, trimmedEnd });
                        }
                        else if (hiVal >= 0 && hiVal < cap)
                        {
                            pred.m_ranges.push_back ({ 0, hiVal, isQt });
                            pred.m_rejected.push_back ({ trimmedBegin, trimmedEnd });
                        }
                        else
                        {
                            pred.m_rejected.push_back ({ trimmedBegin, trimmedEnd });
                        }
                    }
                    else
                    {
                        pred.m_rejected.push_back ({ trimmedBegin, trimmedEnd });
                    }
                }
                else
                {
                    if (ParseValue (trimmed, rawQt, val, isQt))
                    {
                        cap = ValueCap (mode, isQt);

                        if (val >= 0 && val < cap)
                        {
                            pred.m_ranges.push_back ({ val, val, isQt });
                        }
                        else
                        {
                            pred.m_rejected.push_back ({ trimmedBegin, trimmedEnd });
                        }
                    }
                    else
                    {
                        pred.m_rejected.push_back ({ trimmedBegin, trimmedEnd });
                    }
                }
            }

            tokStart = cursor + 1;
        }

        cursor++;
    }

    // FR-014a default: only a literally empty / whitespace-only
    // expression matches everything. An expression with tokens (even
    // if all were rejected) does NOT match everything.
    pred.m_matchAll = !sawAnyToken;

    return pred;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Matches
//
//  Whole-value match path. Used by the sector filter and by the
//  track filter when the user is not in raw-quarter-track mode and
//  the predicate ranges are whole-track. Quarter-track ranges
//  (entered as 17.25 etc.) are skipped here; use
//  MatchesQuarterTrack for those.
//
////////////////////////////////////////////////////////////////////////////////

bool TrackSectorPredicate::Matches (int value) const noexcept
{
    size_t  i = 0;

    if (m_matchAll)
    {
        return true;
    }

    for (i = 0; i < m_ranges.size(); i++)
    {
        const Range & r = m_ranges[i];

        if (r.isQt)
        {
            continue;
        }

        if (value >= r.lo && value <= r.hi)
        {
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MatchesQuarterTrack
//
//  Quarter-track match path. A predicate Range with isQt=true
//  compares directly against `qt`; a whole-track Range matches when
//  qt's whole-track index falls within [lo, hi]. The latter is what
//  lets `17` match any quarter-track on track 17 (qt 68..71).
//
////////////////////////////////////////////////////////////////////////////////

bool TrackSectorPredicate::MatchesQuarterTrack (int qt) const noexcept
{
    size_t  i        = 0;
    int     trackIdx = 0;

    if (m_matchAll)
    {
        return true;
    }

    trackIdx = qt / kQuarterTracksPerTrack;

    for (i = 0; i < m_ranges.size(); i++)
    {
        const Range & r = m_ranges[i];

        if (r.isQt)
        {
            if (qt >= r.lo && qt <= r.hi)
            {
                return true;
            }
        }
        else
        {
            if (trackIdx >= r.lo && trackIdx <= r.hi)
            {
                return true;
            }
        }
    }

    return false;
}
