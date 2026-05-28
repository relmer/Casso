#include "Pch.h"

#include "DriveLabelTruncation.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Constants
//
////////////////////////////////////////////////////////////////////////////////

static constexpr wchar_t  s_kchEllipsis = L'\u2026';





////////////////////////////////////////////////////////////////////////////////
//
//  TruncateToWidth
//
//  Binary-search the longest prefix `p` such that
//  measure (p + ellipsis) <= maxWidthPx. Returns the literal basename
//  when it fits. Returns just the ellipsis when even that doesn't fit.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring TruncateToWidth (
    std::wstring_view                                  basename,
    float                                              maxWidthPx,
    const std::function<float (std::wstring_view)>   & measure)
{
    std::wstring  ellipsisOnly (1, s_kchEllipsis);
    size_t        lo  = 0;
    size_t        hi  = 0;
    size_t        mid = 0;
    size_t        best = 0;



    if (!measure)
    {
        return std::wstring (basename);
    }

    if (basename.empty())
    {
        return std::wstring();
    }

    if (measure (basename) <= maxWidthPx)
    {
        return std::wstring (basename);
    }

    if (measure (ellipsisOnly) > maxWidthPx)
    {
        return ellipsisOnly;
    }

    lo   = 0;
    hi   = basename.size();
    best = 0;
    while (lo <= hi)
    {
        std::wstring  candidate;

        mid = lo + (hi - lo) / 2;
        candidate.assign (basename.substr (0, mid));
        candidate.push_back (s_kchEllipsis);

        if (measure (candidate) <= maxWidthPx)
        {
            best = mid;
            lo   = mid + 1;
        }
        else
        {
            if (mid == 0)
            {
                break;
            }
            hi = mid - 1;
        }
    }

    {
        std::wstring  out;
        out.assign (basename.substr (0, best));
        out.push_back (s_kchEllipsis);
        return out;
    }
}
