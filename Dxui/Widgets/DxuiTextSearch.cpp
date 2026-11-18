#include "Pch.h"

#include "DxuiTextSearch.h"
#include "Theme/DxuiColor.h"
#include "Theme/IDxuiTheme.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextSearch::GetFindHighlightFill
//
//  3:1 is what WCAG asks of a mark that is not text. A dark background
//  takes the accent lighter, a light one darker.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DxuiTextSearch::GetFindHighlightFill (const IDxuiTheme & theme)
{
    constexpr float  kMinRatio = 3.0f;
    constexpr int    kMaxSteps = 40;



    uint32_t  background = theme.TextViewBackground() | 0xFF000000u;
    uint32_t  fill       = theme.WarningAccent()      | 0xFF000000u;
    bool      darkBack   = DxuiColor::ComputeRelativeLuminance (background) < 0.18f;



    for (int step = 0; step < kMaxSteps && DxuiColor::ComputeContrastRatio (fill, background) < kMinRatio; step++)
    {
        fill = darkBack ? DxuiColor::Lighten (fill, 0.1f) : DxuiColor::Darken (fill, 0.9f);
    }

    return fill;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextSearch::GetFindHighlightText
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DxuiTextSearch::GetFindHighlightText (const IDxuiTheme & theme)
{
    uint32_t  fill  = GetFindHighlightFill (theme);
    uint32_t  fore  = theme.Foreground()         | 0xFF000000u;
    uint32_t  back  = theme.TextViewBackground() | 0xFF000000u;



    return (DxuiColor::ComputeContrastRatio (fore, fill) >= DxuiColor::ComputeContrastRatio (back, fill)) ? fore : back;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextSearch::FindAllInRows
//
//  A plain needle is found as a substring, lowered on both sides without
//  matching case. A regular expression uses ECMAScript syntax; a match of
//  no characters is passed over, since it selects nothing.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextSearch::FindAllInRows (
    const std::vector<std::wstring> & rows,
    const std::wstring              & needle,
    bool                              matchCase,
    bool                              wholeWord,
    bool                              isRegex,
    std::vector<FindMatch>          & outMatches)
{
    std::wregex  pattern;
    std::wstring key = matchCase ? needle : GetLowered (needle);



    outMatches.clear();

    if (needle.empty())
    {
        return true;
    }

    if (isRegex)
    {
        try
        {
            pattern = std::wregex (needle, matchCase ? std::regex_constants::ECMAScript : (std::regex_constants::ECMAScript | std::regex_constants::icase));
        }
        catch (const std::regex_error &)
        {
            return false;
        }
    }

    for (size_t r = 0; r < rows.size(); r++)
    {
        const std::wstring &  text = rows[r];

        if (isRegex)
        {
            //  The matcher throws once a pattern backtracks past its budget,
            //  which a pattern typed, or half typed, into find can do.
            try
            {
                for (std::wsregex_iterator it (text.begin(), text.end(), pattern), end; it != end; ++it)
                {
                    size_t  at     = (size_t) it->position (0);
                    size_t  length = (size_t) it->length (0);

                    if (length > 0 && (!wholeWord || IsWholeWordAt (text, at, length)))
                    {
                        outMatches.push_back (FindMatch { Position { (int) r, (int) at }, (int) length });
                    }
                }
            }
            catch (const std::regex_error &)
            {
                outMatches.clear();
                return false;
            }

            continue;
        }

        std::wstring  hay = matchCase ? text : GetLowered (text);

        for (size_t at = hay.find (key); at != std::wstring::npos; at = hay.find (key, at + 1))
        {
            if (!wholeWord || IsWholeWordAt (text, at, key.size()))
            {
                outMatches.push_back (FindMatch { Position { (int) r, (int) at }, (int) key.size() });
            }
        }
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextSearch::FindInRows
//
//  Without a match past the starting point the search goes round: forward
//  from the top, backward from the bottom. Without matching case, both sides
//  are lowered first, which keeps every offset where it was, and lowering
//  leaves a word character a word character.
//
////////////////////////////////////////////////////////////////////////////////

DxuiTextSearch::FindResult DxuiTextSearch::FindInRows (
    const std::vector<std::wstring> & rows,
    const std::wstring              & needle,
    bool                              matchCase,
    bool                              wholeWord,
    bool                              forward,
    Position                          from,
    Position                        & outStart)
{
    std::vector<std::wstring>          folded;
    const std::vector<std::wstring>  * haystack = &rows;
    std::wstring                       key      = needle;
    Position                           around   = forward ? Position() : Position { (int) rows.size(), 0 };



    if (needle.empty())
    {
        return FindResult::NotFound;
    }

    if (!matchCase)
    {
        for (const std::wstring & text : rows)
        {
            folded.push_back (GetLowered (text));
        }

        key      = GetLowered (needle);
        haystack = &folded;
    }

    if (TryFindOnce (*haystack, key, wholeWord, forward, from, outStart))
    {
        return FindResult::Found;
    }

    if (TryFindOnce (*haystack, key, wholeWord, forward, around, outStart))
    {
        return FindResult::Wrapped;
    }

    return FindResult::NotFound;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextSearch::GetLowered
//
//  One character for one, so an offset in the result is the same offset in
//  the text it came from.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DxuiTextSearch::GetLowered (const std::wstring & text)
{
    std::wstring  lowered = text;



    for (wchar_t & ch : lowered)
    {
        ch = (wchar_t) towlower (ch);
    }

    return lowered;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextSearch::TryFindOnce
//
//  One pass, with no going round: forward, the first match starting at or
//  after `from`; backward, the last match starting before it. A match that is
//  part of a longer word is passed over when only whole words count.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextSearch::TryFindOnce (
    const std::vector<std::wstring> & rows,
    const std::wstring              & needle,
    bool                              wholeWord,
    bool                              forward,
    Position                          from,
    Position                        & outStart)
{
    int     count = (int) rows.size();
    size_t  at    = std::wstring::npos;



    if (forward)
    {
        for (int r = (std::max) (from.row, 0); r < count; r++)
        {
            const std::wstring &  text  = rows[(size_t) r];
            size_t                start = (r == from.row) ? (size_t) (std::max) (from.offset, 0) : 0;

            at = (start <= text.size()) ? text.find (needle, start) : std::wstring::npos;

            while (wholeWord && at != std::wstring::npos && !IsWholeWordAt (text, at, needle.size()))
            {
                at = text.find (needle, at + 1);
            }

            if (at != std::wstring::npos)
            {
                outStart = Position { r, (int) at };
                return true;
            }
        }

        return false;
    }

    for (int r = (std::min) (from.row, count - 1); r >= 0; r--)
    {
        const std::wstring &  text = rows[(size_t) r];

        if (r == from.row && from.offset <= 0)
        {
            continue;
        }

        at = text.rfind (needle, (r == from.row) ? (size_t) (from.offset - 1) : std::wstring::npos);

        while (wholeWord && at != std::wstring::npos && !IsWholeWordAt (text, at, needle.size()))
        {
            at = (at == 0) ? std::wstring::npos : text.rfind (needle, at - 1);
        }

        if (at != std::wstring::npos)
        {
            outStart = Position { r, (int) at };
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextSearch::IsWholeWordAt
//
//  Whether the run of text at `at` has a line end or a character that is not
//  part of a word on each side of it.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextSearch::IsWholeWordAt (const std::wstring & text, size_t at, size_t length)
{
    size_t  end = at + length;



    if (at > 0 && IsWordChar (text[at - 1]))
    {
        return false;
    }

    return end >= text.size() || !IsWordChar (text[end]);
}
