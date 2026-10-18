#include "Pch.h"

#include "Debugger/Handlers/BreakpointHandlers.h"
#include "Devices/Disk/DiskImageStore.h"
#include "HandlerTestRig.h"
#include "Machines/Apple2/Common/Disk2Controller.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskBreakpointTests
//
//  Disk breakpoints stopping a real machine: a //e with a Disk II and a
//  blank DOS 3.3 disk in drive 1, started at the boot ROM, which turns the
//  motor on, recalibrates the head against track 0, finds track 0 sector
//  0's address field and reads its data field.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    static constexpr Word      kBootEntry    = 0xC600;
    static constexpr Word      kRomSearch    = 0xC65E;    // the ROM's prologue search
    static constexpr Word      kRomChecksum  = 0xC6CB;    // its read of the data checksum nibble
    static constexpr uint64_t  kBudget       = 3'000'000;



    TEST_CLASS (DiskBreakpointTests)
    {
    public:

        using Rig = MachineHandlerRig<BreakpointHandlers>;

        static std::unique_ptr<Rig> MakeRig (bool isMounted = true)
        {
            auto               rig   = std::make_unique<Rig> ("Apple2e", TestMachine::Slots::DiskOnly);
            std::vector<Byte>  blank (NibblizationLayer::kImageByteSize, 0);
            DiskImage        * image = nullptr;



            rig->machine.GetDiskStore().SetFlushSink ([] (const std::string &, const std::vector<Byte> &) { return S_OK; });

            if (isMounted)
            {
                Assert::AreEqual (S_OK, rig->machine.GetDiskStore().MountFromBytes (6, 0, "blank.dsk", DiskFormat::Dsk, blank));
                image = rig->machine.GetDiskStore().GetImage (6, 0);
                rig->machine.GetRefs().diskController->SetExternalDisk (0, image);
            }

            rig->machine.GetCpu()->SetPC (kBootEntry);
            rig->session.SetBudget (kBudget);
            return rig;
        }

        static const StopEvent & Go (Rig & rig)
        {
            rig.RunOk ("G");
            return rig.LastStop();
        }

        static const Breakpoint & Entry (Rig & rig)
        {
            return rig.session.GetBreakpoints().GetAll().at (0);
        }


        TEST_METHOD (TheBootRomsReadOfTrackZeroSectorZeroStops)
        {
            auto       rig = MakeRig();
            StopEvent  stop;



            rig->RunOk ("BPDISK READ 0 0");
            stop = Go (*rig);

            Assert::IsTrue   (stop.reason == StopReason::Disk, L"a disk stop");
            Assert::IsTrue   (stop.disk.has_value() && stop.disk->field.has_value());
            Assert::AreEqual ((Byte) 0,     stop.disk->field->track);
            Assert::AreEqual ((Byte) 0,     stop.disk->field->sector);
            Assert::IsFalse  (stop.disk->field->isPassed, L"the ROM read it");
            Assert::AreEqual (kRomSearch,   stop.disk->field->addressInstruction);
            Assert::AreEqual (kRomChecksum, stop.disk->instruction, L"after the instruction that read the checksum nibble");
            Assert::IsTrue   (stop.disk->isMotorOn);
            Assert::AreEqual (0, stop.disk->drive);
            Assert::AreEqual (6, stop.disk->slot);
            Assert::AreEqual (1u, Entry (*rig).hits);
        }

        //  The boot ROM's read of track 0 sector 0 is DOS 3.3's logical sector
        //  0, which is physical sector 0.
        TEST_METHOD (ADosSectorStopsOnItsPhysicalSector)
        {
            auto       rig = MakeRig();
            StopEvent  stop;



            rig->RunOk ("BPDISK DOS 0 0");
            stop = Go (*rig);

            Assert::IsTrue   (stop.reason == StopReason::Disk);
            Assert::AreEqual ((Byte) 0, stop.disk->logicalSector.value_or (0xFF));
            Assert::IsTrue   (stop.disk->physicalSectors == std::vector<Byte> { 0 });
            Assert::IsTrue   (stop.disk->order == DiskSectorOrder::Dos33);
        }

        TEST_METHOD (AnAddressFieldStopsAfterItsChecksum)
        {
            auto       rig = MakeRig();
            StopEvent  stop;



            rig->RunOk ("BPDISK ADDRESS 0");
            stop = Go (*rig);

            Assert::IsTrue   (stop.reason == StopReason::Disk);
            Assert::IsTrue   (stop.disk->field->kind == DiskFieldEventKind::AddressField);
            Assert::AreEqual ((Byte) 0, stop.disk->field->track);
            Assert::IsFalse  (stop.disk->field->isWrongTrack, L"the head is on track 0");
        }

        TEST_METHOD (TheMotorStartingStops)
        {
            auto       rig = MakeRig();
            StopEvent  stop;



            rig->RunOk ("BPDISK MOTOR ON");
            stop = Go (*rig);

            Assert::IsTrue (stop.reason == StopReason::Disk);
            Assert::IsTrue (stop.disk->motorEvent == DiskMotorEvent::On);
            Assert::IsTrue (stop.disk->instruction >= kBootEntry && stop.disk->instruction <= 0xC6FF, L"the boot ROM turned it on");
        }

        //  The ROM recalibrates by stepping outward against track 0: a bump at
        //  quarter track 0, which HEAD QT 0 BUMP stops on and a bump at the
        //  outer stop's breakpoint does not.
        TEST_METHOD (HeadBumpStopsOnlyAtItsPosition)
        {
            auto       zero  = MakeRig();
            auto       outer = MakeRig();
            StopEvent  stop;



            zero->RunOk ("BPDISK HEAD QT 0 BUMP");
            stop = Go (*zero);

            Assert::IsTrue   (stop.reason == StopReason::Disk);
            Assert::IsTrue   (stop.disk->isBump);
            Assert::AreEqual (0, stop.disk->quarterTrack);

            outer->RunOk ("BPDISK HEAD QT 8B BUMP");
            outer->RunOk ("BPDISK READ 0 0");
            Assert::AreEqual (1, Go (*outer).breakpointId.value_or (-1), L"the sector read, not a bump at track 0");
        }

        //  The ROM reads the checksum nibble at $C6CB only for the sector it
        //  reads: from a stop just before it, a read watchpoint on the latch
        //  taking a whole nibble and a sector read breakpoint both match the
        //  read of the checksum nibble. The
        //  stop is the watchpoint's, with the disk report beside it (FR-153).
        TEST_METHOD (AWatchpointAndADiskBreakpointOnOneInstruction)
        {
            auto       rig = MakeRig();
            StopEvent  stop;



            rig->RunOk ("BP C6CB");
            stop = Go (*rig);
            Assert::AreEqual (kRomChecksum, stop.pc, L"before the checksum read");

            rig->RunOk ("BPC 0");
            rig->RunOk ("BPMR C0EC IF VALUE >= 80");
            rig->RunOk ("BPDISK READ 0 0");
            stop = Go (*rig);

            Assert::IsTrue   (stop.reason == StopReason::Watchpoint, L"the watchpoint's stop");
            Assert::IsTrue   (stop.disk.has_value(), L"with the disk report");
            Assert::AreEqual (2,  stop.disk->breakpointId);
            Assert::AreEqual (1u, rig->session.GetBreakpoints().GetAll().at (0).hits, L"the disk entry counted the hit");
        }

        TEST_METHOD (AFalseConditionNeitherStopsNorCounts)
        {
            auto  rig = MakeRig();



            rig->RunOk ("BPDISK READ 0 0 IF 1 == 2");

            Assert::IsTrue   (Go (*rig).reason == StopReason::Budget);
            Assert::AreEqual (0u, Entry (*rig).hits);
        }

        TEST_METHOD (ACountOnlyEntryCountsAndRunsOn)
        {
            auto  rig = MakeRig();



            rig->RunOk ("BPDISK READ 0 0");
            rig->session.GetBreakpoints().TrySetFlags (Entry (*rig).id, false, false);
            rig->session.OnStopConditionsChanged();

            Assert::IsTrue (Go (*rig).reason == StopReason::Budget);
            Assert::IsTrue (Entry (*rig).hits >= 1, L"counted");
        }

        TEST_METHOD (DriveAndDiskFiltersStopNothingElse)
        {
            auto  driveTwo = MakeRig();
            auto  noDisk   = MakeRig (false);



            driveTwo->RunOk ("BPDISK D2 READ 0 0");
            noDisk->RunOk   ("BPDISK READ 0 0");

            Assert::IsTrue (Go (*driveTwo).reason == StopReason::Budget, L"the ROM reads drive 1");
            Assert::IsTrue (Go (*noDisk).reason   == StopReason::Budget, L"no disk, no field");
        }

        TEST_METHOD (APowerCycleKeepsDiskBreakpoints)
        {
            auto  rig = MakeRig();



            rig->RunOk ("BPDISK READ 0 0");
            Assert::IsTrue (Go (*rig).reason == StopReason::Disk);

            rig->machine.PowerCycle();
            rig->machine.GetRefs().diskController->SetExternalDisk (0, rig->machine.GetDiskStore().GetImage (6, 0));
            rig->machine.GetCpu()->SetPC (kBootEntry);

            Assert::AreEqual ((size_t) 1, rig->session.GetBreakpoints().GetAll().size());
            Assert::IsTrue   (Go (*rig).reason == StopReason::Disk, L"it stops again");
        }
    };
}
