#include "Pch.h"

#include "../EhmTestHelper.h"
#include "../EmuTests/FakeDiskFileIo.h"
#include "CassoExplorer/ExplorerInspectorHost.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Machines/Apple2/Common/WozLoader.h"
#include "Seams/IIntentChannel.h"
#include "Seams/Win32IntentChannel.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerInspectorHostTests
//
//  Edits applied in Casso Explorer (FR-102, FR-105, FR-107): an apply
//  saved over the file and the reload intent sent to Casso, an undo saved the same
//  way, nothing saved over a file that changed since it was read or when
//  the save fails, the record put back after a failed save, a WOZ whose
//  flag is set not changed, and the note for Casso's reply. "As first
//  opened" (FR-117) is the file as the window first read it, after the file
//  is read again.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ExplorerInspectorHostTests)
{
public:

    static constexpr const wchar_t *  kPath     = L"C:\\Disks\\mist.dsk";
    static constexpr const char *     kUtf8Path = "C:\\Disks\\mist.dsk";



    class FakeIntents : public IIntentChannel
    {
    public:
        void  StateIntent (const std::string & imagePath, ExternalChangeIntent intent) override { stated.emplace_back (imagePath, intent); }

        vector<std::pair<std::string, ExternalChangeIntent>>  stated;
    };



    //  A host on a DSK of $11 bytes, its first copy taken.
    struct Rig
    {
        FakeDiskFileIo         fileIo;
        FakeIntents            intents;
        ExplorerInspectorHost  host     { kPath, fileIo, &intents };
        vector<Byte>           original = vector<Byte> (NibblizationLayer::kImageByteSize, 0x11);
        uint64_t               mediaId  = 0;
        int                    slot     = -1;

        Rig()
        {
            InspectorReply  reply;



            fileIo.files[kUtf8Path]  = original;
            fileIo.stamps[kUtf8Path] = FileStamp { original.size(), 1 };

            (void) host.PostInspectorRequest ({ InspectorRequestKind::CopyDisk, 0, 0, {}, -1 });
            reply   = TakeOne();
            mediaId = reply.mediaId;
            Assert::IsNotNull (reply.disk.get());
            slot    = reply.disk->playedSlot[3 * 4];
        }

        InspectorReply TakeOne()
        {
            vector<InspectorReply>  replies;



            host.TakeInspectorReplies (replies);
            Assert::AreEqual (static_cast<size_t> (1), replies.size());

            return replies[0];
        }

        //  Sector 5 of track 3, all $A5.
        InspectorReply Apply()
        {
            InspectorRequest  request;
            SectorWrite       write;



            write.track  = 3;
            write.slot   = slot;
            write.sector = 5;
            write.bytes.fill (0xA5);

            request.kind           = InspectorRequestKind::Apply;
            request.mediaId        = mediaId;
            request.applyKind      = ApplyKind::Apply;
            request.writes         = { write };
            request.expectedCounts = { { write.slot, 0 } };

            (void) host.PostInspectorRequest (request);

            return TakeOne();
        }

        //  How many bytes of the file differ from the original, and whether
        //  each one that does is $A5.
        size_t CountChanged (bool & outAreAllA5) const
        {
            const vector<Byte>  & now     = fileIo.files.at (kUtf8Path);
            size_t                changed = 0;



            outAreAllA5 = now.size() == original.size();

            for (size_t i = 0; i < now.size() && i < original.size(); i++)
            {
                changed     += (now[i] != original[i]) ? 1 : 0;
                outAreAllA5  = outAreAllA5 && (now[i] == original[i] || now[i] == 0xA5);
            }

            return changed;
        }
    };



    TEST_METHOD (AnApplyIsSavedOverTheFileAndCassoIsTold)
    {
        Rig             rig;
        InspectorReply  reply = rig.Apply();
        bool            isA5  = false;



        Assert::IsTrue   (reply.applyOutcome == ApplyOutcome::Applied, reply.reason.c_str());
        Assert::AreEqual (static_cast<size_t> (256), rig.CountChanged (isA5), L"only the sector's bytes");
        Assert::IsTrue   (isA5);
        Assert::AreEqual (1, rig.fileIo.replaceCount, L"the file is replaced whole");
        Assert::IsTrue   (rig.fileIo.HasNoTemporaryFiles());
        Assert::AreEqual (static_cast<size_t> (1), rig.intents.stated.size());
        Assert::AreEqual (std::string (kUtf8Path), rig.intents.stated[0].first);
        Assert::IsTrue   (rig.intents.stated[0].second == ExternalChangeIntent::ReloadInPlace);
        Assert::IsTrue   (rig.host.IsFileCurrent(), L"the window's save is not a change to read again");
    }



    TEST_METHOD (AnUndoIsSavedAsAnApplyIs)
    {
        Rig               rig;
        InspectorReply    reply = rig.Apply();
        InspectorRequest  undo;



        Assert::IsTrue (reply.applyOutcome == ApplyOutcome::Applied, reply.reason.c_str());

        undo.kind      = InspectorRequestKind::Apply;
        undo.mediaId   = rig.mediaId;
        undo.applyKind = ApplyKind::Undo;
        undo.restore   = reply.before;
        (void) rig.host.PostInspectorRequest (undo);
        reply = rig.TakeOne();

        Assert::IsTrue (reply.applyOutcome == ApplyOutcome::Applied, reply.reason.c_str());
        Assert::IsTrue (rig.fileIo.files.at (kUtf8Path) == rig.original, L"the file is as it was before the apply");
        Assert::AreEqual (static_cast<size_t> (2), rig.intents.stated.size());
    }



    TEST_METHOD (AFileChangedSinceItWasReadIsNotWritten)
    {
        Rig             rig;
        InspectorReply  reply;
        bool            isA5 = false;



        rig.fileIo.mutateStampOnNextStat = true;
        reply = rig.Apply();

        Assert::IsTrue   (reply.applyOutcome == ApplyOutcome::NotApplied);
        Assert::IsTrue   (reply.reason.starts_with (L"Not applied: the file could not be saved"), reply.reason.c_str());
        Assert::IsTrue   (reply.reason.find (L"changed since it was read") != std::wstring::npos, reply.reason.c_str());
        Assert::AreEqual (static_cast<size_t> (0), rig.CountChanged (isA5));
        Assert::AreEqual (0, rig.fileIo.writeCount);
        Assert::IsTrue   (rig.intents.stated.empty(), L"no intent is sent");
    }



    TEST_METHOD (AFailedSaveLeavesTheFileAndPutsTheRecordBack)
    {
        Rig             rig;
        InspectorReply  reply;
        InspectorReply  tracks;
        bool            isA5 = false;



        rig.fileIo.failNextReplace  = true;
        rig.fileIo.nextReplaceError = HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED);
        reply = rig.Apply();

        Assert::IsTrue   (reply.applyOutcome == ApplyOutcome::NotApplied);
        Assert::IsTrue   (reply.reason.find (L"write-protected") != std::wstring::npos, reply.reason.c_str());
        Assert::AreEqual (static_cast<size_t> (0), rig.CountChanged (isA5));
        Assert::IsTrue   (rig.fileIo.HasNoTemporaryFiles());
        Assert::IsTrue   (rig.intents.stated.empty());

        (void) rig.host.PostInspectorRequest ({ InspectorRequestKind::CopyTracks, 0, rig.mediaId, { rig.slot }, -1 });
        tracks = rig.TakeOne();
        Assert::IsTrue (tracks.tracks[0]->bits == reply.before[0]->bits, L"the image has the record as it was");
    }



    TEST_METHOD (AWozWithItsFlagSetIsNotChanged)
    {
        static constexpr size_t  kBitCount = 51200;

        FakeDiskFileIo         fileIo;
        FakeIntents            intents;
        std::string            path      = "C:\\Disks\\locked.woz";
        ExplorerInspectorHost  host (L"C:\\Disks\\locked.woz", fileIo, &intents);
        vector<Byte>           image;
        WozSyntheticTrack      track;
        size_t                 info      = 0;
        vector<InspectorReply> replies;
        InspectorRequest       request;
        SectorWrite            write;



        track.data.assign ((kBitCount + 7) / 8, 0xFF);
        track.bitCount      = kBitCount;
        track.quarterTracks = { 0 };
        AssertSucceeded (WozLoader::BuildSyntheticV21 ({ track }, image));

        info = WozLoader::kHeaderSize;

        while (info + 4 <= image.size() && memcmp (image.data() + info, "INFO", 4) != 0)
        {
            info++;
        }

        image[info + 8 + WozLoader::kInfoOffsetWriteProtected] = 1;
        std::fill (image.begin() + 8, image.begin() + 12, Byte (0));

        fileIo.files[path]  = image;
        fileIo.stamps[path] = FileStamp { image.size(), 1 };

        (void) host.PostInspectorRequest ({ InspectorRequestKind::CopyDisk, 0, 0, {}, -1 });
        host.TakeInspectorReplies (replies);
        Assert::IsNotNull (replies[0].disk.get());
        Assert::IsTrue    (replies[0].disk->writeProtect.imageFlag, L"the cause listed beside Apply");

        write.track            = 0;
        write.slot             = 0;
        write.sector           = 0;
        request.kind           = InspectorRequestKind::Apply;
        request.mediaId        = replies[0].mediaId;
        request.writes         = { write };
        (void) host.PostInspectorRequest (request);
        host.TakeInspectorReplies (replies);

        Assert::IsTrue   (replies[0].applyOutcome == ApplyOutcome::NotApplied);
        Assert::AreEqual (std::wstring (L"The disk is write-protected"), replies[0].reason);
        Assert::IsTrue   (fileIo.files.at (path) == image);
        Assert::AreEqual (0, fileIo.writeCount);
    }



    TEST_METHOD (AsFirstOpenedIsTheFileAsFirstRead)
    {
        Rig             rig;
        vector<Byte>    changed = vector<Byte> (NibblizationLayer::kImageByteSize, 0x22);
        InspectorReply  reply;



        rig.fileIo.files[kUtf8Path]  = changed;
        rig.fileIo.stamps[kUtf8Path] = FileStamp { changed.size(), 2 };
        rig.host.Reload();

        (void) rig.host.PostInspectorRequest ({ InspectorRequestKind::CopyAsInserted, 0, 0, {}, -1 });
        reply = rig.TakeOne();

        Assert::IsNotNull (reply.sourceBytes.get());
        Assert::IsTrue    (*reply.sourceBytes == rig.original, L"the bytes of the first read");

        (void) rig.host.PostInspectorRequest ({ InspectorRequestKind::CopyDisk, 0, 0, {}, -1 });
        reply = rig.TakeOne();
        Assert::IsNotNull (reply.disk.get());
        Assert::IsTrue    (rig.host.GetDriveCount() == 0, L"a file host has no drives");
    }



    TEST_METHOD (OnlyAReplyOtherThanAReloadIsANote)
    {
        Win32IntentChannel::Reply  reply;
        vector<Byte>               bytes;



        reply.kind = Win32IntentChannel::ReplyKind::ReloadDone;
        bytes      = Win32IntentChannel::EncodeReply (reply);
        Assert::IsTrue (ExplorerInspectorHost::DescribeCassoReply (bytes.data(), bytes.size()).empty());

        reply.kind = Win32IntentChannel::ReplyKind::ReloadConflict;
        reply.text = "The disk in drive 1 has unsaved changes.";
        bytes      = Win32IntentChannel::EncodeReply (reply);
        Assert::AreEqual (std::wstring (L"Casso did not reload the changed disk. The disk in drive 1 has unsaved changes."),
                          ExplorerInspectorHost::DescribeCassoReply (bytes.data(), bytes.size()));
    }
};
