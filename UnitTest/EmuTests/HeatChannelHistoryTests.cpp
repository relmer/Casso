#include "Pch.h"

#include "EmuTests/HeatHistoryChecks.h"
#include "EmuTests/InlineWorkQueue.h"
#include "EmuTests/ReverseSessionRig.h"
#include "Debugger/AccessHeatMap.h"
#include "Debugger/HeatCountDelta.h"
#include "Debugger/HeatHistory.h"
#include "Debugger/HeatKeyframeSide.h"
#include "Debugger/MachineDebugTarget.h"
#include "Shell/ScratchHeatReplayer.h"
#include "HResultAssert.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  HeatChannelHistoryTests
//
//  The heat map's newer channels follow the machine's position through
//  history as the counts do: the reads of RAM before anything wrote it, the
//  writes that changed their byte, and the marks of opcodes and of bytes the
//  debugger edited; and what keeping those channels in history costs.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (HeatChannelHistoryTests)
{
public:

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



        while (machine.GetRunner().GetPosition() < position)
        {
            frameEnd = (machine.GetCpu()->GetTotalCycles() / AccessHeatMap::kCyclesPerFrame + 1) * AccessHeatMap::kCyclesPerFrame;

            machine.GetRunner().StepOne();

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
            controller.GetCapturer().SetObserver (target.GetHistoryObserver());
            target.SetHeatMapOn (true);
        }

        ~FillRig()
        {
            controller.GetCapturer().SetObserver (nullptr);
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



            hr = controller.GetSeeker().SeekToPosition (position, result);
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
            wanted.insert (position + HeatHistoryChecks::kRunOn);
        }

        wanted.insert (kFillPhase);
        wanted.insert (kFillEnd);

        straight = CountFillStraight (wanted);

        Assert::IsTrue (SumUnwritten (straight[60000], HeatSpace::Main) > 0, L"the guest read RAM before writing it by then, or the test proves nothing");
        Assert::AreEqual (SumUnwritten (straight[kFillPhase], HeatSpace::Cpu), SumUnwritten (straight[kFillEnd], HeatSpace::Cpu), L"and none after the first pass");

        HeatHistoryChecks::AssertSameTotals (straight[kFillEnd], HeatHistoryChecks::GetShown (rig.GetMap()), L"live");

        //  Seeks scattered, back from the live end and forward again.
        for (size_t i = 0; i < stops.size(); i++)
        {
            uint64_t  at = stops[(i * HeatHistoryChecks::kShuffle) % stops.size()];



            rig.Seek (at);
            HeatHistoryChecks::AssertSameTotals (straight[at], HeatHistoryChecks::GetShown (rig.GetMap()), std::format (L"seek to {}", at));
        }

        //  A step back, then running on from the past.
        for (uint64_t position : stops)
        {
            rig.Seek (position);

            hr = rig.controller.GetStepper().StepBack (result);
            AssertSucceeded (hr, L"StepBack");
            rig.target.NoteHistoryMoved (false);
            HeatHistoryChecks::AssertSameTotals (straight[position - 1], HeatHistoryChecks::GetShown (rig.GetMap()), std::format (L"step back from {}", position));

            RunFillTo (rig.machine, position + HeatHistoryChecks::kRunOn);
            HeatHistoryChecks::AssertSameTotals (straight[position + HeatHistoryChecks::kRunOn], HeatHistoryChecks::GetShown (rig.GetMap()), std::format (L"running on from {}", position - 1));
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

        HeatHistoryChecks::AssertNearHeat (map->GetHeatTable(), results[0].heat, kAt);
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



        for (size_t i = 0; i < keyframes.GetArena().GetCount(); i++)
        {
            keyframes.GetSide (i, side, size);

            if (!HeatKeyframeSide::TrySplit (side, size, counts, written))
            {
                continue;
            }

            countBytes += counts.size;
            bitBytes   += written.size;
            whole      += (written.size > 0) ? 1 : 0;

            if (i == 0 || counts.size == 0 || !history.TryGetTotalsAt (keyframes.GetArena().GetInfo (i - 1).position, before) || !history.TryGetTotalsAt (keyframes.GetArena().GetInfo (i).position, after))
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

        //  The heat map's sides are a small share of the history's bytes.
        Assert::IsTrue ((countBytes + bitBytes) * 20 < keyframes.GetArena().GetByteCount() - keyframes.GetArena().GetSideByteCount(), L"the heat sides take under 5% of the snapshots' bytes");

        return std::format ("{}: {} keyframes, {} bytes of snapshots; counts {} bytes ({} without the two new kinds), kept bits {} bytes, kept whole at {} keyframes\n",
                            name, keyframes.GetArena().GetCount(), keyframes.GetArena().GetByteCount() - keyframes.GetArena().GetSideByteCount(), countBytes, oldBytes, bitBytes, whole);
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


    //  What reads before written and changed values cost history and memory,
    //  written to the test log, over the reverse rig's busy loop and the fill
    //  guest, and what a fold of the larger map takes with every entry hot.
    //  The sides are gated at 5% of the snapshots and, in a Release build
    //  without AddressSanitizer, a fold at a frame's time (1.8 ms measured
    //  2026-10-23, 31 ms with the sanitizer).
    TEST_METHOD (TheNewChannelsCostIsLogged)
    {
        constexpr int      kFolds      = 20;
        constexpr double   kMs         = 1000.0;
        constexpr double   kFrameMs    = 1000.0 / 60.0;
        constexpr size_t   kMapBytes   = sizeof (uint32_t) + sizeof (float) + sizeof (int64_t);
        constexpr size_t   kWalkBytes  = 3 * sizeof (int64_t);
        constexpr size_t   kOldEntries = AccessHeatMap::kEntryCount * 3 / AccessHeatMap::kKindCount;
        std::string        log;
        AccessHeatMap      map;
        std::vector<float> hot         (AccessHeatMap::kEntryCount, 1.0e6f);
        double             ms          = 0.0;

        {
            HeatHistoryRig  rig (ReverseSessionRig::MakeSettings (KeyframeSettings::kDefaultFrames));



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
            map.MergeHeat (hot, (uint64_t) i * AccessHeatMap::kCyclesPerFrame);

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
                            HeatHistoryChecks::IsDebugBuild() ? "Debug" : "Release");

        Logger::WriteMessage (log.c_str());

        Assert::IsTrue (HeatHistoryChecks::IsDebugBuild() || HeatHistoryChecks::IsSanitizedBuild() || ms / kFolds < kFrameMs, L"a fold of the whole map hot takes under a frame");
    }
};
