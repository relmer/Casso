#include "Pch.h"

#include "FileTypes/QuickDrawFont.h"
#include "FileTypes/AppleWorks.h"





////////////////////////////////////////////////////////////////////////////////
//
//  QuickDrawFont::TryParse
//
//  The location and offset/width tables have an entry for each character
//  from the first to the last, one for the missing symbol, and one more
//  that ends the last image in the strike.
//
////////////////////////////////////////////////////////////////////////////////

bool QuickDrawFont::TryParse (std::span<const Byte> bytes, QuickDrawFont & outFont)
{
    constexpr size_t  kFamilyAt = 2;
    constexpr size_t  kStyleAt  = 4;
    constexpr size_t  kSizeAt   = 6;
    constexpr int     kChars    = 256;



    size_t  header  = 0;
    size_t  record  = 0;
    size_t  strike  = 0;
    size_t  locAt   = 0;
    size_t  owAt    = 0;
    size_t  entries = 0;



    if (bytes.empty())
    {
        return false;
    }

    header = (size_t) bytes[0] + 1;

    if (header + kSizeAt + 2 > bytes.size())
    {
        return false;
    }

    record = header + 2 * (size_t) AppleWorks::ReadWord (bytes, header);

    if (record + 2 * kRecordWords > bytes.size())
    {
        return false;
    }

    outFont            = QuickDrawFont();
    outFont.m_family   = AppleWorks::ReadWord (bytes, header + kFamilyAt);
    outFont.m_style    = AppleWorks::ReadWord (bytes, header + kStyleAt);
    outFont.m_size     = AppleWorks::ReadWord (bytes, header + kSizeAt);
    outFont.m_first    = AppleWorks::ReadWord (bytes, record + 2);
    outFont.m_last     = AppleWorks::ReadWord (bytes, record + 4);
    outFont.m_widMax   = AppleWorks::ReadWord (bytes, record + 6);
    outFont.m_kernMax  = (int16_t) AppleWorks::ReadWord (bytes, record + 8);
    outFont.m_height   = AppleWorks::ReadWord (bytes, record + 14);
    outFont.m_ascent   = AppleWorks::ReadWord (bytes, record + 18);
    outFont.m_descent  = AppleWorks::ReadWord (bytes, record + 20);
    outFont.m_rowBytes = 2 * (size_t) AppleWorks::ReadWord (bytes, record + 24);

    if (outFont.m_first > outFont.m_last || outFont.m_last >= kChars || outFont.m_height <= 0)
    {
        return false;
    }

    entries = (size_t) (outFont.m_last - outFont.m_first) + 3;
    strike  = record + 2 * kRecordWords;
    locAt   = strike + outFont.m_rowBytes * (size_t) outFont.m_height;
    owAt    = locAt + 2 * entries;

    if (owAt + 2 * entries > bytes.size())
    {
        return false;
    }

    outFont.m_strike.assign (bytes.begin() + (ptrdiff_t) strike, bytes.begin() + (ptrdiff_t) locAt);

    for (size_t i = 0; i < entries; i++)
    {
        outFont.m_locations.push_back    (AppleWorks::ReadWord (bytes, locAt + 2 * i));
        outFont.m_offsetWidths.push_back (AppleWorks::ReadWord (bytes, owAt  + 2 * i));
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  QuickDrawFont::TryGetIndex
//
//  The character's table entry, or the missing symbol's for a character
//  out of range or marked missing.
//
////////////////////////////////////////////////////////////////////////////////

bool QuickDrawFont::TryGetIndex (Byte character, size_t & outIndex) const
{
    size_t  missing = (size_t) (m_last - m_first) + 1;



    outIndex = missing;

    if (character >= m_first && character <= m_last && m_offsetWidths[(size_t) (character - m_first)] != kMissing)
    {
        outIndex = (size_t) (character - m_first);
    }

    return m_offsetWidths[outIndex] != kMissing;
}





////////////////////////////////////////////////////////////////////////////////
//
//  QuickDrawFont::TryGetChar
//
////////////////////////////////////////////////////////////////////////////////

bool QuickDrawFont::TryGetChar (Byte character, int & outAdvance, int & outWidth, int & outOffset) const
{
    size_t  index = 0;



    if (!TryGetIndex (character, index))
    {
        return false;
    }

    outAdvance = m_offsetWidths[index] & 0xFF;
    outOffset  = m_kernMax + (m_offsetWidths[index] >> 8);
    outWidth   = (int) m_locations[index + 1] - (int) m_locations[index];
    outWidth   = (std::max) (outWidth, 0);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  QuickDrawFont::IsSet
//
////////////////////////////////////////////////////////////////////////////////

bool QuickDrawFont::IsSet (Byte character, int x, int y) const
{
    size_t  index  = 0;
    size_t  column = 0;
    size_t  at     = 0;



    if (!TryGetIndex (character, index) || y < 0 || y >= m_height || x < 0)
    {
        return false;
    }

    column = (size_t) m_locations[index] + (size_t) x;

    if (column >= m_locations[index + 1])
    {
        return false;
    }

    at = (size_t) y * m_rowBytes + column / 8;

    return at < m_strike.size() && ((m_strike[at] >> (7 - column % 8)) & 1) != 0;
}
