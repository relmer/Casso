#include "Pch.h"

#include "Devices/Disk/DiskImageStore.h"
#include "Devices/Disk/ImageClassifier.h"
#include "Devices/Disk/Inspector/Blocks/BlockImageFile.h"
#include "Devices/Tape/TapeImageLoader.h"
#include "Machines/Apple2/Common/WozLoader.h"
#include "ImageHeadBuilder.h"
#include "../EmuTests/FakeDiskFileIo.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ImageClassifierTests
//
//  Each container is built by ImageHeadBuilder from its published layout,
//  not by the code under test.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ImageClassifierTests)
{
public:

    using B = ImageHeadBuilder;



    static void AssertClass (const ImageClass & actual, ImageFormFactor formFactor, ImageContainer container, uint64_t bytes, const wchar_t * label)
    {
        Assert::IsTrue   (actual.IsClassified(),                     label);
        Assert::AreEqual ((int) formFactor, (int) actual.formFactor, label);
        Assert::AreEqual ((int) container,  (int) actual.container,  label);
        Assert::AreEqual (bytes,            actual.formattedBytes,   label);
    }



    static void AssertUnclassified (const ImageClass & actual, const wchar_t * label)
    {
        Assert::AreEqual ((int) ImageStatus::Unclassified, (int) actual.status,     label);
        Assert::AreEqual ((int) ImageFormFactor::None,     (int) actual.formFactor, label);
        Assert::AreEqual ((uint64_t) 0,                    actual.formattedBytes,   label);
    }



    TEST_METHOD (Woz525IsA140KFloppy)
    {
        ImageClass  result = B::ClassifyWoz (L".woz", WozLoader::kDiskType525, 1);



        AssertClass (result, ImageFormFactor::Floppy525, ImageContainer::Woz, B::k140K, L"5.25-inch WOZ");
        Assert::AreEqual (std::wstring (L"140K"),      ImageClassifier::FormatSize (result));
        Assert::AreEqual (std::wstring (L"5.25-inch"), ImageClassifier::FormatFormFactor (result));
    }



    TEST_METHOD (Woz35SidesSetTheSize)
    {
        ImageClass  oneSide  = B::ClassifyWoz (L".woz", WozLoader::kDiskType35, 1);
        ImageClass  twoSides = B::ClassifyWoz (L".woz", WozLoader::kDiskType35, 2);



        AssertClass (oneSide,  ImageFormFactor::Floppy35, ImageContainer::Woz, B::k400K, L"one side");
        AssertClass (twoSides, ImageFormFactor::Floppy35, ImageContainer::Woz, B::k800K, L"two sides");
        Assert::AreEqual ((uint32_t) 800,  oneSide.blockCount);
        Assert::AreEqual ((uint32_t) 1600, twoSides.blockCount);
        Assert::AreEqual (std::wstring (L"3.5-inch"), ImageClassifier::FormatFormFactor (twoSides));
    }



    TEST_METHOD (WozSignatureWinsOverTheName)
    {
        AssertClass (B::ClassifyWoz (L".po", WozLoader::kDiskType35, 2), ImageFormFactor::Floppy35, ImageContainer::Woz, B::k800K, L"WOZ under .po");
    }



    TEST_METHOD (DamagedWozIsUnclassified)
    {
        std::vector<Byte>  noSignature (ImageClassifier::kHeadBytes, 0);
        std::vector<Byte>  badTail     = B::MakeWozHead ("WOZ2", WozLoader::kDiskType525, 1);



        badTail[5] = 0;

        AssertUnclassified (B::ClassifyWoz (L".woz", 3, 1),                                   L"disk type 3");
        AssertUnclassified (ImageClassifier::ClassifyHeader (L".woz", 234496, noSignature),   L"no signature");
        AssertUnclassified (ImageClassifier::ClassifyHeader (L".WOZ", 234496, badTail),       L"damaged signature");
    }



    TEST_METHOD (TwoImgFollowsItsHeader)
    {
        AssertClass (B::ClassifyTwoImg (BlockImageFile::kTwoImgProDos, B::k800K),                    ImageFormFactor::Floppy35,  ImageContainer::TwoImg, B::k800K, L"ProDOS order 800K");
        AssertClass (B::ClassifyTwoImg (BlockImageFile::kTwoImgProDos, B::k32M),                     ImageFormFactor::HardDisk,  ImageContainer::TwoImg, B::k32M,  L"ProDOS order 32 MB");
        AssertClass (B::ClassifyTwoImg (BlockImageFile::kTwoImgDos,    B::k140K),                    ImageFormFactor::Floppy525, ImageContainer::TwoImg, B::k140K, L"DOS order 140K");
        AssertClass (B::ClassifyTwoImg (BlockImageFile::kTwoImgNibble, ImageClassifier::kNibBytes),  ImageFormFactor::Floppy525, ImageContainer::TwoImg, B::k140K, L"nibble");
    }



    TEST_METHOD (InconsistentTwoImgIsUnclassified)
    {
        uint64_t  headerBytes = BlockImageFile::kTwoImgHeader;



        AssertUnclassified (ImageClassifier::ClassifyHeader (L".2mg", headerBytes + 1000, B::MakeTwoImgHead (BlockImageFile::kTwoImgProDos, (uint32_t) B::k800K)), L"data past the end");
        AssertUnclassified (B::ClassifyTwoImg (BlockImageFile::kTwoImgDos,    B::k800K), L"DOS order 800K");
        AssertUnclassified (B::ClassifyTwoImg (BlockImageFile::kTwoImgProDos, 0),        L"no data");
    }



    TEST_METHOD (DiskCopyUsesItsDataSize)
    {
        uint64_t  k1440K = 2880 * B::kBlock;



        AssertClass (B::ClassifyDiskCopy (L".dc42",  B::k400K), ImageFormFactor::Floppy35, ImageContainer::DiskCopy42, B::k400K, L"400K");
        AssertClass (B::ClassifyDiskCopy (L".image", B::k800K), ImageFormFactor::Floppy35, ImageContainer::DiskCopy42, B::k800K, L"800K");
        AssertClass (B::ClassifyDiskCopy (L".dc",    k1440K),   ImageFormFactor::HardDisk, ImageContainer::DiskCopy42, k1440K,   L"1.44 MB is a hard disk image");
    }



    TEST_METHOD (NibbleImagesByLength)
    {
        AssertClass (B::ClassifyRaw (L".nib", ImageClassifier::kNibBytes), ImageFormFactor::Floppy525, ImageContainer::Nibble, B::k140K, L".nib");
        AssertClass (B::ClassifyRaw (L".nb2", ImageClassifier::kNb2Bytes), ImageFormFactor::Floppy525, ImageContainer::Nibble, B::k140K, L".nb2");
        AssertClass (B::ClassifyRaw (L".nb2", ImageClassifier::kNibBytes), ImageFormFactor::Floppy525, ImageContainer::Nibble, B::k140K, L"either size under either name");
        AssertUnclassified (B::ClassifyRaw (L".nib", ImageClassifier::kNibBytes - 1), L"odd length");
    }



    TEST_METHOD (SectorImagesAre140K)
    {
        for (const wchar_t * extension : { L".dsk", L".do", L".po", L".DSK" })
        {
            ImageClass  result = B::ClassifyRaw (extension, B::k140K);



            AssertClass (result, ImageFormFactor::Floppy525, ImageContainer::Sector, B::k140K, extension);
            Assert::AreEqual ((uint32_t) 280, result.blockCount, extension);
        }
    }



    TEST_METHOD (RawBlocksByCount)
    {
        AssertClass (B::ClassifyRaw (L".po",  B::k400K),         ImageFormFactor::Floppy35,  ImageContainer::Raw, B::k400K,         L"400K .po");
        AssertClass (B::ClassifyRaw (L".po",  B::k800K),         ImageFormFactor::Floppy35,  ImageContainer::Raw, B::k800K,         L"800K .po");
        AssertClass (B::ClassifyRaw (L".dsk", B::k800K),         ImageFormFactor::Floppy35,  ImageContainer::Raw, B::k800K,         L"800K .dsk");
        AssertClass (B::ClassifyRaw (L".hdv", B::k32M),          ImageFormFactor::HardDisk,  ImageContainer::Raw, B::k32M,          L"32 MB .hdv");
        AssertClass (B::ClassifyRaw (L".po",  100 * B::kBlock),  ImageFormFactor::HardDisk,  ImageContainer::Raw, 100 * B::kBlock,  L"100 blocks");
        AssertClass (B::ClassifyRaw (L".img", 2880 * B::kBlock), ImageFormFactor::HardDisk,  ImageContainer::Raw, 2880 * B::kBlock, L"2,880 blocks");
        AssertClass (B::ClassifyRaw (L".hdv", B::k140K),         ImageFormFactor::Floppy525, ImageContainer::Raw, B::k140K,         L"140K .hdv is raw, not sectors");
    }



    TEST_METHOD (TruncatedAndEmptyImagesAreUnclassified)
    {
        AssertUnclassified (B::ClassifyRaw (L".dsk", 143000), L"truncated .dsk");
        AssertUnclassified (B::ClassifyRaw (L".po",  0),      L"empty .po");
        AssertUnclassified (B::ClassifyRaw (L".hdv", 1000),   L"not whole blocks");
    }



    TEST_METHOD (OtherFilesAreNotImages)
    {
        for (const wchar_t * extension : { L".txt", L".d13", L".gz", L".zip", L".shk", L"" })
        {
            ImageClass  result = B::ClassifyRaw (extension, B::k140K);



            Assert::AreEqual ((int) ImageStatus::NotAnImage, (int) result.status, extension);
            Assert::AreEqual (std::wstring(), ImageClassifier::FormatSize (result),       extension);
            Assert::AreEqual (std::wstring(), ImageClassifier::FormatFormFactor (result), extension);
        }
    }



    TEST_METHOD (TapesHaveNoSize)
    {
        for (const wchar_t * extension : { L".wav", L".aif", L".aiff", L".aifc", L".mp3", L".flac", L".ogg" })
        {
            ImageClass  result = B::ClassifyRaw (extension, 12345);



            AssertClass (result, ImageFormFactor::Tape, ImageContainer::Tape, 0, extension);
            Assert::AreEqual (std::wstring(),         ImageClassifier::FormatSize (result),       extension);
            Assert::AreEqual (std::wstring (L"Tape"), ImageClassifier::FormatFormFactor (result), extension);
        }
    }



    TEST_METHOD (SizesAreLabeledByMagnitude)
    {
        Assert::AreEqual (std::wstring (L"140K"),   ImageClassifier::FormatSize (B::ClassifyRaw (L".dsk", B::k140K)));
        Assert::AreEqual (std::wstring (L"400K"),   ImageClassifier::FormatSize (B::ClassifyRaw (L".po",  B::k400K)));
        Assert::AreEqual (std::wstring (L"800K"),   ImageClassifier::FormatSize (B::ClassifyRaw (L".po",  B::k800K)));
        Assert::AreEqual (std::wstring (L"50K"),    ImageClassifier::FormatSize (B::ClassifyRaw (L".po",  100 * B::kBlock)));
        Assert::AreEqual (std::wstring (L"1.4 MB"), ImageClassifier::FormatSize (B::ClassifyRaw (L".img", 2880 * B::kBlock)));
        Assert::AreEqual (std::wstring (L"5.0 MB"), ImageClassifier::FormatSize (B::ClassifyRaw (L".hdv", 5 * 1024 * 1024)));
        Assert::AreEqual (std::wstring (L"32 MB"),  ImageClassifier::FormatSize (B::ClassifyRaw (L".hdv", B::k32M)));
    }



    TEST_METHOD (ClassifyReadsOnlyTheHead)
    {
        constexpr uint64_t  kFileBytes = 4096 * B::kBlock;

        FakeDiskFileIo  io;
        std::string     path = "C:\\Disks\\big.hdv";
        ImageClass      result;



        io.files[path]  = std::vector<Byte> ((size_t) kFileBytes, 0);
        io.stamps[path] = FileStamp { kFileBytes, 0 };

        result = ImageClassifier::Classify (io, path);

        AssertClass (result, ImageFormFactor::HardDisk, ImageContainer::Raw, kFileBytes, L"2 MB .hdv");
        Assert::AreEqual ((uint64_t) ImageClassifier::kHeadBytes, io.bytesReadAt, L"one head read");
        Assert::AreEqual (0, io.wholeReads, L"no whole read");
    }



    TEST_METHOD (UnreadableFilesAreResultsNotFailures)
    {
        FakeDiskFileIo  io;



        AssertUnclassified (ImageClassifier::Classify (io, "C:\\Disks\\missing.dsk"), L"missing image");
        Assert::AreEqual ((int) ImageStatus::NotAnImage, (int) ImageClassifier::Classify (io, "C:\\Disks\\missing.txt").status, L"missing text file");
    }



    TEST_METHOD (ImageExtensionsMatchTheDrives)
    {
        static constexpr const wchar_t * kCandidates[] =
        {
            L".dsk", L".do", L".po", L".woz", L".nib", L".nb2", L".hdv", L".2mg", L".2img", L".img", L".dc", L".dc42",
            L".image", L".iso", L".hd", L".wav", L".aif", L".aiff", L".aifc", L".mp3", L".flac", L".ogg", L".d13",
            L".txt", L".zip", L".gz", L".sdk", L".shk", L".bin", L".DSK", L".Po", L".WAV",
        };

        size_t  checked = 0;



        for (const wchar_t * extension : kCandidates)
        {
            std::wstring  name     = std::wstring (L"x") + extension;
            std::string   narrow   = std::string ("x") + std::filesystem::path (extension).string();
            bool          expected = DiskImageStore::IsMountableImageExtension (name)
                                  || BlockImageFile::IsBlockExtension (narrow)
                                  || TapeImageLoader::IsTapeFileExtension (name);



            Assert::AreEqual (expected, ImageClassifier::IsImageExtension (extension), extension);
            checked++;
        }

        Assert::AreEqual (std::size (kCandidates), checked);
    }
};
