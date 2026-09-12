#include "Pch.h"

#include "Controllers/ControllerSelectionPolicy.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ControllerSelectionPolicyTests
//
//  Every row of the policy table, driven with made-up devices.
//
//  THE RULE THAT MATTERS MOST IS THAT AN ABSENT CONTROLLER KEEPS ITS
//  SELECTION. Discarding it would mean a controller unplugged to move a desk
//  comes back unselected, and the user has to choose it again every time.
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


        TEST_METHOD (SelectedAbsent_SelectionIsKept)
        {
            ControllerDeviceInfo               gone     = MakeStick ("{GONE}");
            std::vector<ControllerDeviceInfo>  devices  = { MakeXbox() };
            auto                               decision = ControllerSelectionPolicy::Evaluate (gone.unit, devices, true);

            Assert::IsFalse (decision.hasChanged,                    L"an unplugged controller has not been un-chosen");
            Assert::IsTrue  (decision.selection.value() == gone.unit, L"so its selection is kept, not handed to the attached one");
        }


        TEST_METHOD (SelectedAbsent_SoleSameModelIsAdopted)
        {
            ControllerDeviceInfo               moved    = MakeStick ("{OLD-PORT}");
            ControllerDeviceInfo               same     = MakeStick ("{NEW-PORT}");
            std::vector<ControllerDeviceInfo>  devices  = { same };
            auto                               decision = ControllerSelectionPolicy::Evaluate (moved.unit, devices, true);

            Assert::IsTrue   (decision.hasChanged,                    L"the same stick on another port is still that stick");
            Assert::IsTrue   (decision.selection.value() == same.unit, L"so its new identity is adopted");
            Assert::AreEqual ((int) SelectionChangeReason::Adoption, (int) decision.reason);
        }


        TEST_METHOD (SelectedAbsent_TwoOfTheSameModelAdoptsNothing)
        {
            ControllerDeviceInfo               moved    = MakeStick ("{OLD-PORT}");
            std::vector<ControllerDeviceInfo>  devices  = { MakeStick ("{ONE}"), MakeStick ("{TWO}") };
            auto                               decision = ControllerSelectionPolicy::Evaluate (moved.unit, devices, true);

            Assert::IsFalse (decision.hasChanged,
                L"with two of the model attached, which one the user meant is a coin flip, so nothing is adopted");
        }


        TEST_METHOD (SelectedAbsent_ADifferentModelIsNotAdopted)
        {
            ControllerDeviceInfo               moved    = MakeStick ("{OLD}", 0x0121);
            std::vector<ControllerDeviceInfo>  devices  = { MakeStick ("{OTHER}", 0x0999) };
            auto                               decision = ControllerSelectionPolicy::Evaluate (moved.unit, devices, true);

            Assert::IsFalse (decision.hasChanged, L"another model is another controller, whatever port it is on");
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
