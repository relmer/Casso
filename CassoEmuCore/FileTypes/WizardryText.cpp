#include "Pch.h"

#include "FileTypes/WizardryText.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"





////////////////////////////////////////////////////////////////////////////////
//
//  WizardryText::ReadSymbol
//
//  One character, by the tree from its root, or false when the bits run
//  out before a leaf.
//
////////////////////////////////////////////////////////////////////////////////

bool WizardryText::ReadSymbol (std::span<const Byte> tree, Bits & ioBits, Byte & outSymbol)
{
    int   node   = 0;
    bool  isLeaf = false;



    for (size_t steps = 0; !isLeaf && steps < kMaxSteps; steps++)
    {
        if (ioBits.left == 0)
        {
            if (ioBits.at >= ioBits.data.size())
            {
                return false;
            }

            ioBits.byte = ioBits.data[ioBits.at++];
            ioBits.left = 8;
        }

        bool  bit = (ioBits.byte & 1) != 0;

        ioBits.byte >>= 1;
        ioBits.left--;

        isLeaf = ((tree[(size_t) node] >> (bit ? 0 : 1)) & 1) != 0;
        node   = tree[(bit ? kTableBytes : 2 * kTableBytes) + (size_t) node];
    }

    outSymbol = (Byte) node;

    return isLeaf;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WizardryText::ReadString
//
//  False for a string that runs out of bits or has a character that isn't
//  printable, as the bytes past a block's last string do.
//
////////////////////////////////////////////////////////////////////////////////

bool WizardryText::ReadString (std::span<const Byte> tree, Bits & ioBits, std::wstring & outText)
{
    Byte  length = 0;
    Byte  symbol = 0;
    bool  isRead = false;



    //  Each string starts on a byte, past the bits left of the last one's.
    ioBits.left = 0;
    isRead      = ReadSymbol (tree, ioBits, length) && length > 0;

    outText.clear();

    for (int i = 0; isRead && i < length; i++)
    {
        isRead = ReadSymbol (tree, ioBits, symbol) && symbol >= ' ' && symbol < 0x7F;
        outText.push_back ((wchar_t) symbol);
    }

    return isRead;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WizardryText::ReadItemNames
//
////////////////////////////////////////////////////////////////////////////////

bool WizardryText::ReadItemNames (std::span<const Byte> sectors, std::vector<ItemName> & outItems)
{
    std::vector<Byte>          tree (3 * kTableBytes);
    std::vector<Byte>          block (kBlockBytes);
    std::vector<std::wstring>  strings;
    bool                       isFound = false;



    outItems.clear();

    if (sectors.size() < (size_t) (kLastTextBlock + 1) * kBlockBytes)
    {
        return false;
    }

    for (size_t i = 0; i < tree.size(); i++)
    {
        tree[i] = sectors[ProDosSkeleton::GetBlockByteOffset (sectors.size(), kTreeBlock + (int) (i / kBlockBytes), i % kBlockBytes)];
    }

    for (int number = kFirstTextBlock; number <= kLastTextBlock; number++)
    {
        Bits          bits;
        std::wstring  text;

        for (size_t i = 0; i < kBlockBytes; i++)
        {
            block[i] = sectors[ProDosSkeleton::GetBlockByteOffset (sectors.size(), number, i)];
        }

        bits.data = block;

        while (ReadString (tree, bits, text))
        {
            strings.push_back (text);
        }
    }

    for (size_t i = 0; i + 1 < strings.size(); i++)
    {
        isFound = isFound || strings[i] == L"BROKEN ITEM";

        if (isFound)
        {
            outItems.push_back (ItemName { strings[i], strings[i + 1] });
            i++;
        }
    }

    return !outItems.empty();
}
