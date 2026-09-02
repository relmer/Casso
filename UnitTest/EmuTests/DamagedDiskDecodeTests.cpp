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
    //  GetUnrecoveredCount() > 0, under which every test in this file but the
    //  two after this one still passes.
    //
    //  The case that genuinely separates them is the one the production comment
    //  describes: sixteen slots ALL filled because one sector arrived twice.
    //  Sixteen sectors cannot do that -- it needs a track carrying a
    //  seventeenth address field -- and the two tests that follow build one.
    TEST_METHOD (ASectorDecodedIntoTheWrongSlot_Fails)
    {
        DiskImage     image;
        vector<Byte>  decoded;

        DamagedDisk::BuildGoodDos33 (image);
        DamagedDisk::RedirectSectorToSlot (image, 20, 5, 4);

        HRESULT  hr = NibblizationLayer::Denibblize (image, DiskFormat::Dsk, decoded);

        Assert::IsTrue (FAILED (hr), L"a sector landing in another's slot leaves its own empty");
    }

    //  THE CASE THAT SEPARATES COVERAGE FROM COUNTING, and the one the
    //  production comment on the strict overload describes: sector 5 arrives
    //  twice and nothing is missing. All sixteen slots are filled, the
    //  unrecovered count is zero, and every sector count reads the track as
    //  complete. The only thing wrong is that one slot was claimed by two
    //  headers, and the buffer holds whichever copy landed last -- which the
    //  strict overload has to refuse, because Serialize would write it over the
    //  user's file as a clean save.
    //
    //  Sixteen sectors cannot fill sixteen slots and duplicate one, so the
    //  track is given a SEVENTEENTH address field: a second copy of sector 5,
    //  spliced bit-wise directly after the first. THE TRACK GROWS BY THE COPY
    //  rather than giving up sync gap for it. A sector is 3,164 bits and the
    //  whole track carries 4,160 bits of sync, so paying for the copy out of
    //  gap would strip nearly every self-sync nibble from every sector and make
    //  this a test of gapless decoding, not of duplication. Growing leaves
    //  every original bit and gap exactly where the builder put it, and the
    //  decoder walks whatever bit count it is handed -- WOZ track lengths vary
    //  anyway -- so the ONLY thing different about the track is the extra
    //  sector.
    TEST_METHOD (ASectorArrivingTwice_FailsAlthoughEverySlotIsFilled)
    {
        DiskImage     image;
        vector<Byte>  decoded;

        DamagedDisk::BuildGoodDos33 (image);
        DamagedDisk::DuplicateSector (image, 20, 5);

        Assert::AreEqual (NibblizationLayer::kSectorsPerTrack + 1,
                          DamagedDisk::CountAddressFields (image, 20),
                          L"the splice must leave the track carrying a seventeenth address field");

        HRESULT  hr = NibblizationLayer::Denibblize (image, DiskFormat::Dsk, decoded);

        Assert::IsTrue (FAILED (hr), L"a slot filled twice is damage, however full the track reads");
    }

    //  The same track through the sibling that reports, holding the two
    //  answers apart: coverage says the track is damaged, the count says
    //  nothing was lost. This is what keeps the refusal above from collapsing
    //  into the partial-decode case. If the unrecovered count ever rises here,
    //  the seventeenth field has stopped landing where the decoder reads it,
    //  and the refusal above is passing for the reason RedirectSectorToSlot's
    //  does rather than the one it claims.
    TEST_METHOD (ASectorArrivingTwice_IsDataLossWithNothingUnrecovered)
    {
        DiskImage           image;
        vector<Byte>        decoded;
        SectorDecodeReport  report;

        DamagedDisk::BuildGoodDos33 (image);
        DamagedDisk::DuplicateSector (image, 20, 5);

        AssertSucceeded (NibblizationLayer::Denibblize (image, DiskFormat::Dsk, decoded, report));

        Assert::IsTrue (TrackDecodeOutcome::Partial == report.GetOutcome (20),
                        L"a slot claimed twice leaves the track damaged");
        Assert::IsTrue (report.IsDuplicated (20), L"and the report says which way");
        Assert::IsTrue (report.HasDataLoss(), L"coverage reads it as loss");
        Assert::AreEqual (0, report.GetUnrecoveredCount(),
                          L"while the count reads it as complete, because every slot is filled");
        Assert::IsTrue (SectorDecodeReport::kFullCoverage == report.GetCoverage (20),
                        L"all sixteen slots, none of them empty");
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
