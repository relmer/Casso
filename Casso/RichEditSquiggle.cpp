#include "Pch.h"

#include "RichEditSquiggle.h"





////////////////////////////////////////////////////////////////////////////////
//
//  File-scope constants
//
////////////////////////////////////////////////////////////////////////////////

static const wchar_t  s_kpszLabelPrefix[]   = L"Ignored: ";
static const wchar_t  s_kpszLabelSeparator[] = L", ";

// CHARFORMAT2.bUnderlineColor is a color index (Windows SDK richedit.h
// declares the field but no named color macros). The MSDN-documented
// palette positions us at index 5 for red, 0 for black/automatic.
static constexpr BYTE  kSquiggleUnderlineColor      = 5;
static constexpr BYTE  kSquiggleUnderlineColorBlack = 0;





////////////////////////////////////////////////////////////////////////////////
//
//  BuildIgnoredTokensLabel
//
//  Returns "" when spans is empty. Otherwise returns
//  "Ignored: <tok1>, <tok2>" where each <tokN> is the half-open
//  UTF-16 substring expr[beginUtf16, endUtf16). Tokens that fall
//  outside the source expression are clipped silently (defensive --
//  the parser already records valid offsets, but we don't crash if
//  a future call site hands us stale spans).
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BuildIgnoredTokensLabel (
    std::wstring_view                                       originalExpr,
    const std::vector<TrackSectorPredicate::RejectedSpan> & spans)
{
    std::wstring  out;
    size_t        i      = 0;
    int           begin  = 0;
    int           end    = 0;

    if (spans.empty())
    {
        return out;
    }

    out.assign (s_kpszLabelPrefix);

    for (i = 0; i < spans.size(); i++)
    {
        if (i > 0)
        {
            out.append (s_kpszLabelSeparator);
        }

        begin = spans[i].beginUtf16;
        end   = spans[i].endUtf16;

        if (begin < 0)
        {
            begin = 0;
        }

        if (end > static_cast<int> (originalExpr.size()))
        {
            end = static_cast<int> (originalExpr.size());
        }

        if (end > begin)
        {
            out.append (originalExpr.substr (static_cast<size_t> (begin),
                                             static_cast<size_t> (end - begin)));
        }
    }

    return out;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetIgnoredTokensLabel
//
////////////////////////////////////////////////////////////////////////////////

void SetIgnoredTokensLabel (
    HWND                                                    hStatic,
    std::wstring_view                                       originalExpr,
    const std::vector<TrackSectorPredicate::RejectedSpan> & spans)
{
    std::wstring  text;

    if (hStatic == nullptr)
    {
        return;
    }

    text = BuildIgnoredTokensLabel (originalExpr, spans);
    SetWindowTextW (hStatic, text.c_str());
}





////////////////////////////////////////////////////////////////////////////////
//
//  BuildCombinedInvalidLabel
//
//  Returns "" when neither track nor sector spans contain rejects.
//  Otherwise builds a single string covering whichever side(s) had
//  rejects, joined by " | " when both sides do:
//    "Invalid track: abc"
//    "Invalid sector: 99"
//    "Invalid track: abc | Invalid sector: 99"
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BuildCombinedInvalidLabel (
    std::wstring_view                                       trackExpr,
    const std::vector<TrackSectorPredicate::RejectedSpan> & trackSpans,
    std::wstring_view                                       sectorExpr,
    const std::vector<TrackSectorPredicate::RejectedSpan> & sectorSpans)
{
    std::wstring  out;
    std::wstring  trackJoined;
    std::wstring  sectorJoined;

    auto Append = [] (std::wstring &                                          dst,
                      std::wstring_view                                       src,
                      const std::vector<TrackSectorPredicate::RejectedSpan> & spans)
    {
        size_t  i      = 0;
        int     begin  = 0;
        int     end    = 0;

        for (i = 0; i < spans.size(); i++)
        {
            if (i > 0)
            {
                dst.append (L", ");
            }

            begin = spans[i].beginUtf16;
            end   = spans[i].endUtf16;

            if (begin < 0)
            {
                begin = 0;
            }

            if (end > static_cast<int> (src.size()))
            {
                end = static_cast<int> (src.size());
            }

            if (end > begin)
            {
                dst.append (src.substr (static_cast<size_t> (begin),
                                        static_cast<size_t> (end - begin)));
            }
        }
    };

    if (!trackSpans.empty())
    {
        Append (trackJoined, trackExpr, trackSpans);
    }

    if (!sectorSpans.empty())
    {
        Append (sectorJoined, sectorExpr, sectorSpans);
    }

    if (!trackJoined.empty())
    {
        out.append (L"Invalid track: ");
        out.append (trackJoined);
    }

    if (!sectorJoined.empty())
    {
        if (!out.empty())
        {
            out.append (L"  |  ");
        }

        out.append (L"Invalid sector: ");
        out.append (sectorJoined);
    }

    return out;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetCombinedInvalidLabel
//
////////////////////////////////////////////////////////////////////////////////

void SetCombinedInvalidLabel (
    HWND                                                    hStatic,
    std::wstring_view                                       trackExpr,
    const std::vector<TrackSectorPredicate::RejectedSpan> & trackSpans,
    std::wstring_view                                       sectorExpr,
    const std::vector<TrackSectorPredicate::RejectedSpan> & sectorSpans)
{
    std::wstring  text;

    if (hStatic == nullptr)
    {
        return;
    }

    text = BuildCombinedInvalidLabel (trackExpr, trackSpans, sectorExpr, sectorSpans);
    SetWindowTextW (hStatic, text.c_str());
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyRejectedTokenSquiggles
//
//  Save the current selection, clear underline formatting across the
//  whole input, then re-apply CFU_UNDERLINEWAVE / red on each
//  RejectedSpan range. Finally restore the saved selection so the
//  caret position is undisturbed.
//
////////////////////////////////////////////////////////////////////////////////

void ApplyRejectedTokenSquiggles (
    HWND                                                    hRichEdit,
    const std::vector<TrackSectorPredicate::RejectedSpan> & spans)
{
    CHARRANGE       saved   = {};
    CHARRANGE       all     = { 0, -1 };
    CHARRANGE       range   = {};
    CHARFORMAT2W    clearFmt = {};
    CHARFORMAT2W    waveFmt  = {};
    size_t          i       = 0;

    if (hRichEdit == nullptr)
    {
        return;
    }

    // Spec-006 bug 4. Suppress repaints while we shuttle the
    // selection through every rejected span so the user never sees
    // the caret jump. The matching SetRedraw(TRUE) at the end fires
    // a single InvalidateRect so the final squiggle paint happens in
    // one frame.
    SendMessageW (hRichEdit, WM_SETREDRAW, FALSE, 0);

    SendMessageW (hRichEdit, EM_EXGETSEL, 0, reinterpret_cast<LPARAM> (&saved));

    SendMessageW (hRichEdit, EM_EXSETSEL, 0, reinterpret_cast<LPARAM> (&all));

    clearFmt.cbSize          = sizeof (clearFmt);
    clearFmt.dwMask          = CFM_UNDERLINETYPE;
    clearFmt.bUnderlineType  = CFU_UNDERLINENONE;
    clearFmt.bUnderlineColor = kSquiggleUnderlineColorBlack;
    SendMessageW (hRichEdit, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM> (&clearFmt));

    waveFmt.cbSize          = sizeof (waveFmt);
    waveFmt.dwMask          = CFM_UNDERLINETYPE;
    waveFmt.bUnderlineType  = CFU_UNDERLINEWAVE;
    waveFmt.bUnderlineColor = kSquiggleUnderlineColor;

    for (i = 0; i < spans.size(); i++)
    {
        range.cpMin = spans[i].beginUtf16;
        range.cpMax = spans[i].endUtf16;

        SendMessageW (hRichEdit, EM_EXSETSEL, 0, reinterpret_cast<LPARAM> (&range));
        SendMessageW (hRichEdit, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM> (&waveFmt));
    }

    SendMessageW (hRichEdit, EM_EXSETSEL, 0, reinterpret_cast<LPARAM> (&saved));

    SendMessageW (hRichEdit, WM_SETREDRAW, TRUE, 0);
    InvalidateRect (hRichEdit, nullptr, TRUE);
}
