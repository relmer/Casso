#include "Pch.h"

#include "EhmTestHelper.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ModuleSetup
//
////////////////////////////////////////////////////////////////////////////////

TEST_MODULE_INITIALIZE (ModuleSetup)
{
    UnitTestHelpers::SetupForUnitTests();

    //  No window of the test process is ever under the pointer.
    DxuiTooltip::SetDefaultActiveTest ([] () { return true; });
}
