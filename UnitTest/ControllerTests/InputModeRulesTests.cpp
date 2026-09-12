#include "Pch.h"

#include "Controllers/InputModeRules.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  InputModeRulesTests
//
//  Three sources drive the paddle axes and only one can have them, so these
//  assert that choosing any one gives up the other two, and that a chosen
//  controller that is not attached does not hold the axes hostage.
//
////////////////////////////////////////////////////////////////////////////////

namespace ControllerTests
{
    TEST_CLASS (InputModeRulesTests)
    {
    public:

        TEST_METHOD (Controller_OwnsTheAxesWhileAttached)
        {
            InputModeRules::State  state;

            state.hasController        = true;
            state.isControllerAttached = true;

            Assert::AreEqual ((int) AxisOwner::Controller, (int) InputModeRules::GetAxisOwner (state));
        }


        TEST_METHOD (Controller_DetachedLetsTheArrowsStandIn)
        {
            InputModeRules::State  state;

            state.hasController        = true;
            state.isControllerAttached = false;
            state.arrowsJoystick       = true;

            Assert::AreEqual ((int) AxisOwner::ArrowKeys, (int) InputModeRules::GetAxisOwner (state),
                L"an unplugged controller must not hold the axes while the arrows are on");
        }


        TEST_METHOD (Controller_DetachedWithNothingElseRestsAtCenter)
        {
            InputModeRules::State  state;

            state.hasController        = true;
            state.isControllerAttached = false;

            Assert::AreEqual ((int) AxisOwner::None, (int) InputModeRules::GetAxisOwner (state));
        }


        TEST_METHOD (SelectingAController_TurnsOffTheArrowsAndThePaddle)
        {
            InputModeRules::State  state;
            InputModeRules::State  after;

            state.arrowsJoystick = true;
            state.mousePaddle    = true;
            after                = InputModeRules::AfterSelectingController (state);

            Assert::IsTrue  (after.hasController,  L"the controller is chosen");
            Assert::IsFalse (after.arrowsJoystick, L"and the arrows give up the axes");
            Assert::IsFalse (after.mousePaddle,    L"as does the paddle");
        }


        TEST_METHOD (TurningOnTheArrows_ClearsTheControllerAndThePaddle)
        {
            InputModeRules::State  state;
            InputModeRules::State  after;

            state.hasController = true;
            state.mousePaddle   = true;
            after               = InputModeRules::AfterSettingArrows (state, true);

            Assert::IsTrue  (after.arrowsJoystick, L"the arrows take the axes");
            Assert::IsFalse (after.hasController,  L"the user asked for the keys, so the controller selection is dropped");
            Assert::IsFalse (after.mousePaddle,    L"and the paddle with it");
        }


        TEST_METHOD (TurningOffTheArrows_LeavesTheOthersAlone)
        {
            InputModeRules::State  state;
            InputModeRules::State  after;

            state.hasController  = true;
            state.arrowsJoystick = true;
            after                = InputModeRules::AfterSettingArrows (state, false);

            Assert::IsFalse (after.arrowsJoystick, L"the arrows stop driving");
            Assert::IsTrue  (after.hasController,  L"but turning something off must not un-choose anything else");
        }


        TEST_METHOD (TurningOnThePaddle_ClearsTheControllerAndTheArrows)
        {
            InputModeRules::State  state;
            InputModeRules::State  after;

            state.hasController  = true;
            state.arrowsJoystick = true;
            after                = InputModeRules::AfterSettingMousePaddle (state, true);

            Assert::IsTrue  (after.mousePaddle,    L"the paddle takes the axes");
            Assert::IsFalse (after.hasController,  L"and the controller gives them up");
            Assert::IsFalse (after.arrowsJoystick, L"as do the arrows");
        }


        static ControllerDeviceInfo MakeStick (const std::string & unitId, const std::wstring & description)
        {
            ControllerDeviceInfo  info;

            info.unit.model  = { ControllerKind::DirectInput, 0x231d, 0x0121 };
            info.unit.unitId = unitId;
            info.unit.source = ControllerUnitSource::InstanceGuid;
            info.description = description;
            return info;
        }


        TEST_METHOD (PaddleSources_ListTheBuiltInsThenWhatIsAttached)
        {
            InputModeRules::State              state;
            std::vector<ControllerDeviceInfo>  devices = { MakeStick ("{A}", L"Gladiator"),
                                                           MakeStick ("{B}", L"Gamepad") };
            std::vector<InputModeRules::PaddleSource>  sources;

            state.arrowsJoystick = true;
            sources              = InputModeRules::BuildPaddleSources (state, devices, std::nullopt);

            Assert::AreEqual (size_t (4), sources.size(),      L"the two built-in sources plus each attached controller");
            Assert::IsTrue   (sources[0].isArrowKeys,          L"the always-present sources come first");
            Assert::IsTrue   (sources[1].isMousePaddle);
            Assert::IsTrue   (sources[0].isChecked,            L"and the one in use is checked");
            Assert::AreEqual (std::wstring (L"Gladiator"), sources[2].label);
        }


        TEST_METHOD (PaddleSources_AChosenControllerChecksItselfAndNotTheKeys)
        {
            InputModeRules::State              state;
            std::vector<ControllerDeviceInfo>  devices = { MakeStick ("{A}", L"Gladiator") };
            std::vector<InputModeRules::PaddleSource>  sources;

            state.arrowsJoystick = true;   // stale: the setters clear this when a controller is chosen
            state.hasController  = true;
            sources              = InputModeRules::BuildPaddleSources (state, devices, devices[0].unit);

            Assert::IsFalse (sources[0].isChecked, L"one game port means one checked entry, never two");
            Assert::IsTrue  (sources[2].isChecked, L"and it is the chosen controller");
        }


        TEST_METHOD (PaddleSources_AChosenControllerThatIsGoneKeepsItsRow)
        {
            InputModeRules::State                      state;
            std::vector<ControllerDeviceInfo>          devices;
            ControllerDeviceInfo                       gone    = MakeStick ("{GONE}", L"Gladiator");
            std::vector<InputModeRules::PaddleSource>  sources;

            state.hasController = true;
            sources             = InputModeRules::BuildPaddleSources (state, devices, gone.unit);

            Assert::AreEqual (size_t (3), sources.size(),
                L"a chosen controller that is unplugged must still appear, or the picker would show the keys checked instead");
            Assert::IsTrue  (sources[2].isChecked);
            Assert::IsFalse (sources[2].isConnected);
        }


        TEST_METHOD (NothingChosen_AxesRestAtCenter)
        {
            Assert::AreEqual ((int) AxisOwner::None, (int) InputModeRules::GetAxisOwner (InputModeRules::State()));
        }


        TEST_METHOD (PaddleOutranksTheArrows_WhenBothSomehowOn)
        {
            InputModeRules::State  state;

            state.arrowsJoystick = true;
            state.mousePaddle    = true;

            Assert::AreEqual ((int) AxisOwner::MousePaddle, (int) InputModeRules::GetAxisOwner (state),
                L"the setters make this unreachable, but the owner must still be decided, never ambiguous");
        }
    };
}
