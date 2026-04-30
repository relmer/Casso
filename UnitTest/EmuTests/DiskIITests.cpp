#include "../Casso65EmuCore/Pch.h"

#include <CppUnitTest.h>

#include "Core/MemoryBus.h"
#include "Devices/DiskIIController.h"

// DiskIIController embeds two DiskImage objects (~143KB each) which exceed
// the C6262 stack-size threshold even when heap-allocated via make_unique,
// because the analysis counts the constructor frame.
#pragma warning (disable: 6262)

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
        auto disk = std::make_unique<DiskIIController> (6);

        Assert::AreEqual (static_cast<Word> (0xC0E0), disk->GetStart ());
        Assert::AreEqual (static_cast<Word> (0xC0EF), disk->GetEnd ());
    }

    TEST_METHOD (Slot5_IORange_IsCorrect)
    {
        auto disk = std::make_unique<DiskIIController> (5);

        Assert::AreEqual (static_cast<Word> (0xC0D0), disk->GetStart ());
        Assert::AreEqual (static_cast<Word> (0xC0DF), disk->GetEnd ());
    }

    TEST_METHOD (MotorOn_ViaSoftSwitch)
    {
        auto disk = std::make_unique<DiskIIController> (6);
        MemoryBus bus;
        bus.AddDevice (disk.get ());

        bus.ReadByte (0xC0E9);

        bus.ReadByte (0xC0EC);
        bus.ReadByte (0xC0EE);

        Byte val = bus.ReadByte (0xC0EC);
        Assert::AreEqual (static_cast<Byte> (0), val);
    }

    TEST_METHOD (MotorOff_ViaSoftSwitch)
    {
        auto disk = std::make_unique<DiskIIController> (6);
        MemoryBus bus;
        bus.AddDevice (disk.get ());

        bus.ReadByte (0xC0E9);
        bus.ReadByte (0xC0E8);

        Byte val = bus.ReadByte (0xC0EC);
        Assert::AreEqual (static_cast<Byte> (0), val);
    }

    TEST_METHOD (PhaseHandling_MovesTrack)
    {
        auto disk = std::make_unique<DiskIIController> (6);
        MemoryBus bus;
        bus.AddDevice (disk.get ());

        bus.ReadByte (0xC0E9);

        // Step through 4 phases to move one full track
        bus.ReadByte (0xC0E3);  // Phase 1 on
        bus.ReadByte (0xC0E2);  // Phase 1 off
        bus.ReadByte (0xC0E5);  // Phase 2 on
        bus.ReadByte (0xC0E4);  // Phase 2 off
        bus.ReadByte (0xC0E7);  // Phase 3 on
        bus.ReadByte (0xC0E6);  // Phase 3 off
        bus.ReadByte (0xC0E1);  // Phase 0 on

        bus.ReadByte (0xC0EC);
        bus.ReadByte (0xC0EE);
        Byte val = bus.ReadByte (0xC0EC);

        Assert::AreEqual (static_cast<Byte> (0), val);
    }

    TEST_METHOD (DriveSelect_SwitchesDrive)
    {
        auto disk = std::make_unique<DiskIIController> (6);
        MemoryBus bus;
        bus.AddDevice (disk.get ());

        bus.ReadByte (0xC0EB);  // Drive 1
        bus.ReadByte (0xC0EA);  // Drive 0

        Byte val = bus.ReadByte (0xC0EC);
        Assert::AreEqual (static_cast<Byte> (0), val);
    }

    TEST_METHOD (WriteProtect_NoMountedDisk_Returns0)
    {
        auto disk = std::make_unique<DiskIIController> (6);
        MemoryBus bus;
        bus.AddDevice (disk.get ());

        bus.ReadByte (0xC0EE);  // Q7 = false
        Byte val = bus.ReadByte (0xC0ED);  // Q6 = true

        Assert::AreEqual (static_cast<Byte> (0x00), val);
    }

    TEST_METHOD (Reset_ClearsState)
    {
        auto disk = std::make_unique<DiskIIController> (6);
        MemoryBus bus;
        bus.AddDevice (disk.get ());

        bus.ReadByte (0xC0E9);
        bus.ReadByte (0xC0E3);

        disk->Reset ();

        Assert::AreEqual (static_cast<Word> (0xC0E0), disk->GetStart ());
        Assert::AreEqual (static_cast<Word> (0xC0EF), disk->GetEnd ());

        Byte val = disk->Read (0xC0EC);
        Assert::AreEqual (static_cast<Byte> (0), val);
    }

    TEST_METHOD (GetDisk_ValidDrive_ReturnsNonNull)
    {
        auto disk = std::make_unique<DiskIIController> (6);

        DiskImage * d0 = disk->GetDisk (0);
        DiskImage * d1 = disk->GetDisk (1);

        Assert::IsNotNull (d0);
        Assert::IsNotNull (d1);
        Assert::IsFalse (d0->IsLoaded ());
        Assert::IsFalse (d1->IsLoaded ());
    }

    TEST_METHOD (GetDisk_InvalidDrive_ReturnsNull)
    {
        auto disk = std::make_unique<DiskIIController> (6);

        DiskImage * d2 = disk->GetDisk (2);
        DiskImage * dm = disk->GetDisk (-1);

        Assert::IsNull (d2);
        Assert::IsNull (dm);
    }
};
