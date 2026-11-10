#include "Pch.h"

#include "Devices/Disk/DiskImage.h"
#include "Machines/Apple2/Common/Disk2Controller.h"
#include "Machines/Apple2/Common/Disk2NibbleEngine.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2PeekTests
//
//  A peek at the Disk II's data latch is worked out on a copy of the drive,
//  run on from the last access. The controller stores the copy between peeks
//  and runs it on by the cycles since the last peek, so the debugger's peek
//  every frame costs one frame of the drive, not every frame since the guest
//  last read the disk. Stored or taken again, the copy has the latch a copy
//  run on from the access in one go has.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Disk2PeekTests)
{
public:

    static constexpr size_t  s_kTrackBits = 51200;



    //  One track of bits from a fixed generator, so the latch moves through
    //  every value as the drive turns.
    static void MakeDisk (DiskImage & disk)
    {
        uint32_t  state = 12345;



        disk.EnsureTrackSlots (1);
        disk.ClearQuarterTrackMap();
        disk.ResizeTrack (0, s_kTrackBits);

        for (Byte & b : disk.GetTrackBitsForWrite (0))
        {
            state = state * 1664525u + 1013904223u;
            b     = (Byte) ((state >> 24) | 0x88);
        }

        disk.SetQuarterTrackSlot (0, 0);
    }



    TEST_METHOD (ACopyRunOnTwiceIsWhereOneRunOnByTheSumIs)
    {
        DiskImage          disk;
        Disk2NibbleEngine  engine;



        MakeDisk (disk);
        engine.SetDiskImage    (&disk);
        engine.SetCurrentTrack (0);
        engine.SetMotorOn      (true);

        for (uint32_t first : { 1u, 17u, 1000u, 17030u })
        {
            Disk2NibbleEngine  split = engine.MakePeekCopy();
            Disk2NibbleEngine  whole = engine.MakePeekCopy();

            split.AdvancePeekCopy (first);
            split.AdvancePeekCopy (54321);
            whole.AdvancePeekCopy (first + 54321);

            Assert::IsTrue   (split.IsSameReadState (whole), std::format (L"run on by {} and then 54321", first).c_str());
            Assert::AreEqual ((int) whole.PeekReadLatch(), (int) split.PeekReadLatch());
        }
    }



    TEST_METHOD (ADriveThatMovedIsNotInTheSameReadState)
    {
        DiskImage          disk;
        Disk2NibbleEngine  engine;
        Disk2NibbleEngine  copy;



        MakeDisk (disk);
        engine.SetDiskImage    (&disk);
        engine.SetCurrentTrack (0);
        engine.SetMotorOn      (true);

        copy = engine.MakePeekCopy();
        Assert::IsTrue (engine.IsSameReadState (copy), L"a copy is in the drive's state");

        engine.Tick (8);
        Assert::IsFalse (engine.IsSameReadState (copy), L"a turned drive is not");

        copy = engine.MakePeekCopy();
        engine.SetMotorOn (false);
        Assert::IsFalse (engine.IsSameReadState (copy), L"nor is a stopped one");
    }



    //  Peeks at later and later cycles, with no access between them, as the
    //  debugger peeks once a frame, and then after an access: each has the
    //  latch a copy run on from the last access in one go has.
    TEST_METHOD (EachPeekHasTheLatchOfACopyRunOnFromTheLastAccess)
    {
        DiskImage        disk;
        Disk2Controller  ctrl (6);
        uint64_t         now    = 1000;
        Byte             peeked = 0;



        MakeDisk (disk);
        ctrl.SetExternalDisk   (0, &disk);
        ctrl.SetCpuCycleSource (&now);

        //  The motor on, and its spin-up, which the machine's tick drains,
        //  over before the first peek.
        (void) ctrl.Read (0xC0E9);
        now += Disk2Controller::kMotorSpinupCycles + 10;
        ctrl.Tick (Disk2Controller::kMotorSpinupCycles + 10);
        (void) ctrl.Read (0xC0EC);

        for (int frame = 1; frame <= 40; frame++)
        {
            uint64_t  delta = (uint64_t) frame * 17030;

            now = 1000 + Disk2Controller::kMotorSpinupCycles + 10 + delta;

            for (int each = 0; each < 16; each++)
            {
                Assert::IsTrue (ctrl.TryPeekIo (0xC0EC, peeked));
            }

            Assert::AreEqual ((int) ctrl.GetEngine (0).PeekLatchAfter ((uint32_t) delta), (int) peeked, std::format (L"frame {}", frame).c_str());
        }

        (void) ctrl.Read (0xC0EC);
        now += 777;

        Assert::IsTrue   (ctrl.TryPeekIo (0xC0EC, peeked));
        Assert::AreEqual ((int) ctrl.GetEngine (0).PeekLatchAfter (777), (int) peeked, L"after an access, from that access");

        ctrl.SetExternalDisk (0, nullptr);
    }
};
