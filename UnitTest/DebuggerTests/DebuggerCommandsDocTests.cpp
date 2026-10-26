#include "Pch.h"

#include "Debugger/CommandModeHelp.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerCommandsDocTests
//
//  The reference docs/Debugger-Commands.md is generated from: the help table
//  every mode's HELP ALL reads. The comparison with the copy in the tree
//  writes the regenerated copy to %TEMP%\Casso, so it is in the scenario
//  suite's DebuggerCommandsDocTests.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (DebuggerCommandsDocTests)
    {
    public:

        //  A generator that produced nothing would match an empty document.
        TEST_METHOD (Document_ListsEveryModesCommands)
        {
            std::string        generated = CommandModeHelp::BuildReference();
            const CommandMode  modes[]   = { CommandMode::AppleWin, CommandMode::Monitor, CommandMode::GSSquared, CommandMode::WinDbg, CommandMode::Casso };



            for (CommandMode mode : modes)
            {
                Assert::IsTrue (generated.find (std::format ("## {} mode", CommandModeHelp::GetTitle (mode))) != std::string::npos);

                for (const std::string & line : CommandModeHelp::BuildHelp (mode))
                {
                    Assert::IsTrue (generated.find (line) != std::string::npos);
                }
            }
        }
    };
}
