#include "Pch.h"

#include "../EhmTestHelper.h"
#include "AppleLink/LinkedDisk.h"
#include "Machines/Apple2/Common/Dos33Skeleton.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/ProDosVolume.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  LinkedDiskTests
//
//  A LinkedDisk over a fake drive with a whole volume built in memory; the
//  drive counts the requests made of it. A catalog and a file must come
//  back exactly as the whole volume returns them, in a fixed number of rounds,
//  and a write must send only the sectors it changed.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (LinkedDiskTests)
{
public:

    struct FakeDrive : ILinkedSectorSource
    {
        std::vector<Byte>  image;
        int                reads    = 0;
        int                writes   = 0;
        size_t             fetched  = 0;

        HRESULT  ReadSectors (std::span<const size_t> sectors, std::vector<Byte> & outData) override
        {
            reads++;
            fetched += sectors.size();
            outData.clear();

            for (size_t sector : sectors)
            {
                outData.insert (outData.end(), image.begin() + (ptrdiff_t) (sector * 256), image.begin() + (ptrdiff_t) ((sector + 1) * 256));
            }

            return S_OK;
        }

        HRESULT  WriteSectors (std::span<const size_t> sectors, std::span<const Byte> data) override
        {
            writes++;

            for (size_t i = 0; i < sectors.size(); i++)
            {
                std::copy_n (data.begin() + (ptrdiff_t) (i * 256), 256, image.begin() + (ptrdiff_t) (sectors[i] * 256));
            }

            return S_OK;
        }
    };

    static FilePayload  Binary (Byte type, size_t length, Byte seed)
    {
        FilePayload  payload;

        payload.type           = type;
        payload.loadAddress    = 0x2000;
        payload.hasLoadAddress = true;
        payload.bytes.resize (length);

        for (size_t i = 0; i < length; i++)
        {
            payload.bytes[i] = (Byte) (seed + i * 7 + (i >> 8));
        }

        return payload;
    }

    //  A DOS 3.3 volume with three files, the second several sectors long.
    static std::vector<Byte>  BuildDos33()
    {
        std::vector<Byte>  buffer (NibblizationLayer::kImageByteSize, 0);
        std::vector<Byte>  next;

        AssertSucceeded (Dos33Skeleton::Write (buffer, 254));

        for (const auto & [name, length, seed] : { std::tuple<const char *, size_t, Byte> { "FIRST", 100, 1 }, { "SECOND", 3000, 2 }, { "THIRD", 600, 3 } })
        {
            Dos33Volume  volume (buffer);

            AssertSucceeded (volume.Write (FilePath::Parse (name), Binary (Dos33Volume::kTypeBinary, length, seed), next));
            buffer = next;
        }

        return buffer;
    }

    static std::vector<Byte>  BuildProDos()
    {
        std::vector<Byte>  buffer (NibblizationLayer::kImageByteSize, 0);
        std::vector<Byte>  next;

        AssertSucceeded (ProDosSkeleton::Write (buffer, "LINKED"));

        for (const auto & [name, length, seed] : { std::tuple<const char *, size_t, Byte> { "FIRST", 100, 1 }, { "SECOND", 3000, 2 }, { "THIRD", 600, 3 } })
        {
            ProDosVolume  volume (buffer);

            AssertSucceeded (volume.Write (FilePath::Parse (name), Binary (ProDosVolume::kTypeBinary, length, seed), next));
            buffer = next;
        }

        return buffer;
    }

    static std::vector<std::string>  Names (const VolumeListing & listing)
    {
        std::vector<std::string>  names;

        for (const auto & entry : listing.entries)
        {
            names.push_back (entry.name);
        }

        return names;
    }

    //  catalogRounds: ProDOS lists from its directory blocks; DOS 3.3 also reads
    //  each file's track/sector list and first data sector for its length.
    static void  CheckCatalogAndFile (const std::vector<Byte> & image, VolumeKind kind, const IVolume & whole, int catalogRounds)
    {
        FakeDrive      drive;
        LinkedDisk     disk (drive);
        VolumeListing  expected;
        VolumeListing  listing;
        FilePayload    expectedFile;
        FilePayload    file;

        drive.image = image;

        AssertSucceeded (whole.Enumerate (expected));
        AssertSucceeded (whole.Read (FilePath::Parse ("SECOND"), expectedFile));

        AssertSucceeded (disk.Prefetch());
        Assert::IsTrue (disk.GetKind() == kind);
        Assert::AreEqual (1, drive.reads, L"the catalog's sectors in one request");

        AssertSucceeded (disk.Run ([&] (const IVolume & volume) { return volume.Enumerate (listing); }));
        Assert::IsTrue (Names (listing) == Names (expected));
        Assert::AreEqual (listing.freeUnits, expected.freeUnits);
        Assert::IsTrue (disk.GetRounds() <= catalogRounds, L"the catalog in its rounds");

        drive.reads = 0;
        AssertSucceeded (disk.Run ([&] (const IVolume & volume) { return volume.Read (FilePath::Parse ("SECOND"), file); }));
        Assert::IsTrue (file.bytes == expectedFile.bytes);
        Assert::IsTrue (disk.GetRounds() <= 3, L"the file's list, then its data");
        Assert::AreEqual (disk.GetRounds() - 1, drive.reads, L"one request per round that missed");
        Assert::IsTrue (drive.fetched < LinkedDisk::kSectorCount / 4, L"far less than the disk");
    }

    static void  CheckWrite (const std::vector<Byte> & image)
    {
        FakeDrive            drive;
        LinkedDisk           disk (drive);
        std::vector<Byte>    expected;
        std::vector<size_t>  sent;
        std::vector<size_t>  changed;
        FilePayload          payload = Binary (6, 700, 9);
        VolumeListing        listing;

        drive.image = image;

        if (VolumeImage::DetectFilesystem (image) == VolumeKind::Dos33)
        {
            payload.type = Dos33Volume::kTypeBinary;
            AssertSucceeded (Dos33Volume (image).Write (FilePath::Parse ("FOURTH"), payload, expected));
        }
        else
        {
            AssertSucceeded (ProDosVolume (image).Write (FilePath::Parse ("FOURTH"), payload, expected));
        }

        for (size_t sector = 0; sector < LinkedDisk::kSectorCount; sector++)
        {
            if (!std::equal (expected.begin() + (ptrdiff_t) (sector * 256), expected.begin() + (ptrdiff_t) ((sector + 1) * 256), image.begin() + (ptrdiff_t) (sector * 256)))
            {
                changed.push_back (sector);
            }
        }

        AssertSucceeded (disk.Run ([&] (const IVolume & volume) { return volume.Enumerate (listing); }));
        AssertSucceeded (disk.Write ([&] (const IVolume & volume, std::vector<Byte> & outBuffer) { return volume.Write (FilePath::Parse ("FOURTH"), payload, outBuffer); }, sent));

        Assert::IsTrue (sent == changed, L"exactly the sectors the write changed");
        Assert::AreEqual (1, drive.writes);
        Assert::IsTrue (drive.image == expected, L"the drive has the written volume");
    }




    TEST_METHOD (Dos33_CatalogAndFile_MatchTheWholeVolume)
    {
        std::vector<Byte>  image = BuildDos33();

        CheckCatalogAndFile (image, VolumeKind::Dos33, Dos33Volume (image), 3);
    }




    TEST_METHOD (ProDos_CatalogAndFile_MatchTheWholeVolume)
    {
        std::vector<Byte>  image = BuildProDos();

        CheckCatalogAndFile (image, VolumeKind::ProDos, ProDosVolume (image), 2);
    }




    TEST_METHOD (Dos33_WriteSendsOnlyTheSectorsItChanged)
    {
        CheckWrite (BuildDos33());
    }




    TEST_METHOD (ProDos_WriteSendsOnlyTheSectorsItChanged)
    {
        CheckWrite (BuildProDos());
    }




    TEST_METHOD (Clear_MakesEverySectorAbsent)
    {
        FakeDrive      drive;
        LinkedDisk     disk (drive);
        VolumeListing  listing;

        drive.image = BuildDos33();

        AssertSucceeded (disk.Run ([&] (const IVolume & volume) { return volume.Enumerate (listing); }));
        Assert::IsTrue (disk.IsPresent (17 * 16));

        disk.Clear();

        Assert::IsFalse (disk.IsPresent (17 * 16));
        Assert::IsTrue (disk.GetKind() == VolumeKind::Unknown);
    }




    TEST_METHOD (AnUnformattedDisk_IsUnrecognized)
    {
        FakeDrive      drive;
        LinkedDisk     disk (drive);
        VolumeListing  listing;

        drive.image.assign (NibblizationLayer::kImageByteSize, 0);

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_UNRECOGNIZED_VOLUME), disk.Run ([&] (const IVolume & volume) { return volume.Enumerate (listing); }));
    }
};
