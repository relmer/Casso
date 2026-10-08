#include "Pch.h"

#include "Core/DxuiTextElide.h"

#include "Core/UnicodeSymbols.h"
#include "Render/IDxuiTextRenderer.h"




static const wchar_t   s_kEllipsis[]  = { s_kchEllipsis, L'\0' };
static constexpr wchar_t  s_kSeparator = L'\\';





////////////////////////////////////////////////////////////////////////////////
//
//  Fits
//
//  Whether `candidate` measures within the budget.
//
//  A failed measure counts as FITTING: the alternative is trimming a string
//  on the strength of a number the renderer did not actually produce, which
//  turns a transient device problem into visibly wrong text.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextElide::Fits (IDxuiTextRenderer  & text,
                          const std::wstring & candidate,
                          float                fontDip,
                          const wchar_t      * fontFamily,
                          float                maxWidthDip,
                          bool                 gdiWidths)
{
    HRESULT   hr = S_OK;
    float     w  = 0.0f;
    float     h  = 0.0f;



    hr = gdiWidths ? text.MeasureStringGdi (candidate.c_str(), fontDip, fontFamily, w)
                   : text.MeasureString    (candidate.c_str(), fontDip, fontFamily, w, h);

    if (FAILED (hr))
    {
        return true;
    }

    return w <= maxWidthDip;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ElideTail
//
//  Longest PREFIX that fits once the ellipsis is appended.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DxuiTextElide::ElideTail (IDxuiTextRenderer  & text,
                                       const std::wstring & value,
                                       float                fontDip,
                                       const wchar_t      * fontFamily,
                                       float                maxWidthDip,
                                       bool                 gdiWidths)
{
    size_t   lo  = 0;
    size_t   hi  = value.size();
    size_t   mid = 0;



    while (lo < hi)
    {
        mid = (lo + hi + 1) / 2;

        if (Fits (text, value.substr (0, mid) + s_kEllipsis, fontDip, fontFamily, maxWidthDip, gdiWidths))
        {
            lo = mid;
        }
        else
        {
            hi = mid - 1;
        }
    }

    // Not even one character plus the ellipsis fits: the ellipsis alone still
    // says "there is more here", where an empty box says the value is unset.
    return (lo == 0) ? std::wstring (s_kEllipsis)
                     : value.substr (0, lo) + s_kEllipsis;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WrapToLines
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> DxuiTextElide::WrapToLines (IDxuiTextRenderer  & text,
                                                      const std::wstring & value,
                                                      float                fontDip,
                                                      const wchar_t      * fontFamily,
                                                      float                maxWidthDip,
                                                      int                  maxLines,
                                                      bool                 ellipsis,
                                                      bool                 gdiWidths)
{
    std::vector<std::wstring>  lines;
    std::wstring               rest = value;



    while (!rest.empty() && (int) lines.size() < maxLines)
    {
        size_t  lo    = 1;
        size_t  hi    = rest.size();
        size_t  mid   = 0;
        size_t  cut   = 0;

        if ((int) lines.size() == maxLines - 1 || Fits (text, rest, fontDip, fontFamily, maxWidthDip, gdiWidths))
        {
            //  A rest that fits by the measure the breaks use is kept whole.
            bool  whole = gdiWidths && Fits (text, rest, fontDip, fontFamily, maxWidthDip, true);

            lines.push_back (whole ? rest : ToWidth (text, rest, fontDip, fontFamily, maxWidthDip, ellipsis ? DxuiElide::Tail : DxuiElide::None, gdiWidths));
            break;
        }

        //  The longest start of what is left that fits the width.
        while (lo < hi)
        {
            mid = (lo + hi + 1) / 2;

            if (Fits (text, rest.substr (0, mid), fontDip, fontFamily, maxWidthDip, gdiWidths))
            {
                lo = mid;
            }
            else
            {
                hi = mid - 1;
            }
        }

        //  Back to the last space, as Explorer breaks its names; a word with
        //  no space in reach fills the line and goes on in the next,
        //  periods, hyphens and underscores included. A word that fits up to
        //  the space after it stays: the space may hang past the edge.
        cut = (lo < rest.size() && rest[lo] == L' ') ? lo : rest.find_last_of (L' ', lo - 1);
        cut = (cut != std::wstring::npos && cut > 0) ? cut + 1 : lo;

        lines.push_back (rest.substr (0, cut));
        rest = rest.substr (cut);

        while (!lines.back().empty() && lines.back().back() == L' ')
        {
            lines.back().pop_back();
        }

        while (!rest.empty() && rest.front() == L' ')
        {
            rest.erase (rest.begin());
        }
    }

    return lines;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ElidePathHead
//
//  Longest SUFFIX that fits once the ellipsis is prepended, then snapped
//  forward to a separator.
//
//  Snapping only ever SHORTENS the result, so a cut that fits still fits
//  afterwards -- which is why the search can ignore separators entirely and
//  the boundary rule can be applied once at the end.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DxuiTextElide::ElidePathHead (IDxuiTextRenderer  & text,
                                           const std::wstring & value,
                                           float                fontDip,
                                           const wchar_t      * fontFamily,
                                           float                maxWidthDip)
{
    size_t   lo   = 0;
    size_t   hi   = value.size();
    size_t   mid  = 0;
    size_t   snap = std::wstring::npos;



    //  Search for the smallest start index whose suffix fits.
    while (lo < hi)
    {
        mid = (lo + hi) / 2;

        if (Fits (text, s_kEllipsis + value.substr (mid), fontDip, fontFamily, maxWidthDip))
        {
            hi = mid;
        }
        else
        {
            lo = mid + 1;
        }
    }

    if (lo >= value.size())
    {
        return std::wstring (s_kEllipsis);
    }

    //  Prefer a component boundary at or after the fitting point. Later means
    //  shorter, so this cannot overflow what the search just established.
    snap = value.find (s_kSeparator, lo);

    if (snap != std::wstring::npos)
    {
        lo = snap;
    }

    return s_kEllipsis + value.substr (lo);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ToWidth
//
//  The entry point. Returns `value` untouched when it already fits, when the
//  budget is nonsense, or when no elision was asked for.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DxuiTextElide::ToWidth (IDxuiTextRenderer  & text,
                                     const std::wstring & value,
                                     float                fontDip,
                                     const wchar_t      * fontFamily,
                                     float                maxWidthDip,
                                     DxuiElide            mode,
                                     bool                 gdiWidths)
{
    std::wstring   result = value;
    bool           search = false;



    search = (mode != DxuiElide::None)
          && !value.empty()
          && (maxWidthDip > 0.0f)
          && !Fits (text, value, fontDip, fontFamily, maxWidthDip, gdiWidths);

    if (search)
    {
        result = (mode == DxuiElide::PathHead)
               ? ElidePathHead (text, value, fontDip, fontFamily, maxWidthDip)
               : ElideTail     (text, value, fontDip, fontFamily, maxWidthDip, gdiWidths);
    }

    return result;
}
