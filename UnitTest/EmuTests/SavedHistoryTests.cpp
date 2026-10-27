#include "Pch.h"

#include "HResultAssert.h"
#include "ReverseSessionRig.h"
#include "TestMachine.h"
#include "Core/StateReader.h"
#include "Debugger/Reverse/ReverseHost.h"
#include "Devices/Disk/DiskImage.h"
#include "Shell/SavedHistory.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace SavedHistoryTests
{
    static constexpr uint64_t  kRecordCycles = 400'000;
    static constexpr uint64_t  kBranchCycles = 120'000;
    static constexpr uint64_t  kBack         = 2'000;





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  SavedHistoryTests
    //
    //  A machine's history saved in a state file's section and read back:
    //  every field survives the trip, Parse fails on a damaged section, and
    //  a history with a fork loads into another machine at the same live
    //  end, where a switch to the branch lands exactly as on the machine it
    //  was saved from.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (SavedHistoryTests)
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


        //  A //e with history, forked kBack instructions before its live
        //  end, then run on from the fork.
        static void RecordWithFork (TestMachine & machine, ReverseHost & host)
        {
            ReverseResult  result;
            HRESULT        hr = S_OK;



            ReverseSessionRig::Prepare (machine);

            hr = host.StartRecording (ReverseSessionRig::MakeSettings (1));
            AssertSucceeded (hr, L"StartRecording");

            Run (machine, kRecordCycles);

            hr = host.Execute (ReverseCommand::Seek, machine.GetPosition() - kBack, nullptr, result);
            AssertSucceeded (hr, L"Seek");

            hr = host.Fork();
            AssertSucceeded (hr, L"Fork");

            Run (machine, kBranchCycles);
        }


        //  Two segments, each with keyframes and journal entries, a medium
        //  and a point to resume at, every field set to a distinct value.
        static SavedHistoryContents MakeContents()
        {
            SavedHistoryContents  contents;
            SavedKeyframeSegment  active;
            SavedKeyframeSegment  branch;
            KeyframeInfo          info;
            InputRecord           record;



            contents.fingerprint = 0x1234'5678'9ABC'DEF0;

            contents.keyframes.settings.intervalCycles = 170'300;
            contents.keyframes.settings.wholeEvery     = 30;
            contents.keyframes.settings.longestGroup   = 1800;
            contents.keyframes.nextBranchId            = 2;

            info.position     = 10;
            info.cycle        = 100;
            info.journalIndex = 3;
            info.checksum     = 0xAAAA;
            info.stateBytes   = 64;
            info.storedBytes  = 8;
            info.sideBytes    = 2;
            info.wallTime     = 77;
            info.isWhole      = true;
            active.infos.push_back   (info);
            active.offsets.push_back (0);

            info              = KeyframeInfo();
            info.position     = 20;
            info.cycle        = 200;
            info.checksum     = 0xBBBB;
            info.stateBytes   = 64;
            info.storedBytes  = 4;
            info.gapStart     = 15;
            info.hasGapBefore = true;
            info.isLiveEnd    = true;
            active.infos.push_back   (info);
            active.offsets.push_back (8);

            record.position   = 12;
            record.cycle      = 120;
            record.kind       = InputKind::DiskMount;
            record.value      = 6;
            record.detail     = 1;
            record.data       = 99;
            record.isObserved = true;
            record.payload    = "disk.dsk";

            active.firstJournalIndex    = 3;
            active.info.liveEndPosition = 25;
            active.info.liveEndCycle    = 250;
            active.records.push_back (record);

            info              = KeyframeInfo();
            info.position     = 30;
            info.cycle        = 300;
            info.checksum     = 0xCCCC;
            info.stateBytes   = 64;
            info.storedBytes  = 4;
            info.isBoundary   = true;
            branch.infos.push_back   (info);
            branch.offsets.push_back (12);

            branch.info.id              = 1;
            branch.info.forkPosition    = 15;
            branch.info.forkCycle       = 150;
            branch.info.liveEndPosition = 30;
            branch.info.liveEndCycle    = 300;
            branch.info.leftAtWallTime  = 88;
            branch.firstJournalIndex    = 4;

            record         = InputRecord();
            record.kind    = InputKind::KeyPress;
            record.value   = 0xC1;
            branch.records.push_back (record);

            contents.keyframes.segments.push_back (std::move (active));
            contents.keyframes.segments.push_back (std::move (branch));

            for (size_t i = 0; i < 16; i++)
            {
                contents.keyframes.arena.push_back (static_cast<Byte> (i * 7));
            }

            contents.media.push_back (SavedBayMedium { 6, 1, 42 });

            contents.resumePosition = 22;
            contents.resumeCycle    = 220;

            return contents;
        }


        static void AssertSameInfo (const KeyframeInfo & expected, const KeyframeInfo & actual)
        {
            Assert::AreEqual<uint64_t> (expected.position,     actual.position,     L"position");
            Assert::AreEqual<uint64_t> (expected.cycle,        actual.cycle,        L"cycle");
            Assert::AreEqual<size_t>   (expected.journalIndex, actual.journalIndex, L"journalIndex");
            Assert::AreEqual<uint64_t> (expected.checksum,     actual.checksum,     L"checksum");
            Assert::AreEqual<size_t>   (expected.stateBytes,   actual.stateBytes,   L"stateBytes");
            Assert::AreEqual<size_t>   (expected.storedBytes,  actual.storedBytes,  L"storedBytes");
            Assert::AreEqual<size_t>   (expected.sideBytes,    actual.sideBytes,    L"sideBytes");
            Assert::AreEqual<uint64_t> (expected.wallTime,     actual.wallTime,     L"wallTime");
            Assert::AreEqual<uint64_t> (expected.gapStart,     actual.gapStart,     L"gapStart");
            Assert::AreEqual           (expected.isWhole,      actual.isWhole,      L"isWhole");
            Assert::AreEqual           (expected.isBoundary,   actual.isBoundary,   L"isBoundary");
            Assert::AreEqual           (expected.isLiveEnd,    actual.isLiveEnd,    L"isLiveEnd");
            Assert::AreEqual           (expected.hasGapBefore, actual.hasGapBefore, L"hasGapBefore");
        }


        static void AssertSameRecord (const InputRecord & expected, const InputRecord & actual)
        {
            Assert::AreEqual<uint64_t> (expected.position,   actual.position, L"record position");
            Assert::AreEqual<uint64_t> (expected.cycle,      actual.cycle,    L"record cycle");
            Assert::IsTrue             (expected.kind == actual.kind,         L"record kind");
            Assert::AreEqual           (expected.value,      actual.value,    L"record value");
            Assert::AreEqual           (expected.detail,     actual.detail,   L"record detail");
            Assert::AreEqual<uint64_t> (expected.data,       actual.data,     L"record data");
            Assert::AreEqual           (expected.isObserved, actual.isObserved, L"record isObserved");
            Assert::AreEqual           (expected.payload,    actual.payload,  L"record payload");
        }


        static void AssertRefused (const std::vector<Byte> & section, const wchar_t * what)
        {
            SavedHistoryContents  contents;
            HRESULT               hr = SavedHistory::Parse (section.data(), section.size(), contents);



            Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), hr, what);
            Assert::IsTrue   (contents.keyframes.segments.empty(), what);
        }


        TEST_METHOD (EveryFieldSurvivesTheTrip)
        {
            SavedHistoryContents  expected = MakeContents();
            SavedHistoryContents  actual;
            std::vector<Byte>     section;
            HRESULT               hr       = S_OK;



            hr = SavedHistory::Build (expected, section);
            AssertSucceeded (hr, L"Build");

            hr = SavedHistory::Parse (section.data(), section.size(), actual);
            AssertSucceeded (hr, L"Parse");

            Assert::AreEqual<uint64_t> (expected.fingerprint,                          actual.fingerprint,                          L"fingerprint");
            Assert::AreEqual<uint64_t> (expected.keyframes.settings.intervalCycles,    actual.keyframes.settings.intervalCycles,    L"intervalCycles");
            Assert::AreEqual<uint32_t> (expected.keyframes.settings.wholeEvery,        actual.keyframes.settings.wholeEvery,        L"wholeEvery");
            Assert::AreEqual<uint32_t> (expected.keyframes.settings.longestGroup,      actual.keyframes.settings.longestGroup,      L"longestGroup");
            Assert::AreEqual<uint32_t> (expected.keyframes.nextBranchId,               actual.keyframes.nextBranchId,               L"nextBranchId");
            Assert::IsTrue             (expected.keyframes.arena == actual.keyframes.arena,                                         L"arena");
            Assert::AreEqual<uint64_t> (expected.resumePosition,                       actual.resumePosition,                       L"resumePosition");
            Assert::AreEqual<uint64_t> (expected.resumeCycle,                          actual.resumeCycle,                          L"resumeCycle");
            Assert::AreEqual<size_t>   (expected.keyframes.segments.size(),            actual.keyframes.segments.size(),            L"segments");

            for (size_t s = 0; s < expected.keyframes.segments.size(); s++)
            {
                const SavedKeyframeSegment  & from = expected.keyframes.segments[s];
                const SavedKeyframeSegment  & to   = actual.keyframes.segments[s];



                Assert::AreEqual<uint32_t> (from.info.id,              to.info.id,              L"id");
                Assert::AreEqual<uint32_t> (from.info.parent,          to.info.parent,          L"parent");
                Assert::AreEqual<uint64_t> (from.info.forkPosition,    to.info.forkPosition,    L"forkPosition");
                Assert::AreEqual<uint64_t> (from.info.forkCycle,       to.info.forkCycle,       L"forkCycle");
                Assert::AreEqual<uint64_t> (from.info.liveEndPosition, to.info.liveEndPosition, L"liveEndPosition");
                Assert::AreEqual<uint64_t> (from.info.liveEndCycle,    to.info.liveEndCycle,    L"liveEndCycle");
                Assert::AreEqual<uint64_t> (from.info.leftAtWallTime,  to.info.leftAtWallTime,  L"leftAtWallTime");
                Assert::AreEqual<size_t>   (from.firstJournalIndex,    to.firstJournalIndex,    L"firstJournalIndex");
                Assert::IsTrue             (from.offsets == to.offsets,                         L"offsets");
                Assert::AreEqual<size_t>   (from.infos.size(),         to.infos.size(),         L"keyframes");
                Assert::AreEqual<size_t>   (from.records.size(),       to.records.size(),       L"records");

                for (size_t i = 0; i < from.infos.size(); i++)
                {
                    AssertSameInfo (from.infos[i], to.infos[i]);
                }

                for (size_t i = 0; i < from.records.size(); i++)
                {
                    AssertSameRecord (from.records[i], to.records[i]);
                }
            }

            Assert::AreEqual<size_t>   (1,  actual.media.size(),       L"media");
            Assert::AreEqual           (6,  actual.media[0].slot,      L"slot");
            Assert::AreEqual           (1,  actual.media[0].drive,     L"drive");
            Assert::AreEqual<uint64_t> (42, actual.media[0].mediaId,   L"mediaId");
        }


        //  Parse fails on a wrong tag, version, size or hash, and on a
        //  section cut short or run long, and returns nothing.
        TEST_METHOD (ParseFailsOnADamagedSection)
        {
            std::vector<Byte>  section;
            std::vector<Byte>  damaged;
            HRESULT            hr      = SavedHistory::Build (MakeContents(), section);



            AssertSucceeded (hr, L"Build");

            AssertRefused (std::vector<Byte>(), L"no bytes");

            damaged = section;
            damaged[0] ^= 1;
            AssertRefused (damaged, L"a wrong tag");

            damaged = section;
            damaged[4] = 2;
            AssertRefused (damaged, L"a newer version");

            damaged = section;
            damaged.pop_back();
            AssertRefused (damaged, L"cut short");

            damaged = section;
            damaged.push_back (0);
            AssertRefused (damaged, L"run long");

            damaged = section;
            damaged[SavedHistory::kHeaderSize + 20] ^= 1;
            AssertRefused (damaged, L"a body that does not match its hash");
        }


        //  An input kind past InputKind::StateLoad is bad data, though the
        //  hash matches.
        TEST_METHOD (ParseFailsOnAnUnknownInputKind)
        {
            SavedHistoryContents  contents = MakeContents();
            std::vector<Byte>     section;
            HRESULT               hr       = S_OK;



            contents.keyframes.segments[0].records[0].kind = static_cast<InputKind> (0xEE);

            hr = SavedHistory::Build (contents, section);
            AssertSucceeded (hr, L"Build");

            AssertRefused (section, L"an unknown input kind");
        }


        //  The history of a machine with a fork, saved and loaded into
        //  another machine given the same state: the second machine is at
        //  the same live end, with the same keyframes and branch, and a
        //  switch to the branch puts both machines in the same state
        //  (SC-004).
        TEST_METHOD (AHistoryWithAForkLoadsIntoAnotherMachineAndSwitchesExactly)
        {
            TestMachine                  source     ("Apple2e");
            TestMachine                  target     ("Apple2e");
            ReverseHost                  sourceHost (source);
            ReverseHost                  targetHost (target);
            SavedHistoryContents         saved;
            SavedHistoryContents         parsed;
            std::vector<Byte>            section;
            std::vector<Byte>            state;
            std::vector<KeyframeBranch>  sourceBranches;
            std::vector<KeyframeBranch>  targetBranches;
            ReverseResult                result;
            uint64_t                     oldest      = 0;
            HRESULT                      hr          = S_OK;



            RecordWithFork (source, sourceHost);

            hr = sourceHost.GetController().SaveHistory (saved.keyframes);
            AssertSucceeded (hr, L"SaveHistory");

            hr = SavedHistory::Build (saved, section);
            AssertSucceeded (hr, L"Build");

            hr = SavedHistory::Parse (section.data(), section.size(), parsed);
            AssertSucceeded (hr, L"Parse");

            //  The other machine, loaded with the state at the live end and
            //  its disk given the id the keyframes use for it.
            ReverseSessionRig::Prepare (target);
            target.GetDiskStore().GetImage (ReverseSessionRig::kDiskSlot, ReverseSessionRig::kDiskDrive)->SetImageId (source.GetDiskStore().GetMediaId (ReverseSessionRig::kDiskSlot, ReverseSessionRig::kDiskDrive));

            state = ReverseSessionRig::Save (source);

            {
                StateReader  reader (state);



                hr = target.LoadState (reader);
                AssertSucceeded (hr, L"LoadState");
            }

            hr = targetHost.StartRecordingFromSaved (ReverseSessionRig::MakeSettings (1), parsed.keyframes);
            AssertSucceeded (hr, L"StartRecordingFromSaved");

            Assert::IsTrue             (targetHost.IsRecording(),                                             L"recording from the saved history");
            Assert::AreEqual<uint64_t> (source.GetPosition(), target.GetPosition(),                           L"at the same live end");
            Assert::AreEqual<uint64_t> (ReverseSessionRig::Checksum (source), ReverseSessionRig::Checksum (target), L"with the same machine");
            Assert::AreEqual<uint64_t> (sourceHost.GetController().GetOldestPosition(), targetHost.GetController().GetOldestPosition(), L"and the same oldest point");

            for (size_t i = 0; i < sourceHost.GetController().GetKeyframes().GetCount(); i++)
            {
                Assert::AreEqual<uint64_t> (sourceHost.GetController().GetKeyframes().GetInfo (i).checksum,
                                            targetHost.GetController().GetKeyframes().GetInfo (i).checksum, L"the same keyframe");
            }

            sourceHost.GetController().GetKeyframes().GetBranches (sourceBranches);
            targetHost.GetController().GetKeyframes().GetBranches (targetBranches);

            Assert::AreEqual<size_t>   (1, targetBranches.size(),                                    L"the branch came with it");
            Assert::AreEqual<uint32_t> (sourceBranches[0].id,              targetBranches[0].id,              L"the same branch");
            Assert::AreEqual<uint64_t> (sourceBranches[0].forkPosition,    targetBranches[0].forkPosition,    L"forked at the same position");
            Assert::AreEqual<uint64_t> (sourceBranches[0].liveEndPosition, targetBranches[0].liveEndPosition, L"ending at the same position");

            hr = sourceHost.Execute (ReverseCommand::SwitchBranch, sourceBranches[0].id, nullptr, result);
            AssertSucceeded (hr, L"the first machine's switch");

            hr = targetHost.Execute (ReverseCommand::SwitchBranch, targetBranches[0].id, nullptr, result);
            AssertSucceeded (hr, L"the second machine's switch");

            Assert::AreEqual<uint64_t> (source.GetPosition(), target.GetPosition(),                           L"both at the branch's live end");
            Assert::AreEqual<uint64_t> (ReverseSessionRig::Checksum (source), ReverseSessionRig::Checksum (target), L"with the same machine");

            oldest = targetHost.GetController().GetOldestPosition() + kBack;

            hr = sourceHost.Execute (ReverseCommand::Seek, oldest, nullptr, result);
            AssertSucceeded (hr, L"the first machine's seek");

            hr = targetHost.Execute (ReverseCommand::Seek, oldest, nullptr, result);
            AssertSucceeded (hr, L"the second machine's seek");

            Assert::AreEqual<uint64_t> (ReverseSessionRig::Checksum (source), ReverseSessionRig::Checksum (target), L"and the same machine near the oldest point");
        }


        //  StartRecordingFromSaved fails on history taken with another
        //  interval, and nothing is recording.
        TEST_METHOD (StartingFromHistoryWithAnotherIntervalFails)
        {
            TestMachine           machine ("Apple2e");
            ReverseHost           host    (machine);
            SavedKeyframes        saved;
            HRESULT               hr      = S_OK;



            RecordWithFork (machine, host);

            hr = host.GetController().SaveHistory (saved);
            AssertSucceeded (hr, L"SaveHistory");

            hr = host.StartRecordingFromSaved (ReverseSessionRig::MakeSettings (2), saved);

            Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), hr, L"refused");
            Assert::IsFalse  (host.IsRecording(), L"nothing is recording");
        }


        //  Saved history over the budget is cut to fit: the branches are
        //  dropped, and the active path's newest group stays.
        TEST_METHOD (HistoryOverTheBudgetIsCutToFit)
        {
            TestMachine       machine ("Apple2e");
            ReverseHost       host    (machine);
            SavedKeyframes    saved;
            KeyframeStore     store;
            InputJournal      journal;
            KeyframeSettings  settings;
            HRESULT           hr      = S_OK;



            RecordWithFork (machine, host);

            hr = host.GetController().SaveHistory (saved);
            AssertSucceeded (hr, L"SaveHistory");

            settings             = saved.settings;
            settings.budgetBytes = 1;
            store.Configure (settings);

            hr = store.Import (saved, journal);
            AssertSucceeded (hr, L"Import");

            Assert::AreEqual<size_t>   (0, store.GetBranchCount(),  L"no branch left");
            Assert::IsTrue             (store.GetCount() > 0,       L"the active path's newest group kept");
            Assert::AreEqual<uint64_t> (saved.segments[0].infos.back().position, store.GetInfo (store.GetCount() - 1).position, L"its newest keyframe");
        }


        //  When the keyframes' bytes do not lie end to end in the arena,
        //  Import empties the store and the journal.
        TEST_METHOD (ImportEmptiesTheStoreOnInconsistentHistory)
        {
            TestMachine     machine ("Apple2e");
            ReverseHost     host    (machine);
            SavedKeyframes  saved;
            KeyframeStore   store;
            InputJournal    journal;
            HRESULT         hr      = S_OK;



            RecordWithFork (machine, host);

            hr = host.GetController().SaveHistory (saved);
            AssertSucceeded (hr, L"SaveHistory");

            saved.segments[0].offsets[1]++;

            store.Configure (saved.settings);

            hr = store.Import (saved, journal);

            Assert::AreEqual         (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), hr, L"refused");
            Assert::AreEqual<size_t> (0, store.GetCount(), L"no keyframe");
            Assert::AreEqual<size_t> (journal.GetBeginIndex(), journal.GetEndIndex(), L"no record");
        }


        //  A medium given back its saved id moves later ids past it.
        TEST_METHOD (AnIdGivenBackMovesTheNextIdsPastIt)
        {
            TestMachine        machine ("Apple2e");
            DiskImageStore   & store   = machine.GetDiskStore();
            std::vector<Byte>  raw     (NibblizationLayer::kImageByteSize, 0);
            uint64_t           saved   = 0;
            HRESULT            hr      = S_OK;



            ReverseSessionRig::Prepare (machine);

            saved = store.GetMediaId (ReverseSessionRig::kDiskSlot, ReverseSessionRig::kDiskDrive) + 1'000'000;

            store.GetImage (ReverseSessionRig::kDiskSlot, ReverseSessionRig::kDiskDrive)->SetImageId (saved);

            hr = store.MountFromBytes (ReverseSessionRig::kDiskSlot, 1, "second.dsk", DiskFormat::Dsk, raw);
            AssertSucceeded (hr, L"MountFromBytes");

            Assert::AreEqual<uint64_t> (saved, store.GetMediaId (ReverseSessionRig::kDiskSlot, ReverseSessionRig::kDiskDrive), L"the id given back");
            Assert::IsTrue             (store.GetMediaId (ReverseSessionRig::kDiskSlot, 1) > saved,                           L"the next medium's is past it");
        }


        //  The fingerprint covers how the machine saves, not its contents:
        //  running the machine does not change it, and another kind of
        //  machine has a different one.
        TEST_METHOD (TheFingerprintDependsOnTheMachineKindNotItsState)
        {
            TestMachine  iie   ("Apple2e");
            TestMachine  iic   ("Apple2c");
            uint64_t     first = 0;
            uint64_t     later = 0;
            uint64_t     other = 0;
            HRESULT      hr    = S_OK;



            ReverseSessionRig::Prepare (iie);

            hr = iie.ComputeStateFingerprint (first);
            AssertSucceeded (hr, L"the //e's fingerprint");

            Run (iie, kBranchCycles);

            hr = iie.ComputeStateFingerprint (later);
            AssertSucceeded (hr, L"the //e's fingerprint, later");

            hr = iic.ComputeStateFingerprint (other);
            AssertSucceeded (hr, L"the //c's fingerprint");

            Assert::AreEqual<uint64_t> (first, later, L"running on keeps it");
            Assert::AreNotEqual        (first, other, L"another kind of machine has another");
        }
    };
}
