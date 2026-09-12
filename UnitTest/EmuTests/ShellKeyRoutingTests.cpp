#include "Pch.h"
#include "Shell/Input/ShellKeyRouting.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ShellKeyRoutingTests
//
//  The verdict a keydown gets, and the invariant the whole class of bug
//  violates: anything the shell claimed must swallow its character.
//
//  The sweep matters more than the individual cases. A keystroke reaching the
//  //e when it should not is invisible until someone presses that key over
//  that chrome and watches the screen, which is how Esc-in-paddle-mode shipped.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ShellKeyRoutingTests)
{
public:

    //  A letter, which no arm claims by key, and the two keys that do turn on
    //  key identity somewhere in the shell.
    static constexpr WPARAM  kLetterVk = 'A';

    static ShellKeyRouting::State  MakeIdle()
    {
        ShellKeyRouting::State  state;


        state.pointerMode         = InputMappingMode::Off;
        state.toolbarOwnsKeyboard = false;
        state.isChromeFocused     = false;
        state.isMenuOpen          = false;

        return state;
    }


    TEST_METHOD (IdleChrome_EveryKeyIsTheGuests)
    {
        ShellKeyRouting::State  state = MakeIdle();


        Assert::IsTrue (ShellKeyOwner::Guest == ShellKeyRouting::GetKeyOwner (state, kLetterVk));
        Assert::IsTrue (ShellKeyOwner::Guest == ShellKeyRouting::GetKeyOwner (state, VK_ESCAPE));
        Assert::IsTrue (ShellKeyOwner::Guest == ShellKeyRouting::GetKeyOwner (state, VK_LEFT));
        Assert::IsTrue (ShellKeyOwner::Guest == ShellKeyRouting::GetKeyOwner (state, VK_RETURN));
    }


    TEST_METHOD (PaddleClaimsEscapeOnly_LettersAndArrowsStillType)
    {
        ShellKeyRouting::State  state = MakeIdle();


        state.pointerMode = InputMappingMode::Paddle;

        Assert::IsTrue (ShellKeyOwner::PaddleExit == ShellKeyRouting::GetKeyOwner (state, VK_ESCAPE));

        // The pointer is captured, not the keyboard: typing still reaches the
        // guest, and so do the arrows.
        Assert::IsTrue (ShellKeyOwner::Guest == ShellKeyRouting::GetKeyOwner (state, kLetterVk));
        Assert::IsTrue (ShellKeyOwner::Guest == ShellKeyRouting::GetKeyOwner (state, VK_LEFT));
    }


    TEST_METHOD (OtherPointerModesDoNotClaimEscape)
    {
        ShellKeyRouting::State  state = MakeIdle();


        state.pointerMode = InputMappingMode::Joystick;
        Assert::IsTrue (ShellKeyOwner::Guest == ShellKeyRouting::GetKeyOwner (state, VK_ESCAPE));

        state.pointerMode = InputMappingMode::Mouse;
        Assert::IsTrue (ShellKeyOwner::Guest == ShellKeyRouting::GetKeyOwner (state, VK_ESCAPE));
    }


    TEST_METHOD (AnOpenPickerClaimsEveryKey)
    {
        ShellKeyRouting::State  state = MakeIdle();


        state.toolbarOwnsKeyboard = true;

        Assert::IsTrue (ShellKeyOwner::Toolbar == ShellKeyRouting::GetKeyOwner (state, kLetterVk));
        Assert::IsTrue (ShellKeyOwner::Toolbar == ShellKeyRouting::GetKeyOwner (state, VK_ESCAPE));
        Assert::IsTrue (ShellKeyOwner::Toolbar == ShellKeyRouting::GetKeyOwner (state, VK_RETURN));
    }


    TEST_METHOD (TheFocusRingAndAnOpenMenuEachClaimEveryKey)
    {
        ShellKeyRouting::State  ring = MakeIdle();
        ShellKeyRouting::State  menu = MakeIdle();


        ring.isChromeFocused = true;
        menu.isMenuOpen      = true;

        Assert::IsTrue (ShellKeyOwner::Chrome == ShellKeyRouting::GetKeyOwner (ring, kLetterVk));
        Assert::IsTrue (ShellKeyOwner::Chrome == ShellKeyRouting::GetKeyOwner (menu, kLetterVk));
    }


    //  Precedence. These pin the ORDER of the pre-checks, which is otherwise
    //  recorded nowhere and is easy to reorder without noticing.
    TEST_METHOD (PaddleEscapeBeatsThePickerAndTheRing)
    {
        ShellKeyRouting::State  state = MakeIdle();


        state.pointerMode         = InputMappingMode::Paddle;
        state.toolbarOwnsKeyboard = true;
        state.isChromeFocused     = true;
        state.isMenuOpen          = true;

        Assert::IsTrue (ShellKeyOwner::PaddleExit == ShellKeyRouting::GetKeyOwner (state, VK_ESCAPE));

        // Anything else in that same state is the toolbar's, not the ring's.
        Assert::IsTrue (ShellKeyOwner::Toolbar == ShellKeyRouting::GetKeyOwner (state, kLetterVk));
    }


    TEST_METHOD (ThePickerBeatsTheRing)
    {
        ShellKeyRouting::State  state = MakeIdle();


        state.toolbarOwnsKeyboard = true;
        state.isChromeFocused     = true;

        Assert::IsTrue (ShellKeyOwner::Toolbar == ShellKeyRouting::GetKeyOwner (state, kLetterVk));
    }


    //  The invariant. Swept over every state combination rather than spot
    //  checked, because the failure is a single combination nobody tried.
    TEST_METHOD (EveryClaimedKeySwallowsItsCharacter)
    {
        constexpr InputMappingMode  kModes[] = { InputMappingMode::Off,
                                                 InputMappingMode::Joystick,
                                                 InputMappingMode::Paddle,
                                                 InputMappingMode::Mouse };
        constexpr WPARAM            kVks[]   = { 'A', VK_ESCAPE, VK_TAB, VK_RETURN, VK_LEFT, VK_F10 };
        int                         claimed  = 0;
        int                         visited  = 0;


        for (InputMappingMode mode : kModes)
        {
            for (int bits = 0; bits < 8; bits++)
            {
                for (WPARAM vk : kVks)
                {
                    ShellKeyRouting::State  state;
                    ShellKeyOwner           owner = ShellKeyOwner::Guest;


                    state.pointerMode         = mode;
                    state.toolbarOwnsKeyboard = (bits & 1) != 0;
                    state.isChromeFocused     = (bits & 2) != 0;
                    state.isMenuOpen          = (bits & 4) != 0;

                    owner = ShellKeyRouting::GetKeyOwner (state, vk);
                    visited++;

                    if (owner != ShellKeyOwner::Guest)
                    {
                        claimed++;
                        Assert::IsTrue (ShellKeyRouting::DoesOwnerSwallowChar (owner));
                    }
                    else
                    {
                        Assert::IsFalse (ShellKeyRouting::DoesOwnerSwallowChar (owner));
                    }
                }
            }
        }

        // A sweep over an empty set passes while checking nothing, and so does
        // one where no combination ever claimed.
        Assert::AreEqual (192, visited);
        Assert::IsTrue (claimed > 0);
    }
};
