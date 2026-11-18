#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IBlockSource
//
//  Where a BlockDeviceMedia's blocks come from: an image file, which a write
//  changes and Flush commits, or blocks in memory, which a write doesn't
//  change. The media has an overlay of every written block, so ReadBlock
//  returns a block's content in the source.
//
////////////////////////////////////////////////////////////////////////////////

class IBlockSource
{
public:
    using WrittenBlocks = std::map<uint32_t, std::shared_ptr<const std::vector<Byte>>>;

    virtual ~IBlockSource() = default;

    virtual HRESULT  ReadBlock         (uint32_t block, std::span<Byte> outBytes) = 0;

    //  A block the guest wrote, after the media checked the write.
    virtual HRESULT  WriteBlock        (uint32_t block, std::span<const Byte> bytes) = 0;

    //  The written blocks of a loaded state, in place of the blocks written
    //  before the load.
    virtual HRESULT  RestoreWritten    (const WrittenBlocks & written) = 0;

    virtual HRESULT  Flush             () = 0;
    virtual bool     HasPendingWrites  () const = 0;
    virtual bool     HasChangedOnHost  () = 0;

    //  The blocks as they were when the source was attached.
    virtual HRESULT  GetAttachedBlocks (std::shared_ptr<const std::vector<Byte>> & outBlocks) = 0;
};
