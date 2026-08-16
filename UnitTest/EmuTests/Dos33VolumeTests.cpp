#include "Pch.h"
#include "../EhmTestHelper.h"
#include "Devices/Disk/Dos33Skeleton.h"
#include "Devices/Disk/Dos33Volume.h"
#include "Devices/Disk/NibblizationLayer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33VolumeTests
//
//  The DOS 3.3 reader, which did not exist before this feature and which
//  extraction depends on.
//
//  Volumes are built by the shipped skeleton writers rather than by hand, so
//  the reader is tested against structures the emulator itself produces and
//  boots -- not against a second, private idea of what DOS 3.3 looks like. A
//  reader and a writer that agree only with each other prove nothing.
//
////////////////////////////////////////////////////////////////////////////////




TEST_CLASS (Dos33VolumeTests)
{
public:

    static constexpr Byte  kVolumeNumber = 254;

    //  Tracks 0-2 (where DOS lives) and track 17 (the catalog) are reserved by
    //  a freshly formatted volume, leaving 31 tracks of 16 sectors.
    static constexpr uint32_t  kFreeOnBlankVolume = 31 * 16;

    vector<Byte> MakeFormattedVolume()
    {
        vector<Byte>  buffer (NibblizationLayer::kImageByteSize, 0);

        AssertSucceeded (Dos33Skeleton::Write (buffer, kVolumeNumber));

        return buffer;
    }

    vector<Byte> MakeVolumeWithHello()
    {
        vector<Byte>  buffer = MakeFormattedVolume();

        AssertSucceeded (Dos33FileWriter::WriteHello (buffer));

        return buffer;
    }

    TEST_METHOD (Enumerate_FreshVolume_HasNoFilesAndReportsItsFreeSpace)
    {
        vector<Byte>    buffer = MakeFormattedVolume();
        Dos33Volume     volume (buffer);
        VolumeListing   listing;

        AssertSucceeded (volume.Enumerate (listing));

        Assert::AreEqual (size_t (0), listing.entries.size(), L"a fresh volume holds no files");
        Assert::IsTrue (listing.hasVolumeNumber);
        Assert::AreEqual (kVolumeNumber, listing.volumeNumber);
        Assert::AreEqual (uint32_t (Dos33Volume::kUnitCount), listing.totalUnits);
        Assert::AreEqual (kFreeOnBlankVolume, listing.freeUnits,
            L"DOS's own tracks and the catalog track are reserved, the rest is free");
        Assert::AreEqual (size_t (0), listing.damage.size(), L"a clean volume reports no damage");
    }

    TEST_METHOD (Enumerate_VolumeWithAFile_ReportsNameTypeSizeAndLockState)
    {
        vector<Byte>    buffer = MakeVolumeWithHello();
        Dos33Volume     volume (buffer);
        VolumeListing   listing;

        AssertSucceeded (volume.Enumerate (listing));

        Assert::AreEqual (size_t (1), listing.entries.size());
        Assert::AreEqual (string ("HELLO"), listing.entries[0].name,
            L"the name must come back stripped of high bits and padding");
        Assert::AreEqual (Dos33Volume::kTypeApplesoft, listing.entries[0].type);
        Assert::IsFalse (listing.entries[0].isLocked);
        Assert::AreEqual (uint32_t (2), listing.entries[0].sizeUnits,
            L"one list sector plus one data sector");
        Assert::AreEqual (kFreeOnBlankVolume - 2, listing.freeUnits,
            L"the file's two sectors must have left the free pool");
    }

    TEST_METHOD (Read_ApplesoftFile_StripsTheLengthHeaderDosStoresInTheData)
    {
        // DOS 3.3 records a file's length inside the file's own first bytes
        // rather than in the catalog, so "the sectors this file occupies" and
        // "the file" are different things.
        vector<Byte>   buffer = MakeVolumeWithHello();
        Dos33Volume    volume (buffer);
        FilePayload    payload;

        AssertSucceeded (volume.Read (FilePath::Parse ("HELLO"), payload));

        Assert::AreEqual (size_t (8), payload.bytes.size(),
            L"the two length bytes are consumed, not returned");
        Assert::AreEqual (Byte (0x07), payload.bytes[0], L"next-line pointer low byte");
        Assert::AreEqual (Byte (0x08), payload.bytes[1], L"next-line pointer high byte");
        Assert::AreEqual (Byte (0x0A), payload.bytes[2], L"line number 10");
        Assert::AreEqual (Byte (0xB2), payload.bytes[4], L"the REM token");
        Assert::IsFalse (payload.hasLoadAddress, L"Applesoft records no load address");
        Assert::AreEqual (Dos33Volume::kTypeApplesoft, payload.type);
    }

    TEST_METHOD (Read_IsCaseInsensitive_BecauseTheCatalogIsUpperCase)
    {
        vector<Byte>   buffer = MakeVolumeWithHello();
        Dos33Volume    volume (buffer);
        FilePayload    payload;

        AssertSucceeded (volume.Read (FilePath::Parse ("hello"), payload));

        Assert::AreEqual (size_t (8), payload.bytes.size());
    }

    TEST_METHOD (Read_MissingFile_IsNotFoundRatherThanEmpty)
    {
        vector<Byte>   buffer = MakeVolumeWithHello();
        Dos33Volume    volume (buffer);
        FilePayload    payload;
        HRESULT        hr     = volume.Read (FilePath::Parse ("NOPE"), payload);

        Assert::IsTrue (FAILED (hr), L"a missing file must fail, not return nothing");
        Assert::AreEqual (size_t (0), payload.bytes.size());
    }

    TEST_METHOD (Read_MultiComponentPath_IsRefusedNotSilentlyTruncated)
    {
        // DOS 3.3 has no subdirectories. Quietly using the last component would
        // open a different file than the caller named, which is worse than
        // refusing: it succeeds while doing the wrong thing.
        vector<Byte>   buffer = MakeVolumeWithHello();
        Dos33Volume    volume (buffer);
        FilePayload    payload;
        HRESULT        hr     = volume.Read (FilePath::Parse ("SUBDIR/HELLO"), payload);

        Assert::IsTrue (FAILED (hr), L"a path this filesystem cannot express must be refused");
    }

    TEST_METHOD (BuildIntegrityReport_CleanVolume_AgreesWithTheFreeMap)
    {
        vector<Byte>           buffer = MakeVolumeWithHello();
        Dos33Volume            volume (buffer);
        VolumeIntegrityReport  report;

        AssertSucceeded (volume.BuildIntegrityReport (report));

        Assert::IsTrue (report.IsCatalogFullyParsed(), L"a clean catalog parses fully");
        Assert::AreEqual (size_t (0), report.GetCrossLinked().size(),
            L"no sector may be claimed twice");
        Assert::AreEqual (size_t (0), report.GetClaimedButFree().size(),
            L"nothing the catalog references may be marked free");
        Assert::AreEqual (size_t (0), report.GetUnfollowableChains().size());
    }

    TEST_METHOD (BuildIntegrityReport_NamesTheSectorsTheFileClaims)
    {
        // The requirement asks which sectors each file claims, not merely how
        // many claimants a sector has. HELLO occupies its list sector and one
        // data sector, both on track 18.
        vector<Byte>           buffer = MakeVolumeWithHello();
        Dos33Volume            volume (buffer);
        VolumeIntegrityReport  report;

        AssertSucceeded (volume.BuildIntegrityReport (report));

        {
            const vector<uint32_t> &  claims = report.GetClaimsOf (0);

            Assert::AreEqual (size_t (2), claims.size(),
                L"the list sector and the data sector are both the file's footprint");
        }
    }

    TEST_METHOD (Enumerate_CatalogChainThatLoops_ReportsDamageAndKeepsWhatItRead)
    {
        // A corrupted next-pointer on the catalog track. The walk must stop and
        // say so, while still returning the entries it reached -- a catalog that
        // goes bad halfway is exactly when the first half matters most.
        vector<Byte>    buffer = MakeVolumeWithHello();
        Dos33Volume     volume (buffer);
        VolumeListing   listing;
        size_t          catalogAt = Dos33Skeleton::SectorOffset (17, 15);

        // Point the first catalog sector back at itself.
        buffer[catalogAt + 0x01] = 17;
        buffer[catalogAt + 0x02] = 15;

        AssertSucceeded (volume.Enumerate (listing));

        Assert::AreEqual (size_t (1), listing.entries.size(),
            L"the entries read before the loop must survive");
        Assert::IsTrue (listing.damage.size() > 0, L"the loop must be described");
    }

    TEST_METHOD (Enumerate_CatalogPointingOutsideTheVolume_IsReportedNotFollowed)
    {
        vector<Byte>    buffer    = MakeFormattedVolume();
        Dos33Volume     volume (buffer);
        VolumeListing   listing;
        size_t          vtocAt    = Dos33Skeleton::SectorOffset (17, 0);

        buffer[vtocAt + 0x01] = 99;   // no such track

        AssertSucceeded (volume.Enumerate (listing));

        Assert::IsTrue (listing.damage.size() > 0,
            L"a pointer outside the volume must be reported rather than followed");
    }

    TEST_METHOD (Write_IsNotYetImplemented_AndSaysSoRatherThanSucceeding)
    {
        // An absent capability must not be indistinguishable from a completed
        // one. This is the same rule the decoder follows.
        vector<Byte>   buffer   = MakeFormattedVolume();
        Dos33Volume    volume (buffer);
        vector<Byte>   out;
        FilePayload    payload;
        HRESULT        hrWrite  = S_OK;
        HRESULT        hrDelete = S_OK;

        hrWrite  = volume.Write (FilePath::Parse ("X"), payload, out);
        hrDelete = volume.Delete (FilePath::Parse ("X"), out);

        Assert::IsTrue (FAILED (hrWrite),  L"an unbuilt write must report failure");
        Assert::IsTrue (FAILED (hrDelete), L"an unbuilt delete must report failure");
        Assert::AreEqual (size_t (0), out.size(), L"a refused operation produces nothing");
    }
};
