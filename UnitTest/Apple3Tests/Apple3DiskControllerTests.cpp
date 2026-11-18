#include "Pch.h"

#include "Core/StateReader.h"
#include "Core/StateWriter.h"
#include "Machines/Apple2/Common/Disk2CardSequencer.h"
#include "Machines/Apple3/Apple3DiskController.h"

// The controller has the Disk II logic's DiskImage instances; each test
// allocates it on the heap to stay inside the C6262 stack-frame budget.
#pragma warning (disable: 6262)

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Apple3DiskControllerTests
//
//  The built-in drive: the motor select latches and the external drive
//  lines, I/O select, motor power, the phases, reading nibbles at the
//  drive's speed in master clocks, and the write protect sense.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Apple3DiskControllerTests)
{
public:

    static constexpr Word      kPhase0On      = 0xC0E1;
    static constexpr Word      kMotorOn       = 0xC0E9;
    static constexpr Word      kSelectInt     = 0xC0EA;
    static constexpr Word      kSelectExt     = 0xC0EB;
    static constexpr Word      kQ6Off         = 0xC0EC;
    static constexpr Word      kQ6On          = 0xC0ED;
    static constexpr Word      kQ7Off         = 0xC0EE;
    static constexpr uint64_t  kMasterPerBit  = Apple3DiskController::kMasterPerCycle * Disk2NibbleEngine::kCyclesPerBit;


    static std::unique_ptr<Apple3DiskController> Make()
    {
        return std::make_unique<Apple3DiskController>();
    }


    //  A track of D5 AA 96 repeated, written to drive 1.
    static void WriteTrack (Apple3DiskController & controller)
    {
        constexpr std::array<Byte, 3>  kNibbles = { 0xD5, 0xAA, 0x96 };
        constexpr size_t               kRepeats = 200;
        DiskImage                    * disk     = controller.GetDiskII().GetDrive (0).GetDisk();
        size_t                         bit      = 0;



        disk->ResizeTrack (0, kRepeats * kNibbles.size() * 8);

        for (size_t i = 0; i < kRepeats; i++)
        {
            for (Byte nibble : kNibbles)
            {
                for (int b = 7; b >= 0; b--)
                {
                    disk->WriteBit (0, bit++, (uint8_t) ((nibble >> b) & 1));
                }
            }
        }
    }


    TEST_METHOD (InternalMotorSelect_IsSetAndClearedByC0D4AndC0D5)
    {
        auto  controller = Make();



        controller->Read (0xC0D4);
        Assert::IsTrue (controller->IsInternalSelected());

        controller->Write (0xC0D5, 0);
        Assert::IsFalse (controller->IsInternalSelected());
    }


    TEST_METHOD (ExternalLines_SelectD2D3AndD4)
    {
        auto  controller = Make();



        Assert::AreEqual (0, controller->GetSelectedExternal());

        controller->Read (0xC0D2);
        controller->Read (0xC0D1);
        Assert::AreEqual (2, controller->GetSelectedExternal());

        controller->Read (0xC0D3);
        controller->Read (0xC0D0);
        Assert::AreEqual (3, controller->GetSelectedExternal());

        controller->Read (0xC0D1);
        Assert::AreEqual (4, controller->GetSelectedExternal());

        controller->Read (0xC0D2);
        controller->Read (0xC0D0);
        Assert::AreEqual (0, controller->GetSelectedExternal());
    }


    TEST_METHOD (IoSelect_ConnectsTheInternalOrTheExternalDrive)
    {
        auto  controller = Make();



        controller->Read (kSelectExt);
        Assert::AreEqual (1, controller->GetDiskII().GetLines().GetActiveDrive());

        controller->Read (kSelectInt);
        Assert::AreEqual (0, controller->GetDiskII().GetLines().GetActiveDrive());
    }


    TEST_METHOD (MotorPower_AndPhases_ReachTheDiskIILogic)
    {
        auto  controller = Make();



        controller->Read (kMotorOn);
        controller->Read (kPhase0On);

        Assert::IsTrue   (controller->GetDiskII().GetLines().IsMotorOn());
        Assert::AreEqual ((uint8_t) 0x01, controller->GetDiskII().GetLines().GetPhases());
    }


    TEST_METHOD (Read_ReturnsTheTracksNibbles)
    {
        auto       controller = Make();
        uint64_t   now        = 0;
        int        found      = 0;



        WriteTrack (*controller);

        controller->Read (kSelectInt);
        controller->Read (kMotorOn);
        controller->Read (kQ7Off);

        for (int i = 0; i < 20000 && found < 3; i++)
        {
            now += kMasterPerBit / 2;
            controller->SyncToMasterClocks (now);

            Byte  value = controller->Read (kQ6Off);



            if ((found == 0 && value == 0xD5) || (found == 1 && value == 0xAA) || (found == 2 && value == 0x96))
            {
                found++;
            }
        }

        Assert::AreEqual (3, found, L"D5 AA 96 read in order");
    }


    TEST_METHOD (DiskCycles_AreOnePerFourteenMasterClocks)
    {
        auto  controller = Make();



        controller->GetDiskII().GetDrive (0).GetDisk()->ResizeTrack (0, 8192);
        controller->Read (kMotorOn);
        controller->Read (kQ7Off);
        controller->SyncToMasterClocks (Apple3DiskController::kMasterPerCycle * Disk2CardSequencer::kMotorSpinupCycles);
        controller->Read (kQ6Off);

        size_t  before = controller->GetDiskII().GetDrive (0).GetEngine().GetBitPosition();



        controller->SyncToMasterClocks (Apple3DiskController::kMasterPerCycle * Disk2CardSequencer::kMotorSpinupCycles + kMasterPerBit * 10 + 13);
        controller->Read (kQ6Off);

        Assert::AreEqual ((size_t) 10, controller->GetDiskII().GetDrive (0).GetEngine().GetBitPosition() - before);
    }


    TEST_METHOD (WriteProtectSense_ReadsBit7)
    {
        auto  controller = Make();



        controller->GetDiskII().GetDrive (0).GetDisk()->SetWriteProtected (true);

        controller->Read (kQ7Off);
        Assert::AreEqual ((Byte) 0x80, (Byte) (controller->Read (kQ6On) & 0x80));
    }


    TEST_METHOD (State_RoundTripsTheLatches)
    {
        auto         controller = Make();
        auto         loaded     = Make();
        StateWriter  writer;
        HRESULT      hr         = S_OK;



        controller->Read (0xC0D4);
        controller->Read (0xC0D3);

        hr = controller->SaveState (writer);
        Assert::AreEqual (S_OK, hr);

        StateReader  reader (writer.GetBytes());

        hr = loaded->LoadState (reader);
        Assert::AreEqual (S_OK, hr);
        Assert::IsTrue   (reader.IsAtEnd());
        Assert::IsTrue   (loaded->IsInternalSelected());
        Assert::AreEqual (3, loaded->GetSelectedExternal());
    }

    //  Three external drives, each with a separate disk.
    struct FourDrives
    {
        std::unique_ptr<Apple3DiskController>   controller = std::make_unique<Apple3DiskController>();
        std::array<DiskImage, 3>                disks;

        FourDrives()
        {
            controller->SetExternalDriveCount (3);

            for (int i = 0; i < 3; i++)
            {
                controller->SetDisk (i + 1, &disks[(size_t) i]);
            }
        }

        DiskImage * GetConnectedDisk() { return controller->GetDiskII().GetDrive (1).GetDisk(); }
    };


    static void Select (Apple3DiskController & controller, int drive)
    {
        controller.Read (drive == 3 || drive == 4 ? 0xC0D3 : 0xC0D2);
        controller.Read (drive == 2 || drive == 4 ? 0xC0D1 : 0xC0D0);
    }


    TEST_METHOD (FourDrives_EachExternalDriveIsConnectedWhenSelected)
    {
        auto  rig = std::make_unique<FourDrives>();



        for (int drive = 2; drive <= 4; drive++)
        {
            Select (*rig->controller, drive);

            Assert::AreEqual (drive - 2, rig->controller->GetConnectedExternal());
            Assert::IsTrue   (rig->GetConnectedDisk() == &rig->disks[(size_t) (drive - 2)]);
        }
    }


    TEST_METHOD (FourDrives_EachDriveKeepsItsHead)
    {
        auto  rig = std::make_unique<FourDrives>();



        Select (*rig->controller, 2);
        rig->controller->GetDiskII().GetDrive (1).SetQuarterTrack (20);

        Select (*rig->controller, 3);
        Assert::AreEqual (0, rig->controller->GetDiskII().GetDrive (1).GetQuarterTrack());
        rig->controller->GetDiskII().GetDrive (1).SetQuarterTrack (8);

        Select (*rig->controller, 2);
        Assert::AreEqual (20, rig->controller->GetDiskII().GetDrive (1).GetQuarterTrack());

        Select (*rig->controller, 3);
        Assert::AreEqual (8, rig->controller->GetDiskII().GetDrive (1).GetQuarterTrack());
    }


    TEST_METHOD (UnpluggedDrive_HasNoDisk)
    {
        auto  rig = std::make_unique<FourDrives>();



        rig->controller->SetExternalDriveCount (1);
        Select (*rig->controller, 3);

        Assert::IsFalse (rig->GetConnectedDisk() == &rig->disks[1]);
    }


    TEST_METHOD (WriteProtect_IsTheConnectedDrives)
    {
        auto  rig = std::make_unique<FourDrives>();



        rig->disks[1].SetWriteProtected (true);

        Select (*rig->controller, 2);
        rig->controller->Read (kSelectExt);
        rig->controller->Read (kQ7Off);
        Assert::AreEqual ((Byte) 0x00, (Byte) (rig->controller->Read (kQ6On) & 0x80));

        Select (*rig->controller, 3);
        Assert::AreEqual ((Byte) 0x80, (Byte) (rig->controller->Read (kQ6On) & 0x80));
    }


    TEST_METHOD (InternalMotorSelect_IsIndependentOfTheExternalLines)
    {
        auto  rig = std::make_unique<FourDrives>();



        rig->controller->Read (0xC0D4);
        Select (*rig->controller, 4);

        Assert::IsTrue (rig->controller->IsInternalSelected());
    }
};
