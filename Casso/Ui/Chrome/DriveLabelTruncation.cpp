#include "Pch.h"

#include "DriveLabelTruncation.h"

#include "Core/UnicodeSymbols.h"





////////////////////////////////////////////////////////////////////////////////
//
//  LongestFittingPrefixLen
//
//  Binary-search the longest prefix length `n` of `basename` such that
//  measure(basename[0..n] + ellipsis) <= maxWidthPx. Precondition:
//  caller has already verified that ellipsisOnly fits and basename
//  alone does not.
//
////////////////////////////////////////////////////////////////////////////////

static size_t LongestFittingPrefixLen (
    std::wstring_view                                  basename,
    float                                              maxWidthPx,
    const std::function<float (std::wstring_view)>   & measure)
{
    size_t        lo        = 0;
    size_t        hi        = basename.size();
    size_t        mid       = 0;
    size_t        best      = 0;
    std::wstring  candidate;
    float         widthPx   = 0.0f;
    bool          fits      = false;



    while (lo <= hi)
    {
        mid = lo + (hi - lo) / 2;

        candidate.assign (basename.substr (0, mid));
        candidate.push_back (s_kchEllipsis);

        widthPx = measure (candidate);
        fits    = (widthPx <= maxWidthPx);

        if (fits)
        {
            best = mid;
            lo   = mid + 1;
        }
        else if (mid == 0)
        {
            lo = hi + 1;   // force loop exit (single-exit pattern)
        }
        else
        {
            hi = mid - 1;
        }
    }

    return best;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TruncateToWidth
//
//  Returns the literal basename when it fits, just the ellipsis when
//  even that doesn't fit, otherwise the longest prefix + ellipsis that
//  fits.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring TruncateToWidth (
    std::wstring_view                                  basename,
    float                                              maxWidthPx,
    const std::function<float (std::wstring_view)>   & measure)
{
    std::wstring  result;
    std::wstring  ellipsisOnly (1, s_kchEllipsis);
    size_t        bestPrefixLen = 0;



    if (!measure)
    {
        result.assign (basename);
    }
    else if (basename.empty())
    {
        // result stays empty
    }
    else if (measure (basename) <= maxWidthPx)
    {
        result.assign (basename);
    }
    else if (measure (ellipsisOnly) > maxWidthPx)
    {
        result = ellipsisOnly;
    }
    else
    {
        bestPrefixLen = LongestFittingPrefixLen (basename, maxWidthPx, measure);
        result.assign (basename.substr (0, bestPrefixLen));
        result.push_back (s_kchEllipsis);
    }

    return result;
}
