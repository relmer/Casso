#include "Pch.h"

#include "Debugger/DiskValueSymbols.h"
#include "Debugger/Handlers/BreakpointHandlers.h"
#include "Debugger/Handlers/ConfigHandlers.h"
#include "Devices/Disk/DiskFieldFormat.h"
#include "Devices/Disk/DiskImageStore.h"
#include "HandlerTestRig.h"
#include "Machines/Apple2/Common/Disk2Controller.h"
#include "Machines/Apple2/Common/Disk2NibbleRecord.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "MockDebugTarget.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskValueTests
//
//  The DISK. values on a //e with a blank DOS 3.3 disk in drive 1, started
//  at the boot ROM, and on a target whose nibble record a test fills.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (DiskValueTests)
    {
    public:

        using Rig = MachineHandlerRig<ConfigHandlers>;

        struct ValueRig
        {
            Rig                 rig { "Apple2e", TestMachine::Slots::DiskOnly };
            BreakpointHandlers  breakpoints;

            ValueRig()
            {
                std::vector<Byte>  blank (NibblizationLayer::kImageByteSize, 0);



                rig.session.AddHandler (&breakpoints);
                rig.machine.GetDiskStore().SetFlushSink ([] (const std::string &, const std::vector<Byte> &) { return S_OK; });
                Assert::AreEqual (S_OK, rig.machine.GetDiskStore().MountFromBytes (6, 0, "blank.dsk", DiskFormat::Dsk, blank));
                Controller().SetExternalDisk (0, rig.machine.GetDiskStore().GetImage (6, 0));
                rig.machine.GetCpu()->SetPC (0xC600);
                rig.session.SetBudget (3'000'000);
            }

            Disk2Controller & Controller()
            {
                return *rig.machine.GetRefs().diskController;
            }

            Word Calc (const std::string & expression)
            {
                return std::get<CalcData> (rig.RunOk ("CALC " + expression).data).value;
            }
        };

        //  A target whose disk state and nibble record a test sets.
        class RecordTarget : public MockDebugTarget
        {
        public:
            DebugDiskState     state;
            Disk2NibbleRecord  record;

            bool                       TryGetDiskState (DebugDiskState & out) const override { out = state; return true; }
            const Disk2NibbleRecord  * GetDiskRecord   () const override                     { return &record; }

            void AddAddressField (int drive, Byte volume, Byte track, Byte sector)
            {
                Byte  odd  = 0;
                Byte  even = 0;



                for (Byte nibble : { 0xD5, 0xAA, 0x96 })
                {
                    record.Add (drive, nibble, 0, 0);
                }

                for (Byte value : { volume, track, sector, DiskFieldFormat::ComputeAddressChecksum (volume, track, sector) })
                {
                    DiskFieldFormat::Encode44 (value, odd, even);
                    record.Add (drive, odd,  0, 0);
                    record.Add (drive, even, 0, 0);
                }
            }

            Word Resolve (const std::string & name) const
            {
                Word  value = 0;



                Assert::IsTrue (DiskValueSymbols::TryResolve (name, *this, value));
                return value;
            }
        };


        TEST_METHOD (EachValueIsTheControllers)
        {
            ValueRig  values;



            Assert::AreEqual ((Word) 1,      values.Calc ("DISK.DRIVE"));
            Assert::AreEqual ((Word) values.Controller().GetQuarterTrack(),  values.Calc ("DISK.QTRACK"));
            Assert::AreEqual ((Word) values.Controller().GetQuarterTrack (0), values.Calc ("DISK.QTRACK1"));
            Assert::AreEqual ((Word) values.Controller().GetQuarterTrack (1), values.Calc ("DISK.QTRACK2"));
            Assert::AreEqual ((Word) 0,      values.Calc ("DISK.MOTOR"));
            Assert::AreEqual ((Word) 0,      values.Calc ("DISK.WRITING"));
            Assert::AreEqual ((Word) (values.Controller().GetDisk (0)->IsWriteProtected() ? 1 : 0), values.Calc ("DISK.PROTECTED"));
            Assert::AreEqual ((Word) 1,      values.Calc ("DISK.INSERTED"));
            Assert::AreEqual ((Word) 0xFFFF, values.Calc ("DISK.ATRACK"), L"no field read yet");

            values.rig.RunOk ("BPDISK READ 0 0");
            values.rig.RunOk ("G");

            Assert::AreEqual ((Word) 1,      values.Calc ("disk.motor"), L"the boot ROM turned it on");
            Assert::AreEqual ((Word) 0,      values.Calc ("DISK.ATRACK"));
            Assert::AreEqual ((Word) 0,      values.Calc ("DISK.ASECTOR"));
            Assert::AreEqual ((Word) 0xFE,   values.Calc ("DISK.AVOLUME"));
        }

        //  The IF is set before any field is read, so its values are $FFFF
        //  then, and it reads at the stop what CALC reads after it.
        TEST_METHOD (AnIfReadsWhatCalcReads)
        {
            ValueRig  values;



            values.rig.RunOk ("BPDISK READ * * IF DISK.ASECTOR == 0 && DISK.QTRACK == 0");
            values.rig.RunOk ("G");

            Assert::IsTrue   (values.rig.LastStop().reason == StopReason::Disk);
            Assert::AreEqual ((Word) 0, values.Calc ("DISK.ASECTOR"));
            Assert::AreEqual ((Word) 0, values.Calc ("DISK.QTRACK"));
        }

        TEST_METHOD (AProgramSymbolOfTheSameNameIsHidden)
        {
            ValueRig  values;



            values.rig.session.GetSymbols().Add (SymbolTableId::User, "DISK.DRIVE", 0x1234);

            Assert::AreEqual ((Word) 1, values.Calc ("DISK.DRIVE"));
        }

        TEST_METHOD (TheAddressFieldIsTheSelectedDrives)
        {
            RecordTarget  target;



            target.AddAddressField (0, 0xFE, 0x11, 0x05);
            target.AddAddressField (1, 0x01, 0x22, 0x0A);

            Assert::AreEqual ((Word) 0x11, target.Resolve ("DISK.ATRACK"));
            Assert::AreEqual ((Word) 0x05, target.Resolve ("DISK.ASECTOR"));
            Assert::AreEqual ((Word) 0xFE, target.Resolve ("DISK.AVOLUME"));

            target.state.selectedDrive = 1;
            Assert::AreEqual ((Word) 0x22, target.Resolve ("DISK.ATRACK"));
        }

        TEST_METHOD (OnlyTheOtherDrivesFieldGivesNone)
        {
            RecordTarget  target;



            target.AddAddressField (1, 0xFE, 0x11, 0x05);

            Assert::AreEqual (DiskValueSymbols::kNoField, target.Resolve ("DISK.ATRACK"));
        }

        TEST_METHOD (ABadChecksumIsPassedOver)
        {
            RecordTarget  target;
            Byte          odd    = 0;
            Byte          even   = 0;



            target.AddAddressField (0, 0xFE, 0x03, 0x01);

            for (Byte nibble : { 0xD5, 0xAA, 0x96 })
            {
                target.record.Add (0, nibble, 0, 0);
            }

            for (Byte value : { 0xFE, 0x04, 0x02, 0x00 })
            {
                DiskFieldFormat::Encode44 (value, odd, even);
                target.record.Add (0, odd,  0, 0);
                target.record.Add (0, even, 0, 0);
            }

            Assert::AreEqual ((Word) 0x03, target.Resolve ("DISK.ATRACK"));
        }

        //  Reading every value moves no switch and adds no record entry.
        TEST_METHOD (ReadingTheValuesChangesNothing)
        {
            ValueRig  values;
            uint64_t  count  = 0;
            bool      isQ6   = false;
            bool      isQ7   = false;



            values.rig.RunOk ("BPDISK READ 0 0");
            values.rig.RunOk ("G");

            count = values.Controller().GetNibbleRecord().GetCount();
            isQ6  = values.Controller().IsQ6();
            isQ7  = values.Controller().IsQ7();

            for (const char * name : { "DISK.DRIVE", "DISK.QTRACK", "DISK.QTRACK1", "DISK.QTRACK2", "DISK.MOTOR", "DISK.WRITING",
                                       "DISK.PROTECTED", "DISK.INSERTED", "DISK.ATRACK", "DISK.ASECTOR", "DISK.AVOLUME" })
            {
                values.Calc (name);
            }

            Assert::AreEqual (count, values.Controller().GetNibbleRecord().GetCount());
            Assert::AreEqual (isQ6,  values.Controller().IsQ6());
            Assert::AreEqual (isQ7,  values.Controller().IsQ7());
        }

        TEST_METHOD (WithNoControllerTheNamesAreUnknown)
        {
            Rig  rig;



            Assert::AreNotEqual ((int) CommandStatus::Ok, (int) rig.Run ("CALC DISK.DRIVE").status);
            Assert::AreNotEqual ((int) CommandStatus::Ok, (int) rig.Run ("CALC DISK.QTRACK").status);
        }

        TEST_METHOD (ASwitchInAConditionIsStillAnError)
        {
            ValueRig  values;
            Reply     reply;



            reply = values.rig.Run ("BPDISK READ 0 0 IF *C0EC == 0");

            Assert::AreNotEqual ((int) CommandStatus::Ok, (int) reply.status);
            Assert::AreNotEqual (std::string ("invalid arguments"), reply.error.label, TextEncoding::NarrowToWide (reply.error.detail).c_str());
        }
    };
}
