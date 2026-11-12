#include "Pch.h"

#include "GuestSession.h"
#include "Core/TextEncoding.h"
#include "TestMachine.h"
#include "Debugger/DebugSession.h"
#include "Debugger/Handlers/BreakpointHandlers.h"
#include "Debugger/IDebugNotificationSink.h"
#include "Debugger/MachineDebugTarget.h"
#include "Machines/Apple2/Common/Disk2Controller.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskBreakpointMachineTests
//
//  Disk breakpoint kinds on a real guest, which the unit tests don't run:
//  DOS 3.3 writing a file, to a disk and to a write-protected one; ProDOS
//  reading its volume directory's block; and a disk put in and taken out
//  while the machine runs. Each entry counts only, so the guest runs to its
//  prompt, and its hits are where it matched.
//
//  Covered elsewhere and not repeated: sector, DOS sector, address field,
//  head, motor and select stops (DiskBreakpointTests), RWTS and driver calls
//  (DiskCallBreakpointTests, DiskReadProbeTests), reads counted per sector
//  (DiskReadProbeTests) and disk stops in reverse (ReverseDiskBreakpointTests).
//
////////////////////////////////////////////////////////////////////////////////

namespace DiskBreakpointMachineTests
{
    static constexpr const char *  kpszMachine  = "Apple2eEnhanced";
    static constexpr uint64_t      kSettle      = 2'000'000;
    static constexpr int           kSlot        = 6;
    static constexpr int           kDrive1      = 0;





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  Rig
    //
    //  A machine with a debug session that runs free, so a count-only entry
    //  counts while the guest runs on the host, as it does in the window
    //  with the machine running.
    //
    ////////////////////////////////////////////////////////////////////////////////

    struct Rig
    {
        TestMachine                machine { kpszMachine };
        MachineDebugTarget         target  { machine };
        NullDebugNotificationSink  sink;
        DebugSession               session { target, sink, RunState::FreeRunning };
        BreakpointHandlers         handlers;

        Rig()
        {
            session.AddHandler (&handlers);
            machine.GetDiskStore().SetFlushSink ([] (const std::string &, const std::vector<Byte> &) { return S_OK; });
        }

        //  A count-only entry from a BPDISK line, by its index in the table.
        size_t  Arm (const std::string & line)
        {
            size_t  index = session.GetBreakpoints().GetAll().size();

            Assert::AreEqual ((int) CommandStatus::Ok, (int) session.ExecuteLine (line).status, TextEncoding::NarrowToWide (line).c_str());
            session.GetBreakpoints().TrySetFlags (session.GetBreakpoints().GetAll().at (index).id, false, false);
            session.OnStopConditionsChanged();
            return index;
        }

        uint32_t  Hits (size_t index)
        {
            return session.GetBreakpoints().GetAll().at (index).hits;
        }
    };





    TEST_CLASS (DiskBreakpointMachineTests)
    {
    public:

        //  SAVE writes the catalog on track $11 and the VTOC in its sector 0:
        //  a write, a written sector and a written track each count there,
        //  and none counts on track 0 or as a blocked write.
        TEST_METHOD (ADos33SaveCountsItsWrites)
        {
            Rig     rig;
            size_t  anyWrite = 0;
            size_t  vtoc     = 0;
            size_t  catalog  = 0;
            size_t  bootSide = 0;
            size_t  blocked  = 0;
            size_t  logical  = 0;



            GuestSession::BootToPrompt (rig.machine, GuestSession::RequireDos33Master());
            (void) GuestSession::TypeAndCollect (rig.machine, "10 PRINT 1");

            anyWrite = rig.Arm ("BPDISK WRITE");
            vtoc     = rig.Arm ("BPDISK WRITE 11 0");
            catalog  = rig.Arm ("BPDISK WRITE T 11");
            bootSide = rig.Arm ("BPDISK WRITE T 0");
            blocked  = rig.Arm ("BPDISK WRITE BLOCKED");
            logical  = rig.Arm ("BPDISK DOS 11 0 WRITTEN");

            (void) GuestSession::TypeAndCollect (rig.machine, "SAVE X");

            Assert::IsTrue   (rig.Hits (anyWrite) >= 1, L"DOS switched the drive to write mode");
            Assert::IsTrue   (rig.Hits (vtoc)     >= 1, L"the VTOC written");
            Assert::IsTrue   (rig.Hits (catalog)  >= 1, L"the catalog track written");
            Assert::AreEqual (0u, rig.Hits (bootSide), L"nothing written on track 0");
            Assert::AreEqual (0u, rig.Hits (blocked),  L"the disk isn't write-protected");
            Assert::IsTrue   (rig.Hits (logical)  >= 1, L"the VTOC, as a logical sector, written");
        }


        //  The same SAVE to a write-protected disk: RWTS reads the protect
        //  sense before it switches to write mode and returns the error, so
        //  no write is blocked and no sector is written. A blocked write is a
        //  program writing without that check (DiskBreakpointTests).
        TEST_METHOD (ASaveToAProtectedDiskWritesNothing)
        {
            Rig     rig;
            size_t  blocked = 0;
            size_t  sector  = 0;



            GuestSession::BootToPrompt (rig.machine, GuestSession::RequireDos33Master());
            rig.machine.GetDiskStore().GetImage (kSlot, kDrive1)->SetWriteProtected (true);

            blocked = rig.Arm ("BPDISK WRITE BLOCKED");
            sector  = rig.Arm ("BPDISK WRITE * *");

            (void) GuestSession::TypeAndCollect (rig.machine, "SAVE X");

            Assert::AreEqual (0u, rig.Hits (blocked), L"RWTS checked the protect sense first");
            Assert::AreEqual (0u, rig.Hits (sector),  L"and wrote no sector");
        }


        //  A ProDOS boot reads its volume directory's key block, block 2.
        TEST_METHOD (AProDosBootCountsItsVolumeDirectoryBlock)
        {
            Rig     rig;
            size_t  directory = 0;



            directory = rig.Arm ("BPDISK BLOCK 2");

            GuestSession::MountAndBoot (rig.machine, GuestSession::RequireProDosUsersDisk());

            Assert::IsTrue (rig.Hits (directory) >= 1, L"the volume directory read");
        }


        //  A disk put in drive 1 and taken out while the machine runs each
        //  count once, on drive 1 alone.
        TEST_METHOD (AnInsertAndAnEjectWhileRunningCountOnce)
        {
            Rig                rig;
            std::vector<Byte>  blank (GuestSession::kImageBytes, 0);
            size_t             insert   = 0;
            size_t             eject    = 0;
            size_t             driveTwo = 0;



            insert   = rig.Arm ("BPDISK INSERT");
            eject    = rig.Arm ("BPDISK EJECT");
            driveTwo = rig.Arm ("BPDISK D2 INSERT");

            (void) rig.machine.RunCycles (kSettle);

            Assert::AreEqual (S_OK, rig.machine.GetDiskStore().MountFromBytes (kSlot, kDrive1, "blank.dsk", DiskFormat::Dsk, blank));
            rig.machine.GetRefs().diskController->SetExternalDisk (kDrive1, rig.machine.GetDiskStore().GetImage (kSlot, kDrive1));
            rig.machine.GetRefs().diskController->NotifyDiskInserted (kDrive1);
            (void) rig.machine.RunCycles (kSettle);

            rig.machine.GetRefs().diskController->SetExternalDisk (kDrive1, nullptr);
            rig.machine.GetRefs().diskController->NotifyDiskEjected (kDrive1);
            (void) rig.machine.RunCycles (kSettle);

            Assert::AreEqual (1u, rig.Hits (insert));
            Assert::AreEqual (1u, rig.Hits (eject));
            Assert::AreEqual (0u, rig.Hits (driveTwo), L"drive 2 had no change");
        }
    };
}
