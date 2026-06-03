#include "Pch.h"
#include "InputDebugDialogState.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;




namespace
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MakeGuest
    //
    //  Builds a minimal guest-side display record carrying just the
    //  category and game-port classification MatchesFilter inspects.
    //
    ////////////////////////////////////////////////////////////////////////////////

    InputEventDisplay MakeGuest (InputEventType type, InputGamePortClass gamePort)
    {
        InputEventDisplay  e;

        e.category = InputEventCategory::Guest;
        e.type     = type;
        e.gamePort = gamePort;
        return e;
    }


    InputEventDisplay MakeCategory (InputEventCategory category, InputEventType type)
    {
        InputEventDisplay  e;

        e.category = category;
        e.type     = type;
        e.gamePort = InputGamePortClass::None;
        return e;
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  InputDebugFilterTests
//
//  Exercises MatchesFilter's emulator/host lane gating and the per-pair
//  Joystick-vs-Paddle interpretation that the view dropdowns drive.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (InputDebugFilterTests)
{
public:

    TEST_METHOD (HostKeyboard_GatedByHostCheckbox)
    {
        InputEventDisplay  e = MakeCategory (InputEventCategory::Host, InputEventType::HostKeyDown);
        InputFilterState   f;

        f.showHostKeyboard = true;
        Assert::IsTrue (MatchesFilter (e, f),
            L"Host keyboard events show while the host-keyboard lane is on");

        f.showHostKeyboard = false;
        Assert::IsFalse (MatchesFilter (e, f),
            L"Host keyboard events hide while the host-keyboard lane is off");
    }

    TEST_METHOD (SystemEvents_AlwaysShown)
    {
        InputEventDisplay  e = MakeCategory (InputEventCategory::System, InputEventType::EventsLost);
        InputFilterState   f;

        f.showEmuKeyboard  = false;
        f.showJoystick     = false;
        f.showPaddle       = false;
        f.showHostKeyboard = false;

        Assert::IsTrue (MatchesFilter (e, f),
            L"System (EventsLost) events are never filterable");
    }

    TEST_METHOD (GuestKeyboard_GatedByEmuKeyboardCheckbox)
    {
        InputEventDisplay  e = MakeGuest (InputEventType::KbdDataRead, InputGamePortClass::None);
        InputFilterState   f;

        f.showEmuKeyboard = true;
        Assert::IsTrue (MatchesFilter (e, f),
            L"Guest keyboard reads show while the emulator-keyboard lane is on");

        f.showEmuKeyboard = false;
        Assert::IsFalse (MatchesFilter (e, f),
            L"Guest keyboard reads hide while the emulator-keyboard lane is off");
    }

    TEST_METHOD (Pair0AsJoystick_GatedByJoystickCheckbox)
    {
        InputEventDisplay  e = MakeGuest (InputEventType::PaddleRead, InputGamePortClass::Pair0);
        InputFilterState   f;

        f.pairIsJoystick[0] = true;
        f.showJoystick      = true;
        f.showPaddle        = false;
        Assert::IsTrue (MatchesFilter (e, f),
            L"A pair viewed as Joystick honors the Joystick lane");

        f.showJoystick = false;
        Assert::IsFalse (MatchesFilter (e, f),
            L"A pair viewed as Joystick is hidden when the Joystick lane is off");
    }

    TEST_METHOD (Pair1AsPaddle_GatedByPaddleCheckbox)
    {
        InputEventDisplay  e = MakeGuest (InputEventType::PaddleRead, InputGamePortClass::Pair1);
        InputFilterState   f;

        f.pairIsJoystick[1] = false;
        f.showPaddle        = true;
        f.showJoystick      = false;
        Assert::IsTrue (MatchesFilter (e, f),
            L"A pair viewed as Paddles honors the Paddle lane");

        f.showPaddle = false;
        Assert::IsFalse (MatchesFilter (e, f),
            L"A pair viewed as Paddles is hidden when the Paddle lane is off");
    }

    TEST_METHOD (PairInterpretation_FlipsWithoutReprojecting)
    {
        InputEventDisplay  e = MakeGuest (InputEventType::ButtonRead, InputGamePortClass::Pair0);
        InputFilterState   f;

        f.showJoystick = true;
        f.showPaddle   = false;

        f.pairIsJoystick[0] = true;
        Assert::IsTrue (MatchesFilter (e, f),
            L"Viewed as Joystick with the Joystick lane on -> visible");

        f.pairIsJoystick[0] = false;
        Assert::IsFalse (MatchesFilter (e, f),
            L"Re-viewing the same logged event as Paddles re-gates it on the Paddle lane");
    }

    TEST_METHOD (GlobalTrigger_VisibleIfEitherAnalogLaneOn)
    {
        InputEventDisplay  e = MakeGuest (InputEventType::PaddleTrigger, InputGamePortClass::Global);
        InputFilterState   f;

        f.showJoystick = false;
        f.showPaddle   = false;
        Assert::IsFalse (MatchesFilter (e, f),
            L"The PTRIG strobe hides when both analog lanes are off");

        f.showJoystick = true;
        Assert::IsTrue (MatchesFilter (e, f),
            L"The PTRIG strobe shows when the Joystick lane is on");

        f.showJoystick = false;
        f.showPaddle   = true;
        Assert::IsTrue (MatchesFilter (e, f),
            L"The PTRIG strobe shows when the Paddle lane is on");
    }
};
