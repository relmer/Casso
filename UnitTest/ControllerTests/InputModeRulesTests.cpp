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


        TEST_METHOD (Controller_DetachedWithNothingElseFallsBackToTheArrows)
        {
            InputModeRules::State  state;

            state.hasController        = true;
            state.isControllerAttached = false;

            // The arrows are OFF here: choosing the controller is what turned
            // them off. Resting the axes at center would answer a disconnect
            // by taking the game away entirely (FR-008a).
            Assert::AreEqual ((int) AxisOwner::ArrowKeys, (int) InputModeRules::GetAxisOwner (state),
                L"the arrow keys stand in whether or not the user has them on");
        }


        TEST_METHOD (Controller_StandingInKeepsTheAxesWithTheControllers)
        {
            InputModeRules::State  state;

            state.hasController        = true;
            state.isControllerAttached = false;
            state.hasStandIn           = true;

            // Which controller is read is the service's question. Either way
            // a controller drives the axes, so the arrows do not take them.
            Assert::AreEqual ((int) AxisOwner::Controller, (int) InputModeRules::GetAxisOwner (state),
                L"a stand-in is still a controller");
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


        TEST_METHOD (PaddleSources_ListRealControllersAheadOfTheStandIns)
        {
            InputModeRules::State              state;
            std::vector<ControllerDeviceInfo>  devices = { MakeStick ("{A}", L"Gladiator"),
                                                           MakeStick ("{B}", L"Gamepad") };
            std::vector<InputModeRules::PaddleSource>  sources;

            state.arrowsJoystick = true;
            sources              = InputModeRules::BuildPaddleSources (state, devices, std::nullopt);

            Assert::AreEqual (size_t (4), sources.size(), L"each attached controller plus the two stand-ins");

            // A real stick plays these games better than either stand-in, so
            // it is what the list offers first.
            Assert::AreEqual (std::wstring (L"Gladiator"),            sources[0].label);
            Assert::AreEqual (std::wstring (L"Gamepad"),              sources[1].label);
            Assert::AreEqual (std::wstring (L"Use keys as joystick"), sources[2].label);
            Assert::AreEqual (std::wstring (L"Use mouse as paddle"),  sources[3].label);

            Assert::IsTrue (sources[2].isArrowKeys);
            Assert::IsTrue (sources[3].isMousePaddle);
            Assert::IsTrue (sources[2].isChecked, L"the one in use is checked wherever it sits");
        }


        TEST_METHOD (PaddleSources_AChosenControllerChecksItselfAndNotTheKeys)
        {
            InputModeRules::State              state;
            std::vector<ControllerDeviceInfo>  devices = { MakeStick ("{A}", L"Gladiator") };
            std::vector<InputModeRules::PaddleSource>  sources;

            state.arrowsJoystick = true;   // stale: the setters clear this when a controller is chosen
            state.hasController  = true;
            sources              = InputModeRules::BuildPaddleSources (state, devices, devices[0].unit);

            Assert::IsTrue  (sources[0].isChecked, L"the chosen controller is the checked entry");
            Assert::IsFalse (sources[1].isChecked, L"one game port means one checked entry, never two");
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
            Assert::IsTrue  (sources[0].isChecked,   L"and it stays with the controllers rather than sinking below the stand-ins");
            Assert::IsFalse (sources[0].isConnected);
            Assert::IsTrue  (sources[1].isArrowKeys);
        }


        TEST_METHOD (Shorten_DropsTheVendorParentheticalBeforeCutting)
        {
            // The strip has no room for the ids that tell two units apart, and
            // cutting into them mid-word reads as a bug rather than a name.
            Assert::AreEqual (std::wstring (L"Xbox Controller"),
                              InputModeRules::Shorten (L"Xbox Controller (045e:02ff)"));

            Assert::AreEqual (std::wstring (L"VKBsim Gladiator"),
                              InputModeRules::Shorten (L"VKBsim Gladiator"));
        }


        TEST_METHOD (Shorten_CutsWhatIsStillTooLong)
        {
            std::wstring  shortened = InputModeRules::Shorten (L"An Extremely Verbose Controller Name");

            Assert::AreEqual (InputModeRules::kShortLabelLimit, shortened.size());
            Assert::AreEqual (L'\x2026', shortened.back(), L"one ellipsis, not three dots");
        }


        TEST_METHOD (Shorten_LeavesAParenthesizedNameThatIsAllThereIs)
        {
            // Nothing before the parenthesis means the parenthesis is the
            // name, so removing it would leave an empty label.
            Assert::AreEqual (std::wstring (L"(not connected)"),
                              InputModeRules::Shorten (L"(not connected)"));
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

        TEST_METHOD (StandInBanner_SaysWhichHostControlsTookOver)
        {
            InputModeRules::State  keys;
            InputModeRules::State  mouse;

            keys.arrowsJoystick = true;
            mouse.mousePaddle   = true;

            Assert::AreEqual (std::wstring (L"Using the arrow keys as a joystick. X and Z are the buttons."),
                InputModeRules::GetStandInBannerText (keys, false),
                L"nothing on screen marks X and Z, so the bar is the only place they are stated");

            Assert::AreEqual (std::wstring (L"Using the mouse for paddle input. Press Esc to exit paddle mode."),
                InputModeRules::GetStandInBannerText (mouse, true));
        }


        TEST_METHOD (StandInBanner_MouseSaysNothingUntilThePointerIsCaptured)
        {
            InputModeRules::State  mouse;

            mouse.mousePaddle = true;

            // Paddle mode with the pointer free is armed, not driving, and the
            // way out is the whole reason that line exists.
            Assert::IsTrue (InputModeRules::GetStandInBannerText (mouse, false).empty());
        }


        TEST_METHOD (StandInBanner_AControllerNeedsNoBar)
        {
            InputModeRules::State  pad;

            pad.hasController        = true;
            pad.isControllerAttached = true;

            // Its buttons are labeled on the device, and a bar that never
            // goes away would cost picture for the whole session.
            Assert::IsTrue (InputModeRules::GetStandInBannerText (pad, false).empty());
        }
    };
}
