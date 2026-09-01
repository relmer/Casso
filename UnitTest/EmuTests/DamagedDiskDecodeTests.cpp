#include "Pch.h"

#include "DamagedDisk.h"

#include "Devices/Disk/NibblizationLayer.h"
#include "Devices/Disk/SectorDecodeReport.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DamagedDiskDecodeTests
//
//  What Denibblize does when the disk is wrong.
//
//  THE REPORTLESS OVERLOAD IS THE SUBJECT. Its report-taking sibling hands the
//  caller a coverage map and is already exercised; the reportless one decides
//  FOR the caller whether the result is usable, and DiskImage::Serialize -- the
//  emulator's flush path, on eject, power cycle and reset -- is its one
//  production caller. It used to stop at the first sector it could not decode,
//  leave that sector and every later one on the track as zeros, and still
//  return S_OK, so a guest that left a track partly written lost the rest of it
//  on eject.
//
//  Every existing call to that overload in this suite is AssertSucceeded on a
//  healthy disk, which is why nothing caught it. These are the other half.
//
////////////////////////////////////////////////////////////////////////////////




TEST_CLASS (DamagedDiskDecodeTests)
{
public:

    //  The control. Without it a refusal test proves only that the call can
    //  fail, not that the damage is what failed it.
    TEST_METHOD (AnUndamagedDisk_Denibblizes)
    {
        DiskImage     image;
        vector<Byte>  decoded;

        DamagedDisk::BuildGoodDos33 (image);

        AssertSucceeded (NibblizationLayer::Denibblize (image, DiskFormat::Dsk, decoded));
        Assert::AreEqual (static_cast<size_t> (NibblizationLayer::kImageByteSize),
                          decoded.size(),
                          L"a clean disk decodes to a whole image");
    }

    //  The regression test for the defect itself.
    TEST_METHOD (APartiallyDecodableTrack_FailsInsteadOfReturningZeros)
    {
        DiskImage     image;
        vector<Byte>  decoded;

        DamagedDisk::BuildGoodDos33 (image);
        DamagedDisk::BreakSector (image, 20, 5);

        HRESULT  hr = NibblizationLayer::Denibblize (image, DiskFormat::Dsk, decoded);

        Assert::IsTrue (FAILED (hr), L"a track that stops decoding partway must not report success");
    }

    //  The same damage, through the sibling that reports rather than decides:
    //  the coverage map must actually say a sector was lost. If this fails the
    //  test above is passing for some unrelated reason.
    TEST_METHOD (APartiallyDecodableTrack_IsReportedAsDataLoss)
    {
        DiskImage           image;
        vector<Byte>        decoded;
        SectorDecodeReport  report;

        DamagedDisk::BuildGoodDos33 (image);
        DamagedDisk::BreakSector (image, 20, 5);

        AssertSucceeded (NibblizationLayer::Denibblize (image, DiskFormat::Dsk, decoded, report));

        Assert::IsTrue (report.HasDataLoss(), L"the coverage map must record the lost sector");
        Assert::IsTrue (report.GetUnrecoveredCount() > 0, L"and count it");
    }

    //  A blank track is a legitimate state, not damage. A check that cannot
    //  tell the two apart either refuses good disks or accepts broken ones, and
    //  the tempting fix for a false failure here is to loosen the check --
    //  which puts the original defect straight back.
    TEST_METHOD (AWhollyUnformattedTrack_Denibblizes_BecauseBlankIsNotDamage)
    {
        DiskImage     image;
        vector<Byte>  decoded;

        DamagedDisk::BuildGoodDos33 (image);
        DamagedDisk::WipeTrack (image, 20);

        AssertSucceeded (NibblizationLayer::Denibblize (image, DiskFormat::Dsk, decoded));
    }

    //  A sector whose header decodes cleanly into the WRONG slot. Distinct
    //  from BreakSector: nothing here fails its checksum, so the failure is
    //  about where the content landed rather than whether it could be read.
    //
    //  THIS DOES NOT DISTINGUISH COVERAGE FROM COUNTING, and an earlier version
    //  of this comment claimed it did. Redirecting sector 5 into slot 4 leaves
    //  slot 5 empty, so the unrecovered count rises and a count-based check
    //  catches it too -- verified by mutating the production check to
    //  GetUnrecoveredCount() > 0, under which all five tests here still pass.
    //
    //  The case that genuinely separates them is the one the production comment
    //  describes: sixteen slots ALL filled because one sector arrived twice.
    //  Sixteen sectors cannot do that -- it needs a track carrying a
    //  seventeenth address field -- so it is still uncovered. Producing one
    //  means splicing a field into the bit stream, and a sector is 3,164 bits,
    //  so that splice cannot be a byte-range copy.
    TEST_METHOD (ASectorDecodedIntoTheWrongSlot_Fails)
    {
        DiskImage     image;
        vector<Byte>  decoded;

        DamagedDisk::BuildGoodDos33 (image);
        DamagedDisk::RedirectSectorToSlot (image, 20, 5, 4);

        HRESULT  hr = NibblizationLayer::Denibblize (image, DiskFormat::Dsk, decoded);

        Assert::IsTrue (FAILED (hr), L"a sector landing in another's slot leaves its own empty");
    }


    //  THE HELPER MUST REACH THE WHOLE TRACK. A sector is 3,164 bits, so each
    //  one shifts the alignment by 4 bits and only 8 of the 16 address fields
    //  begin on a byte boundary. The first version of this helper scanned
    //  byte-wise, found those 8, and renumbered them -- so "sector 5" damaged
    //  sector 10 and the tests passed anyway. This is the assertion that would
    //  have caught it.
    TEST_METHOD (TheHelperAddressesAllSixteenSectors_NotTheEightThatAreByteAligned)
    {
        DiskImage  image;

        DamagedDisk::BuildGoodDos33 (image);

        Assert::AreEqual (NibblizationLayer::kSectorsPerTrack,
                          DamagedDisk::CountAddressFields (image, 20),
                          L"a bit-wise scan must find every address field on the track");
    }
};
