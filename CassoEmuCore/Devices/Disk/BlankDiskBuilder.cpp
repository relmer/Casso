#include "Pch.h"

#include "BlankDiskBuilder.h"

#include "Dos33Skeleton.h"
#include "ProDosSkeleton.h"
#include "WozLoader.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ValidateSpec
//
//  The pairing matrix:
//
//      Woz  pairs with anything (order-agnostic bit stream)
//      Dsk  pairs with DOS 3.3 or unformatted (DOS sector order)
//      Po   pairs with ProDOS or unformatted (ProDOS sector order)
//      Do   is not offered for creation (mountable, never created)
//
//  Bootable requires formatted contents -- there is no OS to install on raw
//  media. A ProDOS spec also needs a legal volume name (1-15 chars, leading
//  letter, letters / digits / periods) since it lands in the directory
//  header verbatim.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BlankDiskBuilder::ValidateSpec (const BlankDiskSpec & spec)
{
    HRESULT  hr        = S_OK;
    bool     formatOk  = false;
    bool     nameOk    = true;
    size_t   i         = 0;



    switch (spec.format)
    {
        case DiskFormat::Woz:
            formatOk = true;
            break;

        case DiskFormat::Dsk:
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

    // Asserting variants on purpose: the dialog gates every illegal pairing
    // before the builder is ever called, so an invalid spec arriving here is
    // a caller bug, not a user outcome.
    CBRAEx (formatOk, E_INVALIDARG);
    CBRAEx (!spec.bootable || spec.contents != BlankDiskContents::Unformatted, E_INVALIDARG);

    if (spec.contents == BlankDiskContents::ProDos)
    {
        constexpr size_t  kMaxProDosNameLength = 15;

        nameOk = !spec.volumeName.empty()
              && spec.volumeName.size() <= kMaxProDosNameLength
              && isalpha ((unsigned char) spec.volumeName[0]) != 0;

        for (i = 1; nameOk && i < spec.volumeName.size(); i++)
        {
            unsigned char  c = (unsigned char) spec.volumeName[i];

            nameOk = isalnum (c) != 0 || c == '.';
        }

        CBRAEx (nameOk, E_INVALIDARG);
    }

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
//  Boot-payload install is not yet implemented; a bootable spec fails
//  cleanly as E_NOTIMPL rather than emitting a wrong image.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BlankDiskBuilder::Build (
    const BlankDiskSpec & spec,
    const BootPayload   & payload,
    vector<Byte>        & outBytes)
{
    HRESULT       hr    = S_OK;
    int           track = 0;
    vector<Byte>  buffer;
    vector<Byte>  built;
    DiskImage     img;



    UNREFERENCED_PARAMETER (payload);

    hr = ValidateSpec (spec);
    CHR (hr);

    CBREx (!spec.bootable, E_NOTIMPL);

    buffer.assign ((size_t) NibblizationLayer::kImageByteSize, (Byte) 0);

    if (spec.contents == BlankDiskContents::Dos33)
    {
        hr = Dos33Skeleton::Write (buffer, spec.volumeNumber);
        CHR (hr);
    }
    else if (spec.contents == BlankDiskContents::ProDos)
    {
        hr = ProDosSkeleton::Write (buffer, spec.volumeName);
        CHR (hr);
    }

    switch (spec.format)
    {
        case DiskFormat::Woz:
            if (spec.contents == BlankDiskContents::Unformatted)
            {
                // Raw media: full-capacity all-zero bit tracks, no structure.
                for (track = 0; track < NibblizationLayer::kTrackCount; track++)
                {
                    img.ResizeTrack (track, NibblizationLayer::kTrackBitCapacity);
                }
            }
            else
            {
                hr = NibblizationLayer::NibblizeDsk (buffer, img);
                CHR (hr);
            }

            hr = WozLoader::Serialize (img, built);
            CHR (hr);
            break;

        case DiskFormat::Dsk:
            built = buffer;
            break;

        case DiskFormat::Po:
            ReorderDosToPo (buffer, built);
            break;

        default:
            // ValidateSpec admits exactly the three formats above.
            CBRAEx (false, E_UNEXPECTED);
            break;
    }

    outBytes = std::move (built);

Error:
    return hr;
}
