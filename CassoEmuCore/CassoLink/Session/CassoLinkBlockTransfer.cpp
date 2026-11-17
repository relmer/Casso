#include "Pch.h"

#include "CassoLink/Session/CassoLinkBlockTransfer.h"
#include "CassoLink/CassoLinkCodec.h"
#include "Devices/Disk/Containers/TwoImgContainer.h"
#include "Devices/Disk/DurableCommit.h"
#include "Devices/Disk/IDiskFileIo.h"





static constexpr size_t  s_kBlock = CassoLinkCodec::kBlockSize;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkBlockTransfer::ReadDevice
//
//  A transfer cut short saves what it has before it fails, so a later one
//  can continue from it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkBlockTransfer::ReadDevice (ICassoLinkBlockIo & blocks, IDiskFileIo & io, const CassoLinkBlockDevice & device, const std::string & imagePath, bool verify, bool resume,
                                            const ProgressFn & progress, const CancelFn & cancel, Result & outResult)
{
    HRESULT                hr      = S_OK;
    HRESULT                stop    = S_OK;
    std::string            partial = CassoLinkResume::GetPartialPath (imagePath);
    CassoLinkResumeRecord  record;
    CassoLinkStatus        status;
    std::vector<Byte>      data;
    int64_t                last    = -1;
    uint32_t               next    = 0;
    uint32_t               saved   = 0;



    outResult = {};

    record.slot       = device.slot;
    record.unit       = device.unit;
    record.blockCount = device.blocks;
    record.volumeName = device.volume;
    record.direction  = CassoLinkResumeRecord::kRead;
    record.imagePath  = imagePath;

    hr = HashDevice (blocks, device, record.directoryHash);
    CHR (hr);

    last = resume ? FindRecord (io, imagePath, device, record.directoryHash, CassoLinkResumeRecord::kRead) : -1;
    next = (last >= 0 && IsPartialWhole (io, partial, device.blocks)) ? (uint32_t) (last + 1) : 0;

    //  The partial image is its whole size from the start, as a write at an
    //  offset doesn't grow a file.
    if (next == 0)
    {
        hr = io.WriteAllBytes (partial, std::vector<Byte> ((size_t) device.blocks * s_kBlock, 0));
        CHR (hr);
    }

    outResult.firstBlock = next;
    saved                = next;

    while (SUCCEEDED (stop) && next < device.blocks)
    {
        Byte  count = (Byte) std::min<uint32_t> (kRunBlocks, device.blocks - next);

        stop = (cancel && cancel()) ? HRESULT_FROM_WIN32 (ERROR_CANCELLED) : blocks.ReadBlocks (device, next, count, verify, data, status);

        if (SUCCEEDED (stop) && !data.empty())
        {
            stop = io.WriteAt (partial, (uint64_t) next * s_kBlock, data);
        }

        next += SUCCEEDED (stop) ? status.done : 0;

        if (SUCCEEDED (stop) && status.error != CassoLinkHost::kErrorNone)
        {
            NoteError (status.error, next, outResult);
            stop = HRESULT_FROM_WIN32 (ERROR_READ_FAULT);
        }

        if (SUCCEEDED (stop) && next - saved >= CassoLinkResume::kEveryBlocks)
        {
            record.lastVerifiedBlock = (int64_t) next - 1;
            saved                    = next;

            hr = io.FlushToStorage (partial);
            CHR (hr);

            hr = WriteRecord (io, imagePath, record);
            CHR (hr);
        }

        if (SUCCEEDED (stop) && progress)
        {
            progress (next, device.blocks);
        }
    }

    //  Cut short: what arrived is saved, for the offer to continue.
    if (FAILED (stop) && next > saved)
    {
        record.lastVerifiedBlock = (int64_t) next - 1;

        hr = io.FlushToStorage (partial);
        IGNORE_RETURN_VALUE (hr, S_OK);

        hr = WriteRecord (io, imagePath, record);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    CHR (stop);

    hr = FinishRead (io, imagePath, device.blocks);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkBlockTransfer::WriteDevice
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkBlockTransfer::WriteDevice (ICassoLinkBlockIo & blocks, IDiskFileIo & io, const CassoLinkBlockDevice & device, const std::string & imagePath, bool verify, bool resume,
                                             const ProgressFn & progress, const CancelFn & cancel, Result & outResult)
{
    HRESULT                hr      = S_OK;
    HRESULT                stop    = S_OK;
    std::vector<Byte>      image;
    CassoLinkResumeRecord  record;
    CassoLinkStatus        status;
    int64_t                last    = -1;
    uint32_t               total   = 0;
    uint32_t               next    = 0;
    uint32_t               saved   = 0;
    bool                   fits    = false;



    outResult = {};

    hr = LoadImage (io, imagePath, image);
    CHR (hr);

    total = (uint32_t) (image.size() / s_kBlock);
    fits  = image.size() % s_kBlock == 0 && total > 0 && total <= device.blocks;
    CBREx (fits, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    record.slot       = device.slot;
    record.unit       = device.unit;
    record.blockCount = device.blocks;
    record.volumeName = device.volume;
    record.direction  = CassoLinkResumeRecord::kWrite;
    record.imagePath  = imagePath;

    hr = HashImage (image, record.directoryHash);
    CHR (hr);

    last = resume ? FindRecord (io, imagePath, device, record.directoryHash, CassoLinkResumeRecord::kWrite) : -1;
    next = (last >= 0) ? (uint32_t) (last + 1) : 0;

    outResult.firstBlock = next;
    saved                = next;

    while (SUCCEEDED (stop) && next < total)
    {
        uint32_t  count = std::min<uint32_t> (kRunBlocks, total - next);

        stop  = (cancel && cancel()) ? HRESULT_FROM_WIN32 (ERROR_CANCELLED)
                                     : blocks.WriteBlocks (device, next, std::span<const Byte> (image).subspan ((size_t) next * s_kBlock, (size_t) count * s_kBlock), verify, status);
        next += SUCCEEDED (stop) ? status.done : 0;

        if (SUCCEEDED (stop) && status.error != CassoLinkHost::kErrorNone)
        {
            NoteError (status.error, next, outResult);
            stop = HRESULT_FROM_WIN32 (ERROR_WRITE_FAULT);
        }

        if (SUCCEEDED (stop) && next - saved >= CassoLinkResume::kEveryBlocks)
        {
            record.lastVerifiedBlock = (int64_t) next - 1;
            saved                    = next;

            hr = WriteRecord (io, imagePath, record);
            CHR (hr);
        }

        if (SUCCEEDED (stop) && progress)
        {
            progress (next, total);
        }
    }

    if (FAILED (stop) && next > saved)
    {
        record.lastVerifiedBlock = (int64_t) next - 1;

        hr = WriteRecord (io, imagePath, record);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    CHR (stop);

    if (io.Exists (CassoLinkResume::GetRecordPath (imagePath)))
    {
        hr = io.Remove (CassoLinkResume::GetRecordPath (imagePath));
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkBlockTransfer::FindResume
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkBlockTransfer::FindResume (ICassoLinkBlockIo & blocks, IDiskFileIo & io, const CassoLinkBlockDevice & device, const std::string & imagePath, bool isRead, uint32_t & outNext)
{
    HRESULT            hr    = S_OK;
    std::string        hash;
    std::vector<Byte>  image;
    int64_t            last  = -1;



    outNext = 0;

    if (isRead)
    {
        hr = HashDevice (blocks, device, hash);
    }
    else
    {
        hr = LoadImage (io, imagePath, image);

        if (SUCCEEDED (hr))
        {
            hr = HashImage (image, hash);
        }
    }

    CHR (hr);

    last = FindRecord (io, imagePath, device, hash, isRead ? CassoLinkResumeRecord::kRead : CassoLinkResumeRecord::kWrite);
    BAIL_OUT_IF (last < 0 || (isRead && !IsPartialWhole (io, CassoLinkResume::GetPartialPath (imagePath), device.blocks)), S_OK);

    outNext = (uint32_t) (last + 1);

Error:
    return outNext > 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkBlockTransfer::LoadImage
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkBlockTransfer::LoadImage (IDiskFileIo & io, const std::string & imagePath, std::vector<Byte> & outBlocks)
{
    HRESULT            hr     = S_OK;
    std::vector<Byte>  bytes;
    TwoImgHeader       header;
    std::string        error;



    outBlocks.clear();

    hr = io.ReadAllBytes (imagePath, bytes);
    CHR (hr);

    if (TwoImgContainer::IsTwoImg (bytes))
    {
        hr = TwoImgContainer::Parse (bytes, header, outBlocks, error);
        CHR (hr);
    }
    else
    {
        outBlocks = std::move (bytes);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkBlockTransfer::HashDevice
//
//  A device too small for a volume directory, or whose directory can't be
//  read, has an empty hash.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkBlockTransfer::HashDevice (ICassoLinkBlockIo & blocks, const CassoLinkBlockDevice & device, std::string & outHex)
{
    HRESULT            hr     = S_OK;
    std::vector<Byte>  data;
    CassoLinkStatus    status;



    outHex.clear();
    BAIL_OUT_IF (device.blocks < CassoLinkResume::kDirectoryFirst + CassoLinkResume::kDirectoryCount, S_OK);

    hr = blocks.ReadBlocks (device, CassoLinkResume::kDirectoryFirst, (Byte) CassoLinkResume::kDirectoryCount, false, data, status);
    CHR (hr);
    BAIL_OUT_IF (status.error != CassoLinkHost::kErrorNone, S_OK);

    hr = CassoLinkResume::HashDirectory (data, outHex);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkBlockTransfer::HashImage
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkBlockTransfer::HashImage (std::span<const Byte> image, std::string & outHex)
{
    HRESULT  hr    = S_OK;
    size_t   first = (size_t) CassoLinkResume::kDirectoryFirst * s_kBlock;
    size_t   bytes = (size_t) CassoLinkResume::kDirectoryCount * s_kBlock;



    outHex.clear();
    BAIL_OUT_IF (image.size() < first + bytes, S_OK);

    hr = CassoLinkResume::HashDirectory (image.subspan (first, bytes), outHex);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkBlockTransfer::FindRecord
//
//  The record's last block for a record that matches, -1 for none.
//
////////////////////////////////////////////////////////////////////////////////

int64_t CassoLinkBlockTransfer::FindRecord (IDiskFileIo & io, const std::string & recordFor, const CassoLinkBlockDevice & device, const std::string & hash, const char * direction)
{
    HRESULT                hr    = S_OK;
    std::vector<Byte>      bytes;
    CassoLinkResumeRecord  record;
    int64_t                last  = -1;



    BAIL_OUT_IF (!io.Exists (CassoLinkResume::GetRecordPath (recordFor)), S_OK);

    hr = io.ReadAllBytes (CassoLinkResume::GetRecordPath (recordFor), bytes);
    CHR (hr);

    hr = CassoLinkResume::Parse (std::string (bytes.begin(), bytes.end()), record);
    CHR (hr);

    last = CassoLinkResume::Matches (record, device, hash, direction) ? record.lastVerifiedBlock : -1;

Error:
    return last;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkBlockTransfer::WriteRecord
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkBlockTransfer::WriteRecord (IDiskFileIo & io, const std::string & recordFor, const CassoLinkResumeRecord & record)
{
    std::string  text = CassoLinkResume::Format (record);



    return io.WriteAllBytes (CassoLinkResume::GetRecordPath (recordFor), std::vector<Byte> (text.begin(), text.end()));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkBlockTransfer::FinishRead
//
//  The partial image committed as the image, then it and its record
//  removed.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkBlockTransfer::FinishRead (IDiskFileIo & io, const std::string & imagePath, uint32_t blockCount)
{
    HRESULT               hr      = S_OK;
    std::string           partial = CassoLinkResume::GetPartialPath (imagePath);
    std::string           record  = CassoLinkResume::GetRecordPath (imagePath);
    std::vector<Byte>     blocks;
    std::vector<Byte>     bytes;
    TwoImgHeader          header;
    CommitPlan::Progress  progress;



    hr = io.ReadAllBytes (partial, blocks);
    CHR (hr);

    blocks.resize ((size_t) blockCount * s_kBlock, 0);

    if (IsTwoImg (imagePath))
    {
        header.blockCount = blockCount;
        TwoImgContainer::Build (header, blocks, bytes);
    }
    else
    {
        bytes = std::move (blocks);
    }

    hr = DurableCommit::Commit (io, imagePath, bytes, GetTickCount64(), CommitMode::Replace, progress);
    CHR (hr);

    hr = io.Remove (partial);
    CHR (hr);

    if (io.Exists (record))
    {
        hr = io.Remove (record);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkBlockTransfer::IsPartialWhole
//
//  Whether the partial image is there at the device's size.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkBlockTransfer::IsPartialWhole (IDiskFileIo & io, const std::string & partial, uint32_t blockCount)
{
    HRESULT    hr    = S_OK;
    FileStamp  stamp;



    hr = io.Exists (partial) ? io.Stat (partial, stamp) : HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND);

    return SUCCEEDED (hr) && stamp.sizeBytes == (uint64_t) blockCount * s_kBlock;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkBlockTransfer::IsTwoImg
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkBlockTransfer::IsTwoImg (const std::string & imagePath)
{
    std::string  extension = std::filesystem::path (imagePath).extension().string();



    return _stricmp (extension.c_str(), ".2mg") == 0 || _stricmp (extension.c_str(), ".2img") == 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkBlockTransfer::NoteError
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkBlockTransfer::NoteError (Byte error, uint32_t block, Result & ioResult)
{
    ioResult.error       = error;
    ioResult.failedBlock = block;
    ioResult.message     = CassoLinkHost::DescribeBlockError (error, block);
}
