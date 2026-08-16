#include "Pch.h"
#include "../EhmTestHelper.h"
#include "FixtureProvider.h"
#include "Devices/Disk/Dos33Volume.h"
#include "Devices/Disk/ProDosVolume.h"
#include "Devices/Disk/NibblizationLayer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  VolumeOracleTests
//
//  The volume readers against real 1984 disks, and against files extracted from
//  those disks by a DIFFERENT implementation.
//
//  This is a materially stronger claim than the synthetic suites make. Those
//  build a volume with Casso's own writers and read it back with Casso's own
//  readers, so they establish that the code agrees with itself -- which it would
//  even if both shared a misunderstanding of the format. Here the expected bytes
//  were produced by an independent PowerShell reader walking the same catalog,
//  so agreement means two separate implementations of catalog traversal,
//  track/sector-list walking, and header handling reached the same answer.
//
//  Synthetic fixtures remain right for shapes that must be constructed on
//  purpose -- a deliberately corrupt catalog, an exhausted volume, a chosen
//  boundary. What they cannot answer is whether the reader understands the
//  format or merely agrees with the writer that shares its assumptions.
//
//  The disks are READ-ONLY third-party material. OpenFixture hands back a copy,
//  so nothing here can disturb them; a test that wrote to one would destroy
//  evidence for every later run.
//
////////////////////////////////////////////////////////////////////////////////




TEST_CLASS (VolumeOracleTests)
{
public:

    static constexpr const char *  kDos33Disk  = "Disks/Merlin-proDos2.23.dsk";
    static constexpr const char *  kProDosMerlin = "Disks/Merlin-proProdos2.33-a.dsk";
    static constexpr const char *  kProDosApple  = "Disks/Merlin-proProdos2.33-b.dsk";

    //  Every fixture file is the COMPLETE stored file, so a DOS 3.3 binary
    //  still carries its 4-byte load/length header. The reader strips it, so
    //  comparisons start at offset 4.
    static constexpr size_t  kDosBinHeaderBytes = 4;

    vector<Byte> Load (const char * relativePath)
    {
        FixtureProvider  fixtures;
        vector<Byte>     bytes;

        AssertSucceeded (fixtures.OpenFixture (relativePath, bytes));
        Assert::IsTrue (bytes.size() > 0, L"a fixture must not be empty");

        return bytes;
    }

    //  Compares one extracted file against the independently produced copy.
    void AssertMatchesOracle (const char * onDiskName, const char * fixturePath, Word expectedLoad)
    {
        vector<Byte>  disk     = Load (kDos33Disk);
        vector<Byte>  expected = Load (fixturePath);
        Dos33Volume   volume (disk);
        FilePayload   got;
        size_t        i        = 0;

        AssertSucceeded (volume.Read (FilePath::Parse (onDiskName), got));

        Assert::IsTrue (expected.size() > kDosBinHeaderBytes, L"the fixture carries its header");

        Assert::AreEqual (expected.size() - kDosBinHeaderBytes, got.bytes.size(),
            L"payload length must match the independently extracted file");

        Assert::IsTrue (got.hasLoadAddress, L"a DOS 3.3 binary records where it loads");
        Assert::AreEqual (expectedLoad, got.loadAddress);

        for (i = 0; i < got.bytes.size(); i++)
        {
            if (got.bytes[i] != expected[i + kDosBinHeaderBytes])
            {
                Assert::Fail (L"payload differs from the independently extracted file");
            }
        }
    }

    TEST_METHOD (Oracle_MakeDump_MatchesIndependentExtraction)
    {
        // 593 stored bytes: a 4-byte header declaring load $9000 and length 589.
        AssertMatchesOracle ("MAKE DUMP", "Merlin/MAKE DUMP", 0x9000);
    }

    TEST_METHOD (Oracle_Keymac_MatchesIndependentExtraction)
    {
        AssertMatchesOracle ("KEYMAC", "Merlin/KEYMAC", 0x9000);
    }

    TEST_METHOD (Oracle_Labels_MatchesIndependentExtraction)
    {
        AssertMatchesOracle ("LABELS", "Merlin/LABELS", 0x8000);
    }

    TEST_METHOD (Oracle_Printfiler_MatchesIndependentExtraction)
    {
        // The smallest object, 290 stored bytes loading at $02A0.
        AssertMatchesOracle ("PRINTFILER", "Merlin/PRINTFILER", 0x02A0);
    }

    TEST_METHOD (Oracle_Clock24_MatchesIndependentExtraction)
    {
        AssertMatchesOracle ("CLOCK.24", "Merlin/CLOCK.24", 0x0240);
    }

    TEST_METHOD (Oracle_SourceFilesAreTypeBToo_SoTheHeaderApplies)
    {
        // Merlin stores its SOURCE as type B rather than type T, all loading at
        // $0901. A reader that assumed sources were text would return four
        // extra bytes and call them content.
        AssertMatchesOracle ("MAKE DUMP.S", "Merlin/MAKE DUMP.S", 0x0901);
    }

    TEST_METHOD (Oracle_MultiSectorFile_MatchesIndependentExtraction)
    {
        // 6,663 stored bytes is 27 sectors, well past a single track and deep
        // into the track/sector list. The synthetic multi-list test constructs
        // that shape; this one finds it on a disk somebody shipped.
        AssertMatchesOracle ("KEYMAC.S", "Merlin/KEYMAC.S", 0x0901);
    }

    TEST_METHOD (Dos33_RealDisk_EnumeratesItsCatalog)
    {
        vector<Byte>   disk = Load (kDos33Disk);
        Dos33Volume    volume (disk);
        VolumeListing  listing;
        bool           sawMakeDump = false;
        bool           sawLocked   = false;

        AssertSucceeded (volume.Enumerate (listing));

        Assert::IsTrue (listing.hasVolumeNumber);
        Assert::AreEqual (Byte (254), listing.volumeNumber);
        Assert::AreEqual (size_t (63), listing.entries.size(),
            L"the vendor's own catalog lists 63 entries");
        Assert::AreEqual (size_t (0), listing.damage.size(),
            L"a disk that shipped must read clean");

        for (const FileEntry & entry : listing.entries)
        {
            sawMakeDump = sawMakeDump || entry.name == "MAKE DUMP";
            sawLocked   = sawLocked   || entry.isLocked;
        }

        Assert::IsTrue (sawMakeDump, L"names with embedded spaces must survive");
        Assert::IsTrue (sawLocked,   L"the disk carries locked files");
    }

    TEST_METHOD (Dos33_RealDisk_IntegrityAgreesWithItsFreeMap)
    {
        // The strongest single statement available about the reader: a volume
        // shipped by its vendor must be internally consistent, and any
        // disagreement is a defect here rather than on the disk.
        vector<Byte>           disk = Load (kDos33Disk);
        Dos33Volume            volume (disk);
        VolumeIntegrityReport  report;

        AssertSucceeded (volume.BuildIntegrityReport (report));

        Assert::IsTrue (report.IsCatalogFullyParsed(), L"the catalog must parse fully");
        Assert::AreEqual (size_t (0), report.GetCrossLinked().size(),
            L"no sector may be claimed by two files");
        Assert::AreEqual (size_t (0), report.GetUnfollowableChains().size(),
            L"every file's chain must walk to its end");
    }

    TEST_METHOD (Dos33_DecorativeCatalogEntries_AreNotDamage)
    {
        // This disk draws section headings in CATALOG using entries that
        // occupy no storage: the vendor's own listing shows them as
        // "*T 000 AMERLIN PRO - DOS 3.3", type T, zero sectors, and their
        // track/sector pointer is $7F/$7F -- a sentinel never meant to be
        // followed. DOS lists them without complaint.
        //
        // A reader that called those broken chains would report a disk its
        // publisher shipped as damaged, and on this volume that is twenty of
        // sixty-three entries. The discriminator is the entry's own sector
        // count: zero means it declares it occupies nothing, so there is
        // nothing to reach and nothing lost.
        vector<Byte>           disk = Load (kDos33Disk);
        Dos33Volume            volume (disk);
        VolumeListing          listing;
        VolumeIntegrityReport  report;
        int                    zeroSectorEntries = 0;

        AssertSucceeded (volume.Enumerate (listing));

        for (const FileEntry & entry : listing.entries)
        {
            if (entry.sizeUnits == 0)
            {
                zeroSectorEntries++;
            }
        }

        Assert::AreEqual (20, zeroSectorEntries,
            L"the disk carries twenty heading entries that occupy no sectors");

        AssertSucceeded (volume.BuildIntegrityReport (report));

        Assert::AreEqual (size_t (0), report.GetUnfollowableChains().size(),
            L"an entry that claims no sectors has nothing to follow, which is not damage");
    }

    TEST_METHOD (ProDos_RealDiskInDosSectorOrder_Enumerates)
    {
        // All three images are in DOS sector order, INCLUDING the ProDOS pair.
        // Sector order and filesystem are independent, and this is the common
        // case in the wild rather than an exotic one: block 2 sits at track 0
        // sector 11, not at byte offset 1024.
        vector<Byte>   disk = Load (kProDosMerlin);
        ProDosVolume   volume (disk);
        VolumeListing  listing;

        AssertSucceeded (volume.Enumerate (listing));

        Assert::IsTrue (listing.hasVolumeName);
        Assert::AreEqual (string ("MERLIN"), listing.volumeName);
        Assert::IsTrue (listing.entries.size() > 0, L"the volume holds files");
    }

    TEST_METHOD (ProDos_RealDisk_HasSubdirectoriesSoDeeperPathsAreRefused)
    {
        // /MERLIN carries real subdirectories, so this exercises the refusal
        // rather than assuming it. Traversal is not built; a deeper path must
        // be declined, never reduced to its last component.
        vector<Byte>   disk = Load (kProDosMerlin);
        ProDosVolume   volume (disk);
        VolumeListing  listing;
        FilePayload    got;
        HRESULT        hr          = S_OK;
        bool           sawSubdir   = false;

        AssertSucceeded (volume.Enumerate (listing));

        for (const FileEntry & entry : listing.entries)
        {
            sawSubdir = sawSubdir || entry.isDirectory;
        }

        Assert::IsTrue (sawSubdir, L"the volume must carry a subdirectory to make this meaningful");

        hr = volume.Read (FilePath::Parse ("SOURCE/PI.ADD.S"), got);

        Assert::IsTrue (FAILED (hr), L"a path this reader cannot walk must be refused");
        Assert::AreEqual (size_t (0), got.bytes.size(), L"and must return nothing");
    }

    TEST_METHOD (ProDos_NearlyFullVolume_ReportsItsFreeSpace)
    {
        // /APPLESOFT is nearly full, which is where a free-space count is worth
        // checking: an off-by-one in bitmap walking hides on an empty volume.
        vector<Byte>   disk = Load (kProDosApple);
        ProDosVolume   volume (disk);
        VolumeListing  listing;

        AssertSucceeded (volume.Enumerate (listing));

        Assert::AreEqual (string ("APPLESOFT"), listing.volumeName);
        Assert::AreEqual (uint32_t (280), listing.totalUnits);
        Assert::IsTrue (listing.freeUnits < 30,
            L"a nearly full volume must report little free space");
    }

    TEST_METHOD (ProDos_RealDisk_IntegrityAgreesWithItsBitmap)
    {
        vector<Byte>           disk = Load (kProDosApple);
        ProDosVolume           volume (disk);
        VolumeIntegrityReport  report;

        AssertSucceeded (volume.BuildIntegrityReport (report));

        Assert::IsTrue (report.IsCatalogFullyParsed());
        Assert::AreEqual (size_t (0), report.GetCrossLinked().size(),
            L"no block may be claimed by two files");
    }
};
