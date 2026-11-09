#include "Pch.h"
#include "../EhmTestHelper.h"
#include "Devices/Disk/FilePath.h"
#include "Machines/Apple2/Common/GutenbergVolume.h"
#include "Machines/Apple2/Common/VolumeImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  GutenbergVolumeTests
//
//  Disks built in each test after the layout of a Gutenberg, Jr. data disk:
//  volume SAMPLE, the directory in track 17, sector 7, and three documents in
//  track 3. TEXT is in sector 0, which the directory stores as $40. LONGER is
//  in sectors 1 and 2. ANOTHER is in sector 4, after an empty slot.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (GutenbergVolumeTests)
{
public:

    static constexpr size_t   kImageBytes  = 143360;
    static constexpr size_t   kSectorBytes = 256;
    static constexpr Byte     kStoredZero  = 0x40;
    static constexpr Byte     kEndFlag     = 0x80;
    static constexpr Byte     kFirstFlag   = 0x80;
    static constexpr Byte     kDocument    = 0xA0;
    static constexpr Byte     kLocked      = 0xCC;
    static constexpr HRESULT  kReadOnly    = HRESULT_FROM_WIN32 (ERROR_WRITE_PROTECT);
    static constexpr HRESULT  kCorrupt     = HRESULT_FROM_WIN32 (ERROR_FILE_CORRUPT);



    static size_t At (int track, int sector)
    {
        return ((size_t) track * 16 + (size_t) sector) * kSectorBytes;
    }



    static void SetHeader (std::vector<Byte> & image, int track, int sector, Byte currentSector, Byte nextTrack, Byte nextSector)
    {
        size_t  at = At (track, sector);

        image[at + 0] = (Byte) track;
        image[at + 1] = (Byte) sector;
        image[at + 2] = (Byte) track;
        image[at + 3] = currentSector;
        image[at + 4] = nextTrack;
        image[at + 5] = nextSector;
    }



    static void SetText (std::vector<Byte> & image, int track, int sector, const char * text)
    {
        size_t  at = At (track, sector) + 6;

        for (size_t i = 0; text[i] != 0; i++)
        {
            image[at + i] = (Byte) (text[i] | 0x80);
        }

        image[at + strlen (text)] = 0x8D;
    }



    static void SetEntry (std::vector<Byte> & image, int slot, const char * name, Byte track, Byte sector, Byte type)
    {
        size_t  at = At (17, 7) + 16 + (size_t) slot * 16;

        for (size_t i = 0; name[i] != 0; i++)
        {
            image[at + i] = (Byte) (name[i] | 0x80);
        }

        image[at + 12] = track;
        image[at + 13] = sector;
        image[at + 14] = type;
    }



    static std::vector<Byte> MakeImage()
    {
        std::vector<Byte>  image (kImageBytes, 0);
        size_t             directory = At (17, 7);

        std::fill_n (image.begin() + (ptrdiff_t) directory, kSectorBytes, (Byte) 0xA0);
        SetHeader (image, 17, 7, (Byte) (7 | kFirstFlag), (Byte) (17 | kEndFlag), 7);

        for (size_t marker = 15; marker < kSectorBytes; marker += 16)
        {
            image[directory + marker] = 0x8D;
        }

        for (size_t i = 0; i < 6; i++)
        {
            image[directory + 6 + i] = (Byte) ("SAMPLE"[i] | 0x80);
        }

        SetEntry (image, 0, "DIR",     17, 7,           kLocked);
        SetEntry (image, 1, "TEXT",    3,  kStoredZero, kDocument);
        SetEntry (image, 2, "LONGER",  3,  1,           kDocument);
        SetEntry (image, 4, "ANOTHER", 3,  4,           kDocument);

        SetHeader (image, 3, 0, (Byte) (0 | kFirstFlag), (Byte) (3 | kEndFlag), 0);
        SetHeader (image, 3, 1, (Byte) (1 | kFirstFlag), 3,                     2);
        SetHeader (image, 3, 2, 2,                       (Byte) (3 | kEndFlag), 1);
        SetHeader (image, 3, 4, (Byte) (4 | kFirstFlag), (Byte) (3 | kEndFlag), 4);

        SetText (image, 3, 0, "HELLO, WORLD!");
        SetText (image, 3, 1, "FIRST SECTOR");
        SetText (image, 3, 2, "SECOND SECTOR");
        SetText (image, 3, 4, "ANOTHER (SHORT) FILE.");

        image[At (17, 6)] = 0x81;

        return image;
    }



    static bool HasDamage (const VolumeListing & listing, const std::string & text)
    {
        for (const std::string & damage : listing.damage)
        {
            if (damage == text)
            {
                return true;
            }
        }

        return false;
    }



    TEST_METHOD (IsFound_ByTheDirectoryChain)
    {
        std::vector<Byte>  image   = MakeImage();
        std::vector<Byte>  blank (kImageBytes, 0);
        std::vector<Byte>  unmarked = image;
        std::vector<Byte>  endless  = image;



        unmarked[At (17, 7) + 0x4F] = 0x00;
        endless[At (17, 7) + 4]     = 17;

        Assert::IsTrue  (GutenbergVolume::IsFound (image));
        Assert::IsTrue  (VolumeImage::DetectFilesystem (image) == VolumeKind::Gutenberg);
        Assert::IsFalse (GutenbergVolume::IsFound (blank));
        Assert::IsFalse (GutenbergVolume::IsFound (unmarked));
        Assert::IsFalse (GutenbergVolume::IsFound (endless));
        Assert::IsFalse (GutenbergVolume::IsFound (std::vector<Byte> (kSectorBytes, 0)));
        Assert::AreEqual ("Gutenberg", VolumeImage::GetSystemName (VolumeKind::Gutenberg));
    }


    TEST_METHOD (Enumerate_ListsTheDirectory)
    {
        std::vector<Byte>  image = MakeImage();
        VolumeListing      listing;



        AssertSucceeded (VolumeImage::CreateVolume (VolumeKind::Gutenberg, image)->Enumerate (listing));

        Assert::IsTrue   (listing.hasVolumeName);
        Assert::AreEqual (std::string ("SAMPLE"), listing.volumeName);
        Assert::AreEqual ((size_t) 4, listing.entries.size());
        Assert::IsTrue   (listing.damage.empty());

        Assert::AreEqual (std::string ("DIR"),     listing.entries[0].name);
        Assert::AreEqual (std::string ("TEXT"),    listing.entries[1].name);
        Assert::AreEqual (std::string ("LONGER"),  listing.entries[2].name);
        Assert::AreEqual (std::string ("ANOTHER"), listing.entries[3].name);

        Assert::IsTrue   (listing.entries[0].isLocked);
        Assert::IsFalse  (listing.entries[1].isLocked);
        Assert::IsTrue   (listing.entries[0].type == GutenbergVolume::kTypeLocked);
        Assert::IsTrue   (listing.entries[1].type == GutenbergVolume::kTypeDocument);

        Assert::AreEqual (1u,   listing.entries[1].sizeUnits);
        Assert::AreEqual (250u, listing.entries[1].eofBytes);
        Assert::AreEqual (2u,   listing.entries[2].sizeUnits);
        Assert::AreEqual (500u, listing.entries[2].eofBytes);
        Assert::IsTrue   (listing.entries[1].firstBlock == 48);

        //  Tracks 3 to 34, less the four file sectors, the directory and the map.
        Assert::AreEqual (560u, listing.totalUnits);
        Assert::AreEqual (506u, listing.freeUnits);
    }


    TEST_METHOD (Read_ReturnsTheDataOfEachSector)
    {
        std::vector<Byte>  image = MakeImage();
        GutenbergVolume    volume (image);
        FilePayload        text;
        FilePayload        longer;
        FilePayload        missing;
        FilePayload        directory;



        AssertSucceeded (volume.Read (FilePath::Parse ("TEXT"),   text));
        AssertSucceeded (volume.Read (FilePath::Parse ("LONGER"), longer));
        AssertSucceeded (volume.Read (FilePath::Parse ("DIR"),    directory));

        Assert::AreEqual ((size_t) 250, text.bytes.size());
        Assert::IsTrue   (text.bytes[0] == (Byte) ('H' | 0x80));
        Assert::IsTrue   (text.bytes[13] == 0x8D);
        Assert::IsTrue   (text.bytes[14] == 0x00);
        Assert::IsFalse  (text.IsLocked());

        Assert::AreEqual ((size_t) 500, longer.bytes.size());
        Assert::IsTrue   (longer.bytes[0]   == (Byte) ('F' | 0x80));
        Assert::IsTrue   (longer.bytes[250] == (Byte) ('S' | 0x80));

        Assert::AreEqual ((size_t) 250, directory.bytes.size());
        Assert::IsTrue   (directory.IsLocked());

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND), volume.Read (FilePath::Parse ("NOPE"), missing));
    }


    TEST_METHOD (DamagedChain_IsListedAndIsNotRead)
    {
        std::vector<Byte>      stale   = MakeImage();
        std::vector<Byte>      outside = MakeImage();
        std::vector<Byte>      looped  = MakeImage();
        VolumeListing          listing;
        FilePayload            payload;
        VolumeIntegrityReport  report;



        //  Sector 3 has no header, track 40 isn't on the disk, and the third
        //  chain returns to its first sector with no end flag.
        stale[At (3, 2) + 4]   = 3;
        stale[At (3, 2) + 5]   = 3;
        outside[At (3, 1) + 4] = 40;
        looped[At (3, 2) + 4]  = 3;

        AssertSucceeded  (GutenbergVolume (stale).Enumerate (listing));
        Assert::AreEqual ((size_t) 4, listing.entries.size());
        Assert::AreEqual (2u, listing.entries[2].sizeUnits);
        Assert::IsTrue   (HasDamage (listing, "LONGER is damaged: a link to track 3, sector 3 isn't to a sector of a file"));
        Assert::AreEqual (kCorrupt, GutenbergVolume (stale).Read (FilePath::Parse ("LONGER"), payload));
        AssertSucceeded  (GutenbergVolume (stale).Read (FilePath::Parse ("TEXT"), payload));

        AssertSucceeded  (GutenbergVolume (stale).BuildIntegrityReport (report));
        Assert::IsFalse  (report.IsClean());
        Assert::IsTrue   (report.IsUniquelyOwnedBy (50, 2));

        AssertSucceeded  (GutenbergVolume (outside).Enumerate (listing));
        Assert::AreEqual (1u, listing.entries[2].sizeUnits);
        Assert::IsTrue   (HasDamage (listing, "LONGER is damaged: a link to track 40, sector 2 is outside the disk"));

        AssertSucceeded  (GutenbergVolume (looped).Enumerate (listing));
        Assert::IsTrue   (HasDamage (listing, "LONGER is damaged: the chain returns to track 3, sector 1 before its end"));
    }


    TEST_METHOD (BootTracks_AFileIsFollowedAndABadLinkIsNot)
    {
        std::vector<Byte>  program = MakeImage();
        std::vector<Byte>  bad     = MakeImage();
        VolumeListing      listing;
        FilePayload        payload;



        //  LONGER's second sector moves to track 0, sector 5, with both
        //  tracks stored as $40.
        program[At (3, 1) + 4] = kStoredZero;
        program[At (3, 1) + 5] = 5;
        program[At (0, 5) + 2] = kStoredZero;
        program[At (0, 5) + 3] = 5;
        program[At (0, 5) + 4] = (Byte) (3 | kEndFlag);
        program[At (0, 5) + 5] = 1;
        program[At (0, 5) + 6] = (Byte) ('Z' | 0x80);

        AssertSucceeded  (GutenbergVolume (program).Enumerate (listing));
        Assert::IsTrue   (listing.damage.empty());
        Assert::AreEqual (2u, listing.entries[2].sizeUnits);
        AssertSucceeded  (GutenbergVolume (program).Read (FilePath::Parse ("LONGER"), payload));
        Assert::IsTrue   (payload.bytes[250] == (Byte) ('Z' | 0x80));

        //  A link to track 0, sector 0, which has the boot code.
        bad[At (0, 0) + 0] = 0x01;
        bad[At (0, 0) + 1] = 0xA5;
        bad[At (0, 0) + 2] = 0x27;
        bad[At (0, 0) + 3] = 0xC9;
        bad[At (3, 1) + 4] = kStoredZero;
        bad[At (3, 1) + 5] = kStoredZero;

        AssertSucceeded  (GutenbergVolume (bad).Enumerate (listing));
        Assert::AreEqual (1u, listing.entries[2].sizeUnits);
        Assert::IsTrue   (HasDamage (listing, "LONGER is damaged: a link to track 0, sector 0 isn't to a sector of a file"));
        Assert::AreEqual (kCorrupt, GutenbergVolume (bad).Read (FilePath::Parse ("LONGER"), payload));
    }


    TEST_METHOD (EveryChange_FailsAsAWriteProtectedDiskDoes)
    {
        std::vector<Byte>  image = MakeImage();
        std::vector<Byte>  edited;
        GutenbergVolume    volume (image);
        FilePayload        payload;
        FileAttributes     attributes;
        DeleteOutcome      outcome;



        payload.bytes.assign (250, 0xC1);

        Assert::AreEqual (kReadOnly, volume.Write             (FilePath::Parse ("NEW"),  payload, edited));
        Assert::AreEqual (kReadOnly, volume.Delete            (FilePath::Parse ("TEXT"), edited));
        Assert::AreEqual (kReadOnly, volume.Delete            (FilePath::Parse ("TEXT"), edited, outcome));
        Assert::AreEqual (kReadOnly, volume.Rename            (FilePath::Parse ("TEXT"), "OTHER", edited));
        Assert::AreEqual (kReadOnly, volume.SetAttributes     (FilePath::Parse ("TEXT"), attributes, edited));
        Assert::AreEqual (kReadOnly, volume.SetStartupProgram (FilePath::Parse ("TEXT"), edited));
        Assert::IsTrue   (edited.empty());
    }


    TEST_METHOD (BuildIntegrityReport_ClaimsTheDirectoryTheMapAndEachFile)
    {
        std::vector<Byte>      image = MakeImage();
        VolumeIntegrityReport  report;



        AssertSucceeded (GutenbergVolume (image).BuildIntegrityReport (report));

        Assert::IsTrue  (report.IsClean());
        Assert::IsTrue  (report.IsCatalogFullyParsed());
        Assert::IsTrue  (report.IsUniquelyOwnedBy (17 * 16 + 7, VolumeIntegrityReport::kVolumeOwner));
        Assert::IsTrue  (report.IsUniquelyOwnedBy (17 * 16 + 6, VolumeIntegrityReport::kVolumeOwner));
        Assert::IsTrue  (report.IsUniquelyOwnedBy (48, 1));
        Assert::IsTrue  (report.IsUniquelyOwnedBy (49, 2));
        Assert::IsTrue  (report.IsUniquelyOwnedBy (50, 2));
        Assert::IsTrue  (report.IsUniquelyOwnedBy (52, 3));
        Assert::IsFalse (report.IsClaimed (51));
    }
};
