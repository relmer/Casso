#include "Pch.h"

#include "EhmTestHelper.h"
#include "TestSystemState.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ModuleSetup
//
////////////////////////////////////////////////////////////////////////////////

TEST_MODULE_INITIALIZE (ModuleSetup)
{
    UnitTestHelpers::SetupForUnitTests();

    //  No key, pointer or setting of this desktop's.
    TestSystemState::Install();

    //  No window of the test process is ever under the pointer.
    DxuiTooltip::SetDefaultActiveTest ([] () { return true; });
}
