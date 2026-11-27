#include "Pch.h"

#include "Ui/DiskInspector/Blocks/BlockHexSource.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockHexSource::SetBytes
//
////////////////////////////////////////////////////////////////////////////////

void BlockHexSource::SetBytes (std::span<const Byte> bytes)
{
    m_bytes.assign (bytes.begin(), bytes.end());
    m_marks.assign (m_bytes.size(), kMarkNone);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockHexSource::ClearMarks
//
////////////////////////////////////////////////////////////////////////////////

void BlockHexSource::ClearMarks()
{
    std::fill (m_marks.begin(), m_marks.end(), kMarkNone);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockHexSource::SetMark
//
//  A changed byte's mark isn't replaced by another.
//
////////////////////////////////////////////////////////////////////////////////

void BlockHexSource::SetMark (uint64_t offset, uint8_t mark)
{
    if (offset < m_marks.size() && m_marks[offset] != kMarkChanged)
    {
        m_marks[offset] = std::max (m_marks[offset], mark);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockHexSource::ReadBytes
//
////////////////////////////////////////////////////////////////////////////////

void BlockHexSource::ReadBytes (uint64_t offset, std::span<uint8_t> out) const
{
    size_t  i = 0;



    for (i = 0; i < out.size(); i++)
    {
        out[i] = offset + i < m_bytes.size() ? m_bytes[offset + i] : (uint8_t) 0;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockHexSource::ReadMarks
//
////////////////////////////////////////////////////////////////////////////////

void BlockHexSource::ReadMarks (uint64_t offset, std::span<uint8_t> out) const
{
    size_t  i = 0;



    for (i = 0; i < out.size(); i++)
    {
        out[i] = offset + i < m_marks.size() ? m_marks[offset + i] : kMarkNone;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockHexSource::ReadRegions
//
//  Region n is the nth in the list, counting from one.
//
////////////////////////////////////////////////////////////////////////////////

void BlockHexSource::ReadRegions (uint64_t offset, std::span<uint16_t> out) const
{
    size_t  next = 0;



    for (size_t i = 0; i < out.size(); i++)
    {
        uint64_t  at = offset + i;

        while (next < m_regions.size() && m_regions[next].first + m_regions[next].count <= at)
        {
            next++;
        }

        out[i] = (next < m_regions.size() && at >= m_regions[next].first) ? static_cast<uint16_t> (next + 1) : uint16_t (0);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockHexSource::TryGetRegionStyle
//
////////////////////////////////////////////////////////////////////////////////

bool BlockHexSource::TryGetRegionStyle (uint16_t region, uint32_t & outArgb, std::wstring & outLabel) const
{
    if (region == 0 || region > m_regions.size())
    {
        return false;
    }

    outArgb  = m_regions[region - 1].argb;
    outLabel = m_regions[region - 1].label;
    return true;
}