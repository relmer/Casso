#include "Pch.h"

#include "EmuTests/ReverseSessionRig.h"
#include "Debugger/Reverse/InputJournal.h"
#include "HResultAssert.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


static constexpr uint64_t  s_kPowerCycleWarmupCycles = 60000;
static constexpr uint64_t  s_kPowerCycleAfterCycles  = KeyframeSettings::kFrameCycles * 3;





////////////////////////////////////////////////////////////////////////////////
//
//  NeverStops
//
//  A stop test that never fires, so a reverse continue replays every stretch
//  of history to its end and checks each keyframe it reaches.
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
//  ReversePowerCycleReplayTests
//
//  A power cycle the user asks for while history records is a journaled
//  input, and a replay carries it out again. The Disk II's own power cycle
//  points its drives at their empty internal disks; the mounts persist, so
//  the drives must go back to the disks in the bays, live and in a replay
//  alike, and a keyframe loaded after a replayed power cycle must find them
//  there too, or the first keyframe after it fails its checksum and history
//  is cut.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ReversePowerCycleReplayTests)
{
public:

    TEST_METHOD (APowerCycleKeepsTheDrivesOnTheDisksInTheBays)
    {
        TestMachine  machine ("Apple2e");



        ReverseSessionRig::Prepare (machine);

        machine.PowerCycle();

        Assert::IsTrue (machine.GetRefs().diskController->GetDisk (ReverseSessionRig::kDiskDrive) ==
                        machine.GetDiskStore().GetImage (ReverseSessionRig::kDiskSlot, ReverseSessionRig::kDiskDrive),
                        L"drive 1 reads the disk in its bay after the power cycle");
    }


    TEST_METHOD (AReplayedPowerCycleMatchesEveryKeyframeAfterIt)
    {
        TestMachine        machine    ("Apple2e");
        ReverseController  controller (machine);
        NeverStops         neverStops;
        ReverseResult      result;
        HRESULT            hr         = S_OK;
        size_t             keyframes  = 0;
        uint64_t           oldest     = 0;



        ReverseSessionRig::Prepare (machine);

        hr = controller.Start (ReverseSessionRig::MakeSettings (1));
        AssertSucceeded (hr, L"Start");

        machine.RunCycles (s_kPowerCycleWarmupCycles);

        machine.RecordInput (InputKind::PowerCycle, 0, 0, {});
        machine.PowerCycle();

        machine.RunCycles (s_kPowerCycleAfterCycles);

        keyframes = controller.GetKeyframes().GetCount();
        oldest    = controller.GetOldestPosition();

        hr = controller.ReverseContinue (neverStops, result);
        AssertSucceeded (hr, L"ReverseContinue over the whole history");

        Assert::IsTrue (result.outcome != ReverseOutcome::HistoryCut, L"the replay matched every keyframe after the power cycle");
        Assert::IsTrue (result.outcome == ReverseOutcome::AtHistoryStart, L"nothing stopped it, so it reached the start");
        Assert::AreEqual<uint64_t> (oldest, machine.GetPosition(), L"at the oldest position");
        Assert::AreEqual<size_t>   (keyframes + 1, controller.GetKeyframes().GetCount(), L"no keyframe was dropped, and the live end was kept");
    }


    //  The keyframes are ordered by cycle, which the power cycle restarts; a
    //  debugger edit soon after it is kept as a keyframe all the same, and
    //  history goes on from there.
    TEST_METHOD (AnEditJustAfterAPowerCycleIsKeptAndHistoryGoesOn)
    {
        TestMachine        machine    ("Apple2e");
        ReverseController  controller (machine);
        ReverseResult      result;
        HRESULT            hr         = S_OK;
        uint64_t           edited     = 0;



        ReverseSessionRig::Prepare (machine);

        hr = controller.Start (ReverseSessionRig::MakeSettings (1));
        AssertSucceeded (hr, L"Start");

        machine.RunCycles (s_kPowerCycleWarmupCycles);

        machine.RecordInput (InputKind::PowerCycle, 0, 0, {});
        machine.PowerCycle();
        machine.RunCycles (KeyframeSettings::kFrameCycles);

        machine.NoteDebuggerEdit();
        machine.StepOne();

        edited = machine.GetPosition();

        machine.RunCycles (s_kPowerCycleAfterCycles);

        hr = controller.SeekToPosition (edited, result);
        AssertSucceeded (hr, L"SeekToPosition back past the edit");

        Assert::AreEqual<uint64_t> (edited, machine.GetPosition(), L"history after the edit is there to go back into");
        Assert::IsTrue (controller.GetOldestPosition() < edited, L"and starts before it");
    }


    //  A power cycle sets the CPU's cycle count back to zero, and history's
    //  cycles continue across it: keyframes are taken after it in
    //  order, a seek by cycle lands before it, and reverse continue runs
    //  back across it to history's start.
    TEST_METHOD (HistorySpansAPowerCycle)
    {
        TestMachine          machine    ("Apple2e");
        ReverseController    controller (machine);
        NeverStops             neverStops;
        ReverseResult          result;
        HRESULT                hr         = S_OK;
        uint64_t               before     = 0;
        uint64_t               cycled     = 0;
        size_t                 after      = 0;
        size_t                 i          = 0;
        const KeyframeStore  & keyframes  = controller.GetKeyframes();



        ReverseSessionRig::Prepare (machine);

        hr = controller.Start (ReverseSessionRig::MakeSettings (1));
        AssertSucceeded (hr, L"Start");

        machine.RunCycles (s_kPowerCycleWarmupCycles);
        before = machine.GetCpu()->GetHistoryCycles();
        cycled = machine.GetPosition();

        machine.RecordInput (InputKind::PowerCycle, 0, 0, {});
        machine.PowerCycle();

        Assert::AreEqual<uint64_t> (0, machine.GetCpu()->GetTotalCycles(), L"the CPU's count starts again");
        Assert::IsTrue (machine.GetCpu()->GetHistoryCycles() >= before, L"history's does not");

        machine.RunCycles (s_kPowerCycleWarmupCycles * 2);

        for (i = 0; i < keyframes.GetCount(); i++)
        {
            after += keyframes.GetInfo (i).position > cycled ? 1 : 0;
            Assert::IsTrue (i == 0 || keyframes.GetInfo (i).cycle > keyframes.GetInfo (i - 1).cycle, L"the keyframes' cycles only go up");
        }

        Assert::IsTrue (after > 0, L"keyframes are taken after the power cycle");
        Assert::IsTrue (controller.GetOldestPosition() < cycled, L"and history before it is kept");

        hr = controller.SeekToCycle (before - KeyframeSettings::kFrameCycles, result);
        AssertSucceeded (hr, L"SeekToCycle before the power cycle");
        Assert::IsTrue (machine.GetPosition() < cycled, L"a seek by cycle lands before the power cycle");

        hr = controller.ReverseContinue (neverStops, result);
        AssertSucceeded (hr, L"ReverseContinue across the power cycle");
        Assert::IsTrue (result.outcome == ReverseOutcome::AtHistoryStart, L"back to history's start");
    }
};
