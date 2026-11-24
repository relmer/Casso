#include "Pch.h"

#include "StampDiskFixtures.h"

#include "Devices/Disk/DiskImage.h"
#include "Devices/Disk/Inspector/DecodeSettings.h"
#include "Devices/Disk/Inspector/DiskCopy.h"
#include "Devices/Disk/Stamp/FormatStamp.h"
#include "Devices/Disk/Stamp/FormatStampBuilder.h"
#include "Devices/Disk/Stamp/FormatStampChunk.h"
#include "Devices/Disk/Stamp/FormatStampRefresher.h"
#include "Machines/Apple2/Common/WozLoader.h"
#include "Machines/Apple2/Common/WozMetadata.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  FormatStampRefresherTests
//
//  The stamp a save writes for an in-memory disk stamped as built, after
//  its tracks were rewritten and when they were not.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (FormatStampRefresherTests)
{
public:

    static constexpr int   kRewritten = 9;
    static constexpr Byte  kD5        = 0xD5;
    static constexpr Byte  kD7        = 0xD7;
    static constexpr Byte  kAA        = 0xAA;
    static constexpr Byte  k96        = 0x96;



    //  A 16-sector disk with a capture stamp built from its bits.
    static void MakeStampedDisk (DiskImage & outImage)
    {
        FormatStamp    stamp;
        CaptureReport  capture;
        WozMetadata    meta;



        StampDiskFixtures::MakeSixteenSector (outImage);

        capture.weak     = { kRewritten * DiskImage::kQuarterTracksPerWholeTrack };
        capture.replaced = { { kRewritten * DiskImage::kQuarterTracksPerWholeTrack, 3 } };

        FormatStampBuilder::Build (*StampDiskFixtures::MakeCopy (outImage), DecodeSettings::MakeStandard(), StampSource::Capture, &capture, "2026-11-23T00:00:00Z", stamp);

        meta = outImage.GetWozMetadata();
        FormatStampChunk::SetStamp (meta, stamp);
        outImage.SetWozMetadata (meta);
    }



    static const vector<Byte> & GetChunk (const WozMetadata & meta)
    {
        return std::ranges::find_if (meta.passThrough, [] (const WozChunk & chunk) { return memcmp (chunk.id, FormatStampChunk::kId, sizeof (chunk.id)) == 0; })->payload;
    }



    static void ReadStamp (const WozMetadata & meta, FormatStamp & outStamp)
    {
        Assert::IsTrue (FormatStampChunk::GetStamp (meta, outStamp) == StampState::Readable);
    }



    TEST_METHOD (NoTrackChanged_WritesTheChunkBackByteForByte)
    {
        DiskImage    image;
        WozMetadata  meta;



        MakeStampedDisk (image);

        FormatStampRefresher::GetMetadataForSave (image, meta);

        Assert::IsTrue (GetChunk (meta) == GetChunk (image.GetWozMetadata()));
    }



    TEST_METHOD (RewrittenTrack_IsAnalyzedAgainAndTheOthersAreKept)
    {
        DiskImage    image;
        WozMetadata  meta;
        FormatStamp  before;
        FormatStamp  after;
        Byte         from[] = { kD5, kAA, k96 };
        Byte         to[]   = { kD7, kAA, k96 };



        MakeStampedDisk (image);
        ReadStamp (image.GetWozMetadata(), before);

        StampDiskFixtures::ReplaceProlog (image, kRewritten, from, to);

        FormatStampRefresher::GetMetadataForSave (image, meta);
        ReadStamp (meta, after);

        Assert::AreEqual (before.tracks.size(), after.tracks.size());

        for (size_t i = 0; i < after.tracks.size(); i++)
        {
            bool  isRewritten = after.tracks[i].position == kRewritten * DiskImage::kQuarterTracksPerWholeTrack;

            Assert::AreEqual (isRewritten, after.tracks[i].isChangedAfterCapture);
            Assert::AreEqual (isRewritten, after.tracks[i].bitsCrc != before.tracks[i].bitsCrc);
        }

        const StampTrack &  rewritten = after.tracks[kRewritten];

        Assert::IsTrue  (rewritten.addressPrologs == vector<vector<Byte>> { { kD7, kAA, k96 } }, L"a reformatted track lists its new marks");
        Assert::IsTrue  (rewritten.isVerified);
        Assert::IsFalse (rewritten.hasCapture, L"the capture facts describe bits that are gone");
        Assert::IsTrue  (after.tracks[kRewritten + 1].hasCapture);
    }



    TEST_METHOD (SaveAndLoad_HasTheRefreshedStamp)
    {
        DiskImage     image;
        DiskImage     reloaded;
        vector<Byte>  saved;
        FormatStamp   after;
        Byte          from[] = { kD5, kAA, k96 };
        Byte          to[]   = { kD7, kAA, k96 };
        HRESULT       hr     = S_OK;



        MakeStampedDisk (image);
        StampDiskFixtures::ReplaceProlog (image, kRewritten, from, to);

        hr = WozLoader::Serialize (image, saved);
        Assert::IsTrue (SUCCEEDED (hr));

        hr = WozLoader::Load (saved, reloaded);
        Assert::IsTrue (SUCCEEDED (hr));

        ReadStamp (reloaded.GetWozMetadata(), after);
        Assert::IsTrue (after.tracks[kRewritten].isChangedAfterCapture);
        Assert::IsTrue (after.tracks[kRewritten].addressPrologs == vector<vector<Byte>> { { kD7, kAA, k96 } });
    }



    TEST_METHOD (TrackGainedARecord_GetsAnEntry)
    {
        DiskImage    image;
        WozMetadata  meta;
        FormatStamp  stamp;
        int          slot = 0;



        MakeStampedDisk (image);

        slot = image.GetTrackCount();
        image.EnsureTrackSlots (slot + 1);
        image.ResizeTrack (slot, image.GetTrackBitCount (0));
        image.GetTrackBitsForWrite (slot) = image.GetTrackBits (0);
        image.SetQuarterTrackSlot (2, slot);

        FormatStampRefresher::GetMetadataForSave (image, meta);
        ReadStamp (meta, stamp);

        Assert::IsTrue (std::ranges::any_of (stamp.tracks, [] (const StampTrack & entry) { return entry.position == 2 && entry.isChangedAfterCapture; }));
        Assert::IsTrue (std::ranges::is_sorted (stamp.tracks, {}, &StampTrack::position));
    }



    TEST_METHOD (UnreadableStamp_IsRemovedOnceATrackChanged)
    {
        DiskImage    image;
        WozMetadata  meta;
        WozMetadata  saved;
        WozChunk     chunk;
        std::string  text = "{ \"format\": \"casso-woz-stamp\", \"version\": \"2.0\" }";



        StampDiskFixtures::MakeSixteenSector (image);

        meta = image.GetWozMetadata();
        memcpy (chunk.id, FormatStampChunk::kId, sizeof (chunk.id));
        chunk.payload.assign (text.begin(), text.end());
        meta.passThrough.push_back (chunk);
        image.SetWozMetadata (meta);

        FormatStampRefresher::GetMetadataForSave (image, saved);
        Assert::IsTrue (FormatStampChunk::HasStampChunk (saved), L"with no track changed, it is written back");

        image.MarkTrackDirty (kRewritten);

        FormatStampRefresher::GetMetadataForSave (image, saved);
        Assert::IsFalse (FormatStampChunk::HasStampChunk (saved));
    }
};
