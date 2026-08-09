#include "Pch.h"

#include "BlankDiskBuilder.h"

#include "Dos33Skeleton.h"
#include "ProDosSkeleton.h"
#include "WozLoader.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ValidateSpec
//
//  The FR-010 pairing matrix:
//
//      Woz  pairs with anything (order-agnostic bit stream)
//      Dsk  pairs with DOS 3.3 or unformatted (DOS sector order)
//      Po   pairs with ProDOS or unformatted (ProDOS sector order)
//      Do   is not offered by this feature (mountable, never created)
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
    // before the builder is ever called (FR-010), so an invalid spec arriving
    // here is a caller bug, not a user outcome.
    CBRAEx (formatOk, E_INVALIDARG);
    CBRAEx (!spec.bootable || spec.contents != BlankDiskContents::Unformatted, E_INVALIDARG);

    if (spec.contents == BlankDiskContents::ProDos)
    {
        static constexpr size_t  s_kMaxProDosNameLength = 15;

        nameOk = !spec.volumeName.empty()
              && spec.volumeName.size() <= s_kMaxProDosNameLength
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
//  Build
//
//  Skeleton buffer -> optional payload install -> format-specific output
//  bytes. All-or-nothing: outBytes is written only after every step
//  succeeded (FR-011 starts here). Deterministic by construction -- no
//  clocks, no randomness -- so identical inputs yield identical bytes.
//
//  Formatted contents produce the buffer via the skeleton writers; the WOZ
//  path nibblizes it (the same GCR encoding a mounted .dsk gets) and
//  serializes WOZ v2 with the write-protect INFO byte clear (FR-012).
//
//  "Unformatted" WOZ carries every track sized to the full bit capacity with
//  all-zero bits: the guest reads it as garbage (exactly like raw media) and
//  a guest INIT lays its own sync + sectors over the rotating track.
//
//  ProDOS contents and the DSK/PO outputs arrive in T014; boot-payload
//  install arrives in T018.
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

    // Later-task scaffolding: valid specs whose construction path isn't
    // built yet fail cleanly rather than emitting a wrong image.
    CBREx (!spec.bootable, E_NOTIMPL);                                   // T018
    CBREx (spec.contents != BlankDiskContents::ProDos, E_NOTIMPL);       // T014
    CBREx (spec.format == DiskFormat::Woz, E_NOTIMPL);                   // T014

    buffer.assign ((size_t) NibblizationLayer::kImageByteSize, (Byte) 0);

    if (spec.contents == BlankDiskContents::Dos33)
    {
        hr = Dos33Skeleton::Write (buffer, spec.volumeNumber);
        CHR (hr);
    }

    if (spec.contents == BlankDiskContents::Unformatted)
    {
        // Raw media: full-capacity all-zero bit tracks, no sector structure.
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

    outBytes = std::move (built);

Error:
    return hr;
}
