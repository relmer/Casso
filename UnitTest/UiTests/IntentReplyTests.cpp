#include "Pch.h"
#include "../EhmTestHelper.h"
#include "Seams/IntentReplyTracker.h"
#include "Seams/Win32IntentChannel.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  IntentReplyTests
//
//  The two requests a waiting tool can make, the six answers the emulator can
//  give, and the bookkeeping that holds a request until its answer is known.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (IntentReplyTests)
{
public:

    using Payload   = Win32IntentChannel::Payload;
    using Reply     = Win32IntentChannel::Reply;
    using ReplyKind = Win32IntentChannel::ReplyKind;

    static constexpr const char *  kImagePath = "C:\\work\\Loader.dsk";

    static const HWND  kToolA;
    static const HWND  kToolB;



    //
    //  ------------------------------------------------------------------
    //  The two requests.
    //  ------------------------------------------------------------------
    //

    TEST_METHOD (Insert_RoundTripsWithItsDrive)
    {
        const int  drives[] = { 1, 2 };

        for (int drive : drives)
        {
            std::vector<Byte>  bytes = Win32IntentChannel::EncodeInsert (kImagePath, drive);
            Payload            payload;

            Assert::IsTrue   (Win32IntentChannel::Decode (bytes.data(), bytes.size(), payload));
            Assert::IsTrue   (payload.intent == ExternalChangeIntent::InsertDisk);
            Assert::AreEqual (drive, payload.drive);
            Assert::AreEqual (std::string (kImagePath), payload.imagePath);
        }
    }



    TEST_METHOD (Insert_RefusesADriveItCannotHaveAndAMissingPath)
    {
        std::vector<Byte>  bytes;
        Payload            payload;

        bytes = Win32IntentChannel::EncodeInsert (kImagePath, 3);
        Assert::IsFalse (Win32IntentChannel::Decode (bytes.data(), bytes.size(), payload));

        bytes = Win32IntentChannel::EncodeInsert (kImagePath, 0);
        Assert::IsFalse (Win32IntentChannel::Decode (bytes.data(), bytes.size(), payload));

        bytes = Win32IntentChannel::EncodeInsert ("", 1);
        Assert::IsFalse (Win32IntentChannel::Decode (bytes.data(), bytes.size(), payload));

        //  The intent and nothing else is not an insert of anything.
        bytes.resize (1);
        Assert::IsFalse (Win32IntentChannel::Decode (bytes.data(), bytes.size(), payload));
        Assert::IsTrue  (payload.imagePath.empty());
        Assert::AreEqual (0, payload.drive);
    }



    TEST_METHOD (Describe_IsOneByteAndNothingMore)
    {
        std::vector<Byte>  bytes = Win32IntentChannel::EncodeDescribe();
        Payload            payload;

        Assert::AreEqual ((size_t) 1, bytes.size());
        Assert::IsTrue   (Win32IntentChannel::Decode (bytes.data(), bytes.size(), payload));
        Assert::IsTrue   (payload.intent == ExternalChangeIntent::DescribeMachine);

        bytes.push_back ('x');
        Assert::IsFalse (Win32IntentChannel::Decode (bytes.data(), bytes.size(), payload));
        Assert::IsTrue  (payload.intent == ExternalChangeIntent::Unstated);
    }



    TEST_METHOD (TheOlderIntents_StillNeedAPath)
    {
        std::vector<Byte>  bytes = { (Byte) ExternalChangeIntent::ReloadInPlace };
        Payload            payload;

        Assert::IsFalse (Win32IntentChannel::Decode (bytes.data(), bytes.size(), payload));
    }



    //
    //  ------------------------------------------------------------------
    //  The six answers.
    //  ------------------------------------------------------------------
    //

    TEST_METHOD (EveryReply_RoundTrips)
    {
        const Reply  replies[] =
        {
            { ReplyKind::MachineDescription, 2, "Apple //e" },
            { ReplyKind::InsertDone,         0, ""          },
            { ReplyKind::InsertRefused,      0, "the drive is writing" },
            { ReplyKind::ReloadDone,         0, ""          },
            { ReplyKind::ReloadConflict,     0, "kept in C:\\work\\Loader (Casso).dsk" },
            { ReplyKind::ReloadRefused,      0, "the file is gone" },
        };

        for (const Reply & reply : replies)
        {
            std::vector<Byte>  bytes = Win32IntentChannel::EncodeReply (reply);
            Reply              back;

            Assert::IsTrue   (Win32IntentChannel::DecodeReply (bytes.data(), bytes.size(), back));
            Assert::IsTrue   (back.kind == reply.kind);
            Assert::AreEqual (reply.driveCount, back.driveCount);
            Assert::AreEqual (reply.text, back.text);
        }
    }



    TEST_METHOD (Replies_RefuseWhatThisBuildDidNotSend)
    {
        std::vector<Byte>  unknown     = { 0x7F, 'x' };
        std::vector<Byte>  noCount     = { (Byte) ReplyKind::MachineDescription };
        std::vector<Byte>  tooMany     = { (Byte) ReplyKind::MachineDescription, 9, 'A' };
        std::vector<Byte>  oversized (Win32IntentChannel::kMaxPayloadBytes + 1, 'x');
        Reply              back;

        oversized[0] = (Byte) ReplyKind::InsertRefused;

        Assert::IsFalse (Win32IntentChannel::DecodeReply (nullptr, 0, back));
        Assert::IsFalse (Win32IntentChannel::DecodeReply (unknown.data(), unknown.size(), back));
        Assert::IsFalse (Win32IntentChannel::DecodeReply (noCount.data(), noCount.size(), back));
        Assert::IsFalse (Win32IntentChannel::DecodeReply (tooMany.data(), tooMany.size(), back));
        Assert::IsFalse (Win32IntentChannel::DecodeReply (oversized.data(), oversized.size(), back));
        Assert::IsTrue  (back.text.empty());
    }



    TEST_METHOD (TheTwoDirections_UseDifferentMessageIds)
    {
        Assert::AreNotEqual ((uint64_t) Win32IntentChannel::GetMessageId(), (uint64_t) Win32IntentChannel::GetReplyMessageId());
        Assert::AreNotEqual ((uint64_t) 0, (uint64_t) Win32IntentChannel::GetReplyMessageId());
    }



    //
    //  ------------------------------------------------------------------
    //  Holding a request until its answer is known.
    //  ------------------------------------------------------------------
    //

    TEST_METHOD (Tracker_AnInsertIsAnsweredOnceForItsDriveAndPath)
    {
        IntentReplyTracker  tracker;
        HWND                replyTo = nullptr;

        tracker.NoteInsert (0, kImagePath, kToolA);

        Assert::IsFalse (tracker.TryTakeInsert (1, kImagePath, replyTo), L"another drive");
        Assert::IsFalse (tracker.TryTakeInsert (0, "C:\\work\\Other.dsk", replyTo), L"another image");
        Assert::IsTrue  (tracker.TryTakeInsert (0, "c:/WORK/loader.dsk", replyTo), L"the same image spelled differently");
        Assert::IsTrue  (replyTo == kToolA);
        Assert::IsFalse (tracker.TryTakeInsert (0, kImagePath, replyTo), L"answered once");
    }



    TEST_METHOD (Tracker_ADropHasNobodyToAnswerButIsStillAHandOff)
    {
        IntentReplyTracker  tracker;
        HWND                replyTo = kToolB;

        tracker.NoteInsert (1, kImagePath, nullptr);

        Assert::IsTrue (tracker.TryTakeInsert (1, kImagePath, replyTo));
        Assert::IsNull (replyTo);
    }



    TEST_METHOD (Tracker_ANewerRequestForADriveReplacesTheOlder)
    {
        IntentReplyTracker  tracker;
        HWND                replyTo = nullptr;

        tracker.NoteInsert (0, kImagePath, kToolA);
        tracker.NoteInsert (0, "C:\\work\\Second.dsk", kToolB);

        Assert::IsFalse (tracker.TryTakeInsert (0, kImagePath, replyTo));
        Assert::IsTrue  (tracker.TryTakeInsert (0, "C:\\work\\Second.dsk", replyTo));
        Assert::IsTrue  (replyTo == kToolB);
    }



    TEST_METHOD (Tracker_AReloadIsAnsweredOnce)
    {
        IntentReplyTracker  tracker;
        HWND                replyTo = nullptr;

        tracker.NoteReload (kImagePath, kToolA);

        Assert::IsTrue  (tracker.TryTakeReload (kImagePath, replyTo));
        Assert::IsTrue  (replyTo == kToolA);
        Assert::IsFalse (tracker.TryTakeReload (kImagePath, replyTo));
    }



    TEST_METHOD (Answers_ForAMountAndForEveryReloadDecision)
    {
        Reply  reply;

        reply = IntentReplyTracker::MakeInsertReply (S_OK, "ignored");
        Assert::IsTrue (reply.kind == ReplyKind::InsertDone);
        Assert::IsTrue (reply.text.empty());

        reply = IntentReplyTracker::MakeInsertReply (E_FAIL, "is not a disk image");
        Assert::IsTrue   (reply.kind == ReplyKind::InsertRefused);
        Assert::AreEqual (std::string ("is not a disk image"), reply.text);

        Assert::IsTrue (IntentReplyTracker::TryMakeReloadReply (ChangeAction::ReloadInPlace, false, "", reply));
        Assert::IsTrue (reply.kind == ReplyKind::ReloadDone);

        Assert::IsTrue (IntentReplyTracker::TryMakeReloadReply (ChangeAction::Restart, true, "C:\\kept.dsk", reply));
        Assert::IsTrue (reply.kind == ReplyKind::ReloadConflict);
        Assert::IsTrue (reply.text.find ("C:\\kept.dsk") != std::string::npos, L"the conflict says where the guest's copy went");

        Assert::IsTrue (IntentReplyTracker::TryMakeReloadReply (ChangeAction::Unusable, false, "", reply));
        Assert::IsTrue (reply.kind == ReplyKind::ReloadRefused);

        Assert::IsTrue (IntentReplyTracker::TryMakeReloadReply (ChangeAction::Deleted, false, "", reply));
        Assert::IsTrue (reply.kind == ReplyKind::ReloadRefused);

        Assert::IsTrue (IntentReplyTracker::TryMakeReloadReply (ChangeAction::Conflict, false, "", reply));
        Assert::IsTrue (reply.kind == ReplyKind::ReloadRefused, L"a guest copy that could not be saved leaves the disk as it was");

        Assert::IsFalse (IntentReplyTracker::TryMakeReloadReply (ChangeAction::Ask, false, "", reply), L"a question is no answer yet");
    }
};



const HWND  IntentReplyTests::kToolA = reinterpret_cast<HWND> (0x4004);
const HWND  IntentReplyTests::kToolB = reinterpret_cast<HWND> (0x5005);
