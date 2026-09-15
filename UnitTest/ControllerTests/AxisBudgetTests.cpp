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
//  How many paddle axes a machine has bounds what an assignment can drive:
//  four on the ][, ][+ and //e, two on the //c (FR-034). The machine's own
//  count is asserted in MachineModelTests; these drive the controller service
//  across a change of count.
//
//  AN ASSIGNMENT THE MACHINE CANNOT PLAY IS IGNORED, NOT DISCARDED. Switching
//  to a //c and back to a //e must find a four-axis assignment exactly as it
//  was (FR-035).
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


        TEST_METHOD (Pdl2Assignment_IgnoredOnATwoAxisMachineAndRestoredOnFour)
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
            pushed.buttons.set (1);

            mixer.SetSink (&sink);
            mixer.SetAxisOwner (AxisOwner::Controller);
            backend.AddDevice (xbox, true);
            backend.AddDevice (stick);
            backend.SetSample (stick.unit, pushed);
            SkipCalibration (service, stick.unit);

            service.SetSelection (xbox.unit);
            service.AssignAxes (stick.unit, ControllerAxisAssignment::AxisSet (0x4));
            service.SetAxisCount (2);
            service.Tick();

            Assert::AreEqual (kCenter, mixer.GetTargetState().paddle[2], L"PDL2 is not driven on a machine with two axes");
            Assert::IsFalse  (mixer.GetTargetState().buttons.test (1),
                L"a controller whose only axes the machine lacks does not drive the game port at all");
            Assert::AreEqual (size_t (1), service.GetSnapshot().assignments.size(), L"the assignment is kept, not discarded");
            Assert::AreEqual (0x4ul, service.GetSnapshot().assignments[0].axes.to_ulong());

            service.SetAxisCount (4);
            service.Tick();

            Assert::AreEqual (static_cast<Byte> (255), mixer.GetTargetState().paddle[2], L"a machine with four axes plays it again");
            Assert::IsTrue   (mixer.GetTargetState().buttons.test (1));
        }


        TEST_METHOD (FourAxisAssignment_PlaysItsFirstTwoOnATwoAxisMachine)
        {
            FakeControllerBackend   backend;
            GamePortInputMixer      mixer;
            RecordingGamePortSink   sink;
            ControllerInputService  service (backend, mixer);
            ControllerDeviceInfo    xbox   = MakeXboxDevice();
            ControllerSample        pushed;

            pushed.connected = true;
            pushed.axes[XInputSampleDecoder::kLeftStickX] = 1.0f;

            mixer.SetSink (&sink);
            mixer.SetAxisOwner (AxisOwner::Controller);
            backend.AddDevice (xbox, true);
            backend.SetSample (xbox.unit, pushed);

            service.SetSelection (xbox.unit);
            service.AssignAxes (xbox.unit, ControllerAxisAssignment::AxisSet (0xF));
            service.SetAxisCount (2);
            service.Tick();

            Assert::AreEqual (static_cast<Byte> (255), mixer.GetTargetState().paddle[0], L"PDL0 is driven on the //c");
            Assert::AreEqual (kCenter, mixer.GetTargetState().paddle[1]);
            Assert::AreEqual (0xFul, service.GetSnapshot().assignments[0].axes.to_ulong(), L"and the four-axis assignment survives the switch");
        }


        TEST_METHOD (GetAxesFor_LeavesOutAxesPastTheMachinesCount)
        {
            std::vector<ControllerAxisAssignment>  assignments;
            ControllerUnitKey                      stick = MakeStickDevice().unit;

            ControllerSelectionPolicy::AssignAxes (assignments, stick, ControllerAxisAssignment::AxisSet (0x6));

            Assert::AreEqual (0x2ul, ControllerSelectionPolicy::GetAxesFor (assignments, stick, std::nullopt, 2).to_ulong(),
                L"PDL2 is left out on a machine with two axes");
            Assert::AreEqual (0x6ul, ControllerSelectionPolicy::GetAxesFor (assignments, stick, std::nullopt, 4).to_ulong());
            Assert::AreEqual (0x6ul, assignments[0].axes.to_ulong(), L"asking does not change the assignment");
        }
    };
}
