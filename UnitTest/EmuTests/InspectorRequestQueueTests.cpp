#include "Pch.h"

#include "../EhmTestHelper.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Shell/InspectorRequestQueue.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorRequestQueueTests
//
//  Copy requests from the inspector window to the emulation thread
//  (contracts/emulation-thread.md): answered in order at Drain, a reply for
//  each, DiskChanged when the drive holds another disk, and one wake-up per
//  Drain that answered something.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (InspectorRequestQueueTests)
{
public:

    class FakeSource : public IInspectorDiskSource
    {
    public:
        DiskImage  image;
        uint64_t   mediaId  = 42;
        bool       hasDisk  = true;

        const DiskImage * GetImage (int drive, InspectorDiskIdentity & outIdentity) override
        {
            outIdentity.mediaId  = mediaId;
            outIdentity.fileName = "fake.dsk";

            return (hasDisk && drive == 0) ? &image : nullptr;
        }
    };



    static void MakeSource (FakeSource & source)
    {
        vector<Byte>  sectors (NibblizationLayer::kImageByteSize, 0x11);



        AssertSucceeded (NibblizationLayer::NibblizeDsk (sectors, source.image));
    }



    TEST_METHOD (EachRequestGetsItsReplyAtDrain)
    {
        FakeSource              source;
        InspectorRequestQueue   queue;
        vector<InspectorReply>  replies;
        int                     notified = 0;
        uint64_t                first    = 0;
        uint64_t                second   = 0;



        MakeSource (source);
        queue.SetNotify ([&notified] () { notified++; });

        first  = queue.Post ({ InspectorRequestKind::CopyDisk, 0, 0, {}, -1 });
        second = queue.Post ({ InspectorRequestKind::CopyTracks, 0, 42, { 3, 4 }, -1 });

        queue.TakeReplies (replies);
        Assert::IsTrue (replies.empty(), L"nothing is answered before Drain");
        Assert::IsTrue (queue.HasRequests());

        queue.Drain (source);
        queue.TakeReplies (replies);

        Assert::AreEqual (1, notified);
        Assert::AreEqual (static_cast<size_t> (2), replies.size());
        Assert::AreEqual (first, replies[0].requestId);
        Assert::IsNotNull (replies[0].disk.get());
        Assert::AreEqual (42ull, replies[0].disk->mediaId);
        Assert::AreEqual (second, replies[1].requestId);
        Assert::AreEqual (static_cast<size_t> (2), replies[1].tracks.size());
        Assert::AreEqual (3, replies[1].tracks[0]->slot);
        Assert::IsFalse (queue.HasRequests());
    }



    TEST_METHOD (AnotherDiskOrNoDiskDoesNothing)
    {
        FakeSource              source;
        InspectorRequestQueue   queue;
        vector<InspectorReply>  replies;



        MakeSource (source);

        queue.Post ({ InspectorRequestKind::CopyDisk, 0, 41, {}, -1 });
        queue.Post ({ InspectorRequestKind::CopyDisk, 1, 0, {}, -1 });
        queue.Drain (source);
        queue.TakeReplies (replies);

        Assert::IsTrue (replies[0].status == InspectorReplyStatus::DiskChanged);
        Assert::IsNull (replies[0].disk.get());
        Assert::IsTrue (replies[1].status == InspectorReplyStatus::NoDisk);
    }



    TEST_METHOD (ExportCopiesTheRecordAQuarterTrackPlays)
    {
        FakeSource              source;
        InspectorRequestQueue   queue;
        vector<InspectorReply>  replies;



        MakeSource (source);

        queue.Post ({ InspectorRequestKind::Export, 0, 42, {}, 4 * 7 });
        queue.Drain (source);
        queue.TakeReplies (replies);

        Assert::AreEqual (static_cast<size_t> (1), replies[0].tracks.size());
        Assert::AreEqual (source.image.ResolveQuarterTrack (4 * 7), replies[0].tracks[0]->slot);
    }
};
