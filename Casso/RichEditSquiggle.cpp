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
}
