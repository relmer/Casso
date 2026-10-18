#include "Pch.h"
#include "Machines/Apple2/Common/Disk2Controller.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2PerDriveHeadTests
//
//  Each drive has a stepper of its own: a phase change moves the selected
//  drive's head, and a drive's head and its engine's track are where that
//  drive's own steps put them, whichever drive is selected.
//
////////////////////////////////////////////////////////////////////////////////

namespace Disk2PerDriveHeadTests
{
    static constexpr Word  kPhase0Off  = 0xC0E0;
    static constexpr Word  kPhase0On   = 0xC0E1;
    static constexpr Word  kPhase1Off  = 0xC0E2;
    static constexpr Word  kPhase1On   = 0xC0E3;
    static constexpr Word  kPhase2Off  = 0xC0E4;
    static constexpr Word  kPhase2On   = 0xC0E5;
    static constexpr Word  kPhase3On   = 0xC0E7;
    static constexpr Word  kSelect1    = 0xC0EA;
    static constexpr Word  kSelect2    = 0xC0EB;
    static constexpr Word  kMotorOff   = 0xC0E8;
    static constexpr Word  kMotorOn    = 0xC0E9;
    static constexpr int   kDrive1     = 0;
    static constexpr int   kDrive2     = 1;



    TEST_CLASS (Disk2PerDriveHeadTests)
    {
    public:

        //  Phase 0 on, then a DOS-style half-track step to phase 2, with every
        //  phase off at the end: the head ends on quarter track 4.
        static void SeekToQuarterTrackFour (Disk2Controller & ctrl)
        {
            ctrl.Write (kPhase0On,  0x00);
            ctrl.Write (kPhase1On,  0x00);
            ctrl.Write (kPhase0Off, 0x00);
            ctrl.Write (kPhase2On,  0x00);
            ctrl.Write (kPhase1Off, 0x00);
            ctrl.Write (kPhase2Off, 0x00);
        }

        TEST_METHOD (SteppingOneDriveLeavesTheOthersHead)
        {
            auto  ctrl = std::make_unique<Disk2Controller> (6);



            ctrl->Write (kMotorOn, 0x00);
            ctrl->Write (kSelect1, 0x00);
            SeekToQuarterTrackFour (*ctrl);
            Assert::AreEqual (4, ctrl->GetQuarterTrack (kDrive1));
            Assert::AreEqual (0, ctrl->GetQuarterTrack (kDrive2), L"drive 2's head has not moved");

            ctrl->Write (kSelect2, 0x00);
            Assert::AreEqual (0, ctrl->GetQuarterTrack(), L"drive 2's head is where it was after the select");

            ctrl->Write (kPhase1On,  0x00);
            ctrl->Write (kPhase1Off, 0x00);
            Assert::AreEqual (2, ctrl->GetQuarterTrack (kDrive2));
            Assert::AreEqual (4, ctrl->GetQuarterTrack (kDrive1), L"drive 1's head has not moved");

            ctrl->Write (kSelect1, 0x00);
            Assert::AreEqual (4, ctrl->GetQuarterTrack(), L"selecting drive 1 again finds its head where it was");
        }

        TEST_METHOD (EachEngineIsOnItsOwnDrivesTrack)
        {
            auto  ctrl = std::make_unique<Disk2Controller> (6);



            ctrl->Write (kMotorOn, 0x00);
            ctrl->Write (kSelect1, 0x00);
            SeekToQuarterTrackFour (*ctrl);
            ctrl->Write (kSelect2, 0x00);
            ctrl->Write (kSelect1, 0x00);
            ctrl->Write (kSelect2, 0x00);

            Assert::AreEqual (4, ctrl->GetEngine (kDrive1).GetCurrentTrack(), L"drive 1's engine");
            Assert::AreEqual (0, ctrl->GetEngine (kDrive2).GetCurrentTrack(), L"drive 2's engine, not drive 1's position");
        }

        TEST_METHOD (ABumpMovesNeitherHead)
        {
            auto  ctrl = std::make_unique<Disk2Controller> (6);



            ctrl->Write (kMotorOn, 0x00);
            ctrl->Write (kSelect2, 0x00);
            ctrl->Write (kPhase1On,  0x00);
            ctrl->Write (kPhase1Off, 0x00);

            ctrl->Write (kSelect1, 0x00);
            ctrl->Write (kPhase3On, 0x00);

            Assert::AreEqual (0, ctrl->GetQuarterTrack (kDrive1), L"drive 1's head at the outer stop");
            Assert::AreEqual (2, ctrl->GetQuarterTrack (kDrive2), L"drive 2's head where it was");
        }


        //  A drive's stepper works only while the drive is enabled, the motor
        //  running (GH #176): with it off, a phase change moves no head, and
        //  the phases on when the motor starts pull the head then.
        TEST_METHOD (TheHeadStepsOnlyWithTheMotorOn)
        {
            auto  ctrl = std::make_unique<Disk2Controller> (6);



            ctrl->Write (kPhase1On, 0x00);
            Assert::AreEqual (0, ctrl->GetQuarterTrack(), L"the motor off, the phase moves nothing");

            ctrl->Write (kMotorOn, 0x00);
            Assert::AreEqual (2, ctrl->GetQuarterTrack(), L"the motor on, the phase on pulls the head");

            ctrl->Write (kPhase1Off, 0x00);
            ctrl->Write (kPhase2On,  0x00);
            Assert::AreEqual (4, ctrl->GetQuarterTrack(), L"and the next phase steps it");
        }


        //  The drive stays enabled through the second of spin-down after
        //  $C0E8, so the head steps then, and not once the motor has stopped.
        TEST_METHOD (TheHeadStepsThroughTheSpinDownAndNotAfter)
        {
            auto  ctrl = std::make_unique<Disk2Controller> (6);



            ctrl->Write (kMotorOn,  0x00);
            ctrl->Write (kMotorOff, 0x00);
            ctrl->Write (kPhase1On, 0x00);
            Assert::AreEqual (2, ctrl->GetQuarterTrack(), L"in the spin-down the head steps");

            ctrl->Tick (Disk2Controller::kMotorSpindownCycles);
            Assert::IsFalse  (ctrl->IsMotorOn(), L"the motor has stopped");

            ctrl->Write (kPhase1Off, 0x00);
            ctrl->Write (kPhase2On,  0x00);
            Assert::AreEqual (2, ctrl->GetQuarterTrack(), L"with the motor stopped it does not");
        }


        //  Selecting the other drive with the motor running enables it at
        //  once, so the phase on pulls its head.
        TEST_METHOD (ADriveSelectedWithAPhaseOnStepsThatDrive)
        {
            auto  ctrl = std::make_unique<Disk2Controller> (6);



            ctrl->Write (kMotorOn,  0x00);
            ctrl->Write (kSelect1,  0x00);
            ctrl->Write (kPhase1On, 0x00);
            Assert::AreEqual (2, ctrl->GetQuarterTrack (kDrive1));

            ctrl->Write (kSelect2, 0x00);
            Assert::AreEqual (2, ctrl->GetQuarterTrack (kDrive2), L"drive 2's head pulled to the phase on");
            Assert::AreEqual (2, ctrl->GetQuarterTrack (kDrive1), L"drive 1's head where it was");
        }
    };
}
