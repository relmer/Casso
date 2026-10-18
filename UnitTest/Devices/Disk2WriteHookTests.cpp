#include "Pch.h"
#include "Devices/Disk/DiskImage.h"
#include "Machines/Apple2/Common/Disk2Controller.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2WriteHookTests
//
//  The controller's one write hook, OnLatchLoad: every load of the data latch
//  with Q6 and Q7 set while the motor runs, spin-down included, with whether
//  write protection drops it; nothing with the motor stopped, and nothing
//  for the //c's mode-register load.
//
////////////////////////////////////////////////////////////////////////////////

namespace Disk2WriteHookTests
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



    struct LatchLoad
    {
        int   drive        = 0;
        Byte  nibble       = 0;
        int   quarterTrack = 0;
        bool  isDropped    = false;
    };



    class HookedController : public Disk2Controller
    {
    public:
        std::vector<LatchLoad>  loads;

        HookedController() : Disk2Controller (6) {}

        Byte  GetModeRegister() const { return m_iwmModeReg; }

    private:
        void  OnLatchLoad (int drive, Byte nibble, int quarterTrack, bool isDropped) override
        {
            loads.push_back ({ drive, nibble, quarterTrack, isDropped });
        }
    };



    TEST_CLASS (Disk2WriteHookTests)
    {
    public:

        //  Motor on and at speed, then Q6 and Q7 on: the write mode a DOS
        //  write routine sets up.
        static void StartWriting (Disk2Controller & ctrl)
        {
            ctrl.Read (kMotorOn);
            ctrl.Tick (Disk2Controller::kMotorSpinupCycles);
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

        static std::vector<Byte> ReadTrackBits (DiskImage & image)
        {
            std::vector<Byte>  bits;



            for (size_t bit = 0; bit < kTrackBits; bit++)
            {
                bits.push_back (image.ReadBit (0, bit));
            }

            return bits;
        }

        TEST_METHOD (EveryLoadOnAWritableDiskIsReported)
        {
            auto  ctrl = std::make_unique<HookedController>();



            ctrl->GetDisk (0)->ResizeTrack (0, kTrackBits);
            StartWriting (*ctrl);
            WriteField (*ctrl);

            Assert::AreEqual (s_kField.size(), ctrl->loads.size(), L"one report per latch load");

            for (size_t i = 0; i < s_kField.size(); i++)
            {
                Assert::AreEqual (s_kField[i], ctrl->loads[i].nibble);
                Assert::AreEqual (0,           ctrl->loads[i].drive);
                Assert::AreEqual (0,           ctrl->loads[i].quarterTrack);
                Assert::IsFalse  (ctrl->loads[i].isDropped, L"a writable disk takes the bits");
            }
        }

        TEST_METHOD (AProtectedDiskIsUnchangedAndEveryLoadIsReportedDropped)
        {
            auto               ctrl   = std::make_unique<HookedController>();
            std::vector<Byte>  before;



            ctrl->GetDisk (0)->ResizeTrack (0, kTrackBits);
            ctrl->GetDisk (0)->SetWriteProtected (true);
            before = ReadTrackBits (*ctrl->GetDisk (0));

            StartWriting (*ctrl);
            WriteField (*ctrl);

            Assert::IsTrue   (before == ReadTrackBits (*ctrl->GetDisk (0)), L"the track's bits are the ones before the write");
            Assert::AreEqual (s_kField.size(), ctrl->loads.size());

            for (const LatchLoad & load : ctrl->loads)
            {
                Assert::IsTrue (load.isDropped, L"write protection drops every load");
            }
        }

        TEST_METHOD (ALoadDuringTheSpinDownIsReported)
        {
            auto  ctrl = std::make_unique<HookedController>();



            ctrl->GetDisk (0)->ResizeTrack (0, kTrackBits);
            StartWriting (*ctrl);
            ctrl->Read  (kMotorOff);
            ctrl->Write (kQ6On, 0xFF);

            Assert::AreEqual ((size_t) 1, ctrl->loads.size(), L"the disk turns during the spin-down");
        }

        TEST_METHOD (ALoadWithTheMotorStoppedIsNotReported)
        {
            auto  ctrl = std::make_unique<HookedController>();



            ctrl->GetDisk (0)->ResizeTrack (0, kTrackBits);
            ctrl->Read  (kQ6On);
            ctrl->Read  (kQ7On);
            ctrl->Write (kQ6On, 0xFF);

            Assert::IsTrue (ctrl->loads.empty(), L"a stopped disk takes nothing");
        }

        TEST_METHOD (TheIicModeRegisterLoadIsNotReported)
        {
            auto  ctrl = std::make_unique<HookedController>();



            ctrl->SetIwmMode (true);
            ctrl->Read  (kQ7Off);
            ctrl->Read  (kQ6On);
            ctrl->Read  (kQ7On);
            ctrl->Write (kQ6On, 0x0F);

            Assert::AreEqual ((Byte) 0x0F, ctrl->GetModeRegister(), L"the store loaded the mode register");
            Assert::IsTrue   (ctrl->loads.empty(), L"a mode-register load is not a latch load");
        }
    };
}
