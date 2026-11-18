#include "Pch.h"

#include "Devices/BlockDevice/StockHardDiskImage.h"
#include "Devices/Disk/CommitPlan.h"
#include "Devices/Disk/DurableCommit.h"
#include "Devices/Disk/IDiskFileIo.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Ensure
//
//  The volume is written as `casso disk create --size 32M` writes one: an
//  empty ProDOS volume of the most blocks ProDOS addresses, with block 0
//  blank, so the SCSI card's ROM passes over it to the next slot. It is
//  created with CreateNew, so a file that appears in the meantime is not
//  replaced.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT StockHardDiskImage::Ensure (IDiskFileIo & fileIo, const std::string & folder, std::string & outPath)
{
    HRESULT                 hr       = S_OK;
    std::vector<Byte>       bytes;
    CommitPlan::Progress    progress = {};
    bool                    exists   = false;
    bool                    hasDir   = !folder.empty();



    CBRAEx (hasDir, E_INVALIDARG);

    outPath = (std::filesystem::path (folder) / kpszFileName).string();
    exists  = fileIo.Exists (outPath);
    BAIL_OUT_IF (exists, S_OK);

    bytes.resize ((size_t) kBlocks * ProDosSkeleton::kBlockByteSize, 0);

    hr = ProDosSkeleton::Write (bytes, kpszVolumeName);
    CHR (hr);

    hr = DurableCommit::Commit (fileIo, outPath, bytes, 0, CommitMode::CreateNew, progress);

    if (hr == HRESULT_FROM_WIN32 (ERROR_ALREADY_EXISTS))
    {
        hr = S_OK;
    }

    CHR (hr);

Error:
    return hr;
}
