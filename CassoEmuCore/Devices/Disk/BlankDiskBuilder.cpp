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
//  bytes. Implemented across T006 (WOZ) and T014 (full matrix).
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BlankDiskBuilder::Build (
    const BlankDiskSpec & spec,
    const BootPayload   & payload,
    vector<Byte>        & outBytes)
{
    UNREFERENCED_PARAMETER (spec);
    UNREFERENCED_PARAMETER (payload);
    UNREFERENCED_PARAMETER (outBytes);

    return E_NOTIMPL;
}
