#include "Pch.h"

#include "Core/StateReader.h"
#include "Core/StateWriter.h"
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
        DiskImage                    * disk     = controller.GetDiskII().GetDisk (0);
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
        Assert::AreEqual (1, controller->GetDiskII().GetActiveDrive());

        controller->Read (kSelectInt);
        Assert::AreEqual (0, controller->GetDiskII().GetActiveDrive());
    }


    TEST_METHOD (MotorPower_AndPhases_ReachTheDiskIILogic)
    {
        auto  controller = Make();



        controller->Read (kMotorOn);
        controller->Read (kPhase0On);

        Assert::IsTrue   (controller->GetDiskII().IsMotorOn());
        Assert::AreEqual ((uint8_t) 0x01, controller->GetDiskII().GetPhases());
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



        controller->GetDiskII().GetDisk (0)->ResizeTrack (0, 8192);
        controller->Read (kMotorOn);
        controller->Read (kQ7Off);
        controller->SyncToMasterClocks (Apple3DiskController::kMasterPerCycle * Disk2Controller::kMotorSpinupCycles);
        controller->Read (kQ6Off);

        size_t  before = controller->GetDiskII().GetEngine (0).GetBitPosition();



        controller->SyncToMasterClocks (Apple3DiskController::kMasterPerCycle * Disk2Controller::kMotorSpinupCycles + kMasterPerBit * 10 + 13);
        controller->Read (kQ6Off);

        Assert::AreEqual ((size_t) 10, controller->GetDiskII().GetEngine (0).GetBitPosition() - before);
    }


    TEST_METHOD (WriteProtectSense_ReadsBit7)
    {
        auto  controller = Make();



        controller->GetDiskII().GetDisk (0)->SetWriteProtected (true);

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
};
