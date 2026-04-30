#include "../Casso65EmuCore/Pch.h"

#include <CppUnitTest.h>

#include "Core/MemoryBus.h"
#include "Devices/DiskIIController.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskIIControllerTests
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DiskIIControllerTests)
{
public:

    TEST_METHOD (Slot6_IORange_IsCorrect)
    {
        DiskIIController disk (6);

        // Slot 6: $C080 + 6*16 = $C0E0, end = $C0EF
        Assert::AreEqual (static_cast<Word> (0xC0E0), disk.GetStart ());
        Assert::AreEqual (static_cast<Word> (0xC0EF), disk.GetEnd ());
    }

    TEST_METHOD (Slot5_IORange_IsCorrect)
    {
        DiskIIController disk (5);

        // Slot 5: $C080 + 5*16 = $C0D0, end = $C0DF
        Assert::AreEqual (static_cast<Word> (0xC0D0), disk.GetStart ());
        Assert::AreEqual (static_cast<Word> (0xC0DF), disk.GetEnd ());
    }

    TEST_METHOD (MotorOn_ViaSoftSwitch)
    {
        DiskIIController disk (6);
        MemoryBus bus;
        bus.AddDevice (&disk);

        // Read $C0E9 = motor on
        bus.ReadByte (0xC0E9);

        // Motor should be on — reading data latch shouldn't crash
        // Read Q6=0 Q7=0 (read mode): $C0EC then $C0EE
        bus.ReadByte (0xC0EC);  // Q6 = false
        bus.ReadByte (0xC0EE);  // Q7 = false

        // No disk mounted, should return 0
        Byte val = bus.ReadByte (0xC0EC);
        Assert::AreEqual (static_cast<Byte> (0), val);
    }

    TEST_METHOD (MotorOff_ViaSoftSwitch)
    {
        DiskIIController disk (6);
        MemoryBus bus;
        bus.AddDevice (&disk);

        // Motor on then off
        bus.ReadByte (0xC0E9);  // Motor on
        bus.ReadByte (0xC0E8);  // Motor off

        // Should not crash
        Byte val = bus.ReadByte (0xC0EC);
        Assert::AreEqual (static_cast<Byte> (0), val);
    }

    TEST_METHOD (PhaseHandling_MovesTrack)
    {
        DiskIIController disk (6);
        MemoryBus bus;
        bus.AddDevice (&disk);

        // Motor on
        bus.ReadByte (0xC0E9);

        // Initially at track 0 (quarterTrack 0)
        // Energize phase 1 ON: $C0E3
        bus.ReadByte (0xC0E3);

        // Phase 1 is one step ahead of phase 0, so quarterTrack should move +1
        // De-energize phase 1: $C0E2
        bus.ReadByte (0xC0E2);

        // Energize phase 2 ON: $C0E5
        bus.ReadByte (0xC0E5);

        // De-energize phase 2: $C0E4
        bus.ReadByte (0xC0E4);

        // Energize phase 3 ON: $C0E7
        bus.ReadByte (0xC0E7);

        // De-energize phase 3: $C0E6
        bus.ReadByte (0xC0E6);

        // Energize phase 0 ON: $C0E1
        bus.ReadByte (0xC0E1);

        // Each phase step moves quarterTrack by 1
        // After 4 single-phase steps, quarterTrack = 4 = track 1
        // Reading data latch should not crash
        bus.ReadByte (0xC0EC);
        bus.ReadByte (0xC0EE);
        Byte val = bus.ReadByte (0xC0EC);

        // Without disk mounted, should still return 0
        Assert::AreEqual (static_cast<Byte> (0), val);
    }

    TEST_METHOD (DriveSelect_SwitchesDrive)
    {
        DiskIIController disk (6);
        MemoryBus bus;
        bus.AddDevice (&disk);

        // Select drive 1: $C0EB
        bus.ReadByte (0xC0EB);

        // Select drive 0: $C0EA
        bus.ReadByte (0xC0EA);

        // Should not crash
        Byte val = bus.ReadByte (0xC0EC);
        Assert::AreEqual (static_cast<Byte> (0), val);
    }

    TEST_METHOD (WriteProtect_NoMountedDisk_Returns0)
    {
        DiskIIController disk (6);
        MemoryBus bus;
        bus.AddDevice (&disk);

        // Set Q6=1 (sense write protect): $C0ED
        // Set Q7=0: $C0EE
        bus.ReadByte (0xC0EE);  // Q7 = false
        Byte val = bus.ReadByte (0xC0ED);  // Q6 = true, Q6&&!Q7 = sense WP

        // No disk mounted -> returns 0x00
        Assert::AreEqual (static_cast<Byte> (0x00), val);
    }

    TEST_METHOD (Reset_ClearsState)
    {
        DiskIIController disk (6);
        MemoryBus bus;
        bus.AddDevice (&disk);

        // Activate motor and move phases
        bus.ReadByte (0xC0E9);  // Motor on
        bus.ReadByte (0xC0E3);  // Phase 1 on

        disk.Reset ();

        // After reset, I/O range should still be valid
        Assert::AreEqual (static_cast<Word> (0xC0E0), disk.GetStart ());
        Assert::AreEqual (static_cast<Word> (0xC0EF), disk.GetEnd ());

        // Reading data latch should return 0 (no nibble buffer)
        Byte val = disk.Read (0xC0EC);
        Assert::AreEqual (static_cast<Byte> (0), val);
    }

    TEST_METHOD (GetDisk_ValidDrive_ReturnsNonNull)
    {
        DiskIIController disk (6);

        DiskImage * d0 = disk.GetDisk (0);
        DiskImage * d1 = disk.GetDisk (1);

        Assert::IsNotNull (d0);
        Assert::IsNotNull (d1);
        Assert::IsFalse (d0->IsLoaded ());
        Assert::IsFalse (d1->IsLoaded ());
    }

    TEST_METHOD (GetDisk_InvalidDrive_ReturnsNull)
    {
        DiskIIController disk (6);

        DiskImage * d2 = disk.GetDisk (2);
        DiskImage * dm = disk.GetDisk (-1);

        Assert::IsNull (d2);
        Assert::IsNull (dm);
    }
};
