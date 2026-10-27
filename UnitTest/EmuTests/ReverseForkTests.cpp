#include "Pch.h"

#include "HResultAssert.h"
#include "ReverseSessionRig.h"
#include "TestMachine.h"
#include "Debugger/Reverse/ReverseHost.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace ReverseForkTests
{
    static constexpr uint64_t  kRecordCycles = 400'000;
    static constexpr uint64_t  kBranchCycles = 120'000;
    static constexpr uint64_t  kBack         = 2'000;





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  NeverStops
    //
    //  A stop test that never fires, so a reverse continue replays every
    //  stretch of the active path to its start.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class NeverStops : public IReverseStopTest
    {
    public:
        bool          ShouldStopBefore (MachineHost &, Word) override { return false; }
        bool          TakePendingStop  () override                    { return false; }
        IWatchSink *  GetWatchSink     () override                    { return nullptr; }
    };




    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ReverseForkTests
    //
    //  A change behind live moves the recorded future into a branch, and a
    //  switch to a branch lands at its live end with the machine exactly as
    //  it was there; moves through history stay on the active path.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ReverseForkTests)
    {
    public:

        //  The machine run cycles on, with keys and paddle moves fed to it a
        //  slice at a time.
        static void Run (TestMachine & machine, uint64_t cycles)
        {
            constexpr uint64_t  kSlice = 1000;



            for (uint64_t slice = 0; slice < cycles / kSlice; slice++)
            {
                ReverseSessionRig::Feed (machine, static_cast<size_t> (slice));
                machine.SampleHostInputs();
                machine.RunCycles (kSlice);
            }
        }


        static void Record (TestMachine & machine, ReverseHost & host)
        {
            HRESULT  hr = S_OK;



            ReverseSessionRig::Prepare (machine);

            hr = host.StartRecording (ReverseSessionRig::MakeSettings (1));
            AssertSucceeded (hr, L"StartRecording");

            Run (machine, kRecordCycles);
        }


        static void Execute (ReverseHost & host, ReverseCommand command, uint64_t argument = 0)
        {
            ReverseResult  result;
            HRESULT        hr = host.Execute (command, argument, nullptr, result);



            AssertSucceeded (hr, L"Execute");
        }


        TEST_METHOD (AForkThenASwitchBackLandsExactlyAtTheLiveEnd)
        {
            TestMachine                  machine ("Apple2e");
            ReverseHost                  host    (machine);
            std::vector<KeyframeBranch>  branches;
            uint64_t                     liveEnd  = 0;
            uint64_t                     checksum = 0;
            uint64_t                     forkAt   = 0;
            HRESULT                      hr       = S_OK;



            Record (machine, host);

            liveEnd  = machine.GetPosition();
            checksum = ReverseSessionRig::Checksum (machine);
            forkAt   = liveEnd - kBack;

            Execute (host, ReverseCommand::Seek, forkAt);

            hr = host.Fork();
            AssertSucceeded (hr, L"Fork");

            Run (machine, kBranchCycles);

            host.GetController().GetKeyframes().GetBranches (branches);

            Assert::AreEqual<size_t>   (1, branches.size(), L"the future left behind is a branch");
            Assert::AreEqual<uint64_t> (forkAt, branches[0].forkPosition, L"forked where the change was made");
            Assert::AreEqual<uint64_t> (liveEnd, branches[0].liveEndPosition, L"ending at the old live end");

            Execute (host, ReverseCommand::SwitchBranch, branches[0].id);

            Assert::IsFalse            (host.IsBehindLive(), L"live at the branch's end");
            Assert::AreEqual<uint64_t> (liveEnd, machine.GetPosition(), L"at the old live end");
            Assert::AreEqual<uint64_t> (checksum, ReverseSessionRig::Checksum (machine), L"with the whole machine as it was there");

            host.GetController().GetKeyframes().GetBranches (branches);

            Assert::AreEqual<size_t>   (1, branches.size(), L"the path left is now the branch");
            Assert::AreEqual<uint64_t> (forkAt, branches[0].forkPosition, L"forked at the same position");
        }


        TEST_METHOD (ASwitchedBranchReplaysLikeAnyHistory)
        {
            TestMachine                  machine ("Apple2e");
            ReverseHost                  host    (machine);
            std::vector<KeyframeBranch>  branches;
            uint64_t                     liveEnd  = 0;
            uint64_t                     target   = 0;
            uint64_t                     checksum = 0;
            HRESULT                      hr       = S_OK;



            Record (machine, host);

            liveEnd = machine.GetPosition();
            target  = liveEnd - kBack / 2;

            Execute (host, ReverseCommand::Seek, target);
            checksum = ReverseSessionRig::Checksum (machine);

            Execute (host, ReverseCommand::Seek, liveEnd - kBack);

            hr = host.Fork();
            AssertSucceeded (hr, L"Fork");

            Run (machine, kBranchCycles);

            host.GetController().GetKeyframes().GetBranches (branches);
            Execute (host, ReverseCommand::SwitchBranch, branches[0].id);

            Execute (host, ReverseCommand::Seek, target);

            Assert::AreEqual<uint64_t> (target, machine.GetPosition(), L"a seek into the branch lands");
            Assert::AreEqual<uint64_t> (checksum, ReverseSessionRig::Checksum (machine), L"on the machine as it was there, replayed from before the fork");

            Execute (host, ReverseCommand::GoLive);

            Assert::AreEqual<uint64_t> (liveEnd, machine.GetPosition(), L"and going live returns to the branch's live end");
        }


        //  A fork at the oldest keyframe, where the journal's entries before
        //  it were dropped: a switch back lands exactly at the live end, and
        //  a switch to the path left lands at its end.
        TEST_METHOD (AForkAtTheOldestKeyframeSwitchesBothWays)
        {
            TestMachine                  machine ("Apple2e");
            ReverseHost                  host    (machine);
            std::vector<KeyframeBranch>  branches;
            uint64_t                     liveEnd  = 0;
            uint64_t                     checksum = 0;
            uint64_t                     newEnd   = 0;
            HRESULT                      hr       = S_OK;



            Record (machine, host);

            liveEnd  = machine.GetPosition();
            checksum = ReverseSessionRig::Checksum (machine);

            Execute (host, ReverseCommand::Seek, host.GetController().GetOldestPosition());

            hr = host.Fork();
            AssertSucceeded (hr, L"Fork");

            Run (machine, kBranchCycles);
            newEnd = machine.GetPosition();

            host.GetController().GetKeyframes().GetBranches (branches);
            Execute (host, ReverseCommand::SwitchBranch, branches[0].id);

            Assert::AreEqual<uint64_t> (liveEnd,  machine.GetPosition(),                L"at the old live end");
            Assert::AreEqual<uint64_t> (checksum, ReverseSessionRig::Checksum (machine), L"with the whole machine as it was there");

            host.GetController().GetKeyframes().GetBranches (branches);
            Execute (host, ReverseCommand::SwitchBranch, branches[0].id);

            Assert::AreEqual<uint64_t> (newEnd, machine.GetPosition(), L"back at the end of the path left");
        }


        TEST_METHOD (AReverseContinueAfterASwitchCrossesTheFork)
        {
            TestMachine                  machine ("Apple2e");
            ReverseHost                  host    (machine);
            std::vector<KeyframeBranch>  branches;
            NeverStops                   neverStops;
            ReverseResult                result;
            HRESULT                      hr = S_OK;



            Record (machine, host);

            Execute (host, ReverseCommand::Seek, machine.GetPosition() - kBack);

            hr = host.Fork();
            AssertSucceeded (hr, L"Fork");

            Run (machine, kBranchCycles);

            host.GetController().GetKeyframes().GetBranches (branches);
            Execute (host, ReverseCommand::SwitchBranch, branches[0].id);

            hr = host.Execute (ReverseCommand::ReverseContinue, 0, &neverStops, result);
            AssertSucceeded (hr, L"ReverseContinue over the whole active path");

            Assert::IsTrue             (result.outcome == ReverseOutcome::AtHistoryStart, L"it reached the start, every keyframe matched");
            Assert::AreEqual<uint64_t> (host.GetController().GetOldestPosition(), machine.GetPosition(), L"at the oldest position");
        }

        TEST_METHOD (MovesStayOnTheActivePath)
        {
            TestMachine                  machine ("Apple2e");
            ReverseHost                  host    (machine);
            uint64_t                     forkAt   = 0;
            uint64_t                     newEnd   = 0;
            HRESULT                      hr       = S_OK;



            Record (machine, host);

            forkAt = machine.GetPosition() - kBack;

            Execute (host, ReverseCommand::Seek, forkAt);

            hr = host.Fork();
            AssertSucceeded (hr, L"Fork");

            Run (machine, kBranchCycles / 4);
            newEnd = machine.GetPosition();

            Execute (host, ReverseCommand::Seek, forkAt + 10);
            Execute (host, ReverseCommand::GoLive);

            Assert::AreEqual<uint64_t> (newEnd, machine.GetPosition(), L"going live lands at the active path's end, not the branch's");
        }
    };
}
