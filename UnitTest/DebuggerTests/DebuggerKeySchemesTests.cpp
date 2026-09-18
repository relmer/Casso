#include "Pch.h"

#include "Ui/Debugger/DebuggerKeySchemes.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerKeySchemesTests
//
//  The three keyboard schemes (FR-026c), and which keys a focused text box
//  keeps from them.
//
//  Two schemes bind Space and Enter, which a command box needs for typing, so
//  the box rule is the part that matters most: those keys reach the scheme only
//  from an empty box, as they do in AppleWin, and a letter never does, so a
//  scheme that binds O cannot eat the first letter of OUT.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (DebuggerKeySchemesTests)
    {
    public:

        using Action = DebuggerKeySchemes::Action;



        static Action Translate (DebuggerKeyScheme scheme, WPARAM vk, bool ctrl = false, bool shift = false)
        {
            int  id = 0;



            Assert::IsTrue (DebuggerKeySchemes::GetMap (scheme).TryTranslate (vk, ctrl, false, shift, id));
            return (Action) id;
        }



        TEST_METHOD (VisualStudio_IsTheDefault)
        {
            Assert::IsTrue (DebuggerKeySchemes::kDefault == DebuggerKeyScheme::VisualStudio);
        }


        TEST_METHOD (VisualStudio_Keys)
        {
            Assert::IsTrue (Translate (DebuggerKeyScheme::VisualStudio, VK_F5)                     == Action::Run);
            Assert::IsTrue (Translate (DebuggerKeyScheme::VisualStudio, VK_F5,  false, true)       == Action::Pause);
            Assert::IsTrue (Translate (DebuggerKeyScheme::VisualStudio, VK_F11)                    == Action::StepInto);
            Assert::IsTrue (Translate (DebuggerKeyScheme::VisualStudio, VK_F10)                    == Action::StepOver);
            Assert::IsTrue (Translate (DebuggerKeyScheme::VisualStudio, VK_F11, false, true)       == Action::StepOut);
            Assert::IsTrue (Translate (DebuggerKeyScheme::VisualStudio, VK_F9)                     == Action::ToggleBreakpoint);
            Assert::IsTrue (Translate (DebuggerKeyScheme::VisualStudio, VK_F10, true)              == Action::RunToCursor);
        }


        TEST_METHOD (AppleWin_Keys)
        {
            Assert::IsTrue (Translate (DebuggerKeyScheme::AppleWin, VK_SPACE)                      == Action::StepInto);
            Assert::IsTrue (Translate (DebuggerKeyScheme::AppleWin, VK_SPACE, true)                == Action::StepOver);
            Assert::IsTrue (Translate (DebuggerKeyScheme::AppleWin, VK_SPACE, false, true)         == Action::StepOut);
            Assert::IsTrue (Translate (DebuggerKeyScheme::AppleWin, VK_RETURN)                     == Action::Run);
            Assert::IsTrue (Translate (DebuggerKeyScheme::AppleWin, VK_DOWN,  true)                == Action::RunToCursor);
        }


        TEST_METHOD (GSSquared_Keys)
        {
            Assert::IsTrue (Translate (DebuggerKeyScheme::GSSquared, VK_SPACE)                     == Action::StepInto);
            Assert::IsTrue (Translate (DebuggerKeyScheme::GSSquared, VK_RETURN)                    == Action::Run);
            Assert::IsTrue (Translate (DebuggerKeyScheme::GSSquared, 'O')                          == Action::StepOver);
            Assert::IsTrue (Translate (DebuggerKeyScheme::GSSquared, 'R')                          == Action::StepOut);
        }


        TEST_METHOD (EverySchemeReachesEveryAction)
        {
            for (DebuggerKeyScheme scheme : { DebuggerKeyScheme::VisualStudio, DebuggerKeyScheme::AppleWin, DebuggerKeyScheme::GSSquared })
            {
                const DxuiKeyMap &  map = DebuggerKeySchemes::GetMap (scheme);

                for (int action = (int) Action::First; action <= (int) Action::Last; ++action)
                {
                    Assert::IsFalse (map.GetChordText (action).empty(), map.GetName().c_str());
                }
            }
        }


        TEST_METHOD (NamesRoundTrip)
        {
            DebuggerKeyScheme  scheme = DebuggerKeyScheme::VisualStudio;



            for (DebuggerKeyScheme each : { DebuggerKeyScheme::VisualStudio, DebuggerKeyScheme::AppleWin, DebuggerKeyScheme::GSSquared })
            {
                Assert::IsTrue (DebuggerKeySchemes::TryParse (DebuggerKeySchemes::GetName (each), scheme));
                Assert::IsTrue (scheme == each);
            }

            Assert::IsTrue  (DebuggerKeySchemes::TryParse ("applewin", scheme), L"case does not matter");
            Assert::IsFalse (DebuggerKeySchemes::TryParse ("Emacs",    scheme));
        }


        TEST_METHOD (OutsideABox_TheSchemeGetsEveryKey)
        {
            Assert::IsFalse (DebuggerKeySchemes::DoesBoxKeepKey ('O',       false, false, false, true));
            Assert::IsFalse (DebuggerKeySchemes::DoesBoxKeepKey (VK_SPACE,  false, false, false, false));
            Assert::IsFalse (DebuggerKeySchemes::DoesBoxKeepKey (VK_RETURN, false, false, false, false));
        }


        TEST_METHOD (InABox_SpaceAndEnterReachTheSchemeOnlyWhenItIsEmpty)
        {
            Assert::IsFalse (DebuggerKeySchemes::DoesBoxKeepKey (VK_SPACE,  false, false, true, true));
            Assert::IsFalse (DebuggerKeySchemes::DoesBoxKeepKey (VK_RETURN, false, false, true, true));
            Assert::IsTrue  (DebuggerKeySchemes::DoesBoxKeepKey (VK_SPACE,  false, false, true, false), L"typing a space");
            Assert::IsTrue  (DebuggerKeySchemes::DoesBoxKeepKey (VK_RETURN, false, false, true, false), L"submitting a line");
        }


        TEST_METHOD (InABox_LettersAndEditingKeysStayWithIt)
        {
            Assert::IsTrue (DebuggerKeySchemes::DoesBoxKeepKey ('O',      false, false, true, true), L"the O of OUT");
            Assert::IsTrue (DebuggerKeySchemes::DoesBoxKeepKey ('7',      false, false, true, true));
            Assert::IsTrue (DebuggerKeySchemes::DoesBoxKeepKey (VK_LEFT,  false, false, true, false));
            Assert::IsTrue (DebuggerKeySchemes::DoesBoxKeepKey (VK_BACK,  false, false, true, false));
        }


        TEST_METHOD (InABox_FunctionKeysAndChordsReachTheScheme)
        {
            Assert::IsFalse (DebuggerKeySchemes::DoesBoxKeepKey (VK_F10,   false, false, true, false));
            Assert::IsFalse (DebuggerKeySchemes::DoesBoxKeepKey (VK_SPACE, true,  false, true, false), L"Ctrl+Space");
            Assert::IsFalse (DebuggerKeySchemes::DoesBoxKeepKey (VK_DOWN,  true,  false, true, false), L"Ctrl+Down");
        }
    };
}
