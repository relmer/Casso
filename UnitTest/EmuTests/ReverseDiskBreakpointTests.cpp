#include "Pch.h"

#include "DebuggerTests/HandlerTestRig.h"
#include "Debugger/DiskValueSymbols.h"
#include "Debugger/Handlers/BreakpointHandlers.h"
#include "Debugger/Reverse/ReverseStopTest.h"
#include "EmuTests/ReverseSessionRig.h"
#include "HResultAssert.h"
#include "Machines/Apple2/Common/Disk2Controller.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseDiskBreakpointTests
//
//  Disk breakpoints in history. A //e with a blank DOS 3.3 disk boots from
//  its ROM with history recording, one keyframe a frame: the motor starts,
//  the head recalibrates against track 0, and the ROM reads address fields
//  until it finds sector 0, whose data field it reads. The run is stepped
//  one instruction at a time with a reverse stop test of its own in the
//  controller's slot and on the bus, so each position's entry indicates
//  whether a forward run stops there; then reverse continue must land on
//  those positions, latest first, with no hit counted.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ReverseDiskBreakpointTests)
{
public:

    static constexpr Word    kBootEntry  = 0xC600;
    static constexpr size_t  kBootSteps  = 1'200'000;     // past sector 0's read, with the ROM reading on

    struct Live
    {
        uint64_t  position = 0;
        bool      isStop   = false;
        Word      qtrack   = 0;
        Word      atrack   = 0;
    };

    using Rig = MachineHandlerRig<BreakpointHandlers>;

    struct DiskRig
    {
        Rig                rig        { "Apple2e", TestMachine::Slots::DiskOnly };
        ReverseController  controller { rig.machine };
        ReverseStopTest    stopTest   { rig.session };
        std::vector<Live>  live;

        Disk2Controller & Controller()
        {
            return *rig.machine.GetRefs().diskController;
        }

        Word Value (const char * name)
        {
            Word  value = 0;



            DiskValueSymbols::TryResolve (name, rig.target, value);
            return value;
        }

        //  Mounts a blank disk, starts history and sets the breakpoints.
        void Prepare (std::initializer_list<const char *> breakpoints, Word start)
        {
            std::vector<Byte>  blank (NibblizationLayer::kImageByteSize, 0);



            rig.machine.GetDiskStore().SetFlushSink ([] (const std::string &, const std::vector<Byte> &) { return S_OK; });
            AssertSucceeded (rig.machine.GetDiskStore().MountFromBytes (6, 0, "blank.dsk", DiskFormat::Dsk, blank), L"MountFromBytes");
            Controller().SetExternalDisk (0, rig.machine.GetDiskStore().GetImage (6, 0));
            rig.machine.GetCpu()->SetPC (start);

            AssertSucceeded (controller.Start (ReverseSessionRig::MakeSettings (1)), L"Start");

            for (const char * line : breakpoints)
            {
                rig.RunOk (line);
            }
        }

        //  Steps live until the PC is at end, if given, or for maxSteps,
        //  recording at each position whether the stop test fires, with the
        //  stop test's listener in the slot and its sink on the bus, and the
        //  debugger's hook off so its own breakpoints stop nothing; then the
        //  debugger's are put back.
        void Record (std::optional<Word> end, size_t maxSteps)
        {
            DebugHook  * hook = rig.machine.GetDebugHook();



            rig.machine.SetDebugHook (nullptr);
            rig.machine.GetMemoryBus().SetWatchSink (stopTest.GetWatchSink());
            Controller().SetEventSink (ObserverSlot::Breakpoints, stopTest.GetDiskSink());

            while (rig.machine.GetCpu()->GetPC() != end && live.size() < maxSteps)
            {
                Live  step;



                step.position = rig.machine.GetPosition();
                step.isStop   = stopTest.ShouldStopBefore (rig.machine, rig.machine.GetCpu()->GetPC());
                step.qtrack   = step.isStop ? Value ("DISK.QTRACK") : 0;
                step.atrack   = step.isStop ? Value ("DISK.ATRACK") : 0;
                live.push_back (step);
                rig.machine.StepOne();
            }

            Assert::IsTrue (!end.has_value() || *end == rig.machine.GetCpu()->GetPC(), L"the PC is at the end");

            rig.machine.SetDebugHook (hook);
            rig.machine.GetMemoryBus().SetWatchSink (&rig.session.GetWatchpoints());
            rig.session.OnStopConditionsChanged();
        }

        std::vector<Live> GetHits() const
        {
            std::vector<Live>  hits;



            std::ranges::copy_if (live, std::back_inserter (hits), [] (const Live & step) { return step.isStop; });
            return hits;
        }

        uint32_t GetHitCount()
        {
            uint32_t  count = 0;



            for (const Breakpoint & entry : rig.session.GetBreakpoints().GetAll())
            {
                count += entry.hits;
            }

            return count;
        }
    };


    //  Every hit of the motor starting, the drive select, the recalibration's
    //  bumps at track 0, sector 0's address fields, its read and DOS's
    //  sector 0, latest first; each landing is the machine as it was there.
    TEST_METHOD (ReverseContinueLandsWhereAForwardRunStops)
    {
        DiskRig            disk;
        std::vector<Live>  hits;
        ReverseResult      result;



        disk.Prepare ({ "BPDISK MOTOR ON", "BPDISK SELECT EVERY", "BPDISK HEAD QT 0 BUMP", "BPDISK ADDRESS 0 0", "BPDISK READ 0 0", "BPDISK DOS 0 0" }, kBootEntry);
        disk.Record (std::nullopt, kBootSteps);
        hits = disk.GetHits();

        Assert::IsTrue (hits.size() >= 5, std::format (L"{} hits", hits.size()).c_str());

        for (size_t at = hits.size(); at > 0; at--)
        {
            AssertSucceeded (disk.controller.ReverseContinue (disk.stopTest, result), L"ReverseContinue");

            Assert::IsTrue             (result.outcome == ReverseOutcome::Moved, std::format (L"hit {}", at - 1).c_str());
            Assert::AreEqual<uint64_t> (hits[at - 1].position, result.position, std::format (L"hit {}", at - 1).c_str());
            Assert::AreEqual<Word>     (hits[at - 1].qtrack, disk.Value ("DISK.QTRACK"));
            Assert::AreEqual<Word>     (hits[at - 1].atrack, disk.Value ("DISK.ATRACK"));
        }

        AssertSucceeded (disk.controller.ReverseContinue (disk.stopTest, result), L"ReverseContinue past the first hit");
        Assert::IsTrue   (result.outcome != ReverseOutcome::Moved, L"no hit before the first");
        Assert::AreEqual (0u, disk.GetHitCount(), L"reverse continue counted nothing");
        Assert::IsTrue   (disk.rig.sink.stops.empty());
    }

    //  A step back and a seek count nothing either; running forward from
    //  history then stops on the next hit and counts it.
    TEST_METHOD (RunningForwardFromHistoryStopsAndCounts)
    {
        DiskRig            disk;
        std::vector<Live>  hits;
        ReverseResult      result;



        disk.Prepare ({ "BPDISK ADDRESS 0" }, kBootEntry);
        disk.Record (std::nullopt, kBootSteps);
        hits = disk.GetHits();

        Assert::IsTrue (hits.size() >= 3, std::format (L"{} hits", hits.size()).c_str());

        AssertSucceeded (disk.controller.StepBack (result), L"StepBack");
        AssertSucceeded (disk.controller.SeekToPosition (hits[hits.size() - 2].position - 1, result), L"SeekToPosition");
        AssertSucceeded (disk.controller.ReverseContinue (disk.stopTest, result), L"ReverseContinue");
        Assert::AreEqual<uint64_t> (hits[hits.size() - 3].position, result.position);
        Assert::AreEqual (0u, disk.GetHitCount(), L"history moves count nothing");

        disk.rig.session.SetBudget (3'000'000);
        disk.rig.RunOk ("G");

        Assert::IsTrue             (disk.rig.LastStop().reason == StopReason::Disk);
        Assert::AreEqual<uint64_t> (hits[hits.size() - 2].position, disk.rig.machine.GetPosition(), L"the next hit");
        Assert::AreEqual           (1u, disk.GetHitCount(), L"and counted it");
    }

    //  A program calls its RWTS routine at $2000 directly, as DOS's own calls
    //  do, before and after it fills $03D9's JMP: the first call is before
    //  the vector has a target, in history as live.
    TEST_METHOD (AnRwtsBreakpointIsUnresolvedBeforeItsVectorWasFilled)
    {
        static constexpr Word  kLoop = 0x0813;
        DiskRig                disk;
        std::vector<Live>      hits;
        ReverseResult          result;



        disk.rig.Load (0x2000, { 0x60 }, 0x0800);
        disk.rig.Load (0x2100, { 0x01, 0x60, 0x01, 0x00, 0x11, 0x05, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x01 }, 0x0800);
        disk.rig.Load (0x03D9, { 0x00, 0x00, 0x20 }, 0x0800);
        disk.rig.Load (0x0800, { 0xA9, 0x21, 0xA0, 0x00, 0x20, 0x00, 0x20,      // call before the JMP
                                 0xA9, 0x4C, 0x8D, 0xD9, 0x03,                  // fill $03D9
                                 0xA9, 0x21, 0xA0, 0x00, 0x20, 0x00, 0x20,      // call after it
                                 0x4C, 0x13, 0x08 }, 0x0800);
        disk.Prepare ({ "BPDISK RWTS 11 5" }, 0x0800);
        disk.Record (kLoop, 1000);
        hits = disk.GetHits();

        Assert::AreEqual ((size_t) 1, hits.size(), L"only the call after the JMP");

        AssertSucceeded (disk.controller.ReverseContinue (disk.stopTest, result), L"ReverseContinue");
        Assert::AreEqual<uint64_t> (hits[0].position, result.position);
        Assert::AreEqual<Word>     (0x2000, disk.rig.machine.GetCpu()->GetPC());

        AssertSucceeded (disk.controller.ReverseContinue (disk.stopTest, result), L"ReverseContinue again");
        Assert::IsTrue (result.outcome != ReverseOutcome::Moved, L"the first call is not a hit");
    }
};
