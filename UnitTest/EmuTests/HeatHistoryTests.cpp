#include "Pch.h"

#include "EmuTests/InlineWorkQueue.h"
#include "EmuTests/ReverseSessionRig.h"
#include "Debugger/AccessHeatMap.h"
#include "Debugger/HeatAccessJump.h"
#include "Debugger/HeatCountDelta.h"
#include "Debugger/HeatHistory.h"
#include "Debugger/HeatKeyframeSide.h"
#include "Debugger/MachineDebugTarget.h"
#include "Shell/ScratchHeatReplayer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


static constexpr uint64_t  s_kHeatSliceCycles = 3000;
static constexpr size_t    s_kHeatSlices      = 600;
static constexpr size_t    s_kHeatFirstPart   = 360;
static constexpr size_t    s_kHeatSamples     = 9;
static constexpr size_t    s_kHeatShuffle     = 5;
static constexpr uint64_t  s_kHeatRunOn       = 2500;





////////////////////////////////////////////////////////////////////////////////
//
//  HeatScript
//
//  The reverse rig's scripted session, run so it can stop at any position
//  and carry on from there: each slice feeds the slice's inputs and runs its
//  cycles, as ReverseReplayTests' session does. With a map given, the map is
//  folded at the first instruction at or after every frame boundary, as a
//  heat rebuild folds it.
//
////////////////////////////////////////////////////////////////////////////////

class HeatScript
{
public:
    explicit HeatScript (size_t slices = s_kHeatSlices) : m_slices (slices) {}

    void RunTo (MachineHost & machine, uint64_t endPosition, AccessHeatMap * map = nullptr)
    {
        uint64_t  cycle    = 0;
        uint64_t  frameEnd = 0;



        while (m_slice < m_slices && machine.GetPosition() < endPosition)
        {
            if (!m_isFed)
            {
                ReverseSessionRig::Feed (machine, m_slice);

                m_isFed = true;
                m_spent = 0;
            }

            while (m_spent < s_kHeatSliceCycles && machine.GetPosition() < endPosition)
            {
                cycle    = machine.GetCpu()->GetTotalCycles();
                frameEnd = (cycle / AccessHeatMap::kCyclesPerFrame + 1) * AccessHeatMap::kCyclesPerFrame;

                m_spent += machine.StepOne();

                if (map != nullptr && machine.GetCpu()->GetTotalCycles() >= frameEnd)
                {
                    map->Fold (machine.GetCpu()->GetTotalCycles());
                }
            }

            if (m_spent >= s_kHeatSliceCycles)
            {
                m_slice++;
                m_isFed = false;
            }
        }
    }

    size_t GetSlice() const { return m_slice; }

private:
    size_t    m_slices = 0;
    size_t    m_slice  = 0;
    uint64_t  m_spent  = 0;
    bool      m_isFed  = false;
};





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

    //  A recorded //e running the reverse rig's session, its heat map on
    //  from where history begins and kept in that history.
    struct Rig
    {
        TestMachine          machine    { "Apple2e" };
        MachineDebugTarget   target     { machine };
        ReverseController    controller { machine };
        HeatScript           script;

        explicit Rig (const ReverseSettings & settings, IHeatRebuilder * rebuilder = nullptr)
        {
            HRESULT  hr = S_OK;



            ReverseSessionRig::Prepare (machine);

            hr = controller.Start (settings);
            AssertSucceeded (hr, L"Start");

            target.AttachHistory (&controller.GetKeyframes(), rebuilder);
            controller.SetHistoryObserver (target.GetHistoryObserver());
            target.SetHeatMapOn (true);
        }

        ~Rig()
        {
            controller.SetHistoryObserver (nullptr);
            target.AttachHistory (nullptr, nullptr);
        }

        const AccessHeatMap & GetMap()
        {
            const AccessHeatMap  * map = target.FoldHeatMap();



            Assert::IsNotNull (map, L"the map is on");

            return *map;
        }

        //  A move as the shell makes one: the command, then the map told.
        void Seek (uint64_t position)
        {
            ReverseResult  result;
            HRESULT        hr     = S_OK;



            hr = controller.SeekToPosition (position, result);
            AssertSucceeded (hr, L"SeekToPosition");
            Assert::AreEqual<uint64_t> (position, machine.GetPosition(), L"the seek lands where asked");

            target.NoteHistoryMoved (false);
        }
    };


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
            Assert::AreEqual<uint64_t> (position, machine.GetPosition(), L"the straight run reaches the position");

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
            Assert::AreEqual<uint64_t> (position, machine.GetPosition(), L"the straight run reaches the position");

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

    //  The totals the map shows: none below zero.
    static std::vector<int64_t> GetShown (const AccessHeatMap & map)
    {
        std::vector<int64_t>  totals;



        map.GetTotalsNow (totals);

        for (int64_t & total : totals)
        {
            total = std::max<int64_t> (total, 0);
        }

        return totals;
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


    static void AssertSameTotals (const std::vector<int64_t> & expected, const std::vector<int64_t> & actual, const std::wstring & where)
    {
        size_t  first   = 0;
        size_t  touched = 0;



        Assert::AreEqual (expected.size(), actual.size(), where.c_str());

        while (first < expected.size() && expected[first] == actual[first])
        {
            first++;
        }

        for (int64_t total : expected)
        {
            touched += (total > 0) ? 1 : 0;
        }

        Assert::IsTrue (first == expected.size(),
                        std::format (L"{}: kind {} address ${:04X} counted {}, a straight count gives {}",
                                     where,
                                     first / AccessHeatMap::kAddressCount,
                                     first % AccessHeatMap::kAddressCount,
                                     (first < actual.size())   ? actual[first]   : -1,
                                     (first < expected.size()) ? expected[first] : -1).c_str());

        Assert::IsTrue (touched > 0, (where + L": the straight count touched nothing, so the comparison proves nothing").c_str());
    }


    //  Positions spread over the history held, oldest to newest.
    static std::vector<uint64_t> SpreadPositions (const ReverseController & controller, uint64_t liveEnd)
    {
        uint64_t               oldest = controller.GetOldestPosition();
        std::vector<uint64_t>  spread;



        for (size_t i = 1; i <= s_kHeatSamples; i++)
        {
            spread.push_back (oldest + (liveEnd - oldest) * i / (s_kHeatSamples + 1));
        }

        return spread;
    }



    TEST_METHOD (CumulativeTotalsFollowEverySeekStepAndRunOn)
    {
        Rig                                        rig      (ReverseSessionRig::MakeSettings (KeyframeSettings::kDefaultFrames));
        std::vector<uint64_t>                      spread;
        std::set<uint64_t>                         wanted;
        std::map<uint64_t, std::vector<int64_t>>   straight;
        ReverseResult                              result;
        uint64_t                                   firstEnd = 0;
        uint64_t                                   at       = 0;
        size_t                                     i        = 0;
        HRESULT                                    hr       = S_OK;



        rig.script.RunTo (rig.machine, UINT64_MAX);
        Assert::AreEqual (s_kHeatSlices, rig.script.GetSlice(), L"the whole session ran");

        firstEnd = rig.machine.GetPosition();
        spread   = SpreadPositions (rig.controller, firstEnd);

        for (uint64_t position : spread)
        {
            wanted.insert (position);
            wanted.insert (position - 1);
            wanted.insert (position + 1);
            wanted.insert (position + s_kHeatRunOn);
        }

        wanted.insert (firstEnd);

        straight = CountStraight (wanted);

        AssertSameTotals (straight[firstEnd], GetShown (rig.GetMap()), L"live, before any move");

        //  Seeks back and forward, scattered.
        for (i = 0; i < spread.size(); i++)
        {
            at = spread[(i * s_kHeatShuffle) % spread.size()];

            rig.Seek (at);
            AssertSameTotals (straight[at], GetShown (rig.GetMap()), std::format (L"seek to {}", at));
        }

        //  Step back, then step forward twice, then run on from the past.
        for (uint64_t position : spread)
        {
            rig.Seek (position);

            hr = rig.controller.StepBack (result);
            AssertSucceeded (hr, L"StepBack");
            rig.target.NoteHistoryMoved (false);
            AssertSameTotals (straight[position - 1], GetShown (rig.GetMap()), std::format (L"step back from {}", position));

            hr = rig.controller.StepForward (result);
            AssertSucceeded (hr, L"StepForward");
            hr = rig.controller.StepForward (result);
            AssertSucceeded (hr, L"StepForward");
            rig.target.NoteHistoryMoved (false);
            AssertSameTotals (straight[position + 1], GetShown (rig.GetMap()), std::format (L"two steps forward from {}", position - 1));

            while (rig.machine.GetPosition() < position + s_kHeatRunOn)
            {
                rig.machine.StepOne();
            }

            AssertSameTotals (straight[position + s_kHeatRunOn], GetShown (rig.GetMap()), std::format (L"running on from {}", position + 1));
        }

        //  Back to the live end, and the live end again after a seek back.
        hr = rig.controller.SeekToCycle (UINT64_MAX, result);
        AssertSucceeded (hr, L"SeekToCycle live");
        rig.target.NoteHistoryMoved (false);

        Assert::IsFalse (rig.controller.IsInHistory(), L"live again");
        AssertSameTotals (straight[firstEnd], GetShown (rig.GetMap()), L"back at the live end");
    }


    //  The instruction that last wrote and last read an address, in every
    //  space, follows the machine as the totals do: after any seek, step or
    //  run on, it is the one a straight run to the same position saw, the
    //  map's own record where it holds one as of there and history's,
    //  looked up by replaying a stretch, where it does not.
    TEST_METHOD (LastAccessesFollowEverySeekStepAndRunOn)
    {
        Rig                                                    rig      (ReverseSessionRig::MakeSettings (KeyframeSettings::kDefaultFrames));
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
        spread = SpreadPositions (rig.controller, rig.machine.GetPosition());
        sample = PickLastSample (rig.GetMap());

        for (uint64_t position : spread)
        {
            wanted.insert (position);
            wanted.insert (position - 1);
            wanted.insert (position + s_kHeatRunOn);
        }

        straight = GetStraightLast (wanted, sample);

        for (size_t i = 0; i < spread.size(); i++)
        {
            uint64_t  at = spread[(i * s_kHeatShuffle) % spread.size()];



            rig.Seek (at);
            AssertSameLast (straight[at], LookUpSample (rig.target, sample), std::format (L"seek to {}", at));
        }

        for (size_t i = 0; i < spread.size(); i += 3)
        {
            uint64_t  position = spread[i];



            rig.Seek (position);

            hr = rig.controller.StepBack (result);
            AssertSucceeded (hr, L"StepBack");
            rig.target.NoteHistoryMoved (false);
            AssertSameLast (straight[position - 1], LookUpSample (rig.target, sample), std::format (L"step back from {}", position));

            while (rig.machine.GetPosition() < position + s_kHeatRunOn)
            {
                rig.machine.StepOne();
            }

            AssertSameLast (straight[position + s_kHeatRunOn], LookUpSample (rig.target, sample), std::format (L"running on from {}", position - 1));
        }
    }

    //  Going back to an address's last write lands just after the
    //  instruction that made it: the write is the last there, the step back
    //  before it is that instruction, and there the write has not happened.
    TEST_METHOD (GoingBackToALastWriteLandsJustAfterIt)
    {
        Rig                             rig      (ReverseSessionRig::MakeSettings (KeyframeSettings::kDefaultFrames));
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
            if (rig.GetMap().GetLastAccess (HeatSpace::Main, true, (Word) address, access) == HeatAccessState::Found && access.GetPosition() > rig.controller.GetOldestPosition() + s_kHeatRunOn)
            {
                written = (Word) address;
            }
        }

        Assert::IsTrue (written.has_value(), L"something was written inside history, or the test proves nothing");

        request.isRewind = true;
        request.isWrite  = true;
        request.bank     = HeatMapOptions::Bank::Main;
        request.address  = written.value_or (0);

        plan = HeatAccessJump::Plan (request, HeatAccessState::Found, access, rig.controller.IsRecording(), rig.controller.GetOldestPosition());
        Assert::AreEqual ((int) HeatAccessPlan::Kind::Seek, (int) plan.kind);

        rig.Seek (plan.position);

        Assert::AreEqual<uint64_t> (access.GetPosition() + 1, rig.machine.GetPosition(), L"just after the writer");
        Assert::AreEqual ((int) HeatAccessState::Found, (int) rig.target.LookUpHeatMapAccess (HeatSpace::Main, true, request.address, there), L"the write is there");
        Assert::IsTrue (there == access, L"and it is the last");

        hr = rig.controller.StepBack (result);
        AssertSucceeded (hr, L"StepBack");
        rig.target.NoteHistoryMoved (false);

        Assert::AreEqual (access.GetPc(), rig.target.GetRegisters().pc, L"a step back is the writer");
        (void) rig.target.LookUpHeatMapAccess (HeatSpace::Main, true, request.address, there);
        Assert::IsFalse (there == access, L"before it ran, the write has not happened");
    }


    TEST_METHOD (CumulativeTotalsHoldAcrossHistoryRecordedAfterAMove)
    {
        Rig                                        rig      (ReverseSessionRig::MakeSettings (KeyframeSettings::kDefaultFrames));
        std::set<uint64_t>                         wanted;
        std::map<uint64_t, std::vector<int64_t>>   straight;
        ReverseResult                              result;
        uint64_t                                   midway   = 0;
        uint64_t                                   end      = 0;
        HRESULT                                    hr       = S_OK;



        //  Part of the session, a seek back, then on to the live end and the
        //  rest of the session, recorded after the move.
        rig.script.RunTo (rig.machine, UINT64_MAX);
        end = rig.machine.GetPosition();

        midway = rig.controller.GetOldestPosition() + (end - rig.controller.GetOldestPosition()) / 2;

        rig.Seek (midway);

        hr = rig.controller.SeekToCycle (UINT64_MAX, result);
        AssertSucceeded (hr, L"SeekToCycle live");
        rig.target.NoteHistoryMoved (false);

        wanted.insert (midway);
        wanted.insert (end);

        straight = CountStraight (wanted);

        AssertSameTotals (straight[end], GetShown (rig.GetMap()), L"live after going back and forth");

        rig.Seek (midway);
        AssertSameTotals (straight[midway], GetShown (rig.GetMap()), L"midway again");
    }


    TEST_METHOD (ResetCountsFromWhereTheMachineStands)
    {
        Rig                                        rig      (ReverseSessionRig::MakeSettings (KeyframeSettings::kDefaultFrames));
        std::vector<uint64_t>                      spread;
        std::set<uint64_t>                         wanted;
        std::map<uint64_t, std::vector<int64_t>>   straight;
        uint64_t                                   end      = 0;
        uint64_t                                   reset    = 0;



        rig.script.RunTo (rig.machine, UINT64_MAX);
        end    = rig.machine.GetPosition();
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
            AssertSameTotalsOrNone (GetCountedSince (straight[position], straight[reset]), GetShown (rig.GetMap()), position, reset);
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
            Rig  rig (settings);



            //  Reset live, a little way in, then record on until the budget
            //  has dropped history from before the reset.
            rig.script.RunTo (rig.machine, rig.machine.GetPosition() + 20000);
            reset = rig.machine.GetPosition();
            rig.target.ResetHeatMap();

            rig.script.RunTo (rig.machine, UINT64_MAX);
            end = rig.machine.GetPosition();

            Assert::IsTrue (rig.controller.GetOldestPosition() > reset,
                            std::format (L"history dropped everything up to the reset at {}, or the test proves nothing: oldest {}, {} keyframes in {} bytes",
                                         reset,
                                         rig.controller.GetOldestPosition(),
                                         rig.controller.GetKeyframes().GetCount(),
                                         rig.controller.GetKeyframes().GetByteCount()).c_str());

            spread = SpreadPositions (rig.controller, end);

            for (uint64_t position : spread)
            {
                wanted.insert (position);
            }

            wanted.insert (reset);
            wanted.insert (end);

            straight = CountStraight (wanted);

            AssertSameTotals (GetCountedSince (straight[end], straight[reset]), GetShown (rig.GetMap()), L"live, since the reset");

            for (size_t i = 0; i < spread.size(); i++)
            {
                uint64_t  at = spread[(i * s_kHeatShuffle) % spread.size()];



                rig.Seek (at);
                AssertSameTotals (GetCountedSince (straight[at], straight[reset]), GetShown (rig.GetMap()), std::format (L"seek to {} after the drop", at));
            }
        }
    }


    TEST_METHOD (AMapTurnedOnPartWayCountsFromThere)
    {
        Rig                                        rig      (ReverseSessionRig::MakeSettings (KeyframeSettings::kDefaultFrames));
        std::vector<uint64_t>                      spread;
        std::set<uint64_t>                         wanted;
        std::map<uint64_t, std::vector<int64_t>>   straight;
        uint64_t                                   early    = 0;
        uint64_t                                   on       = 0;
        uint64_t                                   end      = 0;



        //  On for a while, so history keeps counts from then; off; and on
        //  again further on, where the counting starts over.
        rig.script.RunTo (rig.machine, rig.machine.GetPosition() + 60000);
        early = rig.machine.GetPosition() - 30000;

        rig.target.SetHeatMapOn (false);

        rig.script.RunTo (rig.machine, rig.machine.GetPosition() + 90000);
        on = rig.machine.GetPosition();

        rig.target.SetHeatMapOn (true);

        rig.script.RunTo (rig.machine, UINT64_MAX);
        end    = rig.machine.GetPosition();
        spread = SpreadPositions (rig.controller, end);

        for (uint64_t position : spread)
        {
            wanted.insert (position);
        }

        wanted.insert (early);
        wanted.insert (on);
        wanted.insert (on + s_kHeatRunOn);

        straight = CountStraight (wanted);

        //  Turned on again after power-on, the map cannot know which RAM was
        //  written while it was off, so it counts no read before written,
        //  where the straight map, on from power-on, counts every one.
        for (size_t i = 0; i < spread.size(); i++)
        {
            uint64_t  at = spread[(i * s_kHeatShuffle) % spread.size()];



            rig.Seek (at);
            AssertSameTotalsOrNone (WithoutUnwritten (GetCountedSince (straight[at], straight[on])), GetShown (rig.GetMap()), at, on);
        }

        //  From before it was first turned off, a seek past where it came on
        //  again follows the counts kept from the first time it was on, which
        //  must count for nothing: the counts from where it came on, after.
        rig.Seek (early);
        AssertSameTotalsOrNone (WithoutUnwritten (GetCountedSince (straight[early], straight[on])), GetShown (rig.GetMap()), early, on);

        rig.Seek (on + s_kHeatRunOn);
        AssertSameTotals (WithoutUnwritten (GetCountedSince (straight[on + s_kHeatRunOn], straight[on])), GetShown (rig.GetMap()), L"a seek from before it came on to after");
    }


    //  The map puts every access on the bus's slow path, so its being on
    //  or off must leave the machine's state alone: running on from the
    //  past across a stretch recorded with the map in the other state
    //  checks every keyframe it reaches, and a mismatch cuts history.
    TEST_METHOD (RunningOnAcrossAStretchRecordedWithTheMapOffKeepsHistory)
    {
        Rig        rig      (ReverseSessionRig::MakeSettings (KeyframeSettings::kDefaultFrames));
        uint64_t   early    = 0;
        uint64_t   on       = 0;
        uint64_t   end      = 0;
        uint64_t   through  = 0;



        rig.script.RunTo (rig.machine, rig.machine.GetPosition() + 60000);
        early = rig.machine.GetPosition() - 30000;

        rig.target.SetHeatMapOn (false);

        rig.script.RunTo (rig.machine, rig.machine.GetPosition() + 90000);
        on = rig.machine.GetPosition();

        rig.target.SetHeatMapOn (true);

        rig.script.RunTo (rig.machine, UINT64_MAX);
        end     = rig.machine.GetPosition();
        through = on + s_kHeatRunOn;

        Assert::AreEqual<uint64_t> (end, rig.controller.GetLiveEndPosition(), L"history ends where the session did");

        //  With the map on, through the stretch recorded with it off.
        rig.Seek (early);

        while (rig.machine.GetPosition() < through && rig.controller.IsInHistory())
        {
            rig.machine.StepOne();
        }

        Assert::AreEqual<uint64_t> (end, rig.controller.GetLiveEndPosition(), L"running on with the map on across the stretch recorded with it off cut history");
        Assert::IsTrue (rig.controller.IsInHistory(), L"still in history after running on with the map on");

        //  With the map off, through the stretches recorded with it on.
        rig.target.SetHeatMapOn (false);
        rig.Seek (early);

        while (rig.machine.GetPosition() < through && rig.controller.IsInHistory())
        {
            rig.machine.StepOne();
        }

        Assert::AreEqual<uint64_t> (end, rig.controller.GetLiveEndPosition(), L"running on with the map off across the stretches recorded with it on cut history");
        Assert::IsTrue (rig.controller.IsInHistory(), L"still in history after running on with the map off");
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
            Rig  rig (settings);



            rig.script.RunTo (rig.machine, UINT64_MAX);
            end    = rig.machine.GetPosition();
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

            Assert::IsTrue (rig.controller.GetOldestPosition() > spread.front(), L"the new budget dropped where the machine stands, or the test proves nothing");

            for (size_t i = 1; i < spread.size(); i++)
            {
                uint64_t  at = spread[(i * s_kHeatShuffle) % spread.size()];



                if (at < rig.controller.GetOldestPosition())
                {
                    continue;
                }

                rig.Seek (at);
                AssertSameTotals (straight[at], GetShown (rig.GetMap()), std::format (L"seek to {} after the anchor was dropped", at));

                checked++;
            }

            Assert::IsTrue (checked > 0, L"history kept some of the positions to seek to");
        }
    }


    TEST_METHOD (TheRebuiltHeatIsTheHeatOfAStraightRun)
    {
        Rig                                        rig      (ReverseSessionRig::MakeSettings (KeyframeSettings::kDefaultFrames));
        ScratchHeatReplayer                        replayer;
        HeatRebuildJob                             job;
        std::vector<HeatRebuildResult>             results;
        std::vector<uint64_t>                      spread;
        bool                                       hasWindow = false;
        HRESULT                                    hr        = S_OK;



        rig.script.RunTo (rig.machine, UINT64_MAX);
        spread = SpreadPositions (rig.controller, rig.machine.GetPosition());

        replayer.SetMachine (rig.machine.GetConfig(), rig.machine.GetCurrentMachineName());

        for (uint64_t at : { spread[1], spread[spread.size() - 2], spread[spread.size() / 2] })
        {
            rig.Seek (at);

            hr = rig.target.GetHeatHistory().MakeRebuildJob (job, hasWindow);
            AssertSucceeded (hr, L"MakeRebuildJob");
            Assert::IsTrue (hasWindow, L"history holds a window to replay");
            Assert::AreEqual ((size_t) 1, job.parts.size(), L"less than the newest part's seconds of history is one part");
            Assert::AreEqual<uint64_t> (rig.controller.GetOldestPosition(), job.parts[0].startPosition, L"a fade time longer than history reaches back to its start");

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
        Rig                                        rig      (ReverseSessionRig::MakeSettings (KeyframeSettings::kDefaultFrames));
        ScratchHeatReplayer                        replayer;
        HeatRebuildJob                             job;
        std::vector<HeatRebuildResult>             results;
        bool                                       hasWindow = false;
        uint64_t                                   at        = 0;
        HRESULT                                    hr        = S_OK;



        rig.script.RunTo (rig.machine, UINT64_MAX);
        at = rig.machine.GetPosition() - 1000;

        Assert::IsTrue ((double) rig.machine.GetCpu()->GetTotalCycles() > kFade * AccessHeatMap::kCyclesPerSecond, L"history outlasts the fade time, or the test proves nothing");

        replayer.SetMachine (rig.machine.GetConfig(), rig.machine.GetCurrentMachineName());
        rig.target.SetHeatMapFade (kFade);
        rig.Seek (at);

        hr = rig.target.GetHeatHistory().MakeRebuildJob (job, hasWindow);
        AssertSucceeded (hr, L"MakeRebuildJob");

        hr = replayer.Rebuild (job, results);
        AssertSucceeded (hr, L"Rebuild");
        Assert::AreEqual ((size_t) 1, results.size());

        AssertSameHeat (GetStraightHeat (at, s_kHeatSlices, kFade), results[0].heat, at);
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
            Rig  rig (ReverseSessionRig::MakeSettings (KeyframeSettings::kDefaultFrames), &replayer);



            replayer.SetMachine (rig.machine.GetConfig(), rig.machine.GetCurrentMachineName());

            script.RunTo (rig.machine, UINT64_MAX);
            at = rig.machine.GetPosition() - 1000;

            hr = rig.controller.SeekToPosition (at, result);
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

            AssertNearHeat (GetStraightHeat (at, kSlices), rig.GetMap().GetHeatTable(), at);
        }
    }


    //  Not a pass or fail: what a rebuild of each fade time costs, written to
    //  the test log, over the reverse rig's busy guest loop.
    TEST_METHOD (RebuildCostIsLogged)
    {
        static constexpr int              kFades[]  = { 2, 10 };
        constexpr double                  kSpareSec = 0.5;
        Rig                               rig       (ReverseSessionRig::MakeSettings (KeyframeSettings::kDefaultFrames));
        ScratchHeatReplayer               replayer;
        HeatRebuildJob                    job;
        std::vector<HeatRebuildResult>    results;
        bool                              hasWindow = false;
        uint64_t                          cycles    = (uint64_t) ((std::ranges::max (kFades) + kSpareSec) * AccessHeatMap::kCyclesPerSecond);
        HRESULT                           hr        = S_OK;
        std::string                       log;



        rig.machine.RunCycles (cycles);

        replayer.SetMachine (rig.machine.GetConfig(), rig.machine.GetCurrentMachineName());

        log += std::format ("heat rebuild, {} build, {} keyframes, {} bytes of history, {} of them heat counts\n",
                            IsDebugBuild() ? "Debug" : "Release",
                            rig.controller.GetKeyframes().GetCount(),
                            rig.controller.GetKeyframes().GetByteCount(),
                            rig.controller.GetKeyframes().GetSideByteCount());

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
            Rig  rig (ReverseSessionRig::MakeSettings (KeyframeSettings::kDefaultFrames), &replayer);



            replayer.SetMachine (rig.machine.GetConfig(), rig.machine.GetCurrentMachineName());

            rig.script.RunTo (rig.machine, UINT64_MAX);
            spread = SpreadPositions (rig.controller, rig.machine.GetPosition());
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
            Rig  rig (ReverseSessionRig::MakeSettings (KeyframeSettings::kDefaultFrames), &replayer);



            replayer.SetMachine (rig.machine.GetConfig(), rig.machine.GetCurrentMachineName());

            rig.script.RunTo (rig.machine, UINT64_MAX);
            spread = SpreadPositions (rig.controller, rig.machine.GetPosition());

            for (uint64_t position : spread)
            {
                hr = rig.controller.SeekToPosition (position, result);
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
            Rig  rig (ReverseSessionRig::MakeSettings (KeyframeSettings::kDefaultFrames), &replayer);



            replayer.SetMachine (rig.machine.GetConfig(), rig.machine.GetCurrentMachineName());
            rig.target.SetHeatMapCumulative (true);

            rig.script.RunTo (rig.machine, UINT64_MAX);
            spread = SpreadPositions (rig.controller, rig.machine.GetPosition());

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

        AssertSameTotals (expected, actual, std::format (L"at {}, counting from {}", at, from));
    }


    //  The heat of a map on from the session's start, folded at every frame
    //  boundary on a straight run to position, and once more there.
    static std::vector<float> GetStraightHeat (uint64_t position, size_t slices = s_kHeatSlices, double fadeSeconds = HeatMapOptions::kDefaultFadeSeconds)
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


    //  Heat merged from parts fades in steps of a part rather than of a
    //  frame, which rounds differently; and a heat that went cold at a fold
    //  in a straight run can linger below the cold line in a merge. Neither
    //  shows on the map.
    static void AssertNearHeat (const std::vector<float> & expected, const std::vector<float> & actual, uint64_t at)
    {
        constexpr float  kRelative = 1e-4f;
        size_t           first     = 0;
        size_t           warm      = 0;
        float            allowed   = 0.0f;



        Assert::AreEqual (expected.size(), actual.size(), std::format (L"heat at {}: entries", at).c_str());

        for (first = 0; first < expected.size(); first++)
        {
            allowed = std::max (expected[first] * kRelative, AccessHeatMap::kColdHeat);

            if (std::fabs (expected[first] - actual[first]) > allowed)
            {
                break;
            }
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


    static bool IsDebugBuild()
    {
#ifdef _DEBUG
        return true;
#else
        return false;
#endif
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


    //  A //e, power cycled, running a guest that reads each byte of $4000
    //  to $BFFF before it writes it back, a page at a time, then starts over
    //  at $4000, where every byte is written: reads before written come
    //  steadily for the first 132,000 or so instructions and never again.
    //  Its stores put back what was there, so they change nothing, where its
    //  INCs of its own operands change them.
    static constexpr uint64_t  kFillEnd   = 200000;
    static constexpr uint64_t  kFillPhase = 135000;

    static void PrepareFill (TestMachine & machine)
    {
        static constexpr Byte  kFill[] =
        {
            0xA0, 0x00,             // 0300  LDY #$00
            0xB9, 0x00, 0x40,       // 0302  LDA $4000,Y
            0x99, 0x00, 0x40,       // 0305  STA $4000,Y
            0xC8,                   // 0308  INY
            0xD0, 0xF7,             // 0309  BNE $0302
            0xEE, 0x04, 0x03,       // 030B  INC $0304
            0xEE, 0x07, 0x03,       // 030E  INC $0307
            0xAD, 0x04, 0x03,       // 0311  LDA $0304
            0xC9, 0xC0,             // 0314  CMP #$C0
            0xD0, 0xE8,             // 0316  BNE $0300
            0xA9, 0x40,             // 0318  LDA #$40
            0x8D, 0x04, 0x03,       // 031A  STA $0304
            0x8D, 0x07, 0x03,       // 031D  STA $0307
            0x4C, 0x00, 0x03,       // 0320  JMP $0300
        };
        constexpr Word         kStart  = 0x0300;
        Word                   at      = kStart;



        machine.PowerCycle();

        for (Byte b : kFill)
        {
            machine.GetMemoryBus().WriteByte (at++, b);
        }

        machine.GetCpu()->SetPC (kStart);
    }


    //  Steps the guest to position, folding the map, when one is given, at
    //  the first instruction at or after every frame boundary.
    static void RunFillTo (MachineHost & machine, uint64_t position, AccessHeatMap * map = nullptr)
    {
        uint64_t  frameEnd = 0;



        while (machine.GetPosition() < position)
        {
            frameEnd = (machine.GetCpu()->GetTotalCycles() / AccessHeatMap::kCyclesPerFrame + 1) * AccessHeatMap::kCyclesPerFrame;

            machine.StepOne();

            if (map != nullptr && machine.GetCpu()->GetTotalCycles() >= frameEnd)
            {
                map->Fold (machine.GetCpu()->GetTotalCycles());
            }
        }
    }


    //  The fill guest recorded with a keyframe every frame, its map on from
    //  power-on, so it knows which RAM is written.
    struct FillRig
    {
        TestMachine          machine    { "Apple2e" };
        MachineDebugTarget   target     { machine };
        ReverseController    controller { machine };

        explicit FillRig (IHeatRebuilder * rebuilder = nullptr)
        {
            HRESULT  hr = S_OK;



            PrepareFill (machine);

            hr = controller.Start (ReverseSessionRig::MakeSettings (1));
            AssertSucceeded (hr, L"Start");

            target.AttachHistory (&controller.GetKeyframes(), rebuilder);
            controller.SetHistoryObserver (target.GetHistoryObserver());
            target.SetHeatMapOn (true);
        }

        ~FillRig()
        {
            controller.SetHistoryObserver (nullptr);
            target.AttachHistory (nullptr, nullptr);
        }

        const AccessHeatMap & GetMap()
        {
            const AccessHeatMap  * map = target.FoldHeatMap();



            Assert::IsNotNull (map, L"the map is on");

            return *map;
        }

        void Seek (uint64_t position)
        {
            ReverseResult  result;
            HRESULT        hr     = S_OK;



            hr = controller.SeekToPosition (position, result);
            AssertSucceeded (hr, L"SeekToPosition");
            target.NoteHistoryMoved (false);
        }
    };


    //  The totals a second machine running the fill guest from power-on, its
    //  map on, holds at each position.
    static std::map<uint64_t, std::vector<int64_t>> CountFillStraight (const std::set<uint64_t> & positions)
    {
        TestMachine                                machine ("Apple2e");
        MachineDebugTarget                         target  (machine);
        std::map<uint64_t, std::vector<int64_t>>   totals;



        PrepareFill (machine);
        target.SetHeatMapOn (true);

        for (uint64_t position : positions)
        {
            RunFillTo (machine, position);
            Assert::IsNotNull (target.FoldHeatMap());
            target.FoldHeatMap()->GetTotalsNow (totals[position]);
        }

        return totals;
    }


    //  How many reads before written a space's totals hold.
    static int64_t SumUnwritten (const std::vector<int64_t> & totals, HeatSpace space)
    {
        size_t   first = AccessHeatMap::GetIndex (space, HeatKind::UnwrittenRead, 0);
        int64_t  sum   = 0;



        for (size_t i = first; i < first + AccessHeatMap::kAddressCount; i++)
        {
            sum += totals[i];
        }

        return sum;
    }


    //  Which RAM was written follows the machine through history as the
    //  counts do: after a seek back to before a byte was first written, its
    //  read counts as a read before written again, exactly as a straight
    //  run from power-on counts it, and a seek to after counts nothing more.
    //  The writes that changed their byte follow the same way.
    TEST_METHOD (ReadsBeforeWrittenFollowEverySeekStepAndRunOn)
    {
        FillRig                                    rig;
        std::vector<uint64_t>                      stops    = { 4000, 30000, 60000, 90000, 120000, 160000, 190000 };
        std::set<uint64_t>                         wanted;
        std::map<uint64_t, std::vector<int64_t>>   straight;
        ReverseResult                              result;
        HRESULT                                    hr       = S_OK;



        RunFillTo (rig.machine, kFillEnd);

        for (uint64_t position : stops)
        {
            wanted.insert (position);
            wanted.insert (position - 1);
            wanted.insert (position + s_kHeatRunOn);
        }

        wanted.insert (kFillPhase);
        wanted.insert (kFillEnd);

        straight = CountFillStraight (wanted);

        Assert::IsTrue (SumUnwritten (straight[60000], HeatSpace::Main) > 0, L"the guest read RAM before writing it by then, or the test proves nothing");
        Assert::AreEqual (SumUnwritten (straight[kFillPhase], HeatSpace::Cpu), SumUnwritten (straight[kFillEnd], HeatSpace::Cpu), L"and none after the first pass");

        AssertSameTotals (straight[kFillEnd], GetShown (rig.GetMap()), L"live");

        //  Seeks scattered, back from the live end and forward again.
        for (size_t i = 0; i < stops.size(); i++)
        {
            uint64_t  at = stops[(i * s_kHeatShuffle) % stops.size()];



            rig.Seek (at);
            AssertSameTotals (straight[at], GetShown (rig.GetMap()), std::format (L"seek to {}", at));
        }

        //  A step back, then running on from the past.
        for (uint64_t position : stops)
        {
            rig.Seek (position);

            hr = rig.controller.StepBack (result);
            AssertSucceeded (hr, L"StepBack");
            rig.target.NoteHistoryMoved (false);
            AssertSameTotals (straight[position - 1], GetShown (rig.GetMap()), std::format (L"step back from {}", position));

            RunFillTo (rig.machine, position + s_kHeatRunOn);
            AssertSameTotals (straight[position + s_kHeatRunOn], GetShown (rig.GetMap()), std::format (L"running on from {}", position - 1));
        }
    }


    //  A rebuild's replay starts from the written bits history kept at its
    //  keyframe, so the reads before written it heats are the ones a
    //  straight run from power-on heats. The fade is short, so the window
    //  starts well after power-on, where the bits it starts from matter.
    TEST_METHOD (TheRebuiltHeatOfReadsBeforeWrittenIsTheHeatOfAStraightRun)
    {
        constexpr double                  kFade     = 0.1;
        constexpr uint64_t                kAt       = 110000;
        FillRig                           rig;
        ScratchHeatReplayer               replayer;
        HeatRebuildJob                    job;
        std::vector<HeatRebuildResult>    results;
        TestMachine                       machine   ("Apple2e");
        MachineDebugTarget                target    (machine);
        AccessHeatMap                   * map       = nullptr;
        size_t                            warm      = 0;
        bool                              hasWindow = false;
        HRESULT                           hr        = S_OK;



        RunFillTo (rig.machine, kFillEnd);
        replayer.SetMachine (rig.machine.GetConfig(), rig.machine.GetCurrentMachineName());
        rig.target.SetHeatMapFade (kFade);
        rig.Seek (kAt);

        hr = rig.target.GetHeatHistory().MakeRebuildJob (job, hasWindow);
        AssertSucceeded (hr, L"MakeRebuildJob");
        Assert::IsTrue   (hasWindow, L"history holds a window to replay");
        Assert::AreEqual ((size_t) 1, job.parts.size(), L"a short window is one part");
        Assert::IsTrue   (job.parts[0].startPosition > 0, L"the window starts after power-on, or the test proves nothing");

        hr = replayer.Rebuild (job, results);
        AssertSucceeded (hr, L"Rebuild");
        Assert::AreEqual ((size_t) 1, results.size());

        PrepareFill (machine);
        target.SetHeatMapOn   (true);
        target.SetHeatMapFade (kFade);

        map = const_cast<AccessHeatMap *> (target.FoldHeatMap());
        Assert::IsNotNull (map);

        RunFillTo (machine, kAt, map);
        map->Fold (machine.GetCpu()->GetTotalCycles());

        for (size_t address = 0; address < AccessHeatMap::kAddressCount; address++)
        {
            warm += (map->GetHeat (HeatSpace::Main, HeatKind::UnwrittenRead, (Word) address) > 0.0f) ? 1 : 0;
        }

        Assert::IsTrue (warm > 0, L"the straight run heated reads before written, or the test proves nothing");

        AssertNearHeat (map->GetHeatTable(), results[0].heat, kAt);
    }


    //  The opcode marks a second machine running the fill guest from
    //  power-on holds at each position, the CPU's space then main RAM's.
    static std::map<uint64_t, std::vector<Byte>> GetFillStraightMarks (const std::set<uint64_t> & positions)
    {
        TestMachine                              machine ("Apple2e");
        MachineDebugTarget                       target  (machine);
        std::map<uint64_t, std::vector<Byte>>    marks;
        std::vector<Byte>                        main;



        PrepareFill (machine);
        target.SetHeatMapOn (true);

        for (uint64_t position : positions)
        {
            RunFillTo (machine, position);
            Assert::IsNotNull (target.FoldHeatMap());
            target.FoldHeatMap()->GetOpcodeMarks (HeatSpace::Cpu,  marks[position]);
            target.FoldHeatMap()->GetOpcodeMarks (HeatSpace::Main, main);
            marks[position].insert (marks[position].end(), main.begin(), main.end());
        }

        return marks;
    }


    //  Where an opcode was fetched, and which bytes the debugger edited,
    //  follow the machine through history: the fill guest runs its restart
    //  code only after its first pass, so before then those bytes are no
    //  opcode, as a straight run shows; and a byte the debugger poked is
    //  edited from the poke on and not before it.
    TEST_METHOD (OpcodeAndEditedMarksFollowTheMachine)
    {
        constexpr Word                             kPoked    = 0x9000;
        constexpr uint64_t                         kPokeAt   = 50000;
        constexpr Word                             kRestart  = 0x0318;
        FillRig                                    rig;
        std::set<uint64_t>                         wanted    = { 30000, 60000, 120000, 190000 };
        std::map<uint64_t, std::vector<Byte>>      straight;
        std::vector<Byte>                          marks;
        std::vector<Byte>                          main;



        RunFillTo (rig.machine, kPokeAt);
        Assert::IsTrue (rig.target.TryPoke (kPoked, 0x77));
        RunFillTo (rig.machine, kFillEnd);

        straight = GetFillStraightMarks (wanted);

        Assert::AreEqual ((Byte) 0, straight[60000][kRestart],  L"the restart code has not run by then, or the test proves nothing");
        Assert::AreEqual ((Byte) 1, straight[190000][kRestart], L"and has by the end");

        for (uint64_t at : { (uint64_t) 190000, (uint64_t) 30000, (uint64_t) 120000, (uint64_t) 60000 })
        {
            rig.Seek (at);

            rig.GetMap().GetOpcodeMarks (HeatSpace::Cpu,  marks);
            rig.GetMap().GetOpcodeMarks (HeatSpace::Main, main);
            marks.insert (marks.end(), main.begin(), main.end());

            Assert::IsTrue (marks == straight[at], std::format (L"opcode marks after a seek to {}", at).c_str());
            Assert::AreEqual (at > kPokeAt, rig.GetMap().IsEdited (HeatSpace::Cpu,  kPoked), std::format (L"edited after a seek to {}", at).c_str());
            Assert::AreEqual (at > kPokeAt, rig.GetMap().IsEdited (HeatSpace::Main, kPoked));
        }
    }

    //  What a history's heat map sides hold: the counts, those counts as
    //  they would be packed without the two newest kinds, and the kept bits,
    //  with how many keyframes kept them whole.
    static std::string DescribeSideCost (const char * name, HeatHistory & history, const KeyframeStore & keyframes)
    {
        SnapshotCompressor        compressor;
        HeatKeyframeSide::Part    counts;
        HeatKeyframeSide::Part    written;
        std::vector<int64_t>      before;
        std::vector<int64_t>      after;
        std::vector<Byte>         delta;
        std::vector<Byte>         packed;
        const Byte              * side       = nullptr;
        size_t                    size       = 0;
        size_t                    countBytes = 0;
        size_t                    oldBytes   = 0;
        size_t                    bitBytes   = 0;
        size_t                    whole      = 0;
        HRESULT                   hr         = S_OK;



        for (size_t i = 0; i < keyframes.GetCount(); i++)
        {
            keyframes.GetSide (i, side, size);

            if (!HeatKeyframeSide::TrySplit (side, size, counts, written))
            {
                continue;
            }

            countBytes += counts.size;
            bitBytes   += written.size;
            whole      += (written.size > 0) ? 1 : 0;

            if (i == 0 || counts.size == 0 || !history.TryGetTotalsAt (keyframes.GetInfo (i - 1).position, before) || !history.TryGetTotalsAt (keyframes.GetInfo (i).position, after))
            {
                continue;
            }

            before = WithoutNewKinds (before);
            after  = WithoutNewKinds (after);

            HeatCountDelta::Encode (after.data(), before.data(), after.size(), delta);

            hr = HeatCountDelta::Pack (delta, compressor, packed);
            AssertSucceeded (hr, L"Pack");

            oldBytes += packed.size();
        }

        return std::format ("{}: {} keyframes, {} bytes of snapshots; counts {} bytes ({} without the two new kinds), kept bits {} bytes, kept whole at {} keyframes\n",
                            name, keyframes.GetCount(), keyframes.GetByteCount() - keyframes.GetSideByteCount(), countBytes, oldBytes, bitBytes, whole);
    }


    static std::vector<int64_t> WithoutNewKinds (std::vector<int64_t> totals)
    {
        for (size_t space = 0; space < AccessHeatMap::kSpaceCount; space++)
        {
            for (HeatKind kind : { HeatKind::UnwrittenRead, HeatKind::ChangedWrite })
            {
                size_t  first = AccessHeatMap::GetIndex ((HeatSpace) space, kind, 0);



                std::fill (totals.begin() + first, totals.begin() + first + AccessHeatMap::kAddressCount, 0);
            }
        }

        return totals;
    }


    //  Not a pass or fail: what reads before written and changed values cost
    //  history and memory, written to the test log, over the reverse rig's
    //  busy loop and the fill guest, and what a fold of the larger map takes.
    TEST_METHOD (TheNewChannelsCostIsLogged)
    {
        constexpr int      kFolds      = 20;
        constexpr double   kMs         = 1000.0;
        constexpr size_t   kMapBytes   = sizeof (uint32_t) + sizeof (float) + sizeof (int64_t);
        constexpr size_t   kWalkBytes  = 3 * sizeof (int64_t);
        constexpr size_t   kOldEntries = AccessHeatMap::kEntryCount * 3 / AccessHeatMap::kKindCount;
        std::string        log;
        AccessHeatMap      map;
        double             ms          = 0.0;

        {
            Rig  rig (ReverseSessionRig::MakeSettings (KeyframeSettings::kDefaultFrames));



            rig.script.RunTo (rig.machine, UINT64_MAX);
            log += DescribeSideCost ("reverse rig", rig.target.GetHeatHistory(), rig.controller.GetKeyframes());
        }

        {
            FillRig  rig;



            RunFillTo (rig.machine, kFillEnd);
            log += DescribeSideCost ("fill guest, a keyframe a frame", rig.target.GetHeatHistory(), rig.controller.GetKeyframes());
        }

        map.Start (nullptr, 0);

        for (int i = 0; i < kFolds; i++)
        {
            auto  start = std::chrono::steady_clock::now();



            map.Fold ((uint64_t) (i + 1) * AccessHeatMap::kCyclesPerFrame);
            ms += std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - start).count();
        }

        log += std::format ("map: {:.1f} MB of counts, heat and totals ({:.1f} MB with three kinds), {:.1f} MB more in history's walks ({:.1f} MB with three kinds); a fold {:.2f} ms ({} build)\n",
                            (double) (AccessHeatMap::kEntryCount * kMapBytes) / kMs / kMs,
                            (double) (kOldEntries * kMapBytes) / kMs / kMs,
                            (double) (AccessHeatMap::kEntryCount * kWalkBytes) / kMs / kMs,
                            (double) (kOldEntries * kWalkBytes) / kMs / kMs,
                            ms / kFolds,
                            IsDebugBuild() ? "Debug" : "Release");

        Logger::WriteMessage (log.c_str());
    }
};
