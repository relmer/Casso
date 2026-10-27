#include "Pch.h"

#include "HResultAssert.h"
#include "Debugger/Reverse/InputJournal.h"
#include "Debugger/Reverse/KeyframeStore.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace KeyframeStoreBranchTests
{
    static constexpr size_t    kStateBytes = 4096;
    static constexpr uint64_t  kStride     = 100;
    static constexpr uint64_t  kInterval   = 1000;
    static uint64_t            s_clock     = 0;





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  BranchRig
    //
    //  A store of keyframes every kStride positions, kInterval cycles apart,
    //  each a state of noise seeded by its position, with one journal record
    //  made before each, and a clock that counts.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class BranchRig
    {
    public:
        KeyframeStore  store;
        InputJournal   journal;
        uint64_t       position = 0;



        static uint64_t Tick()
        {
            return ++s_clock;
        }


        explicit BranchRig (uint32_t wholeEvery = 3)
        {
            KeyframeSettings  settings;



            settings.intervalCycles = kInterval;
            settings.wholeEvery     = wholeEvery;
            settings.longestGroup   = wholeEvery;

            store.Configure    (settings);
            store.SetWallClock (&Tick);

            journal.SetOn             (true);
            journal.SetPositionSource (&position);
        }


        static std::vector<Byte> MakeState (uint64_t at, Byte seed)
        {
            std::vector<Byte>  state (kStateBytes);
            std::mt19937       noise (static_cast<uint32_t> (at * 31 + seed));



            for (Byte & each : state)
            {
                each = static_cast<Byte> (noise());
            }

            return state;
        }


        //  Keyframes from first to last position, every kStride, with seed
        //  telling one path's states from another's.
        void Fill (uint64_t first, uint64_t last, Byte seed)
        {
            HRESULT  hr = S_OK;



            for (uint64_t at = first; at <= last; at += kStride)
            {
                position = at;
                journal.Record (at / kStride * kInterval, InputKind::KeyPress, seed, 0, std::string_view());

                hr = store.Add (at, at / kStride * kInterval, journal.GetEndIndex(), MakeState (at, seed));
                AssertSucceeded (hr, L"Add");
            }

            hr = store.WaitForPending();
            AssertSucceeded (hr, L"WaitForPending");
        }


        //  The journal index of the first record made at or after at.
        size_t GetJournalIndexAt (uint64_t at) const
        {
            size_t  i = journal.GetBeginIndex();



            while (i < journal.GetEndIndex() && journal.GetRecord (i).position < at)
            {
                i++;
            }

            return i;
        }


        void Fork (uint64_t at)
        {
            HRESULT  hr = store.Fork (at, at / kStride * kInterval, journal, GetJournalIndexAt (at));



            AssertSucceeded (hr, L"Fork");
        }


        uint32_t GetBranchId (size_t index) const
        {
            std::vector<KeyframeBranch>  branches;



            store.GetBranches (branches);
            return branches[index].id;
        }


        std::vector<uint64_t> GetPositions() const
        {
            std::vector<uint64_t>  positions;



            for (size_t i = 0; i < store.GetCount(); i++)
            {
                positions.push_back (store.GetInfo (i).position);
            }

            return positions;
        }


        bool DoesRestore (uint64_t at, Byte seed)
        {
            std::vector<Byte>  state;
            bool               isFound = false;
            HRESULT            hr      = store.RestoreAtPosition (at, state, isFound);



            AssertSucceeded (hr, L"RestoreAtPosition");
            return isFound && state == MakeState (at, seed);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  KeyframeStoreBranchTests
    //
    //  A fork moves the keyframes and records after it into a branch left
    //  behind, with no bytes copied; a switch makes a branch the active path,
    //  its differences restoring against the whole snapshot before the fork;
    //  and room is made from the branches first, oldest first, with no more
    //  than kMaxBranches left.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (KeyframeStoreBranchTests)
    {
    public:

        static bool HasGapFrom (const KeyframeStore & store, uint64_t gapStart)
        {
            bool  isFound = false;



            for (size_t i = 0; i < store.GetCount() && !isFound; i++)
            {
                isFound = store.GetInfo (i).hasGapBefore && store.GetInfo (i).gapStart == gapStart;
            }

            return isFound;
        }


        TEST_METHOD (AForkMovesTheTailIntoABranch)
        {
            BranchRig                    rig;
            std::vector<KeyframeBranch>  branches;
            size_t                       bytes = 0;



            rig.Fill (0, 900, 1);
            bytes = rig.store.GetByteCount();

            rig.Fork (500);
            rig.store.GetBranches (branches);

            Assert::AreEqual<size_t>   (5,     rig.store.GetCount(), L"0 to 400 stay on the active path");
            Assert::AreEqual<size_t>   (1,     branches.size(),      L"one branch");
            Assert::AreEqual<uint64_t> (500,   branches[0].forkPosition,    L"forked at 500");
            Assert::AreEqual<size_t>   (5,     branches[0].keyframeCount,   L"with 500 to 900");
            Assert::AreEqual<uint64_t> (900,   branches[0].liveEndPosition, L"ending at 900");
            Assert::AreEqual           (bytes, rig.store.GetByteCount(),    L"nothing copied or dropped");
            Assert::AreEqual<size_t>   (5, rig.journal.GetEndIndex(), L"the records from the fork on moved out of the journal");
        }


        TEST_METHOD (ASwitchMakesTheBranchTheActivePath)
        {
            BranchRig                    rig;
            std::vector<KeyframeBranch>  branches;
            std::vector<uint64_t>        expected = { 0, 100, 200, 300, 400, 500, 600, 700, 800, 900 };
            HRESULT                      hr       = S_OK;
            size_t                       end      = 0;



            rig.Fill (0, 900, 1);
            end = rig.journal.GetEndIndex();

            rig.Fork (500);
            rig.Fill (500, 700, 2);

            hr = rig.store.SwitchTo (rig.GetBranchId (0), rig.journal);
            AssertSucceeded (hr, L"SwitchTo");

            rig.store.GetBranches (branches);

            Assert::IsTrue             (expected == rig.GetPositions(), L"the branch's keyframes follow the path before the fork");
            Assert::IsTrue             (rig.DoesRestore (700, 1), L"a difference in it restores against the whole snapshot before the fork");
            Assert::IsTrue             (rig.DoesRestore (300, 1), L"and the path before the fork restores");
            Assert::AreEqual<size_t>   (1,   branches.size(), L"the path left is a branch");
            Assert::AreEqual<uint64_t> (500, branches[0].forkPosition, L"forked at the same position");
            Assert::AreEqual<uint64_t> (700, branches[0].liveEndPosition, L"ending where it ended");
            Assert::AreEqual<size_t>   (end, rig.journal.GetEndIndex(), L"the branch's records are back in the journal");
            Assert::AreEqual<int>      (1,   (int) rig.journal.GetRecord (end - 1).value, L"the branch's, not the other path's");
        }


        TEST_METHOD (ASwitchBackRestoresTheOtherPath)
        {
            BranchRig  rig;
            HRESULT    hr = S_OK;



            rig.Fill (0, 900, 1);
            rig.Fork (500);
            rig.Fill (500, 700, 2);

            hr = rig.store.SwitchTo (rig.GetBranchId (0), rig.journal);
            AssertSucceeded (hr, L"to the first path");

            hr = rig.store.SwitchTo (rig.GetBranchId (0), rig.journal);
            AssertSucceeded (hr, L"and back");

            Assert::IsTrue (rig.DoesRestore (700, 2), L"the second path's states");
            Assert::IsTrue (rig.DoesRestore (500, 2), L"from its first keyframe");
        }


        TEST_METHOD (ASwitchSendsPathChangedWithTheForkPosition)
        {
            BranchRig  rig;
            bool       isSent = false;
            uint64_t   from   = 0;
            HRESULT    hr     = S_OK;



            rig.Fill (0, 900, 1);
            rig.Fork (500);
            rig.Fill (500, 700, 2);

            rig.store.SetDropListener (&rig, [&] (KeyframeDrop drop)
            {
                if (drop == KeyframeDrop::PathChanged)
                {
                    isSent = true;
                    from   = rig.store.GetPathChangePosition();
                }
            });

            hr = rig.store.SwitchTo (rig.GetBranchId (0), rig.journal);
            AssertSucceeded (hr, L"SwitchTo");

            Assert::IsTrue             (isSent, L"the listeners are sent PathChanged");
            Assert::AreEqual<uint64_t> (500, from, L"from the fork position");
        }


        TEST_METHOD (ASwitchToABranchOfABranchMovesEachSegment)
        {
            BranchRig  rig;
            uint32_t   inner = 0;
            HRESULT    hr    = S_OK;



            //  Path 1 runs to 900, and path 2 forks from it at 500. Path 3
            //  forks from path 2 at 300, which moves path 2 from 300 on into
            //  a branch, and path 1 now forks from that branch.
            rig.Fill (0, 900, 1);
            rig.Fork (500);
            rig.Fill (500, 900, 2);
            rig.Fork (300);
            rig.Fill (300, 600, 3);

            inner = rig.GetBranchId (0);

            hr = rig.store.SwitchTo (inner, rig.journal);
            AssertSucceeded (hr, L"SwitchTo");

            Assert::IsTrue           (rig.DoesRestore (900, 1), L"path 1's end");
            Assert::IsTrue           (rig.DoesRestore (400, 1), L"through the stretch from 300 to the fork at 500");
            Assert::AreEqual<size_t> (2, rig.store.GetBranchCount(), L"paths 2 and 3 left behind");
        }


        TEST_METHOD (RoomIsMadeFromTheOldestBranchFirst)
        {
            BranchRig                    rig;
            std::vector<KeyframeBranch>  branches;
            size_t                       active = 0;
            HRESULT                      hr     = S_OK;



            rig.Fill (0, 2900, 1);
            rig.Fork (2000);
            rig.Fill (2000, 2900, 2);
            rig.Fork (1500);
            rig.Fill (1500, 2900, 3);

            active = rig.store.GetCount();

            hr = rig.store.ChangeBudget (rig.store.GetByteCount() - 6 * kStateBytes);
            AssertSucceeded (hr, L"ChangeBudget");

            rig.store.GetBranches (branches);

            Assert::AreEqual<size_t> (active, rig.store.GetCount(), L"every keyframe stays on the active path");
            Assert::IsTrue           (branches.size() <= 2, L"the room came from the branches");
        }


        TEST_METHOD (TheActivePathsOldestGoOnlyWithNoBranchLeft)
        {
            BranchRig  rig;
            HRESULT    hr = S_OK;



            rig.Fill (0, 2900, 1);
            rig.Fork (2000);
            rig.Fill (2000, 2900, 2);

            hr = rig.store.ChangeBudget (12 * kStateBytes);
            AssertSucceeded (hr, L"ChangeBudget");

            Assert::AreEqual<size_t> (0, rig.store.GetBranchCount(), L"the branch went first");
            Assert::IsTrue           (rig.store.GetInfo (0).position > 0, L"then the active path's oldest");
            Assert::IsTrue           (rig.DoesRestore (2900, 2), L"the active path's newest stays");
        }


        TEST_METHOD (MoreThanTheMostBranchesDropsTheOldest)
        {
            BranchRig                    rig;
            std::vector<KeyframeBranch>  branches;



            rig.Fill (0, 1000, 0);

            for (Byte seed = 0; seed <= KeyframeStore::kMaxBranches; seed++)
            {
                rig.Fork (1000);
                rig.Fill (1000, 1000, static_cast<Byte> (seed + 1));
            }

            rig.store.GetBranches (branches);

            Assert::AreEqual<size_t> (KeyframeStore::kMaxBranches, branches.size(), L"no more than the most");
            Assert::IsTrue           (std::none_of (branches.begin(), branches.end(), [] (const KeyframeBranch & each) { return each.id == 1; }), L"the first branch left is the one dropped");
        }


        TEST_METHOD (ATrimmedBranchBeginsWithAGap)
        {
            BranchRig                    rig;
            std::vector<KeyframeBranch>  branches;
            HRESULT                      hr = S_OK;



            rig.Fill (0, 2000, 1);
            rig.Fork (500);
            rig.Fill (500, 600, 2);

            hr = rig.store.ChangeBudget (rig.store.GetByteCount() - 2 * kStateBytes);
            AssertSucceeded (hr, L"ChangeBudget");

            rig.store.GetBranches (branches);
            Assert::AreEqual<size_t> (1, branches.size(), L"the branch is trimmed, not dropped");

            hr = rig.store.SwitchTo (branches[0].id, rig.journal);
            AssertSucceeded (hr, L"SwitchTo");

            Assert::IsTrue (rig.DoesRestore (2000, 1), L"its live end restores");
            Assert::IsTrue (HasGapFrom (rig.store, 500), L"after a gap from the fork");
        }
    };
}
