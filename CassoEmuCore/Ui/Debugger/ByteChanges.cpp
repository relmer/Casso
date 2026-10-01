#include "Pch.h"

#include "Ui/Debugger/ByteChanges.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ByteChanges::ByteChanges
//
////////////////////////////////////////////////////////////////////////////////

ByteChanges::ByteChanges()
{
    Clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ByteChanges::Update
//
////////////////////////////////////////////////////////////////////////////////

void ByteChanges::Update (bool isPaused, const Seen & seen)
{
    std::vector<Word>  differing;



    for (const auto & [address, value] : seen)
    {
        if (m_known[address] != kUnknown && m_known[address] != (int) value)
        {
            differing.push_back (address);
        }
    }

    if (!isPaused || !differing.empty())
    {
        m_changed.assign (kAddressSpace, false);

        for (Word address : differing)
        {
            m_changed[address] = true;
        }
    }

    for (const auto & [address, value] : seen)
    {
        m_known[address] = (int) value;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ByteChanges::Clear
//
////////////////////////////////////////////////////////////////////////////////

void ByteChanges::Clear()
{
    m_known.assign   (kAddressSpace, kUnknown);
    m_changed.assign (kAddressSpace, false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ByteChanges::GetEditText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ByteChanges::GetEditText (Word address, size_t count)
{
    return std::format (L"changed {} {} at ${:04X}", count, (count == 1) ? L"byte" : L"bytes", address);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ByteChanges::ParseRowBytes
//
////////////////////////////////////////////////////////////////////////////////

ByteChanges::Seen ByteChanges::ParseRowBytes (Word address, const std::string & text)
{
    constexpr size_t  kStride = 3;   // two digits and a space
    constexpr int     kHex    = 16;
    Seen              seen;
    size_t            at      = 0;



    for (size_t i = 0; i * kStride + 1 < text.size(); i++)
    {
        at = i * kStride;

        if (isxdigit ((unsigned char) text[at]) && isxdigit ((unsigned char) text[at + 1]))
        {
            seen.push_back ({ (Word) ((address + i) & 0xFFFF), (Byte) std::stoi (text.substr (at, 2), nullptr, kHex) });
        }
    }

    return seen;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ByteChanges::GetChangedRanges
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::pair<int, int>> ByteChanges::GetChangedRanges (Word address, const std::string & text) const
{
    constexpr int                     kStride = 3;
    constexpr int                     kDigits = 2;
    std::vector<std::pair<int, int>>  ranges;



    for (const auto & [at, value] : ParseRowBytes (address, text))
    {
        int  first = (int) ((Word) (at - address)) * kStride;

        (void) value;

        if (IsChanged (at))
        {
            ranges.push_back ({ first, first + kDigits });
        }
    }

    return ranges;
}
