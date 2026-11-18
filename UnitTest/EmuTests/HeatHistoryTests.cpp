#include "Pch.h"

#include "EmuTests/InlineWorkQueue.h"
#include "EmuTests/HeatHistoryChecks.h"
#include "EmuTests/ReverseSessionRig.h"
#include "Debugger/AccessHeatMap.h"
#include "Debugger/HeatAccessJump.h"
#include "Debugger/HeatCountDelta.h"
#include "Debugger/HeatHistory.h"
#include "Debugger/HeatKeyframeSide.h"
#include "Debugger/MachineDebugTarget.h"
#include "Shell/ScratchHeatReplayer.h"
#include "HResultAssert.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  HeatHistoryTests
//
//  The heat map follows the machine's position through reverse execution's
//  history, not the host's clock. The cumulative totals at any position,
//  however it was reached -- seeks back and forward, steps, running on from
//  the past -- equal a straight count from where the map started, or from a
//  reset, to that position, also once history has dropped what came before
//  the reset; and the fading heat rebuilt after a move equals the heat a
//  map folded frame by frame on a straight run to the same position holds.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (HeatHistoryTests)
{
public:

    //  The straight counts: a second machine running the same session with
    //  its map on from the same position, its totals kept at each position
    //  asked for.
    static std::map<uint64_t, std::vector<int64_t>> CountStraight (const std::set<uint64_t> & positions)
    {
        TestMachine                                machine ("Apple2e");
        MachineDebugTarget                         target  (machine);
        HeatScript                                 script;
        std::map<uint64_t, std::vector<int64_t>>   totals;
        const AccessHeatMap                      * map     = nullptr;



        ReverseSessionRig::Prepare (machine);
        target.SetHeatMapOn (true);

        for (uint64_t position : positions)
        {
            script.RunTo (machine, position);
            Assert::AreEqual<uint64_t> (position, machine.GetRunner().GetPosition(), L"the straight run reaches the position");

            map = target.FoldHeatMap();
            map->GetTotalsNow (totals[position]);
        }

        return totals;
    }


    //  Entries to compare the last accesses by: every space's writes and
    //  reads the live map holds, picked evenly across the order they were
    //  made in, and a few it never saw.
    static std::vector<size_t> PickLastSample (const AccessHeatMap & map)
    {
        constexpr size_t                                kPicks = 24;
        std::vector<std::pair<uint64_t, size_t>>        made;
        std::vector<size_t>                             sample;
        HeatLastAccess                                  access;



        for (size_t space = 0; space < AccessHeatMap::kSpaceCount; space++)
        {
            for (bool isWrite : { true, false })
            {
                for (size_t address = 0; address < AccessHeatMap::kAddressCount; address++)
                {
                    if (map.GetLastAccess ((HeatSpace) space, isWrite, (Word) address, access) == HeatAccessState::Found)
                    {
                        made.emplace_back (access.GetPosition(), AccessHeatMap::GetLastIndex ((HeatSpace) space, isWrite, (Word) address));
                    }
                }
            }
        }

        Assert::IsTrue (made.size() > kPicks, L"the session made accesses to pick from");

        std::ranges::sort (made);

        for (size_t i = 0; i < kPicks; i++)
        {
            sample.push_back (made[i * made.size() / kPicks].second);
        }

        sample.push_back (AccessHeatMap::GetLastIndex (HeatSpace::Aux, true, 0xBFFE));
        sample.push_back (AccessHeatMap::GetLastIndex (HeatSpace::Rom, false, 0xC3FF));
        return sample;
    }


    //  An entry's space, kind and address, from its index.
    static void SplitEntry (size_t entry, HeatSpace & space, bool & isWrite, Word & address)
    {
        space   = (HeatSpace) (entry / (2 * AccessHeatMap::kAddressCount));
        isWrite = (entry / AccessHeatMap::kAddressCount) % 2 == 0;
        address = (Word) (entry % AccessHeatMap::kAddressCount);
    }


    //  The sample's last accesses as the target gives them where the
    //  machine stands, none for none, and a forgotten stamp for unknown.
    static std::vector<HeatLastAccess> LookUpSample (IDebugTarget & target, const std::vector<size_t> & sample)
    {
        std::vector<HeatLastAccess>  found;
        HeatLastAccess               access;
        HeatSpace                    space   = HeatSpace::Cpu;
        bool                         isWrite = false;
        Word                         address = 0;
        HeatAccessState              state   = HeatAccessState::None;



        for (size_t entry : sample)
        {
            SplitEntry (entry, space, isWrite, address);

            state = target.LookUpHeatMapAccess (space, isWrite, address, access);

            if (state == HeatAccessState::Unknown)
            {
                access.stamp = HeatLastAccess::kForgotten;
            }

            found.push_back (access);
        }

        return found;
    }


    //  The sample's last accesses a second machine running the same session
    //  saw, at each position asked for.
    static std::map<uint64_t, std::vector<HeatLastAccess>> GetStraightLast (const std::set<uint64_t> & positions, const std::vector<size_t> & sample)
    {
        TestMachine                                        machine ("Apple2e");
        MachineDebugTarget                                 target  (machine);
        HeatScript                                         script;
        std::map<uint64_t, std::vector<HeatLastAccess>>   last;



        ReverseSessionRig::Prepare (machine);
        target.SetHeatMapOn (true);

        for (uint64_t position : positions)
        {
            script.RunTo (machine, position);
            Assert::AreEqual<uint64_t> (position, machine.GetRunner().GetPosition(), L"the straight run reaches the position");

            (void) target.FoldHeatMap();
            last[position] = LookUpSample (target, sample);
        }

        return last;
    }


    static void AssertSameLast (const std::vector<HeatLastAccess> & expected, const std::vector<HeatLastAccess> & actual, const std::wstring & where)
    {
        size_t  first = 0;
        size_t  known = 0;



        Assert::AreEqual (expected.size(), actual.size(), where.c_str());

        while (first < expected.size() && expected[first] == actual[first])
        {
            first++;
        }

        known = (size_t) std::ranges::count_if (expected, [] (const HeatLastAccess & access) { return !access.IsNone() && !access.IsForgotten(); });

        Assert::IsTrue (first == expected.size(),
                        std::format (L"{}: sample {} holds PC ${:04X} at {}, a straight run gives PC ${:04X} at {}",
                                     where,
                                     first,
                                     (first < actual.size())   ? actual[first].GetPc()         : 0,
                                     (first < actual.size())   ? actual[first].GetPosition()   : 0,
                                     (first < expected.size()) ? expected[first].GetPc()       : 0,
                                     (first < expected.size()) ? expected[first].GetPosition() : 0).c_str());

        Assert::IsTrue (known > 0, (where + L": the straight run saw no access, so the comparison proves nothing").c_str());
    }

    //  What a straight count from from to to shows: the counts between, or
    //  none before from.
    static std::vector<int64_t> GetCountedSince (const std::vector<int64_t> & to, const std::vector<int64_t> & from)
    {
        std::vector<int64_t>  counted (to.size());



        for (size_t i = 0; i < to.size(); i++)
        {
            counted[i] = std::max<int64_t> (to[i] - from[i], 0);
        }

        return counted;
    }


    //  Totals with every space's reads before written taken out: a map
    //  turned on after power-on cannot tell them, and counts none, where a
    //  map on from power-on counts them all.
    static std::vector<int64_t> WithoutUnwritten (std::vector<int64_t> totals)
    {
        for (size_t space = 0; space < AccessHeatMap::kSpaceCount; space++)
        {
            size_t  first = AccessHeatMap::GetIndex ((HeatSpace) space, HeatKind::UnwrittenRead, 0);



            std::fill (totals.begin() + first, totals.begin() + first + AccessHeatMap::kAddressCount, 0);
        }

        return totals;
    }


    //  Positions spread over the history held, oldest to newest.
    static std::vector<uint64_t> SpreadPositions (const ReverseController & controller, uint64_t liveEnd)
    {
        uint64_t               oldest = controller.GetSeeker().GetOldestPosition();
        std::vector<uint64_t>  spread;



        for (size_t i = 1; i <= HeatHistoryChecks::kSamples; i++)
        {
            spread.push_back (oldest + (liveEnd - oldest) * i / (HeatHistoryChecks::kSamples + 1));
        }

        return spread;
    }



    TEST_METHOD (CumulativeTotalsFollowEverySeekStepAndRunOn)
    {
        HeatHistoryRig                             rig      (ReverseSessionRig::MakeSettings (KeyframeSettings::kDefaultFrames));
        std::vector<uint64_t>                      spread;
        std::set<uint64_t>                         wanted;
        std::map<uint64_t, std::vector<int64_t>>   straight;
        ReverseResult                              result;
        uint64_t                                   firstEnd = 0;
        uint64_t                                   at       = 0;
        size_t                                     i        = 0;
        HRESULT                                    hr       = S_OK;



        rig.script.RunTo (rig.machine, UINT64_MAX);
        Assert::AreEqual (HeatHistoryChecks::kSlices, rig.script.GetSlice(), L"the whole session ran");

        firstEnd = rig.machine.GetRunner().GetPosition();
        spread   = SpreadPositions (rig.controller, firstEnd);

        for (uint64_t position : spread)
        {
            wanted.insert (position);
            wanted.insert (position - 1);
            wanted.insert (position + 1);
            wanted.insert (position + HeatHistoryChecks::kRunOn);
        }

        wanted.insert (firstEnd);

        straight = CountStraight (wanted);

        HeatHistoryChecks::AssertSameTotals (straight[firstEnd], HeatHistoryChecks::GetShown (rig.GetMap()), L"live, before any move");

        //  Seeks back and forward, scattered.
        for (i = 0; i < spread.size(); i++)
        {
            at = spread[(i * HeatHistoryChecks::kShuffle) % spread.size()];

            rig.Seek (at);
            HeatHistoryChecks::AssertSameTotals (straight[at], HeatHistoryChecks::GetShown (rig.GetMap()), std::format (L"seek to {}", at));
        }

        //  Step back, then step forward twice, then run on from the past.
        for (uint64_t position : spread)
        {
            rig.Seek (position);

            hr = rig.controller.GetStepper().StepBack (result);
            AssertSucceeded (hr, L"StepBack");
            rig.target.NoteHistoryMoved (false);
            HeatHistoryChecks::AssertSameTotals (straight[position - 1], HeatHistoryChecks::GetShown (rig.GetMap()), std::format (L"step back from {}", position));

            hr = rig.controller.GetSeeker().StepForward (result);
            AssertSucceeded (hr, L"StepForward");
            hr = rig.controller.GetSeeker().StepForward (result);
            AssertSucceeded (hr, L"StepForward");
            rig.target.NoteHistoryMoved (false);
            HeatHistoryChecks::AssertSameTotals (straight[position + 1], HeatHistoryChecks::GetShown (rig.GetMap()), std::format (L"two steps forward from {}", position - 1));

            while (rig.machine.GetRunner().GetPosition() < position + HeatHistoryChecks::kRunOn)
            {
                rig.machine.GetRunner().StepOne();
            }

            HeatHistoryChecks::AssertSameTotals (straight[position + HeatHistoryChecks::kRunOn], HeatHistoryChecks::GetShown (rig.GetMap()), std::format (L"running on from {}", position + 1));
        }

        //  Back to the live end, and the live end again after a seek back.
        hr = rig.controller.GetSeeker().SeekToCycle (UINT64_MAX, result);
        AssertSucceeded (hr, L"SeekToCycle live");
        rig.target.NoteHistoryMoved (false);

        Assert::IsFalse (rig.controller.GetSpan().IsInHistory(), L"live again");
        HeatHistoryChecks::AssertSameTotals (straight[firstEnd], HeatHistoryChecks::GetShown (rig.GetMap()), L"back at the live end");
    }


    //  A switch to the branch a fork left behind puts the machine at the
    //  branch's live end: the totals there, and at a seek back along the
    //  branch past the fork, are a straight run's.
    TEST_METHOD (CumulativeTotalsFollowASwitchToABranch)
    {
        HeatHistoryRig                             rig      (ReverseSessionRig::MakeSettings (KeyframeSettings::kDefaultFrames));
        std::vector<uint64_t>                      spread;
        std::vector<KeyframeBranch>                branches;
        std::map<uint64_t, std::vector<int64_t>>   straight;
        ReverseResult                              result;
        uint64_t                                   firstEnd = 0;
        uint64_t                                   forkAt   = 0;
        uint64_t                                   onBranch = 0;
        HRESULT                                    hr       = S_OK;



        rig.script.RunTo (rig.machine, UINT64_MAX);

        firstEnd = rig.machine.GetRunner().GetPosition();
        spread   = SpreadPositions (rig.controller, firstEnd);
        forkAt   = spread[spread.size() / 2];
        onBranch = spread[spread.size() - 2];
        straight = CountStraight ({ firstEnd, onBranch });

        rig.Seek (forkAt);

        hr = rig.controller.GetSeeker().OnMachineChanged();
        AssertSucceeded (hr, L"OnMachineChanged forks");

        while (rig.machine.GetRunner().GetPosition() < forkAt + HeatHistoryChecks::kRunOn)
        {
            rig.machine.GetRunner().StepOne();
        }

        rig.controller.GetKeyframes().GetArena().GetBranches (branches);
        Assert::AreEqual<size_t> (1, branches.size(), L"a branch left behind");

        hr = rig.controller.GetSeeker().SwitchBranch (branches[0].id, result);
        AssertSucceeded (hr, L"SwitchBranch");
        rig.target.NoteHistoryMoved (false);

        Assert::AreEqual<uint64_t> (firstEnd, rig.machine.GetRunner().GetPosition(), L"at the branch's live end");
        HeatHistoryChecks::AssertSameTotals (straight[firstEnd], HeatHistoryChecks::GetShown (rig.GetMap()), L"at the branch's live end");

        rig.Seek (onBranch);
        HeatHistoryChecks::AssertSameTotals (straight[onBranch], HeatHistoryChecks::GetShown (rig.GetMap()), L"a seek back along the branch");
    }


    //  The instruction that last wrote and last read an address, in every
    //  space, follows the machine as the totals do: after any seek, step or
    //  run on, it is the one a straight run to the same position saw, the
    //  map's own record where it holds one as of there and history's,
    //  looked up by replaying a stretch, where it does not.
    TEST_METHOD (LastAccessesFollowEverySeekStepAndRunOn)
    {
        HeatHistoryRig                                         rig      (ReverseSessionRig::MakeSettings (KeyframeSettings::kDefaultFrames));
        ScratchHeatReplayer                                    finder;
        std::vector<uint64_t>                                  spread;
        std::set<uint64_t>                                     wanted;
        std::vector<size_t>                                    sample;
        std::map<uint64_t, std::vector<HeatLastAccess>>        straight;
        ReverseResult                                          result;
        HRESULT                                                hr       = S_OK;



        finder.SetMachine (rig.machine.GetConfig(), rig.machine.GetCurrentMachineName());
        rig.target.SetHeatAccessFinder (&finder);

        rig.script.RunTo (rig.machine, UINT64_MAX);
        spread = SpreadPositions (rig.controller, rig.machine.GetRunner().GetPosition());
        sample = PickLastSample (rig.GetMap());

        for (uint64_t position : spread)
        {
            wanted.insert (position);
            wanted.insert (position - 1);
            wanted.insert (position + HeatHistoryChecks::kRunOn);
        }

        straight = GetStraightLast (wanted, sample);

        for (size_t i = 0; i < spread.size(); i++)
        {
            uint64_t  at = spread[(i * HeatHistoryChecks::kShuffle) % spread.size()];



            rig.Seek (at);
            AssertSameLast (straight[at], LookUpSample (rig.target, sample), std::format (L"seek to {}", at));
        }

        for (size_t i = 0; i < spread.size(); i += 3)
        {
            uint64_t  position = spread[i];



            rig.Seek (position);

            hr = rig.controller.GetStepper().StepBack (result);
            AssertSucceeded (hr, L"StepBack");
            rig.target.NoteHistoryMoved (false);
            AssertSameLast (straight[position - 1], LookUpSample (rig.target, sample), std::format (L"step back from {}", position));

            while (rig.machine.GetRunner().GetPosition() < position + HeatHistoryChecks::kRunOn)
            {
                rig.machine.GetRunner().StepOne();
            }

            AssertSameLast (straight[position + HeatHistoryChecks::kRunOn], LookUpSample (rig.target, sample), std::format (L"running on from {}", position - 1));
        }
    }

    //  Going back to an address's last write lands just after the
    //  instruction that made it: the write is the last there, the step back
    //  before it is that instruction, and there the write has not happened.
    TEST_METHOD (GoingBackToALastWriteLandsJustAfterIt)
    {
        HeatHistoryRig                  rig      (ReverseSessionRig::MakeSettings (KeyframeSettings::kDefaultFrames));
        HeatLastAccess                  access;
        HeatLastAccess                  there;
        HeatAccessPlan                  plan;
        HeatAccessRequest               request;
        ReverseResult                   result;
        std::optional<Word>             written;
        HRESULT                         hr       = S_OK;
        ScratchHeatReplayer             finder;



        finder.SetMachine (rig.machine.GetConfig(), rig.machine.GetCurrentMachineName());
        rig.target.SetHeatAccessFinder (&finder);

        rig.script.RunTo (rig.machine, UINT64_MAX);

        //  An address written well inside history.
        for (size_t address = 0; address < AccessHeatMap::kAddressCount && !written.has_value(); address++)
        {
            if (rig.GetMap().GetLastAccess (HeatSpace::Main, true, (Word) address, access) == HeatAccessState::Found && access.GetPosition() > rig.controller.GetSeeker().GetOldestPosition() + HeatHistoryChecks::kRunOn)
            {
                written = (Word) address;
            }
        }

        Assert::IsTrue (written.has_value(), L"something was written inside history, or the test proves nothing");

        request.isRewind = true;
        request.isWrite  = true;
        request.bank     = HeatMapOptions::Bank::Main;
        request.address  = written.value_or (0);

        plan = HeatAccessJump::Plan (request, HeatAccessState::Found, access, rig.controller.GetSpan().IsRecording(), rig.controller.GetSeeker().GetOldestPosition());
        Assert::AreEqual ((int) HeatAccessPlan::Kind::Seek, (int) plan.kind);

        rig.Seek (plan.position);

        Assert::AreEqual<uint64_t> (access.GetPosition() + 1, rig.machine.GetRunner().GetPosition(), L"just after the writer");
        Assert::AreEqual ((int) HeatAccessState::Found, (int) rig.target.LookUpHeatMapAccess (HeatSpace::Main, true, request.address, there), L"the write is there");
        Assert::IsTrue (there == access, L"and it is the last");

        hr = rig.controller.GetStepper().StepBack (result);
        AssertSucceeded (hr, L"StepBack");
        rig.target.NoteHistoryMoved (false);

        Assert::AreEqual (access.GetPc(), rig.target.GetRegisters().pc, L"a step back is the writer");
        (void) rig.target.LookUpHeatMapAccess (HeatSpace::Main, true, request.address, there);
        Assert::IsFalse (there == access, L"before it ran, the write has not happened");
    }


    TEST_METHOD (CumulativeTotalsHoldAcrossHistoryRecordedAfterAMove)
    {
        HeatHistoryRig                             rig      (ReverseSessionRig::MakeSettings (KeyframeSettings::kDefaultFrames));
        std::set<uint64_t>                         wanted;
        std::map<uint64_t, std::vector<int64_t>>   straight;
        ReverseResult                              result;
        uint64_t                                   midway   = 0;
        uint64_t                                   end      = 0;
        HRESULT                                    hr       = S_OK;



        //  Part of the session, a seek back, then on to the live end and the
        //  rest of the session, recorded after the move.
        rig.script.RunTo (rig.machine, UINT64_MAX);
        end = rig.machine.GetRunner().GetPosition();

        midway = rig.controller.GetSeeker().GetOldestPosition() + (end - rig.controller.GetSeeker().GetOldestPosition()) / 2;

        rig.Seek (midway);

        hr = rig.controller.GetSeeker().SeekToCycle (UINT64_MAX, result);
        AssertSucceeded (hr, L"SeekToCycle live");
        rig.target.NoteHistoryMoved (false);

        wanted.insert (midway);
        wanted.insert (end);

        straight = CountStraight (wanted);

        HeatHistoryChecks::AssertSameTotals (straight[end], HeatHistoryChecks::GetShown (rig.GetMap()), L"live after going back and forth");

        rig.Seek (midway);
        HeatHistoryChecks::AssertSameTotals (straight[midway], HeatHistoryChecks::GetShown (rig.GetMap()), L"midway again");
    }


    //  A write made before a reset partway through a stretch is no write
    //  since the map started counting. The stretch's counts include it, so
    //  the search replays the stretch, from the reset, and finds nothing:
    //  that is none, not unknown.
    TEST_METHOD (AWriteBeforeAResetInsideItsStretchIsNone)
    {
        static constexpr Byte  kProgram[] =
        {
            0x8D, 0x00, 0x20,       // 6000  STA $2000
            0xA2, 0x00,             // 6003  LDX #$00
            0xA0, 0x00,             // 6005  LDY #$00
            0x88,                   // 6007  DEY
            0xD0, 0xFD,             // 6008  BNE $6007
            0xCA,                   // 600A  DEX
            0xD0, 0xFA,             // 600B  BNE $6007
            0x8D, 0x00, 0x20,       // 600D  STA $2000
            0x4C, 0x10, 0x60,       // 6010  JMP $6010
        };

        struct ProgramRig
        {
            TestMachine          machine    { "Apple2e" };
            MachineDebugTarget   target     { machine };
            ReverseController    controller { machine };
            ScratchHeatReplayer  finder;

            ProgramRig()
            {
                HRESULT  hr = S_OK;



                machine.PowerCycle();

                for (size_t i = 0; i < sizeof (kProgram); i++)
                {
                    machine.GetMemoryBus().WriteByte (static_cast<Word> (0x6000 + i), kProgram[i]);
                }

                machine.GetCpu()->SetPC (0x6000);

                hr = controller.Start (ReverseSessionRig::MakeSettings (1));
                AssertSucceeded (hr, L"Start");

                target.AttachHistory (&controller.GetKeyframes(), nullptr);
                controller.GetCapturer().SetObserver (target.GetHistoryObserver());
                target.SetHeatMapOn (true);

                finder.SetMachine (machine.GetConfig(), machine.GetCurrentMachineName());
                target.SetHeatAccessFinder (&finder);
            }

            ~ProgramRig()
            {
                target.SetHeatAccessFinder (nullptr);
                controller.GetCapturer().SetObserver (nullptr);
                target.AttachHistory (nullptr, nullptr);
            }

            void Seek (uint64_t position)
            {
                ReverseResult  result;
                HRESULT        hr     = controller.GetSeeker().SeekToPosition (position, result);



                AssertSucceeded (hr, L"SeekToPosition");
                target.NoteHistoryMoved (false);
            }
        };

        ProgramRig       rig;
        HeatLastAccess   access;
        HeatAccessState  state  = HeatAccessState::Found;
        uint64_t         start  = rig.machine.GetRunner().GetPosition();
        uint64_t         middle = 0;



        for (size_t i = 0; i < 1000000 && rig.machine.GetCpu()->GetPC() != 0x6010; i++)
        {
            rig.machine.GetRunner().StepOne();
        }

        for (size_t i = 0; i < 100; i++)
        {
            rig.machine.GetRunner().StepOne();
        }

        Assert::IsTrue (rig.controller.GetKeyframes().GetArena().GetCount() > 6, L"several stretches, or the test proves nothing");

        middle = rig.controller.GetKeyframes().GetArena().GetInfo (4).position + 10;

        //  The reset comes after the first write, inside the first stretch.
        rig.Seek (start + 10);
        rig.target.ResetHeatMap();

        rig.Seek (middle);

        state = rig.target.LookUpHeatMapAccess (HeatSpace::Main, true, 0x2000, access);

        Assert::AreEqual ((int) HeatAccessState::None, (int) state, L"no write since the reset");
    }


    TEST_METHOD (ResetCountsFromWhereTheMachineStands)
    {
        HeatHistoryRig                             rig      (ReverseSessionRig::MakeSettings (KeyframeSettings::kDefaultFrames));
        std::vector<uint64_t>                      spread;
        std::set<uint64_t>                         wanted;
        std::map<uint64_t, std::vector<int64_t>>   straight;
        uint64_t                                   end      = 0;
        uint64_t                                   reset    = 0;



        rig.script.RunTo (rig.machine, UINT64_MAX);
        end    = rig.machine.GetRunner().GetPosition();
        spread = SpreadPositions (rig.controller, end);
        reset  = spread[spread.size() / 2];

        for (uint64_t position : spread)
        {
            wanted.insert (position);
        }

        wanted.insert (end);

        straight = CountStraight (wanted);

        //  Reset behind live, in the middle of a stretch.
        rig.Seek (reset);
        rig.target.ResetHeatMap();

        for (uint64_t position : spread)
        {
            rig.Seek (position);
            AssertSameTotalsOrNone (GetCountedSince (straight[position], straight[reset]), HeatHistoryChecks::GetShown (rig.GetMap()), position, reset);
        }
    }


    TEST_METHOD (ResetTotalsStayRightOnceHistoryDropsWhatCameBefore)
    {
        ReverseSettings                            settings = ReverseSessionRig::MakeSettings (1);
        std::vector<uint64_t>                      spread;
        std::set<uint64_t>                         wanted;
        std::map<uint64_t, std::vector<int64_t>>   straight;
        uint64_t                                   reset    = 0;
        uint64_t                                   end      = 0;



        settings.keyframes.wholeEvery   = 4;
        settings.keyframes.longestGroup = 4;
        settings.keyframes.budgetBytes  = 512 * 1024;

        {
            HeatHistoryRig  rig (settings);



            //  Reset live, a little way in, then record on until the budget
            //  has dropped history from before the reset.
            rig.script.RunTo (rig.machine, rig.machine.GetRunner().GetPosition() + 20000);
            reset = rig.machine.GetRunner().GetPosition();
            rig.target.ResetHeatMap();

            rig.script.RunTo (rig.machine, UINT64_MAX);
            end = rig.machine.GetRunner().GetPosition();

            Assert::IsTrue (rig.controller.GetSeeker().GetOldestPosition() > reset,
                            std::format (L"history dropped everything up to the reset at {}, or the test proves nothing: oldest {}, {} keyframes in {} bytes",
                                         reset,
                                         rig.controller.GetSeeker().GetOldestPosition(),
                                         rig.controller.GetKeyframes().GetArena().GetCount(),
                                         rig.controller.GetKeyframes().GetArena().GetByteCount()).c_str());

            spread = SpreadPositions (rig.controller, end);

            for (uint64_t position : spread)
            {
                wanted.insert (position);
            }

            wanted.insert (reset);
            wanted.insert (end);

            straight = CountStraight (wanted);

            HeatHistoryChecks::AssertSameTotals (GetCountedSince (straight[end], straight[reset]), HeatHistoryChecks::GetShown (rig.GetMap()), L"live, since the reset");

            for (size_t i = 0; i < spread.size(); i++)
            {
                uint64_t  at = spread[(i * HeatHistoryChecks::kShuffle) % spread.size()];



                rig.Seek (at);
                HeatHistoryChecks::AssertSameTotals (GetCountedSince (straight[at], straight[reset]), HeatHistoryChecks::GetShown (rig.GetMap()), std::format (L"seek to {} after the drop", at));
            }
        }
    }


    TEST_METHOD (AMapTurnedOnPartWayCountsFromThere)
    {
        HeatHistoryRig                             rig      (ReverseSessionRig::MakeSettings (KeyframeSettings::kDefaultFrames));
        std::vector<uint64_t>                      spread;
        std::set<uint64_t>                         wanted;
        std::map<uint64_t, std::vector<int64_t>>   straight;
        uint64_t                                   early    = 0;
        uint64_t                                   on       = 0;
        uint64_t                                   end      = 0;



        //  On for a while, so history keeps counts from then; off; and on
        //  again further on, where the counting starts over.
        rig.script.RunTo (rig.machine, rig.machine.GetRunner().GetPosition() + 60000);
        early = rig.machine.GetRunner().GetPosition() - 30000;

        rig.target.SetHeatMapOn (false);

        rig.script.RunTo (rig.machine, rig.machine.GetRunner().GetPosition() + 90000);
        on = rig.machine.GetRunner().GetPosition();

        rig.target.SetHeatMapOn (true);

        rig.script.RunTo (rig.machine, UINT64_MAX);
        end    = rig.machine.GetRunner().GetPosition();
        spread = SpreadPositions (rig.controller, end);

        for (uint64_t position : spread)
        {
            wanted.insert (position);
        }

        wanted.insert (early);
        wanted.insert (on);
        wanted.insert (on + HeatHistoryChecks::kRunOn);

        straight = CountStraight (wanted);

        //  Turned on again after power-on, the map cannot know which RAM was
        //  written while it was off, so it counts no read before written,
        //  where the straight map, on from power-on, counts every one.
        for (size_t i = 0; i < spread.size(); i++)
        {
            uint64_t  at = spread[(i * HeatHistoryChecks::kShuffle) % spread.size()];



            rig.Seek (at);
            AssertSameTotalsOrNone (WithoutUnwritten (GetCountedSince (straight[at], straight[on])), HeatHistoryChecks::GetShown (rig.GetMap()), at, on);
        }

        //  From before it was first turned off, a seek past where it came on
        //  again follows the counts kept from the first time it was on, which
        //  must count for nothing: the counts from where it came on, after.
        rig.Seek (early);
        AssertSameTotalsOrNone (WithoutUnwritten (GetCountedSince (straight[early], straight[on])), HeatHistoryChecks::GetShown (rig.GetMap()), early, on);

        rig.Seek (on + HeatHistoryChecks::kRunOn);
        HeatHistoryChecks::AssertSameTotals (WithoutUnwritten (GetCountedSince (straight[on + HeatHistoryChecks::kRunOn], straight[on])), HeatHistoryChecks::GetShown (rig.GetMap()), L"a seek from before it came on to after");
    }


    //  The map puts every access on the bus's slow path, so its being on
    //  or off must leave the machine's state alone: running on from the
    //  past across a stretch recorded with the map in the other state
    //  checks every keyframe it reaches, and a mismatch cuts history.
    TEST_METHOD (RunningOnAcrossAStretchRecordedWithTheMapOffKeepsHistory)
    {
        HeatHistoryRig  rig      (ReverseSessionRig::MakeSettings (KeyframeSettings::kDefaultFrames));
        uint64_t        early    = 0;
        uint64_t        on       = 0;
        uint64_t        end      = 0;
        uint64_t        through  = 0;



        rig.script.RunTo (rig.machine, rig.machine.GetRunner().GetPosition() + 60000);
        early = rig.machine.GetRunner().GetPosition() - 30000;

        rig.target.SetHeatMapOn (false);

        rig.script.RunTo (rig.machine, rig.machine.GetRunner().GetPosition() + 90000);
        on = rig.machine.GetRunner().GetPosition();

        rig.target.SetHeatMapOn (true);

        rig.script.RunTo (rig.machine, UINT64_MAX);
        end     = rig.machine.GetRunner().GetPosition();
        through = on + HeatHistoryChecks::kRunOn;

        Assert::AreEqual<uint64_t> (end, rig.controller.GetSeeker().GetLiveEndPosition(), L"history ends where the session did");

        //  With the map on, through the stretch recorded with it off.
        rig.Seek (early);

        while (rig.machine.GetRunner().GetPosition() < through && rig.controller.GetSpan().IsInHistory())
        {
            rig.machine.GetRunner().StepOne();
        }

        Assert::AreEqual<uint64_t> (end, rig.controller.GetSeeker().GetLiveEndPosition(), L"running on with the map on across the stretch recorded with it off cut history");
        Assert::IsTrue (rig.controller.GetSpan().IsInHistory(), L"still in history after running on with the map on");

        //  With the map off, through the stretches recorded with it on.
        rig.target.SetHeatMapOn (false);
        rig.Seek (early);

        while (rig.machine.GetRunner().GetPosition() < through && rig.controller.GetSpan().IsInHistory())
        {
            rig.machine.GetRunner().StepOne();
        }

        Assert::AreEqual<uint64_t> (end, rig.controller.GetSeeker().GetLiveEndPosition(), L"running on with the map off across the stretches recorded with it on cut history");
        Assert::IsTrue (rig.controller.GetSpan().IsInHistory(), L"still in history after running on with the map off");
    }


    TEST_METHOD (AnAnchorDroppedWithItsHistoryMovesOn)
    {
        ReverseSettings                            settings = ReverseSessionRig::MakeSettings (1);
        std::vector<uint64_t>                      spread;
        std::set<uint64_t>                         wanted;
        std::map<uint64_t, std::vector<int64_t>>   straight;
        uint64_t                                   end      = 0;
        size_t                                     checked  = 0;
        HRESULT                                    hr       = S_OK;



        settings.keyframes.wholeEvery   = 4;
        settings.keyframes.longestGroup = 4;

        {
            HeatHistoryRig  rig (settings);



            rig.script.RunTo (rig.machine, UINT64_MAX);
            end    = rig.machine.GetRunner().GetPosition();
            spread = SpreadPositions (rig.controller, end);

            for (uint64_t position : spread)
            {
                wanted.insert (position);
            }

            straight = CountStraight (wanted);

            //  Standing at an early keyframe, which a smaller budget drops.
            rig.Seek (spread.front());

            hr = rig.controller.GetKeyframes().ChangeBudget (512 * 1024);
            AssertSucceeded (hr, L"ChangeBudget");

            Assert::IsTrue (rig.controller.GetSeeker().GetOldestPosition() > spread.front(), L"the new budget dropped where the machine stands, or the test proves nothing");

            for (size_t i = 1; i < spread.size(); i++)
            {
                uint64_t  at = spread[(i * HeatHistoryChecks::kShuffle) % spread.size()];



                if (at < rig.controller.GetSeeker().GetOldestPosition())
                {
                    continue;
                }

                rig.Seek (at);
                HeatHistoryChecks::AssertSameTotals (straight[at], HeatHistoryChecks::GetShown (rig.GetMap()), std::format (L"seek to {} after the anchor was dropped", at));

                checked++;
            }

            Assert::IsTrue (checked > 0, L"history kept some of the positions to seek to");
        }
    }


    TEST_METHOD (TheRebuiltHeatIsTheHeatOfAStraightRun)
    {
        HeatHistoryRig                             rig      (ReverseSessionRig::MakeSettings (KeyframeSettings::kDefaultFrames));
        ScratchHeatReplayer                        replayer;
        HeatRebuildJob                             job;
        std::vector<HeatRebuildResult>             results;
        std::vector<uint64_t>                      spread;
        bool                                       hasWindow = false;
        HRESULT                                    hr        = S_OK;



        rig.script.RunTo (rig.machine, UINT64_MAX);
        spread = SpreadPositions (rig.controller, rig.machine.GetRunner().GetPosition());

        replayer.SetMachine (rig.machine.GetConfig(), rig.machine.GetCurrentMachineName());

        for (uint64_t at : { spread[1], spread[spread.size() - 2], spread[spread.size() / 2] })
        {
            rig.Seek (at);

            hr = rig.target.GetHeatHistory().MakeRebuildJob (job, hasWindow);
            AssertSucceeded (hr, L"MakeRebuildJob");
            Assert::IsTrue (hasWindow, L"history holds a window to replay");
            Assert::AreEqual ((size_t) 1, job.parts.size(), L"less than the newest part's seconds of history is one part");
            Assert::AreEqual<uint64_t> (rig.controller.GetSeeker().GetOldestPosition(), job.parts[0].startPosition, L"a fade time longer than history reaches back to its start");

            hr = replayer.Rebuild (job, results);
            AssertSucceeded (hr, L"Rebuild");
            Assert::AreEqual ((size_t) 1, results.size());
            Assert::IsTrue (results[0].isLast);

            AssertSameHeat (GetStraightHeat (at), results[0].heat, at);

            Logger::WriteMessage (std::format ("heat rebuilt at {}: {} instructions in {:.1f} ms\n", at, results[0].instructions, results[0].ms).c_str());
        }
    }


    //  A busy address stays warm for several fade times, so the rebuild
    //  reaches back further than one: with a fade time shorter than history,
    //  it still gives the heat of a straight run.
    TEST_METHOD (AFadeShorterThanHistoryStillGivesTheHeatOfAStraightRun)
    {
        constexpr double                           kFade    = 1.0;
        HeatHistoryRig                             rig      (ReverseSessionRig::MakeSettings (KeyframeSettings::kDefaultFrames));
        ScratchHeatReplayer                        replayer;
        HeatRebuildJob                             job;
        std::vector<HeatRebuildResult>             results;
        bool                                       hasWindow = false;
        uint64_t                                   at        = 0;
        HRESULT                                    hr        = S_OK;



        rig.script.RunTo (rig.machine, UINT64_MAX);
        at = rig.machine.GetRunner().GetPosition() - 1000;

        Assert::IsTrue ((double) rig.machine.GetCpu()->GetTotalCycles() > kFade * AccessHeatMap::kCyclesPerSecond, L"history outlasts the fade time, or the test proves nothing");

        replayer.SetMachine (rig.machine.GetConfig(), rig.machine.GetCurrentMachineName());
        rig.target.SetHeatMapFade (kFade);
        rig.Seek (at);

        hr = rig.target.GetHeatHistory().MakeRebuildJob (job, hasWindow);
        AssertSucceeded (hr, L"MakeRebuildJob");

        hr = replayer.Rebuild (job, results);
        AssertSucceeded (hr, L"Rebuild");
        Assert::AreEqual ((size_t) 1, results.size());

        AssertSameHeat (GetStraightHeat (at, HeatHistoryChecks::kSlices, kFade), results[0].heat, at);
    }


    //  A window longer than the newest part's seconds comes back in parts,
    //  the newest first and ending where the machine stands, each older one
    //  ending where the newer begins, and merged they are the heat of a
    //  straight run, to the rounding of fading in steps.
    TEST_METHOD (ARebuildInPartsAddsUpToAStraightRun)
    {
        constexpr size_t                           kSlices  = 1000;
        InlineWorkQueue                            queue;
        ScratchHeatReplayer                        replayer;
        HeatRebuildJob                             job;
        std::vector<HeatRebuildResult>             results;
        HeatScript                                 script   (kSlices);
        ReverseResult                              result;
        bool                                       hasWindow = false;
        uint64_t                                   at        = 0;
        HRESULT                                    hr        = S_OK;



        replayer.SetWorkQueue (&queue);

        {
            HeatHistoryRig  rig (ReverseSessionRig::MakeSettings (KeyframeSettings::kDefaultFrames), &replayer);



            replayer.SetMachine (rig.machine.GetConfig(), rig.machine.GetCurrentMachineName());

            script.RunTo (rig.machine, UINT64_MAX);
            at = rig.machine.GetRunner().GetPosition() - 1000;

            hr = rig.controller.GetSeeker().SeekToPosition (at, result);
            AssertSucceeded (hr, L"SeekToPosition");

            hr = rig.target.GetHeatHistory().MakeRebuildJob (job, hasWindow);
            AssertSucceeded (hr, L"MakeRebuildJob");
            Assert::IsTrue (job.parts.size() >= 2, L"more than two seconds of history comes in parts");

            hr = replayer.Rebuild (job, results);
            AssertSucceeded (hr, L"Rebuild");
            Assert::AreEqual (job.parts.size(), results.size(), L"a result for every part");
            Assert::AreEqual<uint64_t> (rig.machine.GetCpu()->GetTotalCycles(), results[0].cycle, L"the newest first, ending where the machine stands");

            for (size_t i = 1; i < results.size(); i++)
            {
                Assert::AreEqual<uint64_t> (job.parts[i - 1].startCycle, results[i].cycle, L"each older part ending where the newer begins");
                Assert::AreEqual (i + 1 == results.size(), results[i].isLast);
            }

            rig.target.NoteHistoryMoved (false);
            queue.WaitAll();

            HeatHistoryChecks::AssertNearHeat (GetStraightHeat (at, kSlices), rig.GetMap().GetHeatTable(), at);
        }
    }


    //  Not a pass or fail: what a rebuild of each fade time costs, written to
    //  the test log, over the reverse rig's busy guest loop.
    TEST_METHOD (RebuildCostIsLogged)
    {
        static constexpr int              kFades[]  = { 2, 10 };
        constexpr double                  kSpareSec = 0.5;
        HeatHistoryRig                    rig       (ReverseSessionRig::MakeSettings (KeyframeSettings::kDefaultFrames));
        ScratchHeatReplayer               replayer;
        HeatRebuildJob                    job;
        std::vector<HeatRebuildResult>    results;
        bool                              hasWindow = false;
        uint64_t                          cycles    = (uint64_t) ((std::ranges::max (kFades) + kSpareSec) * AccessHeatMap::kCyclesPerSecond);
        HRESULT                           hr        = S_OK;
        std::string                       log;



        rig.machine.GetRunner().RunCycles (cycles);

        replayer.SetMachine (rig.machine.GetConfig(), rig.machine.GetCurrentMachineName());

        log += std::format ("heat rebuild, {} build, {} keyframes, {} bytes of history, {} of them heat counts\n",
                            HeatHistoryChecks::IsDebugBuild() ? "Debug" : "Release",
                            rig.controller.GetKeyframes().GetArena().GetCount(),
                            rig.controller.GetKeyframes().GetArena().GetByteCount(),
                            rig.controller.GetKeyframes().GetArena().GetSideByteCount());

        for (int fade : kFades)
        {
            rig.target.SetHeatMapFade ((double) fade);

            hr = rig.target.GetHeatHistory().MakeRebuildJob (job, hasWindow);
            AssertSucceeded (hr, L"MakeRebuildJob");
            Assert::IsTrue (hasWindow);

            hr = replayer.Rebuild (job, results);
            AssertSucceeded (hr, L"Rebuild");

            log += DescribeRebuild (fade, job, results);
        }

        Logger::WriteMessage (log.c_str());
    }


    TEST_METHOD (AMoveClearsTheHeatAndTheRebuildRestoresIt)
    {
        InlineWorkQueue                            queue;
        ScratchHeatReplayer                        replayer;
        std::vector<uint64_t>                      spread;
        uint64_t                                   at       = 0;



        replayer.SetWorkQueue (&queue);

        {
            HeatHistoryRig  rig (ReverseSessionRig::MakeSettings (KeyframeSettings::kDefaultFrames), &replayer);



            replayer.SetMachine (rig.machine.GetConfig(), rig.machine.GetCurrentMachineName());

            rig.script.RunTo (rig.machine, UINT64_MAX);
            spread = SpreadPositions (rig.controller, rig.machine.GetRunner().GetPosition());
            at     = spread[spread.size() / 2];

            rig.Seek (at);

            Assert::IsTrue (rig.target.IsHeatMapRebuilding(),       L"after the move, the heat is being rebuilt");
            Assert::AreEqual ((size_t) 1, queue.GetPendingCount(), L"on the rebuilder's worker");
            Assert::IsTrue (IsCold (rig.GetMap()),                  L"and the heat from before the move is gone");

            queue.WaitAll();

            AssertSameHeat (GetStraightHeat (at), rig.GetMap().GetHeatTable(), at);
            Assert::IsFalse (rig.target.IsHeatMapRebuilding(), L"merged, nothing is awaited");
        }
    }


    TEST_METHOD (ADragRebuildsOnlyOnceLetGo)
    {
        InlineWorkQueue                            queue;
        ScratchHeatReplayer                        replayer;
        std::vector<uint64_t>                      spread;
        ReverseResult                              result;
        HRESULT                                    hr       = S_OK;



        replayer.SetWorkQueue (&queue);

        {
            HeatHistoryRig  rig (ReverseSessionRig::MakeSettings (KeyframeSettings::kDefaultFrames), &replayer);



            replayer.SetMachine (rig.machine.GetConfig(), rig.machine.GetCurrentMachineName());

            rig.script.RunTo (rig.machine, UINT64_MAX);
            spread = SpreadPositions (rig.controller, rig.machine.GetRunner().GetPosition());

            for (uint64_t position : spread)
            {
                hr = rig.controller.GetSeeker().SeekToPosition (position, result);
                AssertSucceeded (hr, L"SeekToPosition");

                rig.target.NoteHistoryMoved (true);

                Assert::AreEqual ((size_t) 0, queue.GetPendingCount(), L"no rebuild while the drag goes on");
                Assert::IsTrue   (rig.target.IsHeatMapRebuilding(), L"though the heat waits for one");
            }

            rig.target.NoteHistoryMoved (false);

            Assert::AreEqual ((size_t) 1, queue.GetPendingCount(), L"one rebuild, once let go");

            queue.WaitAll();

            AssertSameHeat (GetStraightHeat (spread.back()), rig.GetMap().GetHeatTable(), spread.back());
        }
    }


    TEST_METHOD (CumulativeModeRebuildsNothingUntilFadingIsChosen)
    {
        InlineWorkQueue                            queue;
        ScratchHeatReplayer                        replayer;
        std::vector<uint64_t>                      spread;



        replayer.SetWorkQueue (&queue);

        {
            HeatHistoryRig  rig (ReverseSessionRig::MakeSettings (KeyframeSettings::kDefaultFrames), &replayer);



            replayer.SetMachine (rig.machine.GetConfig(), rig.machine.GetCurrentMachineName());
            rig.target.SetHeatMapCumulative (true);

            rig.script.RunTo (rig.machine, UINT64_MAX);
            spread = SpreadPositions (rig.controller, rig.machine.GetRunner().GetPosition());

            rig.Seek (spread[2]);

            Assert::AreEqual ((size_t) 0, queue.GetPendingCount(), L"nothing rebuilt while cumulative");
            Assert::IsFalse  (rig.target.IsHeatMapRebuilding(), L"and nothing shown as rebuilding");

            rig.target.SetHeatMapCumulative (false);
            rig.GetMap();

            Assert::AreEqual ((size_t) 1, queue.GetPendingCount(), L"fading chosen: the heat is rebuilt");

            queue.WaitAll();

            AssertSameHeat (GetStraightHeat (spread[2]), rig.GetMap().GetHeatTable(), spread[2]);
        }
    }


private:

    //  Up to from, the shown totals are none; after it, the count
    //  since it.
    static void AssertSameTotalsOrNone (const std::vector<int64_t> & expected, const std::vector<int64_t> & actual, uint64_t at, uint64_t from)
    {
        std::vector<int64_t>  none (expected.size(), 0);



        if (at <= from)
        {
            Assert::IsTrue (actual == none, std::format (L"at {}, not after {}, nothing is shown", at, from).c_str());
            return;
        }

        HeatHistoryChecks::AssertSameTotals (expected, actual, std::format (L"at {}, counting from {}", at, from));
    }


    //  The heat of a map on from the session's start, folded at every frame
    //  boundary on a straight run to position, and once more there.
    static std::vector<float> GetStraightHeat (uint64_t position, size_t slices = HeatHistoryChecks::kSlices, double fadeSeconds = HeatMapOptions::kDefaultFadeSeconds)
    {
        TestMachine           machine ("Apple2e");
        MachineDebugTarget    target  (machine);
        HeatScript            script  (slices);
        AccessHeatMap       * map     = nullptr;



        ReverseSessionRig::Prepare (machine);
        target.SetHeatMapOn   (true);
        target.SetHeatMapFade (fadeSeconds);

        map = const_cast<AccessHeatMap *> (target.FoldHeatMap());
        Assert::IsNotNull (map);

        script.RunTo (machine, position, map);
        map->Fold (machine.GetCpu()->GetTotalCycles());

        return map->GetHeatTable();
    }


    static void AssertSameHeat (const std::vector<float> & expected, const std::vector<float> & actual, uint64_t at)
    {
        size_t  first = 0;
        size_t  warm  = 0;



        Assert::AreEqual (expected.size(), actual.size(), std::format (L"heat at {}: entries", at).c_str());

        while (first < expected.size() && expected[first] == actual[first])
        {
            first++;
        }

        for (float heat : expected)
        {
            warm += (heat > 0.0f) ? 1 : 0;
        }

        Assert::IsTrue (warm > 0, L"the straight run left no heat, so the comparison proves nothing");
        Assert::IsTrue (first == expected.size(),
                        std::format (L"heat at {}: kind {} address ${:04X} is {}, a straight run gives {}",
                                     at,
                                     first / AccessHeatMap::kAddressCount,
                                     first % AccessHeatMap::kAddressCount,
                                     (first < actual.size())   ? actual[first]   : -1.0f,
                                     (first < expected.size()) ? expected[first] : -1.0f).c_str());
    }


    static std::string DescribeRebuild (int fade, const HeatRebuildJob & job, const std::vector<HeatRebuildResult> & results)
    {
        std::string  text = std::format ("  {:2} s fade: {:.2f} s of machine time in {} parts;",
                                         fade,
                                         (double) (job.parts.front().endCycle - job.parts.back().startCycle) / AccessHeatMap::kCyclesPerSecond,
                                         job.parts.size());
        double       ms   = 0.0;



        for (const HeatRebuildResult & result : results)
        {
            ms   += result.ms;
            text += std::format (" {} instructions in {:.1f} ms ({:.1f} ms in);", result.instructions, result.ms, ms);
        }

        return text + "\n";
    }


    static bool IsCold (const AccessHeatMap & map)
    {
        for (float heat : map.GetHeatTable())
        {
            if (heat != 0.0f)
            {
                return false;
            }
        }

        return true;
    }
};
