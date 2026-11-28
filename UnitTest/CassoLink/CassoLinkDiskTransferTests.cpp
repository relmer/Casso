#include "Pch.h"

#include "../EhmTestHelper.h"
#include "CassoLink/Session/CassoLinkDiskTransfer.h"
#include "CassoLink/Session/TrackMeasures.h"
#include "Machines/Apple2/Common/BlankDiskBuilder.h"
#include "Machines/Apple2/Common/NibbleFormat.h"
#include "Machines/Apple2/Common/NibbleImageCodec.h"
#include "Machines/Apple2/Common/TrackLayoutWriter.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkDiskTransferTests
//
//  Which reads measure their tracks, and the image built from a disk's
//  sectors, without a link.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoLinkDiskTransferTests)
{
public:

    static std::vector<Byte>  MakeDisk()
    {
        std::vector<Byte>  disk (NibbleFormat::kImageByteSize, 0);

        for (size_t offset = 0; offset < disk.size(); offset++)
        {
            disk[offset] = (Byte) ((offset * 5 + offset / 256) & 0xFF);
        }

        return disk;
    }

    static std::vector<CassoLinkHost::TrackMeasure>  MakeMeasures()
    {
        std::vector<CassoLinkHost::TrackMeasure>  measures;

        for (int track = 0; track < NibbleFormat::kTrackCount; track++)
        {
            measures.push_back (CassoLinkHost::TrackMeasure { (Word) (track == 0 ? 0 : 1000 + track * 7), 6330, 254 });
        }

        return measures;
    }




    TEST_METHOD (OnlyAWoz_IsMeasured)
    {
        Assert::IsTrue  (CassoLinkDiskTransfer::IsMeasured (DiskFormat::Woz));
        Assert::IsFalse (CassoLinkDiskTransfer::IsMeasured (DiskFormat::Dsk));
        Assert::IsFalse (CassoLinkDiskTransfer::IsMeasured (DiskFormat::Do));
        Assert::IsFalse (CassoLinkDiskTransfer::IsMeasured (DiskFormat::Po));
        Assert::IsFalse (CassoLinkDiskTransfer::IsMeasured (DiskFormat::Nib));
    }




    TEST_METHOD (BuildImage_ToAWoz_IsBuiltToTheMeasuredLayouts)
    {
        std::vector<Byte>  disk     = MakeDisk();
        auto               measures = MakeMeasures();
        std::vector<Byte>  built;
        std::vector<Byte>  wrapped;

        AssertSucceeded (CassoLinkDiskTransfer::BuildImage ("out.woz", DiskFormat::Woz, disk, measures, TrackLayoutWriter::kDefaultBits, built));
        AssertSucceeded (BlankDiskBuilder::WrapInContainer (DiskFormat::Woz, 0, false, 0, disk, wrapped));

        Assert::IsTrue  (built.size() > 4 && memcmp (built.data(), "WOZ2", 4) == 0, L"a WOZ 2 image");
        Assert::IsFalse (built == wrapped, L"built to the measured layouts, not nibblized as a blank disk is");
    }




    TEST_METHOD (BuildImage_ToOtherFormats_WrapsTheSectors_AtThePathsTrackSize)
    {
        std::vector<Byte>  disk = MakeDisk();
        std::vector<Byte>  built;
        std::vector<Byte>  expected;

        AssertSucceeded (CassoLinkDiskTransfer::BuildImage ("out.po", DiskFormat::Po, disk, {}, 0, built));
        AssertSucceeded (BlankDiskBuilder::WrapInContainer (DiskFormat::Po, 0, false, 0, disk, expected));
        Assert::IsTrue  (built == expected, L"a .po is the wrapped sectors");

        AssertSucceeded (CassoLinkDiskTransfer::BuildImage ("out.nb2", DiskFormat::Nib, disk, {}, 0, built));
        Assert::AreEqual (NibbleImageCodec::kNb2ImageSize, built.size(), L"a .nb2 has .nb2's tracks");

        AssertSucceeded (CassoLinkDiskTransfer::BuildImage ("out.nib", DiskFormat::Nib, disk, {}, 0, built));
        Assert::AreEqual (NibbleImageCodec::kNibImageSize, built.size());
    }




    //  A raw capture has no catalog read, so from its first read the drive's
    //  title is the image file's name.
    TEST_METHOD (ARawCapture_TitlesItsDriveWithTheFileName_FromTheStart)
    {
        LinkOperation       operation;
        FullSendSnapshot    snapshot;

        operation.drive     = 1;
        operation.imagePath = "C:/Disks/Karateka side A.woz";
        operation.status    = std::make_shared<FullSendStatus> ([] () { return 0.0; }, [] () { return std::string(); });

        CassoLinkDiskTransfer::BeginStatus (operation, true);

        snapshot = operation.status->GetSnapshot();
        Assert::IsTrue   (snapshot.drives[0].state == FullSendDiskState::Capturing);
        Assert::AreEqual (std::string ("Karateka side A"), snapshot.drives[0].title);
    }



    TEST_METHOD (Automatic_ATrack0WithNoSectorRead_IsCapturedRaw)
    {
        std::vector<Byte>                  disk (NibbleFormat::kImageByteSize, 0);
        std::vector<std::pair<int, int>>   unreadable;
        FullSendProtectionChoices          none { false, false, false, false, false, false };

        for (int sector = 0; sector < NibbleFormat::kSectorsPerTrack; sector++)
        {
            unreadable.push_back ({ 0, sector });
        }

        Assert::IsTrue (CassoLinkDiskTransfer::IsAutomaticRaw (disk, unreadable, none));
    }




    TEST_METHOD (Automatic_WithNoSignChecked_IsASectorImage)
    {
        std::vector<Byte>          disk (NibbleFormat::kImageByteSize, 0);
        FullSendProtectionChoices  none { false, false, false, false, false, false };

        Assert::IsFalse (CassoLinkDiskTransfer::IsAutomaticRaw (disk, { { 5, 3 } }, none));
    }
};
