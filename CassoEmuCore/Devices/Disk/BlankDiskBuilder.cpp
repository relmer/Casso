#include "Pch.h"

#include "BlankDiskBuilder.h"
#include "NibbleImageCodec.h"

#include "Dos33Skeleton.h"
#include "ProDosSkeleton.h"
#include "WozLoader.h"





////////////////////////////////////////////////////////////////////////////////
//
//  WritableContainers
//
//  In the order a chooser should offer them: WOZ first, being the one that
//  carries any filesystem.
//
////////////////////////////////////////////////////////////////////////////////

const DiskFormat * BlankDiskBuilder::WritableContainers (size_t & outCount)
{
    static constexpr DiskFormat  kContainers[] =
    {
        DiskFormat::Woz,
        DiskFormat::Dsk,
        DiskFormat::Do,
        DiskFormat::Po,
        DiskFormat::Nib,
    };



    outCount = _countof (kContainers);

    return kContainers;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ContainersFor
//
//  Which containers can carry a filling.
//
//  IN CORE RATHER THAN IN THE DIALOG THAT ASKS. The create dialog used to
//  hold this as a switch restating the pairing matrix by hand, where no test
//  could reach it -- and it went stale the moment the builder learned a
//  fourth container: `.do` could be written from the command line and was
//  missing from the dropdown. Derived from CheckSpec, it cannot drift.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DiskFormat> BlankDiskBuilder::ContainersFor (BlankDiskContents contents)
{
    std::vector<DiskFormat>   usable;
    const DiskFormat        * containers = nullptr;
    size_t                    count      = 0;
    size_t                    i          = 0;
    BlankDiskSpec             spec;



    containers    = WritableContainers (count);
    spec.contents = contents;

    for (i = 0; i < count; i++)
    {
        spec.format = containers[i];

        if (CheckSpec (spec) == BlankDiskVerdict::Ok)
        {
            usable.push_back (containers[i]);
        }
    }

    return usable;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CheckSpec
//
//  Why a spec cannot be written, or Ok.
//
//  The pairing matrix:
//
//      Woz  pairs with anything (order-agnostic bit stream)
//      Dsk  pairs with DOS 3.3 or unformatted (DOS sector order)
//      Do   pairs with DOS 3.3 or unformatted, the same as Dsk -- the two
//           extensions name one container, and Build has always treated them
//           as one
//      Po   pairs with ProDOS or unformatted (ProDOS sector order)
//
//  Bootable requires formatted contents -- there is no OS to install on raw
//  media. A ProDOS spec also needs a legal volume name (1-15 chars, leading
//  letter, letters / digits / periods) since it lands in the directory
//  header verbatim.
//
//  ANSWERS IN VERDICTS RATHER THAN HRESULTS so that the command line can put
//  the broken rule into a sentence without going through E_INVALIDARG, which
//  asserts because it means a caller has a bug. Every rule here is one a
//  reader can break by typing.
//
////////////////////////////////////////////////////////////////////////////////

BlankDiskVerdict BlankDiskBuilder::CheckSpec (const BlankDiskSpec & spec)
{
    constexpr size_t  kMaxProDosNameLength = 15;



    HRESULT           hr                   = S_OK;   // vestigial, for the bails
    BlankDiskVerdict  verdict              = BlankDiskVerdict::Ok;
    bool              formatOk             = false;
    bool              bootableOk           = false;
    bool              nameOk               = true;
    bool              isProDos             = spec.contents == BlankDiskContents::ProDos;
    size_t            i                    = 0;



    switch (spec.format)
    {
        case DiskFormat::Woz:
        case DiskFormat::Nib:
            //  Both store tracks rather than sectors, so either filesystem
            //  goes in and unformatted media is expressible too.
            formatOk = true;
            break;

        case DiskFormat::Dsk:
        case DiskFormat::Do:
            formatOk = (spec.contents == BlankDiskContents::Dos33 ||
                        spec.contents == BlankDiskContents::Unformatted);
            break;

        case DiskFormat::Po:
            formatOk = (spec.contents == BlankDiskContents::ProDos ||
                        spec.contents == BlankDiskContents::Unformatted);
            break;

        default:
            formatOk = false;
            break;
    }

    CBRF (formatOk, verdict = BlankDiskVerdict::ContentsNotInContainer);

    bootableOk = !spec.bootable || spec.contents != BlankDiskContents::Unformatted;
    CBRF (bootableOk, verdict = BlankDiskVerdict::BootableNeedsFilesystem);

    //  The rest is the ProDOS volume name, so a disk that has none is done.
    BAIL_OUT_IF (!isProDos, S_OK);

    nameOk = !spec.volumeName.empty()
          && spec.volumeName.size() <= kMaxProDosNameLength
          && isalpha ((unsigned char) spec.volumeName[0]) != 0;

    for (i = 1; nameOk && i < spec.volumeName.size(); i++)
    {
        unsigned char  c = (unsigned char) spec.volumeName[i];

        nameOk = isalnum (c) != 0 || c == '.';
    }

    CBRF (nameOk, verdict = BlankDiskVerdict::ProDosNameUnusable);

Error:
    return verdict;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ValidateSpec
//
//  CheckSpec's rules as Build's own precondition.
//
//  ASSERTING ON PURPOSE, and the assert is the whole difference between the
//  two functions. A caller that takes a spec from a person settles the
//  combination before it gets here -- the create dialog by construction, the
//  `disk` subcommand through CheckSpec -- so an illegal one arriving is a
//  caller that skipped its own gate, not a reader who typed something.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BlankDiskBuilder::ValidateSpec (const BlankDiskSpec & spec)
{
    HRESULT           hr      = S_OK;
    BlankDiskVerdict  verdict = CheckSpec (spec);



    CBRAEx (verdict == BlankDiskVerdict::Ok, E_INVALIDARG);

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

    hr = WrapInContainer (spec.format, spec.nibbleTrackSize,
                          spec.contents == BlankDiskContents::Unformatted, buffer, outBytes);
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
    size_t                nibbleTrackSize,
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

        case DiskFormat::Nib:
            if (unformatted)
            {
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

            //  Zero means the caller did not name a size, which is every
            //  caller but the command line's two nibble words. The standard
            //  size is what an unnamed one is, and Build refuses anything that
            //  is neither -- so a wrong value is still a bug rather than a
            //  silently odd disk.
            hr = NibbleImageCodec::Build (img,
                                          nibbleTrackSize != 0 ? nibbleTrackSize
                                                               : NibbleImageCodec::kNibTrackSize,
                                          built);
            CHR (hr);
            break;

        default:
            CBRAEx (false, E_UNEXPECTED);
            break;
    }

    outBytes = std::move (built);

Error:
    return hr;
}
