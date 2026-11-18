#include "Pch.h"
#include "Devices/Disk/DiskImage.h"
#include "Machines/Apple2/Common/Disk2Controller.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2LatchLoadRecordTests
//
//  A guest write in the recent nibbles: an entry for every load of the data
//  latch with Q6 and Q7 set while the motor runs, spin-down included, with
//  whether write protection drops it; none with the motor stopped, and none
//  for the //c's mode-register load.
//
////////////////////////////////////////////////////////////////////////////////

namespace Disk2LatchLoadRecordTests
{
    static constexpr Word    kMotorOff      = 0xC0E8;
    static constexpr Word    kMotorOn       = 0xC0E9;
    static constexpr Word    kQ6On          = 0xC0ED;
    static constexpr Word    kQ7Off         = 0xC0EE;
    static constexpr Word    kQ7On          = 0xC0EF;
    static constexpr int     kBitsPerNibble = 8;
    static constexpr size_t  kTrackBits     = 4096;

    //  A data field's prologue, a few body nibbles and its epilogue.
    static const std::vector<Byte>  s_kField = { 0xD5, 0xAA, 0xAD, 0x96, 0x97, 0x9A, 0x9B, 0xDE, 0xAA, 0xEB };



    TEST_CLASS (Disk2LatchLoadRecordTests)
    {
    public:

        //  A controller with a writable disk in drive 1, one track long.
        static std::unique_ptr<Disk2Controller> MakeController()
        {
            auto         ctrl = std::make_unique<Disk2Controller> (6);
            DiskImage  * disk = ctrl->GetDrive (0).GetDisk();



            disk->ResizeTrack (0, kTrackBits);
            disk->SetLoadedForTest (true, false);

            return ctrl;
        }

        //  Motor on and at speed, then Q6 and Q7 on: the write mode a DOS
        //  write routine sets up.
        static void StartWriting (Disk2Controller & ctrl)
        {
            ctrl.Read (kMotorOn);
            ctrl.Tick (Disk2CardSequencer::kMotorSpinupCycles);
            ctrl.Read (kQ6On);
            ctrl.Read (kQ7On);
        }

        //  Each nibble stored to Q6H, with a nibble's bit cells between.
        static void WriteField (Disk2Controller & ctrl)
        {
            for (Byte nibble : s_kField)
            {
                ctrl.Write (kQ6On, nibble);
                ctrl.Tick (Disk2NibbleEngine::kCyclesPerBit * kBitsPerNibble);
            }
        }

        //  The written entries in the recent nibbles, oldest first.
        static std::vector<Disk2NibbleRecord::Entry> GetLoads (const Disk2Controller & ctrl)
        {
            const Disk2NibbleRecord               & record = ctrl.GetRecorder().GetNibbleRecord();
            std::vector<Disk2NibbleRecord::Entry>   loads;
            uint64_t                                index  = 0;



            for (index = record.GetCount() - record.GetSize(); index < record.GetCount(); index++)
            {
                if ((record.GetEntry (index).flags & Disk2NibbleRecord::kWritten) != 0)
                {
                    loads.push_back (record.GetEntry (index));
                }
            }

            return loads;
        }

        static std::vector<Byte> ReadTrackBits (DiskImage & image)
        {
            std::vector<Byte>  bits;



            for (size_t bit = 0; bit < kTrackBits; bit++)
            {
                bits.push_back (image.ReadBit (0, bit));
            }

            return bits;
        }

        TEST_METHOD (EveryLoadOnAWritableDiskIsRecorded)
        {
            auto                                   ctrl  = MakeController();
            std::vector<Disk2NibbleRecord::Entry>  loads;



            StartWriting (*ctrl);
            WriteField (*ctrl);

            loads = GetLoads (*ctrl);
            Assert::AreEqual (s_kField.size(), loads.size(), L"one entry per latch load");

            for (size_t i = 0; i < s_kField.size(); i++)
            {
                Assert::AreEqual (s_kField[i], loads[i].nibble);
                Assert::AreEqual ((Byte) 0, (Byte) (loads[i].flags & Disk2NibbleRecord::kDrive2), L"drive 1");
                Assert::AreEqual ((Byte) 0, (Byte) (loads[i].flags & Disk2NibbleRecord::kDropped), L"a writable disk takes the bits");
            }
        }

        TEST_METHOD (AProtectedDiskIsUnchangedAndEveryLoadIsRecordedDropped)
        {
            auto                                   ctrl   = MakeController();
            DiskImage                            * disk   = ctrl->GetDrive (0).GetDisk();
            std::vector<Byte>                      before;
            std::vector<Disk2NibbleRecord::Entry>  loads;



            disk->SetWriteProtected (true);
            before = ReadTrackBits (*disk);

            StartWriting (*ctrl);
            WriteField (*ctrl);

            loads = GetLoads (*ctrl);
            Assert::IsTrue   (before == ReadTrackBits (*disk), L"the track's bits are the ones before the write");
            Assert::AreEqual (s_kField.size(), loads.size());

            for (const Disk2NibbleRecord::Entry & load : loads)
            {
                Assert::AreNotEqual ((Byte) 0, (Byte) (load.flags & Disk2NibbleRecord::kDropped), L"write protection drops every load");
            }
        }

        TEST_METHOD (ALoadDuringTheSpinDownIsRecorded)
        {
            auto  ctrl = MakeController();



            StartWriting (*ctrl);
            ctrl->Read  (kMotorOff);
            ctrl->Write (kQ6On, 0xFF);

            Assert::AreEqual ((size_t) 1, GetLoads (*ctrl).size(), L"the disk turns during the spin-down");
        }

        TEST_METHOD (ALoadWithTheMotorStoppedIsNotRecorded)
        {
            auto  ctrl = MakeController();



            ctrl->Read  (kQ6On);
            ctrl->Read  (kQ7On);
            ctrl->Write (kQ6On, 0xFF);

            Assert::IsTrue (GetLoads (*ctrl).empty(), L"a stopped disk takes nothing");
        }

        TEST_METHOD (TheIicModeRegisterLoadIsNotRecorded)
        {
            auto  ctrl = MakeController();



            ctrl->SetIwmMode (true);
            ctrl->Read  (kQ7Off);
            ctrl->Read  (kQ6On);
            ctrl->Read  (kQ7On);
            ctrl->Write (kQ6On, 0x0F);

            Assert::AreEqual ((Byte) 0x0F, ctrl->GetIwmPort().GetIwm().GetMode(), L"the store loaded the mode register");
            Assert::IsTrue   (GetLoads (*ctrl).empty(), L"a mode-register load is not a latch load");
        }
    };
}
