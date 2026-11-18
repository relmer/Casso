#include "Pch.h"
#include "../EhmTestHelper.h"
#include "Devices/Disk/FilePath.h"
#include "Machines/Apple2/Common/RdosVolume.h"
#include "Machines/Apple2/Common/VolumeImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  RdosVolumeTests
//
//  Disks built in each test in both layouts. The catalog has the operating
//  system's entry, HELLO in two sectors, DATA in one, deleted entries to the
//  end of the first catalog sector, and NOTES in the second catalog sector.
//  The files begin at track 2.
//
//  The sector tables here are written out, so a wrong table in the volume
//  fails a test.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (RdosVolumeTests)
{
public:

    static constexpr size_t   kImageBytes  = 143360;
    static constexpr size_t   kSectorBytes = 256;
    static constexpr size_t   kEntryBytes  = 32;
    static constexpr HRESULT  kReadOnly    = HRESULT_FROM_WIN32 (ERROR_WRITE_PROTECT);
    static constexpr HRESULT  kCorrupt     = HRESULT_FROM_WIN32 (ERROR_FILE_CORRUPT);
    static constexpr HRESULT  kNotFound    = HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND);

    //  The DOS logical sector of each position in a ProDOS-ordered track.
    static constexpr int  kProDosOrder[16] = { 0, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 15 };

    //  The DOS logical sector of each physical sector.
    static constexpr int  kPhysicalOrder[16] = { 0, 7, 14, 6, 13, 5, 12, 4, 11, 3, 10, 2, 9, 1, 8, 15 };



    //  The offset in the image of a sector index.
    static size_t At (bool isThirteen, uint32_t index)
    {
        uint32_t  perTrack = isThirteen ? 13u : 16u;
        uint32_t  track    = index / perTrack;
        int       logical  = isThirteen ? kPhysicalOrder[index % perTrack] : kProDosOrder[index % perTrack];

        return ((size_t) track * 16 + (size_t) logical) * kSectorBytes;
    }



    static void SetEntry (std::vector<Byte>  & image,
                          bool                 isThirteen,
                          int                  slot,
                          const char         * name,
                          char                 type,
                          Byte                 sectorCount,
                          Word                 loadAddress,
                          Word                 length,
                          Word                 firstSector)
    {
        uint32_t  perTrack = isThirteen ? 13u : 16u;
        size_t    at       = At (isThirteen, perTrack + (uint32_t) (slot / 8)) + (size_t) (slot % 8) * kEntryBytes;

        std::fill_n (image.begin() + (ptrdiff_t) at, 24, (Byte) 0xA0);

        for (size_t i = 0; name[i] != 0; i++)
        {
            image[at + i] = (Byte) (name[i] | 0x80);
        }

        image[at + 0x18] = (Byte) (type | 0x80);
        image[at + 0x19] = sectorCount;
        image[at + 0x1A] = (Byte) (loadAddress & 0xFF);
        image[at + 0x1B] = (Byte) (loadAddress >> 8);
        image[at + 0x1C] = (Byte) (length & 0xFF);
        image[at + 0x1D] = (Byte) (length >> 8);
        image[at + 0x1E] = (Byte) (firstSector & 0xFF);
        image[at + 0x1F] = (Byte) (firstSector >> 8);
    }



    static void SetDeleted (std::vector<Byte> & image, bool isThirteen, int slot)
    {
        uint32_t  perTrack = isThirteen ? 13u : 16u;
        size_t    at       = At (isThirteen, perTrack + (uint32_t) (slot / 8)) + (size_t) (slot % 8) * kEntryBytes;

        std::fill_n (image.begin() + (ptrdiff_t) at, kEntryBytes, (Byte) 0xA0);

        image[at] = 0x80;
    }



    //  The byte at an offset of HELLO.
    static Byte HelloByte (size_t offset)
    {
        return (Byte) (offset * 7 + 3);
    }



    static std::vector<Byte> MakeImage (bool isThirteen)
    {
        std::vector<Byte>  image (kImageBytes, 0);
        Word               first  = isThirteen ? (Word) 26 : (Word) 32;
        const char       * notes  = "LINE ONE\rLINE TWO\r";
        size_t             at     = 0;

        SetEntry (image, isThirteen, 0, isThirteen ? "RDOS 2.1 COPYRIGHT 1981" : "RDOS 3.3 COPYRIGHT 1986", 'B', (Byte) first, 0, (Word) (first * 256), 0);
        SetEntry (image, isThirteen, 1, "HELLO", 'A', 2, 0,      300, first);
        SetEntry (image, isThirteen, 2, "DATA",  'B', 1, 0x6000, 100, (Word) (first + 2));

        for (int slot = 3; slot < 8; slot++)
        {
            SetDeleted (image, isThirteen, slot);
        }

        SetEntry (image, isThirteen, 8, "NOTES", 'T', 1, 0, 256, (Word) (first + 5));

        for (size_t i = 0; i < 2 * kSectorBytes; i++)
        {
            image[At (isThirteen, (uint32_t) (first + i / kSectorBytes)) + i % kSectorBytes] = HelloByte (i);
        }

        at = At (isThirteen, (uint32_t) (first + 2));
        std::fill_n (image.begin() + (ptrdiff_t) at, kSectorBytes, (Byte) 0x5A);

        at = At (isThirteen, (uint32_t) (first + 5));

        for (size_t i = 0; notes[i] != 0; i++)
        {
            image[at + i] = (Byte) (notes[i] | 0x80);
        }

        return image;
    }



    static void AssertTheSampleFiles (bool isThirteen)
    {
        std::vector<Byte>         image  = MakeImage (isThirteen);
        VolumeKind                kind   = VolumeImage::DetectFilesystem (image);
        uint32_t                  total  = isThirteen ? 455u : 560u;
        uint32_t                  system = isThirteen ? 26u : 32u;
        std::unique_ptr<IVolume>  volume;
        VolumeListing             listing;
        FilePayload               hello;
        FilePayload               data;
        FilePayload               notes;

        Assert::IsTrue (kind == VolumeKind::Rdos, L"the disk is an RDOS disk");
        Assert::AreEqual ("RDOS", VolumeImage::GetSystemName (kind));

        volume = VolumeImage::CreateVolume (kind, image);

        AssertSucceeded (volume->Enumerate (listing), L"the catalog lists");

        Assert::AreEqual ((size_t) 4, listing.entries.size(), L"the deleted entries are left out");
        Assert::IsTrue (listing.damage.empty(), L"the sample has no damage");
        Assert::IsFalse (listing.hasVolumeName);
        Assert::AreEqual (total, (uint32_t) listing.totalUnits);
        Assert::AreEqual (total - system - 4, (uint32_t) listing.freeUnits);

        Assert::AreEqual (std::string ("HELLO"), listing.entries[1].name);
        Assert::AreEqual ((int) 'A', (int) listing.entries[1].type);
        Assert::AreEqual (2u, (uint32_t) listing.entries[1].sizeUnits);
        Assert::AreEqual (300u, (uint32_t) listing.entries[1].eofBytes);
        Assert::AreEqual ((int) system, (int) listing.entries[1].firstBlock);

        Assert::AreEqual (std::string ("DATA"), listing.entries[2].name);
        Assert::IsTrue (listing.entries[2].hasLoadAddress);
        Assert::AreEqual (0x6000, (int) listing.entries[2].loadAddress);

        Assert::AreEqual (std::string ("NOTES"), listing.entries[3].name);
        Assert::AreEqual ((size_t) 8, (size_t) listing.entries[3].catalogIndex);
        Assert::AreEqual (18u, (uint32_t) listing.entries[3].eofBytes, L"a text file ends at its first zero");

        AssertSucceeded (volume->Read (FilePath::FromName ("HELLO"), hello), L"HELLO reads");
        Assert::AreEqual ((size_t) 300, hello.bytes.size());

        for (size_t i = 0; i < hello.bytes.size(); i++)
        {
            Assert::AreEqual ((int) HelloByte (i), (int) hello.bytes[i], L"HELLO's sectors are read in order");
        }

        AssertSucceeded (volume->Read (FilePath::FromName ("DATA"), data), L"DATA reads");
        Assert::AreEqual ((size_t) 100, data.bytes.size());
        Assert::AreEqual (0x5A, (int) data.bytes[99]);
        Assert::AreEqual ((int) 'B', (int) data.type);
        Assert::IsTrue (data.hasLoadAddress);
        Assert::AreEqual (0x6000, (int) data.loadAddress);

        AssertSucceeded (volume->Read (FilePath::FromName ("NOTES"), notes), L"NOTES reads");
        Assert::AreEqual ((size_t) 18, notes.bytes.size());
        Assert::AreEqual ((int) ('L' | 0x80), (int) notes.bytes[0]);
        Assert::AreEqual (0x8D, (int) notes.bytes[17]);
    }



    TEST_METHOD (SixteenSectorDisk_ListsAndReadsTheSampleFiles)
    {
        Assert::IsTrue (RdosVolume::DetectLayout (MakeImage (false)) == RdosVolume::Layout::Rdos33);

        AssertTheSampleFiles (false);
    }



    TEST_METHOD (ThirteenSectorLayout_ListsAndReadsTheSampleFiles)
    {
        Assert::IsTrue (RdosVolume::DetectLayout (MakeImage (true)) == RdosVolume::Layout::Rdos3);

        AssertTheSampleFiles (true);
    }



    TEST_METHOD (DetectLayout_ByTheVersionForAnotherSystemLength)
    {
        std::vector<Byte>  old     = MakeImage (true);
        std::vector<Byte>  current = MakeImage (false);

        old[At (true, 13) + 0x19]      = 20;
        current[At (false, 16) + 0x19] = 20;

        Assert::IsTrue (RdosVolume::DetectLayout (old) == RdosVolume::Layout::Rdos3);
        Assert::IsTrue (RdosVolume::DetectLayout (current) == RdosVolume::Layout::Rdos33);
    }



    TEST_METHOD (IsFound_OnlyForTheOperatingSystemEntry)
    {
        std::vector<Byte>  blank (kImageBytes, 0);
        std::vector<Byte>  moved    = MakeImage (false);
        std::vector<Byte>  other    = MakeImage (false);
        std::vector<Byte>  saveDisk = MakeImage (false);
        std::vector<Byte>  oneTrack (kSectorBytes * 16, 0);

        moved[At (false, 16) + 0x1E] = 1;

        SetEntry (other,    false, 0, "HELLO",                   'A', 32, 0, 0x2000, 0);
        SetEntry (saveDisk, false, 0, " >-SSI GAME SAVE DISK-<", 'S', 32, 0, 0x2000, 0);

        Assert::IsFalse (RdosVolume::IsFound (blank));
        Assert::IsFalse (RdosVolume::IsFound (oneTrack));
        Assert::IsFalse (RdosVolume::IsFound (moved), L"the first entry begins at sector 0");
        Assert::IsFalse (RdosVolume::IsFound (other));
        Assert::IsTrue (RdosVolume::IsFound (saveDisk));
        Assert::IsTrue (VolumeImage::DetectFilesystem (blank) != VolumeKind::Rdos);
    }



    TEST_METHOD (Enumerate_ListsDamageAndReadFailsForThatFile)
    {
        std::vector<Byte>  image = MakeImage (false);
        RdosVolume         volume (image);
        VolumeListing      listing;
        FilePayload        payload;

        SetEntry (image, false, 1, "HELLO", 'A', 2, 0, 300, 559);
        SetEntry (image, false, 2, "DATA",  'B', 1, 0x6000, 300, 34);

        AssertSucceeded (volume.Enumerate (listing), L"the catalog lists");

        Assert::AreEqual ((size_t) 4, listing.entries.size());
        Assert::AreEqual ((size_t) 2, listing.damage.size());
        Assert::AreEqual (std::string ("HELLO is damaged: the file's sectors end past the end of the disk"), listing.damage[0]);
        Assert::AreEqual (std::string ("DATA is damaged: the length is greater than the file's sectors"), listing.damage[1]);

        Assert::AreEqual (kCorrupt, volume.Read (FilePath::FromName ("HELLO"), payload));
        Assert::AreEqual (kCorrupt, volume.Read (FilePath::FromName ("DATA"), payload));
        Assert::AreEqual (kNotFound, volume.Read (FilePath::FromName ("MISSING"), payload));

        AssertSucceeded (volume.Read (FilePath::FromName ("NOTES"), payload), L"the other files read");
    }



    TEST_METHOD (BuildIntegrityReport_ClaimsTheSectorsOfEachFile)
    {
        std::vector<Byte>      image = MakeImage (false);
        RdosVolume             volume (image);
        VolumeIntegrityReport  report;

        AssertSucceeded (volume.BuildIntegrityReport (report), L"the report builds");

        Assert::IsTrue (report.IsClaimed (0), L"the operating system's sectors are in the first entry");
        Assert::IsTrue (report.IsClaimed ((uint32_t) (At (false, 33) / kSectorBytes)), L"HELLO's second sector");
        Assert::IsFalse (report.IsClaimed ((uint32_t) (At (false, 35) / kSectorBytes)), L"a sector in no file");
    }



    TEST_METHOD (EveryChange_FailsAsReadOnly)
    {
        std::vector<Byte>  image = MakeImage (false);
        std::vector<Byte>  out;
        RdosVolume         volume (image);
        FilePayload        payload;
        FileAttributes     attributes;
        DeleteOutcome      outcome;

        Assert::AreEqual (kReadOnly, volume.Write (FilePath::FromName ("NEW"), payload, out));
        Assert::AreEqual (kReadOnly, volume.Delete (FilePath::FromName ("HELLO"), out));
        Assert::AreEqual (kReadOnly, volume.Delete (FilePath::FromName ("HELLO"), out, outcome));
        Assert::AreEqual (kReadOnly, volume.Rename (FilePath::FromName ("HELLO"), "OTHER", out));
        Assert::AreEqual (kReadOnly, volume.SetAttributes (FilePath::FromName ("HELLO"), attributes, out));
        Assert::AreEqual (kReadOnly, volume.SetStartupProgram (FilePath::FromName ("HELLO"), out));
        Assert::IsTrue (out.empty());
    }



    TEST_METHOD (GetProDosType_MapsTheCatalogLetters)
    {
        Assert::AreEqual (0xFC, (int) RdosVolume::GetProDosType ('A'));
        Assert::AreEqual (0x06, (int) RdosVolume::GetProDosType ('B'));
        Assert::AreEqual (0x04, (int) RdosVolume::GetProDosType ('T'));
        Assert::AreEqual (0x00, (int) RdosVolume::GetProDosType ('S'));
    }
};
