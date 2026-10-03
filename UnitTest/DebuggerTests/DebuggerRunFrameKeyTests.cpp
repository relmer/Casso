#include "Pch.h"

#include "Ui/Debugger/DebuggerActions.h"
#include "Ui/Debugger/DebuggerCommands.h"
#include "Ui/Debugger/DebuggerKeySchemes.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerRunFrameKeyTests
//
//  Run one frame has a key in every scheme, F6, and the key and the Debug
//  menu's entry send FRAME in Casso's words whatever the console's mode.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (DebuggerRunFrameKeyTests)
    {
    public:

        using Action = DebuggerKeySchemes::Action;



        TEST_METHOD (EverySchemeRunsAFrameOnF6)
        {
            for (DebuggerKeyScheme scheme : { DebuggerKeyScheme::VisualStudio, DebuggerKeyScheme::AppleWin, DebuggerKeyScheme::GSSquared })
            {
                int  id = 0;



                Assert::IsTrue (DebuggerKeySchemes::GetMap (scheme).TryTranslate (VK_F6, false, false, false, id));
                Assert::AreEqual ((int) Action::RunFrame, id);
                Assert::AreEqual ((int) Action::RunFrame, DebuggerCommands::kRunFrame);
            }
        }



        TEST_METHOD (TheKeySendsFrameInCassoWords)
        {
            for (CommandMode mode : { CommandMode::AppleWin, CommandMode::GSSquared, CommandMode::WinDbg })
            {
                std::optional<DebuggerAction>  action = DebuggerActions::GetForKey (Action::RunFrame, nullptr, -1, mode);



                Assert::IsTrue   (action.has_value(), L"no snapshot is needed to run a frame");
                Assert::IsTrue   (action->command.verb == DebugVerb::RunFrame);
                Assert::AreEqual (1u, action->command.count);
                Assert::AreEqual (std::string ("FRAME"), action->echo);
                Assert::IsTrue   (action->echoMode == CommandMode::Casso);
            }
        }
    };
}
