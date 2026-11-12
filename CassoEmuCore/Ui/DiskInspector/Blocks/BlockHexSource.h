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

    uint64_t  GetByteCount () const override { return m_bytes.size(); }
    void      ReadBytes    (uint64_t offset, std::span<uint8_t> out) const override;
    void      ReadMarks    (uint64_t offset, std::span<uint8_t> out) const override;

    const vector<Byte> &  GetBytes () const { return m_bytes; }

private:
    vector<Byte>     m_bytes;
    vector<uint8_t>  m_marks;
};
