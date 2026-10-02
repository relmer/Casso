#include "Pch.h"

#include "Controllers/InputModeRules.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  InputModeRulesTests
//
//  Player 1 plays on one of three sources, so these assert that choosing any
//  one gives up the other two, that a controller connecting takes nothing from
//  the keys or the mouse, and that the arrow keys never take the axes on their
//  own.
//
////////////////////////////////////////////////////////////////////////////////

namespace ControllerTests
{
    TEST_CLASS (InputModeRulesTests)
    {
    public:

        TEST_METHOD (NothingPicked_TheControllersOwnEveryAxis)
        {
            InputModeRules::State       state;
            InputModeRules::AxisOwners  owners;

            state.hasController        = true;
            state.isControllerAttached = true;
            owners                     = InputModeRules::GetAxisOwners (state);

            for (AxisOwner owner : owners)
            {
                Assert::AreEqual ((int) AxisOwner::Controller, (int) owner,
                    L"the controllers hold every axis; one no controller drives rests at center");
            }
        }


        //  A controller connecting does not take the keys from Player 1: they
        //  keep PDL0 and PDL1, and a controller playing as Player 2 plays the
        //  axes of its own slot beside them.
        TEST_METHOD (KeysPickedForPlayerOne_KeepTheirAxesWhileAControllerIsAttached)
        {
            InputModeRules::State       state;
            InputModeRules::AxisOwners  owners;

            state.arrowsJoystick       = true;
            state.hasController        = true;
            state.isControllerAttached = true;
            owners                     = InputModeRules::GetAxisOwners (state);

            Assert::AreEqual ((int) AxisOwner::ArrowKeys,  (int) owners[0], L"the keys keep PDL0 with a controller attached (FR-032)");
            Assert::AreEqual ((int) AxisOwner::ArrowKeys,  (int) owners[1], L"and PDL1");
            Assert::AreEqual ((int) AxisOwner::Controller, (int) owners[2], L"Player 2's controller owns the rest");
            Assert::AreEqual ((int) AxisOwner::Controller, (int) owners[3]);
        }


        TEST_METHOD (MousePickedForPlayerOne_KeepsItsAxesWhileAControllerIsAttached)
        {
            InputModeRules::State       state;
            InputModeRules::AxisOwners  owners;

            state.mousePaddle          = true;
            state.hasController        = true;
            state.isControllerAttached = true;
            owners                     = InputModeRules::GetAxisOwners (state);

            Assert::AreEqual ((int) AxisOwner::MousePaddle, (int) owners[0], L"the mouse keeps PDL0 with a controller attached");
            Assert::AreEqual ((int) AxisOwner::MousePaddle, (int) owners[1]);
            Assert::AreEqual ((int) AxisOwner::Controller,  (int) owners[2]);
        }


        //  While Player 2 plays the mouse keeps PDL0 alone, so it never covers
        //  the paddle Player 2's mode gives it.
        TEST_METHOD (MouseBesidePlayerTwo_KeepsPdl0Alone)
        {
            InputModeRules::State       state;
            InputModeRules::AxisOwners  owners;



            state.mousePaddle     = true;
            state.isSecondPlaying = true;
            owners                = InputModeRules::GetAxisOwners (state);

            Assert::AreEqual ((int) AxisOwner::MousePaddle, (int) owners[0], L"the mouse on PDL0");
            Assert::AreEqual ((int) AxisOwner::Controller,  (int) owners[1], L"PDL1 left to Player 2");
        }


        //  In Two paddles mode the mouse's X and Y are Player 1's two paddles,
        //  PDL0 and PDL1, while Player 2 plays too: Player 2's paddles are
        //  PDL2 and up beside them.
        TEST_METHOD (MouseAsTwoPaddles_KeepsPdl0AndPdl1BesidePlayerTwo)
        {
            InputModeRules::State       state;
            InputModeRules::AxisOwners  owners;



            state.mousePaddle       = true;
            state.isSecondPlaying   = true;
            state.isMouseTwoPaddles = true;
            owners                  = InputModeRules::GetAxisOwners (state);

            Assert::AreEqual ((int) AxisOwner::MousePaddle, (int) owners[0], L"the mouse's X on PDL0");
            Assert::AreEqual ((int) AxisOwner::MousePaddle, (int) owners[1], L"and its Y on PDL1");
            Assert::AreEqual ((int) AxisOwner::Controller,  (int) owners[2], L"PDL2 left to Player 2");
        }


        //  The keys in a Joyport jack close its switches and take no axis, so
        //  a controller beside them drives the paddle inputs.
        TEST_METHOD (KeysInAJack_TakeNoAxis)
        {
            InputModeRules::State       state;
            InputModeRules::AxisOwners  owners;



            state.arrowsJoystick  = true;
            state.isKeysOnJoyport = true;
            owners                = InputModeRules::GetAxisOwners (state);

            for (AxisOwner owner : owners)
            {
                Assert::AreEqual ((int) AxisOwner::Controller, (int) owner);
            }
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


        TEST_METHOD (Shorten_DropsTheVendorParentheticalBeforeCutting)
        {
            // The strip has no room for the ids that tell two units apart, and
            // cutting into them mid-word reads as a bug rather than a name.
            Assert::AreEqual (std::wstring (L"Xbox Controller"),
                              InputModeRules::Shorten (L"Xbox Controller (045e:02ff)"));

            Assert::AreEqual (std::wstring (L"VKBsim Gladiator"),
                              InputModeRules::Shorten (L"VKBsim Gladiator"));
        }


        //  How much of a long description fits is measured where the label is
        //  drawn, so Shorten cuts nothing by a count of characters.
        TEST_METHOD (Shorten_LeavesALongDescriptionWhole)
        {
            Assert::AreEqual (std::wstring (L"An Extremely Verbose Controller Name"),
                              InputModeRules::Shorten (L"An Extremely Verbose Controller Name (231d:0121)"));
        }


        TEST_METHOD (Shorten_LeavesAParenthesizedNameThatIsAllThereIs)
        {
            // Nothing before the parenthesis means the parenthesis is the
            // name, so removing it would leave an empty label.
            Assert::AreEqual (std::wstring (L"(not connected)"),
                              InputModeRules::Shorten (L"(not connected)"));
        }


        TEST_METHOD (NothingChosen_TheArrowsTakeNothing)
        {
            Assert::AreEqual ((int) AxisOwner::Controller, (int) InputModeRules::GetAxisOwners (InputModeRules::State())[0],
                L"the arrow keys are only ever on because the user turned them on");
        }


        TEST_METHOD (PaddleOutranksTheArrows_WhenBothSomehowOn)
        {
            InputModeRules::State  state;

            state.arrowsJoystick = true;
            state.mousePaddle    = true;

            Assert::AreEqual ((int) AxisOwner::MousePaddle, (int) InputModeRules::GetAxisOwners (state)[0],
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


        TEST_METHOD (FireKeys_DetachedTheAltKeysFireAsBefore)
        {
            //                                               X      Z      LAlt   RAlt   Joyport
            Assert::IsTrue (InputModeRules::GetFireKeyButtons (true,  false, false, false, false).test (0), L"X is PB0");
            Assert::IsTrue (InputModeRules::GetFireKeyButtons (false, false, true,  false, false).test (0), L"left Alt is PB0");
            Assert::IsTrue (InputModeRules::GetFireKeyButtons (false, true,  false, false, false).test (1), L"Z is PB1");
            Assert::IsTrue (InputModeRules::GetFireKeyButtons (false, false, false, true,  false).test (1), L"right Alt is PB1");
        }


        TEST_METHOD (FireKeys_AttachedTheAltKeysAreLeftOut)
        {
            //  The Alt keys are the //e's Open Apple and Closed Apple, which
            //  must change nothing while the Joyport is attached.
            Assert::IsTrue  (InputModeRules::GetFireKeyButtons (false, false, true,  true,  true).none(), L"Alt keys fire nothing");
            Assert::IsTrue  (InputModeRules::GetFireKeyButtons (true,  false, false, false, true).test (0), L"X still fires");
            Assert::IsTrue  (InputModeRules::GetFireKeyButtons (false, true,  false, false, true).test (1), L"Z still drives PB1");
        }
    };
}
