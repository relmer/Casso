#include "Pch.h"

#include "Devices/Disk/Inspector/Blocks/BlockFieldList.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockFieldList::FormatHex
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BlockFieldList::FormatHex (uint32_t value, int digits)
{
    wchar_t  text[16] = {};



    swprintf_s (text, L"$%0*X", digits, value);

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockFieldList::AddRun
//
////////////////////////////////////////////////////////////////////////////////

void BlockFieldList::AddRun (uint32_t offset, uint32_t length, const std::wstring & label, const std::wstring & value)
{
    BlockField  field;



    if (offset >= m_bytes.size() || length == 0)
    {
        return;
    }

    field.offset = offset;
    field.length = std::min<uint32_t> (length, (uint32_t) m_bytes.size() - offset);
    field.label  = label;
    field.value  = value;

    m_fields.push_back (std::move (field));
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockFieldList::AddByte
//
////////////////////////////////////////////////////////////////////////////////

void BlockFieldList::AddByte (uint32_t offset, const std::wstring & label)
{
    constexpr int  kDigits = 2;



    AddRun (offset, 1, label, FormatHex (GetByte (offset), kDigits));
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockFieldList::AddWord
//
////////////////////////////////////////////////////////////////////////////////

void BlockFieldList::AddWord (uint32_t offset, const std::wstring & label)
{
    constexpr int  kDigits = 4;
    constexpr int  kBytes  = 2;



    Word  value = GetWord (offset);



    AddRun (offset, kBytes, label, FormatHex (value, kDigits) + L" (" + std::to_wstring (value) + L")");
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockFieldList::AddLong
//
//  A little-endian number of `length` bytes, such as a ProDOS EOF's three.
//
////////////////////////////////////////////////////////////////////////////////

void BlockFieldList::AddLong (uint32_t offset, uint32_t length, const std::wstring & label)
{
    constexpr int  kBitsPerByte = 8;



    uint32_t  value = 0;
    uint32_t  i     = 0;



    for (i = 0; i < length; i++)
    {
        value |= (uint32_t) GetByte (offset + i) << (kBitsPerByte * i);
    }

    AddRun (offset, length, label, FormatHex (value, (int) length * 2) + L" (" + std::to_wstring (value) + L")");
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockFieldList::AddBigWord
//
////////////////////////////////////////////////////////////////////////////////

void BlockFieldList::AddBigWord (uint32_t offset, const std::wstring & label)
{
    constexpr int  kDigits = 4;
    constexpr int  kBytes  = 2;



    Word  value = GetBigWord (offset);



    AddRun (offset, kBytes, label, FormatHex (value, kDigits) + L" (" + std::to_wstring (value) + L")");
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockFieldList::AddBigLong
//
////////////////////////////////////////////////////////////////////////////////

void BlockFieldList::AddBigLong (uint32_t offset, const std::wstring & label)
{
    constexpr int  kDigits = 8;
    constexpr int  kBytes  = 4;



    uint32_t  value = GetBigLong (offset);



    AddRun (offset, kBytes, label, FormatHex (value, kDigits) + L" (" + std::to_wstring (value) + L")");
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockFieldList::AddLink
//
//  A little-endian block number. A zero or a block past the volume is a
//  field but not a link.
//
////////////////////////////////////////////////////////////////////////////////

void BlockFieldList::AddLink (uint32_t offset, const std::wstring & label, uint32_t blockCount)
{
    Word  value = GetWord (offset);



    AddWord (offset, label);

    if (!m_fields.empty() && m_fields.back().offset == offset && value != 0 && value < blockCount)
    {
        m_fields.back().link = value;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockFieldList::AddBigLink
//
//  A big-endian number of `length` bytes that leads to `link`, as an HFS
//  node pointer leads to the block the node is in.
//
////////////////////////////////////////////////////////////////////////////////

void BlockFieldList::AddBigLink (uint32_t offset, uint32_t length, const std::wstring & label, int64_t link)
{
    constexpr uint32_t  kWordBytes = 2;



    if (length == kWordBytes)
    {
        AddBigWord (offset, label);
    }
    else
    {
        AddBigLong (offset, label);
    }

    if (!m_fields.empty() && m_fields.back().offset == offset)
    {
        m_fields.back().link = link;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockFieldList::AddSplitLink
//
//  A block number with its low byte at one offset and its high byte at
//  another, as ProDOS index blocks have them.
//
////////////////////////////////////////////////////////////////////////////////

void BlockFieldList::AddSplitLink (uint32_t lowOffset, uint32_t highOffset, const std::wstring & label, uint32_t blockCount)
{
    constexpr int  kDigits = 4;



    Word        value = (Word) (GetByte (lowOffset) | (GetByte (highOffset) << 8));
    BlockField  field;



    if (lowOffset >= m_bytes.size() || highOffset >= m_bytes.size())
    {
        return;
    }

    field.offset     = lowOffset;
    field.length     = 1;
    field.pairOffset = (int32_t) highOffset;
    field.label      = label;
    field.value      = FormatHex (value, kDigits) + L" (" + std::to_wstring (value) + L")";

    if (value != 0 && value < blockCount)
    {
        field.link = value;
    }

    m_fields.push_back (std::move (field));
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockFieldList::AddName
//
////////////////////////////////////////////////////////////////////////////////

void BlockFieldList::AddName (uint32_t offset, uint32_t length, const std::wstring & label, bool isHighBitStripped)
{
    constexpr Byte  kHighBit = 0x80;



    std::wstring  text;
    uint32_t      i    = 0;
    Byte          ch   = 0;



    for (i = 0; i < length; i++)
    {
        ch = GetByte (offset + i);

        if (isHighBitStripped)
        {
            ch = (Byte) (ch & ~kHighBit);
        }

        text += (ch >= ' ' && ch < 0x7F) ? (wchar_t) ch : L'.';
    }

    AddRun (offset, length, label, L"\"" + text + L"\"");
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockFieldList::GetText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BlockFieldList::GetText (uint32_t offset, uint32_t length) const
{
    std::wstring  text;
    uint32_t      i    = 0;
    Byte          ch   = 0;



    for (i = 0; i < length; i++)
    {
        ch   = GetByte (offset + i);
        text += (ch >= ' ' && ch < 0x7F) ? (wchar_t) ch : L'.';
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockFieldList::SetLastValue
//
////////////////////////////////////////////////////////////////////////////////

void BlockFieldList::SetLastValue (const std::wstring & value)
{
    if (!m_fields.empty())
    {
        m_fields.back().value = value;
    }
}