#include "Pch.h"

#include "Machines/Apple2/Apple2cPlus/Apple2cPlusAccelerator.h"
#include "Machines/Apple2/Apple2cPlus/MigChip.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Apple2cPlusDeviceTests
//
//  The //c Plus's MIG and accelerator outside a machine: the MIG's RAM window and
//  its drive and head switches, and the accelerator's registers, its cycle
//  scaling and its slowdowns.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Apple2cPlusDeviceTests)
{
public:

    static constexpr uint32_t  kBusHz = 1'020'484;


    static Byte Read (MigChip & mig, Word address)
    {
        Byte  value = 0xEE;



        mig.OnRomWindowAccess (address, false, 0, value);

        return value;
    }


    static void Write (MigChip & mig, Word address, Byte value)
    {
        Byte  ignored = 0;



        mig.OnRomWindowAccess (address, true, value, ignored);
    }


    //  $CE00-$CE1F is 32 bytes at the pointer; $CE20-$CE3F advances it $20,
    //  wrapping at 2 KB; $CEA0 resets it.
    TEST_METHOD (Mig_RamWindowPages)
    {
        MigChip  mig (nullptr);



        Write (mig, 0xCE05, 0x11);
        Assert::AreEqual ((Byte) 0x11, Read (mig, 0xCE05));

        Write (mig, 0xCE25, 0x22);
        Assert::AreEqual ((Byte) 0x22, mig.GetRam()[0x05], L"the advancing window writes at the old pointer");

        Write (mig, 0xCE05, 0x33);
        Assert::AreEqual ((Byte) 0x33, mig.GetRam()[0x25], L"then the pointer is $20 on");

        for (int i = 0; i < 63; i++)
        {
            Read (mig, 0xCE20);
        }

        Write (mig, 0xCE00, 0x44);
        Assert::AreEqual ((Byte) 0x44, mig.GetRam()[0x000], L"the pointer wraps at 2 KB");

        Read (mig, 0xCE20);
        Read (mig, 0xCEA0);
        Write (mig, 0xCE01, 0x55);
        Assert::AreEqual ((Byte) 0x55, mig.GetRam()[0x001], L"$CEA0 resets the pointer");
    }


    TEST_METHOD (Mig_DriveAndHeadSwitches)
    {
        MigChip  mig (nullptr);
        int      resets = 0;



        mig.SetIwmReset ([&resets] () { resets++; });

        Write (mig, 0xCC80, 0);
        Assert::IsTrue (mig.IsInternalOn());
        Write (mig, 0xCCC0, 0);
        Assert::IsFalse (mig.IsInternalOn());

        Read (mig, 0xCE60);
        Assert::IsTrue (mig.IsHeadOne());
        Read (mig, 0xCE40);
        Assert::IsFalse (mig.IsHeadOne());

        Write (mig, 0xCE60, 0);
        Assert::IsTrue (mig.IsSel35());
        Write (mig, 0xCE40, 0);
        Assert::IsFalse (mig.IsSel35());

        Write (mig, 0xCC40, 0);
        Assert::AreEqual (1, resets);

        Assert::IsTrue  (mig.HandlesPage (0xCC));
        Assert::IsTrue  (mig.HandlesPage (0xCE));
        Assert::IsFalse (mig.HandlesPage (0xCD));
    }


    static void IoWrite (Apple2cPlusAccelerator & accel, Word address, Byte value)
    {
        Byte  ignored = 0;



        accel.OnIoAccess (address, true, value, ignored);
    }


    static void Unlock (Apple2cPlusAccelerator & accel)
    {
        for (int i = 0; i < 4; i++)
        {
            IoWrite (accel, 0xC05A, 0x5A);
        }
    }


    //  In fast mode an instruction takes 1.0205 MHz over 4 MHz of its
    //  cycles in the machine's clock.
    TEST_METHOD (Accelerator_ScalesCyclesWhenFast)
    {
        uint64_t                cycles = 0;
        Apple2cPlusAccelerator  accel (&cycles, kBusHz);
        uint64_t                total  = 0;



        for (int i = 0; i < 4000; i++)
        {
            total += accel.Scale (4);
        }

        Assert::AreEqual ((uint64_t) 4081, total, L"16,000 CPU cycles at 4 MHz are 4,081.9 at 1.0205 MHz");

        Unlock (accel);
        IoWrite (accel, 0xC05A, 0x00);
        Assert::IsFalse (accel.IsFast());
        Assert::AreEqual ((Byte) 4, accel.Scale (4), L"normal speed takes every cycle");
    }


    //  Four $5A writes unlock; $08 at $C05B selects fast; $A5 locks, after
    //  which the speed registers are ignored. Another write breaks the
    //  unlock sequence.
    TEST_METHOD (Accelerator_UnlockFastAndLock)
    {
        uint64_t                cycles = 0;
        Apple2cPlusAccelerator  accel (&cycles, kBusHz);
        Byte                    value  = 0;



        IoWrite (accel, 0xC05A, 0x5A);
        IoWrite (accel, 0xC05A, 0x5A);
        IoWrite (accel, 0xC030, 0x00);
        IoWrite (accel, 0xC05A, 0x5A);
        IoWrite (accel, 0xC05A, 0x5A);
        Assert::IsFalse (accel.IsUnlocked(), L"the speaker write broke the sequence");

        Unlock (accel);
        Assert::IsTrue (accel.IsUnlocked());

        IoWrite (accel, 0xC05A, 0x08);
        Assert::IsFalse (accel.IsFast());
        IoWrite (accel, 0xC05B, 0x08);
        Assert::IsTrue (accel.IsFast());

        IoWrite (accel, 0xC05C, 0x67);
        Assert::AreEqual ((Byte) 0x67, accel.GetMask());
        Assert::IsTrue (accel.OnIoAccess (0xC05C, false, 0, value));
        Assert::AreEqual ((Byte) 0x67, value);

        IoWrite (accel, 0xC05A, 0xA5);
        Assert::IsFalse (accel.IsUnlocked());
        IoWrite (accel, 0xC05C, 0x00);
        Assert::AreEqual ((Byte) 0x67, accel.GetMask(), L"locked, the mask is ignored");
        Assert::IsFalse (accel.OnIoAccess (0xC05C, false, 0, value), L"locked, reads go to the IOU");
    }


    //  A masked slot's I/O slows the machine for 52 ms and the speaker for
    //  5 ms; $C05E bit 7 turns the slowdowns off.
    TEST_METHOD (Accelerator_Slowdowns)
    {
        uint64_t                cycles = 0;
        Apple2cPlusAccelerator  accel (&cycles, kBusHz);
        Byte                    value  = 0;



        Unlock (accel);
        IoWrite (accel, 0xC05C, 0x41);

        accel.OnIoAccess (0xC0E0, false, 0, value);
        Assert::IsTrue (accel.IsSlowedNow(), L"slot 6 is masked");

        cycles = kBusHz * 51 / 1000;
        Assert::IsTrue (accel.IsSlowedNow());
        cycles = kBusHz * 53 / 1000;
        Assert::IsFalse (accel.IsSlowedNow());

        accel.OnIoAccess (0xC030, false, 0, value);
        Assert::IsTrue (accel.IsSlowedNow());
        cycles += kBusHz * 6 / 1000;
        Assert::IsFalse (accel.IsSlowedNow(), L"the speaker's slowdown is 5 ms");

        accel.OnIoAccess (0xC0A0, false, 0, value);
        Assert::IsFalse (accel.IsSlowedNow(), L"slot 2 isn't masked");

        IoWrite (accel, 0xC05E, 0x80);
        accel.OnIoAccess (0xC0E0, false, 0, value);
        Assert::IsFalse (accel.IsSlowedNow(), L"slowdowns are off");
    }
};
