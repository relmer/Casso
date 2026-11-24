#include "Pch.h"

#include "StampDiskFixtures.h"

#include "Devices/Disk/DiskImage.h"
#include "Devices/Disk/Inspector/DecodeSettings.h"
#include "Devices/Disk/Inspector/DiskAnalyzer.h"
#include "Devices/Disk/Inspector/DiskCopy.h"
#include "Devices/Disk/Inspector/ImageDetails.h"
#include "Devices/Disk/Stamp/FormatStamp.h"
#include "Devices/Disk/Stamp/FormatStampBuilder.h"
#include "Devices/Disk/Stamp/FormatStampChunk.h"
#include "Devices/Disk/Stamp/StampedTrackMarks.h"
#include "Machines/Apple2/Common/WozMetadata.h"
#include "Shell/InspectorRequestQueue.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  FormatStampInspectorTests
//
//  What the disk inspector does with a format stamp: the Stamp format
//  request on a fake drive, the decode settings a stamped disk starts with,
//  the capture facts copied into a new stamp, and the stamp in Image details.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (FormatStampInspectorTests)
{
public:

    static constexpr Byte  kD4 = 0xD4;
    static constexpr Byte  kD5 = 0xD5;
    static constexpr Byte  kAA = 0xAA;
    static constexpr Byte  k96 = 0x96;



    class FakeSource : public IInspectorDiskSource
    {
    public:
        DiskImage  image;
        uint64_t   mediaId   = 42;
        int        saves     = 0;
        HRESULT    saveHr    = S_OK;

        const DiskImage * GetImage (int drive, InspectorDiskIdentity & outIdentity) override
        {
            outIdentity.mediaId = mediaId;

            return drive == 0 ? &image : nullptr;
        }

        DiskImage * GetImageForEdit (int drive) override
        {
            return drive == 0 ? &image : nullptr;
        }

        InspectorSaveResult SaveDrive (int drive) override
        {
            InspectorSaveResult  result;

            (void) drive;
            saves++;
            result.hr     = saveHr;
            result.path   = "fake.woz";
            result.reason = L"the file is locked";

            return result;
        }
    };



    static InspectorReply PostStamp (FakeSource & source, const FormatStamp & stamp)
    {
        InspectorRequestQueue   queue;
        InspectorRequest        request;
        vector<InspectorReply>  replies;



        request.kind    = InspectorRequestKind::Stamp;
        request.mediaId = source.mediaId;
        request.stamp   = std::make_shared<FormatStamp> (stamp);

        queue.Post (request);
        queue.Drain (source);
        queue.TakeReplies (replies);

        Assert::AreEqual ((size_t) 1, replies.size());

        return replies[0];
    }



    static void MakeD4Disk (DiskImage & outImage)
    {
        Byte  from[] = { kD5, kAA, k96 };
        Byte  to[]   = { kD4, kAA, k96 };



        StampDiskFixtures::MakeSixteenSector (outImage);

        for (int track = 0; track < StampDiskFixtures::kTracks; track++)
        {
            StampDiskFixtures::ReplaceProlog (outImage, track, from, to);
        }
    }



    static FormatStamp BuildStamp (const DiskImage & image)
    {
        FormatStamp  stamp;



        FormatStampBuilder::Build (*StampDiskFixtures::MakeCopy (image), DecodeSettings::MakeStandard(), StampSource::Inspector, nullptr, "2026-11-23T00:00:00Z", stamp);

        return stamp;
    }



    TEST_METHOD (StampRequest_PutsTheStampInTheDiskAndSavesIt)
    {
        FakeSource      source;
        FormatStamp     read;
        InspectorReply  reply;



        StampDiskFixtures::MakeSixteenSector (source.image);

        reply = PostStamp (source, BuildStamp (source.image));

        Assert::IsTrue   (reply.reason.empty());
        Assert::AreEqual (std::string ("fake.woz"), reply.savedPath);
        Assert::AreEqual (1, source.saves);
        Assert::IsTrue   (FormatStampChunk::GetStamp (source.image.GetWozMetadata(), read) == StampState::Readable);
    }



    TEST_METHOD (StampRequest_FailedSave_RestoresTheMetadata)
    {
        FakeSource      source;
        InspectorReply  reply;



        StampDiskFixtures::MakeSixteenSector (source.image);
        source.saveHr = E_ACCESSDENIED;

        reply = PostStamp (source, BuildStamp (source.image));

        Assert::IsFalse (reply.reason.empty());
        Assert::IsFalse (FormatStampChunk::HasStampChunk (source.image.GetWozMetadata()));
    }



    TEST_METHOD (StampRequest_WriteProtectedDisk_IsNotSaved)
    {
        FakeSource      source;
        InspectorReply  reply;



        StampDiskFixtures::MakeSixteenSector (source.image);
        source.image.SetUserWriteProtected (true);

        reply = PostStamp (source, BuildStamp (source.image));

        Assert::AreEqual (std::wstring (L"The disk is write-protected"), reply.reason);
        Assert::AreEqual (0, source.saves);
    }



    TEST_METHOD (ToDecodeSettings_D4Disk_IsOneRangeThatMatchesTheStandardToo)
    {
        DiskImage       image;
        DecodeSettings  settings;



        MakeD4Disk (image);
        StampedTrackMarks::ToDecodeSettings (BuildStamp (image), settings);

        Assert::AreEqual ((size_t) 1, settings.GetRanges().size());
        Assert::AreEqual (0, settings.GetRanges()[0].firstTrack);
        Assert::AreEqual (StampDiskFixtures::kTracks - 1, settings.GetRanges()[0].lastTrack);
        Assert::IsTrue   (settings.GetRanges()[0].isFromStamp);
        Assert::IsTrue   (settings.GetRanges()[0].matchStandardToo);
        Assert::AreEqual (std::string ("D4 AA 96"), settings.GetRanges()[0].customMarks.address16[0].ToText());
    }



    TEST_METHOD (ToDecodeSettings_StandardDisk_IsStandard)
    {
        DiskImage       image;
        DecodeSettings  settings;



        StampDiskFixtures::MakeSixteenSector (image);
        StampedTrackMarks::ToDecodeSettings (BuildStamp (image), settings);

        Assert::IsTrue (settings.IsStandard());
    }



    TEST_METHOD (SeededSettings_AnalyzeAStampedTrackAsItsEncoding)
    {
        DiskImage       image;
        DecodeSettings  settings;
        DiskAnalysis    standard;
        DiskAnalysis    seeded;



        MakeD4Disk (image);
        StampedTrackMarks::ToDecodeSettings (BuildStamp (image), settings);

        DiskAnalyzer::Analyze (StampDiskFixtures::MakeCopy (image), DecodeSettings::MakeStandard(), standard);
        DiskAnalyzer::Analyze (StampDiskFixtures::MakeCopy (image), settings, seeded);

        Assert::IsTrue (standard.entries[20].trackClass == TrackClass::Nonstandard, L"without the stamp's marks, D4 AA 96 is nonstandard");
        Assert::IsTrue (seeded.entries[20].trackClass == TrackClass::Sixteen);
    }



    TEST_METHOD (Build_IgnoresSettingsFromTheStamp)
    {
        DiskImage       image;
        DecodeSettings  settings;
        FormatStamp     stamp;



        MakeD4Disk (image);
        StampedTrackMarks::ToDecodeSettings (BuildStamp (image), settings);

        FormatStampBuilder::Build (*StampDiskFixtures::MakeCopy (image), settings, StampSource::Inspector, nullptr, "", stamp);

        Assert::IsTrue (stamp.decodeRanges.empty());
    }



    TEST_METHOD (KeepCaptureFacts_KeepsThemForTracksWithTheSameBits)
    {
        FormatStamp  earlier;
        FormatStamp  stamp;



        earlier.hasCapture    = true;
        earlier.capture.reads = 9;
        earlier.tracks.resize (2);
        earlier.tracks[0]     = { .position = 0, .hasBitsCrc = true, .bitsCrc = 1, .hasCapture = true };
        earlier.tracks[1]     = { .position = 4, .hasBitsCrc = true, .bitsCrc = 2, .hasCapture = true };
        earlier.tracks[0].capture.isWeak = true;
        earlier.tracks[1].capture.isWeak = true;

        stamp.tracks.resize (2);
        stamp.tracks[0]       = { .position = 0, .hasBitsCrc = true, .bitsCrc = 1 };
        stamp.tracks[1]       = { .position = 4, .hasBitsCrc = true, .bitsCrc = 3 };

        FormatStampBuilder::KeepCaptureFacts (earlier, stamp);

        Assert::IsTrue   (stamp.hasCapture);
        Assert::AreEqual (9, stamp.capture.reads);
        Assert::IsTrue   (stamp.tracks[0].capture.isWeak);
        Assert::IsFalse  (stamp.tracks[1].hasCapture, L"a track whose bits changed has no capture facts");
    }



    TEST_METHOD (FormatUtc_IsIso8601ToTheSecond)
    {
        std::chrono::sys_days  day  = std::chrono::year (2026) / 11 / 23;
        auto                   when = day + std::chrono::hours (21) + std::chrono::minutes (4) + std::chrono::milliseconds (5500);



        Assert::AreEqual (std::string ("2026-11-23T21:04:05Z"), FormatStampBuilder::FormatUtc (when));
    }



    TEST_METHOD (ImageDetails_HasTheStamp)
    {
        DiskImage     image;
        WozMetadata   meta;
        ImageDetails  details;



        StampDiskFixtures::MakeSixteenSector (image);
        meta = image.GetWozMetadata();
        FormatStampChunk::SetStamp (meta, BuildStamp (image));
        image.SetWozMetadata (meta);

        details = ImageDetails::MakeFromCopy (*StampDiskFixtures::MakeCopy (image));

        Assert::IsTrue   (details.stampState == StampState::Readable);
        Assert::AreEqual ((size_t) StampDiskFixtures::kTracks, details.stamp.tracks.size());
    }
};
