#include "Pch.h"

#include "Controllers/ControllerSelectionPolicy.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ControllerSelectionPolicyTests
//
//  Every row of the policy table, driven with made-up devices.
//
//  THE RULE THAT MATTERS MOST IS THAT THE SELECTION STAYS ON THE CONTROLLER
//  IN USE. One arriving never takes it; only the selected one leaving moves
//  it, and then to a controller that is here or to nothing.
//
////////////////////////////////////////////////////////////////////////////////

namespace ControllerTests
{
    TEST_CLASS (ControllerSelectionPolicyTests)
    {
    public:

        static ControllerDeviceInfo MakeXbox()
        {
            ControllerDeviceInfo  info;

            info.unit.model.kind = ControllerKind::XInput;
            info.description     = L"Xbox Controller";
            info.xinputSlot      = 0;
            return info;
        }


        static ControllerDeviceInfo MakeStick (const std::string & unitId, unsigned short product = 0x0121)
        {
            ControllerDeviceInfo  info;

            info.unit.model  = { ControllerKind::DirectInput, 0x231d, product };
            info.unit.unitId = unitId;
            info.unit.source = ControllerUnitSource::InstanceGuid;
            info.description = L"VKBsim Gladiator";
            return info;
        }


        TEST_METHOD (NoneSelected_FirstAttachedIsTaken)
        {
            std::vector<ControllerDeviceInfo>          devices  = { MakeStick ("{A}"), MakeXbox() };
            ControllerSelectionPolicy::Decision        decision = ControllerSelectionPolicy::Evaluate (std::nullopt, devices, true);

            Assert::IsTrue  (decision.hasChanged,                                    L"a controller must be chosen when none is");
            Assert::IsTrue  (decision.selection.value() == devices.front().unit,     L"the first one enumerated is the one chosen");
            Assert::IsTrue  (decision.clearsOtherInputModes,                         L"and it takes the axes from the arrows and the paddle");
            Assert::AreEqual ((int) SelectionChangeReason::AutomaticSelection, (int) decision.reason);
        }


        TEST_METHOD (NoneSelected_NothingAttachedChangesNothing)
        {
            std::vector<ControllerDeviceInfo>    devices;
            ControllerSelectionPolicy::Decision  decision = ControllerSelectionPolicy::Evaluate (std::nullopt, devices, true);

            Assert::IsFalse (decision.hasChanged,            L"there is nothing to choose");
            Assert::IsFalse (decision.selection.has_value(), L"so the selection stays empty");
        }


        TEST_METHOD (AlreadySelected_ConnectingAnotherChangesNothing)
        {
            ControllerDeviceInfo               stick    = MakeStick ("{A}");
            std::vector<ControllerDeviceInfo>  devices  = { stick, MakeXbox() };
            auto                               decision = ControllerSelectionPolicy::Evaluate (stick.unit, devices, true);

            Assert::IsFalse (decision.hasChanged,                     L"a controller arriving must not steal the selection");
            Assert::IsTrue  (decision.selection.value() == stick.unit, L"the chosen one keeps it");
        }


        TEST_METHOD (SelectedAbsent_LongestAttachedTakesOver)
        {
            ControllerDeviceInfo               gone     = MakeStick ("{GONE}");
            std::vector<ControllerDeviceInfo>  devices  = { MakeXbox(), MakeStick ("{LATER}", 0x0999) };
            auto                               decision = ControllerSelectionPolicy::Evaluate (gone.unit, devices, true);

            Assert::IsTrue   (decision.hasChanged,                                L"the selection follows the controllers that are here");
            Assert::IsTrue   (decision.selection.value() == devices.front().unit, L"and the one attached longest, listed first, takes it");
            Assert::AreEqual ((int) SelectionChangeReason::Replacement, (int) decision.reason);
            Assert::IsFalse  (decision.clearsOtherInputModes,                     L"a controller already had the axes, so nothing else was on");
        }


        TEST_METHOD (SelectedAbsent_NothingAttachedClearsTheSelection)
        {
            ControllerDeviceInfo               gone     = MakeStick ("{GONE}");
            std::vector<ControllerDeviceInfo>  devices;
            auto                               decision = ControllerSelectionPolicy::Evaluate (gone.unit, devices, true);

            Assert::IsTrue   (decision.hasChanged);
            Assert::IsFalse  (decision.selection.has_value(), L"nothing drives the axes, and the arrow keys are not turned on for the user");
            Assert::AreEqual ((int) SelectionChangeReason::Cleared, (int) decision.reason);
            Assert::IsFalse  (decision.clearsOtherInputModes);
        }


        TEST_METHOD (SelectedAbsent_SoleSameModelIsAdopted)
        {
            ControllerDeviceInfo               moved    = MakeStick ("{OLD-PORT}");
            ControllerDeviceInfo               same     = MakeStick ("{NEW-PORT}");
            std::vector<ControllerDeviceInfo>  devices  = { MakeXbox(), same };
            auto                               decision = ControllerSelectionPolicy::Evaluate (moved.unit, devices, true);

            Assert::IsTrue   (decision.hasChanged,                    L"the same stick on another port is still that stick");
            Assert::IsTrue   (decision.selection.value() == same.unit, L"so its new identity is adopted, ahead of a controller attached longer");
            Assert::AreEqual ((int) SelectionChangeReason::Adoption, (int) decision.reason);
        }


        TEST_METHOD (SelectedAbsent_TwoOfTheSameModelIsNotAnAdoption)
        {
            ControllerDeviceInfo               moved    = MakeStick ("{OLD-PORT}");
            std::vector<ControllerDeviceInfo>  devices  = { MakeStick ("{ONE}"), MakeStick ("{TWO}") };
            auto                               decision = ControllerSelectionPolicy::Evaluate (moved.unit, devices, true);

            // Which of the two moved is a coin flip, so they count as any
            // other controller would, and the one attached longest takes over.
            Assert::AreEqual ((int) SelectionChangeReason::Replacement, (int) decision.reason);
            Assert::IsTrue   (decision.selection.value() == devices.front().unit);
        }


        TEST_METHOD (SelectedAbsent_ADifferentModelIsNotAdopted)
        {
            ControllerDeviceInfo               moved    = MakeStick ("{OLD}", 0x0121);
            std::vector<ControllerDeviceInfo>  devices  = { MakeStick ("{OTHER}", 0x0999) };
            auto                               decision = ControllerSelectionPolicy::Evaluate (moved.unit, devices, true);

            Assert::AreEqual ((int) SelectionChangeReason::Replacement, (int) decision.reason,
                L"another model is another controller, whatever port it is on");
        }


        TEST_METHOD (XboxSelection_MatchesWhicheverUnitIsAttached)
        {
            ControllerDeviceInfo               xbox     = MakeXbox();
            std::vector<ControllerDeviceInfo>  devices  = { xbox };
            auto                               decision = ControllerSelectionPolicy::Evaluate (xbox.unit, devices, true);

            Assert::IsFalse (decision.hasChanged, L"Xbox-class controllers are recognized by model, so any unit is the selected one");
            Assert::IsTrue  (ControllerSelectionPolicy::IsSelectedAttached (xbox.unit, devices));
        }


        TEST_METHOD (NoGamePort_PolicyIsInertButKeepsTheSelection)
        {
            ControllerDeviceInfo               stick    = MakeStick ("{A}");
            std::vector<ControllerDeviceInfo>  devices  = { stick };
            auto                               decision = ControllerSelectionPolicy::Evaluate (std::nullopt, devices, false);

            Assert::IsFalse (decision.hasChanged,            L"a machine with no game port must not select anything");
            Assert::IsFalse (decision.selection.has_value(), L"and nothing is invented for it");

            decision = ControllerSelectionPolicy::Evaluate (stick.unit, devices, false);
            Assert::IsTrue (decision.selection.value() == stick.unit,
                L"a selection made on another machine survives being carried to one with no port");
        }


        TEST_METHOD (IsSelectedAttached_FalseWithoutASelection)
        {
            std::vector<ControllerDeviceInfo>  devices = { MakeXbox() };

            Assert::IsFalse (ControllerSelectionPolicy::IsSelectedAttached (std::nullopt, devices));
        }
    };
}
