#include "Pch.h"

#include "../EhmTestHelper.h"
#include "CassoExplorer/Model/DiskImagePictures.h"
#include "CassoExplorer/Model/PicturePreview.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImagePicturesTests
//
//  A disk image's picture from the thumbnail store, over a folder of the
//  test's in the temporary folder.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DiskImagePicturesTests)
{
public:

    TEST_METHOD (APicture_IsScaledToThreeQuartersAsTall_AndCenteredInASquare)
    {
        RgbaImage                       picture;
        std::shared_ptr<DxuiIconImage>  image;



        //  The left half red and the right half blue.
        picture.Allocate (560, 384, 0xFF, 0x00, 0x00);

        for (int y = 0; y < 384; y++)
        {
            for (int x = 280; x < 560; x++)
            {
                picture.rgba[((size_t) y * 560 + x) * 4 + 0] = 0x00;
                picture.rgba[((size_t) y * 560 + x) * 4 + 2] = 0xFF;
            }
        }

        image = DiskImagePictures::Scale (picture, 128);

        Assert::IsNotNull (image.get());
        Assert::AreEqual (128, image->width);
        Assert::AreEqual (128, image->height);
        Assert::AreEqual (0x00000000u, image->bgraPremul[(size_t) 15 * 128 + 10], L"Clear above the picture");
        Assert::AreEqual (0xFFFF0000u, image->bgraPremul[(size_t) 16 * 128 + 10], L"The picture's first row, red");
        Assert::AreEqual (0xFF0000FFu, image->bgraPremul[(size_t) 111 * 128 + 120], L"Its last row, blue");
        Assert::AreEqual (0x00000000u, image->bgraPremul[(size_t) 112 * 128 + 120], L"Clear below it");

        Assert::IsNull (DiskImagePictures::Scale (RgbaImage(), 128).get(), L"No picture");
    }



    TEST_METHOD (AFloppysImage_IsUnderAQuarterMegabyte_OrAWoz)
    {
        Assert::IsTrue (DiskImagePictures::IsFloppySize (L".dsk", 143360));
        Assert::IsTrue (DiskImagePictures::IsFloppySize (L".nib", 232960));
        Assert::IsFalse (DiskImagePictures::IsFloppySize (L".po", 819200), L"An 800K disk");
        Assert::IsTrue (DiskImagePictures::IsFloppySize (L".WOZ", 1300000), L"A WOZ with flux tracks");
        Assert::IsFalse (DiskImagePictures::IsFloppySize (L".dsk", 0));
    }



    TEST_METHOD (ARecord_IsALineForTheUser)
    {
        ThumbnailRecord  record;



        record.verdict = "booted";
        record.machine = "Apple2c";
        Assert::AreEqual (std::wstring (L"Boots on the Apple //c."), DiskImagePictures::DescribeRecord (record, false));

        record.verdict = "did-not-boot";
        record.reason  = "The machine stopped at the monitor's prompt.";
        Assert::AreEqual (std::wstring (L"Did not boot on the Apple //c. The machine stopped at the monitor's prompt."), DiskImagePictures::DescribeRecord (record, false));

        Assert::AreEqual (std::wstring (L"The picture was set in Casso."), DiskImagePictures::DescribeRecord (record, true));
        record.verdict = "unclear";
        record.reason  = "The screen changed, and the drive was running at the time limit.";
        Assert::AreEqual (std::wstring (L"It is unclear whether it boots on the Apple //c. The screen changed, and the drive was running at the time limit."),
                          DiskImagePictures::DescribeRecord (record, false));

        Assert::IsTrue (DiskImagePictures::DescribeRecord (ThumbnailRecord(), false).empty());
    }



    //  An image with no thumbnail is queued once and has no picture; once
    //  its thumbnail is stored it has one.
    TEST_METHOD (AnImageWithNoThumbnail_IsQueuedOnce_AndHasItsPictureOnceStored)
    {
        HRESULT                         hr    = S_OK;
        std::filesystem::path           root  = std::filesystem::temp_directory_path() / std::format (L"CassoPictures{}", ::GetCurrentProcessId());
        std::filesystem::path           image = root / L"0001.dsk";
        std::vector<std::wstring>             queued;
        std::vector<Byte>                     png;
        std::shared_ptr<DxuiIconImage>        icon;
        std::shared_ptr<const DxuiIconImage>  whole;
        std::wstring                          result;
        HRESULT                               hrCom  = CoInitializeEx (nullptr, COINIT_MULTITHREADED);



        std::filesystem::remove_all (root);
        std::filesystem::create_directories (root);

        {
            std::ofstream  out (image, std::ios::binary);

            out << std::string (143360, 'Z');
        }

        {
            ThumbnailStore     store (root / L"cache");
            DiskImagePictures  pictures (store, [&queued] (const std::wstring & path) { queued.push_back (path); });

            Assert::IsNull (pictures.Load (image.wstring(), 128).get());
            Assert::IsNull (pictures.Load (image.wstring(), 256).get());
            Assert::AreEqual ((size_t) 1, queued.size(), L"Queued once");
            Assert::AreEqual (image.wstring(), queued[0]);

            hr = PicturePreview::EncodePng (std::vector<uint32_t> ((size_t) 560 * 384, 0xFF00FF00), 560, 384, png);
            AssertSucceeded (hr);

            hr = store.Store (image, png, ThumbnailRecord());
            AssertSucceeded (hr);

            icon = pictures.Load (image.wstring(), 128);

            Assert::IsNotNull (icon.get());
            Assert::AreEqual (0xFF00FF00u, icon->bgraPremul[(size_t) 64 * 128 + 64]);
            Assert::AreEqual ((size_t) 1, queued.size());

            Assert::IsNull (pictures.Load ((root / L"missing.dsk").wstring(), 128).get(), L"No such file");

            Assert::IsTrue (pictures.Find (image.wstring(), whole, result));
            Assert::AreEqual (560, whole->width);
            Assert::AreEqual (384, whole->height);
            Assert::AreEqual (0xFF00FF00u, whole->bgraPremul[0]);
        }

        std::filesystem::remove_all (root);

        if (SUCCEEDED (hrCom))
        {
            CoUninitialize();
        }
    }
};
