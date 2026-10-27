#include "Pch.h"

#include "EmuTests/ShellHistoryRig.h"
#include "Shell/MachineStateFile.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace StateLoadHistoryTests
{
    using ShellHistoryTests::ShellRig;
    using ShellHistoryTests::kBudgetMb;
    using ShellHistoryTests::kFrames;





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  StateLoadHistoryTests
    //
    //  A state file and the history recording: a file with history replaces
    //  it, a file without one is a marked point in it, forking it behind
    //  live (FR-025), and with history off a save has no history and a load
    //  takes none up.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (StateLoadHistoryTests)
    {
    public:

        static ReverseController & GetController (ShellRig & rig)
        {
            return rig.shell->GetHistory().GetReverseHost()->GetController();
        }


        //  The machine as a state file, with its history when withHistory.
        static std::vector<Byte> Save (ShellRig & rig, bool withHistory)
        {
            std::vector<Byte>  history;
            std::vector<Byte>  bytes;
            uint64_t           resumeAt = 0;
            MachineStateError  error;
            HRESULT            hr       = S_OK;



            if (withHistory)
            {
                hr = rig.shell->GetHistory().BuildSavedHistory (history, resumeAt);
                AssertSucceeded (hr, L"BuildSavedHistory");
            }

            hr = MachineStateFile::Build (rig.shell->GetMachine(), std::vector<Byte>(), history, bytes, error);
            AssertSucceeded (hr, L"MachineStateFile::Build");

            rig.shell->GetHistory().ReturnFromSave (resumeAt);

            return bytes;
        }


        static void Load (ShellRig & rig, const std::vector<Byte> & bytes)
        {
            MachineStateError  error;
            HRESULT            hr = rig.shell->LoadMachineStateBytes (bytes, error);



            AssertSucceeded (hr, L"LoadMachineStateBytes");
        }


        //  Whether the journal has a StateLoad record at position.
        static bool HasStateLoadAt (ShellRig & rig, uint64_t position)
        {
            const InputJournal  & journal = rig.shell->GetMachine().GetInputJournal();
            bool                  isFound = false;



            for (size_t i = journal.GetBeginIndex(); i < journal.GetEndIndex() && !isFound; i++)
            {
                isFound = journal.GetRecord (i).kind == InputKind::StateLoad && journal.GetRecord (i).position == position;
            }

            return isFound;
        }


        //  A file saved with history, loaded into another shell that has
        //  history: the shell's history is replaced by the file's.
        TEST_METHOD (AStateWithHistoryReplacesTheHistory)
        {
            ShellRig           source (false);
            ShellRig           target (false);
            std::vector<Byte>  bytes;



            source.RunFrames (kFrames);
            bytes = Save (source, true);

            target.RunFrames (kFrames / 3);
            Load (target, bytes);

            Assert::IsTrue             (target.shell->GetHistory().IsRecording(),                                         L"recording");
            Assert::AreEqual<uint64_t> (source.shell->GetMachine().GetPosition(), target.shell->GetMachine().GetPosition(), L"at the file's live end");
            Assert::AreEqual<uint64_t> (GetController (source).GetOldestPosition(), GetController (target).GetOldestPosition(), L"with the file's history");
            Assert::IsTrue             (GetController (target).GetKeyframes().GetCount() >= GetController (source).GetKeyframes().GetCount(), L"every keyframe of it");
        }


        //  A file whose history section is damaged loads with no error, and
        //  history starts afresh at the load.
        TEST_METHOD (AStateWithDamagedHistoryLoadsWithFreshHistory)
        {
            ShellRig           source (false);
            ShellRig           target (false);
            std::vector<Byte>  bytes;



            source.RunFrames (kFrames);
            bytes = Save (source, true);
            bytes.back() ^= 1;

            target.RunFrames (kFrames / 3);
            Load (target, bytes);

            Assert::IsTrue             (target.shell->GetHistory().IsRecording(),                                             L"recording");
            Assert::AreEqual<uint64_t> (target.shell->GetMachine().GetPosition(), GetController (target).GetOldestPosition(), L"from the load");
        }


        //  A file saved behind live resumes where it was saved, in history.
        TEST_METHOD (AStateSavedBehindLiveResumesWhereItWasSaved)
        {
            ShellRig           source (false);
            ShellRig           target (false);
            std::vector<Byte>  bytes;
            uint64_t           live   = 0;
            uint64_t           shown  = 0;



            source.RunFrames (kFrames);
            live = source.shell->GetMachine().GetPosition();

            source.Reverse (ReverseCommand::Seek, live - ShellHistoryTests::kFrameCycles);
            shown = source.shell->GetMachine().GetPosition();

            bytes = Save (source, true);

            Assert::AreEqual<uint64_t> (shown, source.shell->GetMachine().GetPosition(), L"the save leaves the machine where it was");
            Assert::IsTrue             (source.shell->GetHistory().IsBehindLive(),          L"behind live");

            Load (target, bytes);

            Assert::AreEqual<uint64_t> (shown, target.shell->GetMachine().GetPosition(), L"the load resumes there");
            Assert::IsTrue             (target.shell->GetHistory().IsBehindLive(),          L"behind live");
            Assert::AreEqual<uint64_t> (live,  GetController (target).GetLiveEndPosition(), L"with the same live end");
        }


        //  A file with no history is a marked point in the history
        //  recording, which goes on from before the load.
        TEST_METHOD (AStateWithNoHistoryIsAMarkInTheHistory)
        {
            ShellRig           rig (false);
            std::vector<Byte>  bytes;
            uint64_t           oldest  = 0;
            uint64_t           at      = 0;
            size_t             newest  = 0;



            rig.RunFrames (kFrames / 3);
            bytes = Save (rig, false);

            rig.RunFrames (kFrames);
            oldest = GetController (rig).GetOldestPosition();
            at     = rig.shell->GetMachine().GetPosition();

            Load (rig, bytes);

            newest = GetController (rig).GetKeyframes().GetCount() - 1;

            Assert::IsTrue             (rig.shell->GetHistory().IsRecording(),            L"recording");
            Assert::AreEqual<uint64_t> (oldest, GetController (rig).GetOldestPosition(),  L"history before the load is kept");
            Assert::IsTrue             (HasStateLoadAt (rig, at),                         L"the load is marked where it was made");
            Assert::IsTrue             (GetController (rig).GetKeyframes().GetInfo (newest).isBoundary, L"a boundary keyframe holds the loaded machine");
            Assert::AreEqual<uint64_t> (at, GetController (rig).GetKeyframes().GetInfo (newest).position, L"at the load");
        }


        //  Behind live, the load forks history first.
        TEST_METHOD (AStateWithNoHistoryLoadedBehindLiveForks)
        {
            ShellRig                     rig (false);
            std::vector<Byte>            bytes;
            std::vector<KeyframeBranch>  branches;
            uint64_t                     at  = 0;



            rig.RunFrames (kFrames / 3);
            bytes = Save (rig, false);

            rig.RunFrames (kFrames);
            rig.Reverse (ReverseCommand::Seek, rig.shell->GetMachine().GetPosition() - ShellHistoryTests::kFrameCycles);
            at = rig.shell->GetMachine().GetPosition();

            Load (rig, bytes);

            GetController (rig).GetKeyframes().GetBranches (branches);

            Assert::AreEqual<size_t>   (1, branches.size(),                     L"the future left behind is a branch");
            Assert::AreEqual<uint64_t> (at, branches[0].forkPosition,           L"forked at the load");
            Assert::IsFalse            (rig.shell->GetHistory().IsBehindLive(), L"live at the load");
            Assert::IsTrue             (HasStateLoadAt (rig, at),               L"and the load is marked");
        }


        //  With history off, a save has no history, and a file's history is
        //  not taken up by a load.
        TEST_METHOD (WithHistoryOffNoHistoryIsSavedOrLoaded)
        {
            ShellRig           source (false);
            ShellRig           target (false);
            std::vector<Byte>  bytes;
            std::vector<Byte>  history;
            uint64_t           resumeAt = 0;
            HRESULT            hr       = S_OK;



            source.RunFrames (kFrames);
            bytes = Save (source, true);

            target.Dispatch (IDM_DEBUG_REVERSE_OPTIONS, CpuCommandDispatcher::FormatReverseOptionsPayload (false, kBudgetMb));

            hr = target.shell->GetHistory().BuildSavedHistory (history, resumeAt);
            AssertSucceeded (hr, L"BuildSavedHistory");

            Assert::IsTrue (history.empty(), L"no history to save");

            Load (target, bytes);

            Assert::IsFalse (target.shell->GetHistory().IsRecording(), L"the file's history is not taken up");
        }
    };
}
