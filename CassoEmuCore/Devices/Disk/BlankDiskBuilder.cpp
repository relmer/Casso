#include "Pch.h"

#include "BlankDiskBuilder.h"

#include "Dos33Skeleton.h"
#include "ProDosSkeleton.h"
#include "WozLoader.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DescribeSpecProblem
//
//  Why a spec cannot be built, in the words a reader sees. Empty when it can.
//
//  ONE REASON PER PROBLEM, which is the whole point of it being here rather
//  than folded into the yes-or-no check below. A single catch-all sentence told
//  somebody whose ProDOS volume name began with a digit that dsk carries
//  DOS 3.3 and po carries ProDOS, which is true, unrelated, and no help at all.
//
//  NON-ASSERTING, because every condition here is reachable by typing. The
//  check below keeps its assertions for a caller that skipped this.
//
//  THE CONTAINER NO LONGER CONSTRAINS THE FILESYSTEM. It used to: dsk accepted
//  only DOS 3.3, po only ProDOS, and do nothing at all. Sector order and
//  filesystem are independent -- a ProDOS volume in DOS order is an ordinary
//  thing, this builder already lays every skeleton down in DOS logical order
//  and orders it per container afterwards, and the reader identifies the
//  filesystem from the decoded bytes without consulting the extension. The dsk
//  and do restriction was doubly arbitrary: the two produce byte-identical
//  output, so refusing to create one while creating the other was a rule a
//  rename defeated.
//
////////////////////////////////////////////////////////////////////////////////

std::string BlankDiskBuilder::DescribeSpecProblem (const BlankDiskSpec & spec)
{
    constexpr size_t  kMaxProDosNameLength = 15;
    bool              known                = spec.format == DiskFormat::Woz || spec.format == DiskFormat::Dsk
                                          || spec.format == DiskFormat::Do  || spec.format == DiskFormat::Po;
    bool              nameOk               = true;
    size_t            i                    = 0;



    if (!known)
    {
        return "that container type cannot be created: dsk, do, po and woz can";
    }

    if (spec.bootable && spec.contents == BlankDiskContents::Unformatted)
    {
        return "an unformatted disk cannot be made bootable: there is no filesystem "
               "for an operating system to be installed into";
    }

    if (spec.contents != BlankDiskContents::ProDos)
    {
        return std::string();
    }

    nameOk = !spec.volumeName.empty()
          && spec.volumeName.size() <= kMaxProDosNameLength
          && isalpha ((unsigned char) spec.volumeName[0]) != 0;

    for (i = 1; nameOk && i < spec.volumeName.size(); i++)
    {
        unsigned char  c = (unsigned char) spec.volumeName[i];

        nameOk = isalnum (c) != 0 || c == '.';
    }

    if (!nameOk)
    {
        return "a ProDOS volume name is 1 to 15 characters, begins with a letter, "
               "and holds only letters, digits and periods";
    }

    return std::string();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ValidateSpec
//
//  The same rules as a yes or no, for the build path itself.
//
//  Asserting variants on purpose: every caller is expected to have asked the
//  routine above first, so a spec arriving here invalid is a caller that
//  skipped it rather than a user outcome. That was already the intent; what
//  changed is that there is now something for a caller to ask.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BlankDiskBuilder::ValidateSpec (const BlankDiskSpec & spec)
{
    HRESULT      hr      = S_OK;
    std::string  problem = DescribeSpecProblem (spec);
    bool         valid   = problem.empty();



    CBRAEx (valid, E_INVALIDARG);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReorderDosToPo
//
//  The skeleton buffer is DOS 3.3 logical order; a .po file stores the same
//  track's sixteen 256-byte sectors in ProDOS-block order. File index ->
//  DOS logical sector comes from composing the 2:1 ProDOS physical
//  interleave with the DOS 3.3 physical-to-logical skew.
//
////////////////////////////////////////////////////////////////////////////////

void BlankDiskBuilder::ReorderDosToPo (const vector<Byte> & dosOrdered, vector<Byte> & outPo)
{
    constexpr int  kPoFileToDosLogical[16] =
    {
        0, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 15
    };



    int  track = 0;
    int  file  = 0;



    outPo.assign (dosOrdered.size(), (Byte) 0);

    for (track = 0; track < NibblizationLayer::kTrackCount; track++)
    {
        for (file = 0; file < NibblizationLayer::kSectorsPerTrack; file++)
        {
            size_t  src = ((size_t) track * NibblizationLayer::kSectorsPerTrack
                          + (size_t) kPoFileToDosLogical[file])
                        * (size_t) NibblizationLayer::kSectorByteSize;
            size_t  dst = ((size_t) track * NibblizationLayer::kSectorsPerTrack
                          + (size_t) file)
                        * (size_t) NibblizationLayer::kSectorByteSize;

            std::copy (dosOrdered.begin() + src,
                       dosOrdered.begin() + src + NibblizationLayer::kSectorByteSize,
                       outPo.begin() + dst);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Build
//
//  Skeleton buffer -> optional payload install -> format-specific output
//  bytes. All-or-nothing: outBytes is written only after every step
//  succeeded, so a failed build leaves the caller's vector untouched.
//  Deterministic by construction -- no clocks, no randomness -- so identical
//  inputs yield identical bytes.
//
//  Formatted contents produce the buffer via the skeleton writers (always in
//  DOS 3.3 logical layout; ProDOS blocks land through the interleave map).
//  The WOZ path nibblizes it (the same GCR encoding a mounted .dsk gets) and
//  serializes WOZ v2 with the write-protect INFO byte clear (a new disk is
//  never born protected). DSK output is the buffer verbatim; PO output is
//  the buffer reordered to ProDOS-block order.
//
//  "Unformatted" WOZ carries every track sized to the full bit capacity with
//  all-zero bits: the guest reads it as garbage (exactly like raw media) and
//  a guest INIT lays its own sync + sectors over the rotating track. An
//  unformatted sector image is all zeros -- no filesystem until the guest
//  INITs it.
//
//  A bootable spec additionally installs the OS from the payload: DOS 3.3
//  gets the master's tracks 0-2 plus a HELLO greeting; ProDOS gets the
//  Users Disk's boot blocks plus PRODOS and BASIC.SYSTEM as real files. An
//  empty payload fails the build cleanly -- availability is the shell's
//  problem, checked before Build is called.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BlankDiskBuilder::Build (
    const BlankDiskSpec & spec,
    const BootPayload   & payload,
    vector<Byte>        & outBytes)
{
    HRESULT       hr    = S_OK;
    vector<Byte>  buffer;



    hr = ValidateSpec (spec);
    CHR (hr);

    buffer.assign ((size_t) NibblizationLayer::kImageByteSize, (Byte) 0);

    if (spec.contents == BlankDiskContents::Dos33)
    {
        hr = Dos33Skeleton::Write (buffer, spec.volumeNumber);
        CHR (hr);

        if (spec.bootable)
        {
            bool  havePayload = !payload.dosMasterSectors.empty();

            CBREx (havePayload, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));

            hr = Dos33Skeleton::InstallDos (buffer, payload.dosMasterSectors);
            CHR (hr);

            hr = Dos33FileWriter::WriteHello (buffer);
            CHR (hr);
        }
    }
    else if (spec.contents == BlankDiskContents::ProDos)
    {
        hr = ProDosSkeleton::Write (buffer, spec.volumeName);
        CHR (hr);

        if (spec.bootable)
        {
            bool  havePayload = !payload.proDosUsersDisk.empty();

            CBREx (havePayload, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));

            hr = ProDosSkeleton::InstallBoot (buffer, payload.proDosUsersDisk);
            CHR (hr);
        }
    }

    hr = WrapInContainer (spec.format, spec.contents == BlankDiskContents::Unformatted, buffer, outBytes);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlankDiskBuilder::WrapInContainer
//
//  A DOS-ordered sector buffer written as the container the caller asked for.
//
//  SPLIT OUT BECAUSE IT IS NOT THIS BUILDER'S ALONE. DirectBootBuilder produces
//  the same 143,360-byte DOS-ordered buffer and needs the same three answers,
//  and a second copy of the .po reordering or the WOZ nibblization is a second
//  place for the sector skew to be got wrong.
//
//  `unformatted` asks for raw media rather than an empty filesystem, and only
//  changes the WOZ arm: a disk with no structure gets full-capacity all-zero
//  bit tracks instead of a nibblized image of a buffer that holds nothing.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BlankDiskBuilder::WrapInContainer (
    DiskFormat            format,
    bool                  unformatted,
    const vector<Byte> &  sectors,
    vector<Byte>       &  outBytes)
{
    HRESULT       hr    = S_OK;
    int           track = 0;
    vector<Byte>  built;
    DiskImage     img;



    switch (format)
    {
        case DiskFormat::Woz:
            if (unformatted)
            {
                // Raw media: full-capacity all-zero bit tracks, no structure.
                for (track = 0; track < NibblizationLayer::kTrackCount; track++)
                {
                    img.ResizeTrack (track, NibblizationLayer::kTrackBitCapacity);
                }
            }
            else
            {
                hr = NibblizationLayer::NibblizeDsk (sectors, img);
                CHR (hr);
            }

            hr = WozLoader::Serialize (img, built);
            CHR (hr);
            break;

        case DiskFormat::Dsk:
        case DiskFormat::Do:
            built = sectors;
            break;

        case DiskFormat::Po:
            ReorderDosToPo (sectors, built);
            break;

        default:
            CBRAEx (false, E_UNEXPECTED);
            break;
    }

    outBytes = std::move (built);

Error:
    return hr;
}
