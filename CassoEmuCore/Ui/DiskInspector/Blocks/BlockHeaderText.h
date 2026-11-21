#pragma once

#include "Pch.h"

class BlockInspectorModel;
struct BlockField;





//  A capsule beside the block's number: Free or Used, the block's role, or a
//  file the block is in. A damaged role's capsule is drawn red.
struct BlockCapsule
{
    std::wstring  text;
    bool          isBad = false;
};





////////////////////////////////////////////////////////////////////////////////
//
//  BlockHeaderText
//
//  The text of block mode's block header and hex view: the capsules, the
//  field line, a byte's tip, and the block as text for Copy block.
//
////////////////////////////////////////////////////////////////////////////////

class BlockHeaderText
{
public:
    //  Free or Used, then the block's role for a block in use, then each
    //  file the block is in.
    static vector<BlockCapsule>  BuildCapsules (const BlockInspectorModel & model, uint32_t block);

    //  The block's Disk Copy tag bytes, and whether the block differs from
    //  the other image.
    static std::wstring          BuildFieldLine (const BlockInspectorModel & model, uint32_t block);

    //  A byte's offset, then its field and the field's value, then what its
    //  BlockHexSource mark indicates. `outNoteLine` is the line of the
    //  mark's note, or -1.
    static std::wstring  BuildByteTip   (uint64_t offset, uint8_t mark, const BlockField * field, const std::wstring & otherName, int & outNoteLine);

    //  `bytesPerRow` bytes a line: the offset, the bytes in hex, and their
    //  text.
    static std::wstring  BuildBlockText (std::span<const Byte> bytes, size_t bytesPerRow);

    //  The other image's file name, or empty when the model isn't comparing.
    static std::wstring  GetOtherName   (const BlockInspectorModel & model);
};
