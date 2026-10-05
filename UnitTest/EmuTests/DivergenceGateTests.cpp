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



        Assert::IsTrue (gate.JudgeKey (true, down) == DivergenceVerdict::Ask);
        gate.Hold (down);

        Assert::IsTrue (gate.IsAsking());
        Assert::IsTrue (gate.JudgeKey (true, ch) == DivergenceVerdict::Hold, L"the character waits for the answer");
        gate.Hold (ch);

        gate.Answer (true);

        Assert::IsTrue (gate.IsAwaitingLive());
        Assert::IsTrue (gate.JudgeKey (true, MakeKey (DxuiKeyEventKind::Up, 'A')) == DivergenceVerdict::Hold, L"and so does anything before live");

        gate.TakeHeld (held);

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



        Assert::IsTrue (gate.JudgeKey (true, down) == DivergenceVerdict::Ask);
        gate.Hold (down);

        gate.Answer (false);

        Assert::IsFalse (gate.IsAsking());
        Assert::IsTrue (gate.JudgeKey (true, MakeKey (DxuiKeyEventKind::Char, 'a')) == DivergenceVerdict::Drop, L"the character is dropped");

        gate.TakeHeld (held);

        Assert::IsTrue (held.empty(), L"nothing comes back");
    }


    TEST_METHOD (RepeatsAndReleasesBehindLiveAreDroppedWithoutAsking)
    {
        DivergenceGate  gate;

        Assert::IsTrue (gate.JudgeKey (true, MakeKey (DxuiKeyEventKind::Down, 'A', true)) == DivergenceVerdict::Drop);
        Assert::IsTrue (gate.JudgeKey (true, MakeKey (DxuiKeyEventKind::Up,   'A'))       == DivergenceVerdict::Drop);
        Assert::IsFalse (gate.IsAsking());
    }
};
