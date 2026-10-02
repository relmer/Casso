#include "Pch.h"

#include "Core/DxuiTextElide.h"

#include "Core/DxuiUnicodeSymbols.h"
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
                          float                maxWidthDip)
{
    HRESULT   hr = S_OK;
    float     w  = 0.0f;
    float     h  = 0.0f;



    hr = text.MeasureString (candidate.c_str(), fontDip, fontFamily, w, h);

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
                                       float                maxWidthDip)
{
    size_t   lo  = 0;
    size_t   hi  = value.size();
    size_t   mid = 0;



    while (lo < hi)
    {
        mid = (lo + hi + 1) / 2;

        if (Fits (text, value.substr (0, mid) + s_kEllipsis, fontDip, fontFamily, maxWidthDip))
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
//  JoinMiddle
//
//  `kept` characters of `body` around one ellipsis -- the head taking the odd
//  one -- followed by the whole suffix.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DxuiTextElide::JoinMiddle (const std::wstring & body,
                                        size_t               kept,
                                        const std::wstring & suffix)
{
    size_t  head = (kept + 1) / 2;
    size_t  tail = kept / 2;



    return body.substr (0, head) + s_kEllipsis + body.substr (body.size() - tail) + suffix;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ElideMiddle
//
//  The most characters of the body, split between its head and its tail,
//  that fit around the ellipsis with the suffix after them. The suffix is
//  never cut: with no room for any of the body, the result is the ellipsis
//  and the suffix, even where that still overflows.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DxuiTextElide::ElideMiddle (IDxuiTextRenderer  & text,
                                         const std::wstring & value,
                                         float                fontDip,
                                         const wchar_t      * fontFamily,
                                         float                maxWidthDip,
                                         size_t               keptSuffix)
{
    size_t        suffixLen = (std::min) (keptSuffix, value.size());
    std::wstring  body      = value.substr (0, value.size() - suffixLen);
    std::wstring  suffix    = value.substr (value.size() - suffixLen);
    size_t        lo        = 0;
    size_t        hi        = body.size();
    size_t        mid       = 0;



    if (body.empty())
    {
        return value;
    }

    while (lo < hi)
    {
        mid = (lo + hi + 1) / 2;

        if (Fits (text, JoinMiddle (body, mid, suffix), fontDip, fontFamily, maxWidthDip))
        {
            lo = mid;
        }
        else
        {
            hi = mid - 1;
        }
    }

    return JoinMiddle (body, lo, suffix);
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
                                     size_t               keptSuffix)
{
    std::wstring   result = value;
    bool           search = false;



    search = (mode != DxuiElide::None)
          && !value.empty()
          && (maxWidthDip > 0.0f)
          && !Fits (text, value, fontDip, fontFamily, maxWidthDip);

    if (!search)
    {
        return result;
    }

    switch (mode)
    {
        case DxuiElide::PathHead: result = ElidePathHead (text, value, fontDip, fontFamily, maxWidthDip);             break;
        case DxuiElide::Middle:   result = ElideMiddle   (text, value, fontDip, fontFamily, maxWidthDip, keptSuffix); break;

        case DxuiElide::Tail:
        default:                  result = ElideTail     (text, value, fontDip, fontFamily, maxWidthDip);             break;
    }

    return result;
}
