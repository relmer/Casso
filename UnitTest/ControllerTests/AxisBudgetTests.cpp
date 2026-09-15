#include "Pch.h"

#include "Controllers/ControllerInputService.h"

#include "Controllers/ControllerTokens.h"
#include "Controllers/XInputSampleDecoder.h"
#include "FakeControllerBackend.h"
#include "RecordingGamePortSink.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  AxisBudgetTests
//
//  How many paddle axes a machine has bounds what a player slot can drive:
//  four on the ][, ][+ and //e, two on the //c (FR-034). The machine's own
//  count is asserted in MachineModelTests; these drive the controller service
//  across a change of count.
//
//  A SLOT THE MACHINE CANNOT PLAY IS IGNORED, NOT DISCARDED. Switching to a
//  //c and back to a //e must find a four-axis setup exactly as it was
//  (FR-035).
//
////////////////////////////////////////////////////////////////////////////////

namespace ControllerTests
{
    TEST_CLASS (AxisBudgetTests)
    {
    public:

        static constexpr Byte  kCenter = 127;


        static ControllerDeviceInfo MakeXboxDevice()
        {
            ControllerDeviceInfo  info;

            info.unit.model.kind = ControllerKind::XInput;
            info.description     = L"Xbox Controller";
            info.xinputSlot      = 0;
            info.controls        = XInputSampleDecoder::ListControls();
            return info;
        }


        static ControllerDeviceInfo MakeStickDevice()
        {
            ControllerDeviceInfo  info;

            info.unit.model  = { ControllerKind::DirectInput, 0x231d, 0x0121 };
            info.unit.unitId = "{01661270-ADF7-11F1-8005-444553540000}";
            info.unit.source = ControllerUnitSource::InstanceGuid;
            info.description = L"VKBsim Gladiator";
            info.controls    = { { ControlKind::Axis, 0 }, { ControlKind::Axis, 1 },
                                 { ControlKind::Button, 0 }, { ControlKind::Button, 1 } };
            return info;
        }


        // A full-range user calibration, so the stick's first reading is taken
        // as it arrives rather than as its rest position.
        static void SkipCalibration (ControllerInputService & service, const ControllerUnitKey & unit)
        {
            std::map<std::string, ControllerCalibration>  calibrations;
            ControllerCalibration                         fullRange;

            fullRange.mode = CalibrationMode::User;
            fullRange.axes.fill ({ 0.0f, -1.0f, 1.0f });
            calibrations[ControllerTokens::UnitToToken (unit)] = fullRange;
            service.SetCalibrations (calibrations);
        }


        // Two players: the Xbox controller on joystick 0 and the stick on
        // joystick 1, which a //c does not have.
        static MultiplayerSetup MakeTwoPlayers (const ControllerUnitKey & first, const ControllerUnitKey & second)
        {
            MultiplayerSetup  setup;

            setup.isEnabled          = true;
            setup.players[0].unit    = first;
            setup.players[0].target  = PlayerAxisTarget::Joystick0;
            setup.players[1].unit    = second;
            setup.players[1].target  = PlayerAxisTarget::Joystick1;
            return setup;
        }


        TEST_METHOD (Joystick1Player_PlaysNothingOnATwoAxisMachineAndAgainOnFour)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            RecordingGamePortSink   sink;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    xbox   = MakeXboxDevice();
            ControllerDeviceInfo    stick  = MakeStickDevice();
            ControllerSample        pushed;

            pushed.connected = true;
            pushed.axes[0]   = 1.0f;
            pushed.buttons.set (0);

            mixer.SetSink (&sink);
            mixer.SetAxisOwner (AxisOwner::Controller);
            backend.AddDevice (xbox, true);
            backend.AddDevice (stick);
            backend.SetSample (stick.unit, pushed);
            SkipCalibration (service, stick.unit);

            service.SetMultiplayer (MakeTwoPlayers (xbox.unit, stick.unit));
            service.SetAxisCount (2);
            service.Tick();

            Assert::AreEqual (kCenter, mixer.GetTargetState().paddle[2], L"PDL2 is not driven on a machine with two axes");
            Assert::IsFalse  (mixer.GetTargetState().buttons.test (1),
                L"a player whose paddles the machine lacks does not drive the game port at all, buttons included");
            Assert::IsTrue   (service.GetMultiplayer().players[1].unit.has_value(), L"the slot is kept, not discarded");

            service.SetAxisCount (4);
            service.Tick();

            Assert::AreEqual (static_cast<Byte> (255), mixer.GetTargetState().paddle[2], L"a machine with four axes plays it again");
            Assert::IsTrue   (mixer.GetTargetState().buttons.test (1), L"and player two's button reaches PB1");
        }


        TEST_METHOD (SinglePaddlePlayer_PastTheCountPlaysNothingAndIsKept)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            RecordingGamePortSink   sink;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    xbox   = MakeXboxDevice();
            ControllerDeviceInfo    stick  = MakeStickDevice();
            MultiplayerSetup        setup  = MakeTwoPlayers (xbox.unit, stick.unit);
            ControllerSample        pushed;

            pushed.connected = true;
            pushed.axes[0]   = 1.0f;

            setup.players[1].target = PlayerAxisTarget::Paddle3;

            mixer.SetSink (&sink);
            mixer.SetAxisOwner (AxisOwner::Controller);
            backend.AddDevice (xbox, true);
            backend.AddDevice (stick);
            backend.SetSample (stick.unit, pushed);
            SkipCalibration (service, stick.unit);

            service.SetMultiplayer (setup);
            service.SetAxisCount (2);
            service.Tick();

            Assert::AreEqual (kCenter, mixer.GetTargetState().paddle[1], L"a //c has no PDL3, and the player does not fall back onto PDL1");
            Assert::AreEqual ((int) PlayerAxisTarget::Paddle3, (int) service.GetMultiplayer().players[1].target,
                L"and the slot survives the switch");

            service.SetAxisCount (4);
            service.Tick();

            Assert::AreEqual (static_cast<Byte> (255), mixer.GetTargetState().paddle[3], L"a //e plays it on PDL3");
        }


        TEST_METHOD (GetTargetAxes_LeavesOutPaddlesPastTheMachinesCount)
        {
            Assert::AreEqual (0x3ul, ControllerSelectionPolicy::GetTargetAxes (PlayerAxisTarget::Joystick0, 4).to_ulong(),
                L"joystick 0 is PDL0 and PDL1");
            Assert::AreEqual (0xCul, ControllerSelectionPolicy::GetTargetAxes (PlayerAxisTarget::Joystick1, 4).to_ulong(),
                L"and joystick 1 is PDL2 and PDL3");
            Assert::AreEqual (0x4ul, ControllerSelectionPolicy::GetTargetAxes (PlayerAxisTarget::Paddle2, 4).to_ulong());

            Assert::AreEqual (0x0ul, ControllerSelectionPolicy::GetTargetAxes (PlayerAxisTarget::Joystick1, 2).to_ulong(),
                L"a machine with two axes has neither of joystick 1's paddles");
            Assert::AreEqual (0x3ul, ControllerSelectionPolicy::GetTargetAxes (PlayerAxisTarget::Joystick0, 2).to_ulong());
            Assert::AreEqual (0x0ul, ControllerSelectionPolicy::GetTargetAxes (PlayerAxisTarget::Paddle2, 2).to_ulong());
        }
    };
}
