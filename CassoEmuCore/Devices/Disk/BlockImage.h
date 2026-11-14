#pragma once

#include "Pch.h"

#include "Devices/Disk/IBlockStore.h"

class BlockBuffer;





////////////////////////////////////////////////////////////////////////////////
//
//  BlockImage
//
//  A volume's bytes, read by index as a vector's are, from a vector in memory
//  or from a store of blocks. From a store, each run of kChunkBlocks is read
//  the first time a byte in it is, so a catalog read touches the directory
//  and bitmap blocks, not the volume.
//
//  Over a vector, nothing is copied, the vector must outlive the image, and a
//  change to the vector, its size too, reads through the image. The same is
//  true over a BlockBuffer, so a volume over a buffer a writer is filling
//  reads what has been written so far.
//  A block that can't be read from a store reads as zeros, and GetReadError
//  returns the first failure.
//
////////////////////////////////////////////////////////////////////////////////

class BlockImage
{
public:
    static constexpr size_t  kChunkBlocks = 16;
    static constexpr size_t  kChunkBytes  = kChunkBlocks * IBlockStore::kBlockBytes;

    BlockImage (const std::vector<Byte> & bytes) : m_view (&bytes), m_size (bytes.size()) {}
    explicit BlockImage (IBlockStore & store)    : m_store (&store), m_size ((size_t) store.GetBlockCount() * IBlockStore::kBlockBytes) {}
    explicit BlockImage (const BlockBuffer & buffer) : m_buffer (&buffer) {}

    size_t  size () const;

    Byte  operator[] (size_t at) const { return m_view != nullptr ? (*m_view)[at] : ReadStoreByte (at); }

    //  Every byte, read from the store when the image is over one.
    std::vector<Byte>  ToVector () const;

    //  True for an image over a store.
    bool     IsLazy       () const { return m_store != nullptr; }

    //  The buffer the image is over, or null.
    const BlockBuffer *  GetBuffer () const { return m_buffer; }
    HRESULT  GetReadError () const { return m_readError; }

    //  The bytes read from the store so far, for a test of how little is read.
    size_t   GetBytesLoaded () const { return m_chunks.size() * kChunkBytes; }

private:
    Byte                       ReadStoreByte (size_t at) const;
    const std::vector<Byte>  & LoadChunk     (size_t chunk) const;

    const std::vector<Byte>                       * m_view      = nullptr;
    const BlockBuffer                             * m_buffer    = nullptr;
    IBlockStore                                   * m_store     = nullptr;
    size_t                                          m_size      = 0;
    mutable std::map<size_t, std::vector<Byte>>     m_chunks;
    mutable HRESULT                                 m_readError = S_OK;
};
