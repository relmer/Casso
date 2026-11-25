#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  WizardryText
//
//  The text of Wizardry V, which is Huffman coded: ASCII.HUFF
//  is the tree and ASCII.KRN the strings. The game's scenario disk A has
//  the volume's blocks 0 to 206 and then 412 on, so ASCII.HUFF, at volume
//  block 412, is the disk's block 207, and the start of ASCII.KRN follows.
//  Measured by stepping through the game's decoder in Casso's debugger.
//
//  The tree is three tables of 256 bytes: each node's flags, its child for
//  a 1 bit and its child for a 0 bit. Flag bit 0 marks the 1 child a leaf,
//  a character, and bit 1 the 0 child. Bits are taken low first from each
//  byte, and each string starts on a byte. A string is its length as a
//  character and then its characters.
//  The strings of a block don't run past it, and the block ends with a
//  table of their sizes, so a block's strings run until one doesn't fit.
//
//  The item strings follow the monsters' strings, two for each item from item
//  0, BROKEN ITEM: the name before it's identified and the true name.
//
////////////////////////////////////////////////////////////////////////////////

class WizardryText
{
public:
    struct ItemName
    {
        std::wstring  unknown;
        std::wstring  name;
    };

    //  The item strings from a scenario disk A's sectors, in DOS order, or
    //  false when the disk doesn't have them.
    static bool  ReadItemNames (std::span<const Byte> sectors, std::vector<ItemName> & outItems);

    static constexpr int     kTreeBlock      = 207;
    static constexpr int     kFirstTextBlock = 209;
    static constexpr int     kLastTextBlock  = 279;
    static constexpr size_t  kBlockBytes     = 512;
    static constexpr size_t  kTableBytes     = 256;

private:
    struct Bits
    {
        std::span<const Byte>  data;
        size_t                 at   = 0;
        int                    byte = 0;
        int                    left = 0;
    };

    static constexpr size_t  kMaxSteps = 64;

    static bool  ReadSymbol (std::span<const Byte> tree, Bits & ioBits, Byte & outSymbol);
    static bool  ReadString (std::span<const Byte> tree, Bits & ioBits, std::wstring & outText);
};
