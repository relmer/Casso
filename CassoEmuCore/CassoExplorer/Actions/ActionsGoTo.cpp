#include "Pch.h"

#include "CassoExplorer/Actions/ActionsGoTo.h"
#include "CassoExplorer/CassoExplorerActions.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsGoTo::ActionsGoTo
//
////////////////////////////////////////////////////////////////////////////////

ActionsGoTo::ActionsGoTo (CassoExplorerActions & actions)
    : m_actions (actions)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsGoTo::TryParseGoTo
//
//  A hyphen at the very start moves back from the caret, so only a hyphen
//  after the first address separates a range.
//
////////////////////////////////////////////////////////////////////////////////

bool ActionsGoTo::TryParseGoTo (const std::wstring & text, int64_t caretAddress, int64_t & outFirst, int64_t & outLast)
{
    size_t        first  = text.find_first_not_of (L' ');
    size_t        split  = std::wstring::npos;
    size_t        start  = std::wstring::npos;
    std::wstring  rest;
    uint64_t      amount = 0;



    if (first == std::wstring::npos)
    {
        return false;
    }

    split = text.find (L',', first);
    split = (split != std::wstring::npos) ? split : text.find (L'-', first + 1);

    if (split == std::wstring::npos)
    {
        if (!TryParseGoToTarget (text, caretAddress, outFirst))
        {
            return false;
        }

        outLast = outFirst;
        return true;
    }

    if (!TryParseGoToTarget (text.substr (0, split), caretAddress, outFirst))
    {
        return false;
    }

    rest  = text.substr (split + 1);
    start = rest.find_first_not_of (L' ');

    if ((text[split] == L',') && (start != std::wstring::npos) && (rest[start] == L'+'))
    {
        if (!TryParseOffset (rest.substr (start + 1), amount) || amount == 0)
        {
            return false;
        }

        outLast = outFirst + (int64_t) amount - 1;
    }
    else
    {
        if (!TryParseOffset (rest, amount))
        {
            return false;
        }

        outLast = (int64_t) amount;
    }

    return outLast >= outFirst;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsGoTo::TryParseGoToTarget
//
////////////////////////////////////////////////////////////////////////////////

bool ActionsGoTo::TryParseGoToTarget (const std::wstring & text, int64_t caretAddress, int64_t & outAddress)
{
    size_t   first  = text.find_first_not_of (L' ');
    wchar_t  sign   = 0;
    uint64_t amount = 0;



    if (first == std::wstring::npos)
    {
        return false;
    }

    if (text[first] == L'+' || text[first] == L'-')
    {
        sign = text[first];
        first++;
    }

    if (!TryParseOffset (text.substr (first), amount))
    {
        return false;
    }

    outAddress = (sign == L'+') ? (caretAddress + (int64_t) amount)
               : (sign == L'-') ? (caretAddress - (int64_t) amount)
                                : (int64_t) amount;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsGoTo::TryParseSearch
//
////////////////////////////////////////////////////////////////////////////////

bool ActionsGoTo::TryParseSearch (const std::wstring & text, std::vector<Byte> & outBytes, bool & outIsText)
{
    size_t             first = text.find_first_not_of (L' ');
    size_t             last  = text.find_last_not_of (L' ');
    std::vector<Byte>  bytes;
    std::wstring       digits;



    if (first == std::wstring::npos)
    {
        return false;
    }

    //  Text in quotes: every character must be one an Apple II can store.
    if (text[first] == L'"')
    {
        if (last <= first || text[last] != L'"')
        {
            return false;
        }

        for (size_t i = first + 1; i < last; i++)
        {
            if (text[i] < 0x20 || text[i] > 0x7E)
            {
                return false;
            }

            bytes.push_back ((Byte) text[i]);
        }

        if (bytes.empty())
        {
            return false;
        }

        outBytes  = std::move (bytes);
        outIsText = true;
        return true;
    }

    for (size_t i = first; i <= last; i++)
    {
        if (text[i] != L' ')
        {
            digits.push_back (text[i]);
        }
    }

    //  Anything that is not an even run of hex digits is text, as typed.
    if ((digits.size() % 2) != 0 || digits.find_first_not_of (L"0123456789ABCDEFabcdef") != std::wstring::npos)
    {
        for (size_t i = first; i <= last; i++)
        {
            if (text[i] < 0x20 || text[i] > 0x7E)
            {
                return false;
            }

            bytes.push_back ((Byte) text[i]);
        }

        outBytes  = std::move (bytes);
        outIsText = true;
        return true;
    }

    for (size_t i = 0; i < digits.size(); i += 2)
    {
        bytes.push_back ((Byte) std::stoul (digits.substr (i, 2), nullptr, 16));
    }

    outBytes  = std::move (bytes);
    outIsText = false;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsGoTo::FindBytes
//
////////////////////////////////////////////////////////////////////////////////

size_t ActionsGoTo::FindBytes (const std::vector<Byte> & haystack, const std::vector<Byte> & pattern, bool isText, size_t start)
{
    size_t  count = haystack.size();



    if (pattern.empty() || pattern.size() > count)
    {
        return CassoExplorerActions::kNotFound;
    }

    start = (start > count - pattern.size()) ? 0 : start;

    for (size_t n = 0; n <= count - pattern.size(); n++)
    {
        size_t  at    = (start + n) % (count - pattern.size() + 1);
        bool    match = true;

        for (size_t i = 0; i < pattern.size() && match; i++)
        {
            Byte  have = haystack[at + i];
            Byte  want = pattern[i];

            if (isText)
            {
                have = (Byte) towupper (have & 0x7F);
                want = (Byte) towupper (want & 0x7F);
            }

            match = (have == want);
        }

        if (match)
        {
            return at;
        }
    }

    return CassoExplorerActions::kNotFound;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsGoTo::FindInSource
//
//  From `start` to the end, then from the beginning back to `start`.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t ActionsGoTo::FindInSource (const ActionsTypes::ReadFn & read, uint64_t count, const std::vector<Byte> & pattern, bool isText, uint64_t start)
{
    uint64_t              length = pattern.size();
    uint64_t              last   = 0;
    std::vector<uint8_t>  chunk;



    if (length == 0 || length > count)
    {
        return CassoExplorerActions::kNotFoundOffset;
    }

    last  = count - length;
    start = (start > last) ? 0 : start;

    for (int pass = 0; pass < 2; pass++)
    {
        uint64_t  from = (pass == 0) ? start : 0;
        uint64_t  to   = (pass == 0) ? last  : (std::min) (start, last + 1);

        //  `to` is the last offset a match may start at on the first pass,
        //  and one past it on the second.
        for (uint64_t at = from; (pass == 0) ? (at <= to) : (at < to); )
        {
            uint64_t  starts = (std::min) ((uint64_t) CassoExplorerActions::kSearchChunkBytes, ((pass == 0) ? (to + 1) : to) - at);
            uint64_t  span   = starts + length - 1;

            chunk.resize ((size_t) span);
            read (at, chunk);

            for (uint64_t i = 0; i < starts; i++)
            {
                bool  match = true;

                for (uint64_t j = 0; j < length && match; j++)
                {
                    Byte  have = chunk[(size_t) (i + j)];
                    Byte  want = pattern[(size_t) j];

                    if (isText)
                    {
                        have = (Byte) towupper (have & 0x7F);
                        want = (Byte) towupper (want & 0x7F);
                    }

                    match = (have == want);
                }

                if (match)
                {
                    return at + i;
                }
            }

            at += starts;
        }
    }

    return CassoExplorerActions::kNotFoundOffset;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsGoTo::TryParseAddress
//
////////////////////////////////////////////////////////////////////////////////

bool ActionsGoTo::TryParseAddress (const std::wstring & text, Word & outAddress)
{
    uint64_t  value = 0;



    if (!TryParseOffset (text, value) || value > 0xFFFF)
    {
        return false;
    }

    outAddress = (Word) value;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsGoTo::TryParseOffset
//
//  TryParseAddress's forms, over the whole 64-bit range, for an offset into a
//  file larger than the Apple II's address space.
//
////////////////////////////////////////////////////////////////////////////////

bool ActionsGoTo::TryParseOffset (const std::wstring & text, uint64_t & outValue)
{
    std::wstring        digits = text;
    int                 base   = 16;
    wchar_t           * end    = nullptr;
    unsigned long long  value  = 0;



    while (!digits.empty() && iswspace (digits.front()))
    {
        digits.erase (digits.begin());
    }

    while (!digits.empty() && iswspace (digits.back()))
    {
        digits.pop_back();
    }

    if (!digits.empty() && digits[0] == L'$')
    {
        digits.erase (0, 1);
    }
    else if (digits.size() > 2 && digits[0] == L'0' && (digits[1] == L'x' || digits[1] == L'X'))
    {
        digits.erase (0, 2);
    }
    else if (!digits.empty() && digits[0] == L'#')
    {
        digits.erase (0, 1);
        base = 10;
    }

    if (digits.empty() || !iswxdigit (digits[0]))
    {
        return false;
    }

    errno = 0;
    value = wcstoull (digits.c_str(), &end, base);

    if (end == nullptr || *end != L'\0' || errno == ERANGE)
    {
        return false;
    }

    outValue = (uint64_t) value;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsGoTo::TryParseNumbers
//
////////////////////////////////////////////////////////////////////////////////

bool ActionsGoTo::TryParseNumbers (const std::wstring & text, size_t required, std::vector<int> & outNumbers)
{
    static constexpr size_t  kMaxNumbers = 3;
    std::wstring             token;



    outNumbers.clear();

    for (size_t i = 0; i <= text.size(); i++)
    {
        wchar_t  c = (i < text.size()) ? text[i] : L' ';

        if (iswdigit (c))
        {
            token.push_back (c);
            continue;
        }

        if (c != L' ' && c != L',' && c != L'\t')
        {
            return false;
        }

        if (!token.empty())
        {
            outNumbers.push_back (_wtoi (token.c_str()));
            token.clear();
        }
    }

    return outNumbers.size() >= required && outNumbers.size() <= kMaxNumbers;
}
