#include "Pch.h"

#include "Devices/Disk/FileBlockStore.h"
#include "Devices/Disk/IDiskFileIo.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FileBlockStore::FileBlockStore
//
////////////////////////////////////////////////////////////////////////////////

FileBlockStore::FileBlockStore (IDiskFileIo & fileIo, std::string path, uint64_t firstByte, uint32_t blockCount)
    : m_fileIo     (fileIo),
      m_path       (std::move (path)),
      m_firstByte  (firstByte),
      m_blockCount (blockCount)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileBlockStore::LoadPage
//
//  The page, read from the file the first time it is asked for. The last page
//  is shorter when the block count isn't a whole number of pages.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FileBlockStore::LoadPage (uint32_t page, std::vector<Byte> *& outPage)
{
    HRESULT            hr         = S_OK;
    auto               found      = m_pages.find (page);
    uint32_t           firstBlock = page * kBlocksPerPage;
    uint32_t           blocks     = std::min (kBlocksPerPage, m_blockCount - firstBlock);
    uint64_t           offset     = m_firstByte + (uint64_t) firstBlock * kBlockBytes;
    std::vector<Byte>  bytes ((size_t) blocks * kBlockBytes, 0);



    if (found != m_pages.end())
    {
        outPage = &found->second;

        BAIL_OUT_IF (true, S_OK);
    }

    hr = m_fileIo.ReadAt (m_path, offset, bytes);
    CHR (hr);

    outPage = &(m_pages[page] = std::move (bytes));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileBlockStore::LoadPages
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FileBlockStore::LoadPages (uint32_t first, uint32_t last)
{
    HRESULT            hr     = S_OK;
    uint32_t           page   = first;
    uint32_t           end    = 0;
    uint32_t           block  = 0;
    uint32_t           blocks = 0;
    std::vector<Byte>  bytes;



    while (page <= last)
    {
        if (m_pages.contains (page))
        {
            page++;
            continue;
        }

        end = page;

        while (end + 1 <= last && !m_pages.contains (end + 1))
        {
            end++;
        }

        block  = page * kBlocksPerPage;
        blocks = std::min ((end - page + 1) * kBlocksPerPage, m_blockCount - block);

        bytes.assign ((size_t) blocks * kBlockBytes, 0);

        hr = m_fileIo.ReadAt (m_path, m_firstByte + (uint64_t) block * kBlockBytes, bytes);
        CHR (hr);

        for (uint32_t p = page; p <= end; p++)
        {
            size_t  offset = (size_t) (p - page) * kBlocksPerPage * kBlockBytes;
            size_t  length = std::min ((size_t) kBlocksPerPage * kBlockBytes, bytes.size() - offset);

            m_pages.emplace (p, std::vector<Byte> (bytes.begin() + (ptrdiff_t) offset, bytes.begin() + (ptrdiff_t) (offset + length)));
        }

        page = end + 1;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileBlockStore::ReadBlocks
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FileBlockStore::ReadBlocks (uint32_t first, uint32_t count, std::span<Byte> outBytes)
{
    HRESULT              hr     = S_OK;
    bool                 inside = (uint64_t) first + count <= m_blockCount;
    bool                 sized  = outBytes.size() >= (size_t) count * kBlockBytes;
    uint32_t             i      = 0;
    std::vector<Byte>  * page   = nullptr;



    CBRAEx (inside, E_INVALIDARG);
    CBRAEx (sized,  E_INVALIDARG);
    BAIL_OUT_IF (count == 0, S_OK);

    hr = LoadPages (first / kBlocksPerPage, (first + count - 1) / kBlocksPerPage);
    CHR (hr);

    for (i = 0; i < count; i++)
    {
        uint32_t  block = first + i;

        hr = LoadPage (block / kBlocksPerPage, page);
        CHR (hr);

        std::copy_n (page->begin() + (ptrdiff_t) ((size_t) (block % kBlocksPerPage) * kBlockBytes),
                     kBlockBytes,
                     outBytes.begin() + (ptrdiff_t) ((size_t) i * kBlockBytes));
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileBlockStore::WriteBlocks
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FileBlockStore::WriteBlocks (uint32_t first, std::span<const Byte> bytes)
{
    HRESULT              hr     = S_OK;
    uint32_t             count  = (uint32_t) (bytes.size() / kBlockBytes);
    bool                 whole  = bytes.size() % kBlockBytes == 0;
    bool                 inside = (uint64_t) first + count <= m_blockCount;
    uint32_t             i      = 0;
    std::vector<Byte>  * page   = nullptr;



    CBRAEx (whole,  E_INVALIDARG);
    CBRAEx (inside, E_INVALIDARG);

    for (i = 0; i < count; i++)
    {
        uint32_t  block = first + i;

        hr = LoadPage (block / kBlocksPerPage, page);
        CHR (hr);

        std::copy_n (bytes.begin() + (ptrdiff_t) ((size_t) i * kBlockBytes),
                     kBlockBytes,
                     page->begin() + (ptrdiff_t) ((size_t) (block % kBlocksPerPage) * kBlockBytes));

        m_dirty.insert (block / kBlocksPerPage);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileBlockStore::Commit
//
//  Each changed page is written in place. A page that fails stays marked, so
//  a second Commit tries it again.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FileBlockStore::Commit()
{
    HRESULT                hr = S_OK;
    std::vector<uint32_t>  written;



    for (auto page = m_dirty.begin(); page != m_dirty.end(); )
    {
        uint32_t           first  = *page;
        uint64_t           offset = m_firstByte + (uint64_t) first * kBlocksPerPage * kBlockBytes;
        std::vector<Byte>  run;
        size_t             runs   = 0;

        while (page != m_dirty.end() && *page == first + runs)
        {
            run.insert (run.end(), m_pages[*page].begin(), m_pages[*page].end());
            ++page;
            ++runs;
        }

        hr = m_fileIo.WriteAt (m_path, offset, run);
        CHR (hr);

        for (size_t i = 0; i < runs; i++)
        {
            written.push_back (first + (uint32_t) i);
        }
    }

Error:
    for (uint32_t page : written)
    {
        m_dirty.erase (page);
    }

    return hr;
}
