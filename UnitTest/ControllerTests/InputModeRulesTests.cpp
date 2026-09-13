#include "Pch.h"

#include "Controllers/InputModeRules.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  InputModeRulesTests
//
//  Three sources drive the paddle axes and only one can have them, so these
//  assert that choosing any one gives up the other two, and that the arrow
//  keys never take the axes on their own.
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


        TEST_METHOD (Controller_NotReadingRestsTheAxes)
        {
            InputModeRules::State  state;

            state.hasController        = true;
            state.isControllerAttached = false;

            // Until the selected controller reads, nothing drives the axes.
            // The arrow keys never take them on their own: arrows-to-joystick
            // takes X and Z from the guest's keyboard (FR-008a).
            Assert::AreEqual ((int) AxisOwner::None, (int) InputModeRules::GetAxisOwner (state),
                L"the arrow keys are only ever on because the user turned them on");
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


        TEST_METHOD (PaddleSources_ListRealControllersAheadOfTheKeysAndTheMouse)
        {
            InputModeRules::State              state;
            std::vector<ControllerDeviceInfo>  devices = { MakeStick ("{A}", L"Gladiator"),
                                                           MakeStick ("{B}", L"Gamepad") };
            std::vector<InputModeRules::PaddleSource>  sources;

            state.arrowsJoystick = true;
            sources              = InputModeRules::BuildPaddleSources (state, devices, std::nullopt);

            Assert::AreEqual (size_t (4), sources.size(), L"each attached controller plus the keys and the mouse");

            // A real stick plays these games better than either, so it is
            // what the list offers first.
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


        TEST_METHOD (PaddleSources_ASelectionThatIsNotAttachedHasNoRow)
        {
            InputModeRules::State                      state;
            std::vector<ControllerDeviceInfo>          devices;
            ControllerDeviceInfo                       gone    = MakeStick ("{GONE}", L"Gladiator");
            std::vector<InputModeRules::PaddleSource>  sources;

            state.hasController = true;
            sources             = InputModeRules::BuildPaddleSources (state, devices, gone.unit);

            Assert::AreEqual (size_t (2), sources.size(),
                L"a row for a controller that is not there would offer a pick that drives nothing");
            Assert::IsFalse (sources[0].isChecked, L"and the keys are not checked, because they are not driving");
            Assert::IsFalse (sources[1].isChecked);
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
                InputModeRules::GetStandInBannerText (keys),
                L"nothing on screen marks X and Z, so the bar is the only place they are stated");

            Assert::AreEqual (std::wstring (L"Using the mouse for paddle input. Press Esc to exit paddle mode."),
                InputModeRules::GetStandInBannerText (mouse));
        }


        TEST_METHOD (StandInBanner_FollowsTheModeAndNotThePointerCapture)
        {
            InputModeRules::State  mouse;

            mouse.mousePaddle = true;

            // Whether the pointer is held at this instant is how paddle mode
            // reads the mouse, not something the user asked about, and
            // Escape leaves the mode either way. The rule cannot see the
            // capture at all, which is the point.
            Assert::AreEqual (std::wstring (L"Using the mouse for paddle input. Press Esc to exit paddle mode."),
                InputModeRules::GetStandInBannerText (mouse));
        }


        TEST_METHOD (StandInBanner_AControllerNeedsNoBar)
        {
            InputModeRules::State  pad;

            pad.hasController        = true;
            pad.isControllerAttached = true;

            // Its buttons are labeled on the device, and a bar that never
            // goes away would cost picture for the whole session.
            Assert::IsTrue (InputModeRules::GetStandInBannerText (pad).empty());
        }
    };
}
