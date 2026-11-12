#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockField
//
//  One annotated run of a block's bytes: where it starts, how long it is,
//  what it is and the value it has. A field split in two, as a ProDOS index
//  block's pointer is, low byte in the first half and high byte in the
//  second, has the second part at `pairOffset`. A field that is a block
//  number has that block in `link`, which a click steps to.
//
////////////////////////////////////////////////////////////////////////////////

struct BlockField
{
    static constexpr int64_t  kNoLink = -1;

    uint32_t      offset     = 0;
    uint32_t      length     = 0;
    int32_t       pairOffset = -1;
    std::wstring  label;
    std::wstring  value;
    int64_t       link       = kNoLink;

    bool  Contains (uint32_t at) const
    {
        return (at >= offset && at < offset + length) || (pairOffset >= 0 && at == (uint32_t) pairOffset);
    }
};


using BlockBytes = std::span<const Byte>;





////////////////////////////////////////////////////////////////////////////////
//
//  BlockFieldList
//
//  Builds a block's fields in order: little- and big-endian numbers, text
//  and plain runs, each with its value formatted for the inspector.
//
////////////////////////////////////////////////////////////////////////////////

class BlockFieldList
{
public:
    BlockFieldList (BlockBytes bytes, vector<BlockField> & outFields) : m_bytes (bytes), m_fields (outFields) {}

    void  AddRun       (uint32_t offset, uint32_t length, const std::wstring & label, const std::wstring & value = {});
    void  AddByte      (uint32_t offset, const std::wstring & label);
    void  AddWord      (uint32_t offset, const std::wstring & label);
    void  AddLong      (uint32_t offset, uint32_t length, const std::wstring & label);
    void  AddBigWord   (uint32_t offset, const std::wstring & label);
    void  AddBigLong   (uint32_t offset, const std::wstring & label);
    void  AddLink      (uint32_t offset, const std::wstring & label, uint32_t blockCount);
    void  AddBigLink   (uint32_t offset, uint32_t length, const std::wstring & label, int64_t link);
    void  AddSplitLink (uint32_t lowOffset, uint32_t highOffset, const std::wstring & label, uint32_t blockCount);
    void  AddName      (uint32_t offset, uint32_t length, const std::wstring & label, bool isHighBitStripped);

    //  A run of bytes as text: printable ASCII as it is, the rest as '.'.
    std::wstring  GetText (uint32_t offset, uint32_t length) const;

    Byte      GetByte    (uint32_t at) const { return at < m_bytes.size() ? m_bytes[at] : (Byte) 0; }
    Word      GetWord    (uint32_t at) const { return (Word) (GetByte (at) | (GetByte (at + 1) << 8)); }
    Word      GetBigWord (uint32_t at) const { return (Word) ((GetByte (at) << 8) | GetByte (at + 1)); }
    uint32_t  GetBigLong (uint32_t at) const { return ((uint32_t) GetBigWord (at) << 16) | GetBigWord (at + 2); }

    static std::wstring  FormatHex (uint32_t value, int digits);

private:
    BlockBytes            m_bytes;
    vector<BlockField>  & m_fields;
};
