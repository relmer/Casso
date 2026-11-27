#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockHexSource
//
//  The bytes in block mode's hex view: one block, or a container's header,
//  with a mark for each byte. A changed byte, a byte of the field under the
//  pointer and a byte of a link each have a separate mark.
//
////////////////////////////////////////////////////////////////////////////////

class BlockHexSource : public IDxuiHexSource
{
public:
    static constexpr uint8_t  kMarkNone    = 0;
    static constexpr uint8_t  kMarkLink    = 1;
    static constexpr uint8_t  kMarkField   = 2;
    static constexpr uint8_t  kMarkDiffers = 3;
    static constexpr uint8_t  kMarkChanged = 4;

    void  SetBytes   (std::span<const Byte> bytes);
    void  ClearMarks ();
    void  SetMark    (uint64_t offset, uint8_t mark);

    //  A mark whose bytes are padding, never drawn; 0 for none.
    void  SetAbsentMark (uint8_t mark) { m_absentMark = mark; }

    //  An outlined run of bytes, with a label on its top edge unless the
    //  label is empty.
    struct Region
    {
        uint64_t      first     = 0;
        uint64_t      count     = 0;
        std::wstring  label;
        uint32_t      argb      = 0;
        bool          isRaised  = false;
        uint32_t      labelArgb = 0;   // 0 for the outline's color
    };

    //  Regions in order of their first byte, none overlapping.
    void  SetRegions (vector<Region> regions) { m_regions = std::move (regions); }

    uint64_t  GetByteCount       () const override { return m_bytes.size(); }
    void      ReadBytes          (uint64_t offset, std::span<uint8_t> out) const override;
    void      ReadMarks          (uint64_t offset, std::span<uint8_t> out) const override;
    void      ReadRegions        (uint64_t offset, std::span<uint16_t> out) const override;
    bool      TryGetRegionStyle  (uint16_t region, uint32_t & outArgb, std::wstring & outLabel) const override;
    bool      IsByteAbsent       (uint64_t offset) const override { return m_absentMark != 0 && offset < m_marks.size() && m_marks[offset] == m_absentMark; }
    bool      IsRegionRaised     (uint16_t region) const override { return region > 0 && region <= m_regions.size() && m_regions[region - 1].isRaised; }
    uint32_t  GetRegionLabelArgb (uint16_t region, uint32_t outlineArgb) const override { return (region > 0 && region <= m_regions.size() && m_regions[region - 1].labelArgb != 0) ? m_regions[region - 1].labelArgb : outlineArgb; }

    const vector<Byte> &  GetBytes () const { return m_bytes; }

private:
    vector<Byte>     m_bytes;
    vector<uint8_t>  m_marks;
    vector<Region>   m_regions;
    uint8_t          m_absentMark = 0;
};
