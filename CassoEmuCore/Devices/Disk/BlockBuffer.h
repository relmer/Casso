#pragma once

#include "Pch.h"

#include "Devices/Disk/BlockImage.h"
#include "Devices/Disk/IBlockStore.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockBuffer
//
//  A volume's bytes as a writer changes them. Each block is copied from the
//  image the first time the writer touches it, and the writer changes the
//  copy, so a write to a large volume reads and stores only the blocks it
//  touches. As an IBlockStore, a BlockImage over the buffer reads the
//  copies, so a volume over it reads what the writer has written so far.
//
//  The image must outlive the buffer. A buffer made with no image is empty.
//
////////////////////////////////////////////////////////////////////////////////

class BlockBuffer : public IBlockStore
{
public:
    BlockBuffer () = default;

    //  Over an image of another buffer, a copy of that buffer, so every
    //  buffer reads untouched blocks from the volume's image.
    explicit BlockBuffer (const BlockImage & image);

    size_t  size () const { return m_size; }

    //  A byte of a block, which is copied from the image the first time the
    //  block is touched.
    Byte &  operator[] (size_t at);
    Byte    operator[] (size_t at) const;

    //  Every byte: the image's, with the touched blocks over them. Reads the
    //  whole image.
    std::vector<Byte>  ToVector () const;

    //  The blocks touched, by number, each as it is now.
    const std::map<uint32_t, std::vector<Byte>> &  GetTouchedBlocks () const { return m_blocks; }

    uint32_t  GetBlockCount () const override { return (uint32_t) (m_size / kBlockBytes); }
    HRESULT   ReadBlocks    (uint32_t first, uint32_t count, std::span<Byte> outBytes) override;
    HRESULT   WriteBlocks   (uint32_t first, std::span<const Byte> bytes) override;
    HRESULT   Commit        () override { return S_OK; }

private:
    std::vector<Byte> &  Touch (uint32_t block);

    const BlockImage                         * m_image = nullptr;
    size_t                                     m_size  = 0;
    std::map<uint32_t, std::vector<Byte>>      m_blocks;
};
