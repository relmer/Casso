#pragma once

#include "Pch.h"

#include "CassoLink/CassoLinkHost.h"
#include "CassoLink/Session/CassoLinkResume.h"

class IDiskFileIo;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkBlockTransfer
//
//  A whole block device read to an image file, or an image written to one,
//  over the link (FR-192), kRunBlocks blocks a request. A read's blocks go
//  into NAME.partial as they arrive; after every CassoLinkResume::kEveryBlocks
//  the partial image is flushed and its record written beside it, and after
//  the last block the partial image becomes the image: a .2mg wrapped in its
//  header, any other extension the blocks as they are. A write's record is
//  beside the image it writes from. A transfer the link, a device error or
//  a cancel cuts short has its record saved, and one started with resume set,
//  for the same device in the same direction with the same volume directory,
//  continues after the record's last block.
//
////////////////////////////////////////////////////////////////////////////////

class CassoLinkBlockTransfer
{
public:
    static constexpr Byte  kRunBlocks = 32;

    using ProgressFn = std::function<void (uint32_t done, uint32_t total)>;
    using CancelFn   = std::function<bool ()>;

    struct Result
    {
        Byte         error       = 0;   // the device's, from a Status
        uint32_t     failedBlock = 0;
        uint32_t     firstBlock  = 0;   // the first block this transfer did; past 0 when it continued another
        std::string  message;           // the device's error in words
    };

    //  The device's blocks into imagePath. ERROR_READ_FAULT for a device
    //  error, ERROR_CANCELLED for a cancel, or the link's failure.
    static HRESULT  ReadDevice  (ICassoLinkBlockIo & blocks, IDiskFileIo & io, const CassoLinkBlockDevice & device, const std::string & imagePath, bool verify, bool resume,
                                 const ProgressFn & progress, const CancelFn & cancel, Result & outResult);

    //  imagePath's blocks written to the device, which must have as many or
    //  more. ERROR_WRITE_FAULT for a device error, as ReadDevice otherwise.
    static HRESULT  WriteDevice (ICassoLinkBlockIo & blocks, IDiskFileIo & io, const CassoLinkBlockDevice & device, const std::string & imagePath, bool verify, bool resume,
                                 const ProgressFn & progress, const CancelFn & cancel, Result & outResult);

    //  The block a transfer of the device in that direction would continue
    //  from, for the offer to continue; false with none to continue.
    static bool     FindResume  (ICassoLinkBlockIo & blocks, IDiskFileIo & io, const CassoLinkBlockDevice & device, const std::string & imagePath, bool isRead, uint32_t & outNext);

    //  An image file's blocks: a 2IMG's payload, or the file as it is.
    static HRESULT  LoadImage   (IDiskFileIo & io, const std::string & imagePath, std::vector<Byte> & outBlocks);

private:
    static HRESULT  HashDevice     (ICassoLinkBlockIo & blocks, const CassoLinkBlockDevice & device, std::string & outHex);
    static HRESULT  HashImage      (std::span<const Byte> image, std::string & outHex);
    static int64_t  FindRecord     (IDiskFileIo & io, const std::string & recordFor, const CassoLinkBlockDevice & device, const std::string & hash, const char * direction);
    static HRESULT  WriteRecord    (IDiskFileIo & io, const std::string & recordFor, const CassoLinkResumeRecord & record);
    static HRESULT  FinishRead     (IDiskFileIo & io, const std::string & imagePath, uint32_t blockCount);
    static bool     IsTwoImg       (const std::string & imagePath);
    static bool     IsPartialWhole (IDiskFileIo & io, const std::string & partial, uint32_t blockCount);
    static void     NoteError      (Byte error, uint32_t block, Result & ioResult);
};
