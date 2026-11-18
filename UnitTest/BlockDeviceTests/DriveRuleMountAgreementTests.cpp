#include "Pch.h"

#include "Core/TextEncoding.h"
#include "Devices/BlockDevice/BlockDeviceMedia.h"
#include "Devices/Disk/Containers/DiskCopy42Container.h"
#include "Devices/Disk/Containers/TwoImgContainer.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Devices/Disk/DriveImageRule.h"
#include "Devices/Disk/ImageClassifier.h"
#include "Devices/Floppy35/Gcr35Track.h"
#include "Devices/Floppy35/Sony35Drive.h"
#include "Machines/Apple2/Common/WozLoader.h"
#include "../EmuTests/FakeDiskFileIo.h"
#include "../EmuTests/Woz35Builder.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DriveRuleMountAgreementTests
//
//  The rule against the mounts. Each image is mounted on each kind of drive
//  through the call that drive's mount makes: DiskImageStore for a Disk II,
//  Sony35Drive::Insert for the 3.5-inch drives, and BlockDeviceMedia::Attach
//  then the floppy-size check of StorageCardWiring for a hard disk. Whether
//  it mounts is what DriveImageRule::Accepts returns for the image's class.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DriveRuleMountAgreementTests)
{
public:

    static constexpr uint64_t  kBlock         = 512;
    static constexpr uint32_t  kScsiBlocks    = 65535;
    static constexpr uint32_t  kProFileBlocks = 9728;

    struct Fixture
    {
        std::string        path;
        std::vector<Byte>  bytes;
    };



    static std::vector<Byte> MakeTwoImg (uint32_t format, uint64_t dataBytes)
    {
        TwoImgHeader       header;
        std::vector<Byte>  payload ((size_t) dataBytes, 0);
        std::vector<Byte>  bytes;



        header.format     = format;
        header.blockCount = (format == TwoImgHeader::kFormatProDosOrder) ? (uint32_t) (dataBytes / kBlock) : 0;

        TwoImgContainer::Build (header, payload, bytes);

        return bytes;
    }



    static std::vector<Byte> MakeDiskCopy (uint64_t dataBytes)
    {
        DiskCopy42Header   header;
        std::vector<Byte>  data ((size_t) dataBytes, 0);
        std::vector<Byte>  bytes;



        header.name     = "Test";
        header.dataSize = (uint32_t) dataBytes;

        DiskCopy42Container::Build (header, data, bytes);

        return bytes;
    }



    static std::vector<Byte> MakeWoz525()
    {
        constexpr size_t  kTrackBytes = 6400;
        constexpr size_t  kTrackBits  = 51200;

        std::vector<Byte>  bits (kTrackBytes, 0xFF);
        std::vector<Byte>  woz;



        Assert::AreEqual (S_OK, WozLoader::BuildSyntheticV2 (WozLoader::kDiskType525, false, bits, kTrackBits, woz));

        return woz;
    }



    static std::vector<Fixture> MakeFixtures()
    {
        std::vector<Fixture>  fixtures =
        {
            { "a.dsk",       std::vector<Byte> ((size_t) ImageClassifier::k525Bytes, 0) },
            { "b.po",        std::vector<Byte> ((size_t) ImageClassifier::k525Bytes, 0) },
            { "c.nib",       std::vector<Byte> ((size_t) ImageClassifier::kNibBytes, 0xFF) },
            { "d.woz",       MakeWoz525() },
            { "e.hdv",       std::vector<Byte> ((size_t) ImageClassifier::k525Bytes, 0) },
            { "f.2mg",       MakeTwoImg (TwoImgHeader::kFormatDosOrder, ImageClassifier::k525Bytes) },
            { "g.po",        std::vector<Byte> ((size_t) (800 * kBlock), 0) },
            { "h.po",        std::vector<Byte> ((size_t) (1600 * kBlock), 0) },
            { "i.dsk",       std::vector<Byte> ((size_t) (1600 * kBlock), 0) },
            { "j.2mg",       MakeTwoImg (TwoImgHeader::kFormatProDosOrder, 1600 * kBlock) },
            { "k.dc42",      MakeDiskCopy (1600 * kBlock) },
            { "l.woz",       Woz35Builder::Build (std::vector<Byte> ((size_t) (Gcr35Track::kBlocksTwoSides * kBlock), 0)) },
            { "m.po",        std::vector<Byte> ((size_t) (100 * kBlock), 0) },
            { "n.img",       std::vector<Byte> ((size_t) (2880 * kBlock), 0) },
            { "o.hdv",       std::vector<Byte> ((size_t) (kProFileBlocks * kBlock), 0) },
            { "p.hdv",       std::vector<Byte> ((size_t) ((kProFileBlocks + 1) * kBlock), 0) },
            { "q.2mg",       MakeTwoImg (TwoImgHeader::kFormatProDosOrder, 20000 * kBlock) },
            { "r.dsk",       std::vector<Byte> (143000, 0) },
        };

        return fixtures;
    }



    static bool MountsOnDiskII (const Fixture & fixture)
    {
        DiskImageStore  store;
        DiskFormat      format  = DiskFormat::Dsk;
        HRESULT         hr      = DiskImageStore::GetSourceFormatByExtension (fixture.path, format);



        if (FAILED (hr))
        {
            return false;
        }

        hr = store.MountFromBytes (6, 0, fixture.path, format, fixture.bytes);

        return SUCCEEDED (hr);
    }



    static bool MountsOn35 (FakeDiskFileIo & io, const Fixture & fixture)
    {
        Sony35Drive   drive (0);
        std::wstring  error;
        HRESULT       hr      = drive.Insert (io, fixture.path, {}, error);
        HRESULT       hrEject = drive.Eject();



        IGNORE_RETURN_VALUE (hrEject, S_OK);

        return SUCCEEDED (hr);
    }



    static bool MountsOnHardDisk (FakeDiskFileIo & io, const Fixture & fixture, uint32_t maxBlocks)
    {
        BlockDeviceMedia  media;
        std::wstring      error;
        HRESULT           hr       = media.Attach (io, fixture.path, maxBlocks, WriteProtectInfo {}, error);
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
            ioMismatches += std::format (L"{} (mounts {}, rule {}); ", label, mounts, accepts);
        }
    }



    TEST_METHOD (RuleMatchesEveryMount)
    {
        FakeDiskFileIo  io;
        std::wstring    mismatches;
        size_t          checked = 0;



        for (const Fixture & fixture : MakeFixtures())
        {
            ImageClass    image;
            std::wstring  label  = TextEncoding::NarrowToWide (fixture.path);



            io.files[fixture.path]  = fixture.bytes;
            io.stamps[fixture.path] = FileStamp { fixture.bytes.size(), 1 };

            image = ImageClassifier::Classify (io, fixture.path);

            Compare (MountsOnDiskII (fixture),                       { ReceivingDriveKind::Floppy525, 0 },              image, label + L" on a Disk II",         mismatches);
            Compare (MountsOn35 (io, fixture),                       { ReceivingDriveKind::Apple35, 0 },                image, label + L" on a 3.5-inch drive",  mismatches);
            Compare (MountsOnHardDisk (io, fixture, kScsiBlocks),    { ReceivingDriveKind::HardDisk, kScsiBlocks },     image, label + L" on an HD20SC",         mismatches);
            Compare (MountsOnHardDisk (io, fixture, kProFileBlocks), { ReceivingDriveKind::HardDisk, kProFileBlocks },  image, label + L" on a 5 MB ProFile",    mismatches);
            checked++;
        }

        Assert::AreEqual (std::wstring(), mismatches);

        Assert::IsTrue (checked > 0, L"fixtures were mounted");
    }
};
