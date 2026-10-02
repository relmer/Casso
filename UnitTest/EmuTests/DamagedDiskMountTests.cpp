#include "Pch.h"
#include "Devices/Disk/DamagedMountReport.h"
#include "Devices/Disk/DiskImage.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Machines/Apple2/Common/WozLoader.h"
#include "FluxTestImages.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DamagedDiskMountTests
//
//  A WOZ image with tracks that cannot be read from the file -- flux or bit,
//  WOZ 1 or WOZ 2 -- mounts read-only with those tracks blank, lists them, and
//  offers salvage, instead of refusing the whole disk. The damage is made in
//  the WOZ bytes, the way a bad download or a bad tool would make it.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DamagedDiskMountTests)
{
public:

    static constexpr int     kTracks      = NibblizationLayer::kTrackCount;
    static constexpr size_t  kSectorBytes = NibblizationLayer::kSectorByteSize;

    static vector<Byte> MakeSectors()
    {
        vector<Byte>  sectors (NibblizationLayer::kImageByteSize, 0);
        size_t        i = 0;

        for (i = 0; i < sectors.size(); i++)
        {
            sectors[i] = static_cast<Byte> ((i * 13 + i / 256) & 0xFF);
        }

        return sectors;
    }


    //  A DOS-formatted WOZ 2.1 image, track t in TRKS record t at quarter
    //  track 4t, every track flux or every track bits.
    static vector<Byte> MakeWoz (const vector<Byte> & sectors, bool allFlux)
    {
        DiskImage                  bitImage;
        vector<WozSyntheticTrack>  tracks;
        vector<Byte>               image;
        int                        t  = 0;
        HRESULT                    hr = S_OK;

        hr = NibblizationLayer::NibblizeDsk (sectors, bitImage);
        Assert::IsTrue (SUCCEEDED (hr));

        for (t = 0; t < kTracks; t++)
        {
            WozSyntheticTrack  track;

            track.isFlux        = allFlux;
            track.quarterTracks = { t * 4 };
            track.bitCount      = bitImage.GetTrackBitCount (t);
            track.data          = allFlux
                                  ? FluxTestImages::BitsToNominalFlux (bitImage.GetTrackBits (t), track.bitCount)
                                  : bitImage.GetTrackBits (t);
            tracks.push_back (track);
        }

        hr = WozLoader::BuildSyntheticV21 (tracks, image);
        Assert::IsTrue (SUCCEEDED (hr));

        return image;
    }


    static size_t FindChunk (const vector<Byte> & image, const char * id)
    {
        size_t  at = 0;

        for (at = WozLoader::kHeaderSize; at + 4 <= image.size(); at++)
        {
            if (memcmp (image.data() + at, id, 4) == 0)
            {
                return at;
            }
        }

        return 0;
    }


    static Byte * GetTrkRecord (vector<Byte> & image, int index)
    {
        return image.data() + FindChunk (image, "TRKS") + 8 + static_cast<size_t> (index) * WozLoader::kV2TrkRecordSize;
    }


    static void LoadOk (const vector<Byte> & image, DiskImage & disk)
    {
        HRESULT  hr = WozLoader::Load (image, disk);

        Assert::IsTrue (SUCCEEDED (hr), L"a disk with damaged tracks must still mount");
    }


    static void AssertDamaged (const DiskImage & disk, int index, bool isFlux, DamageReason reason)
    {
        Assert::IsTrue   (disk.IsWriteProtected(), L"a disk with damaged tracks must be read-only");
        Assert::AreEqual (static_cast<size_t> (1), disk.GetDamagedTracks().size());
        Assert::AreEqual (index, disk.GetDamagedTracks()[0].trkIndex);
        Assert::AreEqual (isFlux, disk.GetDamagedTracks()[0].isFlux);
        Assert::IsTrue   (disk.GetDamagedTracks()[0].reason == reason);
        Assert::IsTrue   (disk.GetWriteProtectInfo().damagedTracks);
    }


    TEST_METHOD (FluxTrackEndingInARunByteMountsReadOnly)
    {
        vector<Byte>  image = MakeWoz (MakeSectors(), true);
        DiskImage     disk;
        Byte       *  rec   = GetTrkRecord (image, 7);
        size_t        start = static_cast<size_t> (rec[0] | (rec[1] << 8)) * WozLoader::kV2BlockSize;
        size_t        count = static_cast<size_t> (rec[4] | (rec[5] << 8) | (rec[6] << 16));

        image[start + count - 1] = 255;

        LoadOk (image, disk);
        AssertDamaged (disk, 7, true, DamageReason::TruncatedRun);

        Assert::IsFalse (disk.GetFluxTrack (7).HasTransitions(), L"the damaged track must read as blank");
        Assert::IsTrue  (disk.GetFluxTrack (8).HasTransitions(), L"the tracks around it must still read");
    }


    TEST_METHOD (FluxEntryOutsideTheFileMountsReadOnly)
    {
        vector<Byte>  image = MakeWoz (MakeSectors(), true);
        DiskImage     disk;
        Byte       *  rec   = GetTrkRecord (image, 12);

        rec[0] = 0xF0;
        rec[1] = 0xFF;

        LoadOk (image, disk);
        AssertDamaged (disk, 12, true, DamageReason::OutsideFile);
    }


    TEST_METHOD (FluxCountBeyondItsBlocksMountsReadOnly)
    {
        vector<Byte>  image = MakeWoz (MakeSectors(), true);
        DiskImage     disk;
        Byte       *  rec   = GetTrkRecord (image, 2);

        rec[2] = 1;
        rec[3] = 0;

        LoadOk (image, disk);
        AssertDamaged (disk, 2, true, DamageReason::CountExceedsBlocks);
    }


    TEST_METHOD (BitTrackOutsideTheFileMountsReadOnly)
    {
        vector<Byte>  image = MakeWoz (MakeSectors(), false);
        DiskImage     disk;
        Byte       *  rec   = GetTrkRecord (image, 20);

        rec[0] = 0xF0;
        rec[1] = 0xFF;

        LoadOk (image, disk);
        AssertDamaged (disk, 20, false, DamageReason::OutsideFile);

        Assert::AreEqual (-1, disk.ResolveQuarterTrack (80), L"the damaged bit track must read as blank");
        Assert::AreEqual (21, disk.ResolveQuarterTrack (84));
    }


    TEST_METHOD (Woz1RecordPastTrksMountsReadOnly)
    {
        const Byte    sig[8] = { 'W', 'O', 'Z', '1', 0xFF, 0x0A, 0x0D, 0x0A };
        vector<Byte>  image (sig, sig + 8);
        DiskImage     disk;
        int           qt     = 0;

        image.insert (image.end(), 4, 0);

        image.insert (image.end(), { 'I', 'N', 'F', 'O', 60, 0, 0, 0 });
        image.insert (image.end(), 60, 0);
        image[12 + 8] = 1;
        image[12 + 9] = WozLoader::kDiskType525;

        image.insert (image.end(), { 'T', 'M', 'A', 'P', 160, 0, 0, 0 });

        for (qt = 0; qt < 160; qt++)
        {
            image.push_back ((qt == 0) ? Byte (0) : (qt == 4) ? Byte (3) : Byte (0xFF));
        }

        // One record in TRKS; quarter track 4 asks for the fourth.
        image.insert (image.end(), { 'T', 'R', 'K', 'S', 0x00, 0x1A, 0, 0 });
        image.insert (image.end(), WozLoader::kV1TrackRecordSize, 0xFF);
        image[image.size() - 8] = 0x00;
        image[image.size() - 7] = 0x19;

        LoadOk (image, disk);
        AssertDamaged (disk, 3, false, DamageReason::V1RecordPastTrks);
    }


    TEST_METHOD (UndamagedDiskHasNoDamagedTracks)
    {
        DiskImage  disk;

        LoadOk (MakeWoz (MakeSectors(), true), disk);

        Assert::IsFalse (disk.HasDamagedTracks());
        Assert::IsFalse (disk.IsWriteProtected());
        Assert::IsTrue  (DamagedMountReport::FormatBody (disk, L"x.woz").empty());
    }


    TEST_METHOD (SalvageIsOfferedAndRecoversTheReadableFluxTracks)
    {
        vector<Byte>       sectors = MakeSectors();
        vector<Byte>       image   = MakeWoz (sectors, true);
        DiskImageStore     store;
        SalvageAssessment  assessment;
        DenibblizeReport   report;
        vector<Byte>       salvaged;
        Byte            *  rec     = GetTrkRecord (image, 9);
        DiskImage       *  mounted = nullptr;
        HRESULT            hr      = S_OK;

        rec[0] = 0xF0;
        rec[1] = 0xFF;

        hr = store.MountFromBytes (6, 1, "mem:damaged.woz", DiskFormat::Woz, image);
        Assert::IsTrue (SUCCEEDED (hr));

        hr = store.AssessSalvage (6, 1, assessment);
        Assert::IsTrue (SUCCEEDED (hr));
        Assert::IsTrue (assessment.isOffered, L"salvage must be offered for damaged tracks");

        mounted = store.GetImage (6, 1);
        Assert::IsNotNull (mounted);

        hr = NibblizationLayer::SalvageSectors (*mounted, DiskFormat::Dsk, salvaged, report);
        Assert::IsTrue (SUCCEEDED (hr));

        Assert::IsTrue (memcmp (salvaged.data(), sectors.data(), 9 * 16 * kSectorBytes) == 0,
                        L"the flux tracks before the damaged one must salvage whole");
        Assert::IsTrue (memcmp (salvaged.data() + 10 * 16 * kSectorBytes, sectors.data() + 10 * 16 * kSectorBytes,
                                (kTracks - 10) * 16 * kSectorBytes) == 0,
                        L"the flux tracks after the damaged one must salvage whole");
    }


    TEST_METHOD (TrackListReadsAsASentence)
    {
        Assert::AreEqual (wstring (L"track 3"),                 DamagedMountReport::FormatTrackList ({ 12 }));
        Assert::AreEqual (wstring (L"tracks 3 and 7.5"),        DamagedMountReport::FormatTrackList ({ 12, 30 }));
        Assert::AreEqual (wstring (L"tracks 3, 7.5, and 12"),   DamagedMountReport::FormatTrackList ({ 12, 30, 48 }));
        Assert::AreEqual (wstring (L"tracks 0.25 and 0.75"),    DamagedMountReport::FormatTrackList ({ 1, 3 }));
    }


    TEST_METHOD (TrackListStopsAfterEight)
    {
        vector<int>  quarterTracks;
        int          i = 0;

        for (i = 0; i < 11; i++)
        {
            quarterTracks.push_back (i * 4);
        }

        Assert::AreEqual (wstring (L"tracks 0, 1, 2, 3, 4, 5, 6, 7, and 3 more"),
                          DamagedMountReport::FormatTrackList (quarterTracks));
    }


    TEST_METHOD (ReportListsTheDamagedTrackAtTheMiddleOfItsRun)
    {
        vector<Byte>  image = MakeWoz (MakeSectors(), true);
        DiskImage     disk;
        Byte       *  rec   = GetTrkRecord (image, 5);
        size_t        flux  = FindChunk (image, "FLUX") + 8;
        wstring       body;

        // Track 5 covers quarter tracks 19 through 21, so it reads as 5.
        image[flux + 19] = 5;
        image[flux + 21] = 5;
        rec[0] = 0xF0;
        rec[1] = 0xFF;

        LoadOk (image, disk);

        Assert::IsTrue (DamagedMountReport::GetDamagedQuarterTracks (disk) == vector<int> { 20 });

        body = DamagedMountReport::FormatBody (disk, L"C:\\disks\\damaged.woz");

        Assert::IsTrue (body.find (L"could not be read from the file: track 5.") != wstring::npos);
        Assert::IsTrue (body.find (L"C:\\disks\\damaged.woz") != wstring::npos);
        Assert::IsTrue (body.find (L"write-protected it for this session") != wstring::npos);
        Assert::IsTrue (body.find (L"does not keep this disk's flux timing") != wstring::npos,
                        L"a flux disk's report must say what salvage cannot keep");
    }


    TEST_METHOD (ChecksumAndTrackDamageShareOneReport)
    {
        vector<Byte>  image = MakeWoz (MakeSectors(), false);
        DiskImage     disk;
        Byte       *  rec   = GetTrkRecord (image, 4);
        wstring       body;

        rec[0] = 0xF0;
        rec[1] = 0xFF;

        LoadOk (image, disk);
        disk.SetSourceCrcMismatch (true);

        body = DamagedMountReport::FormatBody (disk, L"d.woz");

        Assert::IsTrue (body.find (L"stored checksum does not match") != wstring::npos);
        Assert::IsTrue (body.find (L"Also, some of this disk image's tracks") != wstring::npos);
        Assert::IsTrue (body.find (L"flux timing") == wstring::npos, L"a bit-only disk has no flux to lose");
    }
};
