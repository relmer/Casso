#include "Pch.h"

#include "Devices/Disk/BlockBuffer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockBuffer::BlockBuffer
//
////////////////////////////////////////////////////////////////////////////////

BlockBuffer::BlockBuffer (const BlockImage & image)
{
    if (image.GetBuffer() != nullptr)
    {
        *this = *image.GetBuffer();
    }
    else
    {
        m_image = &image;
        m_size  = image.size();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockBuffer::Touch
//
//  For a block past the end of a volume whose size isn't whole blocks,
//  only the bytes the volume has are copied; the rest read as zeros.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<Byte> & BlockBuffer::Touch (uint32_t block)
{
    auto    found = m_blocks.find (block);
    size_t  first = (size_t) block * kBlockBytes;



    if (found != m_blocks.end())
    {
        return found->second;
    }

    std::vector<Byte> &  bytes = m_blocks[block];

    bytes.assign (kBlockBytes, 0);

    for (size_t i = 0; i < kBlockBytes && first + i < m_size && m_image != nullptr; i++)
    {
        bytes[i] = (*m_image)[first + i];
    }

    return bytes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockBuffer::operator[]
//
////////////////////////////////////////////////////////////////////////////////

Byte & BlockBuffer::operator[] (size_t at)
{
    return Touch ((uint32_t) (at / kBlockBytes))[at % kBlockBytes];
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockBuffer::operator[] const
//
////////////////////////////////////////////////////////////////////////////////

Byte BlockBuffer::operator[] (size_t at) const
{
    auto  found = m_blocks.find ((uint32_t) (at / kBlockBytes));



    if (found != m_blocks.end())
    {
        return found->second[at % kBlockBytes];
    }

    return m_image != nullptr ? (*m_image)[at] : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockBuffer::ToVector
//
////////////////////////////////////////////////////////////////////////////////

std::vector<Byte> BlockBuffer::ToVector() const
{
    std::vector<Byte>  bytes = m_image != nullptr ? m_image->ToVector() : std::vector<Byte>();



    bytes.resize (m_size, 0);

    for (const auto & [block, contents] : m_blocks)
    {
        size_t  first = (size_t) block * kBlockBytes;
        size_t  count = std::min (kBlockBytes, m_size - std::min (first, m_size));

        std::copy_n (contents.begin(), count, bytes.begin() + (ptrdiff_t) first);
    }

    return bytes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockBuffer::ReadBlocks
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BlockBuffer::ReadBlocks (uint32_t first, uint32_t count, std::span<Byte> outBytes)
{
    HRESULT  hr     = S_OK;
    bool     inside = (uint64_t) first + count <= GetBlockCount() && outBytes.size() >= (size_t) count * kBlockBytes;



    CBRAEx (inside, E_INVALIDARG);

    for (uint32_t i = 0; i < count; i++)
    {
        auto    found = m_blocks.find (first + i);
        size_t  base  = (size_t) (first + i) * kBlockBytes;

        for (size_t b = 0; b < kBlockBytes; b++)
        {
            outBytes[(size_t) i * kBlockBytes + b] = found != m_blocks.end() ? found->second[b] : (*m_image)[base + b];
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockBuffer::WriteBlocks
//
//  Each block is replaced whole, so the image's copy isn't read.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BlockBuffer::WriteBlocks (uint32_t first, std::span<const Byte> bytes)
{
    HRESULT   hr     = S_OK;
    uint32_t  count  = (uint32_t) (bytes.size() / kBlockBytes);
    bool      inside = bytes.size() % kBlockBytes == 0 && (uint64_t) first + count <= GetBlockCount();



    CBRAEx (inside, E_INVALIDARG);

    for (uint32_t i = 0; i < count; i++)
    {
        m_blocks[first + i].assign (bytes.begin() + (ptrdiff_t) ((size_t) i * kBlockBytes), bytes.begin() + (ptrdiff_t) ((size_t) (i + 1) * kBlockBytes));
    }

Error:
    return hr;
}
