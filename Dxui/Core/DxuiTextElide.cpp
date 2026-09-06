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
                                     DxuiElide            mode)
{
    std::wstring   result = value;
    bool           search = false;



    search = (mode != DxuiElide::None)
          && !value.empty()
          && (maxWidthDip > 0.0f)
          && !Fits (text, value, fontDip, fontFamily, maxWidthDip);

    if (search)
    {
        result = (mode == DxuiElide::PathHead)
               ? ElidePathHead (text, value, fontDip, fontFamily, maxWidthDip)
               : ElideTail     (text, value, fontDip, fontFamily, maxWidthDip);
    }

    return result;
}
