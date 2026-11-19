#include "Pch.h"

#include "../EhmTestHelper.h"
#include "CassoLink/Session/CassoLinkDiskTransfer.h"
#include "CassoLink/Session/TrackMeasures.h"
#include "Machines/Apple2/Common/BlankDiskBuilder.h"
#include "Machines/Apple2/Common/NibbleImageCodec.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
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
        std::vector<Byte>  disk (NibblizationLayer::kImageByteSize, 0);

        for (size_t offset = 0; offset < disk.size(); offset++)
        {
            disk[offset] = (Byte) ((offset * 5 + offset / 256) & 0xFF);
        }

        return disk;
    }

    static std::vector<CassoLinkHost::TrackMeasure>  MakeMeasures()
    {
        std::vector<CassoLinkHost::TrackMeasure>  measures;

        for (int track = 0; track < NibblizationLayer::kTrackCount; track++)
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
};
