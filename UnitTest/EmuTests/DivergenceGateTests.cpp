#include "Pch.h"

#include "Debugger/Reverse/DivergenceGate.h"
#include "resource.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DivergenceGateTests
//
//  What happens to the user's actions while the machine replays history:
//  a change is asked about first, only behind live, and a key press's
//  character waits for the answer, then for the machine to be live.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DivergenceGateTests)
{
public:

    static DxuiKeyEvent  MakeKey (DxuiKeyEventKind kind, WPARAM vk, bool isRepeat = false)
    {
        DxuiKeyEvent  ev;

        ev.kind   = kind;
        ev.vk     = vk;
        ev.repeat = isRepeat;
        return ev;
    }


    TEST_METHOD (ResetsPowerCyclesDiskAndMachineChangesChangeTheMachine)
    {
        for (WORD id : { IDM_MACHINE_RESET, IDM_MACHINE_POWERCYCLE, IDM_DISK_INSERT1, IDM_DISK_INSERT2, IDM_DISK_EJECT1, IDM_DISK_EJECT2,
                         IDM_DISK_WRITEPROTECT1, IDM_DISK_WRITEPROTECT2, IDM_DISK_WP1, IDM_DISK_WP2, IDM_DISK_RESOLVE_CHANGE,
                         IDM_FILE_OPEN, IDM_FILE_LOAD_STATE, IDM_DEBUG_RESTART })
        {
            Assert::IsTrue (DivergenceGate::IsStateChangingCommand (id), std::format (L"command {}", id).c_str());
        }
    }


    //  A paste types into the emulated machine as keys do, so behind live it
    //  asks before discarding history, the same as any other input.
    TEST_METHOD (APasteChangesTheMachine)
    {
        Assert::IsTrue (DivergenceGate::IsStateChangingCommand (IDM_EDIT_PASTE), L"paste");
        Assert::IsTrue (DivergenceGate::Judge (true, DivergenceGate::IsStateChangingCommand (IDM_EDIT_PASTE)) == DivergenceVerdict::Ask,
                        L"behind live, a paste asks");
    }


    TEST_METHOD (DebuggerAudioAndHistoryCommandsLeaveTheMachineAlone)
    {
        for (WORD id : { IDM_DEBUG_COMMAND, IDM_DEBUG_REVERSE, IDM_DEBUG_VIEW, IDM_DEBUG_PAUSE, IDM_AUDIO_DRIVE_ENABLE, IDM_DEBUG_DIVERGE })
        {
            Assert::IsFalse (DivergenceGate::IsStateChangingCommand (id), std::format (L"command {}", id).c_str());
        }
    }


    TEST_METHOD (AChangeIsAskedAboutOnlyBehindLive)
    {
        Assert::IsTrue (DivergenceGate::Judge (true,  true)  == DivergenceVerdict::Ask);
        Assert::IsTrue (DivergenceGate::Judge (false, true)  == DivergenceVerdict::Proceed);
        Assert::IsTrue (DivergenceGate::Judge (true,  false) == DivergenceVerdict::Proceed);
        Assert::IsTrue (DivergenceGate::Judge (false, false) == DivergenceVerdict::Proceed);
    }


    TEST_METHOD (LiveKeysGoAhead)
    {
        DivergenceGate  gate;

        Assert::IsTrue (gate.JudgeKey (false, MakeKey (DxuiKeyEventKind::Down, 'A')) == DivergenceVerdict::Proceed);
        Assert::IsTrue (gate.JudgeKey (false, MakeKey (DxuiKeyEventKind::Char, 'a')) == DivergenceVerdict::Proceed);
        Assert::IsTrue (gate.JudgeKey (false, MakeKey (DxuiKeyEventKind::Up,   'A')) == DivergenceVerdict::Proceed);
    }


    //  A press asks; its character arrives while the question is open and
    //  waits. A yes keeps both, in order, until the machine is live.
    TEST_METHOD (AYesHandsThePressAndItsCharacterBackOnceLive)
    {
        DivergenceGate             gate;
        DxuiKeyEvent               down = MakeKey (DxuiKeyEventKind::Down, 'A');
        DxuiKeyEvent               ch   = MakeKey (DxuiKeyEventKind::Char, 'a');
        std::vector<DxuiKeyEvent>  held;
        std::vector<HeldInput>     heldInputs;



        Assert::IsTrue (gate.JudgeKey (true, down) == DivergenceVerdict::Ask);
        gate.Hold (down);

        Assert::IsTrue (gate.IsAsking());
        Assert::IsTrue (gate.JudgeKey (true, ch) == DivergenceVerdict::Hold, L"the character waits for the answer");
        gate.Hold (ch);

        gate.Answer (true);

        Assert::IsTrue (gate.IsAwaitingLive());
        Assert::IsTrue (gate.JudgeKey (true, MakeKey (DxuiKeyEventKind::Up, 'A')) == DivergenceVerdict::Hold, L"and so does anything before live");

        gate.TakeHeld (held, heldInputs);

        Assert::AreEqual<size_t> (2, held.size(), L"the press and its character");
        Assert::IsTrue (held[0].kind == DxuiKeyEventKind::Down);
        Assert::IsTrue (held[1].kind == DxuiKeyEventKind::Char);
        Assert::IsFalse (gate.IsAwaitingLive(), L"done");
        Assert::IsTrue (gate.JudgeKey (false, ch) == DivergenceVerdict::Proceed);
    }


    //  A no drops the press, and the character that follows it.
    TEST_METHOD (ANoDropsThePressAndItsCharacter)
    {
        DivergenceGate             gate;
        DxuiKeyEvent               down = MakeKey (DxuiKeyEventKind::Down, 'A');
        std::vector<DxuiKeyEvent>  held;
        std::vector<HeldInput>     heldInputs;



        Assert::IsTrue (gate.JudgeKey (true, down) == DivergenceVerdict::Ask);
        gate.Hold (down);

        gate.Answer (false);

        Assert::IsFalse (gate.IsAsking());
        Assert::IsTrue (gate.JudgeKey (true, MakeKey (DxuiKeyEventKind::Char, 'a')) == DivergenceVerdict::Drop, L"the character is dropped");

        gate.TakeHeld (held, heldInputs);

        Assert::IsTrue (held.empty(), L"nothing comes back");
    }


    TEST_METHOD (RepeatsAndReleasesBehindLiveAreDroppedWithoutAsking)
    {
        DivergenceGate  gate;

        Assert::IsTrue (gate.JudgeKey (true, MakeKey (DxuiKeyEventKind::Down, 'A', true)) == DivergenceVerdict::Drop);
        Assert::IsTrue (gate.JudgeKey (true, MakeKey (DxuiKeyEventKind::Up,   'A'))       == DivergenceVerdict::Drop);
        Assert::IsFalse (gate.IsAsking());
    }


    //  The owner's report: rewound, played on, and a joystick moved behind
    //  live asked nothing. A deflection, a press and a closed Joyport switch
    //  each change what the machine reads, so each asks.
    TEST_METHOD (ADeflectionAPressOrAClosedJoyportSwitchBehindLiveAsks)
    {
        GamePortState  rest;
        GamePortState  deflected = MakeStick (kFarEnd, GamePortState::kPaddleCenter);
        GamePortState  pressed;
        GamePortState  closed;



        pressed.buttons.set (0);
        closed.jacks.jack[JoyportJacks::kRightJack].set (static_cast<size_t> (JoystickSwitch::Up));

        for (const GamePortState & wanted : { deflected, pressed, closed })
        {
            DivergenceGate  gate;

            Assert::IsTrue (gate.JudgeGamePort (true, rest, wanted, rest) == DivergenceVerdict::Ask, L"behind live, it asks");
            Assert::IsTrue (gate.IsAsking());
        }
    }


    //  A stick left centered, or wandering within the tolerance, centered or
    //  held over to one side, changes nothing the machine reads.
    TEST_METHOD (AStickHeldSteadyOrJitteringDoesNotAsk)
    {
        constexpr Byte  kHeld     = 200;
        DivergenceGate  gate;
        GamePortState   rest;
        GamePortState   jitter    = MakeStick (static_cast<Byte> (GamePortState::kPaddleCenter + DivergenceGate::kAxisTolerance),
                                               static_cast<Byte> (GamePortState::kPaddleCenter - DivergenceGate::kAxisTolerance));
        GamePortState   held      = MakeStick (kHeld, GamePortState::kPaddleCenter);
        GamePortState   heldMoved = MakeStick (static_cast<Byte> (kHeld + DivergenceGate::kAxisTolerance), GamePortState::kPaddleCenter);



        Assert::IsTrue (gate.JudgeGamePort (true, rest, rest,      rest) != DivergenceVerdict::Ask, L"centered");
        Assert::IsTrue (gate.JudgeGamePort (true, rest, jitter,    rest) != DivergenceVerdict::Ask, L"jitter about the center");
        Assert::IsTrue (gate.JudgeGamePort (true, held, heldMoved, rest) != DivergenceVerdict::Ask, L"jitter about a held deflection");
        Assert::IsFalse (gate.IsAsking());
    }


    //  The recording already has the machine reading this: nothing changes.
    TEST_METHOD (AnInputTheRecordingAlreadyHasDoesNotAsk)
    {
        DivergenceGate  gate;
        GamePortState   rest;
        GamePortState   recorded = MakeStick (kFarEnd, GamePortState::kPaddleCenter);



        recorded.buttons.set (0);

        Assert::IsTrue (gate.JudgeGamePort (true, rest, recorded, recorded) != DivergenceVerdict::Ask);
        Assert::IsFalse (gate.IsAsking());
    }


    //  Letting go is never a question, as a key's release is not.
    TEST_METHOD (LettingGoDoesNotAsk)
    {
        DivergenceGate  gate;
        GamePortState   rest;
        GamePortState   held     = MakeStick (kFarEnd, GamePortState::kPaddleCenter);
        GamePortState   recorded = MakeStick (0, GamePortState::kPaddleCenter);



        held.buttons.set (1);

        Assert::IsTrue (gate.JudgeGamePort (true, held, rest, recorded) != DivergenceVerdict::Ask);
        Assert::IsFalse (gate.IsAsking());
    }


    TEST_METHOD (LiveGamePortInputGoesAhead)
    {
        DivergenceGate  gate;
        GamePortState   rest;



        Assert::IsTrue (gate.JudgeGamePort (false, rest, MakeStick (kFarEnd, 0), rest) == DivergenceVerdict::Proceed);
    }


    //  While a question is open the game port waits on its answer.
    TEST_METHOD (WhileAQuestionIsOpenTheGamePortWaits)
    {
        DivergenceGate  gate;
        GamePortState   rest;



        Assert::IsTrue (gate.JudgeKey (true, MakeKey (DxuiKeyEventKind::Down, 'A')) == DivergenceVerdict::Ask);
        Assert::IsTrue (gate.JudgeGamePort (true, rest, MakeStick (kFarEnd, 0), rest) == DivergenceVerdict::Hold);
    }


    //  A no takes the stick where it then was as where it was before: held
    //  there it asks no more, moved on it asks again, and going live forgets
    //  where it was refused.
    TEST_METHOD (AfterANoOnlyAFurtherMoveAsksAgain)
    {
        constexpr Byte  kNearlyFar = static_cast<Byte> (kFarEnd - DivergenceGate::kAxisTolerance);
        DivergenceGate  gate;
        GamePortState   rest;
        GamePortState   right      = MakeStick (kFarEnd, GamePortState::kPaddleCenter);



        Assert::IsTrue (gate.JudgeGamePort (true, rest, right, rest) == DivergenceVerdict::Ask);
        gate.Answer (false);

        Assert::IsTrue (gate.JudgeGamePort (true, rest, right, rest) != DivergenceVerdict::Ask, L"held where it was refused");
        Assert::IsTrue (gate.JudgeGamePort (true, rest, MakeStick (kNearlyFar, GamePortState::kPaddleCenter), rest) != DivergenceVerdict::Ask, L"or close to it");
        Assert::IsTrue (gate.JudgeGamePort (true, rest, MakeStick (kFarEnd, 0), rest) == DivergenceVerdict::Ask, L"pushed on, it asks again");

        gate.Answer (false);
        gate.OnLive();

        Assert::IsTrue (gate.JudgeGamePort (true, rest, right, rest) == DivergenceVerdict::Ask, L"once live, the refusal is forgotten");
    }


    //  The guest mouse's button and the //c's 80/40 switch ask behind live; a
    //  yes hands them back once live, in order, and a no drops them.
    TEST_METHOD (AMousePressOrTheEightyColumnSwitchAsksBehindLive)
    {
        DivergenceGate             gate;
        std::vector<DxuiKeyEvent>  held;
        std::vector<HeldInput>     heldInputs;



        Assert::IsTrue (gate.JudgeInput (false) == DivergenceVerdict::Proceed, L"live, it goes ahead");

        Assert::IsTrue (gate.JudgeInput (true) == DivergenceVerdict::Ask);
        gate.HoldInput (HeldInput::MousePress);

        Assert::IsTrue (gate.JudgeInput (true) == DivergenceVerdict::Hold, L"the next waits on the answer");
        gate.HoldInput (HeldInput::EightyColumnToggle);

        gate.Answer (true);
        gate.TakeHeld (held, heldInputs);

        Assert::AreEqual<size_t> (2, heldInputs.size(), L"both come back");
        Assert::IsTrue (heldInputs[0] == HeldInput::MousePress);
        Assert::IsTrue (heldInputs[1] == HeldInput::EightyColumnToggle);

        Assert::IsTrue (gate.JudgeInput (true) == DivergenceVerdict::Ask);
        gate.HoldInput (HeldInput::MousePress);
        gate.Answer (false);
        gate.TakeHeld (held, heldInputs);

        Assert::IsTrue (heldInputs.empty(), L"a no drops it");
    }


private:

    static constexpr Byte  kFarEnd = 255;


    static GamePortState MakeStick (Byte x, Byte y)
    {
        GamePortState  state;

        state.paddle[0] = x;
        state.paddle[1] = y;
        return state;
    }
};
