#include "Pch.h"

#include "Devices/Disk/DriveImageRule.h"
#include "Devices/Disk/ImageClassifier.h"
#include "Devices/Disk/Inspector/Blocks/BlockImageFile.h"
#include "Machines/Apple2/Common/WozLoader.h"
#include "Ui/Scene/DeskDrivePlan.h"
#include "ImageHeadBuilder.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DriveImageRuleTests
//
//  Every image kind against every drive kind, from the images each drive's
//  mount takes: Disk2 loads sector, nibble and WOZ files; Sony35Drive, which
//  the Apple 3.5 Drive, the //c Plus drive and the UniDisk 3.5 share, loads
//  a 3.5-inch WOZ or 800 or 1,600 blocks; a hard disk loads blocks up to its
//  capacity, except a floppy's size.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DriveImageRuleTests)
{
public:

    using B = ImageHeadBuilder;

    static constexpr uint32_t  kScsiBlocks    = 65535;
    static constexpr uint32_t  kProFileBlocks = 9728;

    struct Sample
    {
        const wchar_t  * label;
        ImageClass       image;
        bool             floppy525;
        bool             floppy35;
        bool             scsi;
        bool             proFile;
        bool             recorder;
    };



    static std::vector<Sample> MakeSamples()
    {
        std::vector<Sample>  samples =
        {
            { L"140K .dsk",          B::ClassifyRaw (L".dsk", B::k140K),                                     true,  false, false, false, false },
            { L"140K .po",           B::ClassifyRaw (L".po",  B::k140K),                                     true,  false, false, false, false },
            { L".nib",               B::ClassifyRaw (L".nib", ImageClassifier::kNibBytes),                   true,  false, false, false, false },
            { L"5.25-inch WOZ",      B::ClassifyWoz (L".woz", WozLoader::kDiskType525, 1),                   true,  false, false, false, false },
            { L"140K .hdv",          B::ClassifyRaw (L".hdv", B::k140K),                                     false, false, false, false, false },
            { L"140K DOS 2IMG",      B::ClassifyTwoImg (BlockImageFile::kTwoImgDos, B::k140K),               false, false, false, false, false },
            { L"400K .po",           B::ClassifyRaw (L".po",  B::k400K),                                     false, true,  false, false, false },
            { L"800K .po",           B::ClassifyRaw (L".po",  B::k800K),                                     false, true,  false, false, false },
            { L"800K 2IMG",          B::ClassifyTwoImg (BlockImageFile::kTwoImgProDos, B::k800K),            false, true,  false, false, false },
            { L"800K Disk Copy",     B::ClassifyDiskCopy (L".dc42", B::k800K),                               false, true,  false, false, false },
            { L"3.5-inch WOZ",       B::ClassifyWoz (L".woz", WozLoader::kDiskType35, 2),                    false, true,  false, false, false },
            { L"100-block .po",      B::ClassifyRaw (L".po",  100 * B::kBlock),                              false, false, true,  true,  false },
            { L"1.44 MB .img",       B::ClassifyRaw (L".img", 2880 * B::kBlock),                             false, false, true,  true,  false },
            { L"5 MB ProFile size",  B::ClassifyRaw (L".hdv", kProFileBlocks * B::kBlock),                   false, false, true,  true,  false },
            { L"One block over",     B::ClassifyRaw (L".hdv", (kProFileBlocks + 1) * B::kBlock),             false, false, true,  false, false },
            { L"32 MB .hdv",         B::ClassifyRaw (L".hdv", B::k32M),                                      false, false, true,  false, false },
            { L"32 MB 2IMG",         B::ClassifyTwoImg (BlockImageFile::kTwoImgProDos, B::k32M),             false, false, true,  false, false },
            { L"Tape",               B::ClassifyRaw (L".wav", 12345),                                        false, false, false, false, true  },
            { L"Truncated .dsk",     B::ClassifyRaw (L".dsk", 143000),                                       false, false, false, false, false },
            { L"Text file",          B::ClassifyRaw (L".txt", B::k140K),                                     false, false, false, false, false },
        };

        return samples;
    }



    TEST_METHOD (EveryImageAgainstEveryDrive)
    {
        ReceivingDrive  floppy525 { ReceivingDriveKind::Floppy525, 0 };
        ReceivingDrive  apple35   { ReceivingDriveKind::Apple35,   0 };
        ReceivingDrive  uniDisk   { ReceivingDriveKind::UniDisk35, 0 };
        ReceivingDrive  scsi      { ReceivingDriveKind::HardDisk,  kScsiBlocks };
        ReceivingDrive  proFile   { ReceivingDriveKind::HardDisk,  kProFileBlocks };
        ReceivingDrive  recorder  { ReceivingDriveKind::Recorder,  0 };
        size_t          checked   = 0;



        for (const Sample & sample : MakeSamples())
        {
            Assert::AreEqual (sample.floppy525, DriveImageRule::Accepts (floppy525, sample.image), sample.label);
            Assert::AreEqual (sample.floppy35,  DriveImageRule::Accepts (apple35,   sample.image), sample.label);
            Assert::AreEqual (sample.floppy35,  DriveImageRule::Accepts (uniDisk,   sample.image), sample.label);
            Assert::AreEqual (sample.scsi,      DriveImageRule::Accepts (scsi,      sample.image), sample.label);
            Assert::AreEqual (sample.proFile,   DriveImageRule::Accepts (proFile,   sample.image), sample.label);
            Assert::AreEqual (sample.recorder,  DriveImageRule::Accepts (recorder,  sample.image), sample.label);
            checked++;
        }

        Assert::IsTrue (checked > 0, L"samples were checked");
    }



    TEST_METHOD (FloppySizesNeverGoToAHardDisk)
    {
        ReceivingDrive  scsi { ReceivingDriveKind::HardDisk, kScsiBlocks };



        for (uint64_t blocks : { 280ull, 800ull, 1600ull })
        {
            Assert::IsFalse (DriveImageRule::Accepts (scsi, B::ClassifyRaw (L".hdv", blocks * B::kBlock)), std::format (L"{} blocks", blocks).c_str());
        }
    }



    TEST_METHOD (EveryDeskModelHasAKind)
    {
        struct Row
        {
            DeskDriveModel      model;
            ReceivingDriveKind  kind;
        };

        static constexpr Row  kRows[] =
        {
            { DeskDriveModel::DiskII,       ReceivingDriveKind::Floppy525 },
            { DeskDriveModel::Disk2c,       ReceivingDriveKind::Floppy525 },
            { DeskDriveModel::UniDisk35,    ReceivingDriveKind::UniDisk35 },
            { DeskDriveModel::Apple35Drive, ReceivingDriveKind::Apple35   },
            { DeskDriveModel::Apple35Plus,  ReceivingDriveKind::Apple35   },
            { DeskDriveModel::HardDisk20SC, ReceivingDriveKind::HardDisk  },
            { DeskDriveModel::ProFile,      ReceivingDriveKind::HardDisk  },
        };

        static_assert (std::size (kRows) == (size_t) DeskDriveModel::Count, "a desk drive model has no row here");



        for (size_t i = 0; i < std::size (kRows); i++)
        {
            Assert::AreEqual ((int) i, (int) kRows[i].model, L"rows are in enum order");
            Assert::AreEqual ((int) kRows[i].kind, (int) DeskDrivePlan::GetReceivingKind (kRows[i].model));
        }
    }



    TEST_METHOD (AcceptedTextForEachKind)
    {
        Assert::AreEqual (std::wstring (L"5.25-inch disk images"),        DriveImageRule::GetAcceptedText ({ ReceivingDriveKind::Floppy525, 0 }));
        Assert::AreEqual (std::wstring (L"400K and 800K disk images"),    DriveImageRule::GetAcceptedText ({ ReceivingDriveKind::Apple35,   0 }));
        Assert::AreEqual (std::wstring (L"400K and 800K disk images"),    DriveImageRule::GetAcceptedText ({ ReceivingDriveKind::UniDisk35, 0 }));
        Assert::AreEqual (std::wstring (L"Hard disk images up to 32 MB"), DriveImageRule::GetAcceptedText ({ ReceivingDriveKind::HardDisk,  kScsiBlocks }));
        Assert::AreEqual (std::wstring (L"Hard disk images up to 5 MB"),  DriveImageRule::GetAcceptedText ({ ReceivingDriveKind::HardDisk, kProFileBlocks }));
        Assert::AreEqual (std::wstring (L"Tape recordings"),              DriveImageRule::GetAcceptedText ({ ReceivingDriveKind::Recorder,  0 }));
    }



    //  The picker's capsules: one term each.
    TEST_METHOD (AcceptedTermsForEachKind)
    {
        using Terms = std::vector<std::wstring>;



        Assert::IsTrue (DriveImageRule::GetAcceptedTerms ({ ReceivingDriveKind::Floppy525, 0 })           == Terms { L"5.25\"", L"140K" });
        Assert::IsTrue (DriveImageRule::GetAcceptedTerms ({ ReceivingDriveKind::Apple35,   0 })           == Terms { L"3.5\"", L"400K", L"800K" });
        Assert::IsTrue (DriveImageRule::GetAcceptedTerms ({ ReceivingDriveKind::UniDisk35, 0 })           == Terms { L"3.5\"", L"400K", L"800K" });
        Assert::IsTrue (DriveImageRule::GetAcceptedTerms ({ ReceivingDriveKind::HardDisk,  kScsiBlocks }) == Terms { L"Hard disk", L"Up to 32 MB" });
        Assert::IsTrue (DriveImageRule::GetAcceptedTerms ({ ReceivingDriveKind::Recorder,  0 })           == Terms { L"Tape recordings" });
    }
};
