#include "Pch.h"

#include "EhmTestHelper.h"
#include "Core/TextEncoding.h"
#include "Devices/BlockDevice/BlockDeviceMedia.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Devices/Disk/DriveImageRule.h"
#include "Devices/Disk/ImageClassifier.h"
#include "Devices/Floppy35/Sony35Drive.h"
#include "FakeDiskFileIo.h"
#include "FixtureProvider.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ImageClassifierScenarioTests
//
//  The classifier and the drive rule against real images: every disk image
//  under the CiderPress and Disks fixtures is classified, then mounted on
//  each kind of drive through the call that drive's mount makes. Whether it
//  mounts is what DriveImageRule::Accepts returns for its class (SC-008).
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ImageClassifierScenarioTests)
{
public:

    static constexpr uint32_t  kScsiBlocks    = 65535;
    static constexpr uint32_t  kProFileBlocks = 9728;



    static bool MountsOnDiskII (const std::string & path, const std::vector<Byte> & bytes)
    {
        DiskImageStore  store;
        DiskFormat      format = DiskFormat::Dsk;
        HRESULT         hr     = DiskImageStore::GetSourceFormatByExtension (path, format);



        if (FAILED (hr))
        {
            return false;
        }

        hr = store.MountFromBytes (6, 0, path, format, bytes);

        return SUCCEEDED (hr);
    }



    static bool MountsOn35 (FakeDiskFileIo & io, const std::string & path)
    {
        Sony35Drive   drive (0);
        std::wstring  error;
        HRESULT       hr      = drive.Insert (io, path, {}, error);
        HRESULT       hrEject = drive.Eject();



        IGNORE_RETURN_VALUE (hrEject, S_OK);

        return SUCCEEDED (hr);
    }



    static bool MountsOnHardDisk (FakeDiskFileIo & io, const std::string & path, uint32_t maxBlocks)
    {
        BlockDeviceMedia  media;
        std::wstring      error;
        HRESULT           hr       = media.Attach (io, path, maxBlocks, WriteProtectInfo {}, error);
        bool              mounted  = SUCCEEDED (hr) && !BlockDeviceMedia::IsFloppyBlockCount (media.GetBlockCount());
        HRESULT           hrDetach = media.Detach();



        IGNORE_RETURN_VALUE (hrDetach, S_OK);

        return mounted;
    }



    static void Compare (bool mounts, const ReceivingDrive & drive, const ImageClass & image, const std::wstring & label, std::wstring & ioMismatches)
    {
        bool  accepts = DriveImageRule::Accepts (drive, image);



        if (mounts != accepts)
        {
            ioMismatches += std::format (L"{} (mounts {}, rule {})\n", label, mounts, accepts);
        }
    }



    //  The image files under a fixtures folder, by path relative to the
    //  fixtures root.
    static std::vector<std::string> ListImages (const std::string & root, const wchar_t * folder)
    {
        std::vector<std::string>  paths;
        std::filesystem::path     base = std::filesystem::path (TextEncoding::Utf8ToWide (root)) / folder;
        std::error_code           ec;



        for (auto it = std::filesystem::recursive_directory_iterator (base, ec); !ec && it != std::filesystem::recursive_directory_iterator(); it.increment (ec))
        {
            std::wstring  extension = it->path().extension().wstring();

            if (it->is_regular_file() && ImageClassifier::IsImageExtension (extension) && !ImageClassifier::IsTapeExtension (extension))
            {
                paths.push_back (TextEncoding::WideToUtf8 (std::filesystem::relative (it->path(), std::filesystem::path (TextEncoding::Utf8ToWide (root))).generic_wstring()));
            }
        }

        return paths;
    }



    TEST_METHOD (RuleMatchesEveryMountOfTheFixtureImages)
    {
        FixtureProvider  fixtures;
        FakeDiskFileIo   io;
        std::wstring     mismatches;
        size_t           compared = 0;



        for (const wchar_t * folder : { L"CiderPress", L"Disks" })
        {
            for (const std::string & relative : ListImages (fixtures.GetRoot(), folder))
            {
                std::vector<Byte>  bytes;
                ImageClass         image;
                std::wstring       label = TextEncoding::Utf8ToWide (relative);



                AssertSucceeded (fixtures.OpenFixture (relative, bytes), label.c_str());

                io.files[relative]  = bytes;
                io.stamps[relative] = FileStamp { bytes.size(), 1 };

                image = ImageClassifier::Classify (io, relative);

                Compare (MountsOnDiskII (relative, bytes),                 { ReceivingDriveKind::Floppy525, 0 },             image, label + L" on a Disk II",        mismatches);
                Compare (MountsOn35 (io, relative),                        { ReceivingDriveKind::Apple35, 0 },               image, label + L" on a 3.5-inch drive", mismatches);
                Compare (MountsOnHardDisk (io, relative, kScsiBlocks),     { ReceivingDriveKind::HardDisk, kScsiBlocks },    image, label + L" on an HD20SC",        mismatches);
                Compare (MountsOnHardDisk (io, relative, kProFileBlocks),  { ReceivingDriveKind::HardDisk, kProFileBlocks }, image, label + L" on a 5 MB ProFile",   mismatches);

                io.files.erase (relative);
                compared++;
            }
        }

        Logger::WriteMessage (std::format (L"{} fixture images compared\n", compared).c_str());

        Assert::AreEqual (std::wstring(), mismatches);
        Assert::IsTrue (compared > 0, L"fixture images were compared");
    }
};