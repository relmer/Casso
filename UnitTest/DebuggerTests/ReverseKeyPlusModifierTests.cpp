#include "Pch.h"

#include "Ui/Debugger/DebuggerActions.h"
#include "Ui/Debugger/DebuggerCommands.h"
#include "Ui/Debugger/DebuggerKeySchemes.h"
#include "Ui/Debugger/DebuggerViewState.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseKeyPlusModifierTests
//
//  In every scheme each reverse command's key is its forward command's key
//  with one more modifier held, Visual Studio keeps its Ctrl+R chords as a
//  second binding and pauses on Ctrl+Break as well as Shift+F5, and a command
//  with two bindings shows both in its tip.
//
////////////////////////////////////////////////////////////////////////////////

namespace ReverseKeyPlusModifier
{
    using Action = DebuggerKeySchemes::Action;



    TEST_CLASS (ReverseKeyPlusModifierTests)
    {
    public:

        static bool IsBound (DebuggerKeyScheme scheme, WPARAM vk, bool ctrl, bool alt, bool shift, Action action)
        {
            int  id = 0;



            return DebuggerKeySchemes::GetMap (scheme).TryTranslate (vk, ctrl, alt, shift, id) && id == (int) action;
        }


        //  The modifiers of a chord as bits, Ctrl, Alt and Shift.
        static int GetModifierBits (const DxuiKeyChord & chord)
        {
            return (chord.ctrl ? 1 : 0) | (chord.alt ? 2 : 0) | (chord.shift ? 4 : 0);
        }


        //  Whether some one-key chord of back is a one-key chord of forward on
        //  the same key with exactly one modifier added.
        static bool IsForwardPlusOneModifier (const DxuiKeyMap & map, Action forward, Action back)
        {
            for (const DxuiKeyChord & b : map.GetChords())
            {
                if (b.id != (int) back || b.prefix.vk != 0)
                {
                    continue;
                }

                for (const DxuiKeyChord & f : map.GetChords())
                {
                    int  added = GetModifierBits (b) & ~GetModifierBits (f);

                    if (f.id == (int) forward && f.prefix.vk == 0 && f.vk == b.vk &&
                        (GetModifierBits (f) & ~GetModifierBits (b)) == 0 && (added == 1 || added == 2 || added == 4))
                    {
                        return true;
                    }
                }
            }

            return false;
        }



        TEST_METHOD (EveryReverseKeyIsItsForwardKeyPlusOneModifier)
        {
            static constexpr std::pair<Action, Action>  kPairs[] =
            {
                { Action::StepInto, Action::StepBackInto },
                { Action::StepOver, Action::StepBackOver },
                { Action::StepOut,  Action::StepBackOut  },
                { Action::Run,      Action::ReverseRun   },
            };



            for (DebuggerKeyScheme scheme : { DebuggerKeyScheme::VisualStudio, DebuggerKeyScheme::AppleWin, DebuggerKeyScheme::GSSquared })
            {
                for (const auto & [forward, back] : kPairs)
                {
                    Assert::IsTrue (IsForwardPlusOneModifier (DebuggerKeySchemes::GetMap (scheme), forward, back),
                                    DebuggerKeySchemes::GetMap (scheme).GetName().c_str());
                }
            }
        }


        TEST_METHOD (VisualStudioReverseKeys)
        {
            constexpr DebuggerKeyScheme  kScheme = DebuggerKeyScheme::VisualStudio;



            Assert::IsTrue (IsBound (kScheme, VK_F11, false, true, false, Action::StepBackInto));
            Assert::IsTrue (IsBound (kScheme, VK_F10, false, true, false, Action::StepBackOver));
            Assert::IsTrue (IsBound (kScheme, VK_F11, false, true, true,  Action::StepBackOut));
            Assert::IsTrue (IsBound (kScheme, VK_F5,  false, true, false, Action::ReverseRun));
        }


        TEST_METHOD (AppleWinReverseKeys)
        {
            constexpr DebuggerKeyScheme  kScheme = DebuggerKeyScheme::AppleWin;



            Assert::IsTrue  (IsBound (kScheme, VK_SPACE,  false, true, false, Action::StepBackInto));
            Assert::IsTrue  (IsBound (kScheme, VK_SPACE,  true,  true, false, Action::StepBackOver));
            Assert::IsTrue  (IsBound (kScheme, VK_SPACE,  false, true, true,  Action::StepBackOut));
            Assert::IsTrue  (IsBound (kScheme, VK_RETURN, false, true, false, Action::ReverseRun));
            Assert::IsFalse (IsBound (kScheme, VK_F11,    false, true, false, Action::StepBackInto), L"no Visual Studio key left over");
        }


        TEST_METHOD (GSSquaredReverseKeys)
        {
            constexpr DebuggerKeyScheme  kScheme = DebuggerKeyScheme::GSSquared;



            Assert::IsTrue  (IsBound (kScheme, VK_SPACE,  false, true, false, Action::StepBackInto));
            Assert::IsTrue  (IsBound (kScheme, 'O',       false, true, false, Action::StepBackOver));
            Assert::IsTrue  (IsBound (kScheme, 'R',       false, true, false, Action::StepBackOut));
            Assert::IsTrue  (IsBound (kScheme, VK_RETURN, false, true, false, Action::ReverseRun));
            Assert::IsFalse (IsBound (kScheme, VK_F11,    false, true, false, Action::StepBackInto), L"no Visual Studio key left over");
        }


        TEST_METHOD (VisualStudioPausesOnCtrlBreakAndShiftF5)
        {
            Assert::IsTrue (IsBound (DebuggerKeyScheme::VisualStudio, VK_CANCEL, true,  false, false, Action::Pause));
            Assert::IsTrue (IsBound (DebuggerKeyScheme::VisualStudio, VK_F5,     false, false, true,  Action::Pause));
        }


        TEST_METHOD (ReverseRunKeyRunsBackward)
        {
            std::optional<DebuggerAction>  taken = DebuggerActions::GetForKey (Action::ReverseRun, nullptr, -1, CommandMode::AppleWin);



            Assert::IsTrue   (taken.has_value(), L"the key has an action");
            Assert::IsTrue   (taken->command.verb == DebugVerb::ReverseGo);
            Assert::AreEqual (std::string ("G-"), taken->echo);
            Assert::AreEqual ((int) Action::ReverseRun, DebuggerCommands::kReverseContinue, L"the button and the key are one command");
        }


        TEST_METHOD (ReverseRunKeyTakesNothingWhileTheMachineRuns)
        {
            DebuggerViewSnapshot  running;



            running.isPaused = false;

            Assert::IsTrue  (DebuggerActions::IsStepAction (Action::ReverseRun));
            Assert::IsFalse (DebuggerActions::GetForKey (Action::ReverseRun, &running, -1, CommandMode::AppleWin).has_value());
        }


        TEST_METHOD (TipsShowBothBindings)
        {
            DebuggerCommands  commands ({});



            commands.ApplyKeyScheme (DebuggerKeyScheme::VisualStudio);

            Assert::IsTrue (commands.Find (DebuggerCommands::kStepBackInto)->tip.starts_with (L"Step back into (Alt+F11 or Ctrl+R, F11)"));
            Assert::IsTrue (commands.Find (DebuggerCommands::kStepBackOut)->tip.starts_with  (L"Step back out (Alt+Shift+F11 or Ctrl+R, Shift+F11)"));
            Assert::IsTrue (commands.Find (DebuggerCommands::kPause)->tip.starts_with        (L"Pause (Shift+F5 or Ctrl+Break)"));
            Assert::IsTrue (commands.Find (DebuggerCommands::kReverseContinue)->tip.starts_with (L"Reverse continue (Alt+F5)"));
        }
    };
}
