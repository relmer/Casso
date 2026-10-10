#include "Pch.h"

#include "Debugger/Reverse/DivergenceGate.h"
#include "resource.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DivergenceGateTests
//
//  What happens to the user's actions while the machine replays history: a
//  command is asked about first, only behind live; input the guest reads is
//  held without a question until the guest reads it or the machine is live,
//  and a key press's character waits with it.
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
    //  is held as keys are, rather than asked about as a command.
    TEST_METHOD (APasteIsHeldAsKeysAre)
    {
        constexpr Byte  kLatch = 0xC1;
        DivergenceGate  gate;



        Assert::IsFalse (DivergenceGate::IsStateChangingCommand (IDM_EDIT_PASTE), L"not a command to ask about");

        gate.HoldPaste (kLatch);

        Assert::IsTrue (gate.IsHolding(), L"held");
        Assert::IsTrue (gate.HasHeldPaste());
        Assert::IsTrue (gate.GetHeldLines().keyLatch == std::optional<Byte> (kLatch), L"its first character is the key waiting");
    }


    TEST_METHOD (DebuggerAudioAndHistoryCommandsLeaveTheMachineAlone)
    {
        for (WORD id : { IDM_DEBUG_COMMAND, IDM_DEBUG_REVERSE, IDM_DEBUG_VIEW, IDM_DEBUG_PAUSE, IDM_AUDIO_DRIVE_ENABLE, IDM_DEBUG_DIVERGE })
        {
            Assert::IsFalse (DivergenceGate::IsStateChangingCommand (id), std::format (L"command {}", id).c_str());
        }
    }


    //  Salvage reads the damaged disk and writes a new file beside it, so it
    //  passes behind live; inserting the copy it wrote is what changes the
    //  machine.
    TEST_METHOD (SalvageLeavesTheMachineAloneAndInsertingTheCopyChangesIt)
    {
        for (WORD id : { IDM_DISK_SALVAGE1, IDM_DISK_SALVAGE2, IDM_DISK_SALVAGE_WRITE })
        {
            Assert::IsFalse (DivergenceGate::IsStateChangingCommand (id), std::format (L"command {}", id).c_str());
        }

        for (WORD id : { IDM_DISK_INSERT1, IDM_DISK_INSERT2 })
        {
            Assert::IsTrue (DivergenceGate::IsStateChangingCommand (id), std::format (L"command {}", id).c_str());
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


    //  A press behind live asks nothing: it is held with its character, the
    //  key it leaves waiting goes on the watched lines, and so does
    //  any-key-down until the key comes up. A key let go of is still waiting.
    TEST_METHOD (AKeyPressBehindLiveIsHeldWithoutAsking)
    {
        DivergenceGate  gate;



        HoldKey (gate, MakeKey (DxuiKeyEventKind::Down, 'A'), std::nullopt);

        Assert::IsTrue  (gate.IsHolding(), L"held");
        Assert::IsFalse (gate.IsAsking(),  L"no question yet");
        Assert::IsTrue  (gate.GetHeldLines().isKeyDown, L"the key is down");

        HoldKey (gate, MakeKey (DxuiKeyEventKind::Char, 'a'), kLatchA);

        Assert::IsTrue (gate.GetHeldLines().keyLatch == std::optional<Byte> (kLatchA), L"its key is waiting");

        HoldKey (gate, MakeKey (DxuiKeyEventKind::Up, 'A'), std::nullopt);

        Assert::IsFalse (gate.GetHeldLines().isKeyDown, L"let go of");
        Assert::IsTrue  (gate.GetHeldLines().keyLatch == std::optional<Byte> (kLatchA), L"but its key still waits");
    }


    //  The guest reads a held line: that asks. The question takes the lines
    //  off the watch, and the key's release waits on the answer with it. A
    //  yes keeps all three, in order, until the machine is live.
    TEST_METHOD (AReadAsksAndAYesHandsTheKeysBackOnceLive)
    {
        DivergenceGate             gate;
        std::vector<DxuiKeyEvent>  held;
        std::vector<HeldInput>     heldInputs;



        HoldKey (gate, MakeKey (DxuiKeyEventKind::Down, 'A'), std::nullopt);
        HoldKey (gate, MakeKey (DxuiKeyEventKind::Char, 'a'), kLatchA);

        Assert::IsTrue (gate.OnHeldInputRead(), L"a read asks");
        Assert::IsTrue (gate.IsAsking());
        Assert::IsTrue (gate.GetHeldLines().IsEmpty(), L"no more reads are in question");

        HoldKey (gate, MakeKey (DxuiKeyEventKind::Up, 'A'), std::nullopt);

        gate.Answer (true);

        Assert::IsTrue (gate.IsAwaitingLive());
        Assert::IsTrue (gate.JudgeKey (true, MakeKey (DxuiKeyEventKind::Down, 'B')) == DivergenceVerdict::Hold, L"anything before live waits");

        gate.TakeHeld (held, heldInputs);

        Assert::AreEqual<size_t> (3, held.size(), L"the press, its character and its release");
        Assert::IsTrue (held[0].kind == DxuiKeyEventKind::Down);
        Assert::IsTrue (held[1].kind == DxuiKeyEventKind::Char);
        Assert::IsTrue (held[2].kind == DxuiKeyEventKind::Up);
        Assert::IsFalse (gate.IsAwaitingLive(), L"done");
        Assert::IsTrue (gate.JudgeKey (false, MakeKey (DxuiKeyEventKind::Char, 'a')) == DivergenceVerdict::Proceed);
    }


    //  A no drops the held keys; a character arriving after it is dropped too.
    TEST_METHOD (ANoDropsTheHeldKeys)
    {
        DivergenceGate             gate;
        std::vector<DxuiKeyEvent>  held;
        std::vector<HeldInput>     heldInputs;



        HoldKey (gate, MakeKey (DxuiKeyEventKind::Down, 'A'), std::nullopt);

        Assert::IsTrue (gate.OnHeldInputRead());

        gate.Answer (false);

        Assert::IsFalse (gate.IsAsking());
        Assert::IsFalse (gate.IsHolding());
        Assert::IsTrue  (gate.JudgeKey (true, MakeKey (DxuiKeyEventKind::Char, 'a')) == DivergenceVerdict::Drop, L"the character is dropped");

        gate.TakeHeld (held, heldInputs);

        Assert::IsTrue (held.empty(), L"nothing comes back");
    }


    //  The replay reached live and the guest never read the key: it goes in
    //  there, with no question at all.
    TEST_METHOD (KeysTheGuestNeverReadGoInAtLive)
    {
        DivergenceGate             gate;
        std::vector<DxuiKeyEvent>  held;
        std::vector<HeldInput>     heldInputs;



        HoldKey (gate, MakeKey (DxuiKeyEventKind::Down, 'A'), std::nullopt);
        HoldKey (gate, MakeKey (DxuiKeyEventKind::Char, 'a'), kLatchA);

        gate.TakeHeld (held, heldInputs);

        Assert::AreEqual<size_t> (2, held.size(), L"handed back");
        Assert::IsFalse (gate.IsHolding());
        Assert::IsFalse (gate.IsAsking(), L"never asked");
    }


    //  With nothing held, a read is in question for nothing: no question.
    TEST_METHOD (AReadWithNothingHeldAsksNothing)
    {
        DivergenceGate  gate;

        Assert::IsFalse (gate.OnHeldInputRead());
        Assert::IsFalse (gate.IsAsking());
    }


    TEST_METHOD (RepeatsAndReleasesBehindLiveAreDroppedWithoutHolding)
    {
        DivergenceGate  gate;

        Assert::IsTrue (gate.JudgeKey (true, MakeKey (DxuiKeyEventKind::Down, 'A', true)) == DivergenceVerdict::Drop);
        Assert::IsTrue (gate.JudgeKey (true, MakeKey (DxuiKeyEventKind::Up,   'A'))       == DivergenceVerdict::Drop);
        Assert::IsFalse (gate.IsHolding());
        Assert::IsFalse (gate.IsAsking());
    }


    //  The owner's report: rewound, played on, and a joystick moved behind
    //  live. A deflection, a press and a closed Joyport switch are each held,
    //  on their own line only, with no question until the guest reads them.
    TEST_METHOD (ADeflectionAPressOrAClosedJoyportSwitchBehindLiveIsHeld)
    {
        GamePortState     rest;
        GamePortState     deflected = MakeStick (kFarEnd, GamePortState::kPaddleCenter);
        GamePortState     pressed;
        GamePortState     closed;
        JoystickSwitches  up;
        HeldInputLines    lines;
        DivergenceGate    gateDeflected;
        DivergenceGate    gatePressed;
        DivergenceGate    gateClosed;



        up.set (static_cast<size_t> (JoystickSwitch::Up));
        pressed.buttons.set (0);
        closed.jacks.jack[JoyportJacks::kRightJack] = up;

        Assert::IsTrue (gateDeflected.JudgeGamePort (true, rest, deflected) == DivergenceVerdict::Hold);
        Assert::IsTrue (gatePressed.JudgeGamePort   (true, rest, pressed)   == DivergenceVerdict::Hold);
        Assert::IsTrue (gateClosed.JudgeGamePort    (true, rest, closed)    == DivergenceVerdict::Hold);

        Assert::IsTrue (gateDeflected.IsHolding() && !gateDeflected.IsAsking(), L"held, not asked");

        lines = gateDeflected.GetHeldLines();
        Assert::IsTrue (lines.paddles[0] == std::optional<Byte> (kFarEnd), L"the deflected axis");
        Assert::IsFalse (lines.paddles[1].has_value(), L"and not the centered one");
        Assert::IsFalse (lines.buttons[0].has_value());

        lines = gatePressed.GetHeldLines();
        Assert::IsTrue (lines.buttons[0] == std::optional<bool> (true), L"the pressed button");
        Assert::IsFalse (lines.paddles[0].has_value(), L"and no axis");

        lines = gateClosed.GetHeldLines();
        Assert::IsTrue (lines.jacks[JoyportJacks::kRightJack] == std::optional<JoystickSwitches> (up), L"the closed switch's jack");
        Assert::IsFalse (lines.jacks[JoyportJacks::kLeftJack].has_value());
    }


    //  A stick left centered, or wandering within the tolerance, centered or
    //  held over to one side, holds nothing.
    TEST_METHOD (AStickHeldSteadyOrJitteringHoldsNothing)
    {
        constexpr Byte  kHeld     = 200;
        DivergenceGate  gate;
        GamePortState   rest;
        GamePortState   jitter    = MakeStick (static_cast<Byte> (GamePortState::kPaddleCenter + DivergenceGate::kAxisTolerance),
                                               static_cast<Byte> (GamePortState::kPaddleCenter - DivergenceGate::kAxisTolerance));
        GamePortState   held      = MakeStick (kHeld, GamePortState::kPaddleCenter);
        GamePortState   heldMoved = MakeStick (static_cast<Byte> (kHeld + DivergenceGate::kAxisTolerance), GamePortState::kPaddleCenter);



        Assert::IsTrue (gate.JudgeGamePort (true, rest, rest)      == DivergenceVerdict::Drop, L"centered");
        Assert::IsTrue (gate.JudgeGamePort (true, rest, jitter)    == DivergenceVerdict::Drop, L"jitter about the center");
        Assert::IsTrue (gate.JudgeGamePort (true, held, heldMoved) == DivergenceVerdict::Drop, L"jitter about a held deflection");
        Assert::IsFalse (gate.IsHolding());
    }


    //  Letting go holds nothing, as a key's release asks nothing: a stick and
    //  a button pressed behind live and let go of before any read are no
    //  longer held.
    TEST_METHOD (LettingGoHoldsNothing)
    {
        DivergenceGate  gate;
        GamePortState   rest;
        GamePortState   held     = MakeStick (kFarEnd, GamePortState::kPaddleCenter);



        held.buttons.set (1);

        Assert::IsTrue (gate.JudgeGamePort (true, held, rest) == DivergenceVerdict::Drop, L"let go of what was held at live");

        Assert::IsTrue (gate.JudgeGamePort (true, rest, held) == DivergenceVerdict::Hold, L"pressed behind live");
        Assert::IsTrue (gate.JudgeGamePort (true, rest, rest) == DivergenceVerdict::Drop, L"and let go of again");
        Assert::IsFalse (gate.IsHolding(), L"nothing held");
        Assert::IsTrue  (gate.GetHeldLines().IsEmpty());
    }


    TEST_METHOD (LiveGamePortInputGoesAhead)
    {
        DivergenceGate  gate;
        GamePortState   rest;



        Assert::IsTrue (gate.JudgeGamePort (false, rest, MakeStick (kFarEnd, 0)) == DivergenceVerdict::Proceed);
    }


    //  While a question is open the game port waits on its answer.
    TEST_METHOD (WhileAQuestionIsOpenTheGamePortWaits)
    {
        DivergenceGate  gate;
        GamePortState   rest;



        HoldKey (gate, MakeKey (DxuiKeyEventKind::Down, 'A'), std::nullopt);
        Assert::IsTrue (gate.OnHeldInputRead());

        Assert::IsTrue (gate.JudgeGamePort (true, rest, MakeStick (kFarEnd, 0)) == DivergenceVerdict::Hold);
        Assert::IsTrue (gate.IsAsking(), L"still the one question");
    }


    //  A no takes the stick where it then was as where it was before: held
    //  there it holds nothing, moved on it is held again, and going live
    //  forgets where it was refused.
    TEST_METHOD (AfterANoOnlyAFurtherMoveIsHeldAgain)
    {
        constexpr Byte  kNearlyFar = static_cast<Byte> (kFarEnd - DivergenceGate::kAxisTolerance);
        DivergenceGate  gate;
        GamePortState   rest;
        GamePortState   right      = MakeStick (kFarEnd, GamePortState::kPaddleCenter);



        Assert::IsTrue (gate.JudgeGamePort (true, rest, right) == DivergenceVerdict::Hold);
        Assert::IsTrue (gate.OnHeldInputRead());
        gate.Answer (false);

        Assert::IsTrue (gate.JudgeGamePort (true, rest, right) == DivergenceVerdict::Drop, L"held where it was refused");
        Assert::IsTrue (gate.JudgeGamePort (true, rest, MakeStick (kNearlyFar, GamePortState::kPaddleCenter)) == DivergenceVerdict::Drop, L"or close to it");
        Assert::IsTrue (gate.JudgeGamePort (true, rest, MakeStick (kFarEnd, 0)) == DivergenceVerdict::Hold, L"pushed on, it is held again");

        Assert::IsTrue (gate.OnHeldInputRead());
        gate.Answer (false);
        gate.OnLive();

        Assert::IsTrue (gate.JudgeGamePort (true, rest, right) == DivergenceVerdict::Hold, L"once live, the refusal is forgotten");
    }


    //  The guest mouse's button is held until the guest reads it, and a yes
    //  hands it back once live. The //c's 80/40 switch flips whatever the
    //  machine holds, so it asks at once, joined by what is held already.
    TEST_METHOD (AMousePressIsHeldAndTheEightyColumnSwitchAsks)
    {
        DivergenceGate             gate;
        std::vector<DxuiKeyEvent>  held;
        std::vector<HeldInput>     heldInputs;



        Assert::IsTrue (gate.JudgeInput (false, true) == DivergenceVerdict::Proceed, L"live, it goes ahead");

        Assert::IsTrue (gate.JudgeInput (true, true) == DivergenceVerdict::Hold, L"the press is held");
        gate.HoldInput (HeldInput::MousePress);

        Assert::IsTrue (gate.IsHolding());
        Assert::IsTrue (gate.GetHeldLines().mouseButton == std::optional<bool> (true), L"the button is down on the watched lines");

        Assert::IsTrue (gate.JudgeInput (true, false) == DivergenceVerdict::Ask, L"the switch asks at once");
        gate.HoldInput (HeldInput::EightyColumnToggle);

        gate.Answer (true);
        gate.TakeHeld (held, heldInputs);

        Assert::AreEqual<size_t> (2, heldInputs.size(), L"both come back");
        Assert::IsTrue (heldInputs[0] == HeldInput::MousePress);
        Assert::IsTrue (heldInputs[1] == HeldInput::EightyColumnToggle);

        Assert::IsTrue (gate.JudgeInput (true, false) == DivergenceVerdict::Ask);
        gate.HoldInput (HeldInput::EightyColumnToggle);
        gate.Answer (false);
        gate.TakeHeld (held, heldInputs);

        Assert::IsTrue (heldInputs.empty(), L"a no drops it");
    }


    //  A click let go of before the guest read the button is dropped: what
    //  the guest would read of it is what it reads anyway.
    TEST_METHOD (AMousePressLetGoOfBeforeAReadIsDropped)
    {
        DivergenceGate             gate;
        std::vector<DxuiKeyEvent>  held;
        std::vector<HeldInput>     heldInputs;



        Assert::IsTrue (gate.JudgeInput (true, true) == DivergenceVerdict::Hold);
        gate.HoldInput (HeldInput::MousePress);

        gate.ReleaseMousePress();

        Assert::IsFalse (gate.IsHolding(), L"nothing held");
        Assert::IsTrue  (gate.GetHeldLines().IsEmpty());

        gate.TakeHeld (held, heldInputs);
        Assert::IsTrue (heldInputs.empty(), L"nothing to hand back");
    }


    //  In Mouse mode the pointer over the picture is the guest mouse's
    //  target: held, and given up when the pointer leaves the picture.
    TEST_METHOD (APointerOverThePictureIsHeldUntilItLeaves)
    {
        constexpr uint32_t  kTarget = 0x80004000;
        DivergenceGate      gate;



        gate.SetMouseTarget (kTarget);

        Assert::IsTrue (gate.IsHolding());
        Assert::IsTrue (gate.GetHeldLines().mouseTarget == std::optional<uint32_t> (kTarget));

        gate.SetMouseTarget (std::nullopt);

        Assert::IsFalse (gate.IsHolding(), L"left the picture");
        Assert::IsTrue  (gate.GetHeldLines().IsEmpty());
    }


    //  After a no to a held pointer, moving on across the picture holds
    //  nothing, so it does not ask at every step; once the pointer has left
    //  the picture, coming back holds again.
    TEST_METHOD (APointerRefusedHoldsNothingUntilItLeavesThePicture)
    {
        constexpr uint32_t  kTarget = 0x80004000;
        constexpr uint32_t  kMoved  = 0x90004000;
        DivergenceGate      gate;



        gate.SetMouseTarget (kTarget);

        Assert::IsTrue (gate.OnHeldInputRead());
        gate.Answer (false);

        gate.SetMouseTarget (kMoved);

        Assert::IsFalse (gate.IsHolding(), L"moved on across the picture");

        gate.SetMouseTarget (std::nullopt);
        gate.SetMouseTarget (kMoved);

        Assert::IsTrue (gate.IsHolding(), L"back over the picture after leaving it");
    }


private:

    static constexpr Byte  kFarEnd = 255;
    static constexpr Byte  kLatchA = 0xC1;


    static GamePortState MakeStick (Byte x, Byte y)
    {
        GamePortState  state;

        state.paddle[0] = x;
        state.paddle[1] = y;
        return state;
    }


    //  As the shell does: judged behind live, then held with what it leaves.
    static void HoldKey (DivergenceGate & gate, const DxuiKeyEvent & ev, std::optional<Byte> latch)
    {
        Assert::IsTrue (gate.JudgeKey (true, ev) == DivergenceVerdict::Hold, L"behind live a key is held");

        gate.HoldKey (ev, latch, true);
    }
};