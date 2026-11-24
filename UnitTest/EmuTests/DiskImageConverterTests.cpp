#include "Pch.h"

#include "../EhmTestHelper.h"
#include "Devices/Disk/DiskImage.h"
#include "Devices/Disk/Containers/TwoImgContainer.h"
#include "Machines/Apple2/Common/DiskImageConverter.h"
#include "Machines/Apple2/Common/NibbleFormat.h"
#include "Machines/Apple2/Common/NibbleImageCodec.h"
#include "Machines/Apple2/Common/Nibblizer.h"
#include "Machines/Apple2/Common/SectorDecodeReport.h"
#include "Machines/Apple2/Common/ThirteenSectorCodec.h"
#include "Machines/Apple2/Common/VolumeImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageConverterTests
//
//  Disk images converted between formats in memory: sector orders, 2IMG
//  and WOZ, and a damaged nibble image that can't become a sector image.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DiskImageConverterTests)
{
public:

    using Image = std::vector<Byte>;

    //  An image of the given size with different bytes in every sector.
    static Image  MakeSectors (size_t size)
    {
        Image  sectors (size);



        for (size_t at = 0; at < sectors.size(); at++)
        {
            sectors[at] = (Byte) (at * 7 + at / 256);
        }

        return sectors;
    }

    static bool  HasTarget (const std::vector<DiskImageFormat> & targets, DiskImageFormat format)
    {
        return std::find (targets.begin(), targets.end(), format) != targets.end();
    }

    TEST_METHOD (GetTargets_LeavesOutTheSourceFormat)
    {
        static constexpr uint64_t  kVolumeBytes  = 800 * 1024;
        static constexpr uint64_t  kWozBytes     = 234567;
        static constexpr uint64_t  kArchiveBytes = 4096;

        std::vector<DiskImageFormat>  targets;



        DiskImageConverter::GetTargets ("C:\\Disks\\game.dsk", NibbleFormat::kImageByteSize, targets);
        Assert::AreEqual (size_t (3), targets.size());
        Assert::IsFalse  (HasTarget (targets, DiskImageFormat::Dsk));
        Assert::IsTrue   (HasTarget (targets, DiskImageFormat::Woz));

        DiskImageConverter::GetTargets ("C:\\Disks\\big.po", kVolumeBytes, targets);
        Assert::AreEqual (size_t (2), targets.size());
        Assert::IsTrue   (HasTarget (targets, DiskImageFormat::TwoImg));
        Assert::IsTrue   (HasTarget (targets, DiskImageFormat::Hdv));

        DiskImageConverter::GetTargets ("C:\\Disks\\old.D13", ThirteenSectorCodec::kImageByteSize, targets);
        Assert::AreEqual (size_t (1), targets.size());
        Assert::IsTrue   (HasTarget (targets, DiskImageFormat::Woz));

        DiskImageConverter::GetTargets ("C:\\Disks\\copy.woz", kWozBytes, targets);
        Assert::AreEqual (size_t (2), targets.size());
        Assert::IsTrue   (HasTarget (targets, DiskImageFormat::Dsk));
        Assert::IsTrue   (HasTarget (targets, DiskImageFormat::Po));

        DiskImageConverter::GetTargets ("C:\\Disks\\notes.shk", kArchiveBytes, targets);
        Assert::IsTrue   (targets.empty());
    }

    TEST_METHOD (DskToPoToDsk_IsIdentical)
    {
        Image        dsk = MakeSectors (NibbleFormat::kImageByteSize);
        Image        po;
        Image        back;
        Image        expected;
        std::string  reason;



        AssertSucceeded (DiskImageConverter::Convert (dsk, "C:\\Disks\\game.dsk", DiskImageFormat::Po, po, reason));
        VolumeImage::DosLogicalToProDosFile (dsk, expected);
        Assert::IsTrue (po == expected);

        AssertSucceeded (DiskImageConverter::Convert (po, "C:\\Disks\\game.po", DiskImageFormat::Dsk, back, reason));
        Assert::IsTrue (back == dsk);
    }

    TEST_METHOD (DskToWozToDsk_IsIdentical)
    {
        Image        dsk = MakeSectors (NibbleFormat::kImageByteSize);
        Image        woz;
        Image        back;
        std::string  reason;



        AssertSucceeded (DiskImageConverter::Convert (dsk, "C:\\Disks\\game.dsk", DiskImageFormat::Woz, woz, reason));
        Assert::IsTrue (woz.size() > 4 && woz[0] == 'W' && woz[1] == 'O' && woz[2] == 'Z' && woz[3] == '2');

        AssertSucceeded (DiskImageConverter::Convert (woz, "C:\\Disks\\game.woz", DiskImageFormat::Dsk, back, reason));
        Assert::IsTrue (back == dsk);
    }

    TEST_METHOD (D13ToWoz_Decodes)
    {
        Image               d13     = MakeSectors (ThirteenSectorCodec::kImageByteSize);
        Image               woz;
        Image               decoded;
        Image               notMade;
        SectorDecodeReport  report;
        std::string         reason;
        HRESULT             hr      = S_OK;



        AssertSucceeded (DiskImageConverter::Convert (d13, "C:\\Disks\\old.d13", DiskImageFormat::Woz, woz, reason));

        AssertSucceeded (VolumeImage::Load (woz, "C:\\Disks\\old.woz", decoded, report));
        Assert::IsTrue (decoded == d13);

        hr = DiskImageConverter::Convert (woz, "C:\\Disks\\old.woz", DiskImageFormat::Dsk, notMade, reason);
        Assert::IsTrue (FAILED (hr));
        Assert::IsTrue (reason.find ("13-sector") != std::string::npos);
        Assert::IsTrue (notMade.empty());
    }

    TEST_METHOD (DamagedNib_RefusesWithReason)
    {
        static constexpr int  kTrack = 17;

        Image        dsk    = MakeSectors (NibbleFormat::kImageByteSize);
        Image        nib;
        Image        out;
        DiskImage    image;
        std::string  reason;
        HRESULT      hr     = S_OK;
        size_t       start  = kTrack * NibbleImageCodec::kNibTrackSize + NibbleImageCodec::kNibTrackSize / 2;



        AssertSucceeded (Nibblizer::NibblizeDsk (dsk, image));
        AssertSucceeded (NibbleImageCodec::Build (image, NibbleImageCodec::kNibTrackSize, nib));

        //  The second half of one track is sync nibbles, so its sectors
        //  there are gone.
        std::fill (nib.begin() + (ptrdiff_t) start, nib.begin() + (ptrdiff_t) (start + NibbleImageCodec::kNibTrackSize / 2), NibbleImageCodec::kSyncNibble);

        hr = DiskImageConverter::Convert (nib, "C:\\Disks\\game.nib", DiskImageFormat::Dsk, out, reason);
        Assert::IsTrue (FAILED (hr));
        Assert::IsTrue (reason.find ("Track 17") != std::string::npos);
        Assert::IsTrue (out.empty());
    }

    TEST_METHOD (TwoImg_HasAProDosOrderHeader)
    {
        static constexpr size_t    kHeaderSize = 64;
        static constexpr size_t    kFormatAt   = 12;
        static constexpr size_t    kBlocksAt   = 20;
        static constexpr size_t    kDataAt     = 24;
        static constexpr size_t    kLengthAt   = 28;
        static constexpr uint32_t  kBlocks     = 280;

        Image        dsk = MakeSectors (NibbleFormat::kImageByteSize);
        Image        made;
        Image        blocks;
        std::string  reason;
        auto         read32 = [&made] (size_t at) { return (uint32_t) made[at] | ((uint32_t) made[at + 1] << 8) | ((uint32_t) made[at + 2] << 16) | ((uint32_t) made[at + 3] << 24); };



        AssertSucceeded (DiskImageConverter::Convert (dsk, "C:\\Disks\\game.dsk", DiskImageFormat::TwoImg, made, reason));

        Assert::AreEqual (kHeaderSize + NibbleFormat::kImageByteSize, made.size());
        Assert::IsTrue   (made[0] == '2' && made[1] == 'I' && made[2] == 'M' && made[3] == 'G');
        Assert::AreEqual (TwoImgHeader::kFormatProDosOrder,         read32 (kFormatAt));
        Assert::AreEqual (kBlocks,                                  read32 (kBlocksAt));
        Assert::AreEqual ((uint32_t) kHeaderSize,                   read32 (kDataAt));
        Assert::AreEqual ((uint32_t) NibbleFormat::kImageByteSize,  read32 (kLengthAt));

        VolumeImage::DosLogicalToProDosFile (dsk, blocks);
        Assert::IsTrue (std::equal (blocks.begin(), blocks.end(), made.begin() + (ptrdiff_t) kHeaderSize));
    }
};
