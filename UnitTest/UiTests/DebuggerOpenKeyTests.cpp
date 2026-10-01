#include "Pch.h"

#include "Ui/Chrome/EmulatorCommands.h"
#include "resource.h"


using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerOpenKeyTests
//
//  The Debug menu's Debugger... row shows F12, the key the emulator window's
//  accelerator table binds to it alongside F7 and Ctrl+F12.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DebuggerOpenKeyTests)
{
public:

    TEST_METHOD (DebuggerRowShowsF12)
    {
        EmulatorCommands                    commands;
        std::shared_ptr<const DxuiCommand>  cmd = commands.Find (IDM_VIEW_DEBUGGER);



        Assert::IsNotNull (cmd.get());
        Assert::AreEqual (std::wstring (L"F12"), cmd->accelerator);
    }
};
