#include "Pch.h"

#include "Devices/Disk/Inspector/Blocks/BlockEditBuffer.h"

#include "Devices/Disk/Inspector/InspectorClipboard.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockEditBuffer::Begin
//
////////////////////////////////////////////////////////////////////////////////

BlockEditBuffer::Pending & BlockEditBuffer::Begin (uint32_t block, const Block & base)
{
    auto  found = m_pending.find (block);



    if (found == m_pending.end())
    {
        found = m_pending.emplace (block, Pending { base, base }).first;
    }

    return found->second;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockEditBuffer::Record
//
//  One action from the edits as they were before it to the edits now. An
//  action after an undo replaces the ones that could be redone.
//
////////////////////////////////////////////////////////////////////////////////

void BlockEditBuffer::Record (const std::map<uint32_t, Pending> & before)
{
    m_actions.resize (m_undoAt);
    m_actions.push_back ({ before, m_pending });

    m_undoAt = m_actions.size();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockEditBuffer::Change
//
////////////////////////////////////////////////////////////////////////////////

bool BlockEditBuffer::Change (uint32_t block, const Block & base, int offset, Byte value)
{
    std::map<uint32_t, Pending>  before = m_pending;
    Pending                    * edit   = nullptr;



    if (offset < 0 || offset >= (int) kBlockBytes)
    {
        return false;
    }

    edit = &Begin (block, base);

    if (edit->bytes[offset] == value)
    {
        m_pending = before;
        return false;
    }

    edit->bytes[offset] = value;

    Record (before);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockEditBuffer::TypeHexDigit
//
////////////////////////////////////////////////////////////////////////////////

bool BlockEditBuffer::TypeHexDigit (uint32_t block, const Block & base, int offset, bool isHighNibble, wchar_t digit)
{
    constexpr int   kNibbleBits = 4;
    constexpr Byte  kLowNibble  = 0x0F;
    constexpr Byte  kHighNibble = 0xF0;
    constexpr int   kDecimal    = 10;



    Block  current = base;
    int    value   = -1;
    Byte   byte    = 0;



    if (offset < 0 || offset >= (int) kBlockBytes)
    {
        return false;
    }

    if (digit >= L'0' && digit <= L'9')
    {
        value = digit - L'0';
    }
    else if (digit >= L'a' && digit <= L'f')
    {
        value = digit - L'a' + kDecimal;
    }
    else if (digit >= L'A' && digit <= L'F')
    {
        value = digit - L'A' + kDecimal;
    }

    if (value < 0)
    {
        return false;
    }

    Overlay (block, current);

    byte = current[offset];
    byte = isHighNibble ? (Byte) ((byte & kLowNibble) | (value << kNibbleBits)) : (Byte) ((byte & kHighNibble) | value);

    return Change (block, base, offset, byte);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockEditBuffer::TypeCharacter
//
//  A printable ASCII character, with the high bit of the byte it replaces.
//
////////////////////////////////////////////////////////////////////////////////

bool BlockEditBuffer::TypeCharacter (uint32_t block, const Block & base, int offset, wchar_t ch)
{
    constexpr Byte     kHighBit        = 0x80;
    constexpr wchar_t  kFirstPrintable = 0x20;
    constexpr wchar_t  kLastPrintable  = 0x7E;



    Block  current = base;



    if (ch < kFirstPrintable || ch > kLastPrintable || offset < 0 || offset >= (int) kBlockBytes)
    {
        return false;
    }

    Overlay (block, current);

    return Change (block, base, offset, (Byte) ((current[offset] & kHighBit) | (Byte) ch));
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockEditBuffer::SetBlock
//
////////////////////////////////////////////////////////////////////////////////

bool BlockEditBuffer::SetBlock (uint32_t block, const Block & base, const Block & bytes)
{
    std::map<uint32_t, Pending>  before = m_pending;
    Pending                    & edit   = Begin (block, base);



    if (edit.bytes == bytes)
    {
        m_pending = before;
        return false;
    }

    edit.bytes = bytes;

    Record (before);

    return true;
}




////////////////////////////////////////////////////////////////////////////////
//
//  BlockEditBuffer::Paste
//
//  Hex text is read by InspectorClipboard::ParseHex. Each pasted character
//  takes the high bit of the byte it replaces, as typing does.
//
////////////////////////////////////////////////////////////////////////////////

int BlockEditBuffer::Paste (uint32_t block, const Block & base, int offset, const std::wstring & text, bool isTextColumn, int & outOverflow)
{
    constexpr Byte     kHighBit        = 0x80;
    constexpr Byte     kLowBits        = 0x7F;
    constexpr wchar_t  kFirstPrintable = 0x20;
    constexpr wchar_t  kLastPrintable  = 0x7E;



    vector<Byte>  values;
    Block         current = base;
    bool          isValid = offset >= 0 && offset < (int) kBlockBytes;
    int           room    = (int) kBlockBytes - offset;
    int           written = 0;
    size_t        i       = 0;



    outOverflow = 0;

    if (!isTextColumn)
    {
        isValid = isValid && InspectorClipboard::ParseHex (text, values) == InspectorClipboard::HexParse::Bytes;
    }

    for (wchar_t ch : (isValid && isTextColumn) ? text : std::wstring())
    {
        if (ch >= kFirstPrintable && ch <= kLastPrintable)
        {
            values.push_back ((Byte) (ch & kLowBits));
        }
    }

    isValid = isValid && !values.empty();

    if (isValid)
    {
        outOverflow = std::max (0, (int) values.size() - room);
        values.resize (std::min ((int) values.size(), room));
        Overlay (block, current);

        for (i = 0; i < values.size(); i++)
        {
            current[offset + i] = isTextColumn ? (Byte) (values[i] | (current[offset + i] & kHighBit)) : values[i];
        }

        written = (int) values.size();
        (void) SetBlock (block, base, current);
    }

    return written;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockEditBuffer::Undo
//
////////////////////////////////////////////////////////////////////////////////

bool BlockEditBuffer::Undo()
{
    if (!CanUndo())
    {
        return false;
    }

    m_undoAt--;
    m_pending = m_actions[m_undoAt].before;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockEditBuffer::Redo
//
////////////////////////////////////////////////////////////////////////////////

bool BlockEditBuffer::Redo()
{
    if (!CanRedo())
    {
        return false;
    }

    m_pending = m_actions[m_undoAt].after;
    m_undoAt++;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockEditBuffer::DiscardAll
//
//  After the edits are applied or thrown away, nothing is left to undo.
//
////////////////////////////////////////////////////////////////////////////////

void BlockEditBuffer::DiscardAll()
{
    m_pending.clear();
    m_actions.clear();

    m_undoAt = 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockEditBuffer::DiscardBlock
//
////////////////////////////////////////////////////////////////////////////////

void BlockEditBuffer::DiscardBlock (uint32_t block)
{
    std::map<uint32_t, Pending>  before = m_pending;



    if (m_pending.erase (block) != 0)
    {
        Record (before);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockEditBuffer::Overlay
//
////////////////////////////////////////////////////////////////////////////////

void BlockEditBuffer::Overlay (uint32_t block, Block & ioBytes) const
{
    auto  found = m_pending.find (block);



    if (found != m_pending.end())
    {
        ioBytes = found->second.bytes;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockEditBuffer::IsChanged
//
////////////////////////////////////////////////////////////////////////////////

bool BlockEditBuffer::IsChanged (uint32_t block, int offset) const
{
    auto  found = m_pending.find (block);



    return found != m_pending.end() && offset >= 0 && offset < (int) kBlockBytes
        && found->second.bytes[offset] != found->second.atStart[offset];
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockEditBuffer::HasChanges
//
////////////////////////////////////////////////////////////////////////////////

bool BlockEditBuffer::HasChanges (uint32_t block) const
{
    auto  found = m_pending.find (block);



    return found != m_pending.end() && found->second.bytes != found->second.atStart;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockEditBuffer::GetChangedBlocks
//
////////////////////////////////////////////////////////////////////////////////

vector<uint32_t> BlockEditBuffer::GetChangedBlocks() const
{
    vector<uint32_t>  blocks;



    for (const auto & [block, edit] : m_pending)
    {
        if (edit.bytes != edit.atStart)
        {
            blocks.push_back (block);
        }
    }

    return blocks;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockEditBuffer::CountChangedBytes
//
////////////////////////////////////////////////////////////////////////////////

int BlockEditBuffer::CountChangedBytes() const
{
    int     count = 0;
    size_t  i     = 0;



    for (const auto & [block, edit] : m_pending)
    {
        for (i = 0; i < kBlockBytes; i++)
        {
            count += edit.bytes[i] != edit.atStart[i] ? 1 : 0;
        }
    }

    return count;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockEditBuffer::HasSameChanges
//
//  Whether the two have the same changed blocks, with the same bytes.
//
////////////////////////////////////////////////////////////////////////////////

bool BlockEditBuffer::HasSameChanges (const BlockEditBuffer & other) const
{
    vector<uint32_t>  blocks  = GetChangedBlocks();
    bool              isSame  = blocks == other.GetChangedBlocks();



    for (size_t i = 0; isSame && i < blocks.size(); i++)
    {
        isSame = m_pending.at (blocks[i]).bytes == other.m_pending.at (blocks[i]).bytes;
    }

    return isSame;
}
