#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockEditBuffer
//
//  The pending edits on a volume's blocks and the undo stack over them, the
//  same as the sector editor's: typing overwrites in place, so a block
//  always has 512 bytes, and a typed character takes the high bit of the
//  byte it replaces. Each typed digit or character, each whole block set and
//  each discard is one action for Undo and Redo. A block's bytes when its
//  first edit began are saved, so a byte is changed where the two differ.
//  Pure.
//
////////////////////////////////////////////////////////////////////////////////

class BlockEditBuffer
{
public:
    static constexpr size_t  kBlockBytes = 512;

    using Block = std::array<Byte, kBlockBytes>;

    //  `base` is the block as it is in the image, used for its first edit.
    bool  TypeHexDigit  (uint32_t block, const Block & base, int offset, bool isHighNibble, wchar_t digit);
    bool  TypeCharacter (uint32_t block, const Block & base, int offset, wchar_t ch);
    bool  SetBlock      (uint32_t block, const Block & base, const Block & bytes);

    bool  Undo    ();
    bool  Redo    ();
    bool  CanUndo () const { return m_undoAt > 0; }
    bool  CanRedo () const { return m_undoAt < m_actions.size(); }

    void  DiscardAll   ();
    void  DiscardBlock (uint32_t block);

    //  A block's bytes with its pending edits.
    void  Overlay    (uint32_t block, Block & ioBytes) const;
    bool  IsChanged  (uint32_t block, int offset) const;
    bool  HasChanges (uint32_t block) const;

    //  The blocks with changed bytes, ascending, and how many bytes changed.
    vector<uint32_t>  GetChangedBlocks  () const;
    int               CountChangedBytes () const;
    bool              IsEmpty           () const { return CountChangedBytes() == 0; }

private:
    struct Pending
    {
        Block  atStart = {};
        Block  bytes   = {};
    };

    struct Action
    {
        std::map<uint32_t, Pending>  before;
        std::map<uint32_t, Pending>  after;
    };

    Pending &  Begin  (uint32_t block, const Block & base);
    bool       Change (uint32_t block, const Block & base, int offset, Byte value);
    void       Record (const std::map<uint32_t, Pending> & before);

    std::map<uint32_t, Pending>  m_pending;
    vector<Action>               m_actions;
    size_t                       m_undoAt = 0;
};
